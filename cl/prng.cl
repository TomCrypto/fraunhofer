#pragma once

/** @file prng.cl
  * @brief Kernel PRNG implementation.
**/

/** This macro converts a 64-bit integer, to a [0..1) float. **/
#define TO_FLOAT(x) ((float)x / (ulong)(18446744073709551615UL))

/** This indicates how many rounds are to be used for the one-way pseudorandom
  * function, higher means greater quality but at a higher computational cost,
  * 4 recommended at a minimum, 9 is more than enough.
**/
#define ROUNDS 4

/* This is a 512-bit > 256-bit one-way function. */
/** One-way pseudorandom function, from 512 bits to 256 bits.
  * @param state The internal state.
  * @param seed The PRNG's seed.
  * @returns A 256-bit pseudorandom output.
**/
void renew(ulong4 *state, ulong4 seed)
{
    /* Retain the PRNG's state. */
    ulong4 block = *state + seed;

    #pragma unroll
    for (ulong t = 0; t < ROUNDS; t++)
    {
        /* Round 2t + 0 (×4 mix & permutation). */
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)(14, 16));
        block.hi ^= block.lo; block = block.xywz;
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)(52, 57));
        block.hi ^= block.lo; block = block.xywz;
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)(23, 40));
        block.hi ^= block.lo; block = block.xywz;
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)( 5, 37));
        block.hi ^= block.lo; block = block.xywz;

        /* Key addition. */
        block += seed;

        /* Round 2t + 1 (×4 mix & permutation). */
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)(25, 33));
        block.hi ^= block.lo; block = block.xywz;
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)(46, 12));
        block.hi ^= block.lo; block = block.xywz;
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)(58, 22));
        block.hi ^= block.lo; block = block.xywz;
        block.lo += block.hi; block.hi = rotate(block.hi, (ulong2)(32, 32));
        block.hi ^= block.lo; block = block.xywz;

        /* Key addition. */
        block += seed;
    }

    /* Feed-forward. */
    *state ^= block;
}

/** @struct PRNG
  * @brief PRNG internal state.
  *
  * This structure contains an instance of PRNG, which is enough information to
  * generate essentially infinitely many unbiased pseudorandom numbers.
**/
typedef struct PRNG
{
    /** @brief The 256-bit internal state. **/
    ulong4 state;
    /** @brief An integer indicating how much of the state has been used. **/
    uint pointer;
    /** @brief A pointer to the PRNG's seed, common to all instances. **/
    ulong4 seed;
} PRNG;

/** This function creates a new PRNG instance, and initializes it to zero.
  * @param ID The ID to create the PRNG instance with, must be unique.
  * @param seed A pointer to the PRNG's seed.
  * @returns The PRNG instance, ready for use.
**/
PRNG init(ulong ID, ulong seed)
{
    PRNG instance;
    instance.state = (ulong4)(ID);
    instance.pointer = 0;
    instance.seed = (ulong4)(seed, 0, 0, 0);
    return instance;
}

/* This function will return a uniform pseudorandom number in [0..1). */
/** This function returns a uniform pseudorandom number in [0..1).
  * @param prng A pointer to the PRNG instance to use.
  * @returns An unbiased uniform pseudorandom number between 0 and 1 exclusive.
**/
float rand(PRNG *prng)
{
    /* Do we need to renew? */
    if (prng->pointer == 0)
    {
         renew(&prng->state, prng->seed);
         prng->pointer = 4;
    }

    /* Return a uniform number in the desired interval. */
    --prng->pointer;
    if (prng->pointer == 3) return TO_FLOAT(prng->state.w);
    if (prng->pointer == 2) return TO_FLOAT(prng->state.z);
    if (prng->pointer == 1) return TO_FLOAT(prng->state.y);
    return TO_FLOAT(prng->state.x);
}
