#!/usr/bin/env python3
"""Build a conservative address-coverage index for the native Windows port.

This deliberately excludes the rejected web prototype.  An address annotation is
evidence that recovered code was consulted; it is not, by itself, a claim of
behavioral equivalence.  Exact function-start annotations and annotations that
fall inside a recovered function candidate are therefore reported separately.
"""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict, deque
from pathlib import Path
from typing import Iterable


FULL_ADDRESS_RE = re.compile(
    r"(?<![0-9A-Fa-f])(?:0x)?([0-9A-Fa-f]{4}):([0-9A-Fa-f]{4})(?![0-9A-Fa-f])"
)
SHORT_ADDRESS_RE = re.compile(r"\s*/\s*([0-9A-Fa-f]{4})(?![0-9A-Fa-f])")


def normalize_address(segment: str, offset: str) -> str:
    return f"{segment.lower()}:{offset.lower()}"


def address_parts(address: str) -> tuple[int, int]:
    segment, offset = address.split(":", 1)
    return int(segment, 16), int(offset, 16)


def addresses_on_line(line: str) -> list[str]:
    """Extract full addresses and same-segment slash shorthand.

    For example, ``1220:1059/10af/1518`` yields three addresses.  Shorthand is
    expanded only while it is immediately chained to a full address.
    """

    found: list[str] = []
    for match in FULL_ADDRESS_RE.finditer(line):
        segment = match.group(1)
        found.append(normalize_address(segment, match.group(2)))
        cursor = match.end()
        while True:
            short = SHORT_ADDRESS_RE.match(line, cursor)
            if short is None:
                break
            found.append(normalize_address(segment, short.group(1)))
            cursor = short.end()
    return found


def source_paths(root: Path, extensions: set[str]) -> list[Path]:
    if root.is_file():
        return [root]
    if not root.exists():
        return []
    return sorted(
        path for path in root.rglob("*") if path.is_file() and path.suffix.lower() in extensions
    )


def scan_group(workspace: Path, roots: Iterable[Path], extensions: set[str]) -> dict[str, list[dict[str, object]]]:
    occurrences: dict[str, list[dict[str, object]]] = defaultdict(list)
    paths: list[Path] = []
    for root in roots:
        paths.extend(source_paths(root, extensions))
    for path in sorted(set(paths)):
        text = path.read_text(encoding="utf-8", errors="replace")
        for line_number, line in enumerate(text.splitlines(), 1):
            context = " ".join(line.strip().split())[:300]
            for address in addresses_on_line(line):
                item = {
                    "file": path.relative_to(workspace).as_posix(),
                    "line": line_number,
                    "context": context,
                }
                if item not in occurrences[address]:
                    occurrences[address].append(item)
    return dict(occurrences)


def function_owner(
    address: str,
    exact: dict[str, dict[str, object]],
    functions_by_segment: dict[int, list[dict[str, object]]],
) -> tuple[str | None, bool]:
    """Return the nearest containing candidate and whether it is an exact start.

    Recovered candidates can overlap.  The nearest preceding candidate whose
    decoded instruction span contains the address is used only as an inferred
    owner, never as equivalence evidence.
    """

    if address in exact:
        return address, True
    segment, offset = address_parts(address)
    candidates = functions_by_segment.get(segment, [])
    owner: dict[str, object] | None = None
    for candidate in candidates:
        start = int(candidate["offset"])
        if start > offset:
            break
        end = int(candidate.get("max_instruction_end", start))
        if start <= offset < end:
            owner = candidate
    if owner is None:
        return None, False
    return str(owner["address"]).lower(), False


def internal_targets(function: dict[str, object], exact: set[str]) -> set[str]:
    targets: set[str] = set()
    for call in function.get("calls", []):
        target = str(call.get("target", "")).lower()
        if target in exact:
            targets.add(target)
    return targets


