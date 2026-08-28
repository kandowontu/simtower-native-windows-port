# Rejected prototype verification

`dist\SimTowerNative.exe` is a self-contained WebView prototype built on 2026-08-15. It is **not a faithful port and is not a release candidate**.

The previous 63 passing tests, embedded-file checks, PE dependency scan, and hash verification establish only that the prototype builds and runs. They do not establish original gameplay, rendering, UI, animation, audio, or save compatibility.

That rejected-prototype record is not evidence for the native port. The
current native release-candidate record follows.

## Current native release

The tested native Win32 build was refreshed and copied byte-identically to
`release-native\SimTower.exe` on 2026-08-27 after the disassembly-backed world
renderer, construction-coordinate, edit hit-test, scheduler timing, exterior
mask, and complete-frame host-order audits.
Neither game nor an emulator was launched for this latest refresh.

- Size: `10,369,638` bytes
- SHA-256: `23E52DDBE182E9CD9A9D3C4D4210B254E54D6AFCC5216EDCAEE95EF48E4ACA0B`
- PE: x86-64 Windows GUI (`pei-x86-64`)
- Headless tests: `14/14` passed
- Static routine audit: `1,175` recovered candidates, `911` exact native
  mappings, `1,484` unique native-source address citations, `0` inferred
  mappings counted as exact, and `264` separately classified compiler/runtime
  candidates without native citations
- Direct test evidence: `1,035` unique recovered-address citations; `909` of
  the `911` mapped starts have direct citations, leaving GUI process entry and
  one unreachable diagnostic outside direct-test eligibility
- Runtime smoke: New Tower, main-window creation, compact legacy Open dialog,
  Cancel, quoted-path native save loading, and clean process exit all passed
  with game audio disabled. The final native timing process ran on a separate,
  non-interactive Windows desktop, remained responsive for 90 seconds, and was
  terminated after capture.
- Hardware PCM smoke: production WAVE_MAPPER open/prepare/write/active/stop/
  close completed for WAVE/20000 (50,300 bytes, 11,127 Hz, 8-bit); its explicit
  validation payload was same-format digital silence, so it made no noise
- Save interoperability: a native-written, deliberately balance-mutated
  65,150-byte revision-0x24 stream (SHA-256
  `C99E96B40A66329EEB9B38D3478F3BB77DFC6405DE31C854EB21B731AA706F76`)
  was accepted and rendered by the supplied original, then reopened by native
  through the corrected quoted absolute-path startup boundary
- Reference comparison: fresh 640x480 original startup and New Tower captures
  were obtained from the supplied runnable image; native startup, New Tower,
  Win16 caption, and Open-dialog states were then compared. The reference
  analog clock advanced approximately 777 frames between its 45- and
  90-second captures. Native Fast Mode now retains that observed 58-ms host
  cadence instead of advancing at modern busy-loop speed; isolated native
  start and 90-second captures both display `1st WD/1Q/1st Year`, matching the
  reference save state.
- PE imports: `comdlg32.dll`, `GDI32.dll`, `KERNEL32.dll`, `USER32.dll`,
  `WINMM.dll`, and Windows Universal CRT API-set DLLs only
- Forbidden ASCII/UTF-16LE runtime-reference scan: clear for DOSBox, Mesen,
  the original executable, supplied network path, extraction tree, and
  reference-runtime paths
- Final exact-name process sweep: no emulator, game, or media process remained

This establishes that the artifact is a standalone native PE and does not load
the original executable at runtime. The visual validation specifically removed
the modern Windows 11 caption and Explorer-picker substitutions: Main now has
the original centered system/down/up caption structure over classic menu and
scrollbar chrome, while Open/Save uses the compact 470x247 legacy control set,
original labels/capitalization, and no later Network control. Exact embedded
WAVE bytes, scheduling/arbitration tests, and the successful silent hardware
transaction jointly verify the native audio path without requiring speaker
output; the ordinary branch submits the unmodified original samples.

The save acceptance used only a disposable copy of the runnable reference
image. Its base remained SHA-256
`7D8CE6D136E7DC2B1BF6F28E5323CC51737B7C3CC68102A6689A66C10B978D18`.
The original displayed `SimTower - NATIVE`, Fund `$234567800`, and the expected
tower; native displayed the same title, fund, and scene. The initial reciprocal
native launch caught a real host-boundary defect because modern Windows quotes
association paths containing spaces. The release now removes exactly one
matching outer quote pair before calling the unchanged recovered Win16 target
logic, with direct regression coverage. Captures are retained under
`.runtime/validation`; host audio remained disabled, and the final process
sweep is clear.

The current quiet static checkpoint exposes and directly tests exact Office
occupancy (`1220:6bef/6cb6`), Stair span/scoring/transfer routing
(`11b0:0dc0/0e80/141c/14c9/0a21/0ad4`), Lobby graphics tier
(`11f8:06cd`), Movie length (`1100:25d9`), commercial closed-hours
(`1108:05e3`), route-boundary (`11b0:0763`), and event-floor (`10c8:033e`)
cores. Additional exact state/pixel evidence now anchors presentation, file,
accounting, Find, transit, fire, rating, construction, information, Map, and
Elevator Control helpers, including signed and opposite-endian edges. All 14
headless suites pass. The ledger contains 911 exact mappings, 1,449 source
citations, 816 direct test citations, and 203 mapped starts without direct
evidence. The refreshed 10,337,876-byte standalone PE matches the tested build
at SHA-256
`153002F79636081258C329B5270382E95BBBDEA562D6362F6776E8C556D76853`, imports
only the listed Windows/UCRT DLLs, and passes every forbidden ASCII/UTF-16LE
runtime-reference check. It was not executed, no audio device was opened, and
both exact-name process sweeps were clear.

The newest quiet static batch replaces Facility Information's local
`1100:4439` painter with a production-consumed core boundary and an in-memory
GDI regression covering the exact RGB(204,204,204) backing, centered 200%
COLORONCOLOR stretch, source quadrants, and DC-state restoration. The
`1020:0f4f` palette constructor is likewise shared and verifies all 256 native
`PALETTEENTRY` values through `GetPaletteEntries`. Static comparison found one
visible Elevator Control discrepancy at `1098:27bd/2893`: the original uses
CBW on persisted schedule bytes, so `0xff` displays as `-1` and `-30`, not
`255` and `7650`; native now preserves that signed behavior. Direct boundary
evidence was also added for ordinal suffixes, Movie/Party schedule changes,
wrapping maintenance width, two-lane waiting focus, pending construction,
upper/lower service joins, and Cathedral composition. All 14 headless suites
pass. The ledger contains 911 exact mappings, 1,449 source citations, 720
direct test citations, and 297 mapped starts without direct evidence. The
refreshed 10,332,533-byte standalone PE matches the tested build at SHA-256
`B2EF681E432BC2F37567665CE220C67B2CF01A34FBCAC8ED0ACE23598B095A85`, imports
only the listed Windows/UCRT DLLs, and passes the forbidden ASCII/UTF-16LE
runtime-reference and exact-name process scans. It was not executed and no
audio device was opened.

The latest quiet static batch corrects several visible and persisted-state
edges without launching either executable. Map focus adjustment at
`1080:055d` now performs the recovered direct-DC XOR erase, coordinate
recalculation, and XOR redraw transaction; Person Information at `1100:0000`
preserves the Find-exit latch; Cathedral saved-name resolution at `1188:0aa0`
uses the floor-109/header-b3ec link; and Elevator dwell at `1090:23a5`
preserves signed `-32768` through the original wrapping absolute-value path.
The Open and Save As `OPENFILENAME` records at `10d0:0122/03f1` now use the
literal zero Flags dword instead of the native-only `OFN_READONLY` bit.
Finance value placement at `11e0:00ca`, Person Information meter endpoints,
colors, and logical-palette brush selection at `11e0:01d8/0358`, and the
temporary white-brush fill wrapper at `1208:05a9` are production-consumed and
directly tested. Additional direct pixel/state evidence covers command
selector sheets, facility-person atlases, signed Map scaling, name-table
updates, transport queues, schedule-bank transactions, DTMP text origins,
and adjacent Elevator waiting-lane boundaries. All 14 headless suites pass.
The ledger contains 911 exact mappings, 1,449 source citations, 706 direct test
citations, and 310 mapped starts without direct evidence. The refreshed
10,331,176-byte standalone PE is byte-identical to the tested build at
SHA-256
`6A1769EBA48EF16B919C651506005E8793C4FF2A220019F3C270A1474267A0F7`, imports
only the listed Windows/UCRT DLLs, and passes the forbidden ASCII/UTF-16LE
runtime-reference scan. It was not executed and no audio device was opened.

The current quiet static batch corrects three observable input/refresh
transactions. Grouped command selection at `1058:04e0` now checks the physical
primary button (including swapped-button systems), opens DIALOG/124 only while
that button remains held, and preserves the current choice on button-up or
cancel. Full-view refresh at `1080:0a02` now runs Main, Map, then Command
synchronously. Successful Find at `10e0:0cea` now performs Command, Map,
preview-scratch restoration, then focused-camera refresh in the recovered
order. Direct evidence was also added for DIB loading `1030:0000/0043`, the
complete TABL/TABM rating matrix `1140:022c`, all message-box jump-table modes
at `1208:0369`, and Find-name slot lookup `1188:04db/0541/05a7`. All 14
Release/headless suites pass. Coverage is 911 exact mappings, 1,439 source
citations, and 622 direct test citations, leaving 391 mapped starts without
direct evidence. The 10,329,898-byte packaged PE is byte-identical to the
tested build at SHA-256
`BD763042C0685F3F9F6547DB05C5891076515BE329E5DE1D457882D597DA59C5`, imports
only the listed Windows/UCRT DLLs, and passes the forbidden ASCII/UTF-16LE
runtime-reference scan. It was not executed, no audio device was opened, and
the final exact-name process sweep found no prohibited process running.

The newest quiet static batch repairs the complete New/successful-Open visible
tail at `10d0:001d/062a`: Map focus adjustment, Fire Crew menu update,
`1080:0a02`'s Main/Map/Command transaction, and `1118:0000`'s Info repaint now
run synchronously in the recovered six-step order, including the intentional
second Map presentation. The `11c8:02c0` channel-stop predicate is now shared
by production and tests and preserves its sound-enabled, mixer-active,
force-nonzero, and two-channel gates. Direct evidence also anchors
`10d0:2a8e`, `1178:0854/08ec`, `1180:0f87`, `1240:020d`,
`1070:051f/0570/05a1`, `11e0:0e00/0e22/0e60`, `1208:00dc/0cf5/0dfc`,
`1188:007e/01be`, `1060:08be/0958`, and `10e0:078d`. The last two WinG
helpers corrected a visible/process-boundary divergence: transport text
origin addition now preserves 16-bit wrapping, and fatal errors perform
MessageBeep(0x30) followed by FatalAppExit code zero instead of an invented
native message-box title and return code. All 14 headless suites pass. The
ledger contains 911 exact mappings, 1,439 source citations, 644 direct test
citations, and 368 mapped starts without direct evidence. The refreshed
10,329,921-byte x86-64 PE is byte-identical to the tested build at SHA-256
`5B6EDEE62A70D2D1BA0B2876D09672F50CC964A0845190F2F81D187D48CD43CE`,
imports only the listed Windows/UCRT DLLs, and passes the forbidden
ASCII/UTF-16LE runtime-reference scan. Neither executable nor an audio path
was launched.

The latest quiet static batch directly audits `11e0:0950`, `1028:1534/1692`,
`1220:5edd`, `11a0:0126/027c`, `1100:2031`, and `1188:02ea`. The placeholder
bevel, both Restaurant/Fast Food presentation steppers, Cathedral wrapper,
opaque type-1/type-16 atlas paths, and commercial attendance meter already
matched and now have direct pixel/state/RNG evidence. The save loader required
one compatibility correction: it now consumes and normalizes `02ea`'s older
256-byte Pascal Find-name records, and rebuilds the b402/b404 tables through
the recovered duplicate-key compaction semantics. Synthetic legacy, duplicate,
short-transfer, and re-save tests cover the branch while all four supplied
saves still rebuild byte-exactly. All 14 headless Release suites pass. Coverage
remains 911 exact mappings and 1,434 source citations; direct test citations
rise to 585, leaving 429 mapped starts without direct evidence. The packaged
10,330,051-byte x86-64 PE is byte-identical to the tested build at SHA-256
`925336B00E8FBA88B1F5E5B134905D2E44C1FB38FD2FB5280E5EE82EFE034FC2`, imports
only the listed Windows/UCRT DLLs, and passes the forbidden ASCII/UTF-16LE
runtime-reference scan. It was not executed and no audio device was opened.

