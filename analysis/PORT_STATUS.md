# Port and reverse-engineering status

## Completion status

The native function translation and standalone Windows release candidate are
complete at the recovered-code/resource boundary. On 2026-08-27, explicit
runtime permission was used for a host-muted side-by-side check of the supplied
Win16 executable and the native build. The original and native startup title,
New Tower desktop, main caption, and Open-dialog flows were captured and
compared. That pass removed the remaining immediately visible host-shell
substitutions: the native build now uses a centered two-button Win16 main
caption, unthemed classic menu/scrollbar rendering, and a compact 470x247
legacy Open/Save dialog with the original labels. The prior WebView/TypeScript
executable remains a rejected approximation and is not counted toward this
port.

Audio certification is complete without making speaker noise. PCM parsing,
arbitration, categories, repeat/preemption, initialization, completion, and
shutdown are translated and tested; a manual hardware smoke also ran the
production `waveOutOpen`/prepare/write/stop/close path with WAVE/20000's exact
format and 50,300-byte length while substituting digital silence only for that
smoke's sample values. The ordinary default path still submits the exact
embedded original samples.

The final loaded-save timing comparison corrected the last observed scheduler
host mismatch. Fast Mode still takes the recovered `1200:0196` bypass branch,
but the native non-preemptive host pass is capped at the original reference's
observed 58-ms cadence instead of running at the speed of a modern
`PeekMessage` busy loop. Native start and 90-second captures made on a separate
Windows desktop both retain the source save's `1st WD/1Q/1st Year` date.

## Binary and resource audit

