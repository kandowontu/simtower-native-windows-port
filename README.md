# SimTower native Windows port

This repository contains a function-by-function, x86-64 Windows port of the
Windows 3.1 version of *SimTower*. It is an unofficial preservation and
interoperability project and is not affiliated with or endorsed by the
original creators, Maxis, or Electronic Arts.

See [CREDITS.md](CREDITS.md) for the
original game and third-party credits and [DISTRIBUTION.md](DISTRIBUTION.md)
for the release boundary.

## Current source of truth

- `analysis/simtower-ne.json`: exact NE segments, exports, and relocations from the locally supplied executable.
- `analysis/simtower-imports.json`: Win16 import ordinals resolved against the DLLs supplied on the disk.
- `analysis/simtower-callgraph.json`: recursive control-flow analysis of this executable, including compiler prologs and 170 recovered switch/message tables.
- `analysis/NE_CALL_GRAPH.md`: compact call-graph summary.
- The private working tree's ignored disassembly contains one relocation- and
  call-annotated assembly file per classified routine.
- `analysis/PORT_STATUS.md`: current audit and translation status.

The current graph classifies 1,175 executable routines, decodes 96.04% of all 309,486 code-segment bytes, and resolves 2,221 imported API callsites. The remaining code-segment bytes are primarily inline switch/message tables and embedded strings; pointer targets are kept separate from functions so data is not counted as code.

All 911 recovered game-owned starts have exact native mappings, all 14 native
test suites pass, and the explicitly permitted muted validation covered the
original/native startup, New Tower, Win16 main frame, legacy Open dialog,
Cancel, clean exit, and reciprocal original/native loading of a deliberately
mutated native-written save. The final timing audit also corrected native Fast
Mode's modern busy-loop runaway: the recovered dispatcher remains unchanged,
while its native host is paced at the reference-observed 58-ms full-frame
cadence (approximately 777 frames per 45 seconds). Isolated native captures at
startup and after 90 seconds both retain the original save's
`1st WD/1Q/1st Year` date. A real Windows PCM submission using same-format
digital silence also passes. The normal audio path retains and submits the
exact embedded original samples; validation made no speaker noise.

The local asset-bearing artifact and requirement ledger are recorded in
`analysis/FINAL_COMPLETION_AUDIT.md`.

## Building locally

The native target is under `port/` and uses CMake 3.20 or newer with a MinGW
C++20 toolchain and `windres`. A local build also requires resources extracted
from a lawfully acquired copy of the Windows game:

- `port/generated/original_resources.pack`
- `port/generated/original_resources.generated.hpp`
- `original/extracted/MAXIS/SIMTOWER/SIMTOWER.HLP`

The repository includes the analysis and packing tools, but not those inputs or
generated outputs. Configure with `cmake -S port -B build/port-make`, then build
with `cmake --build build/port-make --config Release`.

## Porting rule

A subsystem is not considered ported because a replacement implementation has tests. It is complete only when its original routines, data structures, resource consumers, and callers are mapped and translated into the native implementation.

## Rights notice

The original game's code, graphics, sounds, help, trademarks, and other
resources remain the property of their respective rights holders. Commercial
unavailability does not place them in the public domain. This repository does
not grant permission to redistribute them.
