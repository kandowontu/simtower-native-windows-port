// 1228:5b5a gate_object_family_parking_state_handler (family 36)
// 1228:5cd2 dispatch_object_family_parking_state_handler (family 36)
//
// Parking state machine. Current TS (`sims/parking.ts`) contains demand-log
// rebuild + service assignment only; the family-36 state-machine gate and
// dispatch functions are not yet implemented in TS.

import { checkEvalCompletionAndAward } from "../cathedral";
import { decrementRouteQueueDirectionLoad } from "../queue/cancel";
import {
	type RouteResolution,
	resolveSimRouteBetweenFloors,
} from "../queue/resolve";
import { isSimInTransit } from "../sim-access/state-bits";
import {
	EVAL_ZONE_FLOOR,
	LOBBY_FLOOR,
	STATE_ARRIVED,
	STATE_DEPARTURE,
	STATE_DEPARTURE_TRANSIT,
	STATE_EVAL_OUTBOUND,
	STATE_MORNING_GATE,
	STATE_PARKED,
} from "../sims/states";
import type { TimeState } from "../time";
import { type SimRecord, sampleRng, type WorldState } from "../world";

export {
	rebuildParkingDemandLog,
	tryAssignParkingService,
} from "../sims/parking";

// Binary quirk: the dispatch jump table at cs:5f29 lists exactly these four
// state codes mapped to handlers. Any other state_code falls through silently.
// States 0x05/0x45 → handle_family_parking_return_route (1228:5e7e).
// States 0x20/0x60 → handle_family_parking_outbound_route (1228:5ddd).

/** 1228:5ddd handle_family_parking_outbound_route.
 *
 * Binary call site at 1228:5e04 passes `is_passenger_route = 1` and
 * `emit_distance_feedback = (sim.stateCode == 0x20) ? 1 : 0` — i.e. distance
 * feedback fires only on the BASE state 0x20 dispatch, not on the +0x40
 * alias 0x60 retry strides.
 */
function handleFamilyParkingOutboundRoute(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	// Binary: if state_code == 0x20 (fresh dispatch), use ground floor as
	// source with directionFlag=1 (upward). Otherwise use sim[+7] (originFloor)
	// with directionFlag=0.
	const isFreshDispatch = sim.stateCode === STATE_MORNING_GATE;
	const sourceFloor = isFreshDispatch ? LOBBY_FLOOR : sim.originFloor;
	const directionFlag = isFreshDispatch ? 1 : 0;

	const result: RouteResolution = resolveSimRouteBetweenFloors(
		world,
		sim,
		sourceFloor,
		EVAL_ZONE_FLOOR,
		directionFlag,
		time,
		{ emitDistanceFeedback: isFreshDispatch },
	);

	if (result === 3) {
		sim.stateCode = STATE_ARRIVED;
		checkEvalCompletionAndAward(world, time, sim);
	} else if (result === 0 || result === 1 || result === 2) {
		sim.stateCode = STATE_EVAL_OUTBOUND; // 0x60: in-transit to eval zone
	} else {
		sim.stateCode = STATE_PARKED; // 0x27: route failure
	}
}

/** 1228:5e7e handle_family_parking_return_route.
 *
 * Binary call site at 1228:5ea5 passes `is_passenger_route = 1` and
 * `emit_distance_feedback = (sim.stateCode == 0x05) ? 1 : 0` — i.e. distance
 * feedback fires only on the BASE state 0x05 dispatch, not on the +0x40
 * alias 0x45 retry strides.
 */
