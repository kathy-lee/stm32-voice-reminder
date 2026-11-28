#!/bin/sh
# Converts a speech recording into the on-Flash IMA-ADPCM format and a C
# source file the firmware can compile in.
#
# Usage: tools/encode_clip.sh INPUT NAME [START_S [END_S]]
#   INPUT    any audio file FFmpeg reads (WAV, MP3, M4A, ...)
#   NAME     output base name and C symbol, e.g. test_clip
#   START_S  optional: seconds to cut from the start (e.g. a click)
#   END_S    optional: position in seconds where the clip ends
#
# Writes, next to the repository's audio/ folder:
#   audio/NAME.adpcm      raw 256-byte IMA-ADPCM blocks, 8 kHz mono
#   audio/NAME_8k.wav     the same audio decoded again, to listen to on the PC
#   audio/NAME.c          `const uint8_t NAME[]` and `NAME_len` for the firmware
set -eu

if [ $# -lt 2 ]; then
  sed -n '5,10p' "$0"
  exit 1
fi

in="$1"
name="$2"
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/audio"
mkdir -p "$out"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

trim=""
[ $# -ge 3 ] && trim="-ss $3"
[ $# -ge 4 ] && trim="$trim -to $4"

# 1. Trim, mix to mono, remove rumble below 100 Hz, band-limit below the
#    4 kHz Nyquist frequency, and resample to 8 kHz.
# shellcheck disable=SC2086
ffmpeg -hide_banner -loglevel error -y $trim -i "$in" -ac 1 \
  -af "highpass=f=100,lowpass=f=3600,aresample=8000" \
  -c:a pcm_s16le "$tmp/filtered.wav"

# 2. Normalise the peak to -1 dBFS so every clip uses the full range.
peak="$(ffmpeg -hide_banner -nostats -i "$tmp/filtered.wav" -af volumedetect \
  -f null - 2>&1 | sed -n 's/.*max_volume: \(-\{0,1\}[0-9.]*\) dB.*/\1/p')"
boost="$(echo "-1 - ($peak)" | bc -l)"
ffmpeg -hide_banner -loglevel error -y -i "$tmp/filtered.wav" \
  -af "volume=${boost}dB" -c:a pcm_s16le "$tmp/normalised.wav"

# 3. Encode to IMA-ADPCM in 256-byte blocks and keep only the raw blocks.
ffmpeg -hide_banner -loglevel error -y -i "$tmp/normalised.wav" \
  -c:a adpcm_ima_wav -block_size 256 "$tmp/encoded.wav"
ffmpeg -hide_banner -loglevel error -y -i "$tmp/encoded.wav" \
  -map 0:a -c:a copy -f data "$out/$name.adpcm"

# 4. Decode once more for listening: this is what the board will play.
ffmpeg -hide_banner -loglevel error -y -i "$tmp/encoded.wav" \
  -c:a pcm_s16le "$out/${name}_8k.wav"

python3 -I "$root/tools/bin2c.py" "$out/$name.adpcm" "$name" > "$out/$name.c"

bytes="$(wc -c < "$out/$name.adpcm")"
echo "peak was ${peak} dB, boosted by ${boost} dB"
echo "$out/$name.adpcm: $bytes bytes, $((bytes / 256)) blocks," \
  "$(echo "scale=2; $bytes / 256 * 505 / 8000" | bc) s"
