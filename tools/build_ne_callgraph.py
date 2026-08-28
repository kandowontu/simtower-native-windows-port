#!/usr/bin/env python3
"""Build a recursive 16-bit control-flow and call graph for a Windows NE image.

The older disassembly inventory scanned every byte linearly and therefore treated
call-like byte patterns in inline data as functions.  This analyzer begins only
at authoritative NE entry points and relocated far-call/jump targets, follows
reachable control flow with Capstone, and discovers near callees recursively.
"""

from __future__ import annotations

import argparse
import json
from collections import defaultdict, deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable

from capstone import (
    CS_ARCH_X86,
    CS_GRP_IRET,
    CS_GRP_JUMP,
    CS_GRP_RET,
    CS_MODE_16,
    CS_OP_IMM,
    Cs,
)
from capstone.x86_const import (
    X86_OP_IMM,
    X86_OP_MEM,
    X86_OP_REG,
    X86_REG_BX,
    X86_REG_CX,
)


Address = tuple[int, int]


def logical_selector(segment: int) -> int:
    """Return the selector notation used by the original research corpus."""

    return 0x1000 + ((segment - 1) * 8)


def format_address(address: Address) -> str:
    segment, offset = address
    return f"{logical_selector(segment):04x}:{offset:04x}"


@dataclass
class Seed:
    segment: int
    offset: int
    evidence: set[str] = field(default_factory=set)
    name: str | None = None


@dataclass
class FunctionAnalysis:
    address: Address
    instructions: dict[int, Any] = field(default_factory=dict)
    blocks: set[int] = field(default_factory=set)
    calls: list[dict[str, Any]] = field(default_factory=list)
    branch_targets: set[int] = field(default_factory=set)
    decode_failures: set[int] = field(default_factory=set)


