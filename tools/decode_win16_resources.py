#!/usr/bin/env python3
"""Decode self-delimiting Win16 UI resources extracted from an NE executable."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path
from typing import Any

from PIL import Image


CONTROL_CLASSES = {
    0x80: "BUTTON",
    0x81: "EDIT",
    0x82: "STATIC",
    0x83: "LISTBOX",
    0x84: "SCROLLBAR",
    0x85: "COMBOBOX",
}


class Reader:
    def __init__(self, data: bytes):
        self.data = data
        self.pos = 0

    def take(self, size: int) -> bytes:
        end = self.pos + size
        if end > len(self.data):
            raise ValueError(f"read past end at 0x{self.pos:x}")
        result = self.data[self.pos:end]
        self.pos = end
        return result

    def u8(self) -> int:
        return self.take(1)[0]

    def u16(self) -> int:
        return struct.unpack("<H", self.take(2))[0]

    def i16(self) -> int:
        return struct.unpack("<h", self.take(2))[0]

    def u32(self) -> int:
        return struct.unpack("<I", self.take(4))[0]

    def align(self, amount: int = 2) -> None:
        self.pos = (self.pos + amount - 1) & ~(amount - 1)

    def cstring(self, first: int | None = None) -> str:
        value = bytearray()
        if first is not None:
            value.append(first)
        while True:
            char = self.u8()
            if char == 0:
                return value.decode("cp1252", errors="replace")
            value.append(char)


def name_or_ordinal(reader: Reader, *, predefined: bool = False) -> str | int | None:
    first = reader.u8()
    if first == 0:
        return None
    if first == 0xFF:
        return reader.u16()
    if predefined and first >= 0x80:
        return CONTROL_CLASSES.get(first, f"CLASS_{first:02X}")
    return reader.cstring(first)


def decode_dialog(path: Path) -> dict[str, Any]:
    reader = Reader(path.read_bytes())
    style = reader.u32()
    item_count = reader.u8()
    x, y, width, height = (reader.i16() for _ in range(4))
    menu = name_or_ordinal(reader)
    window_class = name_or_ordinal(reader)
    caption = name_or_ordinal(reader)
    font: dict[str, Any] | None = None
    if style & 0x40:  # DS_SETFONT
        font = {"point_size": reader.u16(), "face": reader.cstring()}
    items: list[dict[str, Any]] = []
    for _ in range(item_count):
        item_x, item_y, item_width, item_height, item_id = (reader.i16() for _ in range(5))
        item_style = reader.u32()
        item_class = name_or_ordinal(reader, predefined=True)
        text = name_or_ordinal(reader)
        extra_size = reader.u8()
        extra = reader.take(extra_size)
        items.append({
            "id": item_id & 0xFFFF,
            "rect": [item_x, item_y, item_width, item_height],
            "style": f"0x{item_style:08x}",
            "class": item_class,
            "text": text,
            "extra_hex": extra.hex(),
        })
    return {
        "source": path.name,
        "style": f"0x{style:08x}",
        "rect": [x, y, width, height],
        "menu": menu,
        "class": window_class,
        "caption": caption,
        "font": font,
        "items": items,
        "logical_size": reader.pos,
        "allocated_size": len(reader.data),
    }


def decode_menu(path: Path) -> dict[str, Any]:
    reader = Reader(path.read_bytes())
    version = reader.u16()
    header_size = reader.u16()
    reader.take(header_size)

    def read_level() -> list[dict[str, Any]]:
        items: list[dict[str, Any]] = []
        while True:
            flags = reader.u16()
            popup = bool(flags & 0x10)
            item_id = None if popup else reader.u16()
            text = reader.cstring()
            item: dict[str, Any] = {
                "flags": f"0x{flags:04x}",
                "id": item_id,
                "text": text,
            }
            if popup:
                item["children"] = read_level()
            items.append(item)
            if flags & 0x80:
                return items

    items = read_level()
    return {
        "source": path.name,
        "version": version,
        "items": items,
        "logical_size": reader.pos,
        "allocated_size": len(reader.data),
    }


def decode_accelerators(path: Path) -> dict[str, Any]:
    reader = Reader(path.read_bytes())
    entries: list[dict[str, Any]] = []
    while True:
        flags = reader.u8()
        key = reader.u16()
        command = reader.u16()
        entries.append({"flags": f"0x{flags:02x}", "key": key, "command": command})
        if flags & 0x80:
            break
    return {
        "source": path.name,
        "entries": entries,
        "logical_size": reader.pos,
        "allocated_size": len(reader.data),
    }


def icon_groups(raw: Path, output: Path) -> list[dict[str, Any]]:
    groups: list[dict[str, Any]] = []
    icon_dir = output / "icons"
    icon_dir.mkdir(parents=True, exist_ok=True)
    for group_path in sorted(raw.glob("GROUP_ICON_*.group_icon")):
        reader = Reader(group_path.read_bytes())
        reserved, resource_type, count = reader.u16(), reader.u16(), reader.u16()
        entries = []
        image_parts = []
        offset = 6 + 16 * count
        for _ in range(count):
            width, height, colors, entry_reserved = struct.unpack("<BBBB", reader.take(4))
            planes, bit_count = reader.u16(), reader.u16()
            size, resource_id = reader.u32(), reader.u16()
            source = raw / f"ICON_{resource_id}.icon"
            image_data = source.read_bytes()[:size]
            entry = struct.pack(
                "<BBBBHHII", width, height, colors, entry_reserved,
                planes, bit_count, len(image_data), offset,
            )
            entries.append(entry)
            image_parts.append(image_data)
            offset += len(image_data)
        destination = icon_dir / f"{group_path.stem.removeprefix('GROUP_ICON_')}.ico"
        destination.write_bytes(
            struct.pack("<HHH", reserved, resource_type, count) + b"".join(entries) + b"".join(image_parts)
        )
        png = destination.with_suffix(".png")
        with Image.open(destination) as image:
            image.convert("RGBA").save(png)
        groups.append({
            "source": group_path.name,
            "count": count,
            "logical_size": reader.pos,
            "ico": destination.name,
            "png": png.name,
        })
    return groups


def render_cursor(resource: bytes) -> Image.Image:
    hotspot_x, hotspot_y = struct.unpack_from("<HH", resource, 0)
    del hotspot_x, hotspot_y
    header = struct.unpack_from("<IiiHHIIiiII", resource, 4)
    header_size, width, doubled_height, planes, bit_count, compression, image_size, *_ = header
    if header_size != 40 or planes != 1 or bit_count != 1 or compression != 0:
        raise ValueError("only the supplied 1-bpp Win16 cursors are supported")
    height = abs(doubled_height) // 2
    palette_offset = 4 + header_size
    palette = [resource[palette_offset + index * 4:palette_offset + index * 4 + 3] for index in range(2)]
    stride = ((width + 31) // 32) * 4
    xor_offset = palette_offset + 8
    and_offset = xor_offset + stride * height
    result = Image.new("RGBA", (width, height))
    for y in range(height):
        source_y = height - 1 - y if doubled_height > 0 else y
        for x in range(width):
            mask = 0x80 >> (x & 7)
            xor_bit = bool(resource[xor_offset + source_y * stride + (x >> 3)] & mask)
            and_bit = bool(resource[and_offset + source_y * stride + (x >> 3)] & mask)
            blue, green, red = palette[1 if xor_bit else 0]
            alpha = 0 if and_bit and not xor_bit else 255
            result.putpixel((x, y), (red, green, blue, alpha))
    return result


def cursor_groups(raw: Path, output: Path) -> list[dict[str, Any]]:
    groups: list[dict[str, Any]] = []
    cursor_dir = output / "cursors"
    cursor_dir.mkdir(parents=True, exist_ok=True)
    for group_path in sorted(raw.glob("GROUP_CURSOR_*.group_cursor")):
        reader = Reader(group_path.read_bytes())
        reserved, resource_type, count = reader.u16(), reader.u16(), reader.u16()
        if count != 1:
            raise ValueError(f"{group_path}: multi-image cursors are not implemented")
        width, doubled_height, planes, bit_count = (reader.u16() for _ in range(4))
        size, resource_id = reader.u32(), reader.u16()
        resource = (raw / f"CURSOR_{resource_id}.cursor").read_bytes()[:size]
        hotspot_x, hotspot_y = struct.unpack_from("<HH", resource, 0)
        image_data = resource[4:]
        display_height = doubled_height // 2
        directory = struct.pack(
            "<HHHBBBBHHII", reserved, resource_type, 1,
            width if width < 256 else 0, display_height if display_height < 256 else 0,
            2, 0, hotspot_x, hotspot_y, len(image_data), 22,
        )
        stem = group_path.stem.removeprefix("GROUP_CURSOR_")
        cur_path = cursor_dir / f"{stem}.cur"
        png_path = cursor_dir / f"{stem}.png"
        cur_path.write_bytes(directory + image_data)
        render_cursor(resource).save(png_path)
        groups.append({
            "source": group_path.name,
            "resource_id": resource_id,
            "width": width,
            "height": display_height,
            "planes": planes,
            "bit_count": bit_count,
            "hotspot": [hotspot_x, hotspot_y],
            "logical_size": reader.pos,
            "cur": cur_path.name,
            "png": png_path.name,
        })
    return groups


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("raw", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    dialogs = []
    for path in sorted(args.raw.glob("DIALOG_*.dialog")):
        try:
            dialogs.append(decode_dialog(path))
        except Exception as error:
            raise ValueError(f"failed to decode {path.name}: {error}") from error
    menus = [decode_menu(path) for path in sorted(args.raw.glob("MENU_*.menu"))]
    accelerators = [decode_accelerators(path) for path in sorted(args.raw.glob("ACCELERATOR_*.accelerator"))]
    manifest = {
        "dialogs": dialogs,
        "menus": menus,
        "accelerators": accelerators,
        "icon_groups": icon_groups(args.raw, args.output),
        "cursor_groups": cursor_groups(args.raw, args.output),
    }
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(
        f"Decoded {len(dialogs)} dialogs, {len(menus)} menus, {len(accelerators)} accelerator tables, "
        f"{len(manifest['icon_groups'])} icons, and {len(manifest['cursor_groups'])} cursors"
    )


if __name__ == "__main__":
    main()