| Area | Verified result | Status |
|---|---|---|
| Disk image | Raw MBR/FAT16 layout identified; 428 files extracted without modifying the supplied image | Complete |
| Main executable | Windows 3.1 NE; 80 segments, 483 resources, 21 named exports | Complete inventory |
| Code/data layout | 78 code segments (309,486 bytes), 2 data segments, 7,285 relocations | Complete inventory |
| External APIs | 191 unique imported APIs resolved against the supplied Win16 DLLs | Complete inventory |
| Recursive control flow | 1,175 classified routines; 170 switch/message tables recovered; 2,221 imported callsites resolved | 96.04% of code-segment bytes decoded |
| Pointer/data separation | 150 relocation targets into code segments audited separately; 54 coincide with classified functions and 96 remain data/pointer targets | Complete first-pass classification |
| False legacy candidate | Linear disassembly candidate `1220:0aca` proven to originate in inline data, not executable control flow | Excluded |
| Standard resources | 242 DIBs, 58 WAVE entries (55 valid RIFF/PCM and the original's 3 malformed entries), strings, palettes, dialogs, menu, accelerator, icons, cursors, and the exact 584,831-byte WinHelp payload extracted | Complete extraction; all EXE resources plus `SIMTOWER.HLP` are embedded in the native PE |
| Custom resources | ALRT, DTMP, PART, STRL, TABL, TABM, TEXT, and YEN payloads extracted and structurally decoded | ALRT, DTMP, PART, STRL, TABL, TABM, and YEN native consumers translated and tested; TEXT is preserved raw |
| CGPK | 11 packs extracted; their cell-major 8x36 indexed-tile consumer is translated directly for all Lobby and lobby-spanning Stair/Escalator banks | Pixel-tested against the exact CGPK/CLUT bytes |

## Native translation

| Area | Status |
|---|---|
| Win32/GDI application and original window procedures | Native executable, recovered class registration/window construction, complete 22-entry `MAINWNDPROC` message set, complete 27-entry command table, F1 accelerator/WinHelp boundary, minimize/restore audio activation, session-shutdown confirmation, exact `1158:028c/029f` non-arming double-click dispatch, simulation-message boundary, exact resource-backed Map window paint/mode/drag path, `1080:0209`'s no-upscale aspect-fit/centering transform, shared command/info/map eight-pixel palette frame, close-box, drag-hit, activation, and content-offset behavior, `1080:0b26`'s viewport-center floor label over the vertical scrollbar with `1058:05f8`'s synchronous scroll repaint, `1158:015b/01e7`'s exact line/page arithmetic and message-carried 16-bit thumb positions, `1158:041c/05ef`'s startup-gated fixed-thumb range rebuild and saved-position order, `1128:02aa/08d6` plus `1158:0334`'s desktop-sensitive startup/tracking geometry, and `1080:0054`'s axis-specific captured-drag edge autoscroll implemented. `1158:00da/028c/029f/02d5` now preserves the DS:0244 active-paint suppression latch and DS:0242's exact down/double-click/up arm/clear order; the DS:24b8 modal-manager lock consumes Main hit testing, scrolling, pointer input, commands, and system keys at the recovered boundaries for every native modal. Native modal entry also publishes the nested active dialog HWND corresponding to DS:31a4. `1050:0063`, `1120:005e`, and `1168:006a` use that HWND—not Main—for their exact per-palette activation redirection, Map insert-behind, Command focus, conditional TOPMOST, and DS:31a6 write/suppression branches. `1158:04fe/0508/0c29` plus `1050:0300`, `1120:0170`, and `1168:020c` preserve the exact palette-message return values, self/startup/closing suppression, conditional realization, and synchronous Map/Info/Command/Main repaint order; auxiliary windows also preserve their original startup/closing-gated `WM_QUERYNEWPALETTE` branches. The fan-out also preserves the original direct-DC Map/Command/Main versus Info-only `UpdateWindow` mechanisms, per-target invalidation, caller-DC reuse, and acquired-DC realization/release. Direct Main re-presentation no longer reruns paint-time simulation/RNG or consumes transfer visuals. The separate modal selector follows `1050:090a/095c/096a`: it returns TRUE for self notifications and otherwise uses same-DC `RealizePalette`/conditional `UpdateColors` without scheduling a substitute repaint or entering the top-level fan-out. `1168:0156` also preserves unconditional button-up capture release. The exact `1078:0000` minimize/restore path hides enabled Info/Command/Map palettes in original order, preserves disabled View-menu choices, and restores the enabled windows with Command TOPMOST and the original secondary-window z-order/0x53 flags. `1158:012c/0118` and `1078:01e8` replay that restore on every non-iconic activation, demote enabled palettes behind the main window when the application deactivates, and restore the executable's repeated Command promotion on return. The complete `1010:0018/00fd/014c/0304` startup title-splash boundary uses named DIALOG/TOWER_TITLE with the original black full-desktop surface, centered/clamped BITMAP/128 then BITMAP/256 phases, 180-coarse-tick (nominal 2.88-second) first-image minimum, synchronous replacement, wait cursor, persistence behind New/Load, and cleanup. The exact `1128:1318` startup virtual-memory preflight preserves the original unsigned `(free KB + 2370) >= 6000` gate and byte-exact low-memory diagnostic before any game resources or windows are created. `1158:049e-04fb`, `10d0:0604`, `1258:016e`, and `10b8:0000/0039` now preserve Close/Query-End-Session/Destroy separation, the repeated-session FALSE result, post-destroy Main-handle clearing, Map-to-Info-to-Command teardown, and cursor-to-audio-to-palette resource order. |
| Main cursor and empty-queue host ordering | Exact `1158:0314/032a` live-cursor-before-modal ordering; complete `1258:0505` enabled Map/Info/Command Arrow precedence, non-iconic main-client edit selector, and outside no-op. `1258:0195-023a` now performs the per-idle shared Main/palette/Elevator-Control activation and non-iconic WAVMIX reconciliation plus exact mutually exclusive Command/Elevator-Control z-order maintenance, including the force-zero `11c8:0135(0)` tail. `1258:0244-02d1` preserves the construction/DS:02a6 scheduler gate, live idle cursor sampling, and full versus preview-only `1090:03ab` selection without an invented `WM_MOUSELEAVE` lifecycle. Every eligible non-advanced tick enters `1090:03ab(0)`; native mirrors DS:77ac and applies the recovered post-derivation `EQUALRECT` test, so tool, mode, or viewport changes at a stationary cursor still repaint. Every full pass also performs `1258:0285-02c9`'s synchronous initialized Elevator Control refresh without an invented changed-only gate. The `1080:0a1e`/`1090:03ab`/`1158:00da` split now retains a DS:3264-equivalent native backing raster: exposure `WM_PAINT` is a side-effect-free blit and is skipped while iconic, full simulation advances facility-person presentation exactly once without sky, explicit `1080` rebuilds alone may advance sky first and draw `1080:0b26`'s scrollbar label, and transfer visuals are consumed only after their pixels enter the cache. The `1090:061f-06dc` fan-out now presents a dirty Main directly, invalidates Command only, presents Info content directly every full frame, and leaves Map exclusively on its independent sixteen-tick refresh cadence. Pure branch plans and stationary-point rectangle changes are headless-tested. |
| Original resource loaders and custom decoders | Exact embedded pack plus DIALOG, DTMP, ALRT, STRL, PART, YEN, TABL, TABM, menu, accelerator, icon, and DIB consumers implemented and tested |
| Renderer, palettes, WinG blits, masks, and animation | DIB parser/blitter plus type-zero Floor's exact fixed status-cell span from BITMAP/1000..1003, type-45 Metro boundary spans from BITMAP/3880, and type-47 damage rubble from BITMAP/4008, direct facility frames for types 3-15, all eight 36-row SECOM Center frames, all Movie Theater type-18/19/34/35 body and entrance frames, all six upper and seven lower Recycling Center type-20/type-21 frames (including BITMAP/2280's exact 60-row split), all three upper and three lower Party Hall type-29/type-30 frames with linked dc24 selection/clamping, all nine Metro Station type-31/type-32/type-33 frames, all sixteen Cathedral type-36..40 frames (including type 40's unique fourth frame in BITMAP/3562), the type-0x29 pending-construction strip (including palette fill and per-cell phase), Lobby, Office, all standard/express/service elevator cap, body, car, and sparse-floor variants, normal and multi-story Stairs/Escalators, and all three type-0x2c Parking Ramp connectivity frames are translated and pixel-tested from the original DIB/CGPK/CLUT bytes. The exact `10a8:0000/02aa/0507/07d6` Elevator layer uses the recovered per-floor Shell-sorted x order and shaft/transfer interleave; nonzero SHOW shafts bake cars into their bodies, while zero shafts retain both caps, draw two black 35-pixel boundaries per in-span floor, and alone enter the exact `1090:0cb3/0b10/216e/221f/227b` late car layer. That compositor copies 28/44x31 standard/service/Express sprites from BITMAP/1064..1069 with original occupancy frames, signed six-pixel motion, visibility/clipping, and post-Stair/Escalator ordering. The exact `11c0:0000/024a/02c0/0374/0428/0483/04ce/0518/054e` final exterior layer composes and draws ordinary floor caps, ground foundation ends, and the topmost-wide-floor roof marker from BITMAP/1001/1002/1193/1259/5000. The exact `10a8:088c/0fff/12c1/1737/1875/1913` sorted two-lane Elevator waiting-person layer and `10a8:022b/02aa/0de6` one-frame boarding/alighting layer use BITMAP/1128+1129 with original ring order, widths, boundaries, placement, per-floor/per-Elevator two-sided cache overwrite, explicit-rebuild reset, and boarding-before-alighting draw order. BITMAP/3944..3949 remains exclusively type 46's exact nine-frame fire atlas. The `10e8:04a0/0693` fire-band/crew layers and `10f8:00c9` Security responder layer reproduce their exact frame selection, persisted coordinates and ownership lookup, draw gates, layer positions, and `11a0:0cd9/1144` channel-wise nonzero compositors. The complete `11b8` annual moving effect uses the exact transparent-zero BITMAP/904 pixels, active shared palette, world coordinates, clipping, motion, and full-width exit condition. The complete `1048:00ad/05f0/06a5/0717/083f` four-slot sky-decoration layer uses BITMAP/900..903 with original eligible-world intersection, visible-client expansion, full-rectangle retention, strict fit gates, duplicate randomized selection/placement, exact shared RNG consumption, transparent-zero pixels, and background-before-people ordering. The Map window translates `1160:0000/01dc`, `1080:093a`, `1058:01d6`, and `11d0:0254/0363`: exact 200x306 backing layout, BITMAP/352 clock-driven cyclic background, rating-sensitive BITMAP/310-312 toolbar whose fourth painted cell remains inert at rating one, occupied-floor bands, all three tenant overlays, type-colored Elevator lines, annual marker, BITMAP/313-315 legends, viewport focus transform, and click/drag inverse transform. The main World now also preserves `11d0:0072/0145/04ba` and `11e0:0efb` Map-mode draw order: the four exact eight-pixel strips from BITMAP/1003 tile across tenant rectangles after facility people and before later event/transport layers, with signed mode-specific satisfaction/rent/hotel gates, special full-story geometry, clipping, and source phase. The full `1020:0e29/0f4f/098b/08b4/00cb/053e` logical-palette path is translated: skipped CLUT source 184, exact three flag bands, zero entry 255, native 256-entry `HPALETTE`, CLUT/1000..1003 time/event interpolation and aliases, `AnimatePalette` updates for reserved entries 188..218, persistent 15-coarse-tick (nominal 240-ms) effects phases at entries 194..203, the special-event alternating aliases at 207..218, signed timer/counter edge behavior, and Effects-menu freezing. All indexed World and Map layers consume that one runtime palette. |
| Menus, dialogs, controls, information windows, and input | Original menu/accelerator/icon/cursors and generic Win16-dialog/DTMP runtime translated; exact 27-entry `WM_COMMAND` table recovered. The command palette now uses the original one-based TABM resolution, raw TABL/1000 icon-to-build mapping, mutable group-choice state, exact +8 presentation offset, and the exact `1058:04e0`/`1050:05d6` resource-backed vertical modal selector used for Ramp/Parking and other grouped categories, including selected-row alignment and both desktop-edge corrections. `1050:0219/02b3`'s mouse-phase dispatch is preserved: ordinary edit/facility points activate on button-down so grouped selectors track the held click, while only the build toggle waits for button-up after its pressed frame. The shared `11e0:0b52` outer-window placement is translated for all eleven original callers, including signed centering, its 43/80-pixel vertical policy, bottom clamp, topmost/show flags, and Elevator Control's explicit x=8 position. The native Person/Tenant Find commands now use DIALOG/510/520 and the shared DTMP/BITMAP/510 surface, populate the original saved-name order, preserve selection enable/cancel/double-click behavior, compact link/name tables on Remove, and resolve direct/linked facilities, Stair/Escalator transit, moving Elevator cars, and both waiting rings before applying the original edit-mode/camera transition. The exact `10e0:0669/06cd/0b61/0bc6/0c72` unfocused paths show ALRT/1002 (“not in this tower”) or ALRT/1003 with the saved name and original above-ground/basement Lobby floor text instead of inventing a Person Information modal. The recovered `10d8:02ce-0317` and `10e0:04cf-055b` lifecycle paints transparent BITMAP/21256 at the focused world coordinate through 300 coarse ticks (nominal 4.8 seconds), then restores the original construction/map-mode path. The exact About command uses named DIALOG/TOWER_TITLE, BITMAP/257, all 176 TEXT/128 credit lines, the recovered 604x290 geometry, 236x266 clipped one-pixel/55-ms scroll, cleared class cursor plus selected stock arrow, key/mouse dismissal, and original audio activation lifecycle. Person Information/Rename Person, Facility Information/Rename Tenant/New Movie, and Elevator Control now use their original DIALOG/DTMP resources with translated custom filters, live models, and native painters. Facility Information includes the exact `1108:0000` three-line advisory chain, `1100:465a/4869/4d1d` source selection, crop, signed scaling/centering and RGB(204,204,204) backing, meters/text, and `1100:2852/2c23/2ec2/307e` live BITMAP/700/702/703 person lineups with original item-4/item-9 geometry. Elevator-car and Stair/Escalator information now include `1100:327f/3431/35b7/364a/3856`'s missing live passenger portraits, using exact car slots or `1218:0771` transit scanning, two-row DTMP geometry, normal/named/VIP art, and clickable Person Information drill-down with post-rename refresh; `1100:5043` selects CURSOR/1003 only over those two portrait panels. The Info window translates `1120:0215`, `1118:0044/0143/026a/0368/045d/073d/08f3/0933/09be/0a49/0ad5`, and `1200:0037/058d`: BITMAP/320 backing, clipped BITMAP/321-323/327 clock/rating art, original Arial pixel heights, balance, population, weekday/weekend/quarter/year formatting, 3.14-step x87-floor analog-clock hands, and the complete DS:784c transient construction/gameplay/income/command message state with exact STRL resources, priorities, replacement gates, signed-magnitude tick arithmetic, and greater-than-300-coarse-tick (nominal 4.8-second) expiry. All eight Office/Hotel/Condo/Retail/Restaurant/Fast Food/Movie/Party income messages, ordered person notifications, and Hotel DIALOG/3000..3003 process calls are wired to that shared/native boundary in original callback order. Edit dispatch preserves the original mode meanings (0 Bulldozer, 1 Elevator Finger, 2 Magnifying Glass, 3+ construction), Elevator -> Stair/Escalator -> facility precedence, exact shaft/car and transport/facility geometry, and Shift replacement. Facility, passenger-bearing Stair/Escalator, selected Elevator car, and full Elevator shaft Bulldozer transactions are wired with native family dispatch, the original hit/last-car/confirmation gates, route rebuilds, queue reassignment, and malformed-data rollback. Exact facility/elevator/vertical information-dialog resource IDs, Magnifier precedence, two-lane Elevator waiting-person selection, Finger gates, ordered Lobby-transfer/route graph rebuild, route-loss warning codes, original ALRT/STRL 1005 confirmation, elevator shaft-confirmation connectivity, and post-cleanup car-removal mutation are translated. Native Finger input adds/removes zero-person and passenger-bearing service stops with the exact confirmed graph-before-`14cc` cleanup transaction and native family dispatch. It also handles upper/lower cap extension and passenger-bearing shrinking, including sparse express stops, collision, funds, 29-floor clamp, service/car/route changes, the distinct global `1625` waiting-ring-before-`14fa` car-arrival order, save-record growth/compaction, floor coverage, owner release, target recomputation, and malformed-data rollback. New/Open/Save/Save As/Exit IDs and the complete open/save window boundary are wired, including `10d0:2a8e`'s floor-slot-10 occupancy confirmation predicate rather than a native dirty flag. |
| Simulation/gameplay | Exact timing/day dispatcher boundary; the complete `10b0:0000` New/Open reset chain now flushes pending construction, runs `0072` tenant repair, clears `1198:0000` parking counters and `11a8:14c9` commercial banks, resets all 24 Elevators through `1090:00d9`, all 64 Stairs/Escalators through `10c0:0000`, then runs `031a` people repair. The complete `1220:0000` nightly person-family reset, exact `1228:086b/0968/0b59` pre-midnight/day-start/evening Hotel/Office/Condo and special-part facility sweeps, exact modeled `1198:01ab` day-start parking-index/orphan rebuild (including `1198:00d9`'s deliberately stale category-nine parking ceiling and wrapping total), the complete scheduled `1088:0000/00de/01d1` Recycling Center population/phase/midnight state machine, exact `1170:011f` Medical Center day-start service/orphan/index rebuild, exact `1020:0dcb/0e0b` periodic b406 event-flag pair, exact `1240:01de` nine-day b924/b928 reset, exact `1060:003a` three-day accounting rollover, exact `1060:07b3/07f7/0837/0880` population/income/maintenance primitives and both binary-derived category maps, exact `1178:0854/08ec` YEN/1001 rent addition/removal primitives, the complete `1178:0b44` three-day tenant/Lobby/elevator/Stair/Escalator maintenance sweep, the complete `1180:05af/06a8/0826/090a` Movie Theater/Party Hall population, capacity, arrival/show/departure, dirty-state, and admission-income schedule, exact `1130:01e2` Hotel pair repair, revision-aware b402/dce4 person-link state, complete `1188:0977/0a20` scheduled link filters, exact `11e8:0273` random Metro pulse, complete `11b8:0028/0060/0000` annual moving-effect state (including the corrected BITMAP/904 140-pixel start/termination edge), complete `11c8:03ab/03fb/0426/05e8/0671/06b6` ambient world-sound selection, exact Bomb/Fire offer, damage, modal-boundary, Security/crew, focus, clock-jump, and completion state machines now consumed by the native per-frame host, complete facility-owned person cleanup and `11f8:3528/35ac` facility-damage conversion, the complete live Hotel/Office/Condo/commercial/Security/Housekeeping/entertainment/Metro/Cathedral person-family switch, and original construction paths for Floor, Lobby, all three standard/express/service elevators, Stairs, Escalator, Parking Ramp, Parking, Hotel types 3-5, Office, Restaurant, Condo, Retail Shop, Fast Food, Medical Center, Security, Housekeeping, SECOM Center, the two-floor Movie Theater, the two-floor Recycling Center composite, the two-floor Party Hall, the three-floor Metro Station, and the five-floor Cathedral are translated. Construction input also preserves `11f8:0955-098c`'s hidden empty-tower floor-zero/cell-zero initial-balance transaction. Elevator work includes exact raw-command type/capacity/width/cost dispatch and the captured upper/lower shaft-extension transaction with sparse express floor records, service gates, collision, funds, standard/service span clamp, route rebuild, YEN accounting, and automatic physical floor/Lobby coverage. This includes Floor drag/coalescing/support, the shared 64-slot Stair/Escalator records, exact landing whitelist, lobby-spanning shapes, collision rectangles and directional route-summary flags, the exact 16-cell Ramp and repeated 4-cell Parking drag paths, basement/ramp gates, connectivity rebuild, parking-index allocation, costs, ten-slot deferred activation, exact person/service records (including Housekeeping's `15/00/FE/FF` records and shared cf88 registration), SECOM's deliberate post-activation `ff` lookup key and untouched negative reservation records, Medical Center's dbfc timer/orphan repair, b3fe count repair, bd5a and seven-group route reconstruction, and rating-gated b92d, Movie Theater's paired pending records, 18/19-to-34/35 expansion, 112-person initialization, shared dc24 link, and Microsoft-runtime RNG state, Recycling Center's exact b3f4 alignment/adjacency gate, construction order, `ff` keys, deliberately inactive twelve reservation records, 500-person operating bands, persisted b92c gate, frame-five hold, upper/lower midnight reset, information codes 3/4, and WAVE/2280 request, Party Hall's exact 29/30 ordering, eighty type-29 people, and shared b400/dc24 Movie capacity and link, Metro's unique b3e8/floor-zero gate, exact three-part order, split funds check, 240 active passengers, twelve deliberately inactive reservations, and `11e8:0000` type-45 floor-boundary expansion; Cathedral's unique b3ec/floor-113 gate, exact bottom-up five-part order, split funds check, forty shared type-36 people, bottom-key persistence, and its direct midnight/04b0 state transitions. The exact `1028` facility-interior presentation/animation dispatcher and helpers are integrated into the native world layer and headless-tested. |
| Scheduled commercial services | Complete `11a8:0184/0250/0554/0603` Retail Shop/Fast Food/Restaurant population, dd5c/dd60/dd64 route-block, service-state, attendance-income, orphan-cleanup, dirty-state, event-day, opposite-endian schedule, and exact `1118:0a49` Restaurant/Fast Food income-message calls translated and wired. |
| Scheduled tenant evaluation | Complete `1130:0000/0109` satisfaction, person-performance, amenity-adjacency, three-day Office/Condo/Retail departure/rent/population/service, same-type pairing, person-metric reset, and ordered Hotel checkout schedule translated and wired. Selecting Map mode one also invokes the exact `1130:00b5` all-tenant satisfaction refresh before repaint. |
| Person routing and transit cleanup | Complete `1210:0000` route resolver and `11b0:0f10/0fa5` selector: owner-word lookup, cf10 Stair span gates, 64 Stair/24 Elevator/8 bff0 route scoring, db9c transfers, sparse express mapping, exact counters, both forty-entry `1210:11c2` waiting rings, initial `1090:0a4c` car assignment, queue-full/no-route accounting, and person byte-7/byte-8/word-10 writes are translated and headless-tested, including opposite-endian queues. Complete `1220:1059/10af/1518` tenant-owned sweep, active-car removal, occupancy/target recomputation, metrics, and Office parking/accounting exit are also wired. The exact Hotel type-3..5, Restaurant/Fast Food type-6/type-12, Office type-7, Condo type-9, Retail type-10, Security type-14, Housekeeping type-15, Movie Theater/Party Hall type-18/type-29, Metro type-33, and Cathedral type-36 raw per-frame families are translated. This includes Hotel paired occupancy, parking allocation/capacity/reversal, periodic visitor transactions, activation/checkout rent and population; Office six-person occupancy, commercial/Medical Center detours, parking, rent and population reactivation; Condo three-resident synchronization, commercial detours and reactivation; Cathedral ceremony side effects; bomb patrol and fire-floor partitioning; Housekeeping's six-floor dirty-room search/service routing/three-callback cleaning/guest-room reopen; randomized commercial destinations; entertainment dc24 capacity/show state; food-service reservation/rollback, population/dwell and attendance history; and Retail inactive-store activation. The native normal pass preserves these families in person-index order with exact wrappers, including Hotel ordinal-zero exclusion, Office day/calendar/random gates, Cathedral's calendar-one random/double-call behavior, and native process-only visual/transaction boundaries. The complete signed `1220:1637` timeout, `1210:1b41/1332` queue rotation, `1220:1aed` transit arrival switch, `1220:16ab` raw family dispatcher, and demolition/car-arrival `1210:0883` branch are translated, integrated, and headless-tested. Exact family-specific boarding destinations (`1220:685d/692c/69ae/6aba/6b11`), serviced/transfer-floor selection (`11b0:092f`), alighting and boarding (`1210:07a6/0351`), wait metrics, capacity/occupancy counters, rejection dispatch, and per-floor/per-Elevator/per-side last-writer transfer visuals are now integrated with the complete `1090:06fb/10e4/209f/23a5` car movement/door/dwell state machine and the original all-cars-first `1090:03ab` frame order. The native cleanup overload preserves the car-only Security branch, up/down queue order, stale slots, opposite-endian dwords, and process-only requests without the original executable. |
| Audio | RIFF logical-size parser and original two-channel WAVMIX state machine translated over native `waveOut`: priority/category gates, reserved channel, preemption, 600-coarse-tick (nominal 9.6-second) saturation rule, repeat semantics, activation, and shutdown are implemented. The shared `11e0:0e84` service preserves its unsigned 48-ms rolling deadline, single-interval late catch-up, tick wraparound, and any-category gate; because waveOut exposes `WHDR_DONE` instead of posting the original callback message `0x03bd`, each due native pass reaps equivalent completions. The `1258:0195-01c3` empty-queue gate now retries `11c8:0aab` activation only for an active, non-iconic main window and runs `11c8:0add` deactivation/channel stop after ordinary deactivation as well as minimization. The complete `11c8:03ab/03fb/0426/05e8/0671/06b6` world-state selector reproduces the exact six-point view probe, facility/person/service-state mapping, contextual background sounds, and conditional RNG consumption before submitting a resource to that backend. `1128:03ad-0535` preserves the original SIMTOWER.INI probe/read order, exact `BeepOnly` equality branch, master/category defaults and gates, and `1128:0b0d-0b95` menu check/gray state; the supplied default INI values are embedded so the release remains one file. A silent hardware smoke directly covers `11c8:006b/0920`: 50,300 bytes at 11,127 Hz/8-bit reached an active WAVE_MAPPER channel and clean teardown without audible output. |
| `.TDT` save/load | Native `original_tdt.cpp` translates the complete byte-transfer path for revisions 0x17-0x24: header, 120-floor tenant map, people/retail tables, all 24 variable-size elevators, finance, parking, stairs, routing/fixed/dynamic tail blocks, revision gates, and every recovered opposite-endian transform/no-transform quirk. Four reference files rebuild byte-exactly; editable mutations and newly constructed facilities write/reparse across all translated structures. Pre-0x22 segmented car records, their zero-retained gaps, pre-0x19 active defaults, pre-0x22 capacity repair, and the express-elevator unmapped-floor swap bug are covered. Opposite-endian Open now normalizes every gameplay-facing raw record immediately to `10d0:1518`'s post-load runtime layout while retaining the untouched source stream solely for explicit lossless tooling. The game-facing writer matches `10d0:0b3a`: Save always upgrades imports to revision 0x24 little-endian, including widened floor indexes and expanded link/final blocks. Independent asymmetric opposite-endian and revision-0x18 fixtures plus an on-disk revision-0x17 upgrade test cover the distinction. The writer rejects b402 counts above the exact ten-entry pre-0x23 or twenty-entry 0x23+ dce4 capacities and models revision-0x23+ b404 plus both exact fixed 16-byte Find-name tables written after the core transfer, preserving ordinary post-NUL bytes and unrelated exporter tails. The load path also accepts `1188:02ea`'s older 256-byte Pascal-name records, normalizes them to the runtime 16-byte layout, and compacts duplicate person/tenant keys through the original b402/b404 rebuild semantics. The exact fresh revision-0x24 constructor and native Open/Save/Save As boundary are implemented and tested. A deliberately balance-mutated native-written save is accepted and rendered by the supplied original, then reopened reciprocally by native; the modern quoted-path adapter is regression-tested without changing recovered Win16 parsing. Opaque subfield semantics remain. |
| Self-contained native executable | A statically linked x86-64 Windows GUI PE containing the exact resource pack builds without a runtime dependency on `SIMTOWER.EXE`, imports only Windows/UCRT DLLs, and passes the forbidden-reference scan. The current release candidate has completed muted startup/New/Open/Cancel/exit, reciprocal original/native save-load validation, a 90-second loaded-state soak, and silent real-PCM hardware submission. |

The complete eleven-entry `1050:0000` Command-window table is now audited.
Its shared top-level palette adapter preserves the repeated `1050:0032`,
`1120:002f`, and `1168:0032` `WM_ACTIVATEAPP` prefix: application activation
raises/shows Main with literal `SetWindowPos` flags `0x53` before entering
`1078:01e8`. The three close boxes and the `1158:0886/08a6/08c4` View
commands also retain the executable's observable stale menu marks by changing
only visibility, without a native-only `CheckMenuItem` update. Its WM_PAINT
path realizes the logical palette before—not inside—the visibility/New/Open/
closing content gates. Every non-close button-down also preserves the final
`1208:05e6` call and DS:31b0/31b2 timestamp write after toggle or point
activation, including the shared 48-ms WAVMIX-pump side effect.

The same table-level comparison now covers all ten `1120:0000` Info messages
and all thirteen `1168:0000` Map messages. Separate native class procedures
preserve identity before the global window handles are assigned. Map consumes
all mouse moves, conditionally updates only for its captured left-button drag,
and clears the drag/capture state solely on button-up; Win32's extra
`WM_CAPTURECHANGED` passes through without altering original state.

The full `1158:0000` audit now covers every one of Main's 22 literal message
entries and all 27 literal `1158:06b9` commands. Native-only
`WM_SETCURSOR`/`WM_CAPTURECHANGED` handling and activation-time auxiliary
invalidations were removed. Creation now owns only the original Fast Mode and
release-menu changes; Sound initialization occurs later at its recovered
startup boundary, leaving the resource's visible Windows items unchecked.
Scroll invalidation, non-table `DefWindowProc` command returns, the three
table-owned 3000-series process commands, and File/Exit's distinct stale-main-
handle shutdown tail are preserved. The connected Finger transaction also
captures otherwise-unhandled and empty-space presses exactly as `10a0:0544`
does, suppressing idle simulation through DS:02a6 until release.

The complete `1058:0000` comparison now covers modifier publication and every
armed/isolation/emergency/tool branch. The native dispatcher preserves all
four mouse phases, mode-two's Find-marker toggle tail, Finger move-time cap
acquisition and double-click cleanup, and enabled construction routing. The
connected `10a0:0201` return-value audit also restores hidden-shaft
pass-through: an in-span no-car hit with `word_3c == 0` continues to the
Stair/Escalator and facility Bulldozer legs instead of consuming the click.
Pure branch regressions and all 14 Release suites pass headlessly.

The latest host-flow audit additionally restores direct behavior around those
translated families: revision-aware `b406` gates scheduled WAVE/5005 and
WAVE/5003; frame zero clears `b3e4`; frame `0x04b0` clears the Hotel checkout
counter; the checkout latch drains through `1118:0143` and conditionally emits
WAVE/10013; `1258:023a` runs the 15-coarse-tick (nominal 240-ms) effect-palette gate on every empty-queue
pass; all four `1090:0448/0465/0615/06dc` per-frame palette calls are retained;
and `1090:0452` selects exactly one of the emergency Security pass or normal
person-family pass after Bomb/Fire dispatch.

The adjacent `1258:000b` host-loop audit now restores fetched-message routing:
Elevator Control outranks an active modal, either receives its accelerator
attempt before dialog navigation, and Main is used only when neither exists.
The empty-queue path no longer adds an unoriginal `SwitchToThread` yield.
Startup also honors `1258:0029-0070`/`1128:00e5-0196`'s raw command-line TDT
target and bypasses the New/Load dialog when it is present. Accepted Open paths
now use `10d0:0225/062a`'s reset-before-I/O behavior and create a fresh tower
after an open or parser failure instead of silently retaining the old tower.
Open and Save As additionally preserve `1078:01e8(0/1)`'s palette-window
demotion/restoration and Arrow cursor around every common-dialog attempt; Save
As retries an invalid eight-character DOS basename, and the save transaction
deletes its partial target after a post-create transfer failure. The startup
publication tail now restores Arrow, applies `1058:033c` if construction is
disabled, and synchronously publishes Command after all four windows appear.
That shared construction toggle now also performs its original WAVMIX
deactivate/activate branch, Find reset, mode-two selection, and distinct
preview presentation for Command clicks and Find-marker expiry.

## Current verification boundary

The current construction audit also translates `1118:0933` and
`11f8:0e09/0e21/0e58/0ec0`'s STRL/1003 status and success/failure audio
lifecycle. Raw command type 24 now follows `10a0:12e0` and
`11f8:26dd/284d` for both the automatic ground Lobby and sky Lobbies at
floors 24/39/54/69/84/99, including support/collision gates, adjacent
extension, type-0 placeholder splitting, lookup allocation, and separate
Lobby/floor-cell charging. Separate Floor placements now follow
`10a0:1310` and `11f8:17fd/284d/30ef` exactly: clicked intervals remain
distinct, disjoint gaps become separate automatic Floor records (or Lobby
records on the initial Lobby stories), and overlapping replacement preserves
the uncovered left/right remainders byte-for-byte.

The native target builds cleanly and all fourteen headless suites pass with
assertions forcibly retained in Release builds: resources, embedded resources,
TDT, scheduler, people, construction, world pixels, command palette/frame, map
pixels/transforms, Info derivation/resources, Person/Facility Information,
exact Find resources/state, exact About resources/geometry/scroll state, and
exact Elevator Control state/resources.
The construction-hover overlay now uses `11f8:0000/3cab/3da4`'s exact
resource-derived footprint, signed grid snap, clipping, and clipped-edge white
outline pixels. Command buttons use the original 600/601 and 602/603
up/pressed BITMAP pairs and commit only on mouse release. Facility advisory
noise scans now preserve `1138:00a5/0128`'s current-edge-before-neighbor-step
ordering, including the first noisy tenant just beyond the nominal boundary.

Waiting-person Find now follows `10a8:00a8/09e7`'s exact diminishing-gap Shell
ordering instead of a stable x sort. This preserves the executable's indirect
equal-x shaft reordering and therefore its left/right queue boundaries and
focused coordinates; a four-shaft equal-x regression covers the distinction.

The Find dialog presentation now follows `10d8:00ec-0146/0323-0360` exactly:
both initialization and `WM_PAINT` realize the shared palette and render the
positive DTMP/510 surface, including the original child-control layout replay,
without adding the generic negative-resource bevel/chrome path. Initialization
alone selects the recovered 12-pixel font. Headless phase assertions cover both
paths.

The shared DTMP initializer now also preserves `1070:0005`'s signed-resource
split and ordering. Bitmap-backed layouts resize from their DIB before acquiring
the dialog DC and do not realize the palette there; bitmap-less layouts acquire
the DC, select/realize the logical palette, add `TA_UPDATECP`, release the DC,
and only then apply a nonzero header size. All native DTMP callers use this
central boundary, and pure branch tests cover positive, negative, and zero-size
headers.

The shared font subsystem now follows `1208:0a8d/0ba7/0b6a` instead of
creating and deleting independent native fonts at every paint site. It performs
the original exact Arial enumeration with MS Sans Serif fallback, seeds the
nine-pixel font with default precision, clamps requests below nine, reuses
matching heights, gives later entries TrueType-only precision, stops selecting
as soon as the ten-slot bank is full, and deletes the published handles in slot
order during process teardown. Dialog and Info-window painters borrow the same
process bank. Pure tests cover every cache decision and both creation variants.

The following shared WinG utility pass proves `1208:07d5/09cf`'s top-down
8-bit layout, DWORD stride, BLACKNESS clear, and complete B,G,R,0 palette
transport; `1208:069a -> 1248:0000/1250:0114`'s equal-size opaque destination
clipping and matching source-origin advance; and `1208:0603/063a`'s asymmetric
word/dword byte swaps. It exhaustively validates all 242 embedded BITMAPs
against `1208:049d`'s fixed 40-byte header plus 1024-byte palette and byte-1064
pixel start. Person portrait scaling is now correctly attributed to direct
`1100:364a/37a9/37d1` WinGStretchBlt/WinGBitBlt calls rather than the unscaled
`069a` wrapper.

The native host now also preserves `1000:3a18/3a2f`'s process-global Microsoft
C runtime random sequence across New/Open document replacement. Static recovery
finds no caller or relocation to the seed writer, so TDT load and fresh-tower
fallback carry the live state instead of reseeding it to one. Pure transition
assertions cover both startup and replacement behavior.

The conservative native-source index now records 911 exact recovered routine
starts, 1,330 unique source-cited addresses, zero inferred mappings counted as
exact, and 264 compiler/runtime candidates without native citations. The
remaining set is exhaustively confined to support segments `1000`, `1260`, and
`1268` and is classified in `NATIVE_RUNTIME_CLASSIFICATION.md`; all game-owned
starts in the current 1,175-candidate set have explicit native mappings. The
latest static pass also restored Facility advisory text to
`11e0:0049`/`1100:1760-176c`'s item-8 offset `(8,18)` with a 16-pixel line
step, plus `1080:09c3`'s
every-sixteen-tick animated Map repaint,
`10c0:002e`'s wrapping Stair/Escalator activity scan, and the synchronous
`1118:0143` Info-panel balance repaint performed by construction debit
routines `1178:01db/027c/0697`.
The same disassembly pass corrected `1100:4869`'s Facility preview source
rectangle: Metro and Cathedral start 12 pixels inside their top floor and use
the recovered 60/168-pixel heights, while Restaurant/Retail/Fast Food widths
come from the original type-width table rather than potentially inconsistent
serialized tenant spans. All affected component variants are headless-tested.
The complete `1200:0196` scheduler cycle now also has an explicit native
dispatch contract covering all 36 selector/offset/argument signatures; a
full-cycle test exercises every conditional emission and the host rejects any
unknown, malformed, or unhandled callback instead of silently dropping work.
The shared Command/Info/Map WndProc comparison now also preserves
`1050:00f8/010d/01b0`, `1120:007d/00a5/0167/0215`,
`1168:009f/00c7/016a/0203`, and `1078:00c6`: `WM_NCACTIVATE` paints only the
eight-pixel title strip synchronously, active non-modal `WM_ACTIVATE` validates
the pending client update, each paint uses its original full-region and
startup/visibility/closing gates, and destroying any palette posts the
original quit message. Pure plans plus a memory-DC pixel test cover the shared
behavior without opening a window.
It now also preserves `1058:06df`'s Map-drag `EQUALRECT` no-op: clicking an
unchanged focus rectangle retains a non-map-aligned raw scroll position and
does not trigger an extra presentation/RNG pass. Elevator car construction
now follows `1148:02c8`'s persisted count-byte gate before `11f8:113f`'s raw
record scan, including the distinct entry-24 and silent malformed-save
failures. The transport-only `1158:0ba8` WinG blit and
`1098:1498/1f9d/226e` Elevator Control presentation helpers are explicitly
mapped to their direct native equivalents.
The final game-owned audit additionally restores `1100:0116/085b/0f10/1248`'s
shared 13-pixel font, logical-palette realization, palette-matched RGB(204)
static-control brush, cleared class cursor plus selected Arrow, nested-modal
activation redirect, and post-child TOPMOST restoration for Person, Facility,
Elevator-car, and Stair/Escalator information dialogs. This corrects the former
reversed Facility-panel activation target and preserves ID-1-only transport
closing. It also fixes `1108:014b/016e`'s Facility Information
transport-distance scan to use serialized tenant offset zero (the
left-x field), gates the secondary item-9 portrait cursor zone to the original
dialog groups 9-11, and reproduces `1068:0000 -> 1008:0085/0000`'s event-dialog
font patch from the embedded 10-point resource size to 8 points. The palette,
command-selector, route-scratch, masked/opaque blitter, startup, and shutdown
support boundaries are now explicitly tied to their native value/RAII
equivalents as well.
No DOSBox, original-game window, or native GUI was launched
during the current audit. Visual side-by-side validation remains deferred until
the user explicitly permits opening those windows.

The latest static UI audit also corrected Elevator Control's selected-car
outline to use `1098:1e33`'s live current floor and inverted scroll transform,
rather than the configured home floor. Its `1098:16a4` grid-line geometry and
`1098:1502` six-phase schedule presentation are now explicitly mapped as well.
It also translated `1078:0000`'s exact auxiliary-palette minimize/restore
visibility and z-order lifecycle; all eight persisted visibility combinations
are represented by the headless action planner.
The exact `1128:1318` startup virtual-memory gate is also translated, including
its 2,370-KB resident credit, 6,000-KB threshold, first passing raw-free-space
boundary, and original calculated diagnostic text.

The latest static custom-dialog audit closes the remaining presentation gaps
against NEWORLOADDLOGFILTER `1018:0067`, COUNTDLOGMAIN `1060:00d3`, and
NAMEPEPLE/NAMETENANT/MOVIETITLE filters `1100:3a39/3dc4/4138`. It restores
their exact font sizes, cursor-class clearing, palette realization,
transparent text alignment, immediate DTMP phase, paint phase, and Finance
Return/Space pressed-to-released close sequence. The native executable was not
launched; the complete 14-test suite passes headlessly.

The latest coarse-clock audit translated `1208:05e6` as the original signed
`GetTickCount() >> 4` source and corrected every affected native gate: startup
title splash 180 ticks (nominal 2.88 seconds), palette effects 15 ticks
(240 ms), scheduler 6 ticks (96 ms), Find/Info lifetime through 300 ticks
(4.8 seconds), and saturated-audio recovery 600 ticks (9.6 seconds). Signed
wrap behavior is covered at the exact boundaries, the full 14-test Release
suite passes, and the native GUI/audio paths were not launched.

The complete static audit of construction input routine `11f8:07d8` now fixes
the native host's captured-command boundary. Floor, Parking, Lobby, and Ramp
alone accept move/up; capture and the shared step counter precede the hidden
starting-balance bonus and Shift replacement; Parking/Ramp helper state is
initialized only once those helpers are reached; and abnormal armed
double-clicks reuse the retained down placement without repeating setup.
Release now reproduces the original Floor/Lobby balance-change gate, early
WAVE/7000, Parking/Ramp silence, and zero-count WAVE/7002. Rating/treasure work
runs once at successful command completion rather than once per drag step, the
five-type common-success-wave exclusion is literal, and only command types
`1,22,24,27,42,43` trigger the post-success route rebuild. The full Release
build and all 14 headless suites pass; the package hash matches the tested
binary and its static dependency/runtime-reference scans pass. No GUI or audio
path was opened.

The adjacent `11f8:240d/25a2` comparison now also replaces the host's eager
Parking/Ramp drag approximation with the original retained-state machines.
Construction consumes the previous message's target, updates the current
target only after sound/auto-scroll, uses four-cell Parking and ordered-floor
Ramp loops, and advances its bounds even when a unit is rejected. Ramp attempts
use the current call's x snap, so diagonal motion can correctly fail the
persisted same-x constraint. A multi-unit message exposes only its final result
to the shared step counter, one construction sound, and the axis-specific
auto-scroll call. The DS:24ca direct-first/reserved-later audio boundary and
abnormal double-click's stale c6/c8 versus live LPARAM split are preserved.
Pure transition tests and the full 14-suite Release run pass; the refreshed
self-contained PE passes its build-hash, import, and forbidden-string checks
without execution.

The connected `11f8:26dd/284d/17fd` Floor/Lobby audit removes the remaining
transactional approximation. Ground Lobby selection now publishes b3e6 before
validation, every selected story is attempted in order even after a failure,
successful earlier calls remain committed, and only the final story controls
24cc plus horizontal auto-scroll. Returning a Lobby drag toward its press
anchor preserves split outer Lobby records rather than recoalescing them.
WAVE/7001 is requested only by a nonzero-cost overlapping `284d` path; charged
empty/disjoint `17fd` construction is silent, and an earlier story's sound
request/DS:24ca consumption survives a later-story failure. Native dirty and
repaint state now also records these partial mutations instead of discarding
them at the final failure boundary. The new pure completion and persisted-state
cases pass headlessly; no GUI, emulator, game process, or audio path was
launched for the audit.

The `1100:3a39` Person Rename and `1100:3dc4` Tenant Rename filter audit also
removed a native-only select-all convenience. Both original procedures install
the saved text and return TRUE from `WM_INITDIALOG` without setting focus or
issuing an edit-selection message. Only their later `WM_PAINT` tails enable and
focus item 4, preserving the edit's existing caret/selection. The native host
now follows that two-phase sequence for both dialogs; a pure message-plan test
guards the absence of `EM_SETSEL`, and all 14 headless Release suites pass.
No window or audio path was opened.

The adjacent `10d8:006f` Find filter had the same native initial-focus
substitution: its `00a1-014b` initialization path populates and paints the list
but never calls `SetFocus`, then returns TRUE so Win16 chooses the initial
control. Native Find no longer forces list item 5 or returns FALSE. The exact
focus/result pair is now part of the headless Find contract.

The complete seven-message tables for `1100:0f10` ELVINFODLOGFILTER and
`1100:1248` ESCINFODLOGFILTER are now statically audited against their native
boundary. Their `WM_INITDIALOG` branches, like `1068:00a1`, return TRUE without
forcing a control focus. Their `WM_LBUTTONDOWN` branches always consume the
click and restore the parent as TOPMOST plus DS:31a4, even when no portrait is
hit; both realize the palette, while only the Stair/Escalator filter first
selects it. Native now preserves those distinctions and retains the recovered
portrait drill-down/refresh behavior. Exact-address headless plans cover both
filters, all 14 Release suites pass, and no GUI or audio path was launched.

The adjoining complete `1100:0116` PEPLEINFODLOGFILTER and `1100:085b`
TENANTINFODLOGFILTER audits removed the same forced-focus substitution from
Person and Facility Information. Person Information now preserves the recovered
SetCapture/ReleaseCapture lifecycle and clears/restores DS:31a4 around Rename.
Facility Information now selects/realizes the palette, dispatches portrait hit
testing, and restores DS:31a4/TOPMOST for every left click, including a miss.
Its `0b11-0d57` command branch also retains the original all-command consumption
rule, notification-independent IDs 1/7, and control-13 notification 0-or-1 split
for rent groups 0..5 versus Movie group 10. Exact headless plans cover both full
filters and the 14-suite Release run remains green without opening a window or
audio backend.

The subsequent full modal-table pass covers Rename Person/Tenant, Movie
Choice, Find, About, Elevator Popup/Control, the command selector, New/Load,
Finance, and AHOTTA. Native-only WM_CLOSE routes were removed wherever absent
from the recovered tables; command notification handling, dialog results,
capture/clip cleanup, DS:31a4 order, About's any-ID timer/FALSE return and
palette brush, Finance's common FALSE input/paint tail, Startup's all-command
consumption, and AHOTTA's timer/control-color translations now match their
literal disassembly branches. The exact message sets and pure routing plans
are headless-tested. Coverage remains 911 exact recovered starts, now with
1,409 unique native-source citations and 455 test citations; 554 mapped starts
have no direct test citation. All 14 Release suites pass.

The current in-progress one-file artifact is 10,315,235 bytes with SHA-256
`AE3A5A4B099D2834E5B0FAF57DF04209B812187DB46C490E43C608FC84ED8242`.
It is byte-identical to the tested Release build, imports only Windows/UCRT
DLLs, and passes the ASCII/UTF-16LE original-runtime-reference scan. It was not
executed. Visual, interaction, and audio side-by-side conformance remains
unverified and still requires explicit permission before either game is
launched.

The startup-title follow-up distinguishes SETUPSTARTUPDLGA `1010:014c` from
SETUPSTARTUPDLGB `1010:0304`: the modal filter closes on left-button-down with
result zero, the modeless filter handles WM_DESTROY instead, and both realize
the logical palette before their black fill and DIB draw. Their literal
message sets and dismissal result are headless-tested.

The Elevator Control teardown now follows ELVDLOGMAIN `1098:0ece-0f63`
literally: resume the isolated Elevator, clear the published dialog handle,
re-enable Map then Command then Info then Main, and only then DestroyWindow.
There is no native-only SetActiveWindow(Main), WM_DESTROY, or WM_NCDESTROY
branch. A pure close-plan test guards that order.

The adjacent modal-lifecycle audit removes synthetic destroy handlers from the
remaining recovered dialog filters. Original gray control backgrounds are the
shared `DS:31ae` brush; native per-modal brush ownership is now released after
DialogBox returns, outside the original message tables. Rename Person retains
its distinct recovered command-time brush deletion and capture-release tail.
All 14 headless Release suites pass after the change, and the packaged PE is
the byte-identical tested build at the refreshed hash above. No game,
emulator, GUI, media player, or audio backend was launched.

The complete `1128:01d9-0223` startup-show tail is now explicitly modeled.
The class strings at `1128:05ba/05c8/05da` prove that DS:325a/325c/325e are
Command/Info/Map, so the exact order is Map, Info, `1050:03aa` command-surface
acquisition, Command, Main, Arrow selection, conditional `1058:033c`, and
unconditional `1080:05a1`. Native no longer validates BITMAP/300..302 before
the windows exist or forces a Main paint where `1080:05a1` synchronously
presents only Command. This identity audit also corrected Elevator Control's
close restoration to Map, Command, Info, Main. Both plans are headless-tested.

The adjacent `1258:0345`/`1128:05eb-09ea` startup-construction audit now
preserves the literal Command, Info, Map creation order and the original
post-create host transaction. Command is immediately TOPMOST and, because its
`SetWindowPos` flags are `0x000a`, its border-inflated creation extent is
replaced by the raw 63x100 outer size. Info and Map use `0x000b` and keep their
inflated client extents. Native also publishes the recovered top-level IDs
1000..1002 and initializes the logical palette, nine-pixel font,
`TA_UPDATECP`, and transparent background on each auxiliary DC and Main's DC
before first paint. Command's WNDCLASS retains the original empty non-NULL
menu name. Exact pure specs cover classic and scaled border metrics; all 14
Release suites pass without launching a game or audio path.

The complete `1128:1139-1306` preflight now runs before resource or window
creation. Native captures the system-font ascent, performs the exact memory
gate, then evaluates 8-bpp and all four required raster operations as separate
Yes/No warnings. TrueType support and enabled state use their recovered
mandatory-abort messages; wave device zero is probed afterward and an accepted
failure alone clears sound availability before `11c8:006b`'s backend init.
Finally, a missing legacy `[Extensions] tdt` value receives the original
`<module filename> ^.tdt` registration. Ordered pure capability arrays and
byte-exact messages are tested headlessly; all 14 Release suites pass.

The `1128:03ad-0542` startup profile now also preserves the original
`[Paths] Save` buffer and its two direct common-dialog consumers. Open and
Save As both pass the same fixed 0x80-capacity, non-null initial-directory
buffer recovered at `10d0:01a4-01ab/0462-046b`. External original INI files
retain precedence; the self-contained build otherwise embeds the supplied
installation's `C:\Maxis\Simtower` value.

The adjoining `1128:003a-00ce` launch order now performs the original
document-inactive `10d0:086c/0ac2` bootstrap after the second splash and before
the startup sound/dialog. Its frame `0x09e5` changes the shared startup palette
to CLUT/1002 while construction remains suppressed; repeated Wait-cursor and
class-cursor clearing around both splash phases are also restored.

The `1128:046c`/`11f8:0e21-0e67` follow-up retains the exact BeepOnly latch
instead of discarding it after profile parsing. Successful construction now
clears the status, issues the original system beep when `BeepOnly == 1`, then
applies the independent WAVE/7000 type exclusion. Single-click and captured
drag success tails share that ordering, with captured commands beeping only
once on release. The pure audio plan is headless-tested; no beep or audio device
was activated during verification.

The remainder of `10d0:0ac2` now also synchronizes the reconstructed fire-crew
menu state. Command 40008 is enabled only for b406 bit three with b418 equal to
zero, and is grayed after every other New/Open/bootstrap reconstruction. This
removes stale menu state when opening an active-fire save and preserves the
fresh-tower disabled default. The three exact predicate states are tested
headlessly.

The complete caller audit of `1140:010d` now preserves its argument-dependent
command reset. `10d0:0ac2` passes zero during New/Open reconstruction, whereas
both rating-promotion callers pass one and therefore set command mode two
before the synchronous `1080:05a1` presentation. Native no longer leaves the
old construction tool selected after a promotion. It also keeps the active
TABL handle separate from Open's pre-I/O temporary rating, restores and clamps
the loaded view before `1080:055d`'s synchronous Map composition, preserves the
prior view on New as the original b3f0/b3f2 globals do, and runs the caller's
Main rebuild afterward. The preview-restore isolation gate and all zero/nonzero
argument branches are headless-tested; the full 14-suite run passes.

The core elevator-construction dispatcher at `11f8:0fea` now distinguishes a
new shaft from an added car at its UI boundary. Existing-shaft success bypasses
`140d` and leaves the selected elevator command active; new-shaft success alone
selects Finger mode and synchronously composes Command through `1080:05a1`.
Native formerly applied the mode change to both paths and deferred the paint.
The pure construction result exposes the recovered distinction, zero-cost
mutations still mark the document changed, and both branches are guarded by
headless assertions. All 14 suites pass.

The complete `1220:0000` nightly person-family switch now preserves the signed
tenant-status comparison at `01ba-021b`. Hotel and Condo records whose owner
status has its high bit set take state `0x10`, matching signed `JGE`, instead of
the unsigned-native `0x20` result. A direct recovered-start regression covers
every live family, both tail shapes, threshold boundaries, and high-bit states.
All 14 suites pass headlessly.

The shared WAVMIX pump at `11e0:0e84` now runs at every translated
`1208:05e6` time boundary. Its pure plan retains the unsigned 48-ms gate,
advances a late rolling anchor by one interval per call, handles tick-count
wraparound, and performs backend work only while at least one of Elevator,
Events, or Background is enabled. On native waveOut, reaping `WHDR_DONE`
headers is the transport-equivalent operation to the original drain of message
`0x03bd` followed by `WaveMixPump`. Boundary and wrap cases are directly
headless-tested; no window or audio device was opened.

The direct `1050:0000` Command-window pass also corrects two ordering details.
WM_PAINT now selects and realizes the logical palette unconditionally before
the three content gates, unlike the gated Info/Map painters. Every non-close
button-down samples `1208:05e6` only after its pressed-toggle or point action,
stores the shared DS:31b0/31b2-equivalent value, and therefore enters the audio
pump at the same boundary. The literal eleven-message set and both branches
are covered headlessly.

The adjoining direct `1168:0000/02be` Map pass restores all four painter and
three successful-drag `11e0:0e84` checkpoints, plus the Info painter's single
checkpoint. A drag now XOR-erases the old focus, installs/clamps the view,
XOR-draws the new focus, and only then presents Main, rather than repainting
Main before an asynchronous Map invalidation. DS:0248 is set for every
non-close Map button-down and cleared with DS:3216 on every button-up. The
literal thirteen-message set—including its explicit DefWindowProc WM_COMMAND
entry—is directly tested without opening a window.

The direct `1100:4869` Facility Information preview-crop comparison now
preserves its literal unclamped source width. A zero-width ordinary tenant
stays zero-width, and types 6/10/12 always take DS:74ba[source_type] times
eight even when that table entry is zero; `1100:4514`'s separate minimum is a
temporary-backing allocation rule. Both distinctions are headless-tested.
All 14 suites pass, with 456 direct recovered-address test citations and 553
mapped starts still lacking a direct test citation. The refreshed standalone
PE matches the tested build and passes the static dependency and forbidden-
runtime-reference scans without being launched.

The direct `1158:06b9` command-dispatch pass covers all 27 literal table
entries and both default ranges. Non-table IDs 3000..4001 run the generic
event dialog before DefWindowProc; other unknown IDs use DefWindowProc alone.
Hidden command 9003 now follows `0946 -> 0ba8` through a direct Main DC/backing
blit rather than an invented WM_PAINT invalidation. All 14 headless suites
pass; coverage is 1,410 source citations and 457 test citations, leaving 552
mapped starts without direct test evidence. The refreshed package is
byte-identical to the tested build and passes the static independence and
zero-task-process checks without execution.

The Facility Information lifecycle now follows `1100:03ac/4439`: it renders
one preview backing before `11c8:03fb` sound selection and modal entry, then
stretches that retained snapshot for repaint. The previously absent clicked-
facility sound request is wired, and `11c8:07d2` commercial sound lookup now
uses the linked 18-byte Retail record at DS:b7e2 rather than an unrelated
16-byte person record. Contradictory-table, master-sound, fixed-resource, and
conditional-RNG cases are headless-tested. All 14 suites pass; coverage is
1,411 source citations and 459 test citations, leaving 551 mapped starts
without direct test evidence. The refreshed package is byte-identical to the
tested build and passes the independence and zero-task-process scans without
execution or audio-device access.

Direct `1100:4514` coverage now preserves its independent temporary-backing
counts: Movie's 31-cell minimum, the exact 2-floor and 5-floor type families,
signed ordinary tenant spans, and DS:7782/7784 mirrors. Facility Information
renders that expanded backing once but stretches only `1100:4869`'s unclamped
crop. All 14 headless suites pass; coverage is 1,411 source citations and 460
test citations, leaving 550 mapped starts without direct test evidence. The
refreshed package remains byte-identical to the tested build and passes the
static independence and zero-task-process checks without execution.

The direct `10e0:0042` Find-person dispatcher comparison corrected both
special focus paths that diverged from the recovered control flow.
`10e0:0bc6` now centers Movie/Party destinations from dc24 byte 7 (paired 15-
cell or single-sided 12-cell half-width), not from the destination tenant's
type table. Housekeeping state two now resolves the assigned Hotel room from
person byte 6/word 12, checks serialized tenant type +4 and first-person dword
+8 through `1220:6ba9`, and focuses that room only when its first guest is in
state 3; all other gates report the person's byte-7 Lobby. All 14 headless
suites pass. Coverage is 1,413 source citations and 462 test citations,
leaving 548 mapped starts without direct evidence. The refreshed package is
byte-identical to the tested build, imports only Windows/UCRT DLLs, and passes
the forbidden runtime-reference and zero-task-process scans without execution.

Direct static coverage now also includes `10f8:0c06`'s complete fire-responder
edge/sentinel split and `1220:16ab`'s signed -1..40 family table. Both already
matched the recovered code; new regressions distinguish all-active versus
first-active six-floor scans and confirm that Security is absent from `16ab`
but present in `1210:0883`'s separate Elevator-car callback. The connected
`1018:0067` New/Load audit corrected its unknown-command return: only IDs 1..3
close and consume the modal, while other WM_COMMAND IDs return FALSE through
`01ee`. All 14 headless suites pass. Coverage is 1,414 source citations and
465 test citations, leaving 545 mapped starts without direct evidence. The
refreshed package is byte-identical to the tested build, imports only
Windows/UCRT DLLs, and passes the forbidden runtime-reference and zero-task-
process scans without execution.

The next static audit completed `1220:6383` Housekeeping and
`11a8:07d3` commercial-service activation. Housekeeping's complete five-state
jump table, six-floor room partition, upward/downward scan order, three-tick
cleaning countdown, Hotel guest transitions, Stair release, 1500-tick cutoff,
and `1220:6297` wrapper gates already matched the recovered control flow and
now have direct regressions. The service allocator audit found one gameplay
accounting omission: phase-zero Fast Food activated after frame `0x00f0`
started with ten customers in its service record but did not pass that value
to `1060:07f7`. Native activation now immediately adds ten to Fast Food's
category and total population exactly as `11a8:09b4-09f4` does. All 14
headless suites pass. Coverage is 1,415 source citations and 476 test
citations, leaving 536 mapped starts without direct evidence. The refreshed
10,317,622-byte PE is byte-identical to the tested build at SHA-256
`9F6871522B3F9216B02775ED3B498B84CCD008469F4532E05F38147F7351588D`. It was
not executed and no audio device was opened.

The next static parity batch completed Metro `1220:5227` and corrected both
`10f8:033d` event priority and auxiliary-palette activation repaint behavior.
Combined Bomb+Fire flags now take the original Bomb branch first, so SECOM
keeps responders one through five idle. Command, Info, and Map now validate
only client rows 0..7 at `1050:008d`, `1120:007d`, and `1168:009f` instead of
cancelling a pending repaint for the entire client. Direct regressions also
cover `10c0:0983`, `1090:06fb`, `1130:06e9`, `1220:2068`, `1208:002c/0cb5`,
and pixel-level `1118:045d` date rendering. All 14 headless suites pass.
Coverage is 1,416 source citations and 495 direct test citations, leaving 517
mapped starts without direct evidence. The refreshed 10,317,622-byte PE is
byte-identical to the tested build at SHA-256
`8A2910A0EA889BB818806D2F96B72B6B683BD322ED8767F0AB4AC8D353D3F1B2`,
retains only Windows/UCRT imports, and passes the forbidden runtime-reference
scan without execution or audio-device access.

The following static parity batch corrected four person-presentation and
scheduling divergences. Condo wrapper `1220:38e1` now uses the recovered state
4/10/20/21/22 tables, including resident ordinal rather than tenant key and the
strict frame thresholds. Hotel wrapper `1220:2e92` now performs state-10 phases
zero through four unconditionally. Lobby wait discounts in `11d8:0423` and
Elevator isolation `1210:1332` now compare the wrapping elapsed word as signed,
as do the visible wait-color bands reached from `10a8:12c1`. Simulate-mode
waiting-person rendering now obtains identity from the saved `b3ae` Elevator
snapshot while reading the projected signed wait metric from the live ring, so
metrics cannot be misread as person IDs. Direct regressions also pin the full
`1020:053e` two-phase special palette, `1220:6037` Cathedral paths,
`11a8:02f2` commercial reset, `1220:067c` activated-person initialization,
`1208:0cb5/0d75` resource lifetime, `1208:0004` formatting, and
`11e0:04c0/05d7/06d9` GDI cleanup/frame behavior. All 14 headless Release
suites pass. Coverage is 1,421 source citations and 511 direct test citations,
leaving 503 mapped starts without direct evidence. The refreshed 10,319,355-byte
PE is byte-identical to the tested build at SHA-256
`A0AC84299E8BDEF0676FDD4EB79155819779720F465C842BE98CD47BFF77C14B`, retains
only Windows/UCRT imports, and passes the forbidden runtime-reference scan
without execution or audio-device access.

The latest static comparison found and repaired a loaded-Elevator state
divergence in `1090:0192`. Every initialized car now duplicates its home floor
into byte 13, obtains byte 14 from the current `0x20 + calendar*7 + day-phase`
schedule bank (indices 28..41), and preserves byte 15 when the original caller
passes -1 during loaded-state reset or full-shaft removal. Newly added cars
snapshot the current schedule mode and explicitly activate, while new shafts
still activate only car zero. Direct regressions now also anchor the complete
`11a0:0eaf` waiting-person compositor, `11b0:11af` Elevator routing,
`1130:03f4` Facility Information performance, `1180:0352` Movie activation,
`1098:1ff5` Elevator Control input, `11a0:027c` opaque type-16 atlas copier,
and `10a8:0de6` transfer-cache painter. Exhaustive `1130:0cec` tests verify all
six Office, three Condo, 48 Retail, and Hotel one/two/two guest records while
preserving each Hotel owner record. All 14 headless Release suites pass.
Coverage remains 911 exact native mappings and 1,421 source citations, with
520 direct test citations and 494 mapped starts lacking direct test evidence.
The refreshed 10,319,867-byte PE is byte-identical to the tested build at
SHA-256 `6E84F57896ED0FAAC358BD32EE37A2C946EDF8C7EFB126767A73A313AE7AF469`,
imports only Windows/UCRT DLLs, and passes the forbidden runtime-reference and
zero-task-process scans. It was not executed and no audio device was opened.

The current static parity batch directly audited `1118:0143`'s Info balance
field and checkout-sound latch, `10a0:0544`'s captured Elevator Finger input,
`1180:090a`'s entertainment completion phase, `11b0:0fa5`'s route selector,
and both decoded jump tables in `11f8:2f5a`'s construction floor gate. Two
construction divergences were corrected: raw type 19 no longer bypasses the
original ground-floor status-12 rejection, and paired upper types 18/20/29 now
reject the one-story extension immediately below persisted `b3e8` with status
14. The Elevator Finger double-click miss path also preserves the captured
press latch until the subsequent button-up, while still releasing Windows
capture and restoring Cursor/1004 exactly as the original does. All 14
headless Release suites pass. Coverage remains 911 exact native mappings and
rises to 1,422 source citations and 526 direct test citations, leaving 488
mapped starts without direct test evidence. The refreshed 10,319,867-byte PE is
byte-identical to the tested build at SHA-256
`DAD2934E9D269E246B6AC10237E001CEB4BB38306DF570A79F138E63B6F085CA`, imports
only Windows/UCRT DLLs, and contains none of the forbidden ASCII or UTF-16LE
runtime references. It was not executed and no audio device was opened.

A follow-on visible-field audit directly covered `1128:13fc`, `1118:026a`,
and `1118:073d`. The Info backing and clock compositor matched, including the
431x41 BITMAP/320 surface, clipped 31x31 clock face, time-indexed hand vectors,
and cosmetic black pen. The status-field raster test exposed a genuine GDI
ordering omission: native selected `TA_UPDATECP | TA_BASELINE` but did not run
the original `MoveTo(left+2, bottom)` before `DrawText`, allowing the message to
inherit the balance painter's current position. The recovered MoveTo is now
restored and the text is pixel-tested inside the exact 262x11 field with its
white lower edge. All 14 headless Release suites pass. Coverage is now 911
exact native mappings, 1,423 source citations, and 530 direct test citations,
leaving 484 mapped starts without direct evidence. The refreshed 10,319,867-
byte PE is byte-identical to the tested build at SHA-256
`1C391E62DF501D64570FC37FA940125896F71DC8DA95EBDB1BDB615A5C11E337`, imports
only Windows/UCRT DLLs, and passes the forbidden runtime-reference and zero-
task-process scans. It was not executed and no audio device was opened.

The complete `10d0:03f1` Save As comparison found a path-semantics mismatch.
The original `1000:11e8/1394/1408` helper sequence treats the selected path as
one string: it replaces from the last dot anywhere in that string, then uses
the first dot and last backslash for its signed DOS-basename length check.
Native's path-aware extension/stem handling has been replaced with those exact
whole-string rules. Regressions pin ordinary names, multi-dot names, dotted
directories, the eight-character threshold, and the original signed-negative
edge case. All 14 headless Release suites pass. Coverage is now 911 exact
native mappings, 1,425 source citations, and 531 direct test citations, leaving
483 mapped starts without direct evidence. The refreshed 10,320,279-byte PE is
byte-identical to the tested build at SHA-256
`53B2CA2BBCD55C4434D9DF1A8A7DC99B189B43AA0CEC58D3F7F94DEB40E94CBE`, retains
only Windows/UCRT imports, and passes the forbidden runtime-reference and
zero-task-process scans. It was not executed and no audio device was opened.

The next observable-helper batch directly audits `1118:0044/0368`,
`1200:0037/058d`, `1098:1644`, `1100:0644`, `10a0:1625`, and
`11c8:0135/02c0`. The existing translations matched the recovered branches.
New memory-DC regressions constrain rating changes to the five clipped star
cells or special BITMAP/327 strip and population changes to the exact 86x14
right-aligned field. Boundary tests cover all seven clock phases and hand
quadrants, the inverted Elevator Control grid, all six four-string rent groups,
up-before-down waiting-ring drain order including the service-Elevator delay
exception, and the fixed two-channel stop loop without opening audio. All 14
headless Release suites pass. Coverage remains 911 exact native mappings and
1,425 source citations; direct test citations rise to 540, leaving 474 mapped
starts without direct evidence. The refreshed 10,320,927-byte PE is byte-
identical to the tested build at SHA-256
`622A65BA8C1BEF9A4FEC45CA5674ACE3FEF6E23035421E51CA909B225BE80C6B`, retains
only Windows/UCRT imports, and passes the forbidden runtime-reference and
zero-task-process scans. It was not executed and no audio device was opened.

The next static parity batch audits the full recovered control flow at
`10a0:10e8`, `1198:07e6`, `1210:0f0e`, `1228:0b59`, `1100:2ec2`, and
`1100:307e`. Two signed persisted-state divergences are fixed. The new-Elevator
Stair collision now reproduces `CBW` plus arithmetic `SAR AX,1`, so high-bit
shape bytes no longer become large unsigned obstructions. Cathedral facility
information now applies the original signed `JGE` gate to its anchor-state
word, rejecting high-bit malformed values instead of rendering occupants.
Parking rebuild, Elevator boarding/routing, evening facility transitions, and
Movie lineup geometry otherwise match their recovered routines. Direct tests
cover all six starts, both corrections, the two DTMP lineup rows, and the
relevant derived-state mutations. All 14 headless Release suites pass.
Coverage remains 911 exact native mappings; source citations rise to 1,427 and
direct test citations to 546, leaving 468 mapped starts without direct
evidence. The refreshed 10,320,927-byte PE is byte-identical to the tested
build at SHA-256
`C985E9AECC602D65D32232BCE29C52F59D5E370BB8B5E865307636B53BE14897`, retains
only Windows/UCRT imports, and passes the forbidden runtime-reference scan. It
was not executed and no audio device was opened.

The latest static audit directly compares `1090:1d2f`, `11c8:06b6`,
`1160:0000`, `1228:0968`, and `1100:3856`. Elevator direction arbitration,
facility sound-event selection, and Map backing initialization already
matched. Medical Center day/evening sweeps now preserve tenant `+0x0c` while
marking the record dirty, and the type-7 Person portrait selector now applies
the original signed final comparison for high-bit persisted state. Direct
regressions cover all five routines. All 14 headless Release suites pass;
coverage is 911 exact mappings, 1,429 source citations, and 553 direct test
citations, with 463 mapped starts still lacking direct evidence. The packaged
10,320,927-byte PE matches the tested build at SHA-256
`A9D6ADE5094E68DBC6A89B4BEB096D50FC52B337C954494C84A8CE9FCEA001A7`, retains
only Windows/UCRT imports, and passes the forbidden runtime-reference scan.
Neither game executable nor any audio path was launched.

The newest static batch directly audits `1238:029f`, `1220:426c`,
`1200:0543`, and `1098:0780`. The people-pool clearer and Retail wrapper
already matched, while their shared dependencies exposed two fidelity errors.
The native clock now reproduces `1200:0543`'s signed `CWD`/`IDIV 0x0190`
quotient for high-bit frame words, and all phase consumers retain signed
comparisons. Elevator Control now copies every DS:b3a1 phase byte and changes
only exact six to five, as `1098:0786-0793` does. Direct tests cover a complete
new 256-record pool block, Retail's signed state-five gate, negative phase
quotients, and raw control-dialog phase bytes. All 14 headless Release suites
pass. Coverage is 911 exact mappings, 1,433 source citations, and 558 direct
test citations, leaving 459 mapped starts without direct evidence. The current
10,321,951-byte PE matches the tested build at SHA-256
`049FB38161BAE87E6B3C0FBF471531CC2660D2D73A80027D40E2D3ECA342E27C`, retains
only Windows/UCRT imports, and passes the forbidden runtime-reference and
zero-task-process scans without execution.

The current quiet static batch directly audits `1220:49fa` and `1220:049a`.
The exact owned-person dynamic clear matches and now has a sentinel-filled
activation regression across its complete Condo span. The food-service normal
pass required one correction after the shared signed-clock repair: its Fast
Food random gate now reproduces the disassembly's signed lower and upper phase
bounds (`>= 0 && < 4`) instead of accepting high-bit negative phases. A direct
regression proves that frame `0x8000` neither dispatches nor consumes RNG. All
14 headless Release suites pass. Coverage remains 911 exact mappings and 1,433
source citations; direct test citations rise to 560, leaving 457 mapped starts
without direct evidence. The refreshed 10,321,951-byte package is byte-identical
to the tested build at SHA-256
`57CAE31292266B284518AB2C5000757BC7A547F78465BB4F425AE4E08ED41067` and passes
the static import and forbidden-reference checks without being launched.

The following static batch directly audits `1058:0000`, `1098:1895`,
`11f8:20e7`, `11f8:1452`, `1258:0345`, and `11a0:047c/088f`. Existing
translations of the input dispatcher, Metro/Stair/Escalator construction, and
opaque world-cell copies match. Elevator Control floor-cell choices and the
four startup class records are now explicit pure plans consumed by production
code and headless tests; the latter restores Main's original registered
`TOWER_MENU` class metadata. All 14 Release suites pass. Coverage remains 911
exact mappings and 1,433 source citations; direct test citations rise to 566,
leaving 450 mapped starts without direct evidence. The current 10,323,114-byte
package is byte-identical to the tested build at SHA-256
`D674974C0F671800ECF211C95A1346A61438B30FB858325A52EB33AB85ECAFAD` and passes
the static import, forbidden-reference, and zero-process checks without being
launched.

The latest quiet static batch directly audits `1170:0291`, `1180:06a8`,
`11f8:2291/321e`, `1258:000b/0186`, `1218:08cd`, `1178:0b44`, `1210:11c2`,
`1108:030d`, `1220:55b8`, `1100:1b53`, and `1100:1dca`. Native now restores
the unavailable-Medical waiting metric/finalizer, sign-extends Retail's direct
byte selector before its full-word index comparison, debits full three-day
maintenance without the positive-income cap, and preserves all three Person
Information DS:b3a6 stress-meter contexts with signed Lobby wait adjustment.
The other compared branches match and now have direct headless evidence. All
14 Release suites pass. Coverage remains 911 exact mappings; source citations
rise to 1,434 and direct test citations to 577, leaving 437 mapped starts
without direct evidence. The current 10,323,280-byte package is byte-identical
to the tested build at SHA-256
`F003C81B4CE899FF881A1BAA1D0C1FACAE820BF43E7F42F4FE1CC4A5AA791B19`, retains
only Windows/UCRT imports, and passes the forbidden-reference and zero-process
checks without being launched.

The newest quiet static batch directly audits `11e0:0950`,
`1028:1534/1692`, `1220:5edd`, `11a0:0126/027c`, `1100:2031`, and
`1188:02ea`. Existing bevel, food-service presentation, Cathedral, atlas, and
attendance-meter translations match and now have direct headless evidence.
The TDT loader now reproduces the recovered 256-byte Pascal Find-name import
branch and duplicate-key b402/b404 compaction, including later-name LSTRCPY
overwrite semantics and short-transfer handling. The four supplied saves still
round-trip byte-exactly. All 14 Release suites pass. Coverage remains 911 exact
mappings and 1,434 source citations; direct test citations rise to 585, leaving
429 mapped starts without direct evidence. The current 10,330,051-byte package
is byte-identical to the tested build at SHA-256
`925336B00E8FBA88B1F5E5B134905D2E44C1FB38FD2FB5280E5EE82EFE034FC2`, retains
only Windows/UCRT imports, and passes the forbidden-reference scan without
being launched.

The current quiet static batch directly audits `1090:1f4c`,
`1090:1d2f/13cc`, `1220:50e2`, `1100:248d`, `1108:014b/0a88`,
`10e8:0304`, `1130:0f57`, `10a8:1737/1a88`, `1058:03a9`, `1040:0179`,
`11a8:06b2`, `1220:1518/6d82/6e7d/7005`, `10f0:01f9`, `1098:16a4`,
`1100:22d5/232e`, `1228:07c5`, `10e0:06cd`, `1188:0793/0884`,
`1208:0274`, and `11a0:0a11`. Native now preserves signed high-bit Metro
frame comparisons and the transport advisory's wrapping 16-bit absolute
distance, including its `-32768` overflow. The remaining compared control
flow, naming, cleanup, geometry, span, and compositor paths match and now
have direct headless evidence. All 14 Release suites pass. Coverage remains
911 exact mappings; source citations rise to 1,437 and direct test citations
to 612, leaving 401 mapped starts without direct evidence. The current
10,330,051-byte package is byte-identical to the tested build at SHA-256
`F2917CAC067FF78943D1910465A61E93FB1E2EAD3B98E633A643581690F1DB05`, retains
only Windows/UCRT imports, and passes the forbidden-reference and zero-process
checks without being launched.

The latest quiet static batch corrects the grouped-selector transaction at
`1058:04e0`, including physical-primary-button and swapped-button handling;
the synchronous Main-to-Map-to-Command full-view refresh at `1080:0a02`; and
successful Find's Command-to-Map-to-preview-to-camera sequence at
`10e0:0cea`. Direct tests also anchor the complete embedded DIB loader set
(`1030:0000/0043`), TABL/TABM rating lookup (`1140:022c`), message-box result
jump table (`1208:0369`), and Find-name slot lookup (`1188:04db/0541/05a7`).
All 14 Release/headless suites pass. The ledger now records 911 exact mappings,
1,439 source citations, 622 direct test citations, and 391 mapped starts
without direct evidence. The refreshed 10,329,898-byte package is
byte-identical to the tested build at SHA-256
`BD763042C0685F3F9F6547DB05C5891076515BE329E5DE1D457882D597DA59C5`, retains
only Windows/UCRT imports, and passes the forbidden-reference and zero-process
checks without execution or audio output.

The newest quiet static batch restores New and successful Open's complete
`10d0:001d/062a` presentation tail: Map focus adjustment, Fire Crew menu, Main, Map,
Command, and Info are now presented synchronously in that order. It also
makes `11c8:02c0`'s exact four-gate channel-stop decision production-consumed
and directly tested. The adjacent finance, entertainment-span, person-index,
DTMP ownership, GDI brush/fill, name-table lifetime, and focus helpers at
`10d0:2a8e`, `1178:0854/08ec`, `1180:0f87`, `1240:020d`,
`1070:051f/0570/05a1`, `11e0:0e00/0e22/0e60`, `1208:00dc`,
`1188:007e/01be`, `1060:08be/0958`, and `10e0:078d` now have direct
regression evidence. `1208:0cf5`'s signed 16-bit current-position additions
are consumed by transport-dialog painting, and `1208:0dfc`'s exact
MessageBeep(0x30)-then-FatalAppExit(0) contract replaces the former native
message-box/return-one behavior. All 14 headless suites pass. Coverage is 911
exact mappings, 1,439 source citations, 644 direct test citations, and 368
mapped starts without direct evidence. The 10,329,921-byte standalone package
is byte-identical to the tested build at SHA-256
`5B6EDEE62A70D2D1BA0B2876D09672F50CC964A0845190F2F81D187D48CD43CE`, uses
only Windows/UCRT imports, and passes the forbidden-reference scan without
launching either game or opening audio.

The latest quiet static batch corrects Map's direct-DC focus transaction at
`1080:055d`, the Person Information Find-exit latch at `1100:0000`,
Cathedral saved-name linkage at `1188:0aa0`, signed `-32768` Elevator dwell at
`1090:23a5`, and the zero dialog Flags dwords at `10d0:0122/03f1`. Finance
placement `11e0:00ca`, Person Information meter geometry/logical-palette
brushes `11e0:01d8/0358`, and white-fill lifecycle `1208:05a9` are now exact,
production-consumed helpers with direct regressions. Further direct evidence
covers the command selector, facility-person atlases, Map scaling, name-table
updates, transport queues, schedule banks, DTMP origins, and adjacent
Elevator waiting lanes. All 14 headless suites pass. Coverage remains 911
exact mappings; source citations rise to 1,449 and direct test citations to
706, leaving 310 mapped starts without direct evidence. The refreshed
10,331,176-byte standalone package is byte-identical to the tested build at
SHA-256
`6A1769EBA48EF16B919C651506005E8793C4FF2A220019F3C270A1474267A0F7`, imports
only Windows/UCRT DLLs, and passes the forbidden-reference scan without
launching a game executable or opening audio.

The newest quiet static batch makes the exact `1100:4439` Facility Information
preview painter production-consumed and directly pixel-tested, including its
RGB(204,204,204) backing, `1100:4d1d` geometry, retained snapshot, and GDI
state restoration. `1020:0f4f` now exposes a shared tested 256-entry native
`HPALETTE` constructor. Comparison of Elevator Control's `1098:27bd/2893`
value painters found and fixed a signedness error: their persisted schedule
bytes are CBW-sign-extended before display and departure multiplication.
Direct evidence also covers `1118:0ce7`, `1180:0826`, `1178:0a6a`,
`10a8:1582/165d`, `11f0:0211`, `1180:01ad/0282`, and `11a0:0c01`. All 14
headless suites pass. Coverage remains 911 exact mappings and 1,449 source
citations; direct test citations rise to 720, leaving 297 mapped starts
without direct evidence. The refreshed 10,332,533-byte standalone package
matches the tested build at SHA-256
`B2EF681E432BC2F37567665CE220C67B2CF01A34FBCAC8ED0ACE23598B095A85`, imports
only Windows/UCRT DLLs, and passes the forbidden-reference and exact-name
process scans without execution or audio output.

The latest quiet static checkpoint exposes and directly tests the exact Office
occupancy, Stair span/scoring/transfer routing, Lobby graphics-tier, Movie
length, commercial closed-hours, route-boundary, and event-floor cores. It
also adds direct state/pixel evidence for retained presentation, file transfer,
accounting, Find ordering, transit queues/advisories, fire follow-up, rating
treasure, Hotel activation, Retail reset, magnifier fallthrough, portrait
geometry, Map drag, and Elevator Control icon selection. Signed arithmetic,
wraparound, first-match ordering, and opposite-endian cases are included. The
ledger contains 911 exact mappings, 1,449 source citations, 816 direct test
citations, and 203 mapped starts without direct evidence. All 14 headless
suites pass; the 10,337,876-byte standalone PE matches the tested build at
SHA-256
`153002F79636081258C329B5270382E95BBBDEA562D6362F6776E8C556D76853`, imports
only Windows/UCRT DLLs, and passes the forbidden-reference and exact-name
process checks without launch or audio output.

The current quiet static checkpoint makes the recovered parking
assignment/floor/eligibility rules, commercial lane/capacity/revenue rules and
paired close scans, signed Win16 dialog centering, About line style/text
reader, and reserved-audio submit gate explicit production boundaries. Direct
tests now additionally anchor the Medical route-bank reset, single-range
construction debit, saved-name prefixes, first-free parking allocation,
command-grid geometry, all fifteen Elevator Control row/cell plans, transit
Find geometry, Map overlays, meter thresholds, Elevator car composition, and
the exterior roof/edge pass. All 14 headless suites pass. Coverage remains 911
exact mappings and 1,449 source citations; direct test citations rise to 750,
leaving 267 mapped starts without direct evidence. The refreshed
10,333,795-byte standalone package matches the tested build at SHA-256
`5252EC826F35122FF476180427497B5422311B1D09A394BF606CDFC9AC2E13C3`, imports
only Windows/UCRT DLLs, and passes the forbidden-reference and exact-name
process scans without launching a game executable or opening audio.

The newest quiet static checkpoint adds direct evidence for the exterior/world
helpers, transient facility state, parking admission, vertical summaries and
transport reset, fresh-world sentinels, clock/star raster decisions, people
accounting and route selection, Find shaft resolution, information thresholds,
disabled-audio channel behavior, annual and periodic transitions, command and
Elevator Control presentation plans, resource startup, rename/save policy,
ALRT preparation, and the exact Arial enumeration predicate. All 14 headless
suites pass. The current ledger contains 911 exact native mappings, 1,449
source citations, and 979 direct test citations; 869 of the 911 mapped routine
starts have direct test evidence, leaving 42 host/runtime-boundary starts
without a direct test citation. The remaining 264 recovered candidates are the
already classified compiler/runtime support set, not unmapped game-owned
routines. The refreshed 10,338,821-byte standalone PE is byte-identical to the
tested build at SHA-256
`F4DD08CFEFDD55E40AC80D630B1C8ED99744B8015AED4105C78F13256F6B6AAE`, imports
only Windows/UCRT DLLs, and passes the forbidden ASCII/UTF-16LE runtime-reference
and exact-name process scans. It was not executed and no audio device was
opened. Runtime side-by-side visual, interaction, timing, and audio conformance
remains pending explicit permission and is still required before completion.

The following headless host-boundary pass makes `10c8:01f7` Bomb and
`10e8:025a` Fire orchestration production-consumed ordered plans, preserving
their opposite sound/damage order plus Security, focus, menu, modal, and
deferred-completion tails. The complete `11f8:0793` client-point-to-facility
demolition wrapper is likewise shared with the native host and tested for hit,
miss, mutation, and sound request behavior. Direct anchors now also cover
`1118:08f3`, `1178:0697`, `11c8:09d2`, `10d0:0604`, `10f0:0121`,
`11a0:134c`, and `1208:0b6a`, including wrapped floor-offset arithmetic and a
real but non-visible GDI font-bank teardown/reinitialization cycle. All 14
headless suites pass. Coverage remains 911 exact native mappings and 1,449
source citations; direct test citations rise to 991, covering 879 mapped
starts and leaving 32 individually classified platform boundaries in
`HEADLESS_BOUNDARY_AUDIT.md`. The refreshed 10,339,643-byte standalone PE is
byte-identical to the tested build at SHA-256
`6CA9465496879E58DD44382E6B06F06EB30570FBA862193DD967C25589DC4D12`, imports
only Windows/UCRT DLLs, and passes the forbidden-reference and exact-name
process scans. It was not executed and no audio device was opened.

The latest static presentation audit directly compares `1058:05f8`,
`1038:002f`, `1010:0018/00fd/014c/0304`, `10c8:006e`, `1148:020f`, and the
construction completion callers. Scrollbar changes now rebuild Main
synchronously and perform only the recovered Map-focus XOR transaction,
instead of repainting the whole Map window and entering its unrelated paint
and audio-pump work. Construction, demolition, and Elevator mutations now
dirty only Main; balance changes retain their immediate direct Info paint;
Map remains exclusively on `1080:09c3`'s independent sixteen-tick cadence.
Bomb ransom no longer becomes a full-world mutation, while initial-balance and
buried-treasure credits repaint Info after their original sound/dialog
boundaries. The splash transaction already matched and required no code
change. All 14 headless suites pass. Coverage is 911 exact mappings, 1,452
source citations, and 993 direct test citations; 881 mapped starts have direct
evidence and the remaining 30 are classified in
`HEADLESS_BOUNDARY_AUDIT.md`. The refreshed 10,339,229-byte standalone PE is
byte-identical to the tested build at SHA-256
`9B70620BB389CA650B0527E3D4DD20A16B154F45AD5106559F4F6DB8D0ECDDAC`, imports
only Windows/UCRT DLLs, and passes the forbidden-reference and exact-name
process scans. It was not executed and no audio device was opened.

The subsequent launcher-lifecycle audit corrects two quiet ownership/order
differences. `1010:049e` now stops both channels once and uses direct native
counterparts for its two `WAVEMIXACTIVATE` calls, preserving the original
`11c8` active latch across About instead of entering the stop-and-clear
`11c8:0add` wrapper. `1018:01d3` now releases the parsed DTMP value before
`EndDialog`, matching `1070:051f`'s HWND-slot release order, and `1010:00fd`
clears the retained native splash state after destroying its window. All 14
headless suites pass. Coverage remains 911 exact mappings and 1,452 source
citations; direct test citations rise to 994, so 882 mapped starts have direct
evidence and 29 platform boundaries remain classified in
`HEADLESS_BOUNDARY_AUDIT.md`. The refreshed 10,339,389-byte standalone PE is
byte-identical to the tested build at SHA-256
`9EBB0962CD9E595E6F7A172869CED6EB11100BE4BC40735A22D47B2513B10217`, imports
only Windows/UCRT DLLs, and passes the forbidden-reference and exact-name
process scans. It was not executed and no audio device was opened.

The latest quiet boundary-completion pass restores several exact Win16 host
contracts that exposed real native differences. Person and Tenant Rename are
again owned by Main; Movie Choice preserves the caller owner and actual modal
return; all affected DTMP values release before `EndDialog`; Elevator Control
receives the initiating pointer coordinates and relies on its visible dialog
template without synthetic post-create Show/Update calls. New/Load, Finance,
Find, Movie, Rename, Elevator Control, popup rectangle conversion, Find-list
selection, and failed-save deletion now consume recovered launcher or boundary
contracts. The shared `11e0:0000` dialog getter again caps reads at `0xfe`
characters. Startup precompute order, the no-inbound wrapping POINT subtract,
and process teardown/storage ownership are explicit and tested; shutdown now
releases font, cursor, audio, palette, and tower storage in recovered order.
All 14 headless suites pass. Coverage is 911 exact mappings, 1,452 source
citations, and 1,019 direct test citations; 907 mapped starts have direct
evidence and only four genuine process/audio/diagnostic boundaries remain in
`HEADLESS_BOUNDARY_AUDIT.md`. The refreshed 10,341,922-byte standalone PE is
byte-identical to the tested build at SHA-256
`4EC0ADB9F5774A5D256DB400B77FECA915BA9AFE892A617A33943E69BA9B061D`, imports
only Windows/UCRT DLLs, and passes the forbidden-reference and exact-name
process scans. It was not executed and no audio device was opened. Runtime
side-by-side conformance remained required at that checkpoint.

The explicitly permitted muted runtime pass supersedes that earlier static-only
checkpoint. A fresh original startup and New Tower state were captured from the
supplied runnable image, then matched against the native release candidate.
The audit found and removed the visible Windows 11 host substitutions: Main now
uses the original centered system/down/up Win16 caption over classic menu and
fixed-thumb scrollbar chrome; Open/Save now uses COMDLG32's legacy path,
reflowed to the reference 470x247 geometry with the original labels/font and no
later Network control. The exact final build completed New, Open, Cancel, and
clean exit with host audio disabled. All 14 suites pass. The packaged
10,352,757-byte PE is byte-identical to the tested build at SHA-256
`9972D3FCA066B838ABCDF56D0B18DAB22827E29ADB4FCE1E0054C59E291B0CD1`, retains
911/911 exact game-owned mappings and 907 directly cited mapped starts, imports
only Windows/UCRT DLLs, passes the forbidden-reference scan, and leaves no
emulator/game/media process running. Audible PCM submission and longer
interactive timing are the remaining runtime-certification areas.

The reciprocal save-compatibility pass closes the prior original-acceptance
gate. A native-writer fixture produced the 65,150-byte `NATIVE.TDT` at SHA-256
`C99E96B40A66329EEB9B38D3478F3BB77DFC6405DE31C854EB21B731AA706F76`
with raw balance `2,345,678`. The supplied original opened it as
`SimTower - NATIVE`, displayed `$234567800`, and rendered the tower from a
disposable reference image; the base image stayed byte-identical. Reciprocal
native opening found and fixed modern Windows' quoted association-tail edge
without changing the recovered Win16 target logic. All 14 suites pass. The
refreshed 10,354,395-byte standalone PE is byte-identical to the tested build
at SHA-256
`6715E198FA7F6E0089FD7BB606BC1FD7987C864DF147166D443A6AB49CAAB6E2`,
imports only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs,
passes the forbidden ASCII/UTF-16LE scan, and leaves no emulator/game/media
process running. Audible PCM submission and longer interactive timing remain
the runtime-certification areas.

The final silent runtime checkpoint closes both of those areas. A hidden,
audio-disabled `NATIVE.TDT` run stayed responsive for 90 seconds with private
memory fixed at 6,586,368 bytes and GDI/USER objects fixed at 26/30. The
separate hardware PCM smoke then exercised production initialization plus
`waveOutOpen`, `waveOutPrepareHeader`, `waveOutWrite`, active-channel
observation, reset, unprepare, and close for WAVE/20000: 50,300 bytes at
11,127 Hz and 8-bit reached channel zero. That explicit smoke used digital
silence of the same format and length, so it made no audible output; the normal
branch still points to the exact embedded source samples. Coverage is now
911/911 exact mappings, 1,452 source citations, 1,021 test citations, and
909 directly cited mapped starts; only GUI process entry and the unreachable
no-inbound diagnostic remain outside direct test citations. All 14 deterministic
suites and the manual PCM smoke pass. The subsequent reference-clock audit
measured approximately 777 frames per 45 seconds, fixed native Fast Mode's
unbounded host loop with the 58-ms adapter, and passed isolated native start
and 90-second date captures. The final 10,355,049-byte standalone PE is
byte-identical to the tested build at SHA-256
`8D9AEE8B5D3016F3C3D8ACC8578A21602953C113CF1B0572F4F80AD521450316`,
imports only COMDLG32, GDI32, KERNEL32, USER32, WINMM, and UCRT API-set DLLs,
passes the forbidden-reference scan, and leaves no emulator/game/media process
running.

The latest headless renderer and startup audit fixes the reported white
construction gaps without changing the recovered construction state machine.
Type-zero Floor spans now use `11f8:033a`'s BITMAP/1000..1003 mapping and
`1038:06ad -> 11a0:0000`'s fixed status-cell repetition; BITMAP/3944..3949 is
reserved for type 46 fire. Type-47 damaged records render their bottom-aligned
BITMAP/4008 rubble. A direct integration test proves that demolition produces
type-zero/status-two, Floor can be rebuilt on the same span, and an Office can
then split and occupy it. The live startup splash is also the Win32 modal owner
of New Tower/Load Tower, preventing the splash from covering and disabling the
chooser while the pure recovered owner remains Main. All 14 Release suites
pass. The 10,359,510-byte standalone PE is byte-identical to the tested build
at SHA-256
`628D8933CD25C8E3A76425C9F10A5BCF57D862AC3DD9BF9245FF216BAEC52701`,
retains only the listed Windows/UCRT imports, and passes both forbidden-string
scans and the exact-name process sweep. No executable or emulator was launched
and no audio device was opened for this correction.

The following complete `1038:00a9/050e/06a8` selector audit found assumptions
that the earlier pixel fixtures had repeated instead of independently proving.
Type-45 Metro boundary spans now draw BITMAP/3880 with their exclusive
absolute-world four-cell phase. Pending-construction and type-47 damage cells
start at tenant-relative zero. Office rendering uses the signed status byte and
full signed variant word, Security always selects frame zero, and Party Hall
types 29/30 follow their linked dc24 state with the exact state-three-or-later
clamp. Contradictory-state and non-aligned pixel fixtures directly cover each
rule. All 14 Release suites pass. The 10,362,070-byte standalone PE matches the
tested build at SHA-256
`1DFB73A2D19F83D76CF348E722C6D8C4C7AF136B6E871CA4F26C074BC72F4B79`,
retains only Windows/UCRT imports, and passes both forbidden-reference scans
and the exact-name process sweep. Neither game nor an emulator was launched.

The construction/edit-coordinate audit then restores `11f8:3df4-3e2e`'s
literal vertical snap rule. Lobby is the only tool without the post-snap
twelve-pixel offset; Floor, Office, ordinary facilities, Elevators, and
vertical transports all receive it, including their one-story preview
rectangles. Captured negative-coordinate tests also preserve the resulting
signed-IDIV truncation edge. All 14 Release suites pass. The refreshed
10,362,070-byte PE is byte-identical to the tested build at SHA-256
`5575DA071AEFB549E4EA2BDF4DC87AA454CA3FC163DD9086DA5DCAAC475B1866`.
No game, emulator, visible window, or audio path was launched.

The edit-hit follow-up connects the corrected `11f8:3da4` snap to
`11f8:3d2d/3e3e` and `11f8:0793/35ac` in one client-coordinate regression: the
same visible point builds and activates an Office, bulldozes it into a merged
type-zero/status-two Floor span, and successfully rebuilds an Office there.
This also distinguishes the recovered `11f8:17fd/30ef` behavior: a facility
may be placed away from its neighbor because the original automatically fills
the intervening cells with Floor; it is not an unsupported structural gap.
All 14 Release suites pass and the current package remains the 10,362,070-byte
SHA-256 `5575DA071AEFB549E4EA2BDF4DC87AA454CA3FC163DD9086DA5DCAAC475B1866`
artifact. Nothing graphical or audible was launched.

The `1200:0196` timing follow-up corrects its `0529` tail: after the entry
sample admits a frame, the original waits for every scheduled far call to
return and then samples `1208:05e6` again for DS:776e. Native now performs that
post-dispatch commit before entering the distinct `1090:03ab` frame, so a long
midnight/event modal cannot cause an immediate catch-up frame on close. Direct
tests also preserve the signed overflowing six-tick deadline comparison and
the wrapping `04b3` current-day increment. All 14 Release suites pass; the
current package is 10,362,166 bytes with SHA-256
`3CD22AE1609D1C147B2CA51F20A20247B9E609B8D11BA688924E653FA927F7C5`.
Static scans are clear and no executable or audio path was opened.

The screenshot follow-up found one genuine graphics mismatch in the final
`11c0:0000` exterior pass. Native had copied its five floor-cap, foundation,
and roof-marker fragments opaquely, but every original call passes zero to
`1208:071f`; `1248:0000 -> 1250:0024` skips that source index. Restoring the
mask removes BITMAP/1002's white crane rectangle and the white exterior holes,
with independent pixel coverage for standard, layered ground, and roof cases.
The reported rebuild topology is now also tested after its demolished Office
coalesces with Floor on both sides: reconstructing it splits the merged
type-zero/status-two span and preserves the neighbor. All 14 Release suites
pass. The current 10,362,166-byte package has SHA-256
`91A2F4A5F8BDEF7630EDFD8118284EEE5DEB61C0D9546FF8261F264AE572BF2F`.
Import, forbidden-reference, and exact-name process scans are clear; no
executable, emulator, visible window, or audio path was launched.

The final complete-frame audit restores the remaining observable order inside
`1090:03ab`. Elevator state/passenger work is nested per used shaft, and its
movement-sound, person-family, and `053d` host boundaries are synchronous.
`11f8:3b94/3c13` now carries the DS:025c scratch latch and exact two/three
checkpoint split across preview-only and full passes. The renderer-owned
`056c..05e5` checkpoints run before outline redraw and palette processing;
native RGB palette rematerialization is deliberately separate from the
original DS:31cc gate. The literal direct Main, Command invalidation, direct
Info, status expiry, final palette, and final checkpoint tail is production
consumed and headlessly tested. All 14 optimized Release suites pass. The
current standalone package is 10,369,638 bytes, byte-identical to the tested
build at SHA-256
`23E52DDBE182E9CD9A9D3C4D4210B254E54D6AFCC5216EDCAEE95EF48E4ACA0B`.
Its imports and static forbidden-reference/process scans are clear, and it was
not launched.

## Evidence

- `simtower-callgraph.json` and `NE_CALL_GRAPH.md`
- `original/disassembly/functions/manifest.json`
- `simtower-ne.json`, `simtower-imports.json`, and `simtower-imports.md`
- `CUSTOM_RESOURCES.md`
- `RESOURCE_CONSUMERS.md`
- `SAVE_COMPATIBILITY.md` (native disassembly-backed compatibility boundary)
- `HEADLESS_BOUNDARY_AUDIT.md` (remaining mapped platform-boundary classification)

The prototype's passing tests verify only the prototype. They are not evidence of equivalence to the original executable.
