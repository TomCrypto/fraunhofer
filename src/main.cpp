#include <spectrum.hpp>
#include <pugixml.hpp>
#include <utility.hpp>
#include <iostream>
#include <fstream>
#include <cmath>

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

        for (size_t t = 0; t < samples; ++t)
        {
            uint64_t seed = t;
            float lambda = (float)t / samples;
            kernel.setArg(4, sizeof(cl_float), &lambda);
            kernel.setArg(5, sizeof(uint64_t), &seed);

            cl::NDRange offset(0), global(dim_x * dim_y);
            queue.enqueueNDRangeKernel(kernel, offset, global, cl::NullRange);
        }

        queue.enqueueReadBuffer(render, CL_TRUE, 0, size, &aperture[0]);
    }

    std::fstream stream(argv[2], std::ios::out | std::ios::binary);

    stream << "#?RADIANCE" << std::endl;
    stream << "SOFTWARE=lensfft" << std::endl;
    stream << "FORMAT=32-bit_rle_xyze" << std::endl << std::endl;
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
