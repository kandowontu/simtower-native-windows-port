// 1228:186c dispatch_sim_behavior
//
// Two-tier switch: first on sim.familyCode (via cs:1c71 FAMILY_PROLOGUE_TABLE),
// then on sim.stateCode. Runs the family-specific prologue and jumps into
// the per-family gate or dispatch handler.
//
// Phase 5b body: callable by the daily sweep and `forceDispatchSimStateByFamily`
// test paths. The stride refresh at `sim-refresh/refresh-stride.ts`
// (1228:0d64) still owns the top-level loop; dispatchSimBehavior routes a
// single sim through its family refresh handler, using the state_code
// bits (0x40 in-transit, 0x20 waiting) to decide the two-tier branch
// per ROUTING-BINARY-MAP.md §4.2.

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
import { isSimInTransit } from "../sim-access/state-bits";
import type { TimeState } from "../time";
import type { SimRecord, WorldState } from "../world";
import { refreshObjectFamilyCondoStateHandler } from "./condo";
import { refreshObjectFamilyHotelStateHandler } from "./hotel";
import { updateObjectFamilyHousekeepingConnectionState } from "./housekeeping";
import { maybeDispatchQueuedRouteAfterWait } from "./maybe-dispatch-after-wait";
import { refreshObjectFamilyOfficeStateHandler } from "./office";
import { gateObjectFamilyRestaurantFastFoodStateHandler } from "./restaurant";
import { gateObjectFamilyRetailStateHandler } from "./retail";
import { FAMILY_PROLOGUE_TABLE } from "./state-tables/family-prologue";

/**
 * Binary 1228:186c. The stride refresh (1228:0d64) indexes
 * FAMILY_PROLOGUE_TABLE (cs:1c71) on `family_code - 3`; if an entry is
 * present, the handler is invoked. In-transit sims (state_code & 0x40)
 * hit a different sub-branch that calls maybe_dispatch_queued_route_after_wait
 * (1228:15a0). Waiting sims (0x20) without the 0x40 bit fall through to
 * the family's refresh handler which handles the re-dispatch.
 */
export function dispatchSimBehavior(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
): void {
	// Binary cs:1c71 gate: only families with an entry dispatch here.
	const familyIndex = sim.familyCode - 3;
	if (!FAMILY_PROLOGUE_TABLE.has(familyIndex)) return;

	// Binary: in-transit sims (0x40) route to the wait-timeout path. When
	// the timeout doesn't fire, the sim stays in transit and the family's
	// refresh handler is skipped (that branch runs elsewhere in the binary
	// — specifically the refresh handler's own state_code >= 0x40 arm,
	// which we already wire through the family `refresh*` functions).
	if (isSimInTransit(sim.stateCode)) {
		maybeDispatchQueuedRouteAfterWait(world, ledger, time, sim);
		return;
	}

	// Waiting / idle: dispatch into the family handler.
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
			// Entertainment (18/29), recycling (33), parking (36) still use
			// legacy TS branches.
			return;
	}
}
