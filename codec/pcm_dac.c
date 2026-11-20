#include "pcm_dac.h"

/* 16-bit sample to 12-bit code is >> 4, and Q15 gain is >> 15. */
#define SCALE_SHIFT 19

uint16_t pcm_to_dac(int16_t sample, uint16_t gain_q15)
{
    /* 64-bit: full-scale sample times maximum gain plus the rounding term
     * exceeds INT32_MAX. Adding half before the shift rounds to nearest, so
     * positive and negative samples land symmetrically around mid-scale. */
    int64_t scaled = (int64_t)sample * gain_q15 + (1 << (SCALE_SHIFT - 1));
    int32_t code = PCM_DAC_MIDSCALE + (int32_t)(scaled >> SCALE_SHIFT);

    if (code < 0)
        return 0;
    if (code > 4095)
        return 4095;
    return (uint16_t)code;
}
