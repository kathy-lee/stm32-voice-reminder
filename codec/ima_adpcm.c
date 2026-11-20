#include "ima_adpcm.h"

#define MAX_STEP_INDEX 88

static const int16_t step_table[MAX_STEP_INDEX + 1] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,
    19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
    130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
    337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
    876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
    2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
    5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
};

static const int8_t index_adjust[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

int ima_adpcm_decode_block(const uint8_t *block, size_t len, int16_t *out)
{
    if (len < IMA_ADPCM_BLOCK_HEADER || block[2] > MAX_STEP_INDEX)
        return -1;

    int32_t sample = (int16_t)(block[0] | (block[1] << 8));
    int index = block[2];
    int count = 0;

    out[count++] = (int16_t)sample;

    for (size_t i = IMA_ADPCM_BLOCK_HEADER; i < len; i++) {
        for (int shift = 0; shift <= 4; shift += 4) {
            int nibble = (block[i] >> shift) & 0x0F;
            int magnitude = nibble & 0x07;

            /* FFmpeg's rounding, not the shift-and-add form of the IMA
             * reference: the two differ by a few LSBs, and the decoder must
             * track the encoder's predictor exactly or the error accumulates. */
            int32_t diff = ((2 * magnitude + 1) * step_table[index]) >> 3;
            sample += (nibble & 0x08) ? -diff : diff;
            if (sample > INT16_MAX)
                sample = INT16_MAX;
            else if (sample < INT16_MIN)
                sample = INT16_MIN;

            index += index_adjust[magnitude];
            if (index < 0)
                index = 0;
            else if (index > MAX_STEP_INDEX)
                index = MAX_STEP_INDEX;

            out[count++] = (int16_t)sample;
        }
    }
    return count;
}
