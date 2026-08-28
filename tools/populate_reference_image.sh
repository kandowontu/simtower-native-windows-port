#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 3 || $((($# - 1) % 2)) -ne 0 ]]; then
  echo "usage: $0 IMAGE SAMPLE_TDT DEST_NAME [SAMPLE_TDT DEST_NAME ...]" >&2
  exit 2
fi

image=$1
shift
mount_point=/tmp/simtower-reference-mount

cleanup() {
  if mountpoint -q "$mount_point"; then
    umount "$mount_point"
  fi
  rmdir "$mount_point" 2>/dev/null || true
}
trap cleanup EXIT

mkdir -p "$mount_point"
mount -t vfat -o loop,offset=32256 "$image" "$mount_point"
while [[ $# -gt 0 ]]; do
  sample=$1
  destination=$2
  cp "$sample" "$mount_point/MAXIS/SIMTOWER/$destination"
  shift 2
done
sync
find "$mount_point/MAXIS/SIMTOWER" -maxdepth 1 -iname '*.TDT' -printf '%f %s bytes\n' | sort