The latest quiet static batch directly audits `1170:0291`, `1180:06a8`,
`11f8:2291/321e`, `1258:000b/0186`, `1218:08cd`, `1178:0b44`, `1210:11c2`,
`1108:030d`, `1220:55b8`, `1100:1b53`, and `1100:1dca`. Three gameplay
accounting/routing divergences are corrected: an unavailable Medical Center
now applies `11d8:02f7`'s waiting-delay and `11d8:0000` metric finalizer;
Retail's direct byte-six selector is sign-extended before comparison with the
full 16-bit record index; and three-day maintenance uses the original direct
wrapping debit rather than the positive-income cap at `1178:1377`. Person
Information now preserves `1dca`'s DS:b3a6 source context: main-world people
use a live elapsed wait, transport passengers use the retained low ten bits,
and facility occupants use zero. Its Lobby elapsed discount also uses the
original signed comparison. Direct tests cover these corrections plus the
audited host-loop, construction, Cathedral, entertainment, queue, advisory,
and information branches. All 14 headless Release suites pass. The packaged
10,323,280-byte x86-64 PE is byte-identical to the tested build at SHA-256
`F003C81B4CE899FF881A1BAA1D0C1FACAE820BF43E7F42F4FE1CC4A5AA791B19`, imports
only the listed Windows/UCRT DLLs, and passes the forbidden ASCII/UTF-16LE
runtime-reference and zero-process scans. It was not executed and no audio
device was opened.

The latest static collision/presentation batch completed end-to-end audits of
`10a0:10e8`, `1198:07e6`, `1210:0f0e`, `1228:0b59`, `1100:2ec2`, and
`1100:307e`. Two persisted-state signedness mismatches were corrected.
`10a0:1247-124c` sign-extends the Stair shape byte and uses arithmetic
`SAR AX,1`; the native new-Elevator collision path formerly divided an
unsigned byte and could turn shape `ff` into a false 127-story obstruction.
`1100:3124-312b` compares the Cathedral anchor-state word with signed `JGE`;
the former unsigned native comparison could display Cathedral occupants for a
high-bit malformed state. Direct regressions now cover both mismatches, the
complete Parking connectivity rebuild, Elevator boarding family/route
selection, all evening facility jump-table families, and Movie/Cathedral
item-4/item-9 person lineups. All 14 headless Release suites pass. The package
is byte-identical to the tested build at the current hash, retains only the
listed Windows/UCRT imports, and contains none of the scanned original-EXE,
DOSBox, supplied-network-path, or extraction-tree strings in ASCII or
UTF-16LE. Neither game executable nor any audio path was launched.

The latest static batch directly compares the 513-byte people-pool clearer
`1238:029f`, the Retail wrapper `1220:426c`, shared clock quotient
`1200:0543`, and Elevator Control initialization `1098:0780`. The pool clearer
already matched: native 256-record growth value-initializes every byte covered
by the original's twelve stores. Retail wrapper control flow also matched, but
its shared clock dependency exposed a signedness error. `1200:0543` executes
`CWD` plus signed `IDIV 0x0190`; native had divided the persisted frame word as
unsigned. The signed phase now propagates through scheduling, people,
construction, information, and world decisions, with explicit guards at native
array boundaries. `1098:0786-0793` also now copies every phase byte verbatim
and changes only exact phase six to five instead of native-clamping every
out-of-range value. Direct regressions cover negative high-bit clock words,
Retail state five, raw Elevator Control phase bytes, and a complete newly grown
people block. All 14 headless Release suites pass. Coverage is 911 exact native
mappings, 1,433 source citations, and 558 direct test citations, leaving 459
mapped starts without direct evidence. The packaged 10,321,951-byte x86-64
Windows GUI PE is byte-identical to the tested build at SHA-256
`049FB38161BAE87E6B3C0FBF471531CC2660D2D73A80027D40E2D3ECA342E27C`. It
imports only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs;
ASCII and UTF-16LE scans find no original-EXE, DOSBox, supplied-network-path,
or extraction-tree reference. No game, emulator, or media process remains.
The artifact was not executed and no audio device was opened.

The latest five-routine static audit directly compares `1090:1d2f`,
`11c8:06b6`, `1160:0000`, `1228:0968`, and `1100:3856`. Elevator-car
direction arbitration, the facility sound selector, and Map backing
initialization already matched. Two persisted-state mismatches were repaired.
Medical Center type 13 now follows `1228:0ada/0d1b`: both the day-start and
evening sweeps mark it dirty without clearing its complete word at tenant
`+0x0c`. The Person portrait type-7 tail now follows `1100:38d8-38df`'s signed
`JG`, so high-bit state words select frame one after the exact word-four and
word-five cases. Direct regressions cover the complete affected branches. All
14 headless Release suites pass. Coverage is now 911 exact native mappings,
1,429 source citations, and 553 direct test citations, leaving 463 mapped
starts without direct evidence. The refreshed 10,320,927-byte artifact is
byte-identical to the tested build at SHA-256
`A9D6ADE5094E68DBC6A89B4BEB096D50FC52B337C954494C84A8CE9FCEA001A7`, imports
only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs, and
passes the forbidden ASCII/UTF-16LE runtime-reference scan. It was not
executed and no audio device was opened.

The latest static Elevator presentation audit corrects
`10a8:0000/022b/02aa/0507/07d6` and `1090:0cb3/0b10`. The native frame result now retains both
last-writer slots for every floor/Elevator pair instead of collapsing the
whole tower to one boarding and one alighting person. Rendering uses the
original complete per-floor diminishing-gap Shell sort and interleaves each
shaft with its boarding then alighting slots, so a later right-hand shaft can
cover an adjacent transfer sprite. Explicit `1080:0a1e` rebuilds clear the
process-only slots at `10a8:0000`'s exact pre-redraw boundary. The SHOW split
is also restored: nonzero shafts bake cars into their bodies and skip the late
compositor; zero shafts retain both caps, draw only two black 35-pixel in-span
boundaries, and alone receive the post-Stair/Escalator moving-car layer.
Multi-floor/multi-shaft overwrite, reversed-index x order, adjacent overlap,
outline endpoint, cap retention, and shown/hidden car placement are headless-
tested. All 14 Release suites pass. The refreshed artifact matches the tested
build, imports only standard Windows/UCRT DLLs, and contains no ASCII or UTF-16
reference to the original executable, supplied network path, or extraction
tree. Neither game executable nor any audio path was launched.

The same static ordering audit found and corrected a separate Find mismatch at
`10a8:00a8/09e7`. Waiting-person focus used a stable x sort, but the executable
reuses the renderer's diminishing-gap Shell sequence. Earlier gap passes can
indirectly reverse equal-x shafts even though each direct comparison swaps only
strictly descending values. Native Find now preserves that sequence exactly;
a four-shaft `x=[200,100,100,100]` regression proves the original index order
`[2,1,3,0]` and the resulting queue focus. The full Release suite remains
14/14 passing without launching a game executable or audio path.

The 28-caller Microsoft-runtime RNG audit also corrected New/Open behavior.
`1000:3a2f` advances one DS:0bd4/0bd6 process-global state, initialized to one;
the adjacent seed writer at `1000:3a18` has no inbound call or relocation.
Because the seed is absent from TDT files, replacing the native document must
carry the live state rather than silently restore one. New, Open's pre-I/O
empty state, successful Open, and its fresh-tower error fallback now all retain
the same sequence. Pure transition assertions and all 14 Release suites pass.

The complete `1050:0000` Command-window dispatch audit recovered all eleven
message-table entries and corrected the shared top-level palette adapter.
Command, Info, and Map now repeat their original `WM_ACTIVATEAPP` prefix:
when activating, each raises and shows Main with Win16 `SetWindowPos` flag word
`0x53` before forwarding the state to `1078:01e8`. The audit also removed a
native-only View-menu normalization. `1050:024e`, `1120:0111`, and
`1168:0112` clear/hide a palette without changing its menu mark, and the
`1158:0886/08a6/08c4` View commands likewise toggle only visibility. Pure
branch assertions and the complete 14-suite Release run pass. The refreshed
artifact matches the tested build, retains only the standard Windows/UCRT
imports above, and contains none of the forbidden original-executable, network,
or extraction-tree strings in ASCII or UTF-16LE. No PE or audio path was
launched.

The follow-up auxiliary-window audit recovered the literal ten-entry
`1120:01ed` Info table and thirteen-entry `1168:028a` Map table. Their native
class procedures are now distinct through synchronous creation rather than
inferring identity from global HWNDs that do not yet exist. Map explicitly
owns its creation no-op and consumes every `WM_MOUSEMOVE` exactly as
`1168:013a` does. Its drag latch is cleared only by `1168:0156` button-up;
the former native-only `WM_CAPTURECHANGED` reset was removed because that
message is absent from the Win16 table. Exact table arrays and every move/up/
capture branch are headless-tested, and all 14 Release suites pass. The
refreshed PE matches the tested build, retains the standard Windows/UCRT-only
imports, passes the forbidden ASCII/UTF-16LE scan, and was not launched.

The complete `1158:0000` Main-window audit now retains its literal 22-entry
message table and `1158:06b9`'s literal 27-entry command table as testable
native data. `WM_SETCURSOR` and `WM_CAPTURECHANGED` are no longer consumed by
native-only branches; both pass to `DefWindowProc` because neither appears in
the executable's table. `WM_CREATE` performs only the original positional Fast
Mode check and release-menu deletion, while Sound check/gray state remains at
`1128:0ae7-0b95` after initial scrollbar setup and the resource's visible
Windows items remain deliberately unchecked. Main activation no longer adds
three unoriginal auxiliary invalidations, and both scroll messages preserve
their leading invalidation even for unsupported scroll codes. The command
tail now distinguishes the table-owned DIALOG/3000..3002 entries, forwards
all non-table IDs to `DefWindowProc` after the generic 3000..4001 process call,
and preserves File/Exit's destroy-without-DS:3258-clear path. The adjacent
`1058:00d9`/`10a0:0544` comparison also restores the Finger tool's unconditional
capture and DS:02a6 scheduler gate for otherwise-unhandled presses, including
empty space, plus its immediate cursor restoration on release. All 14 Release
suites pass and the PE was not launched.

The latest shared-font audit replaces native per-paint font creation with the
recovered process-global lifecycle at `1208:0a8d/0ba7/0b6a`. Startup now
enumerates screen fonts for an exact Arial face and falls back to MS Sans Serif,
seeds the original nine-pixel/default-precision entry, clamps smaller requests,
reuses heights linearly, creates later entries with `OUT_TT_ONLY_PRECIS`, and
honors the original early return once all ten slots are published—even for an
existing height. All dialog and Info-window callers borrow this shared bank,
and final teardown deletes its handles in slot order before cursor/audio/palette
shutdown. Pure assertions cover clamping, lookup, insertion, the full-bank
rule, and both creation specifications. The Release suite passes 14/14; the
tested build and refreshed artifact hashes match, and neither executable nor
an audio path was launched.

The adjacent static WinG utility audit now directly covers `1208:07d5/09cf`,
`1208:049d/069a`, `1248:0000`, and `1208:0603/063a`. The native transport is
proved equivalent for top-down 8-bit allocation, DWORD row padding, all 256
B,G,R,0 palette entries, BLACKNESS initialization, equal-size opaque clipping
with source-origin advancement, and asymmetric 16/32-bit byte swaps. Every one
of the 242 embedded BITMAP resources satisfies `049d`'s fixed 40-byte-header +
1024-byte-palette layout and resolves pixels at byte 1064. The audit also
corrected a traceability error: Person portrait scaling belongs to the direct
`1100:364a/37a9/37d1` WinGStretchBlt path, not `1208:069a`. All 14 headless
suites pass; no executable or audio path was launched.

