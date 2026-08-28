/**
 * The raw intermediate model of a `.TDT` file: a dumb mirror of the bytes with
 * no game semantics applied. Extracted from `tdtFormat.ts`. The game-semantic
 * mapping (tenant IDs → FacilityKind, money ×100, frame → clock) lives in
 * `tdtImport.ts`.
 */

/** One tenant record, mirrored raw from the file (doc §4 / tower-docs unit
 *  record: extents, type, status byte, retail variant (byte 6), 9 reserved
 *  bytes, rent (byte 16), byte 17). */
export interface TdtTenant {
  /** Left/right extents in 8-pixel segments == our tiles (half-open range). */
  left: number;
  right: number;
  /** Tenant type ID (doc §5). Negative ⇒ under construction. */
  type: number;
  /** Status/flags byte at offset 5 (a bit field for hotels; nonzero ⇒
   *  tenanted for offices/condos, per the doc). */
  status: number;
  /** Retail variant ordinal at offset 6 (the first byte after status). The
   *  real 1994 game stores a shop/fastFood/restaurant's canon variety HERE, not
   *  at offset 17 as the docs implied: confirmed against game-written saves
   *  (my_tower's fastFood read 3,1,2,4,0 = Ice Cream/Chinese Cafe/Hamburger/
   *  Coffee/Soba, matching the readable "BURGER" stand in the Wine render), while
   *  offset 17 is 0 in every real save. 0 for non-retail. */
  variant: number;
  /** Rent/lease rate byte at offset 16: 0 very low … 3 high, 4 no rate
   *  (unused in v1; see the backlog's tdt-importer row). */
  rentRate: number;
  /** Byte at offset 17. Long assumed to be the retail variant (it is not; see
   *  {@link variant}); 0 in every real save's retail. Hotel dirty-days may live
   *  here (unconfirmed). Unused in v1. */
  subtype: number;
}

/** One floor record: built extent plus its tenant list. */
export interface TdtFloor {
  /** TDT floor index 0–119 (0–9 = B10…B1, 10 = ground; doc §4). */
  index: number;
  /** Built extent in tiles (half-open; equal ⇒ nothing built here). */
  leftEdge: number;
  rightEdge: number;
  tenants: TdtTenant[];
}

/** The fixed-offset header fields v1 consumes (doc §1). */
export interface TdtHeader {
  /** File-format version word; `0x2400` (the magic). */
  version: number;
  /** 1–5 = star rating, 6 = TOWER. */
  level: number;
  /** Funds in STORED units; display dollars are ×100 (doc §2). Signed. */
  balance: number;
  /** Time of day in frames 0–2599 (doc §3). */
  frameTime: number;
  /** Days since "WD 1 / 1Q / Year 1". Signed in the file. */
  currentDay: number;
  /** Ground-lobby height in floor slots (1-3). */
  lobbyHeight: number;
  /** Saved view-scroll (doc §1 row 0x26): top-left of the 1994 window in
   *  world px. (0, 0) means "no saved view" (the game then opens at the
   *  top-left sky); see `viewFromViewWords`. */
  viewX: number;
  viewY: number;
}

/** One decoded elevator entry (doc §8; floors are TDT indexes 0–119). */
export interface TdtElevator {
  /** The elevator kind: 0 = express, 1 = standard, 2 = service. This byte is
   *  authoritative (confirmed against the real game: a service shaft imported as
   *  service only from this byte). */
  type: number;
  /** Byte 2 of the header. The doc calls it per-car capacity (42/21/10), but a
   *  real save read 21 for BOTH standard AND service shafts, so it is NOT a
   *  reliable kind signal; {@link type} is. Kept for research; our per-car
   *  capacity comes from the engine's canon table, not this byte. */
  capacity: number;
  /** Cars in the shaft, 1–8. */
  cars: number;
  /** Horizontal position in tiles from the left. */
  x: number;
  topFloor: number;
  bottomFloor: number;
  /** Per-floor stop flags (120 entries; nonzero = cars stop there). */
  serviced: Uint8Array;
  /** Home floor for each of the 8 car slots (TDT indexes). */
  carHomes: number[];
}

/** One decoded stair/escalator record (doc §8; floor is a TDT index). */
export interface TdtStair {
  /** 0 = escalator, 1 = stairs; 2/3 = two-story, 4/5 = three-story variants. */
  type: number;
  /** Left tile of the flight. */
  x: number;
  /** Base floor (TDT index). */
  floor: number;
}

/** The fixed 132-byte finance block written after the elevator table. */
export interface TdtFinance {
  tenantPopulation: number[];
  towerPopulation: number;
  tenantIncome: number[];
  towerIncome: number;
  tenantMaintenance: number[];
  towerMaintenance: number;
}

/** The whole file, mirrored; no game semantics applied yet. */
export interface TdtTower {
  header: TdtHeader;
  floors: TdtFloor[];
  /** People count from the tail, when it could be read (else null). */
  peopleCount: number | null;
  /** Occupied rows found in the 512-slot retail table (null if unreadable). */
  retailRows: number | null;
  /** Decoded elevator shafts, or null when the block couldn't be read;
   *  the importer then falls back to synthesizing a layout. */
  elevators: TdtElevator[] | null;
  /** Decoded stair/escalator flights (null when unreadable). */
  stairs: TdtStair[] | null;
  /** The save's connected-parking-stall count (null when unreadable). */
  parkingConnected: number | null;
  /** Population and finance-window totals, or null when the tail is unreadable. */
  finance: TdtFinance | null;
  /** Non-fatal oddities found while walking the tolerant tail. */
  warnings: string[];
}

/** Everything the tolerant tail walk can produce. */
export type TdtTail = Pick<
  TdtTower,
  "peopleCount" | "retailRows" | "elevators" | "stairs" | "parkingConnected" | "finance" | "warnings"
>;
