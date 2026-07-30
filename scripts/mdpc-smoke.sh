#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 HOST-MDPC" >&2
    exit 2
fi

mdpc="$1"
if [[ ! -x "$mdpc" ]]; then
    echo "mdpc smoke: not executable: $mdpc" >&2
    exit 2
fi

work_dir="$(mktemp -d /tmp/moon-mdpc.XXXXXX)"
cleanup() {
    case "$work_dir" in
        /tmp/moon-mdpc.*) rm -rf -- "$work_dir" ;;
    esac
}
trap cleanup EXIT HUP INT TERM

printf '\001\002\003\004\005' > "$work_dir/MAP.BIN"
printf '\011\010\007' > "$work_dir/PAL.BIN"

"$mdpc" pack "$work_dir/FIRST.MDP" \
    "PAL " 7 2 "$work_dir/PAL.BIN" \
    "MAP " 42 1 "$work_dir/MAP.BIN"
"$mdpc" pack "$work_dir/SECOND.MDP" \
    "MAP " 42 1 "$work_dir/MAP.BIN" \
    "PAL " 7 2 "$work_dir/PAL.BIN"

cmp "$work_dir/FIRST.MDP" "$work_dir/SECOND.MDP"
cp "$work_dir/FIRST.MDP" "$work_dir/SAVED.MDP"
stale_temp="${work_dir}/FIRST.\$\$\$"
backup_path="${work_dir}/FIRST.\$BK"
: > "$stale_temp"
if "$mdpc" pack "$work_dir/FIRST.MDP" \
    "MAP " 42 1 "$work_dir/MAP.BIN" \
    "PAL " 7 2 "$work_dir/PAL.BIN" >/dev/null 2>&1; then
    echo "mdpc smoke: ignored a stale transaction file" >&2
    exit 1
fi
cmp "$work_dir/FIRST.MDP" "$work_dir/SAVED.MDP"
rm -f -- "$stale_temp"
"$mdpc" pack "$work_dir/FIRST.MDP" \
    "MAP " 42 1 "$work_dir/MAP.BIN" \
    "PAL " 7 2 "$work_dir/PAL.BIN"
cmp "$work_dir/FIRST.MDP" "$work_dir/SECOND.MDP"
if [[ -e "$stale_temp" || -e "$backup_path" ]]; then
    echo "mdpc smoke: transaction sidecar was not cleaned" >&2
    exit 1
fi
"$mdpc" validate "$work_dir/FIRST.MDP"
"$mdpc" list "$work_dir/FIRST.MDP" > "$work_dir/LIST.OUT"
grep -q '^MAP  42 1 ' "$work_dir/LIST.OUT"
grep -q '^PAL  7 2 ' "$work_dir/LIST.OUT"

if "$mdpc" pack "$work_dir/BAD.MDP" \
    TEST -1 1 "$work_dir/MAP.BIN" >/dev/null 2>&1; then
    echo "mdpc smoke: accepted a signed asset ID" >&2
    exit 1
fi
if "$mdpc" pack "$work_dir/BAD.MDP" \
    TEST 0x2 1 "$work_dir/MAP.BIN" >/dev/null 2>&1; then
    echo "mdpc smoke: accepted a non-decimal asset ID" >&2
    exit 1
fi
if "$mdpc" pack "$work_dir/BAD.MDP" \
    TEST 4294967296 1 "$work_dir/MAP.BIN" >/dev/null 2>&1; then
    echo "mdpc smoke: accepted an overflowing asset ID" >&2
    exit 1
fi
if "$mdpc" pack "$work_dir/BAD.MDP" \
    TEST 2 0 "$work_dir/MAP.BIN" >/dev/null 2>&1; then
    echo "mdpc smoke: accepted schema zero" >&2
    exit 1
fi

echo "PASS: host MDPC deterministic CLI smoke"