The latest shared-DTMP audit corrects `1070:0005`'s initialization ordering.
Positive bitmap references now resize from the DIB before the helper acquires a
dialog DC and do not realize the palette in that branch. Empty/negative bitmap
references instead select and realize the logical palette, add `TA_UPDATECP`,
release the DC, and only then apply a nonzero header size. Every native DTMP
caller uses the corrected boundary, with headless assertions for both branches
and the zero-width case. The full Release suite passes 14/14 without launching
either executable or any audio path.

The latest Find-filter audit corrects a visible native-only frame mismatch.
`10d8:00ec-0146/0323-0360` passes positive DTMP/510 during both initialization
and `WM_PAINT`, which realizes the palette, paints BITMAP/510, and replays the
child-control layout without entering the generic negative-resource
bevel/chrome path. Native now follows that split and retains the recovered
12-pixel font only during initialization. Both phases have headless regression
coverage; the full Release suite passes 14/14 without launching either
executable or any audio path.

The latest static utility pass corrected Facility Information advisory text
from the native `(left+2, top+3)` approximation to the exact
`11e0:0049`/`1100:1760-176c` base `(left+8, top+18)`, retaining
`1108:08e4`'s 16-pixel line step. Pure geometry assertions cover all three
advisory rows. The release build and tested artifact hashes match; static ASCII
and UTF-16 scans find no original executable name, supplied network path, or
extraction path. No GUI executable or audio path was launched.

The latest `1100:4869` Facility preview comparison corrects two source-crop
mismatches: all Metro/Cathedral components now begin 12 pixels below their top
floor and use the original 60/168-pixel heights, and Restaurant/Retail/Fast
Food widths now come from the original type-width table instead of serialized
record spans. Component-specific headless assertions cover both corrections.
The `1200:0196` scheduler is also guarded by a complete 36-signature native
dispatch contract; a full-cycle test takes every conditional callback branch,
and unknown, malformed, or unhandled callbacks fail rather than disappearing.
The full Release suite passes 14/14, and neither executable nor any audio path
was launched.

The latest top-level palette audit corrects the shared Command/Info/Map
WndProc lifecycle at `1050:0000`, `1120:0000`, `1168:0000`, and
`1078:00c6`. `WM_NCACTIVATE` now performs the original synchronous
eight-pixel-only frame paint instead of scheduling a full content rebuild;
active non-modal `WM_ACTIVATE` validates the pending client update; Command,
Info, and Map retain their distinct full-region invalidation plus
visibility/New-Open/closing paint gates; and destroying any palette posts the
original quit message. Pure branch assertions and a memory-DC active/inactive
pixel test pass without creating a visible window. The complete Release suite
passes 14/14, the build/artifact hashes match, and no GUI or audio path was
launched.

The latest dialog-filter comparison restores the exact shared presentation
behavior of PEPLEINFODLOGFILTER `1100:0116`, TENANTINFODLOGFILTER
`1100:085b`, ELVINFODLOGFILTER `1100:0f10`, and ESCINFODLOGFILTER
`1100:1248`: 13-pixel Arial selection, palette realization, palette-matched
RGB(204) static backgrounds, cleared class cursors plus selected Arrow,
nested-modal activation redirection, and the original parent TOPMOST recovery
after Person/Rename drill-down. The Facility handler's former reversed
activation target is corrected, and transport panels accept only ID 1 for
closing. The pure activation/control-color plans and the complete Information
suite pass headlessly. No GUI executable or audio path was launched.

The follow-up custom-dialog pass restores NEWORLOADDLOGFILTER
`1018:0067`'s distinct 11-pixel paint/13-pixel control fonts, cleared class
cursor, explicit initialization show, and palette-realized immediate surface;
NAMEPEPLE/NAMETENANT `1100:3a39/3dc4`'s shared palette-transparent immediate
and WM_PAINT phases; and MOVIETITLEDIALOGFILTER `1100:4138`'s 13-pixel,
palette-realized immediate/paint boundary. COUNTDLOGMAIN `1060:02a4-037e`
now presents the custom Finance button pressed and then released before a
Return/Space close. The related style/key plans and full 14-test suite pass
headlessly. No GUI executable or audio path was launched.

The latest static paint-path audit restores the original separation between
`1080:0a1e`, `1090:03ab`, and `1158:00da/0a3c`. Native now retains a persistent
DS:3264-equivalent world raster. Ordinary `WM_PAINT` only presents it and can
no longer advance sky/facility RNG state or consume Elevator transfer visuals.
The full simulation pass advances visible facility people once in its recovered
post-transport position without rerunning sky; explicit `1080:0a1e(1)` scroll,
resize, and camera rebuilds advance sky first, while argument-zero rebuilds do
not. Preview and direct-palette passes remain mutation-free, and one-frame
transfer pixels are cleared only after being retained in the backing cache.
All six pass plans are covered headlessly. No GUI executable was launched.

The adjacent full-frame window fan-out is now recovered as well. Native no
longer invalidates Map and the entire Info palette whenever World changes—a
substitution that made BITMAP/352 animate faster than the original. Following
`1090:061f-06dc`, dirty World pixels are presented directly and invalidate
Command only; Info content receives an unconditional direct-DC pass every full
frame, while Map remains on `1090:046f-047c`'s independent sixteen-tick
cadence. Exposure `1158:00da` now skips its backing blit while iconic, and the
`1080:0b26` floor label is drawn only by explicit `1080:0a1e` rebuild passes.
Both dirty/clean fan-out branches and every surface-pass branch are headless-
tested. No GUI executable was launched.

The save-transfer audit now covers the full `10d0:1518` compatibility path
against `10d0:0b3a`. The native disk writer no longer preserves an imported
old revision or opposite byte order: like the executable, every Save emits
revision `0x24` little-endian after migrating all opaque runtime word/dword
banks. This includes pre-`0x22` segmented elevator cars and gaps,
pre-`0x19`/pre-`0x22` defaults, widened floor indexes, expanded link/final
blocks, the cf88/dbfc no-swap omissions, the tenant-subtype zeroing bug, and
the express-elevator unmapped-floor swap. An independent asymmetric
opposite-endian stream, a revision-`0x18` migration fixture, and an actual
on-disk revision-`0x17` Save upgrade pass headlessly. No game or audio process
was launched.

The loader now applies the same compatibility transform to its live document
immediately, rather than retaining foreign raw records and relying on scattered
endian-aware reads. Direct byte consumers for people, retail, Elevator graphs,
floor/car payloads, routes, and dynamic banks now see the exact post-`1518`
runtime bytes before any Save occurs. The independent foreign fixture asserts
those complete structures against their current little-endian counterparts;
the untouched imported byte stream remains available only through the explicit
lossless serializer.

This refresh includes the exact construction-hover outline and command-button
press/release pixels, `1198:00d9` parking population ceilings (including the
original stale-category bug), `1138:00a5/0128` neighbor-noise boundary order,
the complete `10b0:0000` New/Open transient reset chain, and
`11f8:0955-098c`'s hidden initial-balance transaction. It also restores
`1080:09c3`'s every-sixteen-tick animated Map repaint, `10c0:002e`'s
wrapping Stair/Escalator animation-dirty scan, and the synchronous
`1118:0143` Info balance repaint after `1178:01db/027c/0697` construction
debits. This refresh additionally restores `1058:06df`'s Map-drag
`EQUALRECT` no-op (including preservation of non-map-aligned scroll
positions) and `1148:02c8`'s persisted Elevator car-count gate before the raw
record scan. It also corrects Facility Information's transport-distance scan
to read the tenant's serialized left-x field, limits the secondary portrait
cursor zone to the original dialog groups 9-11, and restores the original
runtime patch that forces event dialogs from their embedded 10-point font to
8 points. The final game-owned static pass maps the original palette/resource
ownership, command-selector, route-scratch, low-level masked/opaque blitter,
startup, and shutdown boundaries to their native value/RAII equivalents. The
PE was not launched during verification.

This build also fixes the latest disassembly-to-host audit findings. The
integrated `1200:0196` boundary now derives its scheduled audio gate from the
revision-aware `b406` field, writes the document clock, clears `b3e4` at frame
zero, and resets the Hotel checkout counter before frame-`0x04b0` callees. The
Hotel checkout presentation latch is consumed at `1090:0696`, including the
original WAVE/10013 cadence and balance repaint. The empty-queue host once
again calls `1020:00cb` independently at `1258:023a`, the per-frame host keeps
all four calls at `1090:0448/0465/0615/06dc`, and `1090:0452` now makes the
original mutually exclusive emergency-Security/normal-people selection. The
New/Open/Exit confirmation gate is the original active-document plus floor-10
occupancy predicate, not a dirty-since-save approximation.

This refresh also restores `1128:03ad-0535`'s exact startup Sound-profile
probe and read sequence, including the `BeepOnly == 1` branch, WAVMIX master
gate, category defaults, and matching menu check/gray state. The supplied
installation defaults are embedded as the one-file release fallback. The
native host now also replays `1078:0000(1)` on every non-iconic `WM_ACTIVATE`
and reproduces `1078:01e8`'s auxiliary-palette demotion/restoration ordering
across `WM_ACTIVATEAPP`. The PE was not launched during this verification.

The `1258:000b` message-loop route is now recovered as well. A modeless
Elevator Control takes precedence over DS:31a4's active modal, accelerators are
offered to that selected target before dialog navigation, and Main receives
accelerators only when neither specialized target exists. The empty-queue path
does not insert a native scheduler yield. The preceding startup path now also
honors a nonempty raw WinMain TDT target with `1258`/`1128`'s backslash-only
path derivation and dialog bypass. Both startup and menu Open use
`10d0:0225/062a`'s reset-before-I/O transaction and fresh-tower fallback after
mapped load errors. Pure routing/path branches are headless-tested; no GUI was
opened.

The following startup/file audit restores `1128:01d9-0223`'s Arrow and
synchronous final Command publication plus `1058:033c`'s shared construction
toggle. Toolbar clicks, Find-marker expiry, and the startup guard now use the
same Map-exit versus ordinary branch, including command mode two, Find reset,
WAVMIX deactivate/activate, preview-scratch restoration, and disabled-mode
Main presentation. Open and Save As demote/restore the palette windows around
each common-dialog attempt, Save As retries invalid DOS basenames, and a
post-create save-transfer error deletes its partial target. These state/branch
plans pass headlessly; no window or audio backend was opened.

The command-palette host now also follows `1050:0219/02b3` exactly: edit and
facility points activate on mouse-down, grouped selectors capture and consume
the held click's release, and the build toggle alone waits for mouse-up after
showing its pressed resource frame. This path is covered by a pure headless
mouse-phase plan test.

The main-window lifecycle now follows `1158:049e-04fb`, `10d0:0604`, and the
post-loop `1258:016e -> 10b8:0000/0039` chain. `WM_DESTROY` only posts quit;
audio is no longer shut down prematurely there. Close clears the native Main
handle only after synchronous destruction, repeated session-end queries return
FALSE once the closing latch is already set, and final teardown destroys Map,
Info, then Command before custom cursors, audio, and the palette. The branch
matrix and auxiliary order are headless-tested; no GUI or audio playback ran.

The auxiliary-window audit additionally restores `1058:01d6`'s rating-one
Map toolbar gate: the disabled fourth cell is painted but inert until rating
two. The complete `1050:0063`, `1120:005e`, and `1168:006a/0156` activation
branches now publish the shared DS:31a6 word and redirect only to DS:31a4's
active modal—not Main. Map inserts behind that modal, Command also restores
modal focus, and the no-modal Command path preserves its prior-latch-dependent
TOPMOST repair. Native modal entry now publishes and nests the actual dialog
HWND for that boundary. Map also releases capture on every button-up. These
decisions have headless branch tests.

The main-client input audit also restores `1158:028c/029f`'s DS:0242 press
latch. Only `WM_LBUTTONDOWN` with MK_LBUTTON begins an edit/construction
interaction; `WM_LBUTTONDBLCLK` no longer starts a duplicate action after the
normal intervening button-up. The exact press-phase decision is headless-
tested.

