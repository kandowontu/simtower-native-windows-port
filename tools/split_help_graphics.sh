#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 /path/to/splitmrb /path/to/decompiled-help-directory" >&2
  exit 2
fi

splitmrb=$1
output_dir=$2
cd "$output_dir"

for source in ./*.shg; do
  "$splitmrb" "$source"
done
