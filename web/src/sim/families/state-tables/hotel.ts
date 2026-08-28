// Binary table: cs:1c41 dispatch_sim_behavior family 3/4/5 (hotel/restaurant/
// fast-food) branch. ROUTING-BINARY-MAP.md §4.2.
//
// These are Map<state_code, HandlerFn> tables. The handlers live in
// sims/hotel.ts. Binary quirk: in-transit states {0x41, 0x45, 0x60, 0x62}
// run decrementRouteQueueDirectionLoad as a prologue (gated in
// families/hotel.ts#refreshObjectFamilyHotelStateHandler), then dispatch
// through HOTEL_REFRESH_HANDLER_TABLE which maps them to the same handler
// as their base state.

export {
	HOTEL_REFRESH_HANDLER_TABLE,
	type HotelHandler,
} from "../../sims/hotel";

/** Shared family 3/4/5 dispatch_sim_behavior table (cs:1c41). */
export const HOTEL_PROLOGUE_TABLE: ReadonlyMap<number, string> = new Map([
	[0x41, "1228:1a4f"],
	[0x45, "1228:19f4"],
	[0x60, "1228:1a4f"],
	[0x62, "1228:1a4f"],
	// else (everything not listed above) → 1228:1c24
]);

export const HOTEL_PROLOGUE_DEFAULT = "1228:1c24";
