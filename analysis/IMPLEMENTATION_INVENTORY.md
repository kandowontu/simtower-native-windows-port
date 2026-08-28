# Implementation inventory

## Authoritative binary inventory

- Classified original routines: 1,175
- Recovered compiler switch/message tables: 170
- Unique decoded code bytes: 297,229 / 309,486 (96.04%)
- Resolved imported API callsites: 2,221
- Named NE exports plus program entry: 22
- Relocation targets into code segments kept as pointer/data candidates: 150

See `simtower-callgraph.json` for the machine-readable graph and `original/disassembly/functions` for annotated per-function assembly.

## Native translated foundation

- Exact 483-resource embedded pack; no runtime dependency on `SIMTOWER.EXE`
- All 242 original 8-bit DIBs validated and a native original-DIB blitter
- All 48 Win16 dialog templates parsed and converted to native templates
- All 45 DTMP resources (332 rectangles) parsed, sized, painted, and positioned
- All six ALRT resources parsed and their formatter/display/result paths translated
- Exact PART, YEN, STRL, TABL, and TABM table readers with byte-for-byte value tests
- Original menu, accelerator, application icon, class names, window styles, and
  startup dialog path. The complete `1010:0018/00fd/014c/0304` title-splash
  lifecycle is translated: named DIALOG/TOWER_TITLE, modeless and modal exported
  filters, black desktop backing, exact centered/clamped BITMAP/128 and 256,
  full-desktop/border-expanded sizing, wait cursor, 180-coarse-tick (nominal
  2.88-second) minimum first phase,
  synchronous replacement, mouse dismissal for the modal variant, and resource/
  window cleanup. `1128:0042-00da` keeps the modeless splash behind New/Load.
- Native host-shell fidelity adapter for the Win16 `DefWindowProc` surfaces:
  unthemed/DWM-disabled legacy non-client rendering, centered active/inactive
  title paint, left system box plus the original down/up main-caption buttons,
  preserved resize/minimize/maximize hit testing, classic fixed-thumb
  scrollbars, and the legacy COMDLG32 path. Open/Save retains the recovered
  zero flags/filter/buffer contract while reflowing the host controls to the
  reference 470x247 layout, original labels/capitalization/font, centered
  system-only dialog caption, and hidden later Network control.
- Exact `1128:1318` startup virtual-memory preflight: the recovered
  `(GETFREESPACE(0) >> 10) + 2370 >= 6000` unsigned gate, first passing raw
  free-space boundary, byte-exact diagnostic prefix/suffix, calculated KB
  values, `Error` title, and `MB_OK | MB_ICONEXCLAMATION` failure path are
  translated. The Win32 host maps available page-file capacity to the original
  `GETFREESPACE` input before any game resources or windows are created.
- Complete `1128:1139-1306` capability wrapper around that memory gate:
  system-font ascent capture; separate 8-bpp, BITBLT, device-independent-DIB,
  DIB-to-device, and StretchBlt consent branches; mandatory TrueType
  available/enabled checks with recovered messages; device-zero wave output
  consent and sound-disable result; and the absent-only legacy
  `[Extensions] tdt=<module filename> ^.tdt` profile registration. Exact
  ordered issue arrays, consent classification, messages, and association
  value are headless-tested.
- Exact `11e0:0d80/1258:0505` custom-cursor path: all CURSOR resources are
  embedded, decoded into native masks, selected from the recovered edit-mode/
  capture table, and released at the original application-lifecycle boundary.
  The complete screen-point resolver checks enabled Map, Info, and Command
  rectangles before the non-iconic main client, preserves the current cursor
  outside every eligible rectangle, and still runs before `1158:032a`'s modal
  mouse-move gate.
- Exact revision-0x17..0x24 TDT parser/writer, four byte-exact sample rebuilds,
  fresh revision-0x24 constructor, and native file-command boundary. The
  complete `10d0:1518` raw swap map is represented, including its cf88/dbfc
  no-swap omissions, destructive tenant-subtype quirk, and express-elevator
  unmapped-floor write. Pre-0x22 segmented cars and all adjacent migrations
  are reconstructed, while game Save follows `10d0:0b3a` by always emitting
  current revision 0x24 little-endian rather than preserving an import's old
  or opposite-endian family.
- Exact scheduler timing/call boundary and native `PeekMessage` idle integration.
  A full `1200:0196` cycle exercises the complete 36-signature host boundary;
  the native dispatch contract rejects unknown, malformed, or unhandled
  callbacks so translated gameplay work cannot be silently skipped.
  The fetched-message branch at `1258:00bc-015c` now gives modeless Elevator
  Control precedence over DS:31a4's active modal, applies
  `TranslateAccelerator` before `IsDialogMessage` to either specialized
  target, and targets Main only when neither exists. The empty-queue branch no
  longer inserts a native `SwitchToThread` absent from the original loop.
  The preceding `1258:0195-023a` empty-queue work now also reconciles the
  DS:0252 WAVMIX active latch from DS:31a6/`IsIconic`, restores enabled
  Command TOPMOST only when no modeless Elevator Control exists, promotes that
  control TOPMOST/active while main is active, and inserts it behind main on
  deactivation. The unconditional `11c8:0135(0)` tail remains its original
  force-zero no-op on every exit.
  The `1258:0244-02d1` host gate now samples the live cursor and calls the
  scheduler/`1090:03ab` only while construction is enabled and DS:02a6's
  Elevator-Finger capture is inactive. Non-advanced ticks use the distinct
  construction-preview-only path, avoiding full-frame RNG/person work and the
  invented `WM_MOUSELEAVE` lifecycle. The outer host no longer prefilters on
  raw cursor motion: every eligible non-advanced tick enters `1090:03ab(0)`,
  which restores and rederives the native DS:77ac rectangle before its exact
  `EQUALRECT` presentation gate. A stationary cursor therefore still updates
  after a footprint, mode, or viewport change.
  The adjacent `1080:0a1e`, `1090:03ab`, and `1158:00da/0a3c/0ae5/0ba8`
  presentation split now retains a native DS:3264-equivalent backing raster.
  Ordinary exposure paint only blits that raster; `1090` advances visible
  facility-person presentation once after transport work and does not rerun
  the sky; `1080` alone optionally runs `1048:03a3` before facility people.
  Preview and direct palette repaints rebuild RGB transport pixels without
  advancing shared RNG or consuming Elevator transfer visuals. One-frame
  transfer pixels are consumed only after the full-frame cache retains them.
  The following `1090:061f-06dc` window fan-out is also separated exactly:
  dirty World pixels are presented directly and invalidate Command only; Info
  receives its unconditional direct-DC content pass every full frame; Map is
  excluded and therefore retains `1090:046f-047c`'s sixteen-tick animation
  cadence. `1158:00da` skips the backing blit while iconic, and `1080:0b26`'s
  scrollbar floor label now runs only for explicit `1080:0a1e` rebuilds.
- Exact `1258:0029-0070` and `1128:00e5-0196` startup command-line file path:
  the raw nonempty ANSI WinMain tail bypasses DIALOG/124, a bare file name is
  prefixed by the executable directory retained through its final backslash,
  and a tail containing a backslash supplies both its full path and basename.
  Both this path and an accepted Open sheet now enter `10d0:0225/062a`'s
  reset-before-I/O transaction; any open/parser failure shows the original
  mapped alert and falls through to a fresh tower instead of retaining the
  previous document.
  The adjacent Open/Save As common-dialog boundary now also performs
  `1078:01e8(0/1)` palette demotion/restoration and Arrow publication on every
   attempt, retries Save As after an invalid DOS basename, and deletes a
   partially created target after `10d0:080e` transfer failure.
- Exact `1158:049e-04fb` main-shutdown split and `1258:0095/016e` process
  teardown: Close confirms action 4, sets DS:31c6, stops both mixer channels,
  destroys Main, and clears DS:3258 afterward; Query End Session returns TRUE
  only when its same confirmation changes the closing latch from zero to one;
  Destroy only posts quit. Both startup cancellation and normal WM_QUIT exit
  destroy Map, Info, then Command through `10b8:0000`, followed by the recovered
  custom-cursor, audio, and palette release order from `10b8:0039`.
- Complete `1220:0000` nightly reset switch for person families 3..7, 9, 10,
  12, 14, 15, 18, 29, 33, and 36, preserving each branch's distinct short
  or full tail clearing behavior and `01ba-021b`'s signed tenant-status
  threshold, including high-bit Hotel/Condo transient states
- Exact `10b0:0072/031a` post-load reset chain for tenant and person runtime
  state, including signed status gates, Retail/DC24 linked-record mutations,
  per-family tenant words, dirty marks, malformed-index guards, and call order
- Complete scheduled `1220:1059/10af/1518` person/transit sweep, including
  the literal tenant-type jump table and `1228:07c5` person spans, both
  `1210:1184` Stair/Escalator counters, both forty-entry Elevator waiting
  lanes, active-car passenger/occupancy removal, `1090:0bcf` target/direction/
  secondary-target recomputation, `11d8:00fc/0000` movement metrics, and the
  unconditional type-7 `1198:0489` b846/parking/tenant cleanup path
- Complete common person route resolver at `1210:0000`, including the exact
  `11b0:0f10/0fa5` owner-word lookup and 64-Stair/24-Elevator/8-route scoring
  chain, cf10 span predicates, bff0/db9c transfer direction, express versus
  standard/service costs, both `1210:114f` Stair counters, both forty-entry
  `1210:11c2` Elevator rings, first-waiter car assignment, the original
  byte-7/byte-8/word-10 mutation order, queue-full/no-route outcomes, and
  `11d8:02f7/0000` travel metrics. Headless tests cover direct and transfer
  legs, sparse express floors, full rings, and opposite-endian persistence.
- Exact `1228:0968` day-start and `1228:0b59` evening facility sweeps,
  including the original Hotel/Office/Condo status transitions, special-part
  frame selection, and per-tenant dirty-byte updates
- Exact modeled `1198:01ab` day-start parking refresh, including deferred
  completion, b958 index reconstruction, inactive-tenant exclusion, orphaned
  cf9c removal, and persisted parking-count repair
- Complete scheduled Recycling Center state machine at `1088:0000/00de/01d1`
  and its `1088:0250` population-per-center selector, including exact 500-person
  bands, persisted b92c gating, frame-five hold, midnight upper/lower reset,
  information codes 3/4, and the WAVE/2280 transition request
- Exact `1170:011f` Medical Center day-start rebuild, including forced pending
  activation, dbfc orphan repair and timer clearing, persisted b3fe count repair,
  bd5a plus seven-group process-route reconstruction, and rating-gated b92d
- Exact `1228:086b` pre-midnight Hotel/Office/Condo status sweep and the
  `1020:0dcb/0e0b` eight-day b406 flag pair, including revision-aware header
  offsets, b3e4 mutation, rating/day gates, and preservation of unrelated bits
- Exact `1240:01de` nine-day b924/b928 reset with both fields modeled through
  the native revision-compatible save/load path
- Exact `1060:003a` three-day accounting rollover: last-balance snapshot,
  complete eleven-dword income and maintenance resets, cleared header period
  accumulators, and deliberately preserved population accounting
- Exact `1060:07b3/07f7/0837/0880` population, income, and maintenance
  accounting primitives plus both `1060:08be/0958` category selectors. This
  includes the maintenance keys recovered directly from segment 13 at
  `CS:09c1`, uncategorized-type behavior, signed 16-bit population deltas, and
  32-bit x86 wrapping arithmetic
- Exact `1178:0854/08ec` YEN/1001 rent addition and removal primitives,
  including the tier-four sentinel, shared signed `1178:1377` add-side cap,
  category accounting, and wrapping 32-bit removal-side NEG
- Complete scheduled `1178:0b44` three-day maintenance sweep, including all
  floor tenants, PART/1000 rating- and width-dependent Lobby charges,
  YEN/1002 elevator-car and Stair/Escalator charges, the hard-coded Lobby
  accounting category, and the direct wrapping debit/accounting behavior of
  `1178:097c/09ee/0a6a` (which deliberately does not call `1178:1377`)
- Exact self-contained `1130:01e2` Hotel pair-repair pass, including its
  signed status tests, DS:b3a1 frame selection, adjacent-record writes, and
  unusual skip/revisit loop behavior
- Revision-aware native model for header b402 and the dce4 table's ten or
  twenty persisted 32-bit person-record indices, plus the complete scheduled
  `1188:0977/0a20` nightly and Hotel link-removal filters. The seven-key
  filter table was extracted directly from segment 50 at CS:0a04
- Exact revision-0x23+ b404 tenant-link count and both post-core fixed
  sixteen-byte Find name tables from `1188:043d/05a7/05e3/0793/0884`, including
  NUL-prefix display, byte-exact preservation after the first NUL, paired link/
  name compaction, real-save round trips, and separation from opaque exporter
  trailing bytes
- Exact process-global Microsoft C runtime RNG lifecycle at
  `1000:3a18/3a2f`: initial seed one, wrapping `state*0x015a4e35+1`, high-word
  `&0x7fff` return, no recovered caller of the seed writer, and live-state
  retention across New, successful/failed Open, and TDT document replacement
