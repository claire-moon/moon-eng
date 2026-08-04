#!/usr/bin/env bash

set -euo pipefail

config="${1:-release}"
case "$config" in
    debug|release) ;;
    *)
        echo "usage: $0 [debug|release] [build-root]" >&2
        exit 2
        ;;
esac

project_dir="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"
build_root="${2:-build}"
case "$build_root" in
    /*) build_dir="$build_root" ;;
    *) build_dir="$project_dir/$build_root" ;;
esac
dosbox_bin="${DOSBOX:-dosbox}"
test_dir="$(mktemp -d /tmp/moon-runtime.XXXXXX)"
empty_conf="$test_dir/EMPTY.CONF"
dosbox_log="$test_dir/DOSBOX.LOG"
dos_output="$test_dir/RUNTIME.OUT"

cleanup() {
    status=$?
    if [[ $status -ne 0 && "${MOON_KEEP_FAILED_TESTS:-0}" == 1 ]]; then
        echo "Retained failed runtime core test directory: $test_dir" >&2
        return
    fi
    if [[ "${MOON_KEEP_TESTS:-0}" == 1 ]]; then
        echo "Retained runtime core test directory: $test_dir" >&2
        return
    fi
    case "$test_dir" in
        /tmp/moon-runtime.*) rm -rf -- "$test_dir" ;;
    esac
}
trap cleanup EXIT HUP INT TERM

for required_command in "$dosbox_bin" timeout grep; do
    if ! command -v "$required_command" >/dev/null 2>&1; then
        echo "Runtime core test prerequisite not found: $required_command" >&2
        exit 2
    fi
done
if [[ ! -f "$build_dir/dos/$config/rtcore.exe" ]] ||
   [[ ! -f "$build_dir/dos/$config/CWSDPMI.EXE" ]]; then
    echo "Runtime core DOS test artifacts are missing under $build_dir/dos/$config" >&2
    exit 2
fi

dosbox_version="$($dosbox_bin -version 2>&1 || true)"
if ! grep -q '^DOSBox version 0\.74-3[, ]' <<<"$dosbox_version"; then
    echo "Runtime core test requires vanilla DOSBox 0.74-3; unexpected version banner:" >&2
    printf '%s\n' "$dosbox_version" >&2
    exit 2
fi
dosbox_version_line="$(grep -m 1 '^DOSBox version ' <<<"$dosbox_version")"

cp "$build_dir/dos/$config/rtcore.exe" "$test_dir/RTCORE.EXE"
cp "$build_dir/dos/$config/CWSDPMI.EXE" "$test_dir/CWSDPMI.EXE"
: >"$empty_conf"
: >"$dosbox_log"

set +e
env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    timeout 120s "$dosbox_bin" -conf "$empty_conf" -noconsole \
    -c "mount c \"$test_dir\"" \
    -c "c:" \
    -c "RTCORE.EXE > RUNTIME.OUT" \
    -c "exit" >"$dosbox_log" 2>&1
dosbox_status=$?
set -e

if [[ $dosbox_status -ne 0 ]]; then
    echo "Vanilla DOSBox runtime core test failed with status $dosbox_status" >&2
    cat "$dosbox_log" >&2
    exit 1
fi
if [[ ! -f "$dos_output" ]] ||
   ! grep -q 'runtime core tests: PASS' "$dos_output"; then
    echo "DOS runtime core test did not produce passing evidence" >&2
    cat "$dosbox_log" >&2
    [[ ! -f "$dos_output" ]] || cat "$dos_output" >&2
    exit 1
fi

printf 'PASS: vanilla DOSBox runtime core (%s)\n' "$config"
printf '%s\n' "$dosbox_version_line"
