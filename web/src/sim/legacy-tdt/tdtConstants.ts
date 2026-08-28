/**
 * Constants, size/offset tables, and the typed error for the 1994 `.TDT` save
 * format. Extracted from `tdtFormat.ts` (the pure binary walker); layout facts
 * come from `docs/canon/tdt-format.md`. No game semantics live here.
 */

/** A `.TDT` file that can't be read. `message` is always player-readable. */
export class LegacyImportError extends Error {}

/** Header magic: the u16 at offset 0 is `0x2400` in every known save. */
export const TDT_MAGIC = 0x2400;

/** Fixed header block size; the floor map starts at offset 560 (doc §1;
 *  confirmed by tower-docs, and by the arithmetic 560 + 10 × 194 empty
 *  basement records = 0x9C4, the ground-floor offset seen in real saves). */
export const TDT_HEADER_SIZE = 0x230;

/** Floor slots in the file: indexes 0–119 (doc §4). */
export const TDT_FLOOR_COUNT = 120;

/** One tenant record is 18 bytes (doc §4). */
export const TDT_TENANT_RECORD_SIZE = 18;

/** Each floor record ends with a 94-entry u16 index map (doc §4). */
export const TDT_FLOOR_INDEX_ENTRIES = 94;

/**
 * Hostile-input ceilings. Each floor record carries a 94-entry index map into
 * its tenant array (doc §4), so ~94 is the format's own structural bound on
 * tenants per floor; 256 is a generous ceiling above that (in case the map's
 * role is misunderstood upstream) that still refuses absurd counts before any
 * loop runs. The size ceiling comfortably holds a maxed tower (~120 floors ×
 * a few KB) while refusing a multi-gigabyte allocation.
 */
export const TDT_MAX_TENANTS_PER_FLOOR = 256;
export const TDT_MAX_FILE_BYTES = 4 * 1024 * 1024;
/** People cap for the tolerant tail walk (canon max census is 15,000; fast
 *  food counts workers+customers, so real files run higher; 100k is far past
 *  any legitimate save while still refusing absurd skip lengths). */
export const TDT_MAX_PEOPLE = 100_000;

/**
 * Byte sizes for the tail blocks (doc §6–§10): 16-byte person records after a
 * u32 count; 512 × 18-byte retail rows; 24 elevator entries (194-byte header
 * each, built shafts appending a 3,140-byte fixed block, then one 324-byte entry
 * per SPANNED floor and a SINGLE 348-byte car block; see
 * {@link builtShaftPayloadSize}, which owns that arithmetic); a 132-byte finance
 * block; a 1,026-byte parking block; and 64 × 10-byte stair/escalator records.
 */
export const TDT_PERSON_RECORD_SIZE = 16;
/** Upper bound the exporter clamps its people count to: the canon TOWER census
 *  (15,000). Bounds the people block a forged save can inflate (each unit adds
 *  16 bytes) while still covering any legitimate tower. */
export const TDT_MAX_CENSUS = 15_000;
/** Default saved view-scroll the exporter writes into the header (0x26 = x,
 *  0x28 = y, in world pixels): the value the 1994 game itself stores for a fresh
 *  New Tower, which opens the view on the ground lobby. Left at 0 a loaded tower
 *  opens at the top-left sky instead of on its entrance. Used only when the
 *  save carries no view of its own (see `viewWordsFromView`). */
export const TDT_DEFAULT_VIEW_X = 1105;
export const TDT_DEFAULT_VIEW_Y = 3491;

/** TDT floor index → our floor: uniform `ours = tdt − 9` (doc §4, proven by
 *  the lobby table; TDT 10/24/39/… = floors 1/15/30/…). Lives here (not in
 *  tdtImport) because the pure view-word mapping needs it too. */
export const TDT_FLOOR_OFFSET = 9;

/**
 * 1994 world metrics for the header's view-scroll words. Horizontal: tenant
 * extents are 8-pixel segments == our tiles (doc §4), on a 375-segment lot
 * (3,000 px). Vertical: 36 px per floor across the 120 floor slots (4,320 px).
 * The window size is DERIVED, not documented: anchoring on the known New
 * Tower default (1105, 3491) which "opens on the ground lobby", a 469-px
 * client height puts 3491 + 469 = 3960 exactly at the bottom edge of TDT
 * floor 10 (the ground floor pinned to the bottom of the screen), and 640 is
 * the 1994 display width. The round-trip tests pin this anchor.
 */
