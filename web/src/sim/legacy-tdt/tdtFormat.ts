/**
 * Pure binary walker for the original 1994 SimTower `.TDT` save format.
 *
 * Layout facts come from `docs/canon/tdt-format.md` (our restatement of the
 * OpenSkyscraper notes, cross-checked against the dfloer/tower-docs
 * `tdt_spec.md` reverse-engineering); this module only mirrors the file
 * into a dumb intermediate model ({@link TdtTower}); all game-semantic
 * mapping (tenant IDs → FacilityKind, money ×100, frame → clock) lives in
 * `tdtImport.ts`.
 *
 * The original game barely validates its own saves, so every count and offset
 * here is treated as hostile input: the reader is bounds-checked, per-floor
 * tenant counts are capped BEFORE any loop, and every failure is a typed
 * {@link LegacyImportError} with a player-readable message; never a raw
 * RangeError or a hang. The header and floor map are load-bearing (a failure
 * there fails the import); everything after (people, retail, elevators,
 * finance, parking, stairs) is walked TOLERANTLY, so a misfit downgrades to
 * a warning and the importer falls back gracefully.
 *
 * This file is the entry point + barrel. The pieces live in cohesive siblings
 * and are re-exported here so every existing `import { … } from "./tdtFormat"`
 * keeps working unchanged:
 *   - `tdtConstants.ts`: LegacyImportError + all size/offset constants.
 *   - `tdtViewMapping.ts`: the pure view/camera word mapping.
 *   - `tdtTypes.ts`: the raw Tdt* interfaces.
 *   - `tdtByteReader.ts`: the bounds-checked ByteReader.
 *   - `tdtTail.ts`: locateStairs + the tolerant tail walk.
 */
import { ByteReader } from "./tdtByteReader";
import {
  LegacyImportError,
  TDT_FLOOR_COUNT,
  TDT_FLOOR_INDEX_ENTRIES,
  TDT_HEADER_SIZE,
  TDT_MAGIC,
  TDT_MAX_FILE_BYTES,
  TDT_MAX_TENANTS_PER_FLOOR,
  TDT_TENANT_RECORD_SIZE,
} from "./tdtConstants";
import type { TdtFloor, TdtHeader, TdtTenant, TdtTower } from "./tdtTypes";
import { walkTolerantTail } from "./tdtTail";

/**
 * Walk a `.TDT` byte stream into the raw {@link TdtTower} model. Throws a
 * {@link LegacyImportError} for anything that can't be a readable save
 * (wrong magic, truncation inside the header/floor map, absurd counts);
 * everything after the floor map; the only part v1 consumes; is walked
 * tolerantly, downgrading misfits to `warnings`.
 */