The completed adjacent dispatch audit now includes `1158:00da/028c/029f/02d5`.
DS:0244 is active for the complete BeginPaint/render/EndPaint interval and
suppresses all three main button messages; DS:0242 is armed before valid-down
dispatch, required by a valid release, and cleared only after that release
returns. The richer headless phase plan also retains the original abnormal
armed-double-click route. Native modal calls are now enclosed by a nested
process-wide counterpart to DS:24b8, preserving `1158:0050/015b/01e7/028c/
0314/046f/0492`'s suppression of hit testing, scrolling, pointer input,
commands, and system keys while a dialog is active.

The neighboring cursor/idle-host audit restores the rest of `1158:0314-032a`
and `1258:0195-0505`. Every main mouse move performs the live
screen-point cursor resolution before the modal gate, including enabled Map/
Info/Command Arrow precedence, non-iconic main-client mode selection, and the
outside-client no-op. Before simulation work, every empty-queue pass now
reconciles the DS:0252 audio latch from main activation/iconic state, stopping
live channels after ordinary deactivation instead of waiting for minimize. It
also restores enabled Command TOPMOST only when Elevator Control is absent;
an existing control is promoted TOPMOST/active with main or inserted behind
main while inactive. The host then samples construction-preview coordinates
itself, suppresses scheduler/world work while construction is off or DS:02a6's
Elevator-Finger capture is active, and uses `1090:03ab(0)`'s preview-only
path whenever the six-coarse-tick (nominal 96-ms) scheduler does not advance.
Native now
mirrors DS:77ac and performs the recovered `EQUALRECT` comparison only after
the outline rectangle is rederived; it no longer prefilters on raw mouse
coordinates and therefore catches stationary-cursor tool, mode, and viewport
changes. Mouse messages do not invent preview invalidations or a
`WM_MOUSELEAVE` state, preview motion cannot rerun the full paint-time
RNG/person path, and all exits retain `11c8:0135(0)`'s deliberate force-zero
no-op tail. Every completed full frame also performs `1258:0285-02c9`'s
synchronous initialized Elevator Control refresh without substituting a native
elevator-changed gate.

The main-scroll audit now also preserves `1158:015b/01e7`'s exact 16-pixel
line steps, client-minus-16 page steps, clamp boundaries, and raw message
position for both thumb tracking and thumb release. The Win32 host consumes
`HIWORD(wParam)` for those two messages instead of substituting
`SCROLLINFO.nTrackPos`; pure headless tests cover all six accepted codes and
the invalid-code boundary.

The adjacent resize audit restores `1158:041c`'s DS:02a4 initialization gate,
so creation-time `WM_SIZE` cannot reveal the Map/Info/Command palettes before
the original startup sequence finishes. `1158:05ef`, `1080:00d7`, and
`1128:08d6` now also use the original vertical-then-horizontal range/position
order and saved-position clamp. Native `SCROLLINFO.nPage` remains zero with
`nMax=world-client`, preserving the original fixed Win16 scrollbar thumb
instead of the prior proportional Win32 thumb. These branches are covered by
pure startup-disposition and resize-state tests.

The main-window geometry audit also restores `1128:02aa`'s desktop-derived
maximum client bounds before the 816x576 caps, `1128:08d6`'s x=204/y=53
startup rectangle and one-pixel extent convention, and `1158:0334-0415`'s
exact minimum/maximum tracking formulas. The old host matched only desktops
large enough to hit both caps; smaller desktops now receive the original
startup height and maximum tracking size. Pure tests cover both branches.

The palette-message audit now restores `1020:0e29/0f4f`'s actual 256-entry
logical palette and its exact flag bands, animates reserved entries 188..218,
and reproduces `1158:04fe/0508/0c29` plus the three auxiliary procedures.
Main `WM_QUERYNEWPALETTE` realizes but returns zero, self/startup/closing
notifications are suppressed, and a changed realization synchronously
repaints Map, Info, Command, then Main. Headless tests cover every plan and
entry boundary. No GUI executable or audio path was launched during this
verification.

The remaining modal-selector branch is now exact as well. The recovered
seven-entry table at `1050:095c/096a` maps only `WM_PALETTECHANGED` to
`1050:090a`; the native selector returns TRUE for self notifications and, for
non-self messages, realizes the shared palette and conditionally calls
`UpdateColors` on the same live DC. It no longer substitutes an unconditional
invalidation or enters the main-window palette fan-out. The decision is
headless-tested.

The deeper `1158:0c29` audit also removed a native side effect that the earlier
ordered invalidation plan did not catch. The original directly paints Map,
Command content, and Main through reused/acquired DCs and uses `UpdateWindow`
only for Info. The native fan-out now matches those four mechanisms, their
client-invalidation differences, and DC realization/release ownership. In
particular, direct Main palette re-presentation no longer re-enters ordinary
WM_PAINT's sky/facility presentation step, advances RNG, or clears one-frame
Elevator transfer visuals. Pure action-plan tests cover Main-origin and
Map-origin ownership branches.

The static `1208:05e6` audit also established that the game's shared clock is
a signed arithmetic `GetTickCount() >> 4` value, not raw milliseconds. The
tested artifact now applies that clock to the 180-tick startup splash,
15-tick effects gate, 6-tick simulation gate, greater-than-300-tick Find/Info
expiry, and 600-tick saturated-audio recovery. This verification remained
headless; neither the executable nor any audio backend was opened.

The complete shared world-input audit now covers every branch of
`1058:0000`. Native publishes the Control/Shift mirrors before the armed,
Elevator-Control-isolation, and Bomb/Fire gates; preserves the emergency
WAVE/7002 down-only branch; and routes all four mouse phases through the exact
Bulldozer, Finger, Magnifier/Find-tail, and enabled-construction paths. Finger
now retains `10a0:0544`'s empty-space capture, move-time cap acquisition, and
double-click cleanup. The connected `10a0:0201` comparison also corrected a
mode-zero precedence mismatch: an in-span no-car hit on a hidden shaft
(`word_3c == 0`) returns zero and falls through to Stair/Escalator and facility
demolition, while a visible shaft consumes it. Pure routing/action-table
assertions and the full 14-suite Release run pass. The refreshed artifact
matches the tested build, keeps only Windows/UCRT imports, and passes the
forbidden ASCII/UTF-16LE runtime-reference scan. No game executable, emulator,
windowed program, or audio backend was launched.

The connected construction-input audit now covers every message and completion
branch of `11f8:07d8`. Only Floor, Parking, Lobby, and Parking Ramp admit
captured move/up processing; down resets the shared successful-step counter,
captures before the hidden initial-balance bonus and Shift replacement, and
snapshots balance only for Floor/Lobby. Parking/Ramp helper anchors remain
deferred until their constructor is actually reached. Double-click preserves
the down-time placement fields and skips down-only setup. On release,
Floor/Lobby require both a successful helper step and a changed balance and
play their early WAVE/7000 before releasing capture; Parking/Ramp require only
a successful step and play no success wave. Zero effective count plays
WAVE/7002. The common success tail now runs rating/treasure completion once per
captured command, honors the literal five-type general-success-sound exclusion
table, and rebuilds transport routes only for the six literal command types
`1,22,24,27,42,43`. Pure phase/release/table assertions and all 14 Release
suites pass. The packaged PE is byte-identical to the tested build at the hash
above, has only Windows/UCRT imports, and passes the forbidden ASCII/UTF-16LE
scan. No executable, emulator, window, media player, or audio backend was
launched.

The adjacent `11f8:240d/25a2` helper audit corrects the continuous Parking and
Parking Ramp transaction itself. Both helpers construct toward the pointer
interval retained from the preceding call and publish the current LPARAM only
at their tail, producing the original one-message lag. Parking advances in
four-cell units; Ramp advances floors in the recovered high/low loop order and
passes the current call's c6 horizontal snap to every attempted floor instead
of pinning the initial x. State bounds advance after failed attempts too. When
one message attempts multiple units, only the final `17fd` return controls the
single 24cc increment, one construction sound, and auto-scroll call; earlier
successful units still mutate the tower but cannot make a final failure count
as a successful helper call. The first accepted helper result consumes
DS:24ca's direct priority-five sound latch, later results use `11c8:0100`'s
reserved-channel-if-idle path, Parking scrolls horizontally, Ramp vertically,
and both re-snap after any resulting viewport change. Down-time versus current
double-click snaps are also preserved. The exact run/completion plans are
headless-tested, all 14 Release suites pass, and the package at the refreshed
hash above passes the same import and forbidden-runtime-reference scans without
being launched.

The connected `11f8:26dd/284d/17fd` audit now reproduces the Floor/Lobby
helper's final-result and non-transactional boundaries. Initial b3e6 Lobby
height is published before validation; every selected ground-Lobby story is
attempted in order; earlier successful mutations and charges survive a later
failure; and only the final story controls 24cc and horizontal auto-scroll.
Returning toward the press anchor preserves split outer Lobby records instead
of atomically recoalescing them. A nonzero-cost overlapping `284d` call alone
requests WAVE/7001; charged empty/disjoint `17fd` calls remain silent, and an
earlier sound request/24ca consumption survives a final failure. Native dirty
and invalidation state now retains those partial commits. These cases and the
zero-cost-success completion boundary are headless-tested. All 14 Release
suites pass; `release-native\SimTower.exe` is byte-identical to the tested
build at the refreshed hash above, contains only the listed Windows/UCRT
imports, and passes the forbidden ASCII/UTF-16LE scan. No game executable,
emulator, windowed program, media player, or audio backend was launched.

The static rename-filter audit found and removed an interaction-only host
substitution. `NAMEPEPLEDIALOGFILTER` at `1100:3a39` and
`NAMETENANTDIALOGFILTER` at `1100:3dc4` never select the installed name during
initialization: both return TRUE without setting focus, then focus edit item 4
only after their first and subsequent paint passes. Native Person/Tenant Rename
no longer sends `EM_SETSEL(0,-1)` or returns FALSE from initialization. The
two-phase focus/selection plan is headless-tested and the complete 14-suite
Release run passes without launching a GUI or audio backend.

`FINDDIALOGFILTER` at `10d8:006f` also no longer substitutes a forced list
focus. Its recovered `00a1-014b` initialization path performs no `SetFocus` and
returns TRUE; the native dialog now leaves initial focus to the dialog manager
instead of focusing item 5 and returning FALSE. A pure Find initialization
plan covers that branch.

The subsequent complete `1100:0f10` ELVINFODLOGFILTER and `1100:1248`
ESCINFODLOGFILTER message-table audit removed two more host substitutions.
Neither recovered initialization branch calls `SetFocus`; both return TRUE.
Both recovered click branches consume empty-panel clicks and unconditionally
restore the information window as TOPMOST and DS:31a4 after the portrait helper.
Elevator clicks call `RealizePalette` directly, whereas Stair/Escalator clicks
first select the logical palette. The native boundary now follows those exact
branches, with pure exact-address initialization/click plans and the existing
portrait-hit tests. The refreshed Release build passes 14/14 headless suites,
matches the packaged artifact byte-for-byte, and no executable or audio backend
was launched.

The complete neighboring PEPLEINFODLOGFILTER `1100:0116` and
TENANTINFODLOGFILTER `1100:085b` audits then corrected the same initial-focus
substitution in Person and Facility Information. The Person filter now performs
its recovered SetCapture at initialization and ReleaseCapture on ID-1 close,
with DS:31a4 cleared/restored around Rename. The Facility filter now handles
empty clicks through its palette, portrait-dispatch, DS:31a4, and TOPMOST tail,
and its `0b11-0d57` command plan consumes every command. IDs 1/7 ignore the
notification, while ID 13 accepts legacy notification zero or one for rent
groups 0..5 and Movie group 10. Exact-address tests cover both complete filters;
the refreshed one-file artifact matches the 14/14-tested Release build.