- Exact per-frame `11e8:0273` Metro visual pulse, including b406/b3e8 gates,
  Microsoft-runtime RNG consumption, one-in-100 selection, complete variant-
  word toggles for types 31..33, dirty marks, and conditional WAVE/10010
- Complete `11b8:0028/0060/0000/0089/014b/020b` annual moving effect, with
  revision-aware runtime/persisted dd6c storage, exact world-derived start
  coordinates, per-frame 16-bit motion, full-width termination clear,
  notification 7, and the pixel-tested transparent-zero BITMAP/904 plus
  CLUT/1000 draw. The `1048:00ad` initialization trace proves DS:775a is the
  140-pixel source-rectangle right edge; the native direct renderer produces
  the same final pixels without WinG's adjacent saved-background scratch area
- Complete `1048:00ad/05f0/06a5/0717/083f` four-slot sky-decoration layer:
  exact BITMAP/900..903 dimensions and transparent-zero composition, original
  `(0,360)-(3000,3888)` eligible world rectangle, `1080:01cb` client-to-world
  expansion, full-rectangle retention, strict source-fit tests, duplicate
  selection, Microsoft-runtime RNG consumption, randomized placement, and the
  original background-before-facility-person paint order. Headless tests cover
  deterministic placement, retention without RNG use, selective replacement
  after scrolling, undersized viewports, source pixels, and palette output.
- Complete ambient-audio selection chain at
  `11c8:03ab/03fb/0426/05e8/0671/06b6`, including exact six-point visible
  world probing, tenant/person/service-state lookup, above/below-ground
  sentinels, annual/day-phase contextual sounds, and branch-exact Microsoft
  runtime RNG consumption; the native scheduler submits only the resulting
  resource request to the already translated audio arbiter
- Complete scheduled entertainment family at
  `1180:05af/06a8/0826/090a`, including the leading all-pending-facility
  activation pass, Movie Theater/Party Hall population rebuild, PART/1000
  age-band capacities and admission payouts, exact dc24 state/counter bytes,
  tenant-owned person spans, dirty-record propagation, arrival/show/departure
  states, event-day income suppression, balance capping, and category income
- Complete scheduled commercial family at `11a8:0184/0250/0554/0603`,
  including the leading pending-facility activation pass, byte-exact orphan
  cleanup and b3f8 decrement, full dd5c/dd60/dd64 route-block rebuild, signed
  lane/capacity/minimum selection, Retail/Fast Food/Restaurant population
  accounting, tenant dirty propagation, PART/1000 attendance payouts, balance
  capping, event-day suppression, and opposite-endian retail words
- Complete scheduled tenant-evaluation family at `1130:0000/0109`, including
  exact tenant-owned person spans and performance division, type/rating PART
  thresholds, rent-rate adjustments, the full `1138` same-floor adjacency
  scan, commercial/movie attendance satisfaction, three-day Office/Condo/
  Retail departure, age, population, service and YEN/1001 rent behavior,
  same-type zero/two pairing, person-counter reset, ordered Hotel pairing, and
  three-tick Hotel checkout transitions
- Revision-specific serializer rejection for b402 person-link counts that
  exceed the physical ten-entry pre-0x23 or twenty-entry 0x23+ dce4 tables
- Pixel-tested exact Floor atlas, direct facility renderer for types 3-15,
  all eight 36-row SECOM Center frames, and all embedded Movie Theater
  type-18/19/34/35 body/entrance frames, plus all six upper and seven lower
  Recycling Center type-20/type-21 frames (including the exact 60-row composite
  extraction), and all three upper and three lower Party Hall type-29/type-30
  frames, plus all nine Metro Station type-31/type-32/type-33 frames and all
  sixteen Cathedral type-36..40 frames (including the type-40-only fourth
  frame from BITMAP/3562),
  pending-construction strip, Lobby, Office, all standard/express/service
  elevator caps, shaft bodies, sparse-floor cars and exact floor-10..18 car
  source selection, normal
  and lobby-spanning Stair/Escalator DIB/CGPK banks, and all three original
  Parking Ramp connectivity frames
- Pixel-tested exact `1090:0b10/216e/221f/227b` live Elevator-car compositor:
  BITMAP/1064..1069 staging coordinates, 28-pixel standard/service and
  44-pixel Express rectangles, signed occupancy-to-crowd frames, full/not-full
  capacity split, signed six-pixel movement increments in both directions,
  aligned visible-row gate, clipping, and the post-Stair/Escalator layer order.
  The preceding `10a8:0507/07d6` SHOW split is also exact: nonzero shafts bake
  the car into their body and skip the late compositor, while zero shafts keep
  both caps, draw only two black 35-pixel boundaries per in-span floor, and
  receive the late moving-car sprite
- Exact `1028:0000/00ba/0446/0872/0a11/0ca9/0d7d` facility-interior
  presentation dispatcher and animation helpers, including control-modifier
  variants, source frames, owner lookup, process-local phase state, and native
  world-layer integration with headless pixel/state coverage
- Pixel-tested exact `11c0:0000/024a/02c0/0374/0428/0483/04ce/0518/054e`
  exterior/floor-edge layer: ordinary 36x24 outer caps, the two 36x56 ground
  foundation ends, topmost-wide-floor roof marker, and their original
  BITMAP/1001/1002/1193/1259/5000 source-sheet composition and draw order.
  All five `1208:071f` calls use zero as the transparency marker, so their
  `1248:0000 -> 1250:0024` byte loops preserve destination pixels wherever the
  source index is zero instead of exposing rectangular DIB backgrounds
- Pixel-tested `10a8:088c/0fff/12c1/1737/1875/1913` Elevator waiting-person
  renderer: active-shaft collection, Shell-sorted x order, both forty-entry
  circular lanes, exact one/two-cell family widths, left/right spans, person
  palette-marker substitutions, wait-metric/Lobby reductions, and original
  world-layer placement from BITMAP/1128+1129 and CLUT/1000
- Pixel-tested `10a8:022b/02aa/0de6` Elevator transfer-person renderer and its
  live producers: four exact boarding/alighting placements, transparent-zero
  source treatment, strict left/inclusive right floor boundaries, original
  per-floor Shell-sorted shaft -> boarding -> alighting interleave before the
  waiting-person layer, and both last-writer-wins cache slots for every visible
  floor/Elevator pair. Distinct shafts/floors coexist in one frame; explicit
  `1080:0a1e` rebuilds reset all slots at the recovered `10a8:0000` boundary
- Pixel-tested `10e8:04a0/0693` fire-band and hired-crew renderers: all nine
  twelve-cell frames from the concatenated BITMAP/3944..3949 atlas, normal and
  SECOM frame selection, both persisted per-floor fire bands, fixed crew frame,
  coordinate gates, exact per-channel nonzero compositor, and original layer
  placement
- Pixel-tested `10f8:00c9` Security responder renderer: all ten packed cf88
  registrations, floor +0xa92 tenant-key indirection, serialized tenant +8
  (runtime +14 after the six-byte floor header) six-person
  spans, active/zero-delay gates, signed floor/x/state fields, two-cell
  BITMAP/1128+1129 family 90+state, full-width viewport gate, per-channel
  compositor, and below-Elevator layer order
- Exact `1020:0e29/0f4f` active `LOGPALETTE` construction for CLUT/1000,
  including the skipped source record 184, `PC_NOCOLLAPSE` entries 0..187,
  `PC_RESERVED` entries 188..218, unflagged entries 219..254, explicitly zero
  destination entry 255, version 0x0300, and 256-entry count
- Exact shared logical-palette lifecycle at `1020:098b/08b4/00cb/053e`:
  CLUT/1000..1003 interpolation for entries 188..193, alias propagation into
  207..218, all ten 15-coarse-tick (nominal 240-ms) effect-animation entries
  at 194..203, strict
  special-event 80/1490 clock gates, alternating six-color alias bands,
  signed tick-magnitude and signed 16-bit remainder behavior, counter wrap,
  Effects-menu freezing, and persistent process-local state. Every indexed
  World and Map source now resolves through the same runtime palette; both
  pipelines and every transition boundary are headless pixel-tested.
- Exact top-level palette-message boundary at `1158:04fe/0508/0c29`,
  `1050:0300`, `1120:0170`, and `1168:020c`: main `WM_QUERYNEWPALETTE`
  realizes but returns zero, self notifications are suppressed, auxiliary
  changes honor the initialization/closing gates, dynamic entries 188..218
  are applied through the native `HPALETTE`, and a changed realization
  synchronously repaints Map, Info, Command, then Main. The recovered fan-out
  retains `1158:0c29`'s exact mechanisms: direct Map with client invalidation,
  Info invalidation plus `UpdateWindow`, direct Command-content blit without
  invalidation, and direct Main with client invalidation. Supplied source DCs
  are reused; other direct targets acquire/select/realize/release their own.
  The direct Main pass does not replay ordinary WM_PAINT's paint-time RNG/person
  step or consume the one-frame Elevator transfer cache. Pure tests cover every
  dispatch gate, result, flag-band boundary, ordered action, invalidation,
  mechanism, and DC-ownership combination.
- Exact modal command-selector palette branch at `1050:090a` and its
  seven-message table at `1050:095c/096a`: self notifications return TRUE
  without work; non-self notifications realize on the selector DC and call
  `UpdateColors` on that same DC only when entries changed. It neither invents
  `WM_QUERYNEWPALETTE` nor enters the top-level four-window repaint fan-out.
- Exact construction/activation paths for Floor, ground and sky Lobby, all three raw elevator
  commands (standard 1, express 42, service 43),
  Stairs, Escalator, Hotel
  types 3-5, Office, Restaurant, Condo, Retail Shop, Fast Food, Medical Center,
  Security, Housekeeping, SECOM Center, the two-floor Movie Theater composite,
  the two-floor Recycling Center composite,
  the two-floor Party Hall composite,
  the three-floor Metro Station composite,
  the five-floor Cathedral composite,
  the exact 16-cell Parking Ramp, and repeated 4-cell Parking,
  including exact `10a0:1310` and `11f8:17fd/284d/30ef` Floor interval
  insertion/replacement: distinct separate-click records, automatic gap
  records, Lobby classification for gaps on initial Lobby stories, and
  byte-preserved uncovered remainders (including split Lobby edge records when
  a drag returns toward its press anchor); the 64-slot
  vertical-transport table,
  commercial landing whitelist, lobby-spanning shapes, collision rectangles,
  cf10/bff0 route summaries, saved people/service records, exact
  `11a8:07d3/1596/17eb/1812` Restaurant/Fast Food service-slot initialization
  and floor-group indexing (including `1060:07f7`'s immediate +10 Fast Food
  population when activation starts open), Movie Theater's
  exact 18/19-to-34/35 record expansion, 112-person initialization, shared
  dc24 link and Microsoft-runtime RNG state, Recycling Center's exact b3f4
  alignment/adjacency gate and deliberately inactive twelve reservation
  records, Party Hall's exact 29/30 order, eighty type-29 people, and shared
  b400/dc24 Movie capacity and link, Housekeeping's exact six-person
  `15/00/FE/FF` activation and shared
  cf88 registration, Metro's unique b3e8/floor-zero gate, split funds check,
  240 active passengers and twelve inactive reservations, ramp-chain
  connectivity, Cathedral's fixed floors 113..109, split funds gate, forty
  shared type-36 people and persisted bottom key, and parking-index
  reconstruction
- Exact `11e8:0000` Metro activation specialization: the bottom station is
  shifted to slot 1, type-45 graphical boundary tenants fill both sides through
  cell `0x177`, the left boundary inherits the station people-start dword, and
  the floor edges/lookup are rebuilt in the recovered mutation order
- Cathedral's direct `1040:0000` and `1040:0179` scheduler mutations,
  including forced deferred completion, forty-person midnight state reset,
  five-part frame clearing, participant transition, and b406 flag removal
- Exact type-36 Cathedral person family at `1220:6037` and its
  `1040:00f0/02b5/03bb` ceremony boundary: completed-Stair release, all four
  persisted states, common-route transitions, forty-person arrival gate,
  rating promotion, facility frames, event flags/deadline, and explicit
  headless effect, focus, repaint, channel-stop, and WAVE requests
- Complete Cathedral normal-pass wrapper at `1220:5edd`: exact calendar/day
  gates, Microsoft-runtime random call, the deliberate post-`00f0` double
  callback fallthrough, positive-day `27` reset, completed-transit dispatch,
  and shared signed Elevator-timeout dispatch. Successful ceremony focus, channel stop,
  and WAVE/10008 requests are consumed by the native host.
- Complete shared Movie Theater/Party Hall person family at `1220:5734` and
  wrapper `1220:55b8`: all eight CS:5ebd states, Movie/Party and upper/lower
  dc24 capacity lanes, initial-route rollback and metric clearing, show-phase
  and attendance counters, linked-tenant dirtying, randomized floor-banded
  Retail/Restaurant/Fast Food detours, full/unavailable service behavior,
  PART-backed dwell, both return legs, completed-Stair release, signed wrapper
  phase gates, phase-four reset, and normal/opposite-endian records are wired
  into the ordered native pass.
