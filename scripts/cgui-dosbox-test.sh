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
expected_hash="EC5F78FB"
test_dir="$(mktemp -d /tmp/moon-cgui.XXXXXX)"
empty_conf="$test_dir/EMPTY.CONF"
dosbox_log="$test_dir/DOSBOX.LOG"
batch_file="$test_dir/RUNTEST.BAT"

cleanup() {
    status=$?
    if [[ $status -ne 0 && "${MOON_KEEP_FAILED_TESTS:-0}" == 1 ]]; then
        echo "Retained failed CGUI test directory: $test_dir" >&2
        return
    fi
    if [[ "${MOON_KEEP_TESTS:-0}" == 1 ]]; then
        echo "Retained CGUI test directory: $test_dir" >&2
        return
    fi
    case "$test_dir" in
        /tmp/moon-cgui.*) rm -rf -- "$test_dir" ;;
    esac
}
trap cleanup EXIT HUP INT TERM

for required_command in "$dosbox_bin" timeout grep; do
    if ! command -v "$required_command" >/dev/null 2>&1; then
        echo "CGUI test prerequisite not found: $required_command" >&2
        exit 2
    fi
done

for artifact in cguitest.exe cguipres.exe CWSDPMI.EXE; do
    if [[ ! -f "$build_dir/dos/$config/$artifact" ]]; then
        echo "CGUI DOS artifact is missing: $build_dir/dos/$config/$artifact" >&2
        exit 2
    fi
done

dosbox_version="$("$dosbox_bin" -version 2>&1 || true)"
if ! grep -q '^DOSBox version 0\.74-3[, ]' <<<"$dosbox_version"; then
    echo "CGUI test requires vanilla DOSBox 0.74-3; unexpected version banner:" >&2
    printf '%s\n' "$dosbox_version" >&2
    exit 2
fi
dosbox_version_line="$(grep -m 1 '^DOSBox version ' <<<"$dosbox_version")"

cp "$build_dir/dos/$config/cguitest.exe" "$test_dir/CGUITEST.EXE"
cp "$build_dir/dos/$config/cguipres.exe" "$test_dir/CGUIPRES.EXE"
cp "$build_dir/dos/$config/CWSDPMI.EXE" "$test_dir/CWSDPMI.EXE"
: >"$empty_conf"
: >"$dosbox_log"
printf '%s\r\n' \
    '@ECHO OFF' \
    'CGUITEST.EXE > CGUITEST.OUT' \
    'IF ERRORLEVEL 1 GOTO FAIL' \
    'CGUIPRES.EXE /SMOKE' \
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
    echo "Vanilla DOSBox CGUI test failed with status $dosbox_status" >&2
    cat "$dosbox_log" >&2
    exit 1
fi
if [[ ! -f "$test_dir/SHELL.OK" ]] ||
   ! grep -q '^PASS' "$test_dir/SHELL.OK"; then
    echo "DOS shell did not regain control after the CGUI tests" >&2
    cat "$dosbox_log" >&2
    exit 1
fi
if [[ ! -f "$test_dir/CGUITEST.OUT" ]] ||
   ! grep -q '^CGUI core tests: PASS' "$test_dir/CGUITEST.OUT"; then
    echo "Portable CGUI core tests did not produce passing evidence" >&2
    cat "$dosbox_log" >&2
    [[ ! -f "$test_dir/CGUITEST.OUT" ]] || cat "$test_dir/CGUITEST.OUT" >&2
    exit 1
fi
if [[ ! -f "$test_dir/CGUIPRS.OUT" ]]; then
    echo "CGUI presentation smoke did not create evidence" >&2
    cat "$dosbox_log" >&2
    exit 1
fi

smoke_line="$(grep -m 1 '^CGUI_PRESENT_SMOKE' "$test_dir/CGUIPRS.OUT" || true)"
smoke_line="${smoke_line%$'\r'}"
if [[ ! "$smoke_line" =~ ^CGUI_PRESENT_SMOKE$'\t'PASS$'\t'HASH=([0-9A-F]{8})$'\t'READBACK=([0-9A-F]{8})$ ]] ||
   [[ "${BASH_REMATCH[1]:-frame}" != "$expected_hash" ]] ||
   [[ "${BASH_REMATCH[1]:-frame}" != "${BASH_REMATCH[2]:-readback}" ]]; then
    echo "CGUI presentation/readback evidence is invalid" >&2
    cat "$dosbox_log" >&2
    cat "$test_dir/CGUIPRS.OUT" >&2
    exit 1
fi

printf 'PASS: portable CGUI core and DOS presentation (%s, hash %s)\n' \
    "$config" "${BASH_REMATCH[1]}"
printf '%s\n' "$dosbox_version_line"
