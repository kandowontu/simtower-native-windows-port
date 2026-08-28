#!/usr/bin/env python3
"""Inspect variable-length SimTower .TDT transport-tail boundaries.

This is a diagnostic walker: it reports where competing documented elevator
payload formulas land without modifying the save.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


HEADER_SIZE = 560
FLOORS = 120
TENANT_SIZE = 18
FLOOR_INDEX_SIZE = 94 * 2
PERSON_SIZE = 16
RETAIL_SIZE = 512 * 18
ELEVATOR_HEADER_SIZE = 194


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def locate_elevator_table(data: bytes) -> tuple[int, int, int]:
    offset = HEADER_SIZE
    tenants = 0
    for _ in range(FLOORS):
        count = u16(data, offset)
        tenants += count
        offset += 6 + count * TENANT_SIZE + FLOOR_INDEX_SIZE
    people = u32(data, offset)
    offset += 4 + people * PERSON_SIZE
    return offset + RETAIL_SIZE, people, tenants


def payload_size(
    formula: str,
    elevator_type: int,
    cars: int,
    bottom: int,
    top: int,
    serviced: bytes,
) -> int:
    spans = top - bottom + 1
    stops = sum(1 for floor in range(bottom, top + 1) if serviced[floor])
    classic_express = sum(
        1
        for floor in range(bottom, top + 1)
        if floor <= 10 or (floor >= 24 and (floor - 24) % 15 == 0)
    )
    floors = (
        classic_express
        if formula == "classic-3488" and elevator_type == 0
        else stops
        if elevator_type == 0
        else spans
    )
    if formula in ("game-3488", "classic-3488"):
        return 3488 + floors * 324
    if formula == "eight-348":
        return 720 + floors * 324 + 8 * 348
    if formula == "cars-346":
        return 720 + floors * 324 + cars * 346
    if formula == "cars-348":
        return 720 + floors * 324 + cars * 348
    raise ValueError(formula)


def walk(data: bytes, start: int, formula: str) -> tuple[int, list[str], str]:
    offset = start
    entries: list[str] = []
    for slot in range(24):
        if offset + ELEVATOR_HEADER_SIZE > len(data):
            return offset, entries, f"slot {slot}: header crosses EOF"
        used, elevator_type, capacity, cars = data[offset : offset + 4]
        x = u16(data, offset + 62)
        top = data[offset + 64]
        bottom = data[offset + 65]
        serviced = data[offset + 66 : offset + 186]
        header = f"slot {slot} @{offset}: used={used} type={elevator_type} cap={capacity} cars={cars} x={x} floors={bottom}..{top}"
        offset += ELEVATOR_HEADER_SIZE
        if used == 0:
            continue
        if used != 1 or elevator_type > 2 or not 1 <= cars <= 8 or top < bottom or top >= 120:
            return offset, entries, f"{header} INVALID"
        size = payload_size(formula, elevator_type, cars, bottom, top, serviced)
        entries.append(f"{header} payload={size}")
        offset += size
        if offset > len(data):
            return offset, entries, f"slot {slot}: payload crosses EOF"
    return offset, entries, "complete"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="+", type=Path)
    args = parser.parse_args()
    formulas = (
        "classic-3488",
        "game-3488",
        "eight-348",
        "cars-346",
        "cars-348",
    )
    for path in args.files:
        data = path.read_bytes()
        start, people, tenants = locate_elevator_table(data)
        print(f"{path}: bytes={len(data)} tenants={tenants} people={people} elevators@{start}")
        for formula in formulas:
            end, entries, status = walk(data, start, formula)
            print(f"  {formula}: end={end} {status}")
            for entry in entries:
                print(f"    {entry}")


if __name__ == "__main__":
    main()