- Complete type-14 Security person callback at `1220:67cf`, including the
  Bomb-first `10f8:033d` event dispatcher and `10f8:0701/0c06/104a` bomb
  patrol and six-way fire-floor partition. Combined Bomb+Fire flags retain
  Bomb priority, including SECOM's responder-zero-only activation. The
  responder countdown/motion words, exact bomb-coordinate completion and
  sibling disable, two-band extinguishing, and process-only `DS:77aa`
  post-extinguish travel acceleration. The per-frame `1220:0f85/6764`
  all-Security scan is wired into the native scheduler and headless-tested
  for normal and opposite-endian records.
- Complete type-15 Housekeeping person family at `1220:6383`, its
  `1220:6297` normal-pass gates, and the exact sixteen-way scheduler stride:
  six-floor ownership bands, upward-first/downward-next dirty Hotel search,
  service-route selection, completed-Stair release, three-callback cleaning,
  first-guest handoff/reopen, return-home behavior, event-pass suppression,
  and normal/opposite-endian records are translated and headless-tested.
- Complete type-33 Metro person state table at `1220:5227` and its
  `11a8:1472/12dc/1061/0cc2/0f11/0bd5` commercial-service chain: exact
  Microsoft-runtime random destination selection, Retail/Restaurant/Fast Food
  group-zero lookup, service population and 0/1/9/10 status edges, tenant
  dirtying, foreign-visitor attendance, PART-backed dwell timing, both common
  route legs, completed-Stair release, full/unavailable service behavior, and
  opposite-endian words are translated and headless-tested. Its `1220:50e2`
  normal wrapper is wired into the complete live family switch of the
  sixteen-way scheduler, including cross-family Elevator timeouts.
- Complete shared type-6/type-12 Restaurant/Fast Food visitor state table at
  `1220:4bde`, wrapper `1220:49fa`, and
  `11a8:10b3/1159/1197/174e/17eb` dependencies: the floor allocation's
  six-byte header offset, service-word lookup, signed person-ordinal
  reservation ceiling, scheduled/customer/attendance counter mutation and
  rollback, closed/full/unavailable behavior, PART-backed dwell, both route
  legs, completed-Stair release, signed person-performance division, rating-
  selected thresholds, three history lanes/capacities, Restaurant/Fast Food
  time and random gates, late idle reset, and opposite-endian words are
  translated and headless-tested. The native normal pass preserves this
  family in index order with Housekeeping and Metro.
- Complete type-10 Retail visitor table at `1220:4453`, wrapper `1220:426c`,
  and the persisted `1178:1140` activation path: inactive/active route-failure
  distinctions, reservation rollback, first-viable-route store activation,
  YEN-backed rent income, ten-person population accounting, tenant/service
  dirty state, all 48 owned person-metric resets, service dwell/population,
  attendance history, wrapper availability/time/random gates, completed-Stair
  release, and opposite-endian records are translated and headless-tested.
  The process-only `1118:0a49(6)` activation visual is exposed as a counted
  host request; it is not replaced with an invented effect.
- Complete type-9 Condo resident table at `1220:3c09`, wrapper `1220:38e1`,
  and persisted `1178:0fe3` reactivation: all twelve CS:423c states, three-
  resident occupancy synchronization, calendar/key-specific commercial
  detours, both Lobby/home route families, active/inactive route-failure
  distinctions, YEN-backed rent, +3 population, owner dirty state, all three
  resident metric resets, wrapper day/calendar/random gates, completed-Stair
  release, opposite-endian words, and the process-only activation visual host
  request are translated and headless-tested in the ordered normal pass.
- Complete Hotel type-3..5 guest table at `1220:3154`, wrapper `1220:2e92`,
  and dependencies `1178:0df9/0eac`, `1198:002f/031a/0489/0621/0650/06a6/
  06e7`, `1230:0000/0244`, and `1240:0000/00d1/0130/0198/020d`: all ten
  raw states, paired-guest occupancy, commercial detours, connected parking
  allocation/capacity/reversal, type-5 periodic visitor score and transaction
  requests, inactive room activation, last-guest YEN checkout/population,
  exact process-local checkout cadence, ordinal-zero wrapper exclusion,
  wrapper timing/random/direct-rewrite gates, completed-Stair release, and
  normal/opposite-endian records are translated and headless-tested. The
  existing parking cleanup was corrected to decrement b846 before clearing
  the person's encoded parking bits, exactly as `1198:0489` does.
- Complete Office type-7 normal person table at `1220:23e4`, wrapper
  `1220:2068`, and dependencies `1170:0291/0414/0522/056f/05f0/061c/0635`,
  `1178:0cb4`, `1198:031a/0489/0650`, `1230:0000/0244`, and
  `1220:6bef/6cb6`: all sixteen raw states, six-person occupancy bands,
  commercial and Medical Center detours, exact 40-person medical capacity and
  16-tick dwell, unavailable-center waiting-delay/finalizer calls,
  notification codes 5/6, eligible-employee parking,
  inactive-office YEN rent and +6 population activation, six-person metric
  reset, wrapper day/calendar/random/direct-rewrite gates, completed-Stair
  release, and normal/opposite-endian records are translated and headless-
  tested. The process-only `1118:0a49(1)` activation visual remains an explicit
  counted host request.
- Complete shared transit and cleanup dispatch at `1220:1637/16ab/1aed` and
  `1210:1b41/1d56/1332/0883`: signed PART/1000 word-zero timeout comparison,
  exact up/down 40-person ring cursor/count rotation through the target,
  stale-slot preservation, every common metric/family arrival branch, Hotel
  parking and special-visitor cleanup, Office/Condo occupancy completion,
  commercial attendance history, Housekeeping reset, process-only view-slot
  restore requests, and the complete raw family switch. The car-arrival-only
  type-14 Security branch is distinguished from `1220:16ab`'s deliberate
  no-op. Normal and opposite-endian rings, malformed-data atomicity, wrapper
  integration, and the self-contained passenger cleanup overload are
  headless-tested without the original executable.
- Exact `1058:04e0` command-palette transaction and `1050:05d6` selector:
  TABL/TABM one-based resolution, raw TABL/1000 icon-to-build mapping, mutable
  category choices, resource-backed modal grouped-selector behavior, selected-
  row alignment, top-edge clamp, and whole-selector desktop-bottom correction.
  `1050:0219/02b3` also preserves the original mouse-phase split: ordinary
  edit/facility points activate on button-down, grouped selectors capture the
  held click and consume its release, and only the build toggle waits for
  button-up after presenting its transient pressed frame.
- Exact shared `11e0:0b52` dialog placement for all eleven original callers:
  outer-window centering, signed Win16 arithmetic, the nonstandard 43-pixel
  minimum and 80-pixel near-full-height policy, final desktop-bottom clamp,
  `HWND_TOPMOST`/show flags, and Elevator Control's explicit left coordinate 8.
  Native startup/About centering remains separate because neither is a caller.
- Exact saved Person/Tenant Find model and native modal boundary from
  `10d8:0000/006f/02ce/038e/0438`,
  `10e0:0000/0042/04cf/051d/055b/078d/0cc9/0cea`, and
  `1188:0793/0884`: DIALOG/510/520 controls, shared DTMP/BITMAP/510 surface,
  original list order and saved names, selection enable/cancel/double-click,
  Remove compaction/dirty state, direct and linked facility targets, Stair/
  Escalator, moving Elevator cars, and both waiting rings through
  `10a8:00a8/09e7`'s exact diminishing-gap Shell-ordered shaft cache (including
  indirect equal-x reordering), followed by the exact edit-mode/camera
  transition. A focused result paints the embedded transparent
  BITMAP/21256 target at its exact world coordinates through 300 coarse ticks
  (nominal 4.8 seconds), including
  wrapping tick arithmetic and the original construction/map-mode restoration.
  `10d8:00ec-0146/0323-0360`'s presentation split is also preserved: both
  initialization and paint realize the palette and use positive DTMP/510 to
  paint BITMAP/510 and replay child placement, neither draws the generic
  negative-resource bevel/chrome, and only initialization selects the 12-pixel
  font. Both phases are covered by pure headless assertions.
  The exact `10e0:0669/06cd/0b61/0bc6/0c72` unfocused branches now use
  ALRT/1002 for a named person outside the tower or ALRT/1003 for a person in
  the Lobby, including STRL/712's `B` prefix, stored-floor conversion, Movie/
  Party links, Retail/Medical records, Housekeeping's state-two exception,
  and failed transit lookup. `10e0:0bc6` centers Movie/Party results with the
  linked dc24 record's signed byte-7 rule (15 cells for a paired record or 12
  for a single side), independently of the target tenant's type table. The
  state-two Housekeeping branch reads its assigned room from person byte 6 and
  word 12, accepts only Hotel types 3..5, obtains the first guest through
  `1220:6ba9`'s serialized tenant dword +8, and focuses that room only while
  the guest is in state 3. Unsupported/invalid states keep Find open without
  inventing either a modal Person Information window or a viewport target.
- Exact `1080:0209` Map/world aspect-fit rectangle transform: original origins,
  integer percentage comparison, dominant-axis constraint, no enlargement of
  already-fitting content, signed-truncation scaling, and centered output.
- Exact About modal from `1010:049e/053f/098f/0a3b/0af1`: named
  DIALOG/TOWER_TITLE, BITMAP/257 title art, TEXT/128's 176 CP1252 credit lines,
  604x290 runtime geometry, beveled 236x266 scrolling viewport, 236x16 line
  surface, timer ID 9 at 55 ms, one-pixel wraparound scrolling, DrawText flags
  `0x0809`, cleared class cursor plus selected stock arrow, key/mouse-release
  dismissal, and the original
  stop/deactivate/reactivate audio boundary. Menu command 40018 is wired.
- Complete `1158:0000/06b9` main-window dispatch: all 22 recovered messages
  and all 27 exact command IDs, including activation repainting, minimize/
  restore audio, session-end confirmation, double-click edit dispatch, hidden
  9000..9003 Bomb/Treasure/Fire/present hooks, the generic DIALOG/3000..4001
  fallback, and command 40021/F1. The exact 584,831-byte `SIMTOWER.HLP` is
  embedded in the PE and materialized only for the original WinHelp command-3
  call, so Help does not require an installed copy of the game.
  The `1158:028c/029f` press split now also preserves DS:0242's original
  arming rule: only `WM_LBUTTONDOWN` with MK_LBUTTON begins a world
  interaction, while `WM_LBUTTONDBLCLK` cannot start a duplicate edit or
  construction transaction after the intervening button-up. The surrounding
  `1158:00da/028c/029f/02d5` audit also restores DS:0244's paint-reentrancy
  latch and the exact down/double-click/up lifetime of DS:0242: all three
  button messages are suppressed during an active main paint, releases require
  a prior armed press and clear the latch only after dispatch, and an abnormal
  armed double-click still reaches the original world dispatcher. The process-
  wide DS:24b8 modal-manager lock is represented for every native modal and
  consumes Main hit testing, scrolling, button/move input, commands, and system
  keys at the same `1158:0050/015b/01e7/028c/0314/046f/0492` boundaries.
- Exact `1078:0000` main-window minimize/restore lifecycle for the three
  auxiliary palettes: minimize hides enabled palettes in Info/Command/Map
  order without changing View-menu state; restore promotes Command with the
  original TOPMOST/0x53 `SetWindowPos` call and inserts enabled Info and Map
  palettes immediately behind it (or at HWND_TOP when Command is disabled).
  `1158:041c` now preserves the original ordering around resize repaint and
  WAVMIX activation/deactivation.
- Exact activation z-order boundaries from `1158:012c`, `1158:0118`, and
  `1078:01e8`: every non-iconic main activation replays the palette restore
  plan, application deactivation places each enabled palette immediately
  behind the main window, and return promotes Command through the executable's
  repeated HWND_TOP then HWND_TOPMOST sequence. The pure plans are headless-
  tested, including disabled-palette combinations.
- Complete eleven-entry `1050:0000` Command-window message boundary and shared
  top-level palette `WM_ACTIVATEAPP` prefix at `1050:0032`, `1120:002f`, and
  `1168:0032`: activation first raises/shows Main with literal flag word `0x53`,
  then enters `1078:01e8`. Palette close boxes at `1050:024e`, `1120:0111`,
  and `1168:0112`, plus View commands `1158:0886/08a6/08c4`, deliberately do
  not update the initialized menu checkmarks; native preserves that observable
  stale-mark behavior. `1050:010d-0138` also realizes the logical palette
  before its visibility/New/Open/closing drawing gates, and every non-close
  button-down performs the trailing `1208:05e6`/DS:31b0-31b2 timestamp update
  after its toggle or activation action. Pure transition plans cover all
  branches and the exact eleven-message set.
