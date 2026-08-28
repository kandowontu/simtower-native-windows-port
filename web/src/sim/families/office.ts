// 1228:1cb5 refresh_object_family_office_state_handler (family 7)
// 1228:2031 dispatch_object_family_office_state_handler (family 7)
//
// Office-family state machine (family 7). The refresh handler is the binary's
// two-tier entry point (gate + dispatch) and drives the per-state handlers
// through a Map<state_code, HandlerFn> table (see families/state-tables/office.ts).
//
// For in-transit states (0x40+), the binary runs decrementRouteQueueDirectionLoad
// as a prologue then dispatches through the same handler as the base state.
// The 0x00↔0x40 and 0x20↔0x60 aliases in OFFICE_REFRESH_HANDLER_TABLE preserve
// this: both map to the same function reference.

import type { LedgerState } from "../ledger";
import { decrementRouteQueueDirectionLoad } from "../queue/cancel";
import { isSimInTransit, simBaseState } from "../sim-access/state-bits";
import {
	handleOfficeSimArrival as _handleOfficeSimArrival,
	processOfficeSim as _processOfficeSim,
} from "../sims/office";
import type { TimeState } from "../time";
import type { CarrierRecord, SimRecord, WorldState } from "../world";
import { maybeDispatchQueuedRouteAfterWait } from "./maybe-dispatch-after-wait";

export {
	advanceOfficePresenceCounter,
	handleOfficeSimArrival,
	nextOfficeReturnState,
	processOfficeSim,
} from "../sims/office";

/**
 * 1228:1cb5 refresh_object_family_office_state_handler. Two-tier entry:
 *   - `state_code < 0x40`: run the gate + dispatch body (processOfficeSim,
 *     which encapsulates the cs:2005 jump table).
 *   - `state_code >= 0x40`: call decrementRouteQueueDirectionLoad prologue
 *     (1218:0fc4) then dispatch through the same handler as the base state
 *     (aliased 0x00↔0x40, 0x20↔0x60 in OFFICE_REFRESH_HANDLER_TABLE).
 *     When the wait-timeout fires via maybe_dispatch_queued_route_after_wait
 *     (1228:15a0), the sim is evicted and set to STATE_NIGHT_B.
 */
export function refreshObjectFamilyOfficeStateHandler(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
): void {
	if (isSimInTransit(sim.stateCode)) {
		// Binary prologue: decrement_route_queue_direction_load runs for every
		// in-transit state before the handler is dispatched (cs:1c51 family-7
		// prologue table). The carrier/direction args match the route the sim
		// is currently on; the stub is a no-op until the TS scorer reads load
		// counters (see queue/cancel.ts TODO).
		const carrier =
			world.carriers.find(
				(c) =>
					c.carrierId ===
					(sim.route.mode === "carrier" ? sim.route.carrierId : undefined),
			) ?? (world.carriers[0] as CarrierRecord | undefined);
		if (carrier) {
			decrementRouteQueueDirectionLoad(carrier, sim.originFloor, 0);
		}
		maybeDispatchQueuedRouteAfterWait(world, ledger, time, sim);
		if (!isSimInTransit(sim.stateCode)) return;
	}
	_processOfficeSim(world, ledger, time, sim);
}

/**
 * 1228:2031 dispatch_object_family_office_state_handler. Binary dispatches
 * through a 16-entry state table (cs:2aac). The TS body lives in
 * processOfficeSim; the aliasing `0x00↔0x40`, `0x20↔0x60` in the binary
 * table is preserved by reading just the base phase via `simBaseState`
 * when strictly needed.
 *
 * This function is called by the arrival path — dispatch_destination_queue_entries
 * (1218:0883) — with the carrier's arrival floor. Delegates to the TS
 * handleOfficeSimArrival which covers the full arrival-phase table.
 */
export function dispatchObjectFamilyOfficeStateHandler(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
	arrivalFloor: number,
): void {
	// Binary quirk: state-table aliases 0x00↔0x40 and 0x20↔0x60 route to the
	// same handler; the difference is whether decrement_route_queue_direction_load
	// ran as prologue. We don't split the cases here because the TS arrival
	// handler uses the transit-phase byte (e.g. STATE_MORNING_TRANSIT = 0x60)
	// directly to distinguish arrival types.
	void simBaseState(sim.stateCode); // reserved for future table-driven dispatch
	_handleOfficeSimArrival(world, time, sim, arrivalFloor);
}