function handleFamilyParkingReturnRoute(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	// Binary: if state_code == 0x05 (fresh return dispatch), use eval zone as
	// source with directionFlag=1 (upward/toward lobby). Otherwise use
	// sim[+7] (originFloor) with directionFlag=0.
	const isFreshDispatch = sim.stateCode === STATE_DEPARTURE;
	const sourceFloor = isFreshDispatch ? EVAL_ZONE_FLOOR : sim.originFloor;
	const directionFlag = isFreshDispatch ? 1 : 0;

	const result: RouteResolution = resolveSimRouteBetweenFloors(
		world,
		sim,
		sourceFloor,
		LOBBY_FLOOR,
		directionFlag,
		time,
		{ emitDistanceFeedback: isFreshDispatch },
	);

	if (result === 0 || result === 1 || result === 2) {
		sim.stateCode = STATE_DEPARTURE_TRANSIT; // 0x45: in-transit back to lobby
	} else {
		// Binary quirk: both result === 3 (same-floor) and result === -1 (failure)
		// park the sim to 0x27. Same-floor arrival is treated as a no-op here.
		sim.stateCode = STATE_PARKED; // 0x27
	}
}

function runDispatchJumpTable(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	// Prologue: for in-transit states, if the sim is on a special-link segment
	// (encoded_route_target in [0, 0x3f]), call decrement_route_queue_direction_load.
	// Carrier-queued sims (route.mode === "carrier") skip this.
	if (isSimInTransit(sim.stateCode) && sim.route.mode === "segment") {
		decrementRouteQueueDirectionLoad(
			world.carriers[0] ?? ({} as (typeof world.carriers)[0]),
			sim.originFloor,
			0,
		);
	}

	// 4-entry jump table at cs:5f29:
	//   0x05 → handle_family_parking_return_route
	//   0x20 → handle_family_parking_outbound_route
	//   0x45 → handle_family_parking_return_route
	//   0x60 → handle_family_parking_outbound_route
	switch (sim.stateCode) {
		case STATE_DEPARTURE: // 0x05
		case STATE_DEPARTURE_TRANSIT: // 0x45
			handleFamilyParkingReturnRoute(world, time, sim);
			break;
		case STATE_MORNING_GATE: // 0x20
		case STATE_EVAL_OUTBOUND: // 0x60
			handleFamilyParkingOutboundRoute(world, time, sim);
			break;
		default:
			break;
	}
}

/** 1228:5b5a gate_object_family_parking_state_handler. */
export function gateObjectFamilyParkingStateHandler(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	const stateCode = sim.stateCode;

	if (isSimInTransit(stateCode)) {
		// stateCode >= 0x40: already in-transit.
		// Binary: if encoded_route_target >= 0x40 (carrier queue), call
		// maybe_dispatch_queued_route_after_wait. For parking, that function
		// exits immediately (family != FAMILY_OFFICE guard), so we skip the
		// call and return. Otherwise fall through to dispatch (segment in progress).
		if (sim.route.mode === "carrier") return;
		runDispatchJumpTable(world, time, sim);
		return;
	}

	if (stateCode === STATE_DEPARTURE) {
		// 0x05: unconditional dispatch (return route)
		runDispatchJumpTable(world, time, sim);
		return;
	}

	if (stateCode === STATE_MORNING_GATE) {
		// 0x20: morning activation gate — weekendFlag must be 1
		if (time.weekendFlag !== 1) return;

		if (time.daypartIndex === 0) {
			// Staggered dispatch: 1/12 chance after tick 0x50, guaranteed after 0xf0.
			if (time.dayTick > 0x50) {
				if (sampleRng(world) % 12 === 0) {
					runDispatchJumpTable(world, time, sim);
				}
			}
			if (time.dayTick > 0xf0) {
				runDispatchJumpTable(world, time, sim);
			}
			return;
		}

		// daypartIndex >= 1: missed dispatch window — park the sim
		if (time.daypartIndex >= 1) {
			sim.stateCode = STATE_PARKED;
		}
		return;
	}

	// All other states (STATE_ARRIVED 0x03, STATE_PARKED 0x27, etc.): no-op.
}

/** 1228:5cd2 dispatch_object_family_parking_state_handler.
 *  Called by dispatch_destination_queue_entries (1218:0883) on carrier arrival. */
export function dispatchObjectFamilyParkingStateHandler(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	runDispatchJumpTable(world, time, sim);
}
