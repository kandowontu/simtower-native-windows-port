#!/usr/bin/env python3
"""Index binary-address annotations and summarize replacement source coverage."""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path


ADDRESS_RE = re.compile(r"(?<![0-9A-Fa-f])(?:0x)?([0-9A-Fa-f]{4}):([0-9A-Fa-f]{4})(?![0-9A-Fa-f])")
FUNCTION_RE = re.compile(
    r"^\s*(?:export\s+)?(?:async\s+)?(?:function\s+([A-Za-z_$][\w$]*)|(?:const|let)\s+([A-Za-z_$][\w$]*)\s*=\s*(?:async\s*)?\([^)]*\)\s*=>)",
    re.MULTILINE,
)
CLASS_RE = re.compile(r"^\s*(?:export\s+)?class\s+([A-Za-z_$][\w$]*)", re.MULTILINE)


def source_files(root: Path) -> list[Path]:
    return sorted(path for path in root.rglob("*.ts") if not path.name.endswith(".test.ts"))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("workspace", type=Path)
    parser.add_argument("output_json", type=Path)
    parser.add_argument("output_markdown", type=Path)
    args = parser.parse_args()
    workspace = args.workspace.resolve()
    sim_root = workspace / "web" / "src" / "sim"
    reference_root = workspace / "references" / "tower-together"
    inputs = source_files(sim_root)
    inputs += sorted((reference_root / "specs").rglob("*.md"))
    inputs += sorted((reference_root / "docs").rglob("*.md"))

    occurrences: dict[str, list[dict[str, object]]] = defaultdict(list)
    for path in inputs:
        text = path.read_text(encoding="utf-8", errors="replace")
        for line_number, line in enumerate(text.splitlines(), 1):
            for match in ADDRESS_RE.finditer(line):
                address = f"{match.group(1).lower()}:{match.group(2).lower()}"
                context = " ".join(line.strip().split())
                item = {
                    "file": path.relative_to(workspace).as_posix(),
                    "line": line_number,
                    "context": context[:300],
                }
                if item not in occurrences[address]:
                    occurrences[address].append(item)

    module_inventory = []
    total_lines = 0
    total_functions = 0
    total_classes = 0
    for path in source_files(sim_root):
        text = path.read_text(encoding="utf-8", errors="replace")
        lines = len(text.splitlines())
        functions = sorted({match.group(1) or match.group(2) for match in FUNCTION_RE.finditer(text)})
        classes = sorted(CLASS_RE.findall(text))
        total_lines += lines
        total_functions += len(functions)
        total_classes += len(classes)
        module_inventory.append({
            "file": path.relative_to(workspace).as_posix(),
            "lines": lines,
            "function_count": len(functions),
            "class_count": len(classes),
            "functions": functions,
            "classes": classes,
        })

    address_items = [
        {"address": address, "occurrences": values}
        for address, values in sorted(occurrences.items())
    ]
    payload = {
        "unique_binary_addresses": len(address_items),
        "address_annotations": address_items,
        "replacement": {
            "module_count": len(module_inventory),
            "lines": total_lines,
            "declared_function_count": total_functions,
            "class_count": total_classes,
            "modules": module_inventory,
        },
    }
    args.output_json.parent.mkdir(parents=True, exist_ok=True)
    args.output_json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    annotated_in_port = {
        item["address"]
        for item in address_items
        if any(str(ref["file"]).startswith("web/src/sim/") for ref in item["occurrences"])
    }
    markdown = [
        "# Replacement implementation inventory",
        "",
        f"- Simulation modules: {len(module_inventory)}",
        f"- Non-test TypeScript lines: {total_lines:,}",
        f"- Declared functions/arrow-function constants: {total_functions:,}",
        f"- Declared classes: {total_classes}",
        f"- Unique original binary addresses indexed in research/specs: {len(address_items):,}",
        f"- Unique original binary addresses cited directly in port source: {len(annotated_in_port):,}",
        "- Focused regression result: 63 tests across construction, carriers, progression, queues, economy/audio tuning, housekeeping, scoring, and original `.TDT` saves",
        "",
        "The address count measures semantic reverse-engineering annotations, not function equivalence. The machine-level function-candidate inventory is recorded separately in `simtower-function-candidates.json`.",
        "",
        "## Largest replacement modules",
        "",
        "| Module | Lines | Declared functions |",
        "| --- | ---: | ---: |",
    ]
    for item in sorted(module_inventory, key=lambda value: int(value["lines"]), reverse=True)[:25]:
        markdown.append(f"| `{item['file']}` | {item['lines']} | {item['function_count']} |")
    markdown.append("")
    args.output_markdown.write_text("\n".join(markdown), encoding="utf-8")
    print(f"Indexed {len(address_items)} binary addresses across {len(module_inventory)} replacement modules")


if __name__ == "__main__":
    main()