class Analyzer:
    def __init__(
        self,
        executable: bytes,
        report: dict[str, Any],
        imports: dict[str, Any],
    ) -> None:
        self.report = report
        self.imports = imports
        self.decoder = Cs(CS_ARCH_X86, CS_MODE_16)
        self.decoder.detail = True

        self.segment_blobs: dict[int, bytes] = {}
        self.code_segments: set[int] = set()
        for segment in report["segments"]:
            number = int(segment["number"])
            offset = int(segment["offset"])
            size = int(segment["size"])
            self.segment_blobs[number] = executable[offset : offset + size]
            if not (int(segment["flags"]) & 1):
                self.code_segments.add(number)

        self.relocations_by_segment: dict[int, list[dict[str, Any]]] = defaultdict(list)
        self.relocations_by_source: dict[Address, list[dict[str, Any]]] = defaultdict(list)
        for relocation in report.get("relocation_records", []):
            segment = int(relocation["source_segment"])
            source = int(relocation["source_offset"])
            self.relocations_by_segment[segment].append(relocation)
            self.relocations_by_source[(segment, source)].append(relocation)

        self.import_names: dict[tuple[str, int], str] = {}
        for item in imports.get("imports", []):
            key = (str(item["module"]).upper(), int(item["ordinal"]))
            self.import_names[key] = str(item.get("name") or f"ORDINAL_{item['ordinal']}")

        self.seeds: dict[Address, Seed] = {}
        self.code_pointer_references: dict[Address, list[dict[str, int]]] = defaultdict(list)
        self._seed_authoritative_entries()

    def add_seed(
        self,
        segment: int,
        offset: int,
        evidence: str,
        name: str | None = None,
    ) -> bool:
        if segment not in self.code_segments:
            return False
        blob = self.segment_blobs[segment]
        if not 0 <= offset < len(blob):
            return False
        key = (segment, offset)
        created = key not in self.seeds
        seed = self.seeds.setdefault(key, Seed(segment, offset))
        seed.evidence.add(evidence)
        if name and not seed.name:
            seed.name = name
        return created

    def _seed_authoritative_entries(self) -> None:
        for entry in self.report.get("entries", []):
            self.add_seed(
                int(entry["segment"]),
                int(entry["offset"]),
                "NE entry table",
                entry.get("name"),
            )

        metadata = self.report["metadata"]
        self.add_seed(
            int(metadata["initial_cs"]),
            int(metadata["initial_ip"]),
            "NE initial CS:IP",
            "PROGRAM_ENTRY",
        )

        # Immediate far CALL/JMP operands are authoritative control flow. Other
        # relocations into a code segment may be callback pointers *or* pointers
        # to embedded strings/tables (for example the custom resource type
        # names). Keep them as pointer candidates; compiler-prolog scanning
        # below independently promotes genuine callback routines to functions.
        for relocation in self.report.get("relocation_records", []):
            if relocation.get("kind") != "internal":
                continue
            source_segment = int(relocation["source_segment"])
            source_offset = int(relocation["source_offset"])
            source_blob = self.segment_blobs.get(source_segment, b"")
            opcode = source_blob[source_offset - 1] if 0 < source_offset <= len(source_blob) else None
            target_segment = relocation.get("target_segment")
            target_offset = relocation.get("target_offset")
            if target_segment is None or target_offset is None:
                continue
            if int(target_segment) not in self.code_segments:
                continue
            if opcode == 0x9A:
                evidence = "relocated far CALL target"
            elif opcode == 0xEA:
                evidence = "relocated far JMP target"
            else:
                target = (int(target_segment), int(target_offset))
                self.code_pointer_references[target].append(
                    {
                        "source_segment": source_segment,
                        "source_offset": source_offset,
                    }
                )
                continue
            self.add_seed(int(target_segment), int(target_offset), evidence)

        # Microsoft/Borland-style 16-bit functions in this image use three
        # stable frame prologs. Some local routines are referenced only through
        # near calls reached from switch tables, so no NE relocation points at
        # them. Scan canonical (longest-first) prologs without double-counting
        # the nested 45 55 8B EC / 55 8B EC bytes inside a far prolog.
        prologs = (
            (bytes.fromhex("8c d0 90 45 55 8b ec"), "compiler far-function prolog"),
            (bytes.fromhex("45 55 8b ec"), "compiler far-frame prolog"),
            (bytes.fromhex("55 8b ec"), "compiler near-function prolog"),
        )
        for segment in sorted(self.code_segments):
            blob = self.segment_blobs[segment]
            covered_nested_offsets: set[int] = set()
            for pattern, evidence in prologs:
                cursor = 0
                while True:
                    offset = blob.find(pattern, cursor)
                    if offset < 0:
                        break
                    cursor = offset + 1
                    if offset in covered_nested_offsets:
                        continue
                    self.add_seed(segment, offset, evidence)
                    # Only suppress offsets that begin a shorter prolog nested
                    # in this one; ordinary instruction bytes remain eligible.
                    if pattern.startswith(bytes.fromhex("8c d0 90 45")):
                        covered_nested_offsets.update({offset + 3, offset + 4})
                    elif pattern.startswith(bytes.fromhex("45 55")):
                        covered_nested_offsets.add(offset + 1)

    def relocations_for_instruction(self, segment: int, offset: int, size: int) -> list[dict[str, Any]]:
        result: list[dict[str, Any]] = []
        for source in range(offset, offset + size):
            result.extend(self.relocations_by_source.get((segment, source), ()))
        return result

    def resolve_relocation(self, relocation: dict[str, Any]) -> dict[str, Any]:
        kind = str(relocation.get("kind"))
        if kind == "internal":
            target_segment = relocation.get("target_segment")
            target_offset = relocation.get("target_offset")
            if target_segment is not None and target_offset is not None:
                target = (int(target_segment), int(target_offset))
                return {
                    "kind": "internal_far",
                    "target": format_address(target),
                    "target_segment": target[0],
                    "target_offset": target[1],
                }
        if kind in {"import_ordinal", "import_name"}:
            module = str(relocation.get("module") or "UNKNOWN").upper()
            ordinal = int(relocation.get("import_ordinal", -1))
            name = self.import_names.get((module, ordinal), f"ORDINAL_{ordinal}")
            return {
                "kind": "import",
                "module": module,
                "ordinal": ordinal,
                "name": name,
                "target": f"{module}!{name}",
            }
        return {"kind": kind, "target": None}

    @staticmethod
    def direct_target(instruction: Any) -> int | None:
        operands = instruction.operands
        if len(operands) == 1 and operands[0].type == CS_OP_IMM:
            return int(operands[0].imm) & 0xFFFF
        return None

    @staticmethod
    def is_conditional_jump(instruction: Any) -> bool:
        mnemonic = instruction.mnemonic.lower()
        return mnemonic != "jmp" and (
            mnemonic.startswith("j") or mnemonic.startswith("loop")
        )

    def recover_jump_table(
        self,
        segment: int,
        instruction: Any,
        prior_instructions: Iterable[Any],
    ) -> dict[str, Any] | None:
        """Recover the compiler's ``cmp bx,N / ja / add bx,bx / jmp cs:[bx+T]`` tables."""

        operands = instruction.operands
        if len(operands) != 1 or operands[0].type != X86_OP_MEM:
            return None
        memory = operands[0].mem
        if memory.base != X86_REG_BX or memory.index != 0:
            return None
        table_offset = int(memory.disp) & 0xFFFF

        prior = list(prior_instructions)
        entry_count: int | None = None
        dispatch_kind = "bounded_index"
        for previous in reversed(prior[-16:]):
            if previous.mnemonic.lower() != "cmp" or len(previous.operands) != 2:
                continue
            left, right = previous.operands
            if (
                left.type == X86_OP_REG
                and left.reg == X86_REG_BX
                and right.type == X86_OP_IMM
            ):
                maximum = int(right.imm) & 0xFFFF
                # The image consistently uses JA for an inclusive maximum.
                # JAE appears in a few hand/runtime dispatchers and denotes an
                # exclusive upper bound.
                following_mnemonics = [
                    item.mnemonic.lower()
                    for item in prior
                    if item.address > previous.address
                ]
                entry_count = maximum if "jae" in following_mnemonics else maximum + 1
                break

        if entry_count is None:
            # Win16 window/dialog procedures use a compact parallel-table
            # dispatch: CX=count, BX=message-id table, then after a lookup loop
            # JMP CS:[BX+delta]. BX still indexes the matching entry, so the
            # target table begins at message_table + delta.
            message_table: int | None = None
            lookup_count: int | None = None
            for previous in reversed(prior[-24:]):
                if previous.mnemonic.lower() != "mov" or len(previous.operands) != 2:
                    continue
                left, right = previous.operands
                if left.type != X86_OP_REG or right.type != X86_OP_IMM:
                    continue
                if left.reg == X86_REG_BX and message_table is None:
                    message_table = int(right.imm) & 0xFFFF
                elif left.reg == X86_REG_CX and lookup_count is None:
                    lookup_count = int(right.imm) & 0xFFFF
            if message_table is not None and lookup_count is not None:
                table_offset = (message_table + table_offset) & 0xFFFF
                entry_count = lookup_count
                dispatch_kind = "parallel_lookup"

        if entry_count is None or not 1 <= entry_count <= 512:
            return None

        blob = self.segment_blobs[segment]
        table_end = table_offset + entry_count * 2
        if not 0 <= table_offset < table_end <= len(blob):
            return None
        targets = [
            int.from_bytes(blob[offset : offset + 2], "little")
            for offset in range(table_offset, table_end, 2)
        ]
        if any(target >= len(blob) for target in targets):
            return None
        return {
            "table_offset": table_offset,
            "entry_count": entry_count,
            "dispatch_kind": dispatch_kind,
            "targets": targets,
        }

    def analyze_function(self, address: Address, entries: set[Address]) -> FunctionAnalysis:
        segment, entry_offset = address
        blob = self.segment_blobs[segment]
        result = FunctionAnalysis(address)
        pending: deque[int] = deque([entry_offset])
        visited_blocks: set[int] = set()

        while pending:
            block_start = pending.popleft()
            if not 0 <= block_start < len(blob) or block_start in visited_blocks:
                continue
            if (segment, block_start) in entries and block_start != entry_offset:
                result.calls.append(
                    {
                        "site_offset": None,
                        "site": None,
                        "kind": "fallthrough_boundary",
                        "target": format_address((segment, block_start)),
                        "target_segment": segment,
                        "target_offset": block_start,
                    }
                )
                continue
            visited_blocks.add(block_start)
            result.blocks.add(block_start)
            cursor = block_start

            while 0 <= cursor < len(blob):
                if cursor != block_start and (segment, cursor) in entries and cursor != entry_offset:
                    result.calls.append(
                        {
                            "site_offset": None,
                            "site": None,
                            "kind": "fallthrough_boundary",
                            "target": format_address((segment, cursor)),
                            "target_segment": segment,
                            "target_offset": cursor,
                        }
                    )
                    break
                if cursor in result.instructions:
                    break

                decoded = next(self.decoder.disasm(blob[cursor:], cursor, count=1), None)
                if decoded is None or decoded.size <= 0:
                    result.decode_failures.add(cursor)
                    break
                result.instructions[cursor] = decoded
                next_offset = cursor + decoded.size
                relocations = self.relocations_for_instruction(segment, cursor, decoded.size)

                if decoded.mnemonic.lower() in {"call", "lcall"}:
                    handled = False
                    for relocation in relocations:
                        resolved = self.resolve_relocation(relocation)
                        if resolved.get("kind") not in {"internal_far", "import"}:
                            continue
                        result.calls.append(
                            {
                                "site_offset": cursor,
                                "site": format_address((segment, cursor)),
                                **resolved,
                            }
                        )
                        handled = True
                    if not handled:
                        target_offset = self.direct_target(decoded)
                        if target_offset is not None and 0 <= target_offset < len(blob):
                            result.calls.append(
                                {
                                    "site_offset": cursor,
                                    "site": format_address((segment, cursor)),
                                    "kind": "near",
                                    "target": format_address((segment, target_offset)),
                                    "target_segment": segment,
                                    "target_offset": target_offset,
                                }
                            )
                        else:
                            result.calls.append(
                                {
                                    "site_offset": cursor,
                                    "site": format_address((segment, cursor)),
                                    "kind": "indirect",
                                    "target": None,
                                    "instruction": f"{decoded.mnemonic} {decoded.op_str}".strip(),
                                }
                            )

                if decoded.group(CS_GRP_RET) or decoded.group(CS_GRP_IRET):
                    break

                if decoded.group(CS_GRP_JUMP):
                    target_offset = self.direct_target(decoded)
                    if target_offset is not None and 0 <= target_offset < len(blob):
                        result.branch_targets.add(target_offset)
                        if (segment, target_offset) in entries and target_offset != entry_offset:
                            result.calls.append(
                                {
                                    "site_offset": cursor,
                                    "site": format_address((segment, cursor)),
                                    "kind": "tail_jump",
                                    "target": format_address((segment, target_offset)),
                                    "target_segment": segment,
                                    "target_offset": target_offset,
                                }
                            )
                        else:
                            pending.append(target_offset)
                    else:
                        resolved_jump = False
                        for relocation in relocations:
                            resolved = self.resolve_relocation(relocation)
                            if resolved.get("kind") in {"internal_far", "import"}:
                                result.calls.append(
                                    {
                                        "site_offset": cursor,
                                        "site": format_address((segment, cursor)),
                                        "kind": "tail_" + str(resolved["kind"]),
                                        **{key: value for key, value in resolved.items() if key != "kind"},
                                    }
                                )
                                resolved_jump = True
                        if not resolved_jump:
                            previous = [
                                item
                                for offset, item in sorted(result.instructions.items())
                                if cursor - 64 <= offset < cursor
                            ]
                            jump_table = self.recover_jump_table(segment, decoded, previous)
                            if jump_table:
                                for target in jump_table["targets"]:
                                    result.branch_targets.add(target)
                                    pending.append(target)
                                result.calls.append(
                                    {
                                        "site_offset": cursor,
                                        "site": format_address((segment, cursor)),
                                        "kind": "jump_table",
                                        "target": None,
                                        "instruction": f"{decoded.mnemonic} {decoded.op_str}".strip(),
                                        "table_offset": jump_table["table_offset"],
                                        "entry_count": jump_table["entry_count"],
                                        "dispatch_kind": jump_table["dispatch_kind"],
                                        "targets": [
                                            format_address((segment, target))
                                            for target in jump_table["targets"]
                                        ],
                                    }
                                )
                            else:
                                result.calls.append(
                                    {
                                        "site_offset": cursor,
                                        "site": format_address((segment, cursor)),
                                        "kind": "indirect_jump",
                                        "target": None,
                                        "instruction": f"{decoded.mnemonic} {decoded.op_str}".strip(),
                                    }
                                )
                    if self.is_conditional_jump(decoded):
                        pending.append(next_offset)
                    break

                # INT 20h and INT 21h/AH=4Ch are process exits in startup code;
                # RET-based function boundaries cover ordinary game routines.
                cursor = next_offset

        return result

    def discover_functions(self) -> dict[Address, FunctionAnalysis]:
        queue: deque[Address] = deque(sorted(self.seeds))
        analyzed: set[Address] = set()
        provisional: dict[Address, FunctionAnalysis] = {}

        while queue:
            address = queue.popleft()
            if address in analyzed:
                continue
            analyzed.add(address)
            analysis = self.analyze_function(address, set(self.seeds))
            provisional[address] = analysis
            for call in analysis.calls:
                if call.get("kind") not in {"near", "internal_far", "tail_jump", "tail_internal_far"}:
                    continue
                target_segment = call.get("target_segment")
                target_offset = call.get("target_offset")
                if target_segment is None or target_offset is None:
                    continue
                created = self.add_seed(
                    int(target_segment),
                    int(target_offset),
                    "reachable call target",
                )
                if created:
                    queue.append((int(target_segment), int(target_offset)))

        # Re-run with the complete entry set so shared/tail functions are not
        # absorbed into a caller discovered earlier in the traversal.
        all_entries = set(self.seeds)
        return {
            address: self.analyze_function(address, all_entries)
            for address in sorted(all_entries)
        }


