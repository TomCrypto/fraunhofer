void kernel cl_lens(global float4 *render, private Params dims,
                    read_only image2d_t fraunhofer,
                    read_only image2d_t spectrum,
                    private float lambda,
                    private ulong seed)
{
    float wavelength = (lambda * 390 + 390);
    size_t index = get_global_id(0);
    PRNG prng = init(index, seed);

    size_t px = index % dims.x, py = index / dims.x;
    if (px < dims.x / 2) { px = dims.x - px; py = dims.y - py; }
    float dx = BLUR * (rand(&prng) - 0.5f) + (float)px / dims.x;
    float dy = BLUR * (rand(&prng) - 0.5f) + (float)py / dims.y;
    dx -= 0.5f; dy -= 0.5f;

    float sx = dx * (wavelength / LAMBDA);
    float sy = dy * (wavelength / LAMBDA);

    float r = (rand(&prng) > 0.5f) ? 1.0f : -1.0f;
    float angle = r * (1.0f - pow(rand(&prng), RINGING)) * RADIAN(ROTATE);

    float rx = sx, ry = sy;
    sx = rx * cos(angle) + ry * sin(angle);
    sy = ry * cos(angle) - rx * sin(angle);

    sx += 0.5f; sy += 0.5f;
    float intensity = read_imagef(fraunhofer, sampler, (float2)(sx, sy)).x;
    float3 xyz = read_imagef(spectrum, sampler, (float2)(lambda, 0)).xyz;

    render[index] += (float4)(xyz * intensity, 1.0f);
}