- Literal ten-entry `1120:01ed` Info and thirteen-entry `1168:028a` Map
  message tables, retained as distinct native procedures through creation-time
  dispatch. Map owns the `WM_CREATE` no-op, consumes every move at `1168:013a`,
  conditionally enters `1058:0284`, and clears capture/DS:3216 only on
  `1168:0156` button-up. The host-only `WM_CAPTURECHANGED` message passes to
  `DefWindowProc` without a native state reset. Active, non-modal palette
  activation preserves `1050:008d/1120:007d/1168:009f`'s eight-row
  `ValidateRect` boundary rather than validating the whole client. Table,
  pointer-phase, activation, and validation-geometry branches are
  headless-tested.
- Literal 22-entry `1158:0597/05c3` Main-window table and complete
  `1158:06b9/09d0/0a06` 27-entry command dispatcher/table, including generic-
  dialog/DefWindowProc fallthrough and command 9003's direct retained-backing
  presentation, with all branch returns and side effects
  audited against the native adapter. Host-only `WM_SETCURSOR` and
  `WM_CAPTURECHANGED` pass to `DefWindowProc`; activation avoids invented
  auxiliary invalidations; creation owns only the positional Fast Mode state
  and release-menu deletion; Sound menu initialization remains after initial
  scrollbar setup at `1128:0ae7-0b95`; and unsupported scroll codes retain the
  leading invalidation. DIALOG/3000..3002 remain table-owned zero-result
  commands, while every other ID follows `1158:0992/09b1`'s optional generic
  process call and `DefWindowProc` tail. File/Exit preserves the distinct
  destroy-without-DS:3258-clear transaction. The connected
  `1058:00d9`/`10a0:0544` Finger path now captures every otherwise-unhandled
  press, gates the idle scheduler through DS:02a6 even in empty space, resolves
  upper/lower cap drags, and restores the Finger cursor on release. Literal
  table, command-membership, shutdown, and Finger-path branches are
  headless-tested.
- Complete shared world-input dispatcher at `1058:0000`: signed POINT
  publication through `1208:002c`, Control/Shift mirrors before every gate,
  DS:0242 arming, Elevator Control isolation, Bomb/Fire down-only feedback,
  and the exact four-phase mode routing are retained. Mode one preserves
  `10a0:0000` before `10a0:0544`, empty-space capture, move-time shaft-cap
  acquisition, and unconditional double-click cleanup. Mode two preserves its
  `1058:033c` Find-marker tail after every routed phase, and modes three and
  above retain the Build-enabled `11f8:07d8` boundary. The adjacent
  `10a0:0201` return-value split is explicit: a no-car in-span shaft hit
  consumes when `word_3c` is nonzero and falls through to `10c0:04e0` then
  `11f8:0793` when it is zero. The routing matrix and all five Elevator
  Bulldozer dispositions are headless-tested.
- Exact edit-mode dispatch identities and the mode-zero hit-test chain through
  `10a0:1397`, `10c0:0606`, and `11f8:3e3e`: Elevator shafts and animated car
  rectangles, normal Stair/Escalator diagonal bands, lobby-spanning transport
  rectangles, facility half-open spans, and first-handler precedence. The
  `11f8:3437` Shift-replacement key/floor-expansion table and its ordered
  flags-one facility-damage prepass are translated, tested, and wired.
- Exact Magnifying Glass dispatch from `1058:015b`, preserving Elevator car/
  control -> Stair/Escalator -> Elevator waiting-person -> facility precedence.
  The waiting-person path includes `10a8:0000/0aae/1a88/1cbb/1d41`'s active
  Elevator sorting, two lane scans, forty-entry ring wrap, and one/two-cell
  person widths. Exact information-dialog resource selection from
  `1100:03ac/0e86/11da` covers every facility family, standard versus service
  elevators, and the shared Stair/Escalator dialog. The native mode-two path
  now invokes the original resource-backed Elevator-car and Stair/Escalator
  modals with `1100:1b53/1cbb`'s labels and counters. Their exact
  `1100:327f/3431/35b7/364a/3856` passenger lineups use live car slots or
  `1218:0771`'s family/state transit scan, DTMP item-4/item-9 overflow rows,
  BITMAP/700/702/703 normal/named/VIP portraits, and exact clickable portrait
  rectangles that open Person Information and refresh after renaming. The
  `1100:5043` mouse filter selects CURSOR/1003 only over those item-4/item-9
  panels. The original Elevator
  Control, Person Information/Rename Person, and Facility Information/Rename
  Tenant/New Movie resources, custom filters, live models, and painters are
  translated and wired. Person Information preserves `1100:1dca`'s DS:b3a6
  source modes: live signed-adjusted wait from Main, retained low-ten-bit wait
  from transport dialogs, and zero from facility dialogs. The latest filter
  audit also restores the shared
  13-pixel Arial selection, logical-palette realization, palette-matched
  RGB(204) static-control brush, cleared class cursor plus selected Arrow,
  nested-modal activation redirect, and post-child TOPMOST restoration across
  `1100:0116/085b/0f10/1248`; it corrects the former reversed Facility-panel
  activation target and retains ID-1-only transport-dialog closing. The
  complete `1100:0f10/1248` seven-message-table audit also preserves their
  no-explicit-focus/TRUE initialization result and unconditional empty-click
  consumption plus TOPMOST/DS:31a4 restoration. Both click paths realize the
  palette; only Stair/Escalator explicitly selects it first. Facility
  Information includes `1108:0000`'s ordered,
  three-line advisory helpers and the `1100:2852/2c23/2ec2/307e` BITMAP/700,
  702, and 703 live-person lineup families with exact DTMP item-4/item-9
  placement and one/two-cell sprite widths. Retail lineups use the exact
  `1218:08cd` family/state/owner-link scan, including CBW sign-extension of
  direct byte-six selectors before full-word index comparison; Medical Center
  lineups use the
  distinct `1100:2d3e -> 1218:0a89` Office/Condo state-0x23 scan rather than
  accepting Retail-state lookalikes. Elevator Control includes `1098:16a4`'s
  exact active-car grid-line count, `1098:1502`'s six schedule phase buttons
  and selected-phase frame, and `1098:1e33`'s outline at each car's live
  current floor after the inverted scrollbar transform (not its home floor).
- Exact Elevator Finger pre-routing gates from `10a0:0000/0085/102d/1296/
  12e0/133b`: used/active shaft and range validation, all-eight-car home-floor
  protection, existing-stop bypass, express-only 24/39/54/... stops, and
  Lobby upper-story exclusion. The ordered `11b0:049f/00f2` global graph
  rebuild, all sixteen Lobby-transfer records, MSB-first masks, type-2
  exclusions, and exact `11b0:0b8b` route-loss warning codes are translated.
  Native Finger input now adds stops and removes both empty and passenger-
  bearing stops, using the original ALRT/STRL 1005 confirmation,
  graph-before-cleanup order, self-contained `14fa/1625` family callbacks,
  malformed-data atomicity, and `1038:0000`'s native repaint equivalent.
- Exact captured shaft-extension halves of `10a0:0544/07b7/0819/0b87/10e8`
  are translated and wired to the native Finger cap drag: pointer-to-floor
  offsets, mouse capture, upper/lower bounds, Metro basement gate, expanded
  collision rectangle, requested-range funds preflight, post-preflight
  29-floor standard/service clamp, exact service bytes, graph rebuild order,
  YEN accounting, sparse/contiguous 324-byte floor-record growth, and
  `11f8:15f7/30ef/3a31` floor/Lobby coverage. The inward upper/lower paths are
  also wired for passenger-bearing ranges, including service clearing,
  car home/current clamping, route rebuild, the original global `1625`
  waiting-ring-before-`14fa` car-arrival order, floor-record compaction,
  endpoint publication, owner release, target recomputation, native family
  callbacks, and malformed-data rollback. Upper and lower traversal order and
  retained-record remapping are headless-tested.
- Exact elevator-removal boundaries from `10a0:0179/036e` and `11b0:0cfe`:
  serviced-floor connectivity/confirmation, same-class alternate-elevator
  coverage, complete selected-car `154a` floor sweep, car clear/count
  decrement, and up/down waiting-ring reassignment. The full-shaft branch now
  clears all stops, rebuilds routing, runs `14cc` bottom-to-top, performs the
  persisted `1090:00d9` reset, and clears the used byte. Both paths use native
  family dispatch, malformed-data rollback, and the original Bulldozer hit/
  last-car/ALRT-1005 gates.
- Complete facility-owned `1220:10af` person retirement and the
  `11f8:3528/35ac` damage consumer, including protected/pending alerts,
  population/rent/service links, paired entertainment/recycling conversion,
  floor/parking rebuilds, and exact WAVE requests
- Complete Bomb and Fire offer/advance/damage/crew/completion state machines,
  split at their original modal boundaries and consumed by the native frame
  scheduler in original audio, damage, Security, focus, dialog, clock, menu,
  and deferred-tail order
- Exact `1218:0000` Stair/Escalator person-selection predicate and post-pass
  demolition commit: signed family/state/index gates, selected bd70 clearing,
  word-state clearing, full cf10 reconstruction from remaining records, and
  bff0 route-summary rebuild. The native Bulldozer now runs the complete
  person-table-ordered family callback pass while bd70 is live, consumes
  process-only results, and commits the structural mutation atomically for
  both inactive and passenger-bearing transports.
- Exact elevator-car primitives from `1210:1ac5` and `1090:0dfc/0a4c`: the
  isolated 42-slot passenger pop, immediate-service detection, idle/same-
  direction/wraparound car arbitration with schedule thresholds, one-based
  up/down waiting-floor ownership, assignment-count mutation, and the already
  translated `1090:0bcf` target recomputation. The per-floor `1210:0883`
  car-arrival family dispatch is translated and available through the native
  person-bearing cleanup overload.
- Complete active Elevator frame path from `1090:06fb/10e4/209f/23a5` and
  `1210:07a6/0351/0f0e/1332/1a3b`: settle countdown, standard/Express motion
  classes, target refresh, one/three-floor movement, arrival sound latch,
  five-state doors, schedule-backed endpoint mode and dwell, owned-floor
  release/reassignment, family-specific direct/transfer destinations, one-
  person alighting, bulk/single and special dual-lane boarding, rejection
  delay/dispatch, passenger/destination/occupancy counters, and revision-aware
  wait words. The native scheduler preserves `1090:03ab`'s all-cars-first,
  then all-passengers order and feeds only the original two cached transfer
  visuals into the renderer. Normal, malformed, ring-wrap, cache-overwrite,
  and opposite-endian cases are headless-tested.
- Exact zero-person tail of `10a0:14cc/154a/1625`: active-car occupancy and
  both forty-entry waiting-ring preflights, up/down owner release, wrapping
  word-10 decrements, and per-car `1090:0bcf` recomputation. Static analysis
  also proved `1038:0000` rebuilds only the original process-local visible-row
  tile cache; the native direct renderer therefore needs only invalidation.
- Exact `1080:017f/01cb/0b26` main-scroll presentation: 36-pixel view
  alignment, visible-floor count, viewport-center above-ground/basement label,
  and the original COLOR_MENU/SYSTEM_FONT indicator over the vertical
  scrollbar's up-arrow. `1058:05f8` and `1158:05ef` retain the synchronous
  scroll and resize repaint boundaries.
- Exact `1158:015b/01e7` main scrollbar command arithmetic: 16-pixel line
  steps, client-minus-16 page steps, original line/page clamps, and both thumb
  codes consuming the raw 16-bit position carried by the message before the
  installed scrollbar range clamps it. The Win32 bridge reads `HIWORD(wParam)`
  for both tracking and release instead of reusing `SCROLLINFO.nTrackPos`.
- Exact `1158:041c/05ef`, `1080:00d7`, `1128:08d6`, and `1258:04e2`
  resize/startup lifecycle: creation-time `WM_SIZE` is suppressed until the
  complete startup/show path returns, preventing early palette-window reveal;
  initial and resized ranges use Win16's vertical-then-horizontal min/max and
  saved-position order. The Win32 representation deliberately sets `nPage=0`
  and `nMax=world-client` to retain the original fixed system thumb rather than
  introducing a proportional Win32 thumb.
- Exact `1128:02aa/08d6` and `1158:0334-0415` main-window geometry: the
  816x576 client maximum is applied only after subtracting the desktop's
  scrollbar/menu/caption extents, startup retains the original x=204/y=53 and
  one-pixel extent convention, and all four tracking limits use the recovered
  Win16 frame, scroll, menu, caption, and border formulas. Small desktops no
  longer inherit the cap-hitting maximum from the native shell.
- Exact `1080:0054` captured-drag edge scrolling: only the requested view axis
  moves, by exactly the pointer distance beyond the client rectangle; points
  on either edge remain visible, committed positions use the scrollbar's
  native range clamp, and successful Floor/Lobby/Parking/Ramp/Elevator-shaft
  drag mutations retain the original synchronous repaint boundary. The pure
  signed geometry is headless-tested.
