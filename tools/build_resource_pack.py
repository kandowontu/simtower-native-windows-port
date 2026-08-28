#!/usr/bin/env python3
"""Pack every extracted NE resource into one deterministic native RCDATA blob."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


STANDARD_TYPES = {
    1: "CURSOR",
    2: "BITMAP",
    3: "ICON",
    4: "MENU",
    5: "DIALOG",
    6: "STRING",
    9: "ACCELERATOR",
    10: "RCDATA",
    12: "GROUP_CURSOR",
    14: "GROUP_ICON",
}


def normalized_type(value: int | str) -> str:
    if isinstance(value, int):
        return STANDARD_TYPES.get(value, f"TYPE_{value}")
    return value.upper()


def cpp_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    parser.add_argument("asset_directory", type=Path)
    parser.add_argument("pack", type=Path)
    parser.add_argument("header", type=Path)
    parser.add_argument("resource_script", type=Path)
    args = parser.parse_args()

    report: dict[str, Any] = json.loads(args.report.read_text(encoding="utf-8"))
    resources = sorted(
        report["resources"],
        key=lambda item: (
            normalized_type(item["type_id"]),
            0 if isinstance(item["resource_id"], int) else 1,
            str(item["resource_id"]),
        ),
    )

    packed = bytearray()
    descriptors: list[dict[str, Any]] = []
    for item in resources:
        while len(packed) % 8:
            packed.append(0)
        data = (args.asset_directory / item["file"]).read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest != item["sha256"]:
            raise ValueError(f"hash mismatch for {item['file']}: {digest}")
        offset = len(packed)
        packed.extend(data)
        resource_id = item["resource_id"]
        descriptors.append(
            {
                "type": normalized_type(item["type_id"]),
                "numeric_id": int(resource_id) if isinstance(resource_id, int) else -1,
                "string_id": "" if isinstance(resource_id, int) else str(resource_id),
                "offset": offset,
                "size": len(data),
                "sha256": digest,
            }
        )

    args.pack.parent.mkdir(parents=True, exist_ok=True)
    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.resource_script.parent.mkdir(parents=True, exist_ok=True)
    args.pack.write_bytes(packed)

    header_lines = [
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstddef>",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace simtower::generated {",
        "",
        "inline constexpr int kResourcePackRCDATAId = 101;",
        "",
        "struct ResourceDescriptor {",
        "  std::string_view type;",
        "  std::int32_t numeric_id;",
        "  std::string_view string_id;",
        "  std::uint32_t offset;",
        "  std::uint32_t size;",
        "  std::string_view sha256;",
        "};",
        "",
        f"inline constexpr std::array<ResourceDescriptor, {len(descriptors)}> kResources = {{{{",
    ]
    for item in descriptors:
        header_lines.append(
            "  {"
            + ", ".join(
                (
                    cpp_string(item["type"]),
                    str(item["numeric_id"]),
                    cpp_string(item["string_id"]),
                    str(item["offset"]),
                    str(item["size"]),
                    cpp_string(item["sha256"]),
                )
            )
            + "},"
        )
    header_lines.extend(("}};", "", "}  // namespace simtower::generated", ""))
    args.header.write_text("\n".join(header_lines), encoding="utf-8")

    pack_path = str(args.pack.resolve()).replace("\\", "\\\\")
    args.resource_script.write_text(
        f'#include <windows.h>\n101 RCDATA "{pack_path}"\n', encoding="utf-8"
    )
    print(f"Packed {len(descriptors)} resources into {len(packed)} bytes")


if __name__ == "__main__":
    main()
