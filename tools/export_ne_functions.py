#!/usr/bin/env python3
"""Export one relocation- and call-annotated assembly file per NE function."""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Any

from capstone import CS_ARCH_X86, CS_MODE_16, Cs


def logical_selector(segment: int) -> int:
    return 0x1000 + ((segment - 1) * 8)


def address(segment: int, offset: int) -> str:
    return f"{logical_selector(segment):04x}:{offset:04x}"


def safe_name(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", value).strip("_")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("ne_report", type=Path)
    parser.add_argument("imports_report", type=Path)
    parser.add_argument("callgraph", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    executable = args.executable.read_bytes()
    ne_report: dict[str, Any] = json.loads(args.ne_report.read_text(encoding="utf-8"))
    imports_report: dict[str, Any] = json.loads(args.imports_report.read_text(encoding="utf-8"))
    callgraph: dict[str, Any] = json.loads(args.callgraph.read_text(encoding="utf-8"))

    segment_blobs: dict[int, bytes] = {}
    for segment in ne_report["segments"]:
        number = int(segment["number"])
        file_offset = int(segment["offset"])
        size = int(segment["size"])
        segment_blobs[number] = executable[file_offset : file_offset + size]

    import_names: dict[tuple[str, int], str] = {}
    for item in imports_report.get("imports", []):
        key = (str(item["module"]).upper(), int(item["ordinal"]))
        import_names[key] = str(item.get("name") or f"ORDINAL_{item['ordinal']}")

    relocations: dict[tuple[int, int], list[dict[str, Any]]] = defaultdict(list)
    for item in ne_report.get("relocation_records", []):
        relocations[(int(item["source_segment"]), int(item["source_offset"]))].append(item)

    decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    decoder.detail = True
    args.output.mkdir(parents=True, exist_ok=True)
    manifest: list[dict[str, Any]] = []

    for function in callgraph["functions"]:
        segment = int(function["segment"])
        offset = int(function["offset"])
        function_address = str(function["address"])
        name = function.get("name")
        suffix = f"_{safe_name(name)}" if name else ""
        filename = f"{logical_selector(segment):04x}_{offset:04x}{suffix}.asm"
        blob = segment_blobs[segment]
        instruction_offsets = [int(item) for item in function["instruction_offsets"]]
        block_offsets = {int(item) for item in function["block_offsets"]}
        branch_targets = {int(item) for item in function["branch_targets"]}
        calls_by_site: dict[int, list[dict[str, Any]]] = defaultdict(list)
        for call in function.get("calls", []):
            if call.get("site_offset") is not None:
                calls_by_site[int(call["site_offset"])].append(call)

        lines = [
            f"; Function {function_address}{' ' + name if name else ''}",
            f"; Evidence: {', '.join(function['evidence'])}",
            f"; Blocks: {function['block_count']}; instructions: {function['instruction_count']}; decoded bytes: {function['decoded_byte_count']}",
            ";",
        ]

        for instruction_offset in instruction_offsets:
            decoded = next(decoder.disasm(blob[instruction_offset:], instruction_offset, count=1), None)
            if decoded is None:
                lines.append(f"; {instruction_offset:04x}: <decode failure>")
                continue
            if instruction_offset == offset:
                lines.append(f"fn_{logical_selector(segment):04x}_{offset:04x}:")
            elif instruction_offset in block_offsets or instruction_offset in branch_targets:
                lines.append(f"loc_{instruction_offset:04x}:")

            raw_bytes = decoded.bytes.hex(" ")
            instruction_text = f"{decoded.mnemonic} {decoded.op_str}".rstrip()
            line = f"  {decoded.address:04x}: {raw_bytes:<23} {instruction_text}"
            annotations: list[str] = []

            for call in calls_by_site.get(instruction_offset, []):
                kind = str(call.get("kind"))
                if kind == "jump_table":
                    targets = ", ".join(str(item) for item in call.get("targets", []))
                    annotations.append(
                        f"{call.get('dispatch_kind')} jump table @{int(call['table_offset']):04x} [{targets}]"
                    )
                elif call.get("target"):
                    annotations.append(f"{kind} => {call['target']}")
                else:
                    annotations.append(kind)

            for source in range(instruction_offset, instruction_offset + decoded.size):
                for relocation in relocations.get((segment, source), []):
                    kind = str(relocation.get("kind"))
                    if kind == "internal":
                        target_segment = relocation.get("target_segment")
                        target_offset = relocation.get("target_offset")
                        if target_segment is not None and target_offset is not None:
                            annotations.append(
                                "reloc => " + address(int(target_segment), int(target_offset))
                            )
                    elif kind in {"import_ordinal", "import_name"}:
                        module = str(relocation.get("module") or "UNKNOWN").upper()
                        ordinal = int(relocation.get("import_ordinal", -1))
                        import_name = import_names.get((module, ordinal), f"ORDINAL_{ordinal}")
                        annotation = f"reloc => {module}!{import_name}"
                        if annotation not in annotations:
                            annotations.append(annotation)
                    elif kind == "os_fixup":
                        annotations.append(f"OS fixup {relocation.get('fixup_type')}")

            if annotations:
                line += "  ; " + " | ".join(annotations)
            lines.append(line)

        output_path = args.output / filename
        output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        manifest.append(
            {
                "address": function_address,
                "name": name,
                "path": str(output_path),
                "instruction_count": function["instruction_count"],
                "decoded_byte_count": function["decoded_byte_count"],
            }
        )

    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Exported {len(manifest)} annotated functions to {args.output}")


if __name__ == "__main__":
    main()