- Original two-channel WAVMIX state machine translated over native `waveOut`.
  The complete `1128:03ad-0535` startup profile boundary probes both original
  SIMTOWER.INI locations, applies `BeepOnly`, `AllSounds`, `Elevator`, `Events`,
  and `Background` in original order, and restores `1128:0b0d-0b95` menu
  check/gray state. The supplied all-enabled INI values are embedded as the
  single-file release fallback; external original profile files are optional.
  The retained BeepOnly latch also reaches its sole game-side consumer at
  `11f8:0e21-0e67`: successful construction clears status, calls
  `MESSAGEBEEP(-1)` when the parsed value equals one, then independently applies
  the five-type WAVE/7000 exclusion. Captured tools reach that tail once on
  release.
  The shared `11e0:0e84` service is entered before every translated
  `1208:05e6` clock read and direct audio submission. Its unsigned
  `last + 0x30 > now` gate, one-interval rolling-anchor advance, wraparound,
  and DS:de2a/de2c/de2e any-category condition are literal. The original
  callback-message drain plus `WaveMixPump` is represented by reaping waveOut
  `WHDR_DONE` headers on each eligible due pass.
- Exact Map window pipeline from `1160:0000/01dc`, `1168:02be`,
  `1080:038e/0440/04b0/093a`, `1058:01d6/0284/064e/085c/094c`, and
  `11d0:0000/0254/0363`: the 200x306 backing surface presented at client
  y=8, BITMAP/352's clock-driven upper-264-row cyclic shift and fixed lower
  24 rows, rating-sensitive four-button BITMAP/310-312 toolbar, exact
  `1058:01d6` one-star hit gate that leaves the disabled fourth cell inert,
  literal thirteen-entry `1168:0000` message table, DS:0248 down/up latch,
  and exact successful-drag focus-before-Main presentation order. The Map
  painter retains its four direct `11e0:0e84` checkpoints, the drag retains
  three, and the adjacent drawable Info painter retains its one checkpoint.
  The 288/4320 signed geometry transform, occupied-floor bands, all three
  tenant-field color overlays, blue/black/red Elevator lines, annual-effect
  marker, BITMAP/313-315 legends, viewport focus rectangle, overlay/build-
  mode coupling, and click/drag inverse-scroll path are translated and
  headless pixel/transform tested. Direct `1160:01dc` coverage pins the exact
  200x20, 200x20, and 81x20 legend shapes, their x=0/0/119 placement at y=18,
  the signed cyclic source phase, and pixels immediately outside each legend.
  Mode one runs `1130:00b5`'s exact all-tenant
  satisfaction refresh before repaint. `1050:0063`, `1120:005e`, and
  `1168:006a/0156` now use DS:31a4's active modal HWND rather than Main for
  activation redirection: Map inserts behind that modal, Command additionally
  returns focus, the no-modal Command path preserves its conditional TOPMOST
  repair, and each branch retains its distinct DS:31a6 write/suppression.
  Map's unconditional button-up capture release is also preserved. The main World's
  `11d0:0072/0145/04ba`
  and `11e0:0efb` path also tiles the four exact eight-pixel BITMAP/1003 strips
  across tenant rectangles in modes one through three, preserving signed field
  gates, special full-story types, clipping/source phase, and layer order.
- Exact Info window pipeline from `1120:0215`,
  `1118:0044/0143/026a/0368/045d/073d/08f3/0933/09be/0a49/0ad5`, and
  `1200:0037/058d`: the
  BITMAP/320 backing and clipped BITMAP/321-323/327 artwork, live rating,
  wrapping balance-times-100, population, DS:784c transient status field,
  STRL/713 weekday/weekend/quarter/ordinal/year formatting, original Arial
  pixel heights and field rectangles, and the analog clock's 3.14-step,
  x87-round-down hand tables are translated and headless-tested. The shared
  transient field now also has its exact STRL/1003/1007/1009/1010 writers,
  priority/replacement gates, zero-index clearing, 1000:39ea signed-magnitude
  tick delta, and greater-than-300-coarse-tick (nominal 4.8-second) expiry;
  native construction/damage,
  gameplay/person, and command-selection outputs feed it in original order.
- Shared `1078:00c6` palette-window chrome and the matching Command/Info/Map
  WndProc boundary are translated: exact eight-pixel active/inactive bar,
  6x6 close box, HTCLIENT/HTCAPTION split, drag behavior, hide/menu state,
  immediate title-strip-only `WM_NCACTIVATE`, active-client validation,
  activation forwarding, exact per-window paint-region and
  visibility/startup/closing gates, palette-destroy `PostQuitMessage`, and the
  Command palette's previously missing +8 content presentation offset. The
  shared title strip is pixel-tested in a memory DC.
- Exact construction-message/audio boundary from `1118:0933` and
  `11f8:0e09/0e21/0e58/0ec0`: one-based STRL/1003 rejection codes, distinct
  facility-versus-exposed-floor funding failures, continuous-drag silence,
  successful-click status clearing/WAVE-7000, and failed-click WAVE-7002.
- Complete `11f8:07d8` construction-input lifecycle: the exact four continuous
  types, down/move/up/double-click filter, capture and 24cc reset order before
  the hidden balance bonus and Shift prepass, Floor/Lobby balance snapshot,
  deferred Parking/Ramp helper anchors, retained double-click placement,
  release-time Floor/Lobby balance gate, early WAVE/7000 versus WAVE/7002,
  one common rating/treasure completion per captured command, literal
  `{0,2,11,24,44}` general-success-sound exclusion, and literal
  `{1,22,24,27,42,43}` transport-route rebuild table are translated and
  headless-tested.
- Exact Floor/Lobby helper completion at `11f8:26dd/284d`: current horizontal
  snap with the down-time floor/anchor, final-result-only 24cc and horizontal
  auto-scroll, and WAVE/7001 only from a nonzero-cost overlapping `284d` path
  (never the charged empty/disjoint `17fd` path). Multi-story ground Lobby
  commands publish b3e6 before validation and attempt every story in order
  without rollback; earlier mutations, charges, and sound requests survive a
  later failure, while the final story alone controls the helper return.
  Partial mutations are explicitly propagated to native dirty/invalidation
  state and the direct-first/reserved-later sound latch remains independent of
  the final return. These success, failure, split-record, and partial-commit
  branches are headless-tested.
- Exact captured Parking/Ramp helper state at `11f8:240d/25a2`: retained
  pointer targets are consumed one message late, current snaps are published
  only after optional auto-scroll, Parking expands in ordered four-cell units,
  Ramp expands in ordered floors using the current call's horizontal snap, and
  bounds advance independently of constructor success. Only the final attempt
  determines the helper return, its one 24cc increment, direct-first versus
  reserved-if-idle WAVE/7001, and horizontal-Parking/vertical-Ramp scroll.
  Multi-unit, direction-change, lag, retained-double-click, live-x, and
  final-result branches are represented by pure headless plans.
- Exact `1100:4869/4d1d` Facility Information preview source, transform, and
  paint boundary: Metro/Cathedral use the recovered 12-pixel top inset and
  60/168-pixel crop heights; Restaurant/Retail/Fast Food use the original
  type-width table rather than serialized spans, including its zero entry;
  ordinary zero-width spans remain unclamped because `1100:4514` separately
  expands the temporary backing to the signed tenant span, Movie's 31-cell
  minimum, or the relevant 2/5-floor minimum and mirrors DS:7782/7784.
  Signed-integer aspect scaling,
  the 200-percent enlargement cap, deliberate small-container cover/crop cases,
  centering, and the original RGB(204,204,204) DTMP item-2 background are
  translated and headless-tested.
- The connected `1100:03ac/4439 -> 11c8:03fb/06b6/0426` preview lifecycle
  renders one retained world snapshot before the clicked-facility sound request
  and modal entry; repaint stretches the snapshot. Commercial sound state uses
  DS:b7e2's 18-byte Retail records at `11c8:07d2`, not the people table, with
  master-sound and conditional shared-RNG behavior headless-tested.
- Complete `1118:0a49` person/schedule host boundary. Office, Hotel, Condo,
  Retail Shop, Restaurant, Fast Food, Movie Theater, and Party Hall emit exact
  STRL/1007 income codes 1..8 in callback order. Hotel special-visitor
  transactions retain their interleaving and invoke DIALOG/3000..3003 through
  `1068:0000` with original amounts and WAVE/10000; notification messages remain
  ordered with those requests rather than replayed from aggregate counters.
- Exact type-24 non-ground branch from `10a0:12e0` and `11f8:26dd/284d`:
  valid floors 24/39/54/69/84/99, support and collision gates, adjacent Lobby
  extension, type-0 placeholder splitting, floor-key allocation, separate
  Lobby/floor-cell charges, and byte-stable TDT serialization round-trips.
- Exact separate-click Floor record behavior from `10a0:1310` and
  `11f8:17fd/284d/30ef`: disjoint clicked spans and their intervening gap are
  serialized as distinct records; automatic gaps on initial Lobby stories are
  type 24/status 0, while overlapping type-0 replacement preserves outside
  remainders.
- Complete custom-dialog presentation follow-up for
  `1018:0067`, `1060:00d3`, and `1100:3a39/3dc4/4138`: New/Load now uses the
  recovered 11-pixel paint and 13-pixel control fonts, cleared class cursor,
  explicit initialization show, and palette-realized immediate surface;
  command IDs 1..3 close with their ID as the result while every other
  WM_COMMAND returns FALSE;
  Person/Tenant Rename restore their shared palette/transparent/TA_UPDATECP
  immediate and WM_PAINT paths; Movie Choice uses its 13-pixel font and
  palette for both presentation phases; and Return/Space on Finance performs
  the original synchronous pressed-then-released button transaction before
  closing. Pure style/key plans and the affected suites are headless-tested.
- Exact shared DTMP initialization ordering at `1070:0005`: positive bitmap
  references resize from the DIB before DC acquisition without realizing the
  palette, while empty/negative references select and realize the logical
  palette, add `TA_UPDATECP`, release the DC, and then apply any nonzero header
  size. Every native DTMP dialog now enters this common boundary; pure tests
  cover both signed-resource branches and the zero-width no-resize case.
- Exact shared font lifecycle at `1208:0a8d/0ba7/0b6a`: startup enumerates an
  exact Arial face with MS Sans Serif fallback and seeds the nine-pixel entry
  with default output precision; requests below nine clamp to it, matching
  heights reuse one of ten process slots, later entries use TrueType-only
  precision, and a full bank returns before even searching an existing height.
  Dialog and Info painters borrow these handles, and teardown deletes the
  published slots in order. Pure tests cover clamp, lookup, insertion,
  saturation, and both initial/cloned creation specifications.
- Exact shared WinG utility contracts at `1208:07d5/09cf`,
  `1208:049d/069a`, and `1248:0000`: top-down 8-bit DIB allocation, DWORD row
  padding, BLACKNESS clear, all 256 B,G,R,0 palette entries, the fixed
  40+1024-byte supplied-resource pixel offset, and equal-size opaque clipping
  with matching source advancement. All 242 embedded BITMAPs are audited;
  odd widths include the 431-to-432-byte Info backing. Direct asymmetric tests
  also cover `1208:0603/063a` word/dword byte swaps. Portrait scaling is
  correctly mapped to `1100:364a/37a9/37d1`'s direct WinG calls.
- Exact `1100:3a39`/`3dc4` rename-dialog focus lifecycle: initialization
  installs the saved Person/Tenant name and returns TRUE without an explicit
  focus or selection change; the paint tail alone re-enables and focuses edit
  item 4. The native host does not add select-all behavior.
- Exact `10d8:006f` Find initialization focus boundary: `00a1-014b` populates
  and presents the resource-backed list, performs no explicit `SetFocus`, and
  returns TRUE for the dialog manager's default control selection.
- Exact painted-modal focus and transport-click contracts from
  `1068:00a1`, `1100:0f10`, and `1100:1248`: initialization never forces item
  1; Elevator and Stair/Escalator information consume every left click, retain
  their distinct palette selection sequence, and always restore TOPMOST plus
  the active modal target after portrait hit testing.
- Complete `1100:0116` Person and `1100:085b` Facility Information filter
  contracts: dialog-manager-selected initial focus; Person SetCapture plus
  ID-1 ReleaseCapture; DS:31a4 transfer around nested Rename; Facility
  palette-realized portrait dispatch and parent restoration on every click;
  and the exact all-command consumption/notification split for IDs 1, 7, and
  13 across rent and Movie dialog groups.
- Complete command routing for NAMEPEPLE/NAMETENANT/MOVIETITLE
  `1100:3a39/3dc4/4138` and FIND `10d8:006f`: notification-independent
  recovered control IDs, exact EndDialog results and DS:31a4 ordering, list
  notification 1/2/3 behavior, and no native-only WM_CLOSE route.
- Literal message-table contracts for ABOUTDLGPROC `1010:053f`, ELVPOPUP
  `1098:22f8`, ELVDLOGMAIN `1098:0628`, and CMDBTNSUBWNDPROC `1050:05a7`:
  exact seven/five/eight/seven message sets, About palette brush and any-ID
  timer with posted paint/FALSE return, and selector-specific down/up/outside
  cleanup semantics. WM_CLOSE and About's substituted WM_ERASEBKGND are absent.
