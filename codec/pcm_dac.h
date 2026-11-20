/*
 * Converts decoded PCM into DAC codes, shared by the STM32 firmware and the
 * PC tests.
 */
#ifndef PCM_DAC_H
#define PCM_DAC_H

#include <stdint.h>

#define PCM_DAC_MIDSCALE 2048 /* 12-bit DAC code for silence */
#define PCM_DAC_GAIN_UNITY 32768 /* gain 1.0 in Q15 */

/*
 * Scales a signed 16-bit sample by `gain_q15` (32768 = 1.0, so the range is
 * 0 to just under 2.0) and returns the unsigned 12-bit DAC code, centred on
 * PCM_DAC_MIDSCALE and saturated to 0..4095.
 */
uint16_t pcm_to_dac(int16_t sample, uint16_t gain_q15);

#endif
