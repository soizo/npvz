#!/bin/sh
set -eu

log="/tmp/npvz_audio_log_${PPID}"
gate="/tmp/npvz_audio_gate_${PPID}"
fail="/tmp/npvz_audio_fail_${PPID}"
last=""
for argument in "$@"; do last=$argument; done
printf '%s %s\n' "$$" "$last" >> "$log"
[ ! -e "$fail" ] || exit 127
while [ -e "$gate" ]; do sleep 0.01; done