- Complete NEWORLOADDLOGFILTER `1018:0067`, COUNTDLOGMAIN `1060:00d3`, and
  AHOTTADLOGFILTER `1068:00a1` tables: Startup returns FALSE for unknown
  commands and consumes only IDs 1..3;
  Finance paint, key, and both mouse paths use the common FALSE return;
  AHOTTA maps non-static control-color types to NULL_BRUSH and kills the
  supplied timer ID. Exact table/command/return plans are tested headlessly.
- Distinct SETUPSTARTUPDLGA/SETUPSTARTUPDLGB `1010:014c/0304` message
  contracts: modal Paint/Init/left-down with EndDialog(0), modeless
  Destroy/Paint/Init, and shared palette selection/realization before black
  fill and centered/clamped bitmap presentation.
- Exact ELVDLOGMAIN `1098:0ece-0f63` teardown plan: resume isolated Elevator
  state, clear the published control HWND before destruction, re-enable
  Map/Command/Info/Main in that order, omit an invented Main activation, and
  call DestroyWindow last. Neither WM_DESTROY nor WM_NCDESTROY is added to the
  recovered eight-message filter.
- Modal GDI ownership is separated from recovered filter routing. Native
  per-call translations of the original shared `DS:31ae` gray brush are
  released after DialogBox returns, leaving New/Load, AHOTTA/transport,
  Person/Facility information, Tenant/Movie rename/choice, and Find without
  synthetic destroy messages. Rename Person alone retains its explicit
  `1100:3ce8-3d0f` command-tail brush deletion and capture release.
- Exact `1000:0000/1258:000b/1128:0005` entry handoff and
  `1128:01d9-0223` visible startup tail. Authoritative class strings map
  DS:325a/325c/325e to Command/Info/Map; startup shows Map then Info, acquires
  `1050:03aa`'s BITMAP/300..302 command surfaces, shows Command then Main,
  selects Arrow, conditionally calls `1058:033c`, and always calls
  `1080:05a1`. The final synchronous presentation targets Command only; the
  prior native-only Main update is removed.
- Exact `1258:0345` class tail and `1128:05eb-09ea` application-window
  construction boundary. Command/Info/Map use the recovered creation order,
  positions, border-inflated extents, IDs 1000..1002, and z-band calls.
  Command's distinct `0x000a` flags make it TOPMOST and replace its inflated
  size with the raw 63x100 outer rectangle; Info/Map's `0x000b` flags retain
  their size. All four windows repeat the logical-palette realization,
  nine-pixel cached-font selection, `TA_UPDATECP`, and TRANSPARENT DC setup
  before first paint; Command retains its empty non-NULL class menu name.
  Pure specs test both classic and scaled border metrics.

The conservative exact-address source index now maps every currently classified
game-observable start: 911 exact starts and 1,421 unique source addresses. The 264
remaining uncited candidates are exhaustively confined to compiler/runtime
support segments `1000`, `1260`, and `1268`; their call/import evidence and
native toolchain-replacement boundary are recorded in
`NATIVE_RUNTIME_CLASSIFICATION.md`. Behavioral audit remains stricter than
address coverage:
the latest host-flow comparison restored `1258:0195-023a`'s idle audio and
auxiliary-window lifecycle plus independent effect-palette pass, all four
`1090:0448/0465/0615/06dc` per-frame palette passes, and `1090:0452`'s
mutually exclusive emergency-Security versus normal person-family branch.
Visual and interactive conformance testing against the runnable original
remains deferred until GUI execution is explicitly permitted.

The full `1128:03ad-0542` profile branch now includes `[Paths] Save`, not just
the five Sound words. Its 0x80-byte `DS:3120` buffer is supplied to both
recovered `GETOPENFILENAME` structures at `10d0:01a4-01ab` and
`10d0:0462-046b`; the native single-file default is the ripped
`C:\Maxis\Simtower` value and either original INI location still overrides it.

The enclosing `1128:003a-00ce` sequence now also runs the document-inactive
`10d0:086c/0ac2` temporary tower before DIALOG/124. Native applies its exact
frame `0x09e5` CLUT/1002 palette and startup-suppressed construction state
without treating that temporary tower as an open document, and republishes
Wait after clearing the splash class cursor at both recovered phases.

The same `10d0:0ac2` reconstruction now applies its final menu side effect:
command 40008 is enabled only when b406 bit three is active and b418 is zero.
New, successful Open, and the pre-dialog temporary tower all synchronize that
derived fire-crew state, preventing a loaded active-fire save from inheriting a
stale menu mark.

Its leading `1140:010d` transaction is now split from `10d0:086c` exactly.
The reset changes document/edit globals without replacing the active command
TABL or painting during Open I/O; successful reconstruction selects the rating
table, refreshes Command geometry and content synchronously, restores the
pending construction scratch outside b3ae isolation, and invalidates the
native visible-row cache. The zero argument from `10d0:0ac2` retains mode three,
while both promotion callers' one forces DS:783c to mode two. Derived view
publication now follows the recovered order: clamp/install the saved position,
compose Map synchronously, apply the fire-menu predicate, then execute the
caller's Main rebuild. New retains the prior b3f0/b3f2 view words; Open uses and
persists the loaded values after scrollbar clamping.

`11f8:0fea`'s elevator success split is also retained explicitly. Its existing-
shaft add-car branch returns from `122f` without touching DS:783c or Command;
the new-shaft allocation branch alone reaches `140d`, selects Finger mode, and
executes synchronous `1080:05a1`. Native construction results now expose that
distinction to the host, while both outcomes publish persistent mutation even
if a custom zero-cost table leaves the finance words unchanged.

The shared clock translation now explicitly maps `1208:05e6`, `11e0:0e84`,
`1000:39b5`, and `1000:39ea`: the WAVMIX service first applies the unsigned
48-ms rolling-deadline gate and advances a late anchor by only one interval;
the clock then returns signed arithmetic `GetTickCount() >> 4`, with wrapping
signed-magnitude delta where each caller uses it. Startup, palette, scheduler,
Find/Info, and audio thresholds consequently retain their original clock and
pump boundaries rather than being misread as millisecond literals.

The latest person-family comparison corrected Condo `1220:38e1` and Hotel
`1220:2e92` scheduling rather than approximating their shared-looking states.
Condo states 4/10/20/21/22 now preserve their distinct phase, resident-ordinal,
tenant-byte, random-consumption, and strict-frame gates; Hotel state 10 performs
phases zero through four without RNG. The shared Lobby discount at `11d8:0423`
now treats its wrapping elapsed word as signed in both normal Elevator stepping
and `1210:1332` isolation. The corresponding `10a8:12c1` renderer also uses
signed wait bands. During `b3ae` Simulate presentation, each waiting slot's
person identity comes from the saved Elevator snapshot and its projected metric
comes from the live simulated queue, exactly separating the two records.

Direct coverage now additionally anchors `1020:053e`'s twelve-color parity
table, `1220:6037` Cathedral, `11a8:02f2` commercial reset, `1220:067c`
activated-person initialization, `1208:0004/0cb5/0d75`, and the
`11e0:04c0/05d7/06d9` dialog GDI lifecycle. The current ledger contains 511
direct test citations and leaves 503 of the 911 mapped starts without a direct
test citation. All 14 Release/headless suites pass. Runtime visual/audio
conformance against the original remains deferred until GUI execution is
explicitly permitted.

The subsequent `1090:0192` comparison corrected the Elevator car initializer
at its byte boundary. Byte 13 now repeats the home floor, byte 14 comes from
the active schedule bank at Elevator offset `0x20 + calendar*7 + day phase`,
and a negative activation argument deliberately preserves byte 15. This keeps
loaded cars active across `1090:00d9` reconstruction and the full-shaft reset,
while newly added cars capture the current floor mode and activate explicitly.
Construction regressions distinguish the obsolete indices 14..27 from the
recovered indices 28..41 and seed byte 15 with nonzero sentinels.

Direct test evidence also now covers `11a0:0eaf`, `11b0:11af`, `1130:03f4`,
`1180:0352`, `1098:1ff5`, `11a0:027c`, and `10a8:0de6`. The `1130:0cec`
regression walks every reset record for Office, Condo, and Retail and all
Hotel type 3/4/5 guest spans, proving that owner ordinal zero remains intact.
The current ledger remains 911 exact native mappings and 1,421 unique source
citations; direct test citations rise to 520, leaving 494 mapped starts without
a direct test citation. All 14 Release/headless suites pass. Runtime visual and
audio conformance against the original remains deferred until GUI execution is
explicitly permitted.

The adjoining static batch now directly covers `1118:0143`, `10a0:0544`,
`1180:090a`, `11b0:0fa5`, and `11f8:2f5a`. The Info test constrains every
balance-dependent pixel to the original 70x14 field and the people test pins
the checkout audio latch. Elevator Finger double-click planning now preserves
the original miss asymmetry: the direction word clears, cursor/capture are
released, but the press latch remains armed for button-up; a hit clears both
words after opening Elevator Control. The complete entertainment-finish and
route-selector branches already matched and now have direct anchors. Decoding
`2f5a`'s 13-key and four-key tables found two floor-gate mismatches: type 19 is
not a ground-floor exception, and types 18/20/29 use the stricter `floor >=
b3e8` basement boundary after the common `b3e8-1` check. Native now preserves
both status codes and byte-atomic paired-constructor rejection. The coverage
ledger contains 911 exact native mappings, 1,422 source citations, and 526
direct test citations; 488 mapped starts lack a direct test citation. All 14
Release/headless suites pass. Runtime visual/audio conformance against the
original remains deferred until GUI execution is explicitly permitted.

Direct raster coverage now also anchors `1128:13fc`, `1118:026a`, and
`1118:073d`. The original 431x41 Info backing and 31x31 clock-face composition
already matched. Status rendering did not: native omitted `026a`'s explicit
current-position move to `(field.left+2, field.bottom)` after selecting
`TA_UPDATECP | TA_BASELINE`, so DrawText could inherit the balance field's
position. That MoveTo is restored in recovered call order, and headless pixels
now prove the 262x11 gray field, one-pixel white lower edge, clipped dark text,
and the clock's isolated hand changes. The ledger contains 911 exact mappings,
1,423 source citations, and 530 direct test citations, leaving 484 mapped starts
without direct evidence. All 14 Release/headless suites pass; runtime visual/
audio conformance remains deferred until GUI execution is explicitly permitted.

The complete `10d0:03f1` Save As path is now compared through its literal
`.TDT` strings and `1000:11e8/1394/1408` helper calls. Extension replacement
uses the last dot in the complete selected string rather than a path-aware
filename extension; validation uses the complete string's first dot and last
backslash with the original signed subtraction. Native tests preserve the
resulting ordinary, multi-dot, dotted-directory, eight-character, and
signed-negative edge cases. The ledger contains 911 exact mappings, 1,425
source citations, and 531 direct test citations, leaving 483 mapped starts
without direct evidence. All 14 Release/headless suites pass; runtime visual/
audio conformance remains deferred until GUI execution is explicitly permitted.

Direct evidence now also anchors `1118:0044/0368`, `1200:0037/058d`,
`1098:1644`, `1100:0644`, `10a0:1625`, and `11c8:0135/02c0`. Their recovered
rating/population raster bounds, clock phase arithmetic, Elevator Control cell
geometry, literal rent table, waiting-ring mutation order, and two-channel
stop loop already matched. The added tests cover every branch or visible
boundary without a top-level window or audio device. The ledger remains 911
exact mappings and 1,425 source citations; direct test citations rise to 540,
leaving 474 mapped starts without direct evidence. All 14 Release/headless
suites pass; runtime visual/audio conformance remains deferred until GUI
execution is explicitly permitted.

The complete static comparison of `10a0:10e8`, `1198:07e6`, `1210:0f0e`,
`1228:0b59`, `1100:2ec2`, and `1100:307e` adds six direct-test citations and
fixes two malformed-save signedness boundaries. Stair shape bytes in the
new-Elevator collision scan now follow the executable's `CBW`/arithmetic-`SAR`
half-height calculation rather than unsigned division. The Cathedral
person-lineup anchor word now follows signed `CMP`/`JGE` instead of an unsigned
threshold. The Parking chain/index rebuild, Elevator destination/transfer
selection, evening facility state table, and Movie two-row lineup required no
other translation changes. The ledger remains 911 exact mappings, with 1,427
source citations and 546 direct test citations, leaving 468 mapped starts
without direct evidence. All 14 Release/headless suites pass; runtime visual/
audio conformance remains deferred until GUI execution is explicitly permitted.

The latest direct static batch covers `1090:1d2f`, `11c8:06b6`, `1160:0000`,
`1228:0968`, and `1100:3856`. It confirms the existing Elevator direction,
facility sound, and Map initialization translations, corrects Medical Center
day/evening `+0x0c` preservation, and restores the signed type-7 Person
portrait comparison. The affected control-flow families have direct headless
regressions; all 14 Release suites pass. The current coverage index records
911 exact mappings, 1,429 source citations, 553 direct test citations, and 463
mapped starts without direct evidence. The refreshed native artifact matches
the tested build and passed static import and forbidden-reference scans without
being launched.

