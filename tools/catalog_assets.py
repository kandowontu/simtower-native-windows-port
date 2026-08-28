#!/usr/bin/env python3
"""Convert standard SimTower NE resources and produce an asset catalog."""

from __future__ import annotations

import argparse
import json
import math
import shutil
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


PRESERVED_RAW_TYPES = {"ALRT", "DTMP", "TABL", "TABM", "PART", "TEXT", "YEN"}


def parse_strl(blob: bytes) -> list[str]:
    """Parse the game's big-endian count + Pascal-string STRL resources."""
    if len(blob) < 2:
        return []
    count = struct.unpack_from(">H", blob, 0)[0]
    cursor = 2
    strings: list[str] = []
    for _ in range(count):
        if cursor >= len(blob):
            break
        length = blob[cursor]
        cursor += 1
        strings.append(blob[cursor : cursor + length].decode("cp1252", "replace"))
        cursor += length
    return strings


def convert_bitmap(source: Path, destination: Path) -> dict[str, object]:
    with Image.open(source) as image:
        image.load()
        metadata = {
            "width": image.width,
            "height": image.height,
            "mode": image.mode,
            "colors": len(image.getcolors(maxcolors=1 << 24) or []),
        }
        destination.parent.mkdir(parents=True, exist_ok=True)
        image.save(destination, format="PNG")
        return metadata


def convert_wave(blob: bytes, destination: Path) -> dict[str, object]:
    if blob[:4] != b"RIFF" or blob[8:12] != b"WAVE":
        return {"container": "raw", "actual_size": len(blob)}
    riff_size = struct.unpack_from("<I", blob, 4)[0] + 8
    actual_size = min(riff_size, len(blob))
    trimmed = blob[:actual_size]
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(trimmed)

    cursor = 12
    audio: dict[str, object] = {"container": "WAVE", "actual_size": actual_size}
    while cursor + 8 <= len(trimmed):
        chunk_id = trimmed[cursor : cursor + 4]
        chunk_size = struct.unpack_from("<I", trimmed, cursor + 4)[0]
        payload = cursor + 8
        if chunk_id == b"fmt " and chunk_size >= 16 and payload + 16 <= len(trimmed):
            fmt, channels, rate, byte_rate, block_align, bits = struct.unpack_from(
                "<HHIIHH", trimmed, payload
            )
            audio.update(
                {
                    "format": fmt,
                    "channels": channels,
                    "sample_rate": rate,
                    "byte_rate": byte_rate,
                    "block_align": block_align,
                    "bits_per_sample": bits,
                }
            )
        elif chunk_id == b"data":
            audio["sample_bytes"] = min(chunk_size, max(0, len(trimmed) - payload))
        cursor = payload + chunk_size + (chunk_size & 1)
    if audio.get("byte_rate"):
        audio["duration_seconds"] = round(
            int(audio.get("sample_bytes", 0)) / int(audio["byte_rate"]), 6
        )
    return audio


