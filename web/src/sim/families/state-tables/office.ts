// Binary tables:
//   cs:2005 refresh_object_family_office_state_handler dispatch table
//   cs:2aac dispatch_object_family_office_state_handler 16-entry table
//   cs:1c51 dispatch_sim_behavior family-7 branch
//
// These are Map<state_code, HandlerFn> tables. The handlers live in
// sims/office.ts. Binary quirk: `0x00 ↔ 0x40` and `0x20 ↔ 0x60` aliasing
// is preserved — both keys map to the same handler reference. The difference
// in the binary is whether `decrement_route_queue_direction_load` ran as
// prologue; that is now gated in families/office.ts#refreshObjectFamilyOfficeStateHandler.

export {
	OFFICE_REFRESH_HANDLER_TABLE,
	type OfficeHandler,
} from "../../sims/office";

/** cs:1c51 dispatch_sim_behavior family-7 prologue table. */
export const OFFICE_PROLOGUE_TABLE: ReadonlyMap<number, string> = new Map([
	[0x40, "1228:1989"],
	[0x41, "1228:1989"],
	[0x42, "1228:1989"],
	[0x45, "1228:193d"],
	[0x60, "1228:193d"],
	[0x61, "1228:193d"],
	[0x62, "1228:193d"],
	[0x63, "1228:193d"],
]);