The latest static batch covers `1238:029f`, `1220:426c`, `1200:0543`, and
`1098:0780`. Native pool growth now has direct byte-complete evidence, and the
Retail wrapper has direct signed-phase coverage. The audit corrects the shared
clock helper from unsigned division to `1200:0543`'s signed CWD/IDIV quotient
and removes Elevator Control's invented phase clamp, preserving every raw byte
except the original exact-six-to-five substitution. All 14 Release suites
pass. Coverage records 911 exact mappings, 1,433 source citations, 558 direct
test citations, and 459 mapped starts without direct evidence. The refreshed
10,321,951-byte package is byte-identical to the tested build at SHA-256
`049FB38161BAE87E6B3C0FBF471531CC2660D2D73A80027D40E2D3ECA342E27C` and passes
the static import, forbidden-reference, and zero-process checks without being
launched.

Direct static evidence now also anchors `1220:49fa` and `1220:049a`. The
pending-facility clear's table-derived record count and bytes 4..15 behavior
already match the native fused activation initializer; nonzero sentinels now
exercise that clear across every owned Condo record. The Fast Food normal-pass
gate is corrected to retain `49fa`'s signed `phase >= 0` test as well as its
`phase < 4` test, preventing a negative high-bit clock phase from dispatching
or advancing RNG. The ledger contains 911 exact mappings, 1,433 source
citations, 560 direct test citations, and 457 mapped starts without direct
evidence. All 14 Release/headless suites pass; the refreshed standalone package
matches the tested build at SHA-256
`57CAE31292266B284518AB2C5000757BC7A547F78465BB4F425AE4E08ED41067` without
having been launched.

Direct static evidence now also anchors `1058:0000`, `1098:1895`,
`11f8:20e7`, `11f8:1452`, `1258:0345`, and `11a0:047c/088f`. The input,
construction, and opaque-cell routines already matched their recovered control
flow and pixel/record results. Elevator Control's complete floor-cell decision
and the original four-class WNDCLASS table are now production-consumed pure
plans with direct boundary tests; Main again registers its original
`TOWER_MENU` metadata. The ledger contains 911 exact mappings, 1,433 source
citations, 566 direct test citations, and 450 mapped starts without direct
evidence. All 14 Release/headless suites pass. The refreshed standalone package
matches the tested build at SHA-256
`D674974C0F671800ECF211C95A1346A61438B30FB858325A52EB33AB85ECAFAD` without
having been launched.

The latest quiet static batch directly audits `1170:0291`, `1180:06a8`,
`11f8:2291/321e`, `1258:000b/0186`, `1218:08cd`, `1178:0b44`, `1210:11c2`,
`1108:030d`, `1220:55b8`, `1100:1b53`, and `1100:1dca`. It restores the
Medical-unavailable waiting metric, Retail selector sign-extension, uncapped
direct maintenance debit, and Person Information's three DS:b3a6 source
contexts plus signed Lobby wait discount. The remaining audited branches
already matched and now have direct tests. The ledger contains 911 exact
mappings, 1,434 source citations, 577 direct test citations, and 437 mapped
starts without direct evidence. All 14 Release/headless suites pass; the
standalone package is byte-identical to the tested build at SHA-256
`F003C81B4CE899FF881A1BAA1D0C1FACAE820BF43E7F42F4FE1CC4A5AA791B19` without
having been launched.

The newest quiet static batch directly audits `11e0:0950`,
`1028:1534/1692`, `1220:5edd`, `11a0:0126/027c`, `1100:2031`, and
`1188:02ea`. Pixel/state/RNG tests now anchor the already-matching placeholder,
food-service, Cathedral, atlas, and commercial-meter translations. The save
loader now implements `02ea`'s 256-byte Pascal-name compatibility branch and
the recovered duplicate-key b402/b404 rebuild behavior; synthetic legacy and
duplicate records normalize and re-save exactly as the original path, while
all supplied saves retain byte-exact round trips. The ledger contains 911 exact
mappings, 1,434 source citations, 585 direct test citations, and 429 mapped
starts without direct evidence. All 14 Release/headless suites pass; the
10,330,051-byte package is byte-identical to the tested build at SHA-256
`925336B00E8FBA88B1F5E5B134905D2E44C1FB38FD2FB5280E5EE82EFE034FC2` without
having been launched.

The current quiet static batch anchors a connected information/people/UI
slice. `1100:22d5/232e` now has a complete normalization matrix for Movie,
Party Hall, Metro, and Cathedral parts plus Restaurant/Retail/Fast Food
subtype and saved-name precedence. `1228:07c5` is exercised through its real
facility-retirement consumer for every specialized span, while parallel
`1220:6d82/7005` Hotel/Condo owner transitions cover both sides of phase four
and byte wrap. Existing translations of `1058:03a9`, `1040:0179`,
`11a8:06b2`, `1220:1518/6e7d`, `10f0:01f9`, `1098:16a4`, `10e0:06cd`,
`1188:0793/0884`, `1208:0274`, `10a8:1a88`, and `11a0:0a11` match their
recovered branch/data behavior. Two shared edge cases were repaired:
`1220:50e2` now treats the frame word as signed for its `JLE` gates, and
`1108:014b` performs the original wrapping 16-bit absolute-distance sequence.
The ledger contains 911 exact mappings, 1,437 source citations, 612 direct
test citations, and 401 mapped starts without direct evidence. All 14
Release/headless suites pass; the 10,330,051-byte standalone package matches
the tested build at SHA-256
`F2917CAC067FF78943D1910465A61E93FB1E2EAD3B98E633A643581690F1DB05` without
having been launched.

The latest quiet static batch corrects three recovered UI transactions:
`1058:04e0` opens grouped command DIALOG/124 only while the physical primary
button is held and preserves the current selection otherwise; `1080:0a02`
repaints Main, Map, and Command synchronously in that order; and `10e0:0cea`
performs successful Find's Command, Map, preview-scratch, and focused-camera
steps in order. Direct regression evidence now also covers `1030:0000/0043`,
`1140:022c`, every `1208:0369` style/result mode, and
`1188:04db/0541/05a7`. The ledger contains 911 exact mappings, 1,439 source
citations, 622 direct test citations, and 391 mapped starts without direct
evidence. All 14 Release/headless suites pass; the 10,329,898-byte standalone
package matches the tested build at SHA-256
`BD763042C0685F3F9F6547DB05C5891076515BE329E5DE1D457882D597DA59C5`, imports
only Windows/UCRT DLLs, and passed forbidden-reference and exact-name process
checks without being launched or producing audio.

The newest quiet static batch restores the shared New/Open document-transition
tail from `10d0:001d/062a/0ac2`, `1080:0a02`, and `1118:0000`. Native now
presents the Map focus adjustment, updates the Fire Crew menu, then presents Main, Map,
Command, and Info synchronously; the repeated Map pass is retained. Exact
production-consumed plans also cover `11c8:02c0`'s sound/mixer/force/channel
gate, `1208:0cf5`'s wrapping signed-word GDI origin offset, and
`1208:0dfc`'s MessageBeep(0x30)/FatalAppExit(0) sequence. The latter removes
the native-only titled MessageBox and return-one boundary. Direct tests now
also cite the audited document confirmation, rent, entertainment span,
person-index, DTMP lifetime, brush/fill, name-table, finance, and focus
helpers. The ledger contains 911 exact mappings, 1,439 source citations, 644
direct test citations, and 368 mapped starts without direct evidence. All 14
headless suites pass; the 10,329,921-byte standalone PE matches the tested
build at SHA-256
`5B6EDEE62A70D2D1BA0B2876D09672F50CC964A0845190F2F81D187D48CD43CE`, imports
only Windows/UCRT DLLs, and passed forbidden-reference and exact-name process
checks without launch or audio output.

The latest quiet static batch repairs Map's direct-DC focus transaction
`1080:055d`, Person Information's Find-exit latch `1100:0000`, Cathedral
saved-name linkage `1188:0aa0`, signed Elevator dwell `1090:23a5`, and the
literal zero common-dialog Flags at `10d0:0122/03f1`. Exact
production-consumed helpers and tests now cover Finance text placement
`11e0:00ca`, Person Information meter endpoints/colors/logical-palette
brushes `11e0:01d8/0358`, and temporary white fill `1208:05a9`. Direct
evidence also expands across command selectors, facility-person atlases,
signed Map scaling, name-table maintenance, transport queues, schedule banks,
DTMP text origins, and adjacent Elevator waiting lanes. The ledger contains
911 exact mappings, 1,449 source citations, 706 direct test citations, and 310
mapped starts without direct evidence. All 14 headless suites pass; the
10,331,176-byte standalone PE is byte-identical to the tested build at
SHA-256
`6A1769EBA48EF16B919C651506005E8793C4FF2A220019F3C270A1474267A0F7`, retains
only Windows/UCRT imports, and passed the forbidden-reference scan without
launch or audio output.

The newest quiet static batch extracts `1100:4439`'s Facility Information
preview presentation into the shared native core. A memory-DIB test verifies
the complete 0xcccccc container fill, exact signed scaling/centering result,
nearest-neighbor retained-snapshot pixels, and restored stretch mode. The
shared `1020:0f4f` constructor now creates and exposes the exact 256-entry
`HPALETTE`, verified back through GDI. Elevator Control's `1098:27bd/2893`
schedule display now reproduces CBW sign extension, including `0xff -> -1`
and the corresponding `-30` departure value. Direct regressions additionally
anchor ordinal formatting, Movie/Party state transitions, wrapping signed
maintenance width, two-lane waiting focus, pending completion, paired service
allocation, and the Cathedral atlas compositor. The ledger contains 911 exact
mappings, 1,449 source citations, 720 direct test citations, and 297 mapped
starts without direct evidence. All 14 headless suites pass; the 10,332,533-
byte standalone PE matches the tested build at SHA-256
`B2EF681E432BC2F37567665CE220C67B2CF01A34FBCAC8ED0ACE23598B095A85`, imports
only Windows/UCRT DLLs, and passes the forbidden-reference and exact-name
process checks without launch or audio output.

The current quiet static checkpoint exposes exact production helpers for
parking assignment/floor decoding and eligibility, commercial
lane/capacity/revenue and both close scans, signed Win16 dialog centering,
About line style/text parsing, and reserved-audio submission. Direct tests
also cover Medical route-bank clearing, single-range construction debit,
saved-name prefix search, first-free parking allocation, command-grid
geometry, all Elevator Control row/cell plans, transit Find geometry, Map
overlay ordering, meter thresholds, Elevator car composition, and final
exterior roof/edge drawing. The ledger contains 911 exact mappings, 1,449
source citations, 750 direct test citations, and 267 mapped starts without
direct evidence. All 14 headless suites pass; the 10,333,795-byte standalone
PE matches the tested build at SHA-256
`5252EC826F35122FF476180427497B5422311B1D09A394BF606CDFC9AC2E13C3`, imports
only Windows/UCRT DLLs, and passes the forbidden-reference and exact-name
process checks without launch or audio output.

The latest quiet static checkpoint promotes exact Office occupancy, Stair
span/scoring/transfer routing, Lobby graphics-tier, Movie-length, commercial
closed-hours, route-boundary, and event-floor behavior into directly tested
core boundaries. Direct evidence also expands across retained presentation,
file transfer, accounting, Find ordering, transit queues and advisories, fire
follow-up, rating treasure, Hotel activation, Retail reset, magnifier
fallthrough, portrait geometry, Map dragging, and Elevator Control icons,
including signed, wrapping, first-match, and opposite-endian edge cases. The
ledger contains 911 exact mappings, 1,449 source citations, 816 direct test
citations, and 203 mapped starts without direct evidence. All 14 headless
suites pass; the refreshed 10,337,876-byte standalone PE matches the tested
build at SHA-256
`153002F79636081258C329B5270382E95BBBDEA562D6362F6776E8C556D76853`, imports
only Windows/UCRT DLLs, and passes forbidden-reference and exact-name process
checks without launch or audio output.

The newest quiet static checkpoint expands direct evidence across exterior and
world helpers, fresh-state sentinels, facility/parking/transport behavior,
clock and star rendering, people accounting and route selection, Find,
information thresholds, disabled-audio behavior, scheduled transitions,
command/Elevator Control presentation, resource startup, rename/save policy,
ALRT preparation, and Arial enumeration. The ledger contains 911 exact native
mappings, 1,449 source citations, and 979 direct test citations. Direct test
evidence covers 869 of the 911 mapped routine starts, leaving 42 host/runtime
boundaries without a direct citation; the other 264 recovered candidates are
the classified compiler/runtime support set rather than unmapped game-owned
routines. All 14 headless suites pass. The refreshed 10,338,821-byte standalone
PE is byte-identical to the tested build at SHA-256
`F4DD08CFEFDD55E40AC80D630B1C8ED99744B8015AED4105C78F13256F6B6AAE`, imports
only Windows/UCRT DLLs, and passes the forbidden-reference and exact-name
process scans without launch or audio output. Side-by-side runtime conformance
remains a required, explicitly deferred release gate.

