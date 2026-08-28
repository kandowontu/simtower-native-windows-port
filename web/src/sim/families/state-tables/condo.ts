// Binary table: cs:1c2d dispatch_sim_behavior family-9 (condo) branch.
// ROUTING-BINARY-MAP.md §4.2.
//
// These are Map<state_code, HandlerFn> tables. The handlers live in
// sims/condo.ts. Binary quirk: in-transit states {0x40, 0x41, 0x60, 0x61, 0x62}
// run decrementRouteQueueDirectionLoad as a prologue (gated in
// families/condo.ts#refreshObjectFamilyCondoStateHandler), then dispatch
// through CONDO_REFRESH_HANDLER_TABLE which maps them to the same handler
// as their base state.

export {
	CONDO_REFRESH_HANDLER_TABLE,
	type CondoHandler,
} from "../../sims/condo";

/** Family-9 (condo) dispatch_sim_behavior table (cs:1c2d). */
export const CONDO_PROLOGUE_TABLE: ReadonlyMap<number, string> = new Map([
	[0x40, "1228:1aba"],
	[0x41, "1228:1aba"],
	[0x60, "1228:1aba"],
	[0x61, "1228:1aba"],
	[0x62, "1228:1aba"],
]);
