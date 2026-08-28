#!/usr/bin/env python3
"""Decode SimTower's custom NE resources and emit original tuning constants.

The executable allocates every custom resource in a 512-byte NE block, so this
decoder uses each format's own count/header rather than trimming zero padding.
Numeric fields in PART, TABL, TABM, and YEN are big-endian. DTMP begins with
a NUL-terminated optional bitmap-resource number; fields after that prefix
are little-endian, matching the pointer arithmetic in 1070:0005/0231.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path
from typing import Any


PART_HEAD_END = 0x42
PART_LONG_END = 0x52
PART_LOGICAL_SIZE = 0xAE
YEN_ENTRY_COUNT = 45


def be16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def be32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def le16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def cstring(data: bytes, offset: int = 0) -> tuple[str, int]:
    end = data.find(b"\0", offset)
    if end < 0:
        end = len(data)
    return data[offset:end].decode("cp1252", errors="replace"), min(end + 1, len(data))


def resource_id(path: Path) -> int:
    return int(path.stem.rsplit("_", 1)[1])


def source_fields(path: Path, logical_size: int) -> dict[str, Any]:
    data = path.read_bytes()
    return {
        "id": resource_id(path),
        "source": path.name,
        "logicalSize": logical_size,
        "allocatedSize": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
    }


def decode_alert(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    message, end = cstring(data, 4)
    return {
        **source_fields(path, end),
        # The consumer treats these as two native Win16 words.  Their precise
        # UI labels remain unproven, so preserve them without over-naming.
        "headerWordsLe": [le16(data, 0), le16(data, 2)],
        "messageTemplate": message,
    }


def rect(data: bytes, offset: int, endian: str) -> list[int]:
    return list(struct.unpack_from(f"{endian}4H", data, offset))


def decode_dtmp(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    bitmap_reference, cursor = cstring(data)
    first_word = le16(data, cursor)
    second_word = le16(data, cursor + 2)
    item_count = le16(data, cursor + 4)
    logical_size = cursor + 6 + item_count * 8
    if logical_size > len(data):
        raise ValueError(f"{path.name}: DTMP count exceeds allocation")
    rectangles = [rect(data, cursor + 6 + index * 8, "<") for index in range(item_count)]
    result = {
        **source_fields(path, logical_size),
        "format": "bitmap-referenced" if bitmap_reference else "inline-size",
        "prefix": bitmap_reference,
        "itemCount": item_count,
        "rectangles": rectangles,
    }
    if bitmap_reference:
        result["bitmapResourceId"] = int(bitmap_reference)
        result["headerWordsLe"] = [first_word, second_word]
    else:
        result["width"] = first_word
        result["height"] = second_word
    return result


def decode_tabl(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    rid = resource_id(path)
    if rid == 1000:
        item_count = data[0]
        item_start = 2
        item_end = item_start + item_count
        logical_size = 48
        return {
            **source_fields(path, logical_size),
            "format": "facility-catalog",
            "reservedByte": data[1],
            "facilityOrder": list(data[item_start:item_end]),
            "sentinelBytes": data[item_end:32].hex(),
            "trailingU32Be": [be32(data, offset) for offset in range(32, 48, 4)],
        }

    item_count = be16(data, 0)
    logical_size = 2 + item_count * 2
    values = [be16(data, 2 + index * 2) for index in range(item_count)]
    return {
        **source_fields(path, logical_size),
        "format": "rating-layout",
        "entriesU16Be": values,
        "entryBytePairs": [[value >> 8, value & 0xFF] for value in values],
    }


def decode_tabm(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    item_count = be16(data, 0)
    logical_size = 2 + item_count * 2
    return {
        **source_fields(path, logical_size),
        "entriesU16Be": [be16(data, 2 + index * 2) for index in range(item_count)],
    }


def decode_part(path: Path) -> tuple[dict[str, Any], list[int], list[int], list[int]]:
    data = path.read_bytes()
    head = [be16(data, offset) for offset in range(0, PART_HEAD_END, 2)]
    longs = [be32(data, offset) for offset in range(PART_HEAD_END, PART_LONG_END, 4)]
    tail = [be16(data, offset) for offset in range(PART_LONG_END, PART_LOGICAL_SIZE, 2)]
    fields: list[dict[str, Any]] = []
    for offset in list(range(0, PART_HEAD_END, 2)) + list(range(PART_HEAD_END, PART_LONG_END, 4)) + list(range(PART_LONG_END, PART_LOGICAL_SIZE, 2)):
        bits = 32 if PART_HEAD_END <= offset < PART_LONG_END else 16
        value = be32(data, offset) if bits == 32 else be16(data, offset)
        sign_bit = 1 << (bits - 1)
        signed = value - (1 << bits) if value & sign_bit else value
        fields.append({
            "offset": f"0x{offset:02x}",
            "bits": bits,
            "value": value,
            "signedValue": signed,
            # The loader copies raw offset N to DS:dd7a+N; runtime captures use
            # the rebased selector where the same globals appear at e5ee+N.
            "loaderDestination": f"DS:{0xDD7A + offset:04x}",
            "runtimeAddress": f"DS:{0xE5EE + offset:04x}",
        })
    decoded = {
        **source_fields(path, PART_LOGICAL_SIZE),
        "format": "33*u16be + 4*u32be + 46*u16be",
        "fields": fields,
        "knownSemantics": {
            "routeWaitTimeoutTicks": head[0],
            "waitingDelayTicks": head[1],
            "requeueFailureDelayTicks": head[2],
            "routeFailureDelayTicks": head[3],
            "venueUnavailableDelayTicks": head[4],
            "escalatorStopDelayTicks": head[31],
            "stairStopDelayTicks": head[32],
            "ratingScoreThresholds": longs,
            "fireSpreadRate": tail[2],
            "fireVerticalDelay": tail[3],
            "helicopterExtinguishRate": tail[4],
            "helicopterPromptDelay": tail[5],
            "rescueCountdownWithSecurity": tail[6],
            "parkingExpenseRatesFromStar2": tail[37:40],
            "helicopterRescueCostHundreds": tail[36],
            "bombRansomHundredsByStars2To4": tail[40:43],
        },
    }
    return decoded, head, longs, tail


def decode_text(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    value, end = cstring(data)
    return {**source_fields(path, end), "text": value}


def decode_yen(path: Path) -> tuple[dict[str, Any], list[int]]:
    data = path.read_bytes()
    logical_size = YEN_ENTRY_COUNT * 4
    values = [be32(data, index * 4) for index in range(YEN_ENTRY_COUNT)]
    return ({**source_fields(path, logical_size), "entriesU32Be": values}, values)


def typescript_array(name: str, values: list[int]) -> str:
    joined = ", ".join(str(value) for value in values)
    return f"export const {name} = [{joined}] as const;"


def emit_typescript(
    output: Path,
    part_path: Path,
    part_head: list[int],
    part_longs: list[int],
    part_tail: list[int],
    yen_paths: dict[int, Path],
    yen_values: dict[int, list[int]],
) -> None:
    part_hash = hashlib.sha256(part_path.read_bytes()).hexdigest()
    lines = [
        "// Generated by tools/decode_custom_resources.py. Do not hand-edit.",
        f'export const PART_1000_SHA256 = "{part_hash}";',
        typescript_array("PART_1000_U16_HEAD", part_head),
        typescript_array("PART_1000_U32_THRESHOLDS", part_longs),
        typescript_array("PART_1000_U16_TAIL", part_tail),
        "",
    ]
    for rid in sorted(yen_values):
        digest = hashlib.sha256(yen_paths[rid].read_bytes()).hexdigest()
        lines.append(f'export const YEN_{rid}_SHA256 = "{digest}";')
        lines.append(typescript_array(f"YEN_{rid}_U32", yen_values[rid]))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("raw", type=Path, nargs="?", default=Path("original/assets_raw"))
    parser.add_argument("output", type=Path, nargs="?", default=Path("original/assets_converted/custom/manifest.json"))
    parser.add_argument("--typescript", type=Path, default=Path("web/src/sim/original-tuning.generated.ts"))
    args = parser.parse_args()

    raw: Path = args.raw
    part_path = raw / "PART_1000.bin"
    part, part_head, part_longs, part_tail = decode_part(part_path)
    yen_paths = {resource_id(path): path for path in sorted(raw.glob("YEN_*.bin"))}
    yen_decoded: list[dict[str, Any]] = []
    yen_values: dict[int, list[int]] = {}
    for rid, path in yen_paths.items():
        decoded, values = decode_yen(path)
        yen_decoded.append(decoded)
        yen_values[rid] = values

    text_paths = sorted(raw.glob("TEXT_*.bin"))
    manifest = {
        "schemaVersion": 1,
        "source": "Custom resources extracted from the original SimTower Win16 NE executable",
        "byteOrderNotes": {
            "PART_TABL_TABM_YEN": "big-endian",
            "ALRT": "two little-endian header words followed by a NUL-terminated CP1252 template",
            "DTMP": "optional NUL-terminated bitmap ID, then little-endian words",
        },
        "resources": {
            "ALRT": [decode_alert(path) for path in sorted(raw.glob("ALRT_*.bin"))],
            "DTMP": [decode_dtmp(path) for path in sorted(raw.glob("DTMP_*.bin"))],
            "PART": [part],
            "TABL": [decode_tabl(path) for path in sorted(raw.glob("TABL_*.bin"))],
            "TABM": [decode_tabm(path) for path in sorted(raw.glob("TABM_*.bin"))],
            "TEXT": [decode_text(path) for path in text_paths],
            "YEN": yen_decoded,
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    emit_typescript(args.typescript, part_path, part_head, part_longs, part_tail, yen_paths, yen_values)
    total = sum(len(items) for items in manifest["resources"].values())
    print(f"Decoded {total} custom resources to {args.output}")
    print(f"Generated tuning constants at {args.typescript}")


if __name__ == "__main__":
    main()
