# Final native-port completion audit

The deliverable is `release-native\SimTower.exe`, an x86-64 Windows GUI
executable built from the disassembly-backed native source under `port/`.
It does not execute, load, or locate the supplied Win16 executable or an
emulator at runtime.

## Requirement ledger

| Requirement | Completion evidence |
| --- | --- |
| Disassemble and audit the supplied game | 1,175 recovered candidates are classified. All 911 game-owned/game-observable starts have exact native-source mappings; the other 264 are exhaustively classified Win16 compiler, CRT, heap, and exception-runtime support replaced by the native toolchain. |
| Port gameplay and functions | Construction, demolition, pending activation, population/rating, finance, people families, routes, parking, elevators, stairs/escalators, events, annual effects, clock/calendar, menus, dialogs, Find, Map, Information, and save transitions are production-consumed through their recovered call boundaries. The final `1090:03ab` pass preserves per-Elevator nesting, synchronous host callbacks, preview scratch state, renderer checkpoints, and presentation tail order. |
| Preserve original graphics and UI | Original resources are embedded in the PE and decoded by the native resource path. The world, Command, Map, Info, dialogs, caption, controls, construction outline, sprites, and effects use recovered selectors and geometry. The screenshot audit restored `11c0:0000`'s transparent-index-zero exterior blits, eliminating the native-only white crane and edge boxes. |
| Preserve construction topology | `11f8:17fd/30ef` intentionally fills a separated placement with Floor. Demolition coalesces adjacent type-zero/status-two Floor spans; rebuilding splits the merged span and preserves neighboring facilities. Both model-level and client-coordinate regressions cover the reported case. |
| Preserve audio | Original WAVE bytes are embedded and the production backend uses WinMM/WAVE_MAPPER with recovered arbitration and checkpoint order. Earlier explicitly permitted hardware validation used same-format digital silence; ordinary execution submits the original samples. |
| Preserve save behavior | Native reads and writes the recovered revision-0x24 TDT layout, upgrades older/opposite-endian streams as the original writer does, passes byte/round-trip fixtures, and a deliberately mutated native-written save was accepted and rendered by the supplied original. |
| Self-contained executable | PE imports are limited to COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs. ASCII and UTF-16LE scans contain no DOSBox/Mesen, original-EXE, source-network-path, disassembly-tree, or validation-tree reference. |
| Verification | All 14 optimized Release suites and all 14 unoptimized Debug suites pass. The packaged file is byte-identical to the tested Release build. The latest correction was validated without launching the game, an emulator, a visible window, or an audio path. |

## Final artifact

- Path: `release-native\SimTower.exe`
- Size: `10,369,638` bytes
- SHA-256: `23E52DDBE182E9CD9A9D3C4D4210B254E54D6AFCC5216EDCAEE95EF48E4ACA0B`
- Format: x86-64 Windows GUI PE
- Headless routine ledger: 1,484 unique native-source address citations and
  1,035 unique native-test address citations
- Direct-test boundary: 909 of 911 mapped starts; the two exceptions are the
  runtime-smoke-covered GUI process entry and an unreachable no-inbound debug
  formatter
- Final exact-name process sweep: clear

The rejected WebView prototype under `dist/` is not part of this completion
and must not be substituted for the release above.
