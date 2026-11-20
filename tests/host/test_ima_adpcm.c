#include "ima_adpcm.h"

#include "check.h"

#include <stdio.h>
#include <stdlib.h>

/* Catches: header sample not emitted, or read with the wrong byte order. */
static void header_only_block_yields_its_first_sample(void)
{
    const uint8_t block[] = {0xD2, 0x04, 0, 0}; /* 1234 little-endian */
    int16_t out[1] = {0};

    CHECK_INT(1, ima_adpcm_decode_block(block, sizeof block, out));
    CHECK_INT(1234, out[0]);
}

/*
 * Catches: wrong nibble order, sign bit ignored, step index not advanced.
 * Hand-derived: start 0, index 0 (step 7).
 *   low nibble 0x4:  diff = (2*4+1)*7 >> 3 = 7   ->  7, index 0+2 = 2 (step 9)
 *   high nibble 0xC: diff = (2*4+1)*9 >> 3 = 10  -> -3
 */
static void low_nibble_is_decoded_before_high_nibble(void)
{
    const uint8_t block[] = {0, 0, 0, 0, 0xC4};
    int16_t out[3] = {0};

    CHECK_INT(3, ima_adpcm_decode_block(block, sizeof block, out));
    CHECK_INT(0, out[0]);
    CHECK_INT(7, out[1]);
    CHECK_INT(-3, out[2]);
}

/* Catches: missing saturation, which would wrap loud peaks to the opposite sign. */
static void samples_saturate_at_int16_limits(void)
{
    /* 32767 at the largest step, pushed further up, then far down twice. */
    const uint8_t up[] = {0xFF, 0x7F, 88, 0, 0x07};
    const uint8_t down[] = {0x00, 0x80, 88, 0, 0x0F};
    int16_t out[3] = {0};

    CHECK_INT(3, ima_adpcm_decode_block(up, sizeof up, out));
    CHECK_INT(32767, out[1]);

    CHECK_INT(3, ima_adpcm_decode_block(down, sizeof down, out));
    CHECK_INT(-32768, out[1]);
}

/* Catches: step index running past either end of the 89-entry step table. */
static void step_index_stays_within_table(void)
{
    /* Index 0 with "decrease" nibbles must stay at step 7: 0 -> 0 -> 0 (diff 7>>3 = 0). */
    const uint8_t low[] = {0, 0, 0, 0, 0x00};
    /* Index 88 with "increase" nibbles must stay at step 32767:
     * 0 + (15*32767 >> 3 = 61438) saturates; next 0xF subtracts 61438 -> -28671. */
    const uint8_t high[] = {0, 0, 88, 0, 0xF7};
    int16_t out[3] = {0};

    CHECK_INT(3, ima_adpcm_decode_block(low, sizeof low, out));
    CHECK_INT(0, out[1]);
    CHECK_INT(0, out[2]);

    CHECK_INT(3, ima_adpcm_decode_block(high, sizeof high, out));
    CHECK_INT(32767, out[1]);
    CHECK_INT(-28671, out[2]);
}

/* Catches: corrupt Flash data indexing outside the step table. */
static void corrupt_blocks_are_rejected(void)
{
    const uint8_t bad_index[] = {0, 0, 89, 0, 0x11};
    const uint8_t truncated[] = {0, 0, 0};
    int16_t out[3] = {0};

    CHECK_INT(-1, ima_adpcm_decode_block(bad_index, sizeof bad_index, out));
    CHECK_INT(-1, ima_adpcm_decode_block(truncated, sizeof truncated, out));
}

static unsigned char *read_file(const char *name, size_t *size)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", VECTOR_DIR, name);
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("  FAIL cannot open %s\n", path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    *size = (size_t)ftell(f);
    rewind(f);
    unsigned char *data = malloc(*size);
    if (fread(data, 1, *size, f) != *size) {
        printf("  FAIL cannot read %s\n", path);
        exit(2);
    }
    fclose(f);
    return data;
}

/*
 * Catches: any deviation from FFmpeg's decoder (rounding formula, tables,
 * block framing) on a 2 s chirp that sweeps small and large step sizes.
 * chirp.pcm is FFmpeg's own decode of chirp.adpcm.
 */
static void decoding_matches_ffmpeg_sample_for_sample(void)
{
    size_t adpcm_size, pcm_size;
    unsigned char *adpcm = read_file("chirp.adpcm", &adpcm_size);
    unsigned char *pcm = read_file("chirp.pcm", &pcm_size);
    size_t blocks = adpcm_size / IMA_ADPCM_BLOCK_BYTES;

    CHECK_INT(0, (long)(adpcm_size % IMA_ADPCM_BLOCK_BYTES));
    CHECK_INT((long)(blocks * IMA_ADPCM_SAMPLES_PER_BLOCK * 2), (long)pcm_size);

    long mismatches = 0;
    for (size_t b = 0; b < blocks; b++) {
        int16_t out[IMA_ADPCM_SAMPLES_PER_BLOCK];
        int n = ima_adpcm_decode_block(adpcm + b * IMA_ADPCM_BLOCK_BYTES,
                                       IMA_ADPCM_BLOCK_BYTES, out);
        if (n != IMA_ADPCM_SAMPLES_PER_BLOCK) {
            mismatches += IMA_ADPCM_SAMPLES_PER_BLOCK;
            continue;
        }
        const unsigned char *ref = pcm + b * IMA_ADPCM_SAMPLES_PER_BLOCK * 2;
        for (int i = 0; i < n; i++) {
            int16_t want = (int16_t)(ref[2 * i] | (ref[2 * i + 1] << 8));
            if (out[i] != want)
                mismatches++;
        }
    }
    CHECK_INT(0, mismatches);

    free(adpcm);
    free(pcm);
}

int main(void)
{
    header_only_block_yields_its_first_sample();
    low_nibble_is_decoded_before_high_nibble();
    samples_saturate_at_int16_limits();
    step_index_stays_within_table();
    corrupt_blocks_are_rejected();
    decoding_matches_ffmpeg_sample_for_sample();

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all ADPCM decoder tests passed\n");
    return 0;
}