export function parseTdtBinary(bytes: Uint8Array): TdtTower {
  if (bytes.byteLength > TDT_MAX_FILE_BYTES) {
    throw new LegacyImportError("This file is too large to be a SimTower save.");
  }
  if (bytes.byteLength < TDT_HEADER_SIZE) {
    throw new LegacyImportError("This file is too small to be a SimTower save.");
  }
  const r = new ByteReader(bytes);

  r.enterBlock("header");
  const version = r.u16();
  if (version !== TDT_MAGIC) {
    throw new LegacyImportError("This file doesn't look like a SimTower save.");
  }
  const level = r.u16();
  const balance = r.i32();
  r.skip(12); // otherIncome, constructionCosts, lastQuarterMoney (finance import is a queued follow-up)
  const frameTime = r.u16();
  const currentDay = r.i32();
	// 0x1A is undocumented; 0x1C stores the ground-lobby height.
	r.skip(2);
	const lobbyHeight = r.u16();
	// Skip the remaining words before the view scroll at 0x26/0x28.
  r.skip(0x26 - r.offset());
  const viewX = r.u16();
  const viewY = r.u16();
  // The rest of the ~518-byte misc block carries nothing v1 imports.
  r.skip(TDT_HEADER_SIZE - r.offset());
  const header: TdtHeader = {
	version,
	level,
	balance,
	frameTime,
	currentDay,
	lobbyHeight,
	viewX,
	viewY,
  };

  const floors: TdtFloor[] = [];
  for (let index = 0; index < TDT_FLOOR_COUNT; index++) {
    r.enterBlock(`floor map (floor record ${index})`);
    const tenantCount = r.u16();
    const leftEdge = r.u16();
    const rightEdge = r.u16();
    // Cap and pre-check BEFORE the loop: a forged count must fail loudly here,
    // not allocate/iterate its way into a hang.
    if (tenantCount > TDT_MAX_TENANTS_PER_FLOOR) {
      throw new LegacyImportError(
        `This SimTower save is corrupt: one floor claims ${tenantCount.toLocaleString()} rooms.`,
      );
    }
    const floorBytes = tenantCount * TDT_TENANT_RECORD_SIZE + TDT_FLOOR_INDEX_ENTRIES * 2;
    if (r.remaining() < floorBytes) {
      throw new LegacyImportError(
        "This SimTower save is cut short. The file ends in the middle of its floor map.",
      );
    }
    const tenants: TdtTenant[] = [];
    for (let t = 0; t < tenantCount; t++) {
      const left = r.u16();
      const right = r.u16();
      const type = r.i8();
      const status = r.u8();
      const variant = r.u8(); // byte 6: retail variant (the real game's variety byte)
      r.skip(9); // reserved bytes 7–15 (tower-docs)
      const rentRate = r.u8(); // byte 16
      const subtype = r.u8(); // byte 17 (0 in every real save's retail; not the variant)
      tenants.push({ left, right, type, status, variant, rentRate, subtype });
    }
    r.skip(TDT_FLOOR_INDEX_ENTRIES * 2); // per-floor index map into the tenant array
    floors.push({ index, leftEdge, rightEdge, tenants });
  }

  const tail = walkTolerantTail(r);
  return { header, floors, ...tail };
}

// ---- Barrel: preserve the original public surface of this module. ----
export {
  LegacyImportError,
  TDT_MAGIC,
  TDT_HEADER_SIZE,
  TDT_FLOOR_COUNT,
  TDT_TENANT_RECORD_SIZE,
  TDT_FLOOR_INDEX_ENTRIES,
  TDT_MAX_TENANTS_PER_FLOOR,
  TDT_MAX_FILE_BYTES,
  TDT_MAX_PEOPLE,
  TDT_PERSON_RECORD_SIZE,
  TDT_MAX_CENSUS,
  TDT_DEFAULT_VIEW_X,
  TDT_DEFAULT_VIEW_Y,
  TDT_FLOOR_OFFSET,
  TDT_TILE_PX,
  TDT_FLOOR_PX,
  TDT_WORLD_W,
  TDT_WORLD_H,
  TDT_VIEW_W,
  TDT_VIEW_H,
  TDT_RETAIL_SLOTS,
  TDT_RETAIL_RECORD_SIZE,
  TDT_ELEVATOR_SLOTS,
  TDT_ELEVATOR_HEADER_SIZE,
  TDT_ELEVATOR_BUILT_FIXED,
  TDT_ELEVATOR_PER_FLOOR_SIZE,
  TDT_ELEVATOR_CAR_BLOCK_SIZE,
  TDT_ELEVATOR_SCHEDULE_DEFAULT,
  builtShaftPayloadSize,
  builtShaftPayloadSizeFor,
  TDT_ELEVATOR_TYPE_EXPRESS,
  TDT_FINANCE_SIZE,
  TDT_PARKING_SIZE,
  TDT_STAIR_SLOTS,
  TDT_STAIR_RECORD_SIZE,
  TDT_ROUTING_TAIL_SIZE,
  TDT_STAMP_GENERATION,
  TDT_STAMP_MAGIC,
  TDT_STAMP_SIZE,
  TDT_MAX_TILE,
  TDT_MAX_STAIR_CROWD,
  TDT_STAIR_SCAN_WINDOW,
} from "./tdtConstants";
export { viewWordsFromView, viewFromViewWords } from "./tdtViewMapping";
export type { TdtTenant, TdtFloor, TdtHeader, TdtElevator, TdtStair, TdtFinance, TdtTower } from "./tdtTypes";
export { ByteReader } from "./tdtByteReader";
export { locateStairs } from "./tdtTail";
