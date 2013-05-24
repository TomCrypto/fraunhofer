#include <spectrum.hpp>
#include <pugixml.hpp>
#include <utility.hpp>
#include <iostream>
#include <fstream>
#include <cmath>

/* These are some scene file definitions for color systems. */
#define ID_EBU 0
#define ID_SMPTE 1
#define ID_HDTV 2
#define ID_REC709 3
#define ID_NTSC 4
#define ID_CIE 5

/* This is a color system. */
typedef struct ColorSystem{
    double xRed, yRed;	    	    /* Red x, y */
    double xGreen, yGreen;  	    /* Green x, y */
    double xBlue, yBlue;     	    /* Blue x, y */
    double xWhite, yWhite;  	    /* White point x, y */
	double gamma;   	    	    /* Gamma correction for system */
} ColorSystem;

/* These are some relatively common illuminants (white points). */
#define IlluminantC     0.3101, 0.3162	    	/* For NTSC television */
#define IlluminantD65   0.3127, 0.3291	    	/* For EBU and SMPTE */
#define IlluminantE 	0.33333333, 0.33333333  /* CIE equal-energy illuminant */

/* 0 represents a special gamma function. */
#define GAMMA_REC709 0

/* These are some standard color systems. */
const ColorSystem /* xRed    yRed    xGreen  yGreen  xBlue  yBlue    White point        Gamma   */
    EBUSystem    =  {0.64,   0.33,   0.29,   0.60,   0.15,   0.06,   IlluminantD65,  GAMMA_REC709},
    SMPTESystem  =  {0.630,  0.340,  0.310,  0.595,  0.155,  0.070,  IlluminantD65,  GAMMA_REC709},
    HDTVSystem   =  {0.670,  0.330,  0.210,  0.710,  0.150,  0.060,  IlluminantD65,  GAMMA_REC709},
    Rec709System =  {0.64,   0.33,   0.30,   0.60,   0.15,   0.06,   IlluminantD65,  GAMMA_REC709},
    NTSCSystem   =  {0.67,   0.33,   0.21,   0.71,   0.14,   0.08,   IlluminantC,    GAMMA_REC709},
    CIESystem    =  {0.7355, 0.2645, 0.2658, 0.7243, 0.1669, 0.0085, IlluminantE,    GAMMA_REC709};

void XYZtoRGB(float x, float y, float z, float *r, float *g, float *b, ColorSystem colorSystem)
{
	/* Decode the color system. */
    float xr = colorSystem.xRed;   float yr = colorSystem.yRed;   float zr = 1 - (xr + yr);
    float xg = colorSystem.xGreen; float yg = colorSystem.yGreen; float zg = 1 - (xg + yg);
    float xb = colorSystem.xBlue;  float yb = colorSystem.yBlue;  float zb = 1 - (xb + yb);
    float xw = colorSystem.xWhite; float yw = colorSystem.yWhite; float zw = 1 - (xw + yw);

    /* Compute the XYZ to RGB matrix. */
    float rx = (yg * zb) - (yb * zg);
    float ry = (xb * zg) - (xg * zb);
    float rz = (xg * yb) - (xb * yg);
    float gx = (yb * zr) - (yr * zb);
    float gy = (xr * zb) - (xb * zr);
    float gz = (xb * yr) - (xr * yb);
    float bx = (yr * zg) - (yg * zr);
    float by = (xg * zr) - (xr * zg);
    float bz = (xr * yg) - (xg * yr);

    /* Compute the RGB luminance scaling factor. */
    float rw = ((rx * xw) + (ry * yw) + (rz * zw)) / yw;
    float gw = ((gx * xw) + (gy * yw) + (gz * zw)) / yw;
    float bw = ((bx * xw) + (by * yw) + (bz * zw)) / yw;

    /* Scale the XYZ to RGB matrix to white. */
    rx = rx / rw;  ry = ry / rw;  rz = rz / rw;
    gx = gx / gw;  gy = gy / gw;  gz = gz / gw;
    bx = bx / bw;  by = by / bw;  bz = bz / bw;

    /* Calculate the desired RGB. */
    *r = (rx * x) + (ry * y) + (rz * z);
    *g = (gx * x) + (gy * y) + (gz * z);
    *b = (bx * x) + (by * y) + (bz * z);

    /* Constrain the RGB color within the RGB gamut. */
    float w = std::min(0.0f, std::min(*r, std::min(*g, *b)));
	*r -= w; *g -= w; *b -= w;
}

cl::Program LoadProgram(cl::Context context, std::vector<cl::Device> devices)
{
    const char* src = "#include <def.cl>\n"
                      "#include <fft.cl>\n"
                      "#include <lens.cl>\n";

    cl::Program::Sources data;
    data = cl::Program::Sources(1, std::make_pair(src, strlen(src)));

    cl::Program program = cl::Program(context, data, 0);

    if (program.build(devices, "-cl-std=CL1.1 -I cl/") != CL_SUCCESS)
    {
        std::string log;
        program.getBuildInfo(devices[0], CL_PROGRAM_BUILD_LOG, &log);
        std::cout << log << std::endl;
    }

    return program;
}