The adjoining command-filter audit now covers the complete four-message
tables for `1100:3a39` NAMEPEPLEDIALOGFILTER,
`1100:3dc4` NAMETENANTDIALOGFILTER, `1100:4138`
MOVIETITLEDIALOGFILTER, and `10d8:006f` FINDDIALOGFILTER. Rename controls
1..4 and Movie controls 1..3 ignore notification words exactly; Rename Cancel
shares EndDialog(1), Movie New/Classic always return one, and Find Remove/Go-To
ignore notifications while list notifications 1/2/3 retain their distinct
enable/resolve/disable routes. All four filters omit the host-added WM_CLOSE
path and preserve their recovered DS:31a4 cleanup order. Exact command plans
are headless-tested.

The remaining modal procedure audit decodes and pins ABOUTDLGPROC's literal
seven messages (`1010:0973`), ELVPOPUP's five (`1098:27a5`), ELVDLOGMAIN's
eight (`1098:12c9`), and CMDBTNSUBWNDPROC's seven (`1050:095c`). About now
uses its palette-matched RGB(230) control brush, accepts every WM_TIMER ID,
posts WM_PAINT and returns FALSE, and no longer substitutes WM_ERASEBKGND or
WM_CLOSE. Elevator Popup, Elevator Control, and the command selector likewise
have no WM_CLOSE branch. Selector button-down retains the original distinct
no-explicit-unclip/release tail, button-up performs full pointer cleanup, and
outside mouse motion closes through FALSE. Direct table assertions and all 14
Release suites pass.

Finally, the four-entry NEWORLOADDLOGFILTER (`1018:01fd`), six-entry
COUNTDLOGMAIN (`1060:0461`), and five-entry AHOTTADLOGFILTER (`1068:0421`)
tables are complete. Startup consumes every WM_COMMAND while closing only IDs
1..3. Finance WM_PAINT, WM_KEYUP, WM_LBUTTONDOWN, and WM_LBUTTONUP all perform
their recovered work through the common FALSE return; its keyboard and mouse
close paths clear DS:31a4 in the original order. AHOTTA now maps every split
Win32 control-color subtype and kills the actual WM_TIMER identifier. The
packaged PE is byte-identical to the tested build at the hash above, retains
only Windows/UCRT imports, and contains no ASCII or UTF-16LE occurrence of the
original executable name, supplied UNC source, or extraction-tree path. No
game executable, emulator, windowed program, media player, or audio backend
was launched.

The adjacent `1010:014c/0304` startup-title filter audit now distinguishes the
literal three-message modal and modeless sets. SETUPSTARTUPDLGA dismisses on
left-button-down with result zero, while SETUPSTARTUPDLGB handles WM_DESTROY
instead. Both paint paths select and realize the shared logical palette before
the black fill and centered/clamped DIB presentation. Direct message/result
assertions pass with all 14 Release suites. The artifact at the hash above is
the byte-identical tested build and passes the same import, forbidden-string,
and zero-running-process checks without execution.

ELVDLOGMAIN's recovered close tail at `1098:0ece-0f63` is now preserved as a
testable native plan and in the Win32 host. An active isolated Elevator is
resumed first; the published control HWND is cleared before destruction;
Map, Command, Info, and Main are re-enabled in that exact order; and
DestroyWindow is last. The native-only WM_DESTROY/WM_NCDESTROY cleanup and
SetActiveWindow(Main) substitution are gone because neither destroy message nor
explicit activation exists in the recovered filter/tail. The exact close plan
passes with the complete 14-suite Release run.

The modal ownership follow-up also removes synthetic destroy-message branches
from New/Load, AHOTTA/transport information, Person/Rename Person,
Facility/Rename Tenant/Movie Choice, and Find. The executable uses the shared
process-global gray brush at `DS:31ae`; the Win32 translation uses per-call
brushes, which are now released only after the native modal loop returns. The
one exception remains Rename Person's recovered `1100:3ce8-3d0f` command tail,
which explicitly deletes its brush before clearing DS:31a4, releasing capture,
and ending the dialog. This preserves the literal filter message sets without
leaking host-owned GDI objects. The refreshed 10,312,949-byte PE is
byte-identical to the 14/14-tested build, has SHA-256
`C1F96991C296CFF2EB3F1C829AC009AA03968829DF58E2CC01D8110298F5604C`, imports
only the Windows/UCRT DLLs listed above, and contains none of the forbidden
runtime-reference strings in ASCII or UTF-16LE. It was not executed.

The `1000:0000 -> 1258:000b -> 1128:0005` entry-path audit now pins the
complete `1128:01d9-0223` visible startup tail. The recovered class strings at
`1128:05ba/05c8/05da` authoritatively identify DS:325a, DS:325c, and DS:325e
as Command, Info, and Map. Startup consequently shows Map, then Info, performs
`1050:03aa`'s command-surface acquisition, and only then shows Command and
Main. Arrow selection follows; `1058:033c` remains conditional on DS:783e;
and `1080:05a1` synchronously presents Command without the former native-only
forced Main paint. The same identity check corrected ELVDLOGMAIN's close
restoration labels to Map, Command, Info, Main. Exact headless plans cover both
sequences, all 14 Release suites pass, and coverage records 1,396 native-source
address citations plus 450 test citations; 559 mapped starts lack a direct test
citation. The packaged PE at the refreshed hash is byte-identical to the tested
build and passes the same import and forbidden-string scans without execution.

The adjoining class/window-construction audit now preserves
`1258:0345` and `1128:05eb-09ea` at the native host boundary. Command, Info,
and Map are created in that order from their literal rectangles. Command's
`SETWINDOWPOS` word is `0x000a`, so it enters the topmost band and deliberately
replaces its border-inflated creation size with a raw 63x100 outer extent;
Info and Map use `0x000b` and retain their inflated client extents. The three
top-level palettes receive IDs 1000, 1001, and 1002, and all four application
windows perform the recovered hidden-window palette realization, nine-pixel
font selection, `TA_UPDATECP`, and transparent-background DC sequence before
their first paint. Command's class also retains the recovered empty (non-NULL)
menu-name pointer. Border-metric-independent pure assertions and all 14
Release suites pass. The refreshed 10,312,949-byte PE is byte-identical to the
tested build at SHA-256
`C1F96991C296CFF2EB3F1C829AC009AA03968829DF58E2CC01D8110298F5604C`; it was
not executed.

The complete `1128:1139-1306` startup-capability audit restores the pre-window
host gates that the prior native preflight skipped. It captures the system
font ascent, executes the memory check, then preserves the independent
8-bpp, `RC_BITBLT`, `RC_DI_BITMAP`, `RC_DIBTODEV`, and `RC_STRETCHBLT`
consent prompts in order. TrueType availability and enabled state retain their
distinct unconditional-abort messages; the device-zero wave probe retains its
system-modal Yes/No path and disables sound only after accepted failure. An
absent legacy `[Extensions] tdt` profile value is written as the native module
filename followed by the recovered ` ^.tdt` placeholder. Exact ordered issue
arrays and all recovered message text are headless-tested. All 14 Release
suites pass, and `1128:1139` now has a direct test citation. The packaged PE
at the current hash above is byte-identical to the tested build and was not
executed.

The complete `1128:03ad-0542` startup-profile audit now carries the recovered
`[Paths] Save` field through its actual consumers. Both `10d0:0122` Open and
`10d0:03f1` Save As pass the same non-null `DS:3120` buffer as
`OPENFILENAME.lpstrInitialDir`; native previously left that member null. The
single-file build embeds the supplied installation's
`C:\Maxis\Simtower` value alongside its existing shipped Sound defaults, while
either original INI search location still overrides it and an explicitly
missing/empty key retains an empty buffer. The fixed capacity remains 0x80,
all 14 headless Release suites pass, and the packaged PE at the current hash
above is byte-identical to the tested build without having been launched.

The enclosing `1128:003a-00ce` ordering audit restores the temporary
fresh-tower bootstrap that occurs after BITMAP/256 is presented but before
WAVE/20000 and DIALOG/124. `10d0:086c/0ac2` leaves DS:31ca inactive, holds
construction off through DS:31c4, and applies frame `0x09e5` to the shared
logical palette; that frame selects CLUT/1002 rather than the base CLUT the
native dialog previously retained. The native bootstrap now updates the live
palette without publishing a document. It also reproduces Wait selection
before first splash creation and the repeated class-cursor-clear/Wait sequence
after each splash phase. Palette inequality and the exact startup frame are
headless-tested; all 14 suites pass and the executable was not launched.

The adjoining Sound/construction audit restores the only game-side consumer of
`DS:de28` after startup. At `11f8:0e21-0e67`, every successful construction
command clears its status, emits `MESSAGEBEEP(-1)` when `BeepOnly == 1`, and
then applies the five-type exclusion before requesting WAVE/7000. Native had
parsed BeepOnly but discarded the latch, making that mode silent. The latch is
now retained and both single-click and captured drag commands reach the shared
audio tail exactly once. Pure branch tests cover beep/wave independence; all 14
Release suites pass without invoking `MessageBeep`, `waveOut`, or a game GUI.

The remainder of `10d0:0ac2` now also restores command 40008's derived state.
At `0b03-0b31`, the original enables the fire-crew menu item only when b406 bit
three is set and b418 is zero; it grays the item for every other reconstructed
state. Native previously updated this menu only during live fire transitions,
so New/Open or the pre-dialog temporary tower could leave stale resource/default
state—most visibly when loading a save during an active fire. The exact
predicate now runs after New, successful Open, and startup bootstrap. Three-way
headless tests cover no fire, fire/no crew, and fire/crew; all 14 suites pass.

The adjoining `1140:010d` caller audit corrects an interaction-visible rating
promotion error. `10d0:0ac2` supplies zero, but both ordinary `1140:002d` and
rating-six `1140:00a8` promotions supply one; the nonzero argument writes
DS:783c=2 before `1080:05a1` synchronously presents Command. Native previously
preserved the active construction tool and deferred that paint. The translated
refresh now selects the new rating table, resizes and presents Command in the
recovered order, conditionally restores `11f8:3b94`'s pending outline outside
Elevator Control isolation, and invalidates the native equivalent of
`1038:0000`'s visible-row scratch. The surrounding New/Open path now keeps the
pre-I/O active command-table handle, restores/clamps the viewport before
synchronously composing Map, then rebuilds Main; New preserves the prior view
because `10d0:086c` does not clear b3f0/b3f2. Four exact branch assertions cover
the recovered split. All 14 headless suites pass.

The subsequent `11f8:0fea` construction-dispatch audit separates its two
successful elevator exits. Adding a car to an existing shaft commits and jumps
from `122f` directly to `1446`; only a newly allocated shaft reaches `140d`,
writes DS:783c=1, and synchronously calls `1080:05a1`. Native previously forced
Finger mode after both outcomes and merely invalidated Command. The result now
publishes whether a new shaft was created, the host applies the Finger/tool
refresh only for that result, and the command surface is presented before
return. Elevator mutation also publishes persistent document change even when
a supplied cost is zero, preventing stale Main/Map surfaces. Direct recovered-
start assertions raised coverage to 449 unique test citations and reduced mapped
starts without a direct test citation to 560 at that audit point. All 14
headless suites passed.

The complete `1160:01dc` Map-compositor audit now has a direct recovered-start
regression. Static resource inspection proves BITMAP/313 and /314 are 200x20
and BITMAP/315 is 81x20. The executable forms
`RECT(0,0,biWidth,biHeight)` and applies `OffsetRect(200-right,18)`, so the first
two legends begin at x=0 and the third at x=119. The three underlying opaque
copies also confirm that each upper-row destination pixel samples
`source x + signed_phase` modulo 200 while the lower 24 rows remain fixed.
Corner, boundary, and immediately-outside pixels are now checked against the
original DIB/CLUT bytes. All 14 headless suites pass; coverage is now 1,396
unique native-source citations and 450 test citations, with 559 mapped starts
without a direct test citation. The refreshed 10,312,949-byte package is
byte-identical to the tested build at SHA-256
`C1F96991C296CFF2EB3F1C829AC009AA03968829DF58E2CC01D8110298F5604C`, retains
only the Windows/UCRT imports above, and passes the forbidden ASCII/UTF-16LE
runtime-reference scan. It was not executed and no audio backend was invoked.