export const TDT_TILE_PX = 8;
export const TDT_FLOOR_PX = 36;
export const TDT_WORLD_W = 375 * TDT_TILE_PX;
export const TDT_WORLD_H = TDT_FLOOR_COUNT * TDT_FLOOR_PX;
export const TDT_VIEW_W = 640;
export const TDT_VIEW_H = 469;

export const TDT_RETAIL_SLOTS = 512;
export const TDT_RETAIL_RECORD_SIZE = 18;
export const TDT_ELEVATOR_SLOTS = 24;
export const TDT_ELEVATOR_HEADER_SIZE = 194;
// Built shafts append a fixed block whose true size (3140 B) was measured from
// real 1994 saves via the tools/simtower round-trip harness: walking the three
// shafts in my_tower.TDT, the record stride
//   194-byte header + 3140 + 324 * spannedFloors + 348
// reproduces every shaft's exact file offset. The earlier 480 + 2 * 120 = 720
// estimate undercounted this block by 2420 B, which desynced the whole table
// after the first shaft and forced the importer to synthesize fakes instead.
// The trailing 348-byte block is cars-INDEPENDENT (one per shaft, NOT `* cars`):
// harness-confirmed against the retail game in 2026-07-13. See TDT_ELEVATOR_CAR_BLOCK_SIZE.
export const TDT_ELEVATOR_BUILT_FIXED = 3140;
export const TDT_ELEVATOR_PER_FLOOR_SIZE = 324;
/** The single 348-byte car block appended per BUILT shaft. It appears exactly
 *  ONCE per shaft regardless of the car count (harness-confirmed on the real
 *  1994 game): never multiply by cars. The former name `TDT_ELEVATOR_PER_CAR_SIZE`
 *  wrongly implied a per-car block and invited a `* cars` regression, so it was
 *  renamed to `..._CAR_BLOCK_SIZE`. */
export const TDT_ELEVATOR_CAR_BLOCK_SIZE = 348;
/**
 * The 56-byte per-shaft schedule/config block (bytes 4–59 of an elevator
 * header) the 1994 game reads to dispatch cars. Every built shaft in a sampled
 * real save carried this exact default; a zero-filled block instead reads as
 * "run no cars", so the shaft loads with EMPTY cars and traps everyone
 * (confirmed via the SimTower harness). The exporter emits this so exported
 * shafts actually run. The precise per-hour scheduling model (the WD/WE strip
 * in the elevator editor) is a separate feature; see the backlog's
 * `elevator-scheduling`. Layout: 14 bytes 0x01, 14 bytes 0x05, 28 bytes 0x00.
 */
export const TDT_ELEVATOR_SCHEDULE_DEFAULT: readonly number[] = [
  ...(Array(14).fill(0x01) as number[]),
  ...(Array(14).fill(0x05) as number[]),
  ...(Array(28).fill(0x00) as number[]),
];

/**
 * Bytes a BUILT elevator slot appends after its 194-byte header: the fixed
 * block, one per-floor entry for every floor the shaft SPANS (`bottomFloor..
 * topFloor` inclusive, whether or not it stops there), and a SINGLE car block.
 *
 * The writer, the reader's skip, and the test fixture must agree byte for byte
 * or the whole elevator table desyncs after the first shaft, so they all call
 * THIS function rather than re-typing the arithmetic. Both operands have been
 * wrong before, each time because every reference save on hand happened to make
 * the wrong rule indistinguishable from the right one: `* cars` looked right on
 * 1-car saves, and counting serviced floors looked right until a save arrived
 * whose shafts skip floors. Floors are TDT floor bytes, not our floor numbers.
 *
 * A degenerate span (`top === bottom`) sizes as one entry; an INVERTED pair is
 * not a shaft the game can have written, so it throws rather than return a
 * nonsense (possibly negative) size that a caller would pad or skip by. A
 * FRACTIONAL span throws for the same reason: it returns a fractional size,
 * which a writer pads up to the next whole byte while a reader skips the
 * fraction, so the two disagree by a byte with nothing to show for it.
 *
 * Both operands must be in the SAME space (TDT floor bytes at every call site
 * today). Only their difference is used, so a uniform offset cancels out; what
 * would NOT cancel, and what nothing here can detect, is passing one operand in
 * engine floors and the other in TDT bytes.
 */
