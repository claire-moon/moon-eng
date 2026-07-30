#!/usr/bin/env bash

set -euo pipefail

config="${CONFIG:-release}"
project_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
if [[ $# -gt 0 && ( "$1" == "debug" || "$1" == "release" ) ]]; then
    config="$1"
    shift
fi

if [[ $# -gt 0 ]]; then
    echo "usage: $0 [debug|release]" >&2
    exit 2
fi

echo "** [WINBUILD] BUILDING CONFIG=${config} **"
"$project_dir/build.sh" "$config" all

# Override DOSBOX when DOSBox is installed somewhere other than PATH, for
# example: DOSBOX='/mnt/c/Program Files (x86)/DOSBox-0.74-3/DOSBox.exe'.
dosbox_bin="${DOSBOX:-dosbox}"
program="${MOON_PROGRAM:-zeus.exe}"
runtime_dir="$project_dir/build/dos/$config"
dosbox_args=()
if [[ -n "${DOSBOX_ARGS:-}" ]]; then
    read -r -a dosbox_args <<< "$DOSBOX_ARGS"
fi

echo "** [WINBUILD] STARTING VANILLA DOSBOX: ${program} **"
cd "$runtime_dir"
exec "$dosbox_bin" "${dosbox_args[@]}" \
    -c "mount c ." \
    -c "c:" \
    -c "$program"
