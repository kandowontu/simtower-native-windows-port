#!/usr/bin/env python3
"""Extract and disassemble each 16-bit NE code segment with relocation annotations."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from collections import defaultdict
from pathlib import Path
from typing import Any


INSTRUCTION_RE = re.compile(r"^\s*([0-9a-fA-F]+):")
NEAR_CALL_RE = re.compile(r"^\s*([0-9a-fA-F]+):.*\bcall\s+([0-9a-fA-Fx]+)\s*$")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("report", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--objdump", type=Path, default=Path(r"C:\Strawberry\c\bin\objdump.exe"))
    args = parser.parse_args()
    report: dict[str, Any] = json.loads(args.report.read_text(encoding="utf-8"))
    executable = args.executable.read_bytes()
    args.output.mkdir(parents=True, exist_ok=True)
    segment_dir = args.output / "segments"
    segment_dir.mkdir(parents=True, exist_ok=True)

    entries_by_segment: dict[int, dict[int, list[str]]] = defaultdict(lambda: defaultdict(list))
    entry_by_ordinal: dict[int, dict[str, Any]] = {}
    candidates: dict[tuple[int, int], dict[str, Any]] = {}

    def add_candidate(segment: int, offset: int, evidence: str, name: str | None = None) -> None:
        key = (segment, offset)
        item = candidates.setdefault(key, {"segment": segment, "offset": offset, "evidence": []})
        if evidence not in item["evidence"]:
            item["evidence"].append(evidence)
        if name:
            item["name"] = name

    for entry in report["entries"]:
        entry_by_ordinal[int(entry["ordinal"])] = entry
        name = entry.get("name") or f"ENTRY_ORDINAL_{entry['ordinal']}"
        entries_by_segment[int(entry["segment"])][int(entry["offset"])].append(name)
        add_candidate(int(entry["segment"]), int(entry["offset"]), "NE entry table", entry.get("name"))
    metadata = report["metadata"]
    add_candidate(int(metadata["initial_cs"]), int(metadata["initial_ip"]), "NE initial CS:IP", "PROGRAM_ENTRY")

    segment_blobs: dict[int, bytes] = {}
    code_segments: set[int] = set()
    for segment in report["segments"]:
        number = int(segment["number"])
        offset = int(segment["offset"])
        size = int(segment["size"])
        blob = executable[offset:offset + size]
        segment_blobs[number] = blob
        if not (int(segment["flags"]) & 1):
            code_segments.add(number)

    for relocation in report.get("relocation_records", []):
        if relocation["kind"] != "internal":
            continue
        source_segment = int(relocation["source_segment"])
        source_offset = int(relocation["source_offset"])
        target_segment = relocation.get("target_segment")
        target_offset = relocation.get("target_offset")
        if target_segment is None:
            entry = entry_by_ordinal.get(int(relocation.get("target_entry_ordinal", -1)))
            if entry:
                target_segment, target_offset = int(entry["segment"]), int(entry["offset"])
        if target_segment is None or target_offset is None or int(target_segment) not in code_segments:
            continue
        source_blob = segment_blobs.get(source_segment, b"")
        opcode = source_blob[source_offset - 1] if 0 < source_offset <= len(source_blob) else None
        if opcode == 0x9A:
            add_candidate(int(target_segment), int(target_offset), "relocated far CALL target")
        elif opcode == 0xEA:
            add_candidate(int(target_segment), int(target_offset), "relocated far JMP target")

    disassembled = 0
    for segment in report["segments"]:
        number = int(segment["number"])
        if number not in code_segments:
            continue
        blob = segment_blobs[number]
        binary_path = segment_dir / f"segment-{number:02d}.bin"
        asm_path = segment_dir / f"segment-{number:02d}.asm"
        binary_path.write_bytes(blob)
        command = [
            str(args.objdump), "-D", "-b", "binary", "-m", "i8086", "-M", "intel", str(binary_path),
        ]
        completed = subprocess.run(command, check=True, capture_output=True, text=True, errors="replace")
        lines: list[str] = []
        labels = entries_by_segment.get(number, {})
        for line in completed.stdout.splitlines():
            match = INSTRUCTION_RE.match(line)
            if match:
                instruction_offset = int(match.group(1), 16)
                for label in labels.get(instruction_offset, []):
                    lines.append(f"\n; ===== {label} (segment {number:02X}:{instruction_offset:04X}) =====")
                near = NEAR_CALL_RE.match(line)
                if near:
                    try:
                        target = int(near.group(2), 0)
                    except ValueError:
                        target = -1
                    if 0 <= target < len(blob):
                        add_candidate(number, target, "near CALL target")
            lines.append(line)
        asm_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        disassembled += len(blob)

    candidate_list = sorted(candidates.values(), key=lambda item: (item["segment"], item["offset"]))
    (args.output / "function-candidates.json").write_text(
        json.dumps(candidate_list, indent=2) + "\n", encoding="utf-8"
    )
    summary = [
        "# 16-bit disassembly inventory",
        "",
        f"- Code segments disassembled: {len(code_segments)}",
        f"- Code bytes disassembled: {disassembled:,}",
        f"- Function-entry candidates: {len(candidate_list):,}",
        f"- Named/exported entry points: {len(report['entries'])}",
        f"- Relocation records available for annotation: {len(report.get('relocation_records', [])):,}",
        "",
        "Function candidates combine the NE entry table, initial CS:IP, relocated far CALL/JMP targets, and direct near CALL targets. Because the binary has no full symbol table, candidates are evidence for audit and naming rather than a claim that every byte range is a distinct source-level function.",
        "",
        "Per-segment `.asm` and `.bin` files are kept under `original/disassembly/segments` with the supplied copyrighted binary.",
        "",
    ]
    (args.output / "README.md").write_text("\n".join(summary), encoding="utf-8")
    print(f"Disassembled {disassembled} code bytes across {len(code_segments)} segments; {len(candidate_list)} function candidates")


if __name__ == "__main__":
    main()
