// Binary table: cs:1c71 dispatch_sim_behavior family-prologue dispatch.
// 0x22-entry jump table indexed by (family_code - 3). ROUTING-BINARY-MAP.md §4.2.
//
// Phase 5a records the table; the `advanceSimRefreshStride` switch in
// `sims/index.ts` approximates this today. Phase 5b routes dispatch_sim_behavior
// through this table.
//
// TODO: binary table at cs:1c71 — port to table-driven prologue dispatch.

/** family_code - 3 → binary prologue handler address (ROUTING-BINARY-MAP.md §4.2). */
export const FAMILY_PROLOGUE_TABLE: ReadonlyMap<number, string> = new Map([
	[0, "1228:19d4"], // family 3 — hotel single
	[1, "1228:19d4"], // family 4 — hotel twin
	[2, "1228:19d4"], // family 5 — hotel suite
	[3, "1228:1b05"], // family 6 — restaurant
	[4, "1228:191d"], // family 7 — office
	// 5 reserved
	[6, "1228:1a9a"], // family 9 — condo
	[7, "1228:1b05"], // family 10 — retail
	// 8 reserved
	[9, "1228:1b05"], // family 12 — fast food
	// 10, 11 reserved
	[12, "1228:1bd8"], // family 15 — housekeeping
	// 13, 14 reserved
	[15, "1228:1bb0"], // family 18 — entertainment guest
	// 16..25 reserved
	[26, "1228:1bb0"], // family 29 — entertainment variant
	// 27..29 reserved
	[30, "1228:1bb0"], // family 33 — recycling center lower
	// remaining 31..33 reserved
]);
