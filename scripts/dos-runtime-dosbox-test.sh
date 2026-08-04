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
test_dir="$(mktemp -d /tmp/moon-dos-runtime.XXXXXX)"
empty_conf="$test_dir/EMPTY.CONF"
dosbox_log="$test_dir/DOSBOX.LOG"
batch_file="$test_dir/RUNTEST.BAT"

cleanup() {
    status=$?
    if [[ $status -ne 0 && "${MOON_KEEP_FAILED_TESTS:-0}" == 1 ]]; then
        echo "Retained failed DOS runtime test directory: $test_dir" >&2
        return
    fi
    if [[ "${MOON_KEEP_TESTS:-0}" == 1 ]]; then
        echo "Retained DOS runtime test directory: $test_dir" >&2
        return
    fi
    case "$test_dir" in
        /tmp/moon-dos-runtime.*) rm -rf -- "$test_dir" ;;
    esac
}
trap cleanup EXIT HUP INT TERM

for required_command in "$dosbox_bin" timeout grep; do
    if ! command -v "$required_command" >/dev/null 2>&1; then
        echo "DOS runtime test prerequisite not found: $required_command" >&2
        exit 2
    fi
done

for artifact in rtdos.exe moon.exe CWSDPMI.EXE; do
    if [[ ! -f "$build_dir/dos/$config/$artifact" ]]; then
        echo "DOS runtime artifact is missing: $build_dir/dos/$config/$artifact" >&2
        exit 2
    fi
done

dosbox_version="$("$dosbox_bin" -version 2>&1 || true)"
if ! grep -q '^DOSBox version 0\.74-3[, ]' <<<"$dosbox_version"; then
    echo "DOS runtime test requires vanilla DOSBox 0.74-3; unexpected version banner:" >&2
    printf '%s\n' "$dosbox_version" >&2
    exit 2
fi
dosbox_version_line="$(grep -m 1 '^DOSBox version ' <<<"$dosbox_version")"

cp "$build_dir/dos/$config/rtdos.exe" "$test_dir/RTDOS.EXE"
cp "$build_dir/dos/$config/moon.exe" "$test_dir/MOON.EXE"
cp "$build_dir/dos/$config/CWSDPMI.EXE" "$test_dir/CWSDPMI.EXE"
: >"$empty_conf"
: >"$dosbox_log"
printf '%s\r\n' \
    '@ECHO OFF' \
    'RTDOS.EXE > RTDOS.OUT' \
    'IF ERRORLEVEL 1 GOTO FAIL' \
    'MOON.EXE /RUNTIME-SMOKE' \
    'IF ERRORLEVEL 1 GOTO FAIL' \
    'ECHO PASS>SHELL.OK' \
    'GOTO END' \
    ':FAIL' \
    'ECHO FAIL>SHELL.OK' \
    ':END' \
    'EXIT' >"$batch_file"

set +e
env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    timeout 120s "$dosbox_bin" -conf "$empty_conf" -noconsole \
    -c "mount c \"$test_dir\"" \
    -c "c:" \
    -c "RUNTEST.BAT" \
    -c "exit" >"$dosbox_log" 2>&1
dosbox_status=$?
set -e

if [[ $dosbox_status -ne 0 ]]; then
    echo "Vanilla DOSBox adapter/consumer test failed with status $dosbox_status" >&2
    cat "$dosbox_log" >&2
    exit 1
fi
if [[ ! -f "$test_dir/SHELL.OK" ]] ||
   ! grep -q '^PASS' "$test_dir/SHELL.OK"; then
    echo "DOS shell did not regain control after the adapter tests" >&2
    cat "$dosbox_log" >&2
    exit 1
fi
if [[ ! -f "$test_dir/RTDOS.OUT" ]] ||
   ! grep -q '^runtime DOS adapter tests: PASS' "$test_dir/RTDOS.OUT"; then
    echo "DOS hardware adapter test did not produce passing evidence" >&2
    cat "$dosbox_log" >&2
    [[ ! -f "$test_dir/RTDOS.OUT" ]] || cat "$test_dir/RTDOS.OUT" >&2
    exit 1
fi
if [[ ! -f "$test_dir/MOONRT.OUT" ]] ||
   ! grep -q $'^MOON_RUNTIME_SMOKE\tPASS\t' "$test_dir/MOONRT.OUT" ||
   ! grep -Eq 'SIMULATION=[1-9][0-9]*' "$test_dir/MOONRT.OUT" ||
   ! grep -Eq 'PRESENTED=[1-9][0-9]*' "$test_dir/MOONRT.OUT" ||
   ! grep -Eq 'DOS_PRESENTED=[1-9][0-9]*' "$test_dir/MOONRT.OUT"; then
    echo "MOON first-consumer smoke did not produce passing evidence" >&2
    cat "$dosbox_log" >&2
    [[ ! -f "$test_dir/MOONRT.OUT" ]] || cat "$test_dir/MOONRT.OUT" >&2
    exit 1
fi

printf 'PASS: DJGPP DOS runtime adapter and MOON consumer (%s)\n' "$config"
printf '%s\n' "$dosbox_version_line"
