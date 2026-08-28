// 1228:1614 force_dispatch_sim_state_by_family
//
// Forces a dispatch into the family handler ignoring the 0x20/0x40 mode
// bits — used by external paths (daily sweep, transfer-floor failure
// re-dispatch) to re-drive state transitions. The binary strips the bits
// and indexes the per-family state table using the low 4 bits of
// state_code.
//
// Phase 5b note: the TS state constants (e.g. STATE_MORNING_GATE = 0x20,
// STATE_NIGHT_B = 0x26) ENCODE the waiting bit as part of the phase byte,
// unlike the binary where 0x20 is strictly the "waiting" mode bit. So
// this function does NOT strip 0x20 from the state byte before
// dispatching — doing so would convert MORNING_GATE into STATE_COMMUTE
// (0x00) and break the trace. Instead we strip only the 0x40 in-transit
// bit, which the TS constants use via `withTransitFlag` (e.g.
// STATE_COMMUTE_TRANSIT = 0x40, STATE_MORNING_TRANSIT = 0x60); stripping
// 0x40 from those yields the un-transit variant (COMMUTE / MORNING_GATE),
// which is the binary's intent.

import type { LedgerState } from "../ledger";
import {
	FAMILY_CONDO,
	FAMILY_FAST_FOOD,
	FAMILY_HOTEL_SINGLE,
	FAMILY_HOTEL_SUITE,
	FAMILY_HOTEL_TWIN,
	FAMILY_HOUSEKEEPING,
	FAMILY_OFFICE,
	FAMILY_RESTAURANT,
	FAMILY_RETAIL,
} from "../resources";
import {
	SIM_STATE_IN_TRANSIT_BIT,
	setSimInTransit,
} from "../sim-access/state-bits";
import type { TimeState } from "../time";
import type { SimRecord, WorldState } from "../world";
import { refreshObjectFamilyCondoStateHandler } from "./condo";
import { refreshObjectFamilyHotelStateHandler } from "./hotel";
import { updateObjectFamilyHousekeepingConnectionState } from "./housekeeping";
import { refreshObjectFamilyOfficeStateHandler } from "./office";
import { gateObjectFamilyRestaurantFastFoodStateHandler } from "./restaurant";
import { gateObjectFamilyRetailStateHandler } from "./retail";

/**
 * 1228:1614 force_dispatch_sim_state_by_family. Strips the in-transit bit
 * (0x40) from state_code and re-enters the family refresh handler. Callers
 * include `assign_request_to_runtime_route` on transfer-floor failure
 * (1218:0d4e) and the daily drain sweep at checkpoint 0x9c4
 * (`dispatch_active_requests_by_family`).
 *
 * See file-level comment for why we do NOT strip 0x20 (that bit is part
 * of the TS state byte for phases 0x20..0x27, not a separate mode flag).
 */
export function forceDispatchSimStateByFamily(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
): void {
	// Strip the 0x40 in-transit bit only. TS transit states use
	// withTransitFlag(base) = 0x40 | base; stripping 0x40 restores the base
	// phase which is what the binary's force-dispatch feeds into the
	// refresh handler.
	if ((sim.stateCode & SIM_STATE_IN_TRANSIT_BIT) !== 0) {
		setSimInTransit(sim, false);
	}
	switch (sim.familyCode) {
		case FAMILY_HOTEL_SINGLE:
		case FAMILY_HOTEL_TWIN:
		case FAMILY_HOTEL_SUITE:
			refreshObjectFamilyHotelStateHandler(world, ledger, time, sim);
			return;
		case FAMILY_OFFICE:
			refreshObjectFamilyOfficeStateHandler(world, ledger, time, sim);
			return;
		case FAMILY_CONDO:
			refreshObjectFamilyCondoStateHandler(world, ledger, time, sim);
			return;
		case FAMILY_RETAIL:
			gateObjectFamilyRetailStateHandler(world, ledger, time, sim);
			return;
		case FAMILY_RESTAURANT:
		case FAMILY_FAST_FOOD:
			gateObjectFamilyRestaurantFastFoodStateHandler(world, ledger, time, sim);
			return;
		case FAMILY_HOUSEKEEPING:
			updateObjectFamilyHousekeepingConnectionState(world, time, sim);
			return;
		default:
			// TODO: binary 1228:1614 — entertainment (18/29), recycling (33),
			// parking (36) refresh handlers are TODO stubs.
			return;
	}
}