def reachable_from(seeds: set[str], edges: dict[str, set[str]]) -> set[str]:
    reached: set[str] = set()
    queue = deque(seed for seed in seeds if seed in edges)
    while queue:
        address = queue.popleft()
        if address in reached:
            continue
        reached.add(address)
        queue.extend(edges.get(address, ()))
    return reached


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("workspace", type=Path)
    parser.add_argument("output_json", type=Path)
    parser.add_argument("output_markdown", type=Path)
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    callgraph_path = workspace / "analysis" / "simtower-callgraph.json"
    callgraph = json.loads(callgraph_path.read_text(encoding="utf-8"))
    functions: list[dict[str, object]] = callgraph["functions"]
    exact = {str(item["address"]).lower(): item for item in functions}

    functions_by_segment: dict[int, list[dict[str, object]]] = defaultdict(list)
    for function in functions:
        functions_by_segment[int(function["segment"])].append(function)
    for candidates in functions_by_segment.values():
        candidates.sort(key=lambda item: int(item["offset"]))

    groups = {
        "native_source": scan_group(
            workspace,
            [workspace / "port" / "src"],
            {".cpp", ".hpp", ".h", ".rc", ".in"},
        ),
        "native_tests": scan_group(
            workspace,
            [workspace / "port" / "tests"],
            {".cpp", ".hpp", ".h"},
        ),
        "inventory": scan_group(
            workspace,
            [workspace / "analysis" / "IMPLEMENTATION_INVENTORY.md"],
            {".md"},
        ),
    }

    evidence_by_function: dict[str, dict[str, object]] = {
        address: {
            "native_source_exact": [],
            "native_source_containing": [],
            "native_tests_exact": [],
            "native_tests_containing": [],
            "inventory_exact": [],
            "inventory_containing": [],
        }
        for address in exact
    }
    orphan_annotations: list[dict[str, object]] = []
    for group_name, occurrences in groups.items():
        for annotated_address, refs in occurrences.items():
            owner, is_exact = function_owner(annotated_address, exact, functions_by_segment)
            if owner is None:
                orphan_annotations.append(
                    {"group": group_name, "address": annotated_address, "occurrences": refs}
                )
                continue
            suffix = "exact" if is_exact else "containing"
            evidence_by_function[owner][f"{group_name}_{suffix}"].append(
                {"address": annotated_address, "occurrences": refs}
            )

    edges = {
        address: internal_targets(function, set(exact))
        for address, function in exact.items()
    }
    inbound: dict[str, int] = {address: 0 for address in exact}
    for targets in edges.values():
        for target in targets:
            inbound[target] += 1

    # The normal entry point plus recovered callback/code-pointer roots gives a
    # useful prioritization set.  It is not a proof that every reached routine is
    # exercised in a particular play session.
    seeds = {"1000:0000"}
    for function in functions:
        evidence = " ".join(str(item) for item in function.get("evidence", []))
        if "relocated" in evidence.lower() or "entry" in evidence.lower() or function.get("name"):
            seeds.add(str(function["address"]).lower())
    reachable = reachable_from(seeds, edges)

    rows: list[dict[str, object]] = []
    for address, function in exact.items():
        evidence = evidence_by_function[address]
        source_exact = bool(evidence["native_source_exact"])
        source_containing = bool(evidence["native_source_containing"])
        test_exact = bool(evidence["native_tests_exact"])
        test_containing = bool(evidence["native_tests_containing"])
        inventory_exact = bool(evidence["inventory_exact"])
        inventory_containing = bool(evidence["inventory_containing"])
        decoded_bytes = int(function.get("decoded_byte_count", 0))
        inbound_calls = inbound[address]
        priority_score = decoded_bytes + inbound_calls * 48
        if address in reachable:
            priority_score += 256
        if function.get("name"):
            priority_score += 512
        rows.append(
            {
                "address": address,
                "name": function.get("name", ""),
                "decoded_byte_count": decoded_bytes,
                "instruction_count": int(function.get("instruction_count", 0)),
                "inbound_function_count": inbound_calls,
                "reachable_from_recovered_roots": address in reachable,
                "native_source_exact": source_exact,
                "native_source_containing": source_containing,
                "native_test_exact": test_exact,
                "native_test_containing": test_containing,
                "inventory_exact": inventory_exact,
                "inventory_containing": inventory_containing,
                "priority_score": priority_score,
                "evidence": evidence,
            }
        )

    mapped = [row for row in rows if row["native_source_exact"] or row["native_source_containing"]]
    exact_mapped = [row for row in rows if row["native_source_exact"]]
    inferred_mapped = [row for row in rows if row["native_source_containing"] and not row["native_source_exact"]]
    unmapped = [row for row in rows if not row["native_source_exact"] and not row["native_source_containing"]]
    unmapped_by_segment: dict[str, int] = defaultdict(int)
    for row in unmapped:
        unmapped_by_segment[str(row["address"]).split(":", 1)[0]] += 1
    untested_mapped = [
        row
        for row in mapped
        if not row["native_test_exact"] and not row["native_test_containing"]
    ]
    top_unmapped = sorted(
        unmapped,
        key=lambda row: (
            int(row["priority_score"]),
            int(row["decoded_byte_count"]),
            str(row["address"]),
        ),
        reverse=True,
    )[:100]

    payload = {
        "metadata": {
            "source_callgraph": callgraph_path.relative_to(workspace).as_posix(),
            "recovered_function_candidates": len(rows),
            "warning": "Address annotations are mapping evidence, not behavioral-equivalence proof.",
            "excluded_roots": ["web/", "references/"],
        },
        "summary": {
            "unique_native_source_annotations": len(groups["native_source"]),
            "unique_native_test_annotations": len(groups["native_tests"]),
            "unique_inventory_annotations": len(groups["inventory"]),
            "native_source_exact_function_starts": len(exact_mapped),
            "native_source_inferred_containing_only": len(inferred_mapped),
            "native_source_mapped_candidates_total": len(mapped),
            "native_source_unmapped_candidates": len(unmapped),
            "native_source_unmapped_by_segment": dict(
                sorted(unmapped_by_segment.items())
            ),
            "mapped_candidates_without_test_annotation": len(untested_mapped),
            "reachable_candidates": len(reachable),
            "orphan_annotations": len(orphan_annotations),
        },
        "top_unmapped_candidates": top_unmapped,
        "functions": rows,
        "orphan_annotations": orphan_annotations,
    }

    args.output_json.parent.mkdir(parents=True, exist_ok=True)
    args.output_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    summary = payload["summary"]
    markdown = [
        "# Native port recovered-routine coverage audit",
        "",
        "> Address annotations are mapping evidence, not behavioral-equivalence proof. This audit excludes `web/` and `references/`.",
        "",
        f"- Recovered function candidates: {summary['recovered_function_candidates'] if 'recovered_function_candidates' in summary else len(rows):,}",
        f"- Unique addresses cited in native source: {summary['unique_native_source_annotations']:,}",
        f"- Exact recovered function starts cited in native source: {summary['native_source_exact_function_starts']:,}",
        f"- Additional candidates inferred from an in-body native citation: {summary['native_source_inferred_containing_only']:,}",
        f"- Candidates with no native-source citation: {summary['native_source_unmapped_candidates']:,}",
        f"- Source-mapped candidates with no test citation: {summary['mapped_candidates_without_test_annotation']:,}",
        "",
        "## Candidates without native-source citations by segment",
        "",
        "| Segment | Candidates |",
        "| --- | ---: |",
    ]
    for segment, count in summary["native_source_unmapped_by_segment"].items():
        markdown.append(f"| `{segment}` | {count:,} |")
    markdown.extend(
        [
        "",
        "## Highest-priority candidates with no native-source mapping",
        "",
        "| Address | Name | Bytes | Inbound callers | Reachable-root heuristic | Inventory claim |",
        "| --- | --- | ---: | ---: | :---: | :---: |",
        ]
    )
    for row in top_unmapped[:50]:
        inventory_claim = bool(row["inventory_exact"] or row["inventory_containing"])
        markdown.append(
            f"| `{row['address']}` | {row['name'] or ''} | {row['decoded_byte_count']} | "
            f"{row['inbound_function_count']} | {'yes' if row['reachable_from_recovered_roots'] else 'no'} | "
            f"{'yes' if inventory_claim else 'no'} |"
        )
    markdown.extend(
        [
            "",
            "The inferred-containing count can over-associate annotations when recovered candidates overlap. Exact starts remain the conservative metric.",
            "",
        ]
    )
    args.output_markdown.write_text("\n".join(markdown), encoding="utf-8")
    print(
        f"Indexed {len(rows)} recovered candidates: {len(exact_mapped)} exact native mappings, "
        f"{len(inferred_mapped)} inferred in-body mappings, {len(unmapped)} without native citations"
    )


if __name__ == "__main__":
    main()
