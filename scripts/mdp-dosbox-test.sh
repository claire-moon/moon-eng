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

project_dir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
build_root="${2:-build}"
case "$build_root" in
    /*) build_dir="$build_root" ;;
    *) build_dir="$project_dir/$build_root" ;;
esac
dosbox_bin="${DOSBOX:-dosbox}"
test_dir="$(mktemp -d /tmp/moon-mdptest.XXXXXX)"
dosbox_log="$test_dir/DOSBOX.LOG"
empty_conf="$test_dir/EMPTY.CONF"

cleanup() {
    status=$?
    if [[ $status -ne 0 && "${MOON_KEEP_FAILED_TESTS:-0}" == 1 ]]; then
        echo "Retained failed MDP test directory: $test_dir" >&2
        return
    fi
    case "$test_dir" in
        /tmp/moon-mdptest.*) rm -rf -- "$test_dir" ;;
    esac
}
trap cleanup EXIT HUP INT TERM

: > "$empty_conf"
cp "$build_dir/dos/$config/mdptest.exe" "$test_dir/MDPTEST.EXE"
cp "$build_dir/dos/$config/maptest.exe" "$test_dir/MAPTEST.EXE"
cp "$build_dir/dos/$config/mdpc.exe" "$test_dir/MDPC.EXE"
cp "$build_dir/dos/$config/CWSDPMI.EXE" "$test_dir/CWSDPMI.EXE"
printf '\001\002\003\004\005' > "$test_dir/MAP.BIN"
printf '\115\101\120\061\001\000\000\000\001\000\000\000\001\000\000\000\100\000\000\000\007\000\000\000\360\377\170\000\100\000\001\002\003\310\064\022\005\000\000\000' > "$test_dir/MAP1.BIN"
cp "$test_dir/MAP1.BIN" "$test_dir/BADMAP.BIN"
printf '\000' >> "$test_dir/BADMAP.BIN"
: > "$dosbox_log"

run_dosbox() {
    local label="$1"
    local command
    local dosbox_status
    local -a arguments
    shift

    arguments=(-conf "$empty_conf" -noconsole
               -c "mount c \"$test_dir\""
               -c "c:")
    for command in "$@"; do
        arguments+=(-c "$command")
    done
    arguments+=(-c "exit")

    set +e
    env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
        timeout 60s "$dosbox_bin" "${arguments[@]}" \
        >>"$dosbox_log" 2>&1
    dosbox_status=$?
    set -e
    if [[ $dosbox_status -ne 0 ]]; then
        echo "DOSBox MDP step failed ($label), status $dosbox_status" >&2
        cat "$dosbox_log" >&2
        return 1
    fi
}

run_dosbox codec "mdptest.exe > MDPTEST.OUT"
run_dosbox map-codec "maptest.exe > MAPTEST.OUT"
run_dosbox pack \
    "mdpc.exe pack GOOD.MDP TEST 4294967295 1 MAP.BIN > GOOD.OUT"
run_dosbox validate "mdpc.exe validate GOOD.MDP >> GOOD.OUT"
run_dosbox typed-pack \
    "mdpc.exe pack MAPGOOD.MDP \"MAP \" 7 1 MAP1.BIN > MAPGOOD.OUT"
run_dosbox typed-validate \
    "mdpc.exe validate MAPGOOD.MDP >> MAPGOOD.OUT"
run_dosbox typed-reject \
    "mdpc.exe pack BADMAP.MDP \"MAP \" 7 1 BADMAP.BIN" \
    "if errorlevel 1 echo PASS > BADMAP.OUT"
run_dosbox signed-id \
    "mdpc.exe pack BAD1.MDP TEST -1 1 MAP.BIN" \
    "if errorlevel 1 echo PASS > NEG1.OUT"
run_dosbox hex-id \
    "mdpc.exe pack BAD2.MDP TEST 0x2 1 MAP.BIN" \
    "if errorlevel 1 echo PASS > NEG2.OUT"
run_dosbox overflow-id \
    "mdpc.exe pack BAD3.MDP TEST 4294967296 1 MAP.BIN" \
    "if errorlevel 1 echo PASS > NEG3.OUT"
run_dosbox schema-zero \
    "mdpc.exe pack BAD4.MDP TEST 2 0 MAP.BIN" \
    "if errorlevel 1 echo PASS > NEG4.OUT"

if [[ ! -f "$test_dir/MDPTEST.OUT" ]] ||
   ! grep -q 'PASS: MDP v1 codec tests' "$test_dir/MDPTEST.OUT"; then
    echo "DOS MDP test did not produce passing evidence" >&2
    cat "$dosbox_log" >&2
    [[ ! -f "$test_dir/MDPTEST.OUT" ]] || cat "$test_dir/MDPTEST.OUT" >&2
    exit 1
fi

if [[ ! -f "$test_dir/MAPTEST.OUT" ]] ||
   ! grep -q 'PASS: MDP MAP v1 codec tests' "$test_dir/MAPTEST.OUT"; then
    echo "DOS typed MAP test did not produce passing evidence" >&2
    cat "$dosbox_log" >&2
    [[ ! -f "$test_dir/MAPTEST.OUT" ]] || cat "$test_dir/MAPTEST.OUT" >&2
    exit 1
fi

if [[ ! -f "$test_dir/GOOD.MDP" ]] ||
   [[ ! -f "$test_dir/GOOD.OUT" ]] ||
   ! grep -q 'valid MDP 1.0' "$test_dir/GOOD.OUT"; then
    echo "DOS MDPC positive boundary case failed" >&2
    cat "$dosbox_log" >&2
    [[ ! -f "$test_dir/GOOD.OUT" ]] || cat "$test_dir/GOOD.OUT" >&2
    find "$test_dir" -maxdepth 1 -type f -printf '%f\n' >&2
    exit 1
fi

if [[ ! -f "$test_dir/MAPGOOD.MDP" ]] ||
   [[ ! -f "$test_dir/MAPGOOD.OUT" ]] ||
   ! grep -q 'valid MDP 1.0' "$test_dir/MAPGOOD.OUT" ||
   [[ ! -f "$test_dir/BADMAP.OUT" ]] ||
   ! grep -q 'PASS' "$test_dir/BADMAP.OUT"; then
    echo "DOS MDPC typed MAP validation cases failed" >&2
    cat "$dosbox_log" >&2
    [[ ! -f "$test_dir/MAPGOOD.OUT" ]] || cat "$test_dir/MAPGOOD.OUT" >&2
    exit 1
fi

for evidence in NEG1.OUT NEG2.OUT NEG3.OUT NEG4.OUT; do
    if [[ ! -f "$test_dir/$evidence" ]] ||
       ! grep -q 'PASS' "$test_dir/$evidence"; then
        echo "DOS MDPC numeric rejection evidence missing: $evidence" >&2
        cat "$dosbox_log" >&2
        exit 1
    fi
done

echo "PASS: vanilla DOSBox MDP v1 codec and CLI test ($config)"
