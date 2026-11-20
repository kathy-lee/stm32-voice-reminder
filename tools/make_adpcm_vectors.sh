#!/bin/sh
# Regenerates the ADPCM test vectors with FFmpeg (the trusted reference).
#   chirp.adpcm  raw 256-byte IMA-ADPCM blocks, exactly as stored in Flash
#   chirp.pcm    FFmpeg's own decode of those blocks (s16le, 8 kHz, mono)
# The committed vectors are the source of truth; rerun only on purpose.
set -eu
out="$(dirname "$0")/../tests/vectors"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# Chirp with a slow amplitude envelope: exercises small and large step sizes.
ffmpeg -hide_banner -loglevel error -y -f lavfi \
  -i "aevalsrc=0.9*sin(2*PI*(200+900*t)*t)*(0.05+0.95*abs(sin(3*t))):s=8000:d=2" \
  -ac 1 -c:a adpcm_ima_wav -block_size 256 "$tmp/chirp.wav"

ffmpeg -hide_banner -loglevel error -y -i "$tmp/chirp.wav" -map 0:a -c:a copy -f data "$out/chirp.adpcm"
ffmpeg -hide_banner -loglevel error -y -i "$tmp/chirp.wav" -f s16le -c:a pcm_s16le "$out/chirp.pcm"
ls -l "$out"
