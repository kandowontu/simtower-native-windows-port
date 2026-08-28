// 1228:5f39 gate_housekeeping_room_claim_state (family 15)
// 1228:602b update_object_family_housekeeping_connection_state (family 15)
// 1228:6480 activate_object_family_housekeeping_connection_state (family 14)
//
// Housekeeping helper (family 0x0f) state machine. Phase 5b makes this the
// real implementation that wraps `sims/housekeeping.ts#processHousekeepingSim`
// with the binary's two-tier structure:
//   - gate (1228:5f39): room-claim search (state HK_STATE_SEARCH)
//   - update (1228:602b): countdown + route-to-target (state HK_STATE_COUNTDOWN,
//     HK_STATE_ROUTE_TO_*)
//   - activate (1228:6480): arrival handler (arrival at claimed floor)
// The existing TS function drives all three sub-states; Phase 5b keeps the
// driver in `sims/housekeeping.ts` and exposes the binary names here.

import {
	handleHousekeepingSimArrival as _handleHousekeepingSimArrival,
	processHousekeepingSim as _processHousekeepingSim,
} from "../sims/housekeeping";
import type { TimeState } from "../time";
import type { SimRecord, WorldState } from "../world";

export {
	handleHousekeepingSimArrival,
	processHousekeepingSim,
} from "../sims/housekeeping";

/**
 * 1228:5f39 gate_housekeeping_room_claim_state. Entry point for the
 * search/claim gate (state HK_STATE_SEARCH).
 * Binary quirk: housekeeping uses low-valued states (0..4) that do NOT
 * overlap the 0x20/0x40 bits used by the dispatch_sim_behavior families,
 * so isSimInTransit/isSimWaiting are NOT consulted here.
 */
export function gateHousekeepingRoomClaimState(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	_processHousekeepingSim(world, time, sim);
}

/**
 * 1228:602b update_object_family_housekeeping_connection_state. Per-stride
 * update; currently shares the driver with the gate because the TS body
 * already handles all housekeeping states inline.
 */
export function updateObjectFamilyHousekeepingConnectionState(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	_processHousekeepingSim(world, time, sim);
}

/** 1228:6480 activate_object_family_housekeeping_connection_state. */
export function activateObjectFamilyHousekeepingConnectionState(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
	arrivalFloor: number,
): void {
	_handleHousekeepingSimArrival(world, time, sim, arrivalFloor);
}
