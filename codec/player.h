/*
 * Streams an IMA-ADPCM clip (see ima_adpcm.h) as 12-bit DAC codes, shared by
 * the STM32 firmware and the PC tests.
 *
 * The clip repeats with a silent pause between repetitions until stopped.
 * Each repetition fades in over its first PLAYER_FADE_SAMPLES and out over
 * its last, and player_stop() fades out from wherever playback is, so the
 * output never jumps between silence and a non-zero level (no clicks).
 *
 * player_fill() is meant to run from the DMA half/complete interrupts;
 * player_start() and player_stop() from the main loop.
 */
#ifndef PLAYER_H
#define PLAYER_H

#include "ima_adpcm.h"
#include "pcm_dac.h"

#include <stddef.h>
#include <stdint.h>

#define PLAYER_FADE_SAMPLES 80 /* 10 ms at 8 kHz */

typedef enum {
    PLAYER_IDLE,
    PLAYER_PLAYING,
    PLAYER_PAUSE,
} player_state_t;

typedef struct {
    const uint8_t *clip;
    size_t clip_blocks;
    uint16_t gain_q15;
    uint32_t pause_samples;

    volatile player_state_t state;
    volatile uint8_t stop_requested;
    uint16_t stop_fade_left;

    size_t next_block;
    uint32_t clip_pos;      /* sample index within the current repetition */
    uint32_t clip_samples;  /* samples per repetition */
    uint32_t pause_left;

    int16_t block[IMA_ADPCM_SAMPLES_PER_BLOCK];
    int block_len;
    int block_pos;
} player_t;

/*
 * Starts playing `clip` (`len` bytes, whole IMA_ADPCM_BLOCK_BYTES blocks)
 * at `gain_q15`, with `pause_samples` of silence between repetitions.
 */
void player_start(player_t *p, const uint8_t *clip, size_t len,
                  uint16_t gain_q15, uint32_t pause_samples);

/* Requests a fade-out; the player is idle once it has completed. */
void player_stop(player_t *p);

/* Writes the next `n` DAC codes; mid-scale whenever nothing is playing. */
void player_fill(player_t *p, uint16_t *dst, size_t n);

int player_is_idle(const player_t *p);

#endif