The following host-boundary checkpoint promotes the opposite Bomb/Fire
sound-and-damage order and their optional process tails into production-consumed
plans, and shares `11f8:0793`'s complete point-hit-demolition transaction with
the native host. Direct regressions also cover Info expiry, direct construction
debit, WAVMIX completion state, shutdown confirmation, Elevator-isolation
presentation, wrapped floor-offset precomputation, and GDI font-bank teardown.
The ledger contains 911 exact native mappings, 1,449 source citations, and 991
direct test citations. Direct evidence covers 879 mapped routine starts; the
remaining 32 are individually audited platform adapters, native ownership
replacements, or no-inbound artifacts in `HEADLESS_BOUNDARY_AUDIT.md`. All 14
headless suites pass. The refreshed 10,339,643-byte standalone PE matches the
tested build at SHA-256
`6CA9465496879E58DD44382E6B06F06EB30570FBA862193DD967C25589DC4D12`, imports
only Windows/UCRT DLLs, and passes forbidden-reference and exact-name process
checks without launch or audio output. Runtime side-by-side conformance remains
the required release gate.

The latest static presentation checkpoint replaces two broad native repaint
shortcuts with recovered transactions. `1058:05f8` now consumes a tested
Main-rebuild/Map-focus-only order, and `1038:002f` plus the construction,
demolition, and Elevator callers consume a tested world-mutation plan that
never invents an Info or Map invalidation. `1178:01db/027c/0697/076f/07e8`
finance changes still repaint Info synchronously in their recovered
sound/dialog order. This also prevents paid Bomb ransom from entering the
full-world path and restores immediate Info presentation for the fresh-tower
bonus and buried treasure. The complete startup splash create/update/destroy
transaction was re-audited and already matched. The ledger contains 911 exact
native mappings, 1,452 source citations, and 993 direct test citations; 881
mapped starts have direct evidence and 30 classified platform boundaries do
not. All 14 headless suites pass. The refreshed 10,339,229-byte standalone PE
matches the tested build at SHA-256
`9B70620BB389CA650B0527E3D4DD20A16B154F45AD5106559F4F6DB8D0ECDDAC`, imports
only Windows/UCRT DLLs, and passes forbidden-reference and exact-name process
checks without launch or audio output. Runtime side-by-side conformance
remains the required release gate.

The launcher-lifecycle follow-up directly tests and production-consumes
`1010:049e`'s stop/direct-deactivate/modal/direct-activate order. This removes
the native About path's redundant second channel stop and, more importantly,
keeps the original `11c8` active latch unchanged across the modal dialog.
New/Load command IDs 1..3 now release their parsed DTMP value before
`EndDialog`, matching `1018:01d3`'s call to `1070:051f`; splash teardown also
clears its retained native state after window destruction. The ledger contains
911 exact mappings, 1,452 source citations, and 994 direct test citations.
Direct evidence covers 882 mapped starts, leaving 29 classified host/platform
boundaries. All 14 headless suites pass. The refreshed 10,339,389-byte package
matches the tested build at SHA-256
`9EBB0962CD9E595E6F7A172869CED6EB11100BE4BC40735A22D47B2513B10217`, imports
only Windows/UCRT DLLs, and passes the forbidden-reference and exact-name
process checks without launch or audio output.

The final quiet boundary batch production-consumes the exact launch/ownership
contracts for `1018:0000`, `1060:0083`, `1098:0000`, `10d8:0000`,
`1100:39df`, `1100:3d5b`, and `1100:40d5`. It restores Main ownership for
both Rename dialogs, preserves Movie Choice's real `DialogBox` result, forwards
the initiating pointer into Elevator Control, removes its redundant native
Show/Update tail, and releases modal DTMP state before every recovered
`EndDialog` path. Direct contracts also cover `1000:2140`, `1070:06cd`,
`10d8:0487`, `11e0:0000/0026`, `1128:0ba3`, and `1208:0083`. The complete
`10b8:0039/011e` teardown group order is executable, and native fixed ownership
derives from `1020:0019/008f`, `1050:03aa/0503`, `1090:0014/0074`,
`1170:0014/004e`, `11e0:0cfb/0d44`, and `1238:001e/0073` block contracts.
The ledger contains 911 exact mappings, 1,452 source citations, and 1,019
direct test citations. Direct evidence covers 907 starts; only process entry,
two live PCM calls, and one unreachable debug formatter remain classified in
`HEADLESS_BOUNDARY_AUDIT.md`. All 14 headless suites pass, and the packaged
10,341,922-byte PE matches the tested build at SHA-256
`4EC0ADB9F5774A5D256DB400B77FECA915BA9AFE892A617A33943E69BA9B061D`.

The permitted host-muted runtime audit compared fresh original and native
startup/New states, then replaced modern caption and Explorer-picker host
substitutions with the recovered Win16 presentation. The subsequent reciprocal
save audit used a native-writer fixture to produce a deliberately
balance-mutated 65,150-byte revision-0x24 `NATIVE.TDT` at SHA-256
`C99E96B40A66329EEB9B38D3478F3BB77DFC6405DE31C854EB21B731AA706F76`.
The supplied original accepted and rendered it as `SimTower - NATIVE` with
Fund `$234567800`; native then reopened the same stream. That reciprocal check
found a modern quoted association-path edge, now handled by a narrow host
adapter with the recovered Win16 target parser unchanged. All 14 headless
suites pass. The refreshed 10,354,395-byte package matches the tested build at
SHA-256
`6715E198FA7F6E0089FD7BB606BC1FD7987C864DF147166D443A6AB49CAAB6E2`,
imports only Windows/UCRT DLLs, and passes the forbidden-reference and
exact-name process scans. Audible PCM submission and longer interactive timing
remain runtime-certification work.

The closing silent runtime pass completes that work without making speaker
noise. A hidden loaded-state run remained responsive for 90 seconds with
private memory stable at 6,586,368 bytes and GDI/USER counts stable at 26/30.
The production `11c8:006b/0920` native backend then opened WAVE_MAPPER,
prepared and wrote WAVE/20000's 50,300-byte, 11,127-Hz, 8-bit format, observed
channel zero active, and completed reset/unprepare/close. Only this explicit
hardware smoke replaced samples with same-format digital silence; normal
execution submits the exact embedded resource bytes. Coverage is 911/911 exact
mappings, 1,452 native-source citations, 1,021 test citations, and 909 directly
cited mapped starts. All 14 deterministic suites and the manual hardware smoke
pass. The final timing comparison then exposed native Fast Mode advancing at
modern busy-loop speed. The recovered `1200:0196` dispatcher remains exact;
the native Win16 host adapter now enforces the reference-observed 58-ms
full-frame cadence (approximately 777 frames per 45 seconds). Separate-desktop
native captures at the beginning and end of a responsive 90-second run both
retain `1st WD/1Q/1st Year`. The final 10,355,049-byte package matches the
tested build at SHA-256
`8D9AEE8B5D3016F3C3D8ACC8578A21602953C113CF1B0572F4F80AD521450316`,
retains only Windows/UCRT imports, passes the forbidden-reference scan, and
leaves no emulator/game/media process running.

The latest headless correction follows `11f8:033a`'s type-indexed resource
formula instead of treating type 46's BITMAP/3944..3949 fire atlas as an empty
Floor atlas. Type zero now draws its fixed fresh/bulldozed status cell from
BITMAP/1000..1003 across the full span, and type 47 draws its bottom-aligned
BITMAP/4008 rubble. The existing demolition replacement and construction
preflight are now joined by an integration regression that demolishes to
type-zero/status-two and rebuilds both Floor and Office on those same cells.
The New Tower/Load Tower chooser also uses the live splash as its native modal
owner so the splash cannot cover and disable it. All 14 Release suites pass.
The packaged 10,359,510-byte PE matches the tested build at SHA-256
`628D8933CD25C8E3A76425C9F10A5BCF57D862AC3DD9BF9245FF216BAEC52701`,
has only Windows/UCRT imports, passes the forbidden-reference scans, and leaves
no emulator/game/media process running. The game and emulator were not
launched for this correction.

The subsequent `1038:00a9/050e/06a8` renderer audit adds the omitted type-45
BITMAP/3880 Metro boundary layer and corrects the separate phase rules: only
type 45 is absolute-world phased, while pending construction and type-47
damage start at tenant-relative cell zero. It also restores Office's signed
status/full signed variant-word selector, Security's unconditional frame zero,
and Party Hall's linked dc24 state with the state-three-or-later frame-two
clamp. The fixtures use contradictory tenant fields and non-aligned spans to
prove the recovered selectors independently. All 14 Release suites pass. The
10,362,070-byte package is byte-identical to the tested build at SHA-256
`1DFB73A2D19F83D76CF348E722C6D8C4C7AF136B6E871CA4F26C074BC72F4B79`,
retains only Windows/UCRT imports, passes the forbidden-reference scans, and
leaves no emulator/game/media process running. No executable was launched.

The construction-input audit also restores `11f8:3df4-3e2e`: type-24 Lobby is
the sole unshifted vertical snap, while Floor, Office, all ordinary facilities,
Elevators, and vertical transports add twelve pixels after snapping. This
corrects every one-story construction outline that native had displayed twelve
pixels too high and preserves the signed negative-coordinate IDIV edge in the
placement transform. Direct outline and placement regressions pass with all
14 Release suites. The refreshed 10,362,070-byte package matches the tested
build at SHA-256
`5575DA071AEFB549E4EA2BDF4DC87AA454CA3FC163DD9086DA5DCAAC475B1866`.
It was not launched.

The corresponding client-coordinate edit regression now traverses
`11f8:3da4`, `11f8:3d2d/3e3e`, and `11f8:0793/35ac` without precomputed model
coordinates: it builds and activates an Office, bulldozes it to a merged
type-zero/status-two Floor interval, and immediately rebuilds on the same
visible point. Recovered `11f8:17fd/30ef` separately confirms that spacing a
new facility away from its neighbor intentionally auto-constructs the Floor
between them. All 14 suites pass; the packaged 10,362,070-byte PE remains
SHA-256 `5575DA071AEFB549E4EA2BDF4DC87AA454CA3FC163DD9086DA5DCAAC475B1866`.
No executable or audio path was opened.

The scheduler now preserves both `1208:05e6` samples around `1200:0196`.
`01ac` supplies only the signed six-tick admission comparison; `0529` takes a
fresh post-callback sample after scheduled far calls and stores the baseline
used by the next frame. This prevents long modal callbacks from collapsing the
following wait. The same audit replaces C++ signed increment with the exact
wrapping `1200:04b3` DWORD increment and directly tests the CMP/JL overflow
edge. All 14 suites pass. The packaged 10,362,166-byte PE has SHA-256
`3CD22AE1609D1C147B2CA51F20A20247B9E609B8D11BA688924E653FA927F7C5`;
no visible or audible process was launched.

The subsequent screenshot audit follows all five exterior calls through
`11c0:0000 -> 1208:071f -> 1248:0000 -> 1250:0024` and restores their
transparent-index-zero semantics. This removes the native-only white rectangle
around BITMAP/1002's roof crane and preserves lower layers through palette-zero
holes in both ordinary and ground edge fragments. The pixel suite now proves
that behavior independently for each fragment class. Construction coverage
also includes the reported neighbor topology: a demolished Office coalesces
with the automatic Floor gap and the Floor to its right, then rebuilding at
the former coordinate splits that combined type-zero/status-two interval and
preserves the left Office. All 14 suites pass. The packaged 10,362,166-byte PE
has SHA-256
`91A2F4A5F8BDEF7630EDFD8118284EEE5DEB61C0D9546FF8261F264AE572BF2F`;
its imports and forbidden-reference/process scans are clear, and it was not
launched.

The completed `1090:03ab` ordering pass replaces remaining frame-level
batching with the recovered call topology. Each used Elevator owns its eight
car-state calls, eight passenger pairs, and one audio checkpoint; movement
sound and person-family host dispatches occur inside those loops at the exact
mutation boundary. Preview-only and full frames preserve `11f8:3b94/3c13`'s
DS:025c latch plus two restore/three draw checkpoints. World renderer
checkpoints execute before construction-outline and palette work, palette
recoloring no longer contaminates DS:31cc's dynamic-layer gate, changed
preview rectangles present directly, and the Main/Command/Info/status/palette
tail follows `1090:061f-06ed`. Pure plan tests, mutation-observing Elevator
callbacks, and dirty/clean renderer checkpoint counts cover the new boundary.
All 14 optimized Release suites pass. The 10,369,638-byte package is
byte-identical to the tested build at SHA-256
`23E52DDBE182E9CD9A9D3C4D4210B254E54D6AFCC5216EDCAEE95EF48E4ACA0B`;
it retains only Windows/UCRT imports and passes forbidden-reference and
exact-name process scans without being launched.

## Rejected prototype

The 109-module TypeScript simulation and its 63 focused tests are retained as a disposable experimentation harness. Its modules, line count, address comments, and tests do not count as original-function coverage because the implementation was not translated from this supplied executable and its behavior was not equivalent to the game.
