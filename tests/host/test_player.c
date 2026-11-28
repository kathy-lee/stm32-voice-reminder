#include "player.h"

#include "check.h"

#include <string.h>

/*
 * Test clips: a block whose nibbles are all 0 at step index 0 decodes to a
 * constant (each nibble adds (2*0+1)*7 >> 3 = 0 and keeps the index at 0).
 * At unity gain, sample 16384 -> DAC 2048 + 16384/16 = 3072 and
 * 8192 -> 2560. Half of 16384 (mid-fade) -> 2560 as well.
 */
#define N IMA_ADPCM_SAMPLES_PER_BLOCK /* 505 samples per block */
#define UNITY PCM_DAC_GAIN_UNITY

static uint8_t clip[2 * IMA_ADPCM_BLOCK_BYTES];

static void make_block(uint8_t *block, int16_t value)
{
    memset(block, 0, IMA_ADPCM_BLOCK_BYTES);
    block[0] = (uint8_t)(value & 0xFF);
    block[1] = (uint8_t)((uint16_t)value >> 8);
}

/* Fills `count` samples and returns the last one. */
static uint16_t skip(player_t *p, size_t count)
{
    uint16_t out[2048];
    player_fill(p, out, count);
    return out[count - 1];
}

/* Fills one sample. */
static uint16_t next(player_t *p)
{
    return skip(p, 1);
}

/* Catches: idle output away from mid-scale (DC step into the amplifier). */
static void idle_player_outputs_midscale(void)
{
    player_t p;
    uint16_t out[4] = {0};

    memset(&p, 0, sizeof p);
    player_start(&p, clip, IMA_ADPCM_BLOCK_BYTES, UNITY, 0);
    player_stop(&p);
    skip(&p, 200); /* let the stop fade complete */

    player_fill(&p, out, 4);
    CHECK_INT(1, player_is_idle(&p));
    CHECK_INT(2048, out[0]);
    CHECK_INT(2048, out[3]);
}

/* Catches: missing fade-in (click at start) or a fade that never reaches full level. */
static void playback_fades_in_from_midscale(void)
{
    player_t p;

    make_block(clip, 16384);
    player_start(&p, clip, IMA_ADPCM_BLOCK_BYTES, UNITY, 1000);

    CHECK_INT(0, player_is_idle(&p));
    CHECK_INT(2048, next(&p));          /* sample 0: factor 0/80 */
    CHECK_INT(2560, skip(&p, 40));      /* sample 40: factor 40/80 */
    CHECK_INT(3072, skip(&p, 60));      /* sample 100: full level */
}

/* Catches: missing fade-out at the end of the clip (click before the pause). */
static void playback_fades_out_at_clip_end(void)
{
    player_t p;

    make_block(clip, 16384);
    player_start(&p, clip, IMA_ADPCM_BLOCK_BYTES, UNITY, 1000);

    CHECK_INT(2560, skip(&p, N - 40));  /* sample 464: 40 samples left */
    CHECK_INT(2048, skip(&p, 40));      /* sample 504, the last: factor 0 */
}

/* Catches: no pause, wrong pause length, or the clip not repeating. */
static void clip_repeats_after_a_silent_pause(void)
{
    player_t p;

    make_block(clip, 16384);
    player_start(&p, clip, IMA_ADPCM_BLOCK_BYTES, UNITY, 10);

    skip(&p, N);
    CHECK_INT(2048, next(&p));          /* pause sample 1 of 10 */
    CHECK_INT(2048, skip(&p, 9));       /* pause sample 10 of 10 */
    CHECK_INT(2048, next(&p));          /* repeat, sample 0: fade-in starts */
    CHECK_INT(3072, skip(&p, 100));     /* repeat, sample 100: full level */
    CHECK_INT(0, player_is_idle(&p));
}

/* Catches: second block not decoded, or the first block replayed instead. */
static void multi_block_clip_plays_each_block_in_order(void)
{
    player_t p;

    make_block(clip, 16384);
    make_block(clip + IMA_ADPCM_BLOCK_BYTES, 8192);
    player_start(&p, clip, 2 * IMA_ADPCM_BLOCK_BYTES, UNITY, 1000);

    CHECK_INT(3072, skip(&p, 300));     /* sample 299, block 1 */
    CHECK_INT(2560, skip(&p, 400));     /* sample 699, block 2 */
}

/* Catches: gain ignored by the player. */
static void gain_is_applied(void)
{
    player_t p;

    make_block(clip, 16384);
    player_start(&p, clip, IMA_ADPCM_BLOCK_BYTES, UNITY / 2, 1000);

    CHECK_INT(2560, skip(&p, 200));     /* 16384 at gain 0.5 -> 512 codes */
}

/* Catches: stop cutting the signal abruptly, or never reaching idle. */
static void stop_fades_out_then_goes_idle(void)
{
    player_t p;

    make_block(clip, 16384);
    player_start(&p, clip, IMA_ADPCM_BLOCK_BYTES, UNITY, 1000);
    skip(&p, 200);                      /* at full level */

    player_stop(&p);
    CHECK_INT(3072, next(&p));          /* stop fade 80/80 */
    CHECK_INT(2560, skip(&p, 40));      /* stop fade 40/80 */
    CHECK_INT(2048, skip(&p, 40));      /* stop fade 0/80 */
    CHECK_INT(1, player_is_idle(&p));
    CHECK_INT(2048, skip(&p, 600));     /* stays silent, no repeat */
}

/* Catches: stop ignored during the pause, so the clip would start again. */
static void stop_during_pause_goes_idle(void)
{
    player_t p;

    make_block(clip, 16384);
    player_start(&p, clip, IMA_ADPCM_BLOCK_BYTES, UNITY, 1000);
    skip(&p, N + 10);                   /* inside the pause */

    player_stop(&p);
    CHECK_INT(2048, skip(&p, 1200));    /* past where the repeat would start */
    CHECK_INT(1, player_is_idle(&p));
}

/* Catches: corrupt Flash data played as noise instead of stopping. */
static void corrupt_block_stops_playback(void)
{
    player_t p;

    make_block(clip, 16384);
    clip[2] = 89; /* step index out of range */
    player_start(&p, clip, IMA_ADPCM_BLOCK_BYTES, UNITY, 0);

    CHECK_INT(2048, skip(&p, 100));
    CHECK_INT(1, player_is_idle(&p));
}

int main(void)
{
    idle_player_outputs_midscale();
    playback_fades_in_from_midscale();
    playback_fades_out_at_clip_end();
    clip_repeats_after_a_silent_pause();
    multi_block_clip_plays_each_block_in_order();
    gain_is_applied();
    stop_fades_out_then_goes_idle();
    stop_during_pause_goes_idle();
    corrupt_block_stops_playback();

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all player tests passed\n");
    return 0;
}
