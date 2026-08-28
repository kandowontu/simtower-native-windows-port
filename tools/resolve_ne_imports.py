#!/usr/bin/env python3
"""Resolve an NE import audit against exported names from local Win16 modules."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    parser.add_argument("module_reports", type=Path)
    parser.add_argument("--json", type=Path, required=True)
    parser.add_argument("--markdown", type=Path, required=True)
    args = parser.parse_args()

    report = json.loads(args.report.read_text(encoding="utf-8"))
    export_maps: dict[str, dict[int, str]] = {}
    for module_path in args.module_reports.glob("*.json"):
        module = module_path.stem.upper()
        module_report = json.loads(module_path.read_text(encoding="utf-8"))
        export_maps[module] = {
            int(entry["ordinal"]): entry["name"]
            for entry in module_report["entries"]
            if entry.get("name")
        }

    resolved: list[dict[str, object]] = []
    for item in report["external_imports"]:
        module = str(item["module"]).upper()
        symbol = item["symbol"]
        if item["kind"] == "ordinal":
            name = export_maps.get(module, {}).get(int(symbol))
        else:
            name = str(symbol)
        resolved.append(
            {
                "module": module,
                "ordinal": symbol if item["kind"] == "ordinal" else None,
                "name": name,
                "reference_count": len(item["references"]),
                "references": item["references"],
            }
        )

    resolved.sort(key=lambda item: (str(item["module"]), str(item["name"]), int(item["ordinal"] or 0)))
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps({"imports": resolved}, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# SimTower Win16 API import audit",
        "",
        "Names are resolved from the exact Win16 DLLs found on the supplied disk image.",
        "Reference counts are relocation sites in `SIMTOWER.EXE`, not runtime call counts.",
        "",
        "| Module | Ordinal | API | References |",
        "| --- | ---: | --- | ---: |",
    ]
    for item in resolved:
        name = item["name"] or "*(unnamed export)*"
        ordinal = item["ordinal"] if item["ordinal"] is not None else "—"
        lines.append(
            f"| {item['module']} | {ordinal} | `{name}` | {item['reference_count']} |"
        )
    lines.append("")
    args.markdown.parent.mkdir(parents=True, exist_ok=True)
    args.markdown.write_text("\n".join(lines), encoding="utf-8")

    named = sum(1 for item in resolved if item["name"])
    print(f"resolved {named} of {len(resolved)} unique imports")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
