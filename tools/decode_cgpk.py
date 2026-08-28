#!/usr/bin/env python3
"""Decode SimTower CGPK resources as raw 288-byte palette-indexed scanlines."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

from PIL import Image


ROW_BYTES = 288


def read_palette(path: Path) -> list[int]:
    data = path.read_bytes()
    if len(data) < 256 * 8:
        raise ValueError(f"{path}: CLUT is shorter than 256 entries")
    palette: list[int] = []
    for offset in range(0, 256 * 8, 8):
        _tag, red, green, blue = struct.unpack_from(">HHHH", data, offset)
        palette.extend((red >> 8, green >> 8, blue >> 8))
    return palette


def content_height(data: bytes) -> int:
    last = next((index for index in range(len(data) - 1, -1, -1) if data[index]), -1)
    if last < 0:
        return 0
    return math.ceil((last + 1) / ROW_BYTES)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("raw", type=Path, help="directory containing CGPK_*.bin and CLUT_*.bin")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    palettes: dict[int, list[int]] = {}
    for path in sorted(args.raw.glob("CLUT_*.bin")):
        palettes[int(path.stem.split("_")[1])] = read_palette(path)
    if not palettes:
        raise ValueError("no CLUT resources found")

    manifest: list[dict[str, object]] = []
    for source in sorted(args.raw.glob("CGPK_*.bin")):
        resource_id = int(source.stem.split("_")[1])
        data = source.read_bytes()
        height = content_height(data)
        content_size = ROW_BYTES * height
        pixels = data[:content_size]
        if len(pixels) != content_size:
            raise ValueError(f"{source}: incomplete final scanline")
        outputs: dict[str, str] = {}
        for palette_id, palette in palettes.items():
            directory = args.output / f"palette-{palette_id}"
            directory.mkdir(parents=True, exist_ok=True)
            destination = directory / f"{resource_id}.png"
            image = Image.frombytes("P", (ROW_BYTES, height), pixels)
            image.putpalette(palette)
            image.save(destination)
            outputs[str(palette_id)] = destination.relative_to(args.output).as_posix()
        manifest.append({
            "resource_id": resource_id,
            "allocated_size": len(data),
            "content_size": content_size,
            "padding_size": len(data) - content_size,
            "width": ROW_BYTES,
            "height": height,
            "sha256": hashlib.sha256(data).hexdigest(),
            "palette_variants": outputs,
        })
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Decoded {len(manifest)} CGPK resources with {len(palettes)} palette variants each")


if __name__ == "__main__":
    main()
