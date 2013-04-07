#pragma once

#include <CL/cl.hpp>
#include <cstddef>

struct XYZ
{
    cl_float4 data;
    XYZ(float x, float y, float z)
    {
        data.s[0] = x;
        data.s[1] = y;
        data.s[2] = z;
        data.s[3] = 1.0f; /* By convention. */
    }
};

size_t Resolution();

XYZ* Curve();