int main(int argc, char* argv[])
{
    if (argc != 4) return 0;
    size_t pla_num, dev_num;
    float lensDistance;
    float threshold;

    {
        std::fstream xml("config.xml", std::ios::in);
        pugi::xml_document doc; doc.load(xml);

        pugi::xml_node node = doc.child("Settings");
        pla_num      = node.child("OpenCL").attribute("Platform").as_uint();
        dev_num      = node.child("OpenCL").attribute("Device").as_uint();
        lensDistance = node.child("FFT").attribute("LensDistance").as_float();
        threshold    = node.child("FFT").attribute("Threshold").as_float();

        if (lensDistance == 0.0f) return 0;
    }

    std::vector<cl_float4> aperture;
    size_t samples = atoi(argv[3]);
    size_t dim_x = 0, radix_x = 0;
    size_t dim_y = 0, radix_y = 0;

    {
        std::fstream stream(argv[1], std::ios::in | std::ios::binary);
        std::string header; stream >> header;
        size_t resolution = 0;

        if ((header != "P3") && (header != "P6")) return 0;
        stream >> dim_x; if ((radix_x = radix(dim_x)) == 0) return 0;
        stream >> dim_y; if ((radix_y = radix(dim_y)) == 0) return 0;
        aperture.reserve(dim_x * dim_y);
        stream >> resolution;

        for (size_t y = 0; y < dim_y; ++y)
            for (size_t x = 0; x < dim_x; ++x)
            {
                float R, G, B, A;

                if (header == "P3")
                {
                    size_t r, g, b;
                    stream >> r;
                    stream >> g;
                    stream >> b;

                    R = (float)r / resolution;
                    G = (float)g / resolution;
                    B = (float)b / resolution;
                }
                else
                {
                    if (resolution < 256)
                    {
                        uint8_t r, g, b;
                        stream.read((char*)&r, sizeof(uint8_t));
                        stream.read((char*)&g, sizeof(uint8_t));
                        stream.read((char*)&b, sizeof(uint8_t));

                        R = (float)r / resolution;
                        G = (float)g / resolution;
                        B = (float)b / resolution;
                    }
                    else
                    {
                        uint16_t r, g, b;
                        stream.read((char*)&r, sizeof(uint16_t));
                        stream.read((char*)&g, sizeof(uint16_t));
                        stream.read((char*)&b, sizeof(uint16_t));

                        R = (float)r / resolution;
                        G = (float)g / resolution;
                        B = (float)b / resolution;
                    }
                }

                if (threshold == 1) A = sqrt((R + G + B) / 3.0f);
                else A = (sqrt((R + G + B) / 3.0f) > threshold) ? 1 : 0;
                cl_float4 Aper = {{A, 0, 0, 0}};
                aperture.push_back(Aper);
            }
    }

    cl::Platform platform;
    cl::Device     device;

    {
        std::vector<cl::Platform> platforms; cl::Platform::get(&platforms);
        if (pla_num >= platforms.size()) return 0;
        platform = platforms[pla_num];

        std::vector<cl::Device> devices;
        platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
        if (dev_num >= devices.size()) return 0;
        device = devices[dev_num];
    }

    cl::CommandQueue queue;
    cl::Context context;
    cl::Program program;
    cl::Image2D diff;

    {
        std::vector<cl::Device> devices(&device, &device + 1);
        context = cl::Context(devices, 0, 0, 0, 0);
        queue = cl::CommandQueue(context, device, 0);
        program = LoadProgram(context, devices);
    }

    {
        CLParams clParams = { (uint32_t)dim_x, (uint32_t)radix_x,
                              (uint32_t)dim_y, (uint32_t)radix_y };

        uint32_t *reversal_x = new uint32_t[dim_x];
        uint32_t *reversal_y = new uint32_t[dim_y];
        ReversalTable(dim_x, radix_x, reversal_x);
        ReversalTable(dim_y, radix_y, reversal_y);

        cl::NDRange offset(0);
        void *ptr = &aperture[0];
        size_t brt_x_size = sizeof(uint32_t) * dim_x;
        size_t brt_y_size = sizeof(uint32_t) * dim_y;
        size_t size = dim_x * dim_y * sizeof(cl_float4);
        cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR;
        cl::Buffer clAperture = cl::Buffer(context, flags, size, ptr);
        cl::Buffer brtx = cl::Buffer(context, flags, brt_x_size, reversal_x);
        cl::Buffer brty = cl::Buffer(context, flags, brt_y_size, reversal_y);

        cl::Kernel kernel_x = cl::Kernel(program, "cl_fft_row");
        kernel_x.setArg(1, sizeof(clParams), &clParams);
        kernel_x.setArg(0, clAperture);
        kernel_x.setArg(2, brtx);

        cl::NDRange global_x(dim_y);
        queue.enqueueNDRangeKernel(kernel_x, offset, global_x, cl::NullRange);

        cl::Kernel kernel_y = cl::Kernel(program, "cl_fft_col");
        kernel_y.setArg(1, sizeof(clParams), &clParams);
        kernel_y.setArg(0, clAperture);
        kernel_y.setArg(2, brty);

        cl::NDRange global_y(dim_x);
        queue.enqueueNDRangeKernel(kernel_y, offset, global_y, cl::NullRange);

        flags = CL_MEM_WRITE_ONLY;
        cl::ImageFormat format(CL_INTENSITY, CL_FLOAT);
        cl::Image2D tmp = cl::Image2D(context, flags, format, dim_x, dim_y, 0);

        flags = CL_MEM_READ_ONLY;
        diff = cl::Image2D(context, flags, format, dim_x, dim_y, 0);

        cl::Kernel kernel = cl::Kernel(program, "cl_fft_normalize");
        kernel.setArg(3, sizeof(cl_float), &lensDistance);
        kernel.setArg(1, sizeof(clParams), &clParams);
        kernel.setArg(0, clAperture);
        kernel.setArg(2, tmp);

        cl::NDRange global_xy(dim_x * dim_y);
        queue.enqueueNDRangeKernel(kernel, offset, global_xy, cl::NullRange);
        
        cl::size_t<3> origin; origin[0] = 0; origin[1] = 0; origin[2] = 0;
        cl::size_t<3> rgn; rgn[0] = dim_x; rgn[1] = dim_y; rgn[2] = 1;
        queue.enqueueCopyImage(tmp, diff, origin, origin, rgn);

        delete[] reversal_x;
        delete[] reversal_y;
    }

    for (size_t t = 0; t < dim_x * dim_y; ++t)
    {
        aperture[t].s[0] = 0.0f;
        aperture[t].s[1] = 0.0f;
        aperture[t].s[2] = 0.0f;
        aperture[t].s[3] = 0.0f;
    }

    size_t size = dim_x * dim_y * sizeof(cl_float4);
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR;
    cl::Buffer render = cl::Buffer(context, flags, size, &aperture[0]);

    {
        CLParams clParams = { (uint32_t)dim_x, (uint32_t)radix_x,
                              (uint32_t)dim_y, (uint32_t)radix_y };

        cl::ImageFormat format(CL_RGBA, CL_FLOAT);
        cl::Image2D spectrum = cl::Image2D(context, CL_MEM_READ_ONLY, format,
                                           Resolution(), 1, 0);

        cl::size_t<3> origin; origin[0] = 0; origin[1] = 0; origin[2] = 0;
        cl::size_t<3> rgn; rgn[0] = Resolution(); rgn[1] = 1; rgn[2] = 1;
        queue.enqueueWriteImage(spectrum, CL_TRUE, origin, rgn, 0, 0, Curve());

        cl::Kernel kernel = cl::Kernel(program, "cl_lens");
        kernel.setArg(1, sizeof(clParams), &clParams);
        kernel.setArg(3, spectrum);
        kernel.setArg(0, render);
        kernel.setArg(2, diff);

		cl_uint sampleCount = samples, seed = 0;
		kernel.setArg(4, sizeof(cl_uint), &sampleCount);
		kernel.setArg(5, sizeof(uint64_t), &seed);

        cl::NDRange offset(0), global(dim_x * dim_y);
        queue.enqueueNDRangeKernel(kernel, offset, global, cl::NullRange);
        queue.enqueueReadBuffer(render, CL_TRUE, 0, size, &aperture[0]);
    }

    std::fstream stream(argv[2], std::ios::out | std::ios::binary);

    stream << "#?RADIANCE" << std::endl;
    stream << "SOFTWARE=fraunhofer" << std::endl;
    stream << "FORMAT=32-bit_rle_rgbe" << std::endl << std::endl;
    stream << "-Y " << dim_y << " +X " << dim_x << std::endl;

    for (size_t y = 0; y < dim_y; ++y)
        for (size_t x = 0; x < dim_x; ++x)
        {
            float a = aperture[y * dim_x + x].s[0];
            float b = aperture[y * dim_x + x].s[1];
            float c = aperture[y * dim_x + x].s[2];
            float n = aperture[y * dim_x + x].s[3];

            if (n != 0.0f)
            {
                a /= n;
                b /= n;
                c /= n;
            }

            a = sqrt(a);
            b = sqrt(b);
            c = sqrt(c);

			/* This is TEMPORARY as the code is supposed to output
			 * the render in XYZ format. However, apparently handling
			 * XYZ colors is so mind-blowingly difficult that HDR
			 * viewers aren't capable of doing so, so at the moment
			 * we're outputting in RGB as a stopgap solution. */
			XYZtoRGB(a, b, c, &a, &b, &c, CIESystem);

            float m = std::max(a, std::max(b, c));
            uint8_t pe = ceil(log(m) / log(2.0f) + 128);
            uint8_t px = floor((256 * a) / pow(2.0f, pe - 128));
            uint8_t py = floor((256 * b) / pow(2.0f, pe - 128));
            uint8_t pz = floor((256 * c) / pow(2.0f, pe - 128));

            stream.write((char*)&px, sizeof(uint8_t));
            stream.write((char*)&py, sizeof(uint8_t));
            stream.write((char*)&pz, sizeof(uint8_t));
            stream.write((char*)&pe, sizeof(uint8_t));
        }

    stream.close();
}
