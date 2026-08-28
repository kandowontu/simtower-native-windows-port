// 1228:2aec refresh_object_family_hotel_state_handler (family 3/4/5)
// 1228:2dae dispatch_object_family_hotel_state_handler (family 3/4/5)
//
// Hotel-family state machine (family 3 single, 4 twin, 5 suite). The refresh
// handler is the binary's two-tier entry point (gate + dispatch), driving the
// per-state handlers through the cs:1c41 dispatch table (see
// families/state-tables/hotel.ts).
//
// For in-transit states (0x40+), the binary runs decrementRouteQueueDirectionLoad
// as a prologue (cs:1c41 table entries 0x41, 0x45, 0x60, 0x62 all point to
// handlers that call it), then dispatches through the same handler as the base state.

import type { LedgerState } from "../ledger";
import { decrementRouteQueueDirectionLoad } from "../queue/cancel";
import { isSimInTransit } from "../sim-access/state-bits";
import {
	handleHotelSimArrival as _handleHotelSimArrival,
	processHotelSim as _processHotelSim,
} from "../sims/hotel";
import type { TimeState } from "../time";
import type { CarrierRecord, SimRecord, WorldState } from "../world";
import { maybeDispatchQueuedRouteAfterWait } from "./maybe-dispatch-after-wait";

export {
	checkoutHotelStay,
	handleHotelSimArrival,
	processHotelSim,
} from "../sims/hotel";

/**
 * 1228:2aec refresh_object_family_hotel_state_handler — two-tier entry:
 *   - `state_code < 0x40`: run the gate + dispatch body (processHotelSim).
 *   - `state_code >= 0x40`: call decrementRouteQueueDirectionLoad prologue
 *     (1218:0fc4) then dispatch through the same handler as the base state.
 *     The binary's family-3/4/5 dispatch table (cs:1c41) maps waiting/in-transit
 *     states {0x41, 0x45, 0x60, 0x62} to in-place handlers; the TS body
 *     already distinguishes these via the TRANSIT-suffixed state constants.
 */
export function refreshObjectFamilyHotelStateHandler(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
): void {
	if (isSimInTransit(sim.stateCode)) {
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
	_processHotelSim(world, ledger, time, sim);
}

/**
 * 1228:2dae dispatch_object_family_hotel_state_handler — arrival dispatch.
 * Called by dispatch_destination_queue_entries (1218:0883). TS body lives
 * in handleHotelSimArrival.
 */
export function dispatchObjectFamilyHotelStateHandler(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	arrivalFloor: number,
): void {
	_handleHotelSimArrival(world, ledger, time, sim, arrivalFloor);
}