def deduplicate_calls(calls: Iterable[dict[str, Any]]) -> list[dict[str, Any]]:
    seen: set[str] = set()
    output: list[dict[str, Any]] = []
    for call in calls:
        key = json.dumps(call, sort_keys=True)
        if key in seen:
            continue
        seen.add(key)
        output.append(call)
    return output


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("ne_report", type=Path)
    parser.add_argument("imports_report", type=Path)
    parser.add_argument("output_json", type=Path)
    parser.add_argument("output_markdown", type=Path)
    args = parser.parse_args()

    executable = args.executable.read_bytes()
    ne_report: dict[str, Any] = json.loads(args.ne_report.read_text(encoding="utf-8"))
    imports_report: dict[str, Any] = json.loads(args.imports_report.read_text(encoding="utf-8"))
    analyzer = Analyzer(executable, ne_report, imports_report)
    analyses = analyzer.discover_functions()

    functions: list[dict[str, Any]] = []
    all_instruction_bytes: set[tuple[int, int]] = set()
    import_call_sites = 0
    internal_call_sites = 0
    indirect_call_sites = 0
    indirect_jump_sites = 0
    jump_table_sites = 0
    named_functions = 0

    for address, analysis in analyses.items():
        seed = analyzer.seeds[address]
        calls = deduplicate_calls(analysis.calls)
        for call in calls:
            kind = str(call.get("kind"))
            if kind in {"import", "tail_import"}:
                import_call_sites += 1
            elif kind == "indirect":
                indirect_call_sites += 1
            elif kind == "indirect_jump":
                indirect_jump_sites += 1
            elif kind == "jump_table":
                jump_table_sites += 1
            elif kind not in {"fallthrough_boundary"}:
                internal_call_sites += 1
        for offset, instruction in analysis.instructions.items():
            for byte_offset in range(offset, offset + instruction.size):
                all_instruction_bytes.add((address[0], byte_offset))
        if seed.name:
            named_functions += 1
        functions.append(
            {
                "address": format_address(address),
                "segment": address[0],
                "offset": address[1],
                "name": seed.name,
                "evidence": sorted(seed.evidence),
                "block_count": len(analysis.blocks),
                "block_offsets": sorted(analysis.blocks),
                "instruction_count": len(analysis.instructions),
                "instruction_offsets": sorted(analysis.instructions),
                "branch_targets": sorted(analysis.branch_targets),
                "decoded_byte_count": sum(item.size for item in analysis.instructions.values()),
                "min_instruction_offset": min(analysis.instructions, default=None),
                "max_instruction_end": max(
                    (offset + item.size for offset, item in analysis.instructions.items()),
                    default=None,
                ),
                "decode_failures": sorted(analysis.decode_failures),
                "calls": calls,
            }
        )

    total_code_bytes = sum(len(analyzer.segment_blobs[number]) for number in analyzer.code_segments)
    segment_function_counts: dict[int, int] = defaultdict(int)
    for function in functions:
        segment_function_counts[int(function["segment"])] += 1
    decoded_by_segment: dict[int, set[int]] = defaultdict(set)
    for segment, offset in all_instruction_bytes:
        decoded_by_segment[segment].add(offset)
    segment_coverage = []
    for segment in sorted(analyzer.code_segments):
        total = len(analyzer.segment_blobs[segment])
        decoded = len(decoded_by_segment[segment])
        segment_coverage.append(
            {
                "segment": segment,
                "selector": f"{logical_selector(segment):04x}",
                "code_bytes": total,
                "decoded_unique_bytes": decoded,
                "decoded_percent": round(100 * decoded / total, 2) if total else 100.0,
                "function_count": segment_function_counts[segment],
            }
        )
    api_frequency: dict[str, int] = defaultdict(int)
    for function in functions:
        for call in function["calls"]:
            if call.get("kind") in {"import", "tail_import"}:
                api_frequency[str(call["target"])] += 1

    output = {
        "metadata": {
            "source_executable": str(args.executable),
            "source_sha256": ne_report["metadata"]["sha256"],
            "code_segments": len(analyzer.code_segments),
            "total_code_bytes": total_code_bytes,
            "function_count": len(functions),
            "named_function_count": named_functions,
            "decoded_unique_code_bytes": len(all_instruction_bytes),
            "decoded_unique_code_percent": round(
                100 * len(all_instruction_bytes) / total_code_bytes, 2
            ),
            "internal_call_sites": internal_call_sites,
            "import_call_sites": import_call_sites,
            "indirect_call_sites": indirect_call_sites,
            "indirect_jump_sites": indirect_jump_sites,
            "recovered_jump_table_sites": jump_table_sites,
            "relocated_code_pointer_targets": len(analyzer.code_pointer_references),
            "relocated_code_pointers_classified_as_functions": sum(
                address in analyses for address in analyzer.code_pointer_references
            ),
        },
        "segment_coverage": segment_coverage,
        "code_pointer_candidates": [
            {
                "address": format_address(address),
                "segment": address[0],
                "offset": address[1],
                "classified_as_function": address in analyses,
                "references": references,
            }
            for address, references in sorted(analyzer.code_pointer_references.items())
        ],
        "api_frequency": [
            {"api": api, "call_sites": count}
            for api, count in sorted(api_frequency.items(), key=lambda item: (-item[1], item[0]))
        ],
        "functions": functions,
    }
    args.output_json.parent.mkdir(parents=True, exist_ok=True)
    args.output_json.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")

    metadata = output["metadata"]
    markdown = [
        "# Recursive NE call graph",
        "",
        f"Source SHA-256: `{metadata['source_sha256']}`",
        "",
        "This inventory follows executable control flow from authoritative NE entry points and relocated far-call targets. It replaces the earlier linear-scan candidate count, which included call-like bytes from embedded data.",
        "",
        f"- Code segments: {metadata['code_segments']}",
        f"- Code bytes: {metadata['total_code_bytes']:,}",
        f"- Reachable function entries: {metadata['function_count']:,}",
        f"- Named NE entries: {metadata['named_function_count']}",
        f"- Unique decoded instruction bytes: {metadata['decoded_unique_code_bytes']:,} ({metadata['decoded_unique_code_percent']:.2f}%)",
        f"- Internal call sites: {metadata['internal_call_sites']:,}",
        f"- Resolved Win16 import call sites: {metadata['import_call_sites']:,}",
        f"- Indirect call sites: {metadata['indirect_call_sites']:,}",
        f"- Unresolved indirect jump sites: {metadata['indirect_jump_sites']:,}",
        f"- Recovered compiler jump tables: {metadata['recovered_jump_table_sites']:,}",
        "",
        "## Exported/named entry points",
        "",
        "| Address | Name | Instructions | Calls |",
        "|---|---|---:|---:|",
    ]
    for function in functions:
        if not function["name"]:
            continue
        markdown.append(
            f"| `{function['address']}` | `{function['name']}` | {function['instruction_count']} | {len(function['calls'])} |"
        )
    markdown.extend(["", "## Most-used resolved imports", "", "| API | Call sites |", "|---|---:|"])
    for item in output["api_frequency"][:50]:
        markdown.append(f"| `{item['api']}` | {item['call_sites']} |")
    markdown.append("")
    args.output_markdown.write_text("\n".join(markdown), encoding="utf-8")

    print(
        f"Discovered {metadata['function_count']} reachable functions; "
        f"resolved {metadata['import_call_sites']} import call sites; "
        f"decoded {metadata['decoded_unique_code_percent']:.2f}% of code-segment bytes"
    )


if __name__ == "__main__":
    main()
