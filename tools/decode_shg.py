#!/usr/bin/env python3
"""Decode Windows Help SHG/MRB bitmap containers without platform-size assumptions."""

from __future__ import annotations

import argparse
import json
import struct
from dataclasses import asdict, dataclass
from pathlib import Path

from PIL import Image


@dataclass
class Hotspot:
    kind: int
    flags: int
    reserved: int
    x: int
    y: int
    width: int
    height: int
    hash_or_macro_index: int
    name: str = ""
    target: str = ""


class Reader:
    def __init__(self, data: bytes, offset: int = 0):
        self.data = data
        self.pos = offset

    def take(self, size: int) -> bytes:
        end = self.pos + size
        if end > len(self.data):
            raise ValueError(f"read past end at 0x{self.pos:x} (+{size})")
        value = self.data[self.pos:end]
        self.pos = end
        return value

    def u8(self) -> int:
        return self.take(1)[0]

    def u16(self) -> int:
        return struct.unpack("<H", self.take(2))[0]

    def u32(self) -> int:
        return struct.unpack("<I", self.take(4))[0]

    def cword(self) -> int:
        low = self.u8()
        if low & 1:
            return ((self.u8() << 8) | low) >> 1
        return low >> 1

    def cdword(self) -> int:
        low = self.u16()
        if low & 1:
            return ((self.u16() << 16) | low) >> 1
        return low >> 1

    def cstring(self) -> str:
        end = self.data.find(b"\0", self.pos)
        if end < 0:
            raise ValueError(f"unterminated string at 0x{self.pos:x}")
        value = self.data[self.pos:end].decode("cp1252", errors="replace")
        self.pos = end + 1
        return value


def decode_rle(data: bytes) -> bytes:
    result = bytearray()
    source = Reader(data)
    while source.pos < len(data):
        count = source.u8()
        length = count & 0x7F
        if length == 0:
            continue
        if count & 0x80:
            result.extend(source.take(length))
        else:
            result.extend(source.take(1) * length)
    return bytes(result)


def decode_lz77(data: bytes) -> bytes:
    history = bytearray(0x1000)
    result = bytearray()
    source = Reader(data)
    position = 0
    while source.pos < len(data):
        flags = source.u8()
        for bit in range(8):
            if source.pos >= len(data):
                break
            if flags & (1 << bit):
                token = source.u16()
                length = ((token >> 12) & 0x0F) + 3
                back = position - (token & 0x0FFF) - 1
                for _ in range(length):
                    value = history[back & 0x0FFF]
                    history[position & 0x0FFF] = value
                    result.append(value)
                    position += 1
                    back += 1
            else:
                value = source.u8()
                history[position & 0x0FFF] = value
                result.append(value)
                position += 1
    return bytes(result)


def decompress(method: int, data: bytes) -> bytes:
    if method & ~3:
        raise ValueError(f"unsupported packing method {method}")
    value = decode_lz77(data) if method & 2 else data
    return decode_rle(value) if method & 1 else value


def parse_hotspots(data: bytes, offset: int, size: int) -> list[Hotspot]:
    if not offset or not size:
        return []
    reader = Reader(data, offset)
    if reader.u8() != 1:
        raise ValueError("invalid SHG hotspot marker")
    count = reader.u16()
    macro_size = reader.u32()
    records: list[Hotspot] = []
    for _ in range(count):
        values = struct.unpack("<BBBHHHHI", reader.take(15))
        records.append(Hotspot(*values))
    reader.take(macro_size)
    for record in records:
        record.name = reader.cstring()
        record.target = reader.cstring()
    return records


def bitmap_bytes(
    width: int,
    height: int,
    planes: int,
    bit_count: int,
    x_dpi: int,
    y_dpi: int,
    colors_used: int,
    colors_important: int,
    palette: bytes,
    pixels: bytes,
) -> bytes:
    stride = ((width * bit_count + 31) // 32) * 4
    expected_size = stride * abs(height)
    if len(pixels) < expected_size:
        raise ValueError(f"short bitmap data: expected {expected_size}, decoded {len(pixels)}")
    pixels = pixels[:expected_size]
    offset = 14 + 40 + len(palette)
    file_size = offset + len(pixels)
    file_header = struct.pack("<2sIHHI", b"BM", file_size, 0, 0, offset)
    info_header = struct.pack(
        "<IiiHHIIiiII",
        40,
        width,
        height,
        planes,
        bit_count,
        0,
        len(pixels),
        (x_dpi * 79 + 1) // 2,
        (y_dpi * 79 + 1) // 2,
        colors_used,
        colors_important,
    )
    return file_header + info_header + palette + pixels


def decode_file(source_path: Path, output_dir: Path) -> dict[str, object]:
    data = source_path.read_bytes()
    header = Reader(data)
    magic = header.u16()
    if magic not in (0x506C, 0x706C):
        raise ValueError(f"{source_path}: invalid SHG/MRB magic 0x{magic:04x}")
    count = header.u16()
    offsets = [header.u32() for _ in range(count)]
    result: dict[str, object] = {
        "source": source_path.name,
        "magic": f"0x{magic:04x}",
        "pictures": [],
    }
    pictures = result["pictures"]
    assert isinstance(pictures, list)

    for index, picture_offset in enumerate(offsets):
        picture = Reader(data, picture_offset)
        picture_type = picture.u8()
        packing = picture.u8()
        if picture_type not in (5, 6):
            raise ValueError(f"{source_path}: picture {index} type {picture_type} is not a bitmap")
        x_dpi = picture.cdword()
        y_dpi = picture.cdword()
        planes = picture.cword()
        bit_count = picture.cword()
        width = picture.cdword()
        height = picture.cdword()
        colors_used = picture.cdword()
        palette_entries = colors_used or (1 << bit_count)
        colors_important = picture.cdword()
        compressed_size = picture.cdword()
        hotspot_size = picture.cdword()
        data_offset = picture.u32()
        hotspot_offset = picture.u32()
        palette = picture.take(palette_entries * 4) if picture_type == 6 else b"\0\0\0\0\xff\xff\xff\0"
        packed = data[picture_offset + data_offset:picture_offset + data_offset + compressed_size]
        pixels = decompress(packing, packed)
        stem = source_path.stem if count == 1 else f"{source_path.stem}-{index}"
        bmp_path = output_dir / f"{stem}.bmp"
        png_path = output_dir / f"{stem}.png"
        bmp_path.write_bytes(bitmap_bytes(
            width, height, planes, bit_count, x_dpi, y_dpi,
            colors_used, colors_important, palette, pixels,
        ))
        with Image.open(bmp_path) as image:
            image.save(png_path)
        hotspots = parse_hotspots(
            data,
            picture_offset + hotspot_offset if hotspot_offset else 0,
            hotspot_size,
        )
        pictures.append({
            "index": index,
            "type": picture_type,
            "packing": packing,
            "width": width,
            "height": height,
            "bit_count": bit_count,
            "x_dpi": x_dpi,
            "y_dpi": y_dpi,
            "compressed_size": compressed_size,
            "decoded_size": len(pixels),
            "bmp": bmp_path.name,
            "png": png_path.name,
            "hotspots": [asdict(item) for item in hotspots],
        })
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path, help="SHG/MRB file or directory")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    sources = sorted(args.source.glob("*.shg")) if args.source.is_dir() else [args.source]
    manifest = [decode_file(source, args.output) for source in sources]
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(f"Decoded {sum(len(item['pictures']) for item in manifest)} pictures from {len(sources)} containers")


if __name__ == "__main__":
    main()
