#!/bin/sh
set -eu

[ "$(uname -s)" = Darwin ] || exit 0

make clean
make SOUND_BACKEND=audioqueue
if nm -u npvz | grep -Eq '(_fork|_execl|_posix_spawn)'; then
    echo "Audio Queue fallback imports process-launch symbols" >&2
    exit 1
fi

echo "Audio Queue fallback build passed"
