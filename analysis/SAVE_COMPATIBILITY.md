# Original `.TDT` save compatibility

## Current verified boundary

The previous document claimed full native import/export compatibility. That
claim described the rejected TypeScript prototype and was not established by a
translation of this executable. It is withdrawn.

The native C++ implementation now translates the exact prefix consumed by
`10d0:0b3a`, plus the opposite-endian compatibility branch at `10d0:1518`:

- the special version word and original return statuses (1 short transfer, 4
  version too new, 5 version too old);
- format revisions `0x17` through `0x24`, including the `0x20`, `0x23`, and
  `0x24` conditional fields;
- the fixed 490-byte miscellaneous header block;
- all 120 floor records, 18-byte tenant records, and 94-entry per-floor index
  maps (byte entries before revision `0x24`, words at `0x24`);
- the people count and 16-byte people-table extent;
- the fixed 512 x 18-byte retail/service table;
- all 24 elevator headers, including the pre-`0x21` split-header migration;
- the exact `10a0:17ee` express/contiguous floor-record map;
- each built shaft's three fixed blocks, 324-byte mapped-floor records, and all
  eight 346-byte current car records (`3488 + 324 * mapped floors` total
  payload), plus the pre-`0x22` four-segment 316-byte car migration, its two
  zero-retained gaps, pre-`0x19` active-car default, and pre-`0x22` capacity
  repair;
- the complete post-elevator transfer sequence: state, finance, parking, 64
  stair records, eight routing records, fixed tables, three dynamic tables,
  and the revision-gated final blocks;
- native field decoding for the three 10-category finance ledgers and totals,
  the 512-entry parking table, the 10-entry index list, and stair position/state
  records, with the opposite-endian word/dword transforms preserved;
- every raw reversal in `10d0:1518`, including people offsets 2/10/12/14,
  retail offsets 12/14/16, the complete elevator graph/floor/car payload,
  first 480 bytes of each route, `cf9c`/`db9c`, all three dynamic banks, and
  the final `dd6c` words; the original's deliberate no-reversal transfers for
  `cf88`, `dbfc`, `dc24`, and each route's final dword are retained;
- lossless preservation of bytes trailing the executable's final transfer.

`original_tdt.cpp` is tested against four files without launching the original:

| Save | Bytes | Raw tenants | People | Elevator start/end per supplied EXE | Lossless result |
| --- | ---: | ---: | ---: | ---: | --- |
| `s2b-exported.TDT` | 65,150 | 2 | 0 | 33,096 / 37,752 | byte-exact |
| `ElDlux.TDT` | 518,528 | 1,412 | 15,360 | 304,236 / 499,276 | byte-exact |
| `RoyalA.TDT` | 647,740 | 2,480 | 22,784 | 442,244 / 628,536 | byte-exact |
| `SimEmpire.TDT` | 809,932 | 2,714 | 33,024 | 610,296 / 790,680 | byte-exact |

The loader's complete byte-transfer boundary is represented. The general
serializer can still rebuild the represented source revision for analysis,
while the game-facing disk writer now follows `10d0:0b3a`'s distinct policy:
every Save emits revision `0x24` little-endian. Imported old and
opposite-endian documents are migrated through the recovered runtime layout,
including widened floor indexes, split elevator headers, segmented cars,
expanded `dce4`/zeroed `dd34`, `dd6c`, opaque word/dword banks, the express-
elevator unmapped-floor `-1` swap quirk, and `10d0:1b4d-1b62`'s destructive
tenant-subtype zeroing. An independently walked asymmetric opposite-endian
fixture converts byte-for-byte to the expected current stream; a revision
`0x18` fixture covers all legacy elevator migrations, and the actual native
file-save entry point is checked to upgrade revision `0x17` on disk. The exact
fresh revision-`0x24` constructor remains a 65,112-byte byte-exact round trip.

Original-game acceptance is now verified as well. The current-revision native
writer loaded `s2b-exported.TDT`, changed its raw balance to `2,345,678`, and
wrote a 65,150-byte `NATIVE.TDT` with SHA-256
`C99E96B40A66329EEB9B38D3478F3BB77DFC6405DE31C854EB21B731AA706F76`.
Only a disposable copy of the reference image was modified; the base image
remained byte-identical at SHA-256
`7D8CE6D136E7DC2B1BF6F28E5323CC51737B7C3CC68102A6689A66C10B978D18`.
The supplied original executable opened that native-written stream as
`SimTower - NATIVE`, displayed Fund `$234567800`, and rendered its tower. The
native executable then reopened the same file through a quoted absolute path,
showed the same title, balance, and tower, and exposed a modern-host quote bug
that is now covered by a regression: native startup removes exactly one
matching outer quote pair before entering the unchanged recovered Win16 path
logic. Evidence captures are
`.runtime/validation/reference-native-save-load-result.png` and
`.runtime/validation/native-save-direct-load-fixed-refreshed.png`.

Opposite-endian parsing also performs those transformations immediately on
the live native document. People, tenant, retail, elevator fixed/floor/car,
route, link, and dynamic records therefore have the same little-endian runtime
byte layout that exists after `10d0:1518`, including its overlapping express-
elevator write. Direct byte-oriented gameplay no longer observes foreign file
order between Open and the first Save. `document.exact_bytes` remains the
untouched source stream, so the explicit lossless tooling serializer can still
recover the imported bytes exactly.

The Win16 file-command boundary is also translated from `10d0:0122`,
`10d0:0225`, `10d0:0305`, `10d0:03f1`, `10d0:0604`, `10d0:062a`,
`10d0:0777`, and `10d0:2a8e`: command IDs, open/save filters, captions,
128/15-character buffers, the original `GETOPENFILENAME`-based Save As path,
`.TDT` suffix replacement, eight-character DOS basename check, overwrite
question, 2/4/5 error-string mapping, and window-title construction. Open and
Save As now bracket every common-dialog attempt with `1078:01e8(0/1)`'s exact
palette-window demotion/restoration and Arrow-cursor publication; Save As
retries the dialog after an over-eight-character basename. A post-create
transfer failure closes and deletes the partial target at `10d0:080e-0837`,
while an initial create failure leaves no owned path to delete. The
New/Open/Exit confirmation predicate is not a dirty-since-save flag:
`10d0:2a8e` requires an active document and a nonzero first word at the
`DS:b622` floor pointer, represented exactly by occupancy of native floor slot
10. It therefore prompts even immediately after Save once that floor has a
tenant, and skips only the empty pre-Lobby tower. `original_tdt_file.cpp`
supplies the native disk boundary and is covered by a write/read byte-exact
round-trip test; fresh New state is built by the translated constructor.
