void kernel cl_fft_row(global float4 *v, private Params dims, constant uint *r)
{
	size_t row = get_global_id(0);
    for (size_t t = 0; t < dims.x; ++t)
        v[row * dims.x + r[t]].zw = v[row * dims.x + t].xy;

    for (size_t i = 0; i < dims.rx; ++i)
    {
        size_t m = 1 << i, n = m * 2;
        float arg = -(2 * PI / n);

        for (size_t k = 0; k < m; ++k)
        {
            float2 twiddle = (float2)(cos(arg * k), sin(arg * k));

            for (size_t j = k; j < dims.x; j += n)
            {
                float2 e = v[row * dims.x + j].zw;
                float2 q = v[row * dims.x + (j + m)].zw;

                float2 o = (float2)(twiddle.x * q.x - twiddle.y * q.y,
                                    twiddle.x * q.y + twiddle.y * q.x);

                v[row * dims.x + j].zw       = e + o;
                v[row * dims.x + (j + m)].zw = e - o;
            }
        }
    }

    for (size_t t = 0; t < dims.x; ++t)
        v[row * dims.x + t].zw /= dims.x;
}

void kernel cl_fft_col(global float4 *v, private Params dims, constant uint *r)
{
    size_t col = get_global_id(0);
    for (size_t t = 0; t < dims.y; ++t)
        v[r[t] * dims.x + col].xy = v[t * dims.x + col].zw;

    for (size_t i = 0; i < dims.ry; ++i)
    {
        size_t m = 1 << i, n = m * 2;
        float arg = -(2 * PI / n);

        for (size_t k = 0; k < m; ++k)
        {
            float2 twiddle = (float2)(cos(arg * k), sin(arg * k));

            for (size_t j = k; j < dims.y; j += n)
            {
                float2 e = v[j * dims.x + col].xy;
                float2 q = v[(j + m) * dims.x + col].xy;

                float2 o = (float2)(twiddle.x * q.x - twiddle.y * q.y,
                                    twiddle.x * q.y + twiddle.y * q.x);

                v[j * dims.x + col].xy       = e + o;
                v[(j + m) * dims.x + col].xy = e - o;
            }
        }
    }

    for (size_t t = 0; t < dims.y; ++t)
        v[t * dims.x + col].xy /= dims.y;
}

void kernel cl_fft_normalize(global float4 *v, private Params dims,
                             write_only image2d_t fraunhofer,
                             private float lensDistance)
{
    size_t pixel = get_global_id(0);
    size_t x = pixel % dims.x;
    size_t y = pixel / dims.x;

    size_t cx = (x + dims.x / 2) % dims.x; /* FFT ratios. */
    size_t cy = (y + dims.y / 2) % dims.y * dims.x / dims.y;

    float2 A = v[cx * dims.x + cy].xy;
	float far = pow(LAMBDA * lensDistance, 2);
    float intensity = (A.x * A.x + A.y * A.y) / far;
    write_imagef(fraunhofer, (int2)(x, y), (float4)intensity);
}
