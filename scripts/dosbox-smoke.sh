#!/usr/bin/env bash

set -euo pipefail

config="${1:-release}"
case "$config" in
    debug|release) ;;
    *)
        echo "usage: $0 [debug|release]" >&2
        exit 2
        ;;
esac

project_dir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
dosbox_bin="${DOSBOX:-dosbox}"
smoke_dir="$(mktemp -d /tmp/moon-dosbox.XXXXXX)"
smoke_log="$smoke_dir/DOSBOX.LOG"
empty_conf="$smoke_dir/EMPTY.CONF"

cleanup() {
    case "$smoke_dir" in
        /tmp/moon-dosbox.*) rm -rf -- "$smoke_dir" ;;
    esac
}
trap cleanup EXIT HUP INT TERM

: > "$empty_conf"
cp "$project_dir/dist/game/$config/ZEUS.EXE" "$smoke_dir/ZEUS.EXE"
cp "$project_dir/dist/game/$config/CWSDPMI.EXE" "$smoke_dir/CWSDPMI.EXE"
cp "$project_dir/dist/game/$config/GAME.MDP" "$smoke_dir/GAME.MDP"

set +e
env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    timeout 15s "$dosbox_bin" -conf "$empty_conf" -noconsole \
    -c "mount c \"$smoke_dir\"" \
    -c "c:" \
    -c "zeus.exe /SMOKE" \
    -c "exit" >"$smoke_log" 2>&1
dosbox_status=$?
set -e

if [[ $dosbox_status -ne 0 ]]; then
    echo "DOSBox smoke failed with status $dosbox_status" >&2
    cat "$smoke_log" >&2
    if [[ -f "$smoke_dir/SMOKE.LOG" ]]; then
        echo "ZEUS smoke phases:" >&2
        cat "$smoke_dir/SMOKE.LOG" >&2
    fi
    exit 1
fi

if [[ ! -f "$smoke_dir/SMOKE.OUT" ]] ||
   ! grep -q $'^PASS\tZEUS.STARTUP\tTICKS\t1' "$smoke_dir/SMOKE.OUT"; then
    echo "ZEUS did not produce valid SMOKE.OUT evidence" >&2
    cat "$smoke_log" >&2
    if [[ -f "$smoke_dir/SMOKE.LOG" ]]; then
        echo "ZEUS smoke phases:" >&2
        cat "$smoke_dir/SMOKE.LOG" >&2
    fi
    if [[ -f "$smoke_dir/SMOKE.OUT" ]]; then
        cat "$smoke_dir/SMOKE.OUT" >&2
    fi
    exit 1
fi

echo "PASS: vanilla DOSBox ZEUS startup smoke ($config)"