The subsequent `1220:0000` nightly person-family audit found a signedness
mismatch in both tenant-dependent reset branches. At `01ba-021b` the original
uses signed `JGE` against status byte `0x18`; native had compared its modeled
`uint8_t` directly. Hotel and Condo owners with transient high-bit status
`0x80..0xff` consequently received state `0x20` instead of the executable's
`0x10`. Both branches now bit-cast to signed eight-bit before comparison. The
direct recovered-start test exercises every live jump-table family, both
short/full clearing tails, both sides of the threshold, and high-bit Hotel and
Condo states. All 14 headless suites pass. Coverage is 1,398 source citations
and 451 test citations, leaving 558 mapped starts without a direct test. The
refreshed 10,312,949-byte package is byte-identical to the tested build at
SHA-256 `087BE58B1BF1778DE2F29FD151C4398791F4E4D7FC6C171FEA8F5978263A3C29` and
passes the same import and forbidden-string checks without execution.

The shared WAVMIX service at `11e0:0e84` is now translated at every native
boundary corresponding to original `1208:05e6`, including startup, palette,
scheduler, Find/Info, simulation-clock, and direct audio paths. It preserves
the executable's unsigned `last + 0x30 > now` gate, advances the rolling anchor
by exactly one 48-ms interval on each due call rather than snapping to `now`,
and retains tick-count wraparound. The original drains callback message
`0x03bd` and calls `WaveMixPump` only when any of DS:de2a/de2c/de2e is enabled;
native waveOut posts no such message, so the due pass performs the equivalent
`WHDR_DONE` channel reap only when Elevator, Events, or Background sound is
enabled. Direct recovered-start tests cover early, exact-boundary, disabled,
late catch-up, and wraparound cases. All 14 headless suites pass. Coverage is
now 1,399 source citations and 452 test citations, leaving 557 mapped starts
without a direct test. The refreshed 10,314,101-byte package is byte-identical
to the tested build at SHA-256
`32B41FA495E4D0CEEAC016F213038E72D62AFF0371E3A17D20B1DBB2B2E87863`, imports
only the Windows/UCRT DLLs above, and contains none of the scanned original
executable, source-share, or extracted-resource runtime references. It was not
executed and no audio backend was invoked.

The direct `1050:0000` Command-window audit now covers all eleven parallel-
table messages and its default fallthrough set. It found two omissions in the
native adapter. First, `1050:010d-0138` selects and realizes DS:795e before the
visibility, New/Open, and closing gates, whereas native had coupled palette
realization to content drawing. Command WM_PAINT now retains that unconditional
palette side effect while Info and Map keep their distinct gated behavior.
Second, every non-close `WM_LBUTTONDOWN` reaches `1050:02a4-02ad` after its
toggle/activation path, invokes `1208:05e6` (and therefore the shared WAVMIX
pump), and stores the returned coarse tick in DS:31b0/31b2. Native now mirrors
that shared timestamp and call boundary; close-box and button-up paths remain
excluded. The full 14-suite headless run passes. Coverage is now 1,401 source
citations and 453 test citations, leaving 556 mapped starts without a direct
test. The refreshed 10,314,101-byte package is byte-identical to the tested
build at SHA-256
`806BC475B9ADFB70E02D0D0665CDACD1C07A4126A9FAC5F4D60F6F5850403AF7`, retains
only the Windows/UCRT imports above, and passes the forbidden ASCII/UTF-16LE
runtime-reference scan. It was not executed.

