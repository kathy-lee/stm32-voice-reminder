/*
 * IMA-ADPCM block decoder, shared by the STM32 firmware and the PC tests.
 *
 * On-Flash format (mono, 4 bits per sample), identical to the data chunk
 * FFmpeg writes for `-c:a adpcm_ima_wav -block_size 256`:
 *
 *   byte 0..1  first sample of the block, int16 little-endian
 *   byte 2     step index for the samples that follow (0..88)
 *   byte 3     reserved
 *   byte 4..   two samples per byte, LOW nibble first
 *
 * Each block is self-contained, so playback can start at any block.
 */
#ifndef IMA_ADPCM_H
#define IMA_ADPCM_H

#include <stddef.h>
#include <stdint.h>

#define IMA_ADPCM_BLOCK_BYTES       256
#define IMA_ADPCM_BLOCK_HEADER      4
#define IMA_ADPCM_SAMPLES_PER_BLOCK (1 + (IMA_ADPCM_BLOCK_BYTES - IMA_ADPCM_BLOCK_HEADER) * 2)

/*
 * Decodes one block of `len` bytes into `out`, which must have room for
 * 1 + (len - 4) * 2 samples. Returns the number of samples written, or -1
 * if the block is shorter than its header or carries an invalid step index.
 */
int ima_adpcm_decode_block(const uint8_t *block, size_t len, int16_t *out);

#endif