export function builtShaftPayloadSize(bottomFloor: number, topFloor: number): number {
  const spannedFloors = topFloor - bottomFloor + 1;
  // Check the OPERANDS, not just their difference: two matching fractions
  // (1.5 and 3.5) cancel into a whole span while describing floors that do not
  // exist, so the size would look valid and mean nothing.
  if (!Number.isInteger(bottomFloor) || !Number.isInteger(topFloor) || spannedFloors < 1) {
    throw new RangeError(
      `elevator span must be a whole number of floors, at least one (bottom ${bottomFloor}, top ${topFloor})`,
    );
  }
  return TDT_ELEVATOR_BUILT_FIXED + spannedFloors * TDT_ELEVATOR_PER_FLOOR_SIZE + TDT_ELEVATOR_CAR_BLOCK_SIZE;
}

/** Elevator `type` byte for an express shaft (first entry in ELEVATOR_KINDS).
 *  Named because the payload rule below turns on it. */
export const TDT_ELEVATOR_TYPE_EXPRESS = 0;

/**
 * One built shaft's payload size the way the 1994 GAME sizes it, by kind.
 *
 * Standard and service shafts hold one per-floor entry for every floor they
 * span. An EXPRESS shaft holds one for every floor it STOPS at. That is the
 * only place the two rules can differ: an express is the kind that skips most
 * of what it spans, while a standard or service shaft usually stops everywhere
 * it passes, which is why this went unnoticed for so long. (Usually, not
 * always: a standard shaft CAN be given skip floors, and whether the game spans
 * or stops for that case is unmeasured. Both this reader and our writer span it,
 * per the 2026-07-13 measurement in doc §8.)
 *
 * **Our own EXPORTER does not call this, on purpose.** It sizes every kind by
 * span via {@link builtShaftPayloadSize}, express included, and the game accepts
 * those files, losing only what follows the first express (which the express-
 * last ordering makes the second express alone; re-save-measured 22 of 23 kept).
 * Aligning the writer with the game looks like the obvious follow-up, but the
 * one stop-sized-express experiment run so far fared far WORSE in the game
 * (8 of 23 kept), confounded though it is with express position. Do not "fix"
 * the exporter to call this without isolating that; see issue #740 and doc §8.
 *
 * Harness-measured 2026-08-04 against a save retail SimTower wrote: an express
 * spanning floors 10..100 and stopping at 8 of them occupies 6,274 bytes, which
 * is `194 + 3140 + 324 * 8 + 348`. Sizing that same shaft by its 91 spanned
 * floors gives 33,166, so a writer that spans runs 26,892 bytes long and the
 * 1994 game loses every shaft after the express. Issue #740 and doc §8.
 *
 * `servicedCount` is clamped up to one entry. Our writer always stops at both
 * endpoints so it cannot emit zero, and a file claiming zero is malformed
 * either way; what matters is that the reader and the writer size such a record
 * the SAME, which they do only by both coming through here.
 */
export function builtShaftPayloadSizeFor(
  type: number,
  bottomFloor: number,
  topFloor: number,
  servicedCount: number,
): number {
  // Validate the SPAN for every kind, express included. The express branch does
  // not use the span to size anything, but this is the entry point a writer is
  // told to call, and an inverted or fractional span is a bug either way: it
  // would be caught for a standard shaft and wave through for an express, which
  // is the least useful place to be lenient.
  const spanned = builtShaftPayloadSize(bottomFloor, topFloor);
  if (type !== TDT_ELEVATOR_TYPE_EXPRESS) return spanned;
  if (!Number.isInteger(servicedCount)) {
    throw new RangeError(`express stop count must be a whole number of floors (got ${servicedCount})`);
  }
  // Expressed as a 1..n range so ALL of the arithmetic stays in one function.
  return builtShaftPayloadSize(1, Math.max(1, servicedCount));
}
export const TDT_FINANCE_SIZE = 132;
export const TDT_PARKING_SIZE = 2 + 512 * 2;
export const TDT_STAIR_SLOTS = 64;
export const TDT_STAIR_RECORD_SIZE = 10;
/** Stories a stair-table record spans, from its type ordinal: 0-1 one story,
 *  2-3 two, 4-5 three. One source for the exporter's flight accounting and the
 *  importer's reconstruction, so the two can never disagree. */
