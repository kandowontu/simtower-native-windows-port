# Original compiler/runtime candidate classification

This audit explains the 264 recovered function candidates that deliberately
have no native game-source address citation. It is derived from
`native-port-coverage.json`, `simtower-callgraph.json`, the NE relocation
records, and the per-function disassemblies under
`original/disassembly/functions/`.

## Result

All 1,175 recovered candidates are now classified:

| Class | Candidates | Native treatment |
| --- | ---: | --- |
| Game-owned or game-observable routines | 911 | Explicit exact-start mapping in native source |
| Original compiler/C/C++ runtime support | 264 | Replaced by the native compiler, CRT, RAII, and Win32 process startup |

The support-only set is exhaustive and confined to three segments:

| Segment | Candidates | Reachable heuristic | Unreachable heuristic | Decoded candidate bytes | Classification |
| --- | ---: | ---: | ---: | ---: | --- |
| `1000` | 187 | 123 | 64 | 17,445 | Win16 process startup, far heap/selector support, C library, integer/floating helpers |
| `1260` | 58 | 15 | 43 | 13,286 | C++ exception, unwind, and saved-CPU-context runtime |
| `1268` | 19 | 6 | 13 | 2,326 | Exception-object allocation, reference counting, destruction, and unwind shims |

“Reachable” here is the call-graph prioritization heuristic, not a claim that a
particular play session executes the routine. Candidate-byte counts are the
non-overlapping decoded extents in the current candidate set.

## Segment evidence

### `1000`

The segment's external relocations are limited to Win16 loader/task/global
memory services, three USER startup/error services, and `WIN87EM.__FPMATH`.
There are no game-window, resource, simulation, save, sound, or drawing imports
in the uncited subset. Representative instruction bodies identify the standard
support roles directly:

- `1000:3994` is the compiler's 32-bit left-shift helper.
- `1000:45ab` is a far `memcpy` (`rep movsw` followed by `rep movsb`).
- `1000:1d8a` manages selector/far-heap metadata and validates its `0xfeed`
  heap signature.
- `1000:3bfe` is a deallocation wrapper over the shared far-heap free path.
- `1000:20a6` selects an entry from the runtime character/collation table.

Game-observable runtime algorithms are not hidden by this classification. For
example, the clock's signed shift helpers `1000:39b5/39ea` and the executable's
random-number behavior have explicit native mappings where their exact results
affect gameplay.

### `1260`

This segment has no external imports and no outbound call to any game segment:
its direct calls target only `1000` or `1260`. Direct incoming calls likewise
originate only in `1000`, `1260`, or `1268`.

- `1260:1497` saves general registers, segment registers, flags, and a large
  processor context before dispatching the runtime handler.
- `1260:3649` installs a stack-frame/unwind record.
- `1260:3622` obtains the current runtime frame record.
- `1260:36ba` restores/unlinks that record.

Those signatures and the closed call island classify all 58 candidates as the
original toolchain's exception/unwind runtime rather than game logic.

### `1268`

This segment also has no external imports. Its outbound calls target only
`1000`, `1260`, or `1268`, and all direct inbound calls originate only in
`1000` or `1268`.

Representative `1268:00e8` allocates an object, installs an unwind frame through
`1260:3649`, and adjusts a shared reference count. `1268:0304` performs the
matching reference decrement, destructor dispatch, deallocation through
`1000:3bfe`, and unwind-frame removal through `1260:36ba`. The remaining
candidates are the same allocation/copy/destruction support family.

## Native replacement boundary

The native executable does not copy or call any of these 264 original machine
code bodies. Normal C++ compilation supplies arithmetic and language-runtime
helpers; C++ value ownership and local RAII replace the old far-heap and unwind
bookkeeping; the Win32 entry point supplies process startup and termination.
This is a source-level port boundary, not an original-EXE runtime dependency.

The conservative address index intentionally continues to report these as
“without native-source citation”: an exact address citation means recovered
code was consulted for a game-visible implementation, while this document is
the separate evidence for an audited toolchain replacement. Address coverage
alone is not fidelity proof. The separate muted side-by-side, reciprocal-save,
90-second timing, silent hardware-audio, and final headless frame-order
validations are recorded in `RELEASE_VERIFICATION.md` and
`FINAL_COMPLETION_AUDIT.md`.
