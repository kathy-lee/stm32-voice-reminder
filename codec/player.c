#include "player.h"

/* Decodes the next block of the clip; returns 0 on corrupt data. */
static int load_next_block(player_t *p)
{
    const uint8_t *block = p->clip + p->next_block * IMA_ADPCM_BLOCK_BYTES;
    int n = ima_adpcm_decode_block(block, IMA_ADPCM_BLOCK_BYTES, p->block);

    if (n <= 0)
        return 0;
    p->block_len = n;
    p->block_pos = 0;
    p->next_block++;
    return 1;
}

static int begin_repetition(player_t *p)
{
    p->next_block = 0;
    p->clip_pos = 0;
    return load_next_block(p);
}

void player_start(player_t *p, const uint8_t *clip, size_t len,
                  uint16_t gain_q15, uint32_t pause_samples)
{
    p->state = PLAYER_IDLE;
    p->clip = clip;
    p->clip_blocks = len / IMA_ADPCM_BLOCK_BYTES;
    p->clip_samples = (uint32_t)(p->clip_blocks * IMA_ADPCM_SAMPLES_PER_BLOCK);
    p->gain_q15 = gain_q15;
    p->pause_samples = pause_samples;
    p->stop_requested = 0;
    p->stop_fade_left = PLAYER_FADE_SAMPLES;

    if (p->clip_blocks > 0 && begin_repetition(p))
        p->state = PLAYER_PLAYING;
}

void player_stop(player_t *p)
{
    p->stop_requested = 1;
}

int player_is_idle(const player_t *p)
{
    return p->state == PLAYER_IDLE;
}

/* Fade factor (0..PLAYER_FADE_SAMPLES) for the current sample. */
static uint32_t envelope(const player_t *p)
{
    uint32_t to_start = p->clip_pos;
    uint32_t to_end = p->clip_samples - 1 - p->clip_pos;
    uint32_t factor = PLAYER_FADE_SAMPLES;

    if (to_start < factor)
        factor = to_start;
    if (to_end < factor)
        factor = to_end;
    if (p->stop_requested && p->stop_fade_left < factor)
        factor = p->stop_fade_left;
    return factor;
}

static uint16_t next_sample(player_t *p)
{
    if (p->state == PLAYER_PAUSE) {
        if (p->stop_requested) {
            p->state = PLAYER_IDLE;
        } else if (p->pause_left > 0) {
            p->pause_left--;
            return PCM_DAC_MIDSCALE;
        } else if (begin_repetition(p)) {
            p->state = PLAYER_PLAYING;
        } else {
            p->state = PLAYER_IDLE;
        }
    }
    if (p->state != PLAYER_PLAYING)
        return PCM_DAC_MIDSCALE;

    int32_t sample = p->block[p->block_pos] * (int32_t)envelope(p) / PLAYER_FADE_SAMPLES;
    uint16_t code = pcm_to_dac((int16_t)sample, p->gain_q15);

    if (p->stop_requested) {
        if (p->stop_fade_left == 0) {
            p->state = PLAYER_IDLE;
            return code;
        }
        p->stop_fade_left--;
    }

    p->block_pos++;
    p->clip_pos++;
    if (p->clip_pos >= p->clip_samples) {
        p->pause_left = p->pause_samples;
        p->state = PLAYER_PAUSE;
    } else if (p->block_pos >= p->block_len && !load_next_block(p)) {
        p->state = PLAYER_IDLE;
    }
    return code;
}

void player_fill(player_t *p, uint16_t *dst, size_t n)
{
    for (size_t i = 0; i < n; i++)
        dst[i] = next_sample(p);
}
