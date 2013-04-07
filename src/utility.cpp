#include <utility.hpp>

uint32_t reverse(uint32_t x, uint32_t radix)
{
    x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
    x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
    x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
    x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
    return (((x >> 16) | (x << 16)) >> (32 - radix));
}

void ReversalTable(uint32_t size, uint32_t radix, uint32_t *table)
{
    for (uint32_t t = 0; t < size; ++t) table[t] = reverse(t, radix);
}

size_t radix(size_t n)
{
    size_t m = 0;
    if (n & (n - 1)) return 0;
    while ((n /= 2) != 0) m++;
    return m;
}