The direct `1168:0000/02be` Map-window audit now covers the literal thirteen-
message table (including WM_COMMAND's explicit DefWindowProc target) and the
complete painter/drag sequencing. A drawable Map paint contains four direct
`11e0:0e84` checkpoints: before palette selection, after the first focus/frame
pass, before the backing blit, and after the final focus outline. The adjacent
drawable Info painter retains its single checkpoint between SelectPalette and
RealizePalette. Native had omitted all of these direct pump calls. Successful
`1058:0284` Map drags now likewise retain their three pump checkpoints and the
original visible order: XOR-erase old focus, commit/clamp both scroll words,
XOR-draw new focus, then synchronously present Main. Native previously rebuilt
Main first and only invalidated Map. The adapter also mirrors DS:0248 from each
non-close button-down through every button-up alongside DS:3216 capture state.
All 14 headless suites pass. Coverage is now 1,409 source citations and 455
test citations, leaving 554 mapped starts without a direct test. The refreshed
10,315,235-byte package is byte-identical to the tested build at SHA-256
`AE3A5A4B099D2834E5B0FAF57DF04209B812187DB46C490E43C608FC84ED8242`, retains
only the Windows/UCRT imports above, and passes the forbidden runtime-reference
scan. It was not executed and no audio device was opened.

The direct `1100:4869` Facility Information preview-crop audit removes two
native-only fallbacks. Ordinary source rectangles now retain an exact
zero-width span instead of being forced to eight pixels. Restaurant, Retail,
and Fast Food previews now always replace that serialized span with the
linked source type's DS:74ba width, including a zero table entry. This also
separates `1100:4514`'s minimum temporary-backing dimensions from `4869`'s
unclamped crop rectangle. Both malformed-boundary cases have direct headless
regressions; all 14 suites pass. Coverage is now 1,409 source citations and
456 test citations, leaving 553 mapped starts without a direct test. The
refreshed 10,315,235-byte artifact is byte-identical to the tested build at
SHA-256
`0BFE5CED1BDEB24B1C57545C939D60C181BDB8B681931E2D23E58006C35CCB41`, retains
only the Windows/UCRT imports above, and passes the forbidden ASCII/UTF-16LE
runtime-reference scan. It was not executed and no audio path was opened.

The direct `1158:06b9` Main command-dispatch audit now pins the literal
27-entry parallel table and the non-table fallback boundary. Commands
3000..4001 that are not table entries run `1068:0000` and then still reach
DefWindowProc; all other unknown IDs go directly to DefWindowProc. Hidden
command 9003 now follows `1158:0946 -> 0ba8`: it acquires a Main DC and blits
the retained backing directly instead of manufacturing an invalidation and
routing through WM_PAINT. All 14 headless suites pass. Coverage is now 1,410
source citations and 457 test citations, leaving 552 mapped starts without a
direct test. The refreshed 10,315,450-byte artifact is byte-identical to the
tested build at SHA-256
`218B93FFAB1CCB1DAE3E9E9B1E197B3E52BBD62C96BD50FF8B7ABA7DDA46EBE4`, retains
only Windows/UCRT imports, and passes the forbidden runtime-reference and
zero-task-process checks. It was not executed and no audio path was opened.

The connected `1100:03ac/4439 -> 11c8:03fb/06b6/0426` Facility Information
lifecycle audit restores the original render/sound/modal order. Native now
renders and retains one preview snapshot before selecting the clicked
facility's sound and entering the modal; subsequent dialog paints stretch that
snapshot instead of rerendering live world state. The missing direct facility-
sound request is wired through the existing native audio boundary. The same
comparison corrected commercial ambient lookup: `11c8:07d2` multiplies the
tenant link by 18 and reads DS:b7e2's Retail record, not the unrelated 16-byte
people table previously used by native. Headless tests use contradictory
people/Retail bytes to pin the data source and cover master-sound and RNG
boundaries. All 14 suites pass. Coverage is now 1,411 source citations and 459
test citations, leaving 551 mapped starts without a direct test. The refreshed
10,316,324-byte artifact is byte-identical to the tested build at SHA-256
`5080810614710FA45E907830F5AAE012C5585796EC9888837847AAC8532F4062`, retains
only Windows/UCRT imports, and passes the forbidden runtime-reference and
zero-task-process checks. It was not executed and no audio device was opened.

The direct `1100:4514` audit now separates all four temporary-backing words
from `1100:4869`'s crop. Native derives the current visible cell/floor counts
with the recovered grid units, raises Movie types 18/19/34/35 to 31 cells,
raises types 18..21 and 29..35 to two floors, and raises Cathedral types 36..40
to five floors. Other facilities expand horizontally only when their signed
right-minus-left span exceeds the current count; malformed wrapped spans do
not expand. DS:7782/7784 mirrors are retained as twice the cell count and the
floor count. The render-once Facility Information snapshot now uses this
expanded backing and stretches only `4869`'s source crop. All 14 headless
suites pass. Coverage is 1,411 source citations and 460 test citations, leaving
550 mapped starts without a direct test. The refreshed 10,317,051-byte
artifact is byte-identical to the tested build at SHA-256
`292EA2E8B6FC8117661C5F2B73BB37AACBD128CECD5C93D63FAD2DD35AB89245`, retains
only Windows/UCRT imports, and passes the forbidden runtime-reference and
zero-task-process scans. It was not executed and no audio device was opened.

The direct `10e0:0042` Find-person dispatcher audit corrected two visible
focus targets. Movie/Party state-zero resolution through `10e0:0bc6` now uses
the linked dc24 record's signed byte-7 center rule: 15 cells for a paired
record and 12 for a single-sided record, rather than deriving the center from
the destination tenant's type. Housekeeping state two now follows person byte
6/word 12 to the assigned room, gates on Hotel type 3..5, obtains that room's
first guest through `1220:6ba9`'s serialized tenant dword +8, and focuses the
room only while that guest is in state 3; native previously inspected and
focused the Housekeeping owner. Contradictory target-type and owner/room
regressions pin both distinctions. All 14 headless Release suites pass.
Coverage is now 1,413 source citations and 462 test citations, leaving 548
mapped starts without direct test evidence. The refreshed 10,317,622-byte PE
is byte-identical to the tested build at SHA-256
`3D9BC2EEDB6490C1644208D2DA73E09AAE872F0810BA601CFDB13E415044EA54`, imports
only Windows/UCRT DLLs, and passes the forbidden runtime-reference and
zero-task-process scans. It was not executed and no audio device was opened.

The next static batch directly audited `10f8:0c06`, `1220:16ab`, and named
dialog procedure `1018:0067`. The fire-responder implementation already
matched `0c06`; new tests distinguish its all-matches left-edge sweep from
the no-current-fire branch that moves to the first active floor in the
responder's six-floor partition. The complete signed type range -1..40 now
pins `16ab`'s family jump table, including its deliberate lack of a Security
entry and the separate `1210:0883` Elevator-car callback. The New/Load dialog
comparison found and corrected one input-routing mismatch: unknown
WM_COMMAND IDs now return FALSE through `1018:01ee`; only IDs 1..3 are
consumed and close the modal with their own ID. All 14 headless Release suites
pass. Coverage is now 1,414 source citations and 465 test citations, leaving
545 mapped starts without direct evidence. The current 10,317,622-byte PE is
byte-identical to the tested build at SHA-256
`DDE6CDB05FEE38FDB2593BB378A72FA7B3843A216F837B01F8B039023F4C78F6`, imports
only Windows/UCRT DLLs, and passes the forbidden runtime-reference and
zero-task-process scans. It was not executed and no audio device was opened.

Direct static comparison of `1220:6383` now pins every Housekeeping state and
its `1150:0000/01f5/03f3` helpers: modulo-six room ownership, two-pass search,
route result tables, Stair completion release, exact three-callback countdown,
Hotel room/guest mutations, the `b3de >= 1500` same-floor rejection, and
`1220:6297` scheduling gates. That implementation already matched. Auditing
`11a8:07d3` then exposed a genuine accounting mismatch. When Fast Food is
activated after frame `0x00f0` during day phase zero, the original initializes
service byte 8 to ten and immediately calls `1060:07f7(type, 10)`; native code
had initialized the record but omitted the population transaction. Activation
now adds ten to finance category five and total population before returning.
The related `11a8:1596/17eb/1812` group, lane, and opening behavior is directly
tested. All 14 headless Release suites pass. Coverage is now 1,415 source
citations and 476 test citations, leaving 536 mapped starts without direct
evidence. The current 10,317,622-byte PE is byte-identical to the tested build
at SHA-256
`9F6871522B3F9216B02775ED3B498B84CCD008469F4532E05F38147F7351588D`. It was
not executed and no audio device was opened.

The following static batch completed the Metro `1220:5227` audit and exposed
two additional control/presentation mismatches. `10f8:033d` now preserves the
original Bomb-first event priority: with combined Bomb+Fire flags and SECOM,
only responder zero is activated and responders one through five remain idle.
The full ten-message `1120:0000` Info-window comparison then found that the
shared Command/Info/Map activation adapter validated the entire client. The
three Win16 procedures overwrite only `RECT.bottom` with eight at
`1050:008d`, `1120:007d`, and `1168:009f`, so native now validates just the
title strip and leaves pending content paints intact. Direct audits also cover
`10c0:0983`'s split-diagonal Stair/Escalator collision geometry,
`1090:06fb`'s Elevator car state pass, `1130:06e9` tenant satisfaction,
`1220:2068` Office scheduling, `1208:002c` point publication,
`1208:0cb5`'s immutable-resource replacement, and a pixel-level
`1118:045d` Info date-field render. All 14 headless suites pass. Coverage is
now 1,416 source citations and 495 direct test citations, leaving 517 mapped
starts without direct evidence. The refreshed 10,317,622-byte artifact is
byte-identical to the tested build at SHA-256
`8A2910A0EA889BB818806D2F96B72B6B683BD322ED8767F0AB4AC8D353D3F1B2`,
imports only standard Windows/UCRT DLLs, and contains no ASCII or UTF-16
original-executable, supplied-network-path, extraction-tree, or DOSBox
reference. It was not executed and no audio device was opened.

The next disassembly batch found and repaired four behavioral mismatches.
Condo `1220:38e1` now dispatches states 4/10/20/21/22 through their exact phase,
ordinal, tenant-byte, RNG, and strict-frame gates; Hotel `1220:2e92` restores the
unconditional early phases of state 10. The Lobby discount shared through
`11d8:0423` now interprets wrapped elapsed time as signed in the ordinary and
`1210:1332` isolated Elevator paths, and `10a8:12c1` uses the same signed value
for visible wait-color selection. The `b3ae` preview renderer no longer treats
live projected metrics as person IDs: identity is read from the saved Elevator
snapshot while the signed metric remains live. Direct tests also cover
`1020:053e`, `1220:6037`, `11a8:02f2`, `1220:067c`, `1208:0004/0cb5/0d75`,
and `11e0:04c0/05d7/06d9`. All 14 headless Release suites pass. Coverage is now
1,421 source citations and 511 direct test citations, leaving 503 mapped starts
without direct evidence. The refreshed 10,319,355-byte artifact is
byte-identical to the tested build at SHA-256
`A0AC84299E8BDEF0676FDD4EB79155819779720F465C842BE98CD47BFF77C14B`, imports
only standard Windows/UCRT DLLs, and contains no ASCII or UTF-16 original-EXE,
supplied-network-path, extraction-tree, or DOSBox reference. It was not executed
and no audio device was opened.

The latest disassembly batch repaired `1090:0192` Elevator-car reconstruction.
Native now writes the duplicate home-floor byte 13, selects floor-mode byte 14
from schedule indices 28..41 (`0x20 + calendar*7 + day phase`), and preserves
the preexisting active byte 15 for the original -1 reset argument. Added cars
snapshot the current mode and activate; newly created shafts continue to mark
only their first car active. Sentinel regressions cover new, loaded, added, and
removed-shaft paths. Direct evidence also now names and exercises
`11a0:0eaf`, `11b0:11af`, `1130:03f4`, `1180:0352`, `1098:1ff5`,
`11a0:027c`, `10a8:0de6`, and every `1130:0cec` record-span variant. All 14
headless Release suites pass. Coverage is 911 exact native mappings, 1,421
source citations, and 520 direct test citations, leaving 494 mapped starts
without direct evidence. The refreshed 10,319,867-byte x86-64 Windows GUI PE
is byte-identical to the tested build at SHA-256
`6E84F57896ED0FAAC358BD32EE37A2C946EDF8C7EFB126767A73A313AE7AF469`.
Its import table contains only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and
UCRT API-set DLLs; ASCII and UTF-16 scans find no original-EXE,
supplied-network-path, extraction-tree, or DOSBox reference. No game,
emulator, or media process remains. The artifact was not executed and no audio
device was opened.

The current static batch directly compares `1118:0143`, `10a0:0544`,
`1180:090a`, `11b0:0fa5`, and `11f8:2f5a`. It adds pixel/latch coverage for the
Info balance field, restores the Elevator Finger double-click miss path's
retained press latch, and anchors the complete entertainment-finish and route-
selection branches. Static decoding of `2f5a`'s literal key/target words found
and corrected two observable construction gates: type 19 receives ground-floor
status 12, and upper paired types 18/20/29 receive status 14 at `b3e8-1`
instead of extending the basement. All 14 headless Release suites pass.
Coverage is now 911 exact native mappings, 1,422 source citations, and 526
direct test citations, leaving 488 mapped starts without direct evidence. The
packaged `release-native/SimTower.exe` is a 10,319,867-byte x86-64 Windows GUI
PE, byte-identical to the tested build at SHA-256
`DAD2934E9D269E246B6AC10237E001CEB4BB38306DF570A79F138E63B6F085CA`. Its
import table contains only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT
API-set DLLs. ASCII and UTF-16LE scans find no DOSBox, original-EXE,
supplied-network-path, extraction-tree, or extraction reference. The artifact
was not executed and no audio device was opened.

The follow-on Info audit directly compares `1128:13fc`, `1118:026a`, and
`1118:073d`. BITMAP/320 and the retained/clipped clock compositor already
matched. Pixel isolation exposed one visible status-text error: after selecting
`TA_UPDATECP | TA_BASELINE`, native did not reproduce `026a`'s
`MoveTo(field.left+2, field.bottom)` before DrawText and could inherit the
balance painter's current position. The MoveTo is restored in exact order;
tests constrain text to the 262x11 gray field, preserve its white lower edge,
and separately prove that changing only b3de changes pixels only inside the
31x31 clock face. All 14 headless Release suites pass. Coverage is 911 exact
native mappings, 1,423 source citations, and 530 direct test citations, leaving
484 mapped starts without direct evidence. The refreshed 10,319,867-byte PE is
byte-identical to the tested build at SHA-256
`1C391E62DF501D64570FC37FA940125896F71DC8DA95EBDB1BDB615A5C11E337`, imports
only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs, and passes
the forbidden ASCII/UTF-16LE runtime-reference and zero-task-process scans. It
was not executed and no audio device was opened.

The latest file-dialog audit directly compares the complete `10d0:03f1` Save
As path and its `1000:11e8/1394/1408` runtime helpers. The executable searches
the complete selected string for its last dot when replacing or appending
`.TDT`, then validates the DOS basename with the complete string's first dot
and last backslash. Native previously used path-aware extension and stem
operations, which diverged for multi-dot names and directories containing a
dot. Native now preserves the original whole-string behavior, including
`C:\dir.with.dot\Tower` becoming `C:\dir.with.TDT`, first-dot eight-character
validation, and the signed-negative dotted-directory edge case. Direct tests
cover ordinary, multi-dot, dotted-directory, accepted, and rejected names.
All 14 headless Release suites pass. Coverage is 911 exact native mappings,
1,425 source citations, and 531 direct test citations, leaving 483 mapped
starts without direct evidence. The packaged 10,320,279-byte x86-64 Windows
GUI PE is byte-identical to the tested build at SHA-256
`53B2CA2BBCD55C4434D9DF1A8A7DC99B189B43AA0CEC58D3F7F94DEB40E94CBE`. It
imports only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs;
ASCII and UTF-16LE scans find no DOSBox, original-EXE, supplied-network-path,
or extraction-tree reference. It was not executed and no audio device was
opened.

The following static batch directly compares the observable helpers at
`1118:0044/0368`, `1200:0037/058d`, `1098:1644`, `1100:0644`, `10a0:1625`,
and `11c8:0135/02c0`. No translation divergence was found. Added tests isolate
the rating and population painters at pixel level, exercise every clock phase
and hand quadrant, verify inverted Elevator Control cells, all 24 rent-choice
literals, complete waiting-ring order and service-Elevator exception, and the
fixed channel-zero/channel-one stop pass. All 14 headless Release suites pass.
Coverage is 911 exact native mappings, 1,425 source citations, and 540 direct
test citations, leaving 474 mapped starts without direct evidence. The
packaged 10,320,927-byte x86-64 Windows GUI PE is byte-identical to the tested
build at SHA-256
`622A65BA8C1BEF9A4FEC45CA5674ACE3FEF6E23035421E51CA909B225BE80C6B`. It
imports only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs;
ASCII and UTF-16LE scans find no DOSBox, original-EXE, supplied-network-path,
or extraction-tree reference. It was not executed and no audio device was
opened.

The current quiet static batch directly compares `1220:49fa` and `1220:049a`.
The pending-facility owned-person clear already matched: activation covers the
exact table-selected span and produces the same 16-byte records after the
following common initializer. A sentinel regression now proves that every
dynamic byte in all three Condo records is cleared before the activated values
are written. The Fast Food/Restaurant normal-pass wrapper exposed one shared
signed-clock follow-on error: after converting `1200:0543` to signed
`CWD`/`IDIV`, native admitted negative phases to Fast Food's `< 4` random gate,
while `1220:4b72-4b7e` explicitly requires phase `>= 0 && < 4`. The missing
lower bound is restored, with a high-bit frame regression proving no dispatch
and no RNG consumption. All 14 headless Release suites pass. Coverage is 911
exact mappings, 1,433 source citations, and 560 direct test citations, leaving
457 mapped starts without direct evidence. The refreshed 10,321,951-byte
x86-64 Windows GUI PE is byte-identical to the tested build at SHA-256
`57CAE31292266B284518AB2C5000757BC7A547F78465BB4F425AE4E08ED41067`, imports
only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs, and passes
the forbidden ASCII/UTF-16LE runtime-reference scan. It was not executed and
no audio device was opened.

The next quiet static batch directly compares `1058:0000`, `1098:1895`,
`11f8:20e7`, `11f8:1452`, `1258:0345`, and `11a0:047c/088f`. The complete
world-input priority matrix, Metro and vertical-transport constructors, and
both opaque facility-cell compositors already matched and now have direct
headless anchors. Elevator Control's served/unserved, above-top, basement, and
three-digit floor-label decisions are now shared with a pure tested row plan.
The four original startup WNDCLASS records are likewise table-driven; this
restores Main's registered `TOWER_MENU` metadata while retaining the parsed
original menu supplied at window creation and Command's non-null empty menu
name. All 14 headless Release suites pass. Coverage is 911 exact mappings,
1,433 source citations, and 566 direct test citations, leaving 450 mapped
starts without direct evidence. The refreshed 10,323,114-byte x86-64 Windows
GUI PE is byte-identical to the tested build at SHA-256
`D674974C0F671800ECF211C95A1346A61438B30FB858325A52EB33AB85ECAFAD`, imports
only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs, and passes
the forbidden ASCII/UTF-16LE runtime-reference and zero-process scans. It was
not executed and no audio device was opened.

The newest quiet static checkpoint expands direct evidence across exterior and
world helpers, facility/parking/transport state, fresh initialization, clock
and star rendering, people accounting and routing, Find, information
thresholds, disabled-audio behavior, annual/periodic transitions, command and
Elevator Control plans, resource startup, rename/save policy, ALRT preparation,
and Arial enumeration. All 14 headless suites pass. Coverage is 911 exact
native mappings, 1,449 source citations, and 979 direct test citations. Of the
911 mapped routine starts, 869 now have direct test evidence and 42 host/runtime
boundaries do not; the other 264 recovered candidates are the classified
compiler/runtime support set rather than unmapped game-owned routines. The
refreshed 10,338,821-byte x86-64 Windows GUI PE is byte-identical to the tested
build at SHA-256
`F4DD08CFEFDD55E40AC80D630B1C8ED99744B8015AED4105C78F13256F6B6AAE`. It imports
only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs. ASCII and
UTF-16LE scans find no DOSBox, Mesen, original-EXE, supplied-network-path, or
extraction-tree reference. No game executable, emulator, or media player was
launched and no audio device was opened. Side-by-side runtime conformance
remains an explicit release gate.

The following quiet host-boundary checkpoint makes the exact `10c8:01f7`
Bomb and `10e8:025a` Fire execution orders production-consumed, including
their intentionally inverse sound/damage pair and conditional Security, focus,
menu, modal, and deferred-completion tails. It also shares and directly tests
`11f8:0793`'s complete point-hit-demolition wrapper. Further direct evidence
covers `1118:08f3`, `1178:0697`, `11c8:09d2`, `10d0:0604`, `10f0:0121`,
`11a0:134c`, and `1208:0b6a`. All 14 headless suites pass. Coverage is 911
exact native mappings, 1,449 source citations, and 991 direct test citations;
879 mapped starts have direct evidence and the remaining 32 platform
boundaries are individually classified in `HEADLESS_BOUNDARY_AUDIT.md`. The
refreshed 10,339,643-byte x86-64 Windows GUI PE is byte-identical to the tested
build at SHA-256
`6CA9465496879E58DD44382E6B06F06EB30570FBA862193DD967C25589DC4D12`. It imports
only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs. ASCII and
UTF-16LE scans find no DOSBox, Mesen, original-EXE, supplied-network-path, or
extraction-tree reference. No game executable, emulator, or media player was
launched and no audio device was opened. Runtime side-by-side conformance
remains the release gate.

The launcher-lifecycle audit fixes the remaining proven differences in this
static batch. About now follows `1010:049e` by stopping both channels once,
directly deactivating the mixer backend without clearing the `11c8` active
latch, running the modal dialog, and directly reactivating the backend.
New/Load releases its native DTMP value before `EndDialog`, matching
`1018:01d3`/`1070:051f`, and splash destruction clears its retained native
state. All 14 headless suites pass. Coverage is 911 exact native mappings,
1,452 source citations, and 994 direct test citations; 882 mapped starts have
direct evidence and 29 outer platform boundaries remain classified in
`HEADLESS_BOUNDARY_AUDIT.md`. The packaged 10,339,389-byte x86-64 Windows GUI
PE is byte-identical to the tested build at SHA-256
`9EBB0962CD9E595E6F7A172869CED6EB11100BE4BC40735A22D47B2513B10217`. It imports
only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs. ASCII and
UTF-16LE scans find no DOSBox, Mesen, original-EXE, supplied-network-path, or
extraction-tree reference. No game executable, emulator, or media player was
launched and no audio device was opened. Runtime side-by-side conformance
remains the release gate.

The latest static presentation checkpoint corrects two native host-order
shortcuts. `1058:05f8` now performs synchronous Main reconstruction followed
only by `1080:055d`'s direct Map-focus XOR update. `1038:002f` and its
construction/demolition/Elevator consumers now dirty Main without inventing
Info or Map invalidations; finance helpers retain their immediate direct Info
paint, and Map remains on `1080:09c3`'s sixteen-tick cadence. Bomb ransom,
fresh-tower bonus, and buried treasure now preserve their recovered
sound/dialog/finance presentation order. The startup splash lifecycle was
re-audited and already matched. All 14 headless suites pass. Coverage is 911
exact native mappings, 1,452 source citations, and 993 direct test citations;
881 mapped starts have direct evidence and 30 platform boundaries remain
classified in `HEADLESS_BOUNDARY_AUDIT.md`. The packaged 10,339,229-byte
x86-64 Windows GUI PE is byte-identical to the tested build at SHA-256
`9B70620BB389CA650B0527E3D4DD20A16B154F45AD5106559F4F6DB8D0ECDDAC`. It imports
only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs. ASCII and
UTF-16LE scans find no DOSBox, Mesen, original-EXE, supplied-network-path, or
extraction-tree reference. No game executable, emulator, or media player was
launched and no audio device was opened. Runtime side-by-side conformance
remains the release gate.

The latest quiet release refresh incorporates the final static launcher,
dialog-text, startup, teardown, and native storage-ownership audit. Both Rename
dialogs now use Main as in the original, Movie Choice returns the true modal
result, Elevator Control receives the initiating pointer and has no invented
post-create presentation, and modal DTMP state is released before `EndDialog`.
The `0xfe` shared text-read cap, popup conversion order, Find selection query,
failed-save deletion gate, startup precompute order, wrapping POINT subtraction,
and complete process teardown groups are production-consumed and directly
tested. All 14 headless Release suites pass. Coverage is 911 exact mappings,
1,452 source citations, and 1,019 direct test citations; 907 starts have direct
evidence and the remaining four real process/audio/diagnostic boundaries are
classified in `HEADLESS_BOUNDARY_AUDIT.md`. The packaged 10,341,922-byte x86-64
Windows GUI PE is byte-identical to the tested build at SHA-256
`4EC0ADB9F5774A5D256DB400B77FECA915BA9AFE892A617A33943E69BA9B061D`. Imports
are limited to COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs.
ASCII and UTF-16LE scans find no DOSBox, Mesen, original-EXE,
supplied-network-path, or extraction-tree reference. No game executable,
emulator, or media player was launched and no audio device was opened. Runtime
side-by-side conformance remains the required release gate.

The latest headless correction resolves the reported construction gaps at the
recovered resource and state boundaries. `11f8:033a` maps type-zero Floor to
BITMAP/1000..1003, while BITMAP/3944..3949 belongs to type 46 fire;
`1038:06ad -> 11a0:0000` repeats the Floor record's fixed status cell across
the whole span. Fresh and bulldozed Floor records therefore render from source
cell two, and the existing legal demolition-to-rebuild path is visible again.
Type-47 disaster damage now renders its exact bottom-aligned BITMAP/4008 rubble
cells. An integration regression demolishes a facility to type-zero/status-two,
rebuilds Floor on the same cells, and then places an Office there. Startup also
retains the recovered Main owner in the pure model while using the live splash
as the Win32 modal owner, so the splash cannot be clicked above and disable the
New Tower/Load Tower chooser. All 14 Release suites pass. The packaged
10,359,510-byte x86-64 Windows GUI PE is byte-identical to the tested build at
SHA-256
`628D8933CD25C8E3A76425C9F10A5BCF57D862AC3DD9BF9245FF216BAEC52701`.
It imports only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set
DLLs. ASCII and UTF-16LE forbidden-runtime-reference scans and the exact-name
process sweep are clear. The artifact was not launched, no emulator was
launched, and no audio device was opened for this correction.

The complete `1038:00a9/050e` tenant-cell and `06a8` frame-selector audit
removes five additional self-confirming renderer assumptions. Type-45 Metro
boundary spans now paint the full four-cell BITMAP/3880 bank with the original
absolute-world phase. Pending construction and type-47 damage instead begin at
tenant-relative cell zero, as the ordinary `00a9` counter requires. Office
uses the signed status byte and full signed variant word; Security always uses
frame zero; and Party Hall types 29/30 use their linked dc24 state with the
recovered state-three-or-later clamp to frame two. Tests deliberately set
contradictory tenant bytes and non-aligned positions so these rules cannot be
validated by restating the implementation assumption. All 14 Release suites
pass. The packaged 10,362,070-byte x86-64 Windows GUI PE is byte-identical to
the tested build at SHA-256
`1DFB73A2D19F83D76CF348E722C6D8C4C7AF136B6E871CA4F26C074BC72F4B79`.
Its imports remain limited to COMDLG32, GDI32, KERNEL32, USER32, WINMM, and
UCRT API-set DLLs. Both forbidden-reference scans and the exact-name process
sweep are clear. No executable, emulator, visible window, or audio path was
launched for this audit.

The subsequent construction-input audit restores `11f8:3df4-3e2e`'s exact
vertical transform. Lobby type 24 alone retains the unshifted 36-pixel snap;
Floor, Office, every ordinary facility, Elevator, and vertical-transport tool
adds twelve pixels after that signed snap. Native had incorrectly limited the
offset to multi-story preview rectangles, placing all one-story construction
outlines twelve pixels too high. The same correction is applied to placement
coordinates, where captured negative points expose Win16 IDIV's
truncation-toward-zero result. Direct preview-border and negative-coordinate
placement tests cover both sides of the Lobby/non-Lobby branch. All 14 Release
suites pass. The refreshed 10,362,070-byte standalone PE matches the tested
build at SHA-256
`5575DA071AEFB549E4EA2BDF4DC87AA454CA3FC163DD9086DA5DCAAC475B1866`.
It retains only Windows/UCRT imports and was not launched.

The adjacent edit-hit audit directly joins `11f8:3da4` placement to
`11f8:3d2d/3e3e` tenant selection and `11f8:0793/35ac` demolition. One fixed
client point now has an end-to-end regression that constructs and activates an
Office, hits and bulldozes that exact Office, verifies the resulting merged
type-zero/status-two Floor interval, and immediately reconstructs another
Office in it. This proves the reported rebuild interaction through the public
client-coordinate boundaries rather than only by calling the model builders
with precomputed cells. The full 14-suite Release run passes; the standalone
PE remains 10,362,070 bytes with SHA-256
`5575DA071AEFB549E4EA2BDF4DC87AA454CA3FC163DD9086DA5DCAAC475B1866`.
No game, emulator, visible window, or audio path was launched.

The scheduler timing audit restores the second `1208:05e6` coarse-clock read
at `1200:0529`. The original commits its six-tick gate baseline only after all
scheduled far calls—including modal event boundaries—return; native had stored
the entry sample before dispatch, allowing the first post-modal frame to run
immediately after a long callback. The host now commits a fresh clock sample
between the scheduled-call loop and the separate `1090:03ab` full-frame pass.
Tests cover a multi-tick callback interval, the recovered signed CMP/JL
deadline-overflow edge, and `1200:04b3`'s wrapping 32-bit day increment. All 14
Release suites pass. The packaged 10,362,166-byte PE is byte-identical to the
tested build at SHA-256
`3CD22AE1609D1C147B2CA51F20A20247B9E609B8D11BA688924E653FA927F7C5`.
Its imports and forbidden-reference/process scans are clear; nothing graphical
or audible was launched.

The screenshot-driven exterior audit corrected a visible masked-blit mismatch
without launching either executable. Every ordinary cap, ground foundation
end, and BITMAP/1002 roof-marker call in `11c0:0000` supplies palette index
zero to `1208:071f`; its `1248:0000 -> 1250:0024` path therefore skips those
source bytes. Native had instead overwritten the destination, exposing the
white index-zero rectangle around the roof crane and white holes in the edge
art. The shared exterior fragment path now preserves the destination for
index zero, with pixel tests for standard caps, both layered ground ends, the
roof marker, and transfer-person visibility through transparent holes. A
second integration regression covers the reported adjacent rebuild shape:
demolishing one Office coalesces its type-zero/status-two plot with Floor on
both sides, and rebuilding at the former coordinate splits that run while
leaving the neighboring Office unchanged. All 14 Release suites pass. The
packaged 10,362,166-byte PE is byte-identical to the tested build at SHA-256
`91A2F4A5F8BDEF7630EDFD8118284EEE5DEB61C0D9546FF8261F264AE572BF2F`.
Its imports remain limited to COMDLG32, GDI32, KERNEL32, USER32, WINMM, and
UCRT API-set DLLs; both forbidden-reference scans and the exact-name process
sweep are clear. No executable, emulator, visible window, or audio path was
launched.

The final `1090:03ab` audit then corrected the complete-frame order rather
than only its final image. Elevator work now follows the original per-shaft
nesting: all eight car-state slots, all eight passenger pairs, then that used
shaft's `11e0:0e84` checkpoint. Movement sound and person-family host work run
at their recovered in-loop call sites instead of after the whole frame.
`11f8:3b94/3c13`'s two restore and three redraw checkpoints retain the original
DS:025c scratch-latch transitions in both preview-only and complete passes.
The dynamic renderer's `056c..05e5` checkpoints now precede construction
outline redraw and `060d` palette work; palette-only RGB rematerialization is
kept separate from the original DS:31cc dirty gate. Main, Command, Info,
transient-status, final-palette, and final-audio operations follow the literal
`061f..06ed` sequence, while changed preview rectangles use the synchronous
owned-DC path and never queue a Main paint. All 14 optimized Release suites
pass. The packaged 10,369,638-byte x86-64 Windows GUI PE is byte-identical to
the tested build at SHA-256
`23E52DDBE182E9CD9A9D3C4D4210B254E54D6AFCC5216EDCAEE95EF48E4ACA0B`.
Imports remain limited to COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT
API-set DLLs; both forbidden-reference scans and the exact-name process sweep
are clear. The executable and audio paths were not launched.
