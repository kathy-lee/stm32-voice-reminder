#include "pcm_dac.h"

#include "check.h"

/* Catches: missing mid-scale offset, which would put DC on the amplifier input. */
static void silence_maps_to_midscale_at_any_gain(void)
{
    CHECK_INT(2048, pcm_to_dac(0, 0));
    CHECK_INT(2048, pcm_to_dac(0, 983));
    CHECK_INT(2048, pcm_to_dac(0, PCM_DAC_GAIN_UNITY));
}

/* Catches: wrong 16-bit to 12-bit scaling. */
static void unity_gain_spans_the_full_dac_range(void)
{
    CHECK_INT(4095, pcm_to_dac(32767, PCM_DAC_GAIN_UNITY));
    CHECK_INT(0, pcm_to_dac(-32768, PCM_DAC_GAIN_UNITY));
}

/*
 * Catches: gain not applied, or applied with the wrong fixed-point shift.
 * Hand-derived: gain 0.03 is 983 in Q15; 0.03 * 2048 = 61.4 codes of swing.
 */
static void gain_scales_the_swing_around_midscale(void)
{
    CHECK_INT(2048 + 61, pcm_to_dac(32767, 983));
    CHECK_INT(2048 - 61, pcm_to_dac(-32767, 983));
    CHECK_INT(2048 + 1024, pcm_to_dac(32767, PCM_DAC_GAIN_UNITY / 2));
    CHECK_INT(2048, pcm_to_dac(32767, 0));
}

/*
 * Catches: truncation that rounds positive and negative samples differently,
 * which would shift the average output away from mid-scale.
 * Hand-derived: 100 / 16 = 6.25 codes, rounds to 6 on both sides.
 */
static void positive_and_negative_samples_round_symmetrically(void)
{
    CHECK_INT(2048 + 6, pcm_to_dac(100, PCM_DAC_GAIN_UNITY));
    CHECK_INT(2048 - 6, pcm_to_dac(-100, PCM_DAC_GAIN_UNITY));
}

/* Catches: missing saturation, which would wrap loud samples to the opposite rail. */
static void gain_above_unity_saturates_at_the_rails(void)
{
    CHECK_INT(4095, pcm_to_dac(32767, 65535));
    CHECK_INT(0, pcm_to_dac(-32768, 65535));
}

int main(void)
{
    silence_maps_to_midscale_at_any_gain();
    unity_gain_spans_the_full_dac_range();
    gain_scales_the_swing_around_midscale();
    positive_and_negative_samples_round_symmetrically();
    gain_above_unity_saturates_at_the_rails();

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all PCM-to-DAC tests passed\n");
    return 0;
}