def convert_clut(blob: bytes, destination: Path) -> dict[str, object]:
    if len(blob) < 8 or len(blob) % 8:
        return {"entries": 0}
    colors: list[tuple[int, int, int]] = []
    records: list[dict[str, int]] = []
    for cursor in range(0, len(blob), 8):
        tag, red, green, blue = struct.unpack_from(">HHHH", blob, cursor)
        rgb = (red >> 8, green >> 8, blue >> 8)
        colors.append(rgb)
        records.append({"tag": tag, "red": red, "green": green, "blue": blue})
    side = math.ceil(math.sqrt(len(colors)))
    swatch = 16
    image = Image.new("RGB", (side * swatch, side * swatch), "black")
    draw = ImageDraw.Draw(image)
    for index, color in enumerate(colors):
        x = (index % side) * swatch
        y = (index // side) * swatch
        draw.rectangle((x, y, x + swatch - 1, y + swatch - 1), fill=color)
    destination.parent.mkdir(parents=True, exist_ok=True)
    image.save(destination, format="PNG")
    destination.with_suffix(".json").write_text(
        json.dumps(records, indent=2) + "\n", encoding="utf-8"
    )
    return {"entries": len(colors)}


def make_contact_sheets(bitmaps: list[tuple[str, Path]], output: Path) -> list[str]:
    columns = 6
    cell_width = 220
    cell_height = 150
    per_sheet = 48
    font = ImageFont.load_default()
    sheets: list[str] = []
    output.mkdir(parents=True, exist_ok=True)
    for sheet_number, start in enumerate(range(0, len(bitmaps), per_sheet), 1):
        batch = bitmaps[start : start + per_sheet]
        rows = math.ceil(len(batch) / columns)
        sheet = Image.new("RGB", (columns * cell_width, rows * cell_height), "#d0d0d0")
        draw = ImageDraw.Draw(sheet)
        for index, (label, image_path) in enumerate(batch):
            x = (index % columns) * cell_width
            y = (index // columns) * cell_height
            draw.rectangle(
                (x + 1, y + 1, x + cell_width - 2, y + cell_height - 2),
                fill="white",
                outline="#808080",
            )
            with Image.open(image_path) as source:
                source.load()
                thumbnail = source.convert("RGB")
                thumbnail.thumbnail((cell_width - 12, cell_height - 28), Image.Resampling.NEAREST)
                px = x + (cell_width - thumbnail.width) // 2
                py = y + 18 + (cell_height - 24 - thumbnail.height) // 2
                sheet.paste(thumbnail, (px, py))
            draw.text((x + 5, y + 4), label, fill="black", font=font)
        filename = f"bitmaps-{sheet_number:02d}.png"
        sheet.save(output / filename, format="PNG")
        sheets.append(filename)
    return sheets


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("ne_report", type=Path)
    parser.add_argument("raw", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    report = json.loads(args.ne_report.read_text(encoding="utf-8"))
    args.output.mkdir(parents=True, exist_ok=True)
    catalog: list[dict[str, object]] = []
    bitmaps: list[tuple[str, Path]] = []
    for resource in report["resources"]:
        filename = resource.get("file")
        if not filename:
            continue
        source = args.raw / filename
        blob = source.read_bytes()
        item = dict(resource)
        item["allocated_size"] = len(blob)
        type_id = resource["type_id"]
        resource_id = resource["resource_id"]
        if type_id == 2:
            destination = args.output / "bitmaps" / f"{resource_id}.png"
            item.update(convert_bitmap(source, destination))
            item["converted_file"] = destination.relative_to(args.output).as_posix()
            bitmaps.append((f"BITMAP {resource_id}", destination))
        elif type_id == "WAVE":
            destination = args.output / "audio" / f"{resource_id}.wav"
            item.update(convert_wave(blob, destination))
            if destination.exists():
                item["converted_file"] = destination.relative_to(args.output).as_posix()
            else:
                raw_destination = args.output / "audio" / f"{resource_id}.raw"
                raw_destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, raw_destination)
                item["converted_file"] = raw_destination.relative_to(args.output).as_posix()
        elif type_id == "STRL":
            strings = parse_strl(blob)
            destination = args.output / "strings" / f"{resource_id}.json"
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(json.dumps(strings, indent=2) + "\n", encoding="utf-8")
            item["string_count"] = len(strings)
            item["converted_file"] = destination.relative_to(args.output).as_posix()
        elif type_id == "CLUT":
            destination = args.output / "palettes" / f"{resource_id}.png"
            item.update(convert_clut(blob, destination))
            item["converted_file"] = destination.relative_to(args.output).as_posix()
        elif type_id in PRESERVED_RAW_TYPES:
            destination = args.output / "raw" / filename
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)
            item["converted_file"] = destination.relative_to(args.output).as_posix()
        catalog.append(item)

    sheets = make_contact_sheets(bitmaps, args.output / "contact-sheets")
    catalog_path = args.output / "catalog.json"
    catalog_path.write_text(
        json.dumps({"resources": catalog, "contact_sheets": sheets}, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"cataloged {len(catalog)} resources")
    print(f"converted {len(bitmaps)} bitmaps into {len(sheets)} contact sheets")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
