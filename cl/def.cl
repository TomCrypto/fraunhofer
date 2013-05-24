#include <prng.cl>

/* Ref. wavelength.. */
#define LAMBDA (575.0f)

#define PI 3.14159265f

#define RADIAN(x) (x * (PI / 180.0f))

typedef struct Params { int x, rx, y, ry; } Params;

constant sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE |
                             CLK_ADDRESS_CLAMP |
                             CLK_FILTER_LINEAR;

/* Colorization parameters. */
#define RINGING 1.25f
#define ROTATE 2.75f
#define BLUR 3.5f