export function tdtStairStories(type: number): number {
  if (type <= 1) return 1;
  if (type <= 3) return 2;
  return 3;
}
/**
 * Size of the trailing routing/reachability region the 1994 game reads AFTER
 * the stairs table (doc §11+: lobby/reachability tables and related caches).
 *
 * The importer skips this region (its live crowd re-simulates), but the real
 * game reads it on load, to an extent that depends on the file's own content
 * (doc §11: its own re-saves carry far less than this constant and re-open
 * fine). Our exporter used to end right after the stairs table, well short of
 * what the game read from our files, so it read off the end and page-faulted
 * (0x0799, surfaced as "This file is already open, or damaged"). Emitting this
 * fixed region is the over-emit that keeps every measured case (through 4-star
 * towers) safely inside the file.
 *
 * It is filled with 0xFF, the format's empty-slot sentinel. That choice is
 * load-bearing and was confirmed with the SimTower harness (tools/simtower/):
 * ZERO-filling the same span loads but makes the game read the zeros as live
 * routing data and invent a phantom population (an empty tower reported Pop
 * 1,280 and a bogus star), whereas 0xFF reads as "every slot empty", so the game
 * rebuilds reachability and the crowd from the floor map and reports the right
 * population (an empty tower loads at Pop 0).
 *
 * This fixed size is validated against real saves for Classic towers up to two
 * stars. The exact per-tower size for larger towers (more elevators, sky
 * lobbies, big crowds) is not yet pinned down and is a follow-up; see the
 * backlog's `tdt-export-routing-tail` row. Over-emitting is safe (the game
 * ignores trailing slack); under-emitting is the crash, so this is generous.
 */
export const TDT_ROUTING_TAIL_SIZE = 0x6400;

/**
 * Trailer stamped on the very end of a `.TDT` WE write, so a later reader knows
 * which of our writers produced it instead of inferring that from the bytes.
 *
 * The importer spent four review rounds telling one of our own payload layouts
 * from another by hunting for structures at known offsets, because a file
 * carries no statement of who wrote it. Every one of those tests is an
 * inference a truncated or unusual file can defeat. A stamp is a fact.
 *
 * Layout: the ASCII magic, then a u16 generation, little-endian like every other
 * word in the format. It goes AFTER the routing region, at the very end, which
 * is the one place bytes can be added without disturbing anything the 1994 game
 * reads: the game reads a fixed extent and ignores trailing slack (our exports
 * already run ~150 KB past its own re-saves and load fine). That placement is
 * harness-verified against the real game, not assumed; see
 * docs/canon/tdt-format.md §12a.
 *
 * A file WITHOUT the trailer is one of ours from before it existed, or a save
 * the 1994 game wrote; the reader keeps its existing inference for those. This
 * cannot help a file already in the wild. It exists so the NEXT layout change
 * is a fact rather than another round of guessing.
 */
export const TDT_STAMP_MAGIC = "VCTDT";
/** Bumped when OUR writer changes the bytes in a way a reader must know about.
 *  1 = every built shaft sized by its SPANNED floors, express included. That is
 *  NOT what the 1994 game writes for an express (see
 *  {@link builtShaftPayloadSizeFor}), but it is what our writer emits and what
 *  the game accepts, so it stays generation 1 until the writer itself changes. */
export const TDT_STAMP_GENERATION = 1;
/** Magic bytes plus a u16 generation. */
export const TDT_STAMP_SIZE = TDT_STAMP_MAGIC.length + 2;

/** Sanity bounds used to recognize a real stair record while scanning for the
 *  table: a built flight sits within the tower's tile span, on a valid floor,
 *  with a plausible waiting-crowd count. Generous on purpose (validation, not
 *  gameplay): the goal is to reject finance/parking bytes, not to clamp data. */
export const TDT_MAX_TILE = 800;
export const TDT_MAX_STAIR_CROWD = 4000;
/** How far past the elevator table to scan for the stairs table (bytes). The
 *  finance block (132) + parking/lobby region between the elevator table and the
 *  stairs table measured ~0.7 KB in a real save and is bounded by parking's
 *  512-stall table (~1 KB); 4 KB covers that with slack while keeping the scan
 *  away from the far §11 lobby/§12 named-tenant blocks (smaller window = less
 *  chance a coincidental later window out-counts the real table). */
export const TDT_STAIR_SCAN_WINDOW = 4096;
