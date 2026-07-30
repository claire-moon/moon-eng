#!/usr/bin/env bash

set -euo pipefail

config="${CONFIG:-release}"
target="all"
project_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

if [[ $# -gt 0 && ( "$1" == "debug" || "$1" == "release" ) ]]; then
    config="$1"
    shift
fi

if [[ $# -gt 0 ]]; then
    target="$1"
    shift
fi

if [[ $# -gt 0 ]]; then
    echo "usage: $0 [debug|release] [make-target]" >&2
    exit 2
fi

echo "** [BUILD] CONFIG=${config} TARGET=${target} **"
"${MAKE:-make}" -C "$project_dir" CONFIG="$config" "$target"
echo "** [BUILD] COMPLETE: build/dos/${config} **"
