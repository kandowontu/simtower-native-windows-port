#!/usr/bin/env python3
"""Inspect and extract a 16-bit Windows New Executable (NE) image."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import asdict, dataclass
from pathlib import Path


RESOURCE_TYPES = {
    1: "CURSOR",
    2: "BITMAP",
    3: "ICON",
    4: "MENU",
    5: "DIALOG",
    6: "STRING",
    7: "FONTDIR",
    8: "FONT",
    9: "ACCELERATOR",
    10: "RCDATA",
    11: "MESSAGETABLE",
    12: "GROUP_CURSOR",
    14: "GROUP_ICON",
    15: "NAMETABLE",
    16: "VERSION",
}


@dataclass
class Segment:
    number: int
    offset: int
    size: int
    flags: int
    minimum_allocation: int
    relocation_count: int = 0


@dataclass
class Resource:
    type_id: int | str
    resource_id: int | str
    offset: int
    size: int
    flags: int
    sha256: str
    file: str | None = None


@dataclass
class EntryPoint:
    ordinal: int
    segment: int
    offset: int
    flags: int
    name: str | None = None


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def counted_string(data: bytes, offset: int) -> tuple[str, int]:
    length = data[offset]
    start = offset + 1
    end = start + length
    return data[start:end].decode("cp1252", "replace"), end


def parse_name_table(data: bytes, offset: int, limit: int | None = None) -> dict[int, str]:
    names: dict[int, str] = {}
    end = len(data) if limit is None else min(len(data), offset + limit)
    cursor = offset
    while cursor < end and data[cursor]:
        name, cursor = counted_string(data, cursor)
        if cursor + 2 > end:
            break
        ordinal = u16(data, cursor)
        cursor += 2
        names[ordinal] = name
    return names


class NewExecutable:
    def __init__(self, path: Path) -> None:
        self.path = path
        self.data = path.read_bytes()
        if self.data[:2] != b"MZ":
            raise ValueError("not an MZ executable")
        self.ne_offset = u32(self.data, 0x3C)
        if self.data[self.ne_offset : self.ne_offset + 2] != b"NE":
            raise ValueError("not a New Executable (NE) image")
        ne = self.ne_offset
        self.entry_table_offset = ne + u16(self.data, ne + 0x04)
        self.entry_table_size = u16(self.data, ne + 0x06)
        self.flags = u16(self.data, ne + 0x0C)
        self.auto_data_segment = u16(self.data, ne + 0x0E)
        self.initial_heap = u16(self.data, ne + 0x10)
        self.initial_stack = u16(self.data, ne + 0x12)
        self.initial_ip = u16(self.data, ne + 0x14)
        self.initial_cs = u16(self.data, ne + 0x16)
        self.initial_sp = u16(self.data, ne + 0x18)
        self.initial_ss = u16(self.data, ne + 0x1A)
        self.segment_count = u16(self.data, ne + 0x1C)
        self.module_count = u16(self.data, ne + 0x1E)
        self.nonresident_size = u16(self.data, ne + 0x20)
        self.segment_table_offset = ne + u16(self.data, ne + 0x22)
        self.resource_table_offset = ne + u16(self.data, ne + 0x24)
        self.resident_name_offset = ne + u16(self.data, ne + 0x26)
        self.module_reference_offset = ne + u16(self.data, ne + 0x28)
        self.imported_name_offset = ne + u16(self.data, ne + 0x2A)
        self.nonresident_name_offset = u32(self.data, ne + 0x2C)
        self.movable_entry_count = u16(self.data, ne + 0x30)
        self.alignment_shift = u16(self.data, ne + 0x32)
        self.resource_count = u16(self.data, ne + 0x34)
        self.target_os = self.data[ne + 0x36]

    def _resource_identifier(self, raw: int) -> int | str:
        if raw & 0x8000:
            return raw & 0x7FFF
        if raw == 0:
            return 0
        name, _ = counted_string(self.data, self.resource_table_offset + raw)
        return name

    def resources(self, output: Path | None = None) -> list[Resource]:
        if self.resource_table_offset >= self.resident_name_offset:
            return []
        alignment = u16(self.data, self.resource_table_offset)
        cursor = self.resource_table_offset + 2
        resources: list[Resource] = []
        if output:
            output.mkdir(parents=True, exist_ok=True)
        while True:
            raw_type = u16(self.data, cursor)
            cursor += 2
            if raw_type == 0:
                break
            count = u16(self.data, cursor)
            cursor += 2
            cursor += 4  # reserved
            type_id = self._resource_identifier(raw_type)
            for _ in range(count):
                raw_offset, raw_size, flags, raw_id, _handle, _usage = struct.unpack_from(
                    "<HHHHHH", self.data, cursor
                )
                cursor += 12
                offset = raw_offset << alignment
                size = raw_size << alignment
                blob = self.data[offset : offset + size]
                resource_id = self._resource_identifier(raw_id)
                type_name = (
                    RESOURCE_TYPES.get(type_id, f"TYPE_{type_id}")
                    if isinstance(type_id, int)
                    else str(type_id)
                )
                suffix = {
                    1: ".cursor",
                    2: ".dib",
                    3: ".icon",
                    4: ".menu",
                    5: ".dialog",
                    6: ".strings",
                    7: ".fontdir",
                    8: ".fnt",
                    9: ".accelerator",
                    10: ".bin",
                    12: ".group_cursor",
                    14: ".group_icon",
                    15: ".nametable",
                    16: ".version",
                }.get(type_id, ".bin")
                filename = f"{type_name}_{resource_id}{suffix}".replace("/", "_").replace("\\", "_")
                if output:
                    (output / filename).write_bytes(blob)
                resources.append(
                    Resource(
                        type_id=type_id,
                        resource_id=resource_id,
                        offset=offset,
                        size=size,
                        flags=flags,
                        sha256=hashlib.sha256(blob).hexdigest(),
                        file=filename if output else None,
                    )
                )
        return resources

    def segments(self) -> list[Segment]:
        segments: list[Segment] = []
        for index in range(self.segment_count):
            cursor = self.segment_table_offset + index * 8
            sector, raw_size, flags, minimum = struct.unpack_from("<HHHH", self.data, cursor)
            offset = sector << self.alignment_shift
            size = raw_size or 0x10000
            relocation_count = 0
            if flags & 0x0100 and offset + size + 2 <= len(self.data):
                relocation_count = u16(self.data, offset + size)
            segments.append(Segment(index + 1, offset, size, flags, minimum, relocation_count))
        return segments

    def module_names(self) -> list[str]:
        names: list[str] = []
        for index in range(self.module_count):
            name_offset = u16(self.data, self.module_reference_offset + index * 2)
            name, _ = counted_string(self.data, self.imported_name_offset + name_offset)
            names.append(name)
        return names

    def imported_names(self) -> list[dict[str, int | str]]:
        names: list[dict[str, int | str]] = []
        cursor = self.imported_name_offset
        # The imported-name table is the last variable-sized table before the
        # entry table in this image.  Parsing into the entry table makes its
        # machine code look like bogus counted strings.
        limit = self.entry_table_offset
        while cursor < limit and cursor < len(self.data):
            relative_offset = cursor - self.imported_name_offset
            name, next_cursor = counted_string(self.data, cursor)
            if not name and next_cursor == cursor + 1:
                cursor = next_cursor
                continue
            names.append({"offset": relative_offset, "name": name})
            cursor = next_cursor
        return names

    def external_imports(self, segments: list[Segment]) -> list[dict[str, object]]:
        modules = self.module_names()
        imports: dict[tuple[str, str, int | str], dict[str, object]] = {}
        for segment in segments:
            if not segment.relocation_count:
                continue
            cursor = segment.offset + segment.size + 2
            for _ in range(segment.relocation_count):
                if cursor + 8 > len(self.data):
                    break
                source_type, flags, source_offset, target1, target2 = struct.unpack_from(
                    "<BBHHH", self.data, cursor
                )
                cursor += 8
                reference_type = flags & 0x03
                if reference_type not in (1, 2):
                    continue
                module = modules[target1 - 1] if 0 < target1 <= len(modules) else f"MODULE_{target1}"
                if reference_type == 1:
                    import_kind = "ordinal"
                    symbol: int | str = target2
                else:
                    import_kind = "name"
                    symbol, _ = counted_string(self.data, self.imported_name_offset + target2)
                key = (module, import_kind, symbol)
                item = imports.setdefault(
                    key,
                    {
                        "module": module,
                        "kind": import_kind,
                        "symbol": symbol,
                        "references": [],
                    },
                )
                item["references"].append(
                    {
                        "segment": segment.number,
                        "offset": source_offset,
                        "source_type": source_type,
                        "flags": flags,
                    }
                )
        return sorted(imports.values(), key=lambda item: (str(item["module"]), str(item["symbol"])))

    def entries(self) -> list[EntryPoint]:
        names = parse_name_table(self.data, self.resident_name_offset)
        names.update(
            parse_name_table(
                self.data, self.nonresident_name_offset, self.nonresident_size
            )
        )
        cursor = self.entry_table_offset
        limit = cursor + self.entry_table_size
        ordinal = 1
        entries: list[EntryPoint] = []
        while cursor + 2 <= limit:
            count = self.data[cursor]
            segment_indicator = self.data[cursor + 1]
            cursor += 2
            if count == 0:
                break
            if segment_indicator == 0:
                ordinal += count
                continue
            for _ in range(count):
                flags = self.data[cursor]
                if segment_indicator == 0xFF:
                    segment = self.data[cursor + 3]
                    offset = u16(self.data, cursor + 4)
                    cursor += 6
                else:
                    segment = segment_indicator
                    offset = u16(self.data, cursor + 1)
                    cursor += 3
                entries.append(EntryPoint(ordinal, segment, offset, flags, names.get(ordinal)))
                ordinal += 1
        return entries

    def relocation_summary(self, segments: list[Segment]) -> dict[str, int]:
        summary: dict[str, int] = {}
        for segment in segments:
            if not segment.relocation_count:
                continue
            cursor = segment.offset + segment.size + 2
            for _ in range(segment.relocation_count):
                if cursor + 8 > len(self.data):
                    break
                _source_type, flags, _source_offset, _target1, _target2 = struct.unpack_from(
                    "<BBHHH", self.data, cursor
                )
                cursor += 8
                reference_type = flags & 0x03
                label = {0: "internal", 1: "import_ordinal", 2: "import_name", 3: "os_fixup"}[reference_type]
                summary[label] = summary.get(label, 0) + 1
        return summary

    def relocation_records(self, segments: list[Segment]) -> list[dict[str, object]]:
        modules = self.module_names()
        records: list[dict[str, object]] = []
        labels = {0: "internal", 1: "import_ordinal", 2: "import_name", 3: "os_fixup"}
        for segment in segments:
            if not segment.relocation_count:
                continue
            cursor = segment.offset + segment.size + 2
            for index in range(segment.relocation_count):
                if cursor + 8 > len(self.data):
                    break
                source_type, flags, source_offset, target1, target2 = struct.unpack_from(
                    "<BBHHH", self.data, cursor
                )
                file_offset = cursor
                cursor += 8
                reference_type = flags & 0x03
                item: dict[str, object] = {
                    "source_segment": segment.number,
                    "source_offset": source_offset,
                    "source_type": source_type,
                    "flags": flags,
                    "kind": labels[reference_type],
                    "file_offset": file_offset,
                    "target1": target1,
                    "target2": target2,
                }
                if reference_type == 0:
                    if target1 == 0x00FF:
                        item["target_entry_ordinal"] = target2
                    else:
                        item["target_segment"] = target1
                        item["target_offset"] = target2
                elif reference_type in (1, 2):
                    item["module"] = (
                        modules[target1 - 1] if 0 < target1 <= len(modules) else f"MODULE_{target1}"
                    )
                    if reference_type == 1:
                        item["import_ordinal"] = target2
                    else:
                        name, _ = counted_string(self.data, self.imported_name_offset + target2)
                        item["import_name"] = name
                else:
                    item["fixup_type"] = target1
                    item["fixup_data"] = target2
                records.append(item)
        return records

    def metadata(self) -> dict[str, int | str]:
        return {
            "path": str(self.path),
            "sha256": hashlib.sha256(self.data).hexdigest(),
            "size": len(self.data),
            "ne_header_offset": self.ne_offset,
            "flags": self.flags,
            "target_os": self.target_os,
            "segment_count": self.segment_count,
            "module_count": self.module_count,
            "resource_count_header": self.resource_count,
            "alignment_shift": self.alignment_shift,
            "auto_data_segment": self.auto_data_segment,
            "initial_heap": self.initial_heap,
            "initial_stack": self.initial_stack,
            "initial_cs": self.initial_cs,
            "initial_ip": self.initial_ip,
            "initial_ss": self.initial_ss,
            "initial_sp": self.initial_sp,
            "movable_entry_count": self.movable_entry_count,
        }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("--extract-resources", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    executable = NewExecutable(args.executable)
    segments = executable.segments()
    resources = executable.resources(args.extract_resources)
    report = {
        "metadata": executable.metadata(),
        "modules": executable.module_names(),
        "imported_names": executable.imported_names(),
        "external_imports": executable.external_imports(segments),
        "entries": [asdict(item) for item in executable.entries()],
        "segments": [asdict(item) for item in segments],
        "relocations": executable.relocation_summary(segments),
        "relocation_records": executable.relocation_records(segments),
        "resources": [asdict(item) for item in resources],
    }
    text = json.dumps(report, indent=2) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(text, encoding="utf-8")
    else:
        print(text, end="")
    print(
        f"{len(segments)} segments, {len(resources)} resources, "
        f"{len(report['entries'])} entry points, {len(report['imported_names'])} imported names"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
