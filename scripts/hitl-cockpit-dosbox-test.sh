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
fixture_dir="$project_dir/tests/hitl/fixtures"
test_dir="$(mktemp -d /tmp/moon-hitl-cockpit.XXXXXX)"
empty_conf="$test_dir/EMPTY.CONF"
dosbox_log="$test_dir/DOSBOX.LOG"
batch_file="$test_dir/RUNTEST.BAT"

cleanup() {
    status=$?
    if [[ $status -ne 0 && "${MOON_KEEP_FAILED_TESTS:-0}" == 1 ]]; then
        echo "Retained failed HITL cockpit directory: $test_dir" >&2
        return
    fi
    if [[ "${MOON_KEEP_TESTS:-0}" == 1 ]]; then
        echo "Retained HITL cockpit directory: $test_dir" >&2
        return
    fi
    case "$test_dir" in
        /tmp/moon-hitl-cockpit.*) rm -rf -- "$test_dir" ;;
    esac
}
trap cleanup EXIT HUP INT TERM

for required_command in "$dosbox_bin" timeout grep; do
    if ! command -v "$required_command" >/dev/null 2>&1; then
        echo "HITL cockpit prerequisite not found: $required_command" >&2
        exit 2
    fi
done

for artifact in moon.exe CWSDPMI.EXE; do
    if [[ ! -f "$build_dir/dos/$config/$artifact" ]]; then
        echo "HITL cockpit artifact is missing: $build_dir/dos/$config/$artifact" >&2
        exit 2
    fi
done
for fixture in HITL.IN AUTO.OUT; do
    if [[ ! -f "$fixture_dir/$fixture" ]]; then
        echo "HITL cockpit fixture is missing: $fixture_dir/$fixture" >&2
        exit 2
    fi
done

dosbox_version="$("$dosbox_bin" -version 2>&1 || true)"
if ! grep -q '^DOSBox version 0\.74-3[, ]' <<<"$dosbox_version"; then
    echo "HITL cockpit test requires vanilla DOSBox 0.74-3; unexpected version banner:" >&2
    printf '%s\n' "$dosbox_version" >&2
    exit 2
fi
dosbox_version_line="$(grep -m 1 '^DOSBox version ' <<<"$dosbox_version")"

cp "$build_dir/dos/$config/moon.exe" "$test_dir/MOON.EXE"
cp "$build_dir/dos/$config/CWSDPMI.EXE" "$test_dir/CWSDPMI.EXE"
cp "$fixture_dir/HITL.IN" "$test_dir/HITL.IN"
cp "$fixture_dir/AUTO.OUT" "$test_dir/AUTO.OUT"
: >"$empty_conf"
: >"$dosbox_log"
printf '%s\r\n' \
    '@ECHO OFF' \
    'MOON.EXE /HITL HITL.IN /SMOKE' \
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
    echo "Vanilla DOSBox HITL cockpit smoke failed with status $dosbox_status" >&2
    cat "$dosbox_log" >&2
    exit 1
fi
if [[ ! -f "$test_dir/SHELL.OK" ]] ||
   ! grep -q '^PASS' "$test_dir/SHELL.OK"; then
    echo "DOS shell did not regain control after the HITL cockpit smoke" >&2
    cat "$dosbox_log" >&2
    exit 1
fi
if [[ ! -f "$test_dir/HITLSMOK.OUT" ]]; then
    echo "HITL cockpit smoke did not create evidence" >&2
    cat "$dosbox_log" >&2
    exit 1
fi

smoke_line="$(grep -m 1 '^HITL_COCKPIT_SMOKE' "$test_dir/HITLSMOK.OUT" || true)"
smoke_line="${smoke_line%$'\r'}"
IFS=$'\t' read -r smoke_kind smoke_status smoke_provenance smoke_frames \
    smoke_events smoke_manual smoke_hash smoke_readback smoke_commit \
    smoke_extra <<<"$smoke_line"
frame_hash="${smoke_hash#HASH=}"
readback_hash="${smoke_readback#READBACK=}"
if [[ "$smoke_kind" != HITL_COCKPIT_SMOKE ]] ||
   [[ "$smoke_status" != PASS ]] ||
   [[ "$smoke_provenance" != PROVENANCE=SYNTHETIC ]] ||
   [[ ! "$smoke_frames" =~ ^FRAMES=[1-9][0-9]*$ ]] ||
   [[ ! "$smoke_events" =~ ^EVENTS=[1-9][0-9]*$ ]] ||
   [[ "$smoke_manual" != MANUAL=0 ]] ||
   [[ ! "$frame_hash" =~ ^[0-9A-F]{8}$ ]] ||
   [[ ! "$readback_hash" =~ ^[0-9A-F]{8}$ ]] ||
   [[ "$frame_hash" != "$readback_hash" ]] ||
   [[ "$smoke_commit" != COMMIT=PASS ]] ||
   [[ -n "$smoke_extra" ]]; then
    echo "HITL cockpit smoke evidence is invalid" >&2
    cat "$dosbox_log" >&2
    cat "$test_dir/HITLSMOK.OUT" >&2
    exit 1
fi

if [[ ! -f "$test_dir/HITL.JRN" ]] ||
   [[ ! -f "$test_dir/HITL.OUT" ]]; then
    echo "HITL cockpit did not produce its journal and committed summary" >&2
    exit 1
fi
if grep -q $'^EVENT\t[^\t]*\t[^\t]*\tMANUAL\t' "$test_dir/HITL.JRN"; then
    echo "Synthetic HITL smoke illegally appended a MANUAL journal event" >&2
    cat "$test_dir/HITL.JRN" >&2
    exit 1
fi
if grep -q $'^RESULT\t[^\t]*\t[^\t]*\t\(PASS\|FAIL\|BLOCKED\)\t' \
        "$test_dir/HITL.OUT"; then
    echo "Synthetic HITL smoke illegally committed a MANUAL result" >&2
    cat "$test_dir/HITL.OUT" >&2
    exit 1
fi
if [[ -e "$test_dir/HITL.NEW" ]] || [[ -e "$test_dir/HITL.OLD" ]]; then
    echo "HITL cockpit left transactional summary debris" >&2
    exit 1
fi

printf 'PASS: synthetic HITL cockpit DOS integration (%s, hash %s)\n' \
    "$config" "$frame_hash"
printf '%s\n' "$dosbox_version_line"
