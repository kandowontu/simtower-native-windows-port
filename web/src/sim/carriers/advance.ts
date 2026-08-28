// 1098:06fb advance_carrier_car_state
//
// Pass 1 of the per-tick carrier update, invoked from carrier_tick
// (1098:03ab) before dispatch_carrier_car_arrivals and process_unit_travel_queue.
// Three-way branch on two countdowns (settle / dwell) driving the car state
// machine described in ROUTING-BINARY-MAP.md §4.5.
//
// State counters correspond to the binary's car struct fields:
//   settleCounter ↔ -0x5d (settle after a motion step)
//   dwellCounter  ↔ -0x5c (dwell/boarding at a stop; 5 = just arrived)
import type { TimeState } from "../time";
import type { CarrierCar, CarrierRecord, LobbyMode } from "../world";
import {
	cancelStaleFloorAssignment,
	clearFloorRequestsOnArrival,
	resetOutOfRangeCar,
} from "./arrival";
import { assignCarToFloorRequest } from "./assign";
import { shouldCarDepart } from "./depart";
import { computeCarMotionMode } from "./motion";
import { advanceCarPositionOneStep } from "./position";
import { floorToSlot } from "./slot";
import { recomputeCarTargetAndDirection } from "./target";

const DEPARTURE_SEQUENCE_TICKS = 5;

function getCarCapacity(carrier: CarrierRecord): number {
	return carrier.assignmentCapacity;
}

function getScheduleIndex(time: TimeState): number {
	return time.weekendFlag * 7 + time.daypartIndex;
}

export function advanceCarrierCarState(
	car: CarrierCar,
	carrier: CarrierRecord,
	carIndex: number,
	time: TimeState,
	lobbyMode: LobbyMode,
): void {
	if (!car.active) return;

	if (
		car.currentFloor < carrier.bottomServedFloor ||
		car.currentFloor > carrier.topServedFloor
	) {
		resetOutOfRangeCar(carrier, car);
		return;
	}

	// Branch C (1098:06fb): stabilize countdown. While nonzero, the car is
	// mid-motion-cycle. Decrement only if recomputed mode is still 0;
	// otherwise snap to 0 (fast-cancel).
	if (car.settleCounter > 0) {
		if (computeCarMotionMode(carrier, car) === 0) car.settleCounter--;
		else car.settleCounter = 0;
		return;
	}

	if (car.dwellCounter === 0) {
		car.suppressDwellOppositeDirectionFlip = false;
		// Branch A. A1 (arrival / idle-at-target) fires when target == cur and
		// either the per-car destination queue has riders for this floor, or the
		// car is not full. A1 is level-triggered: as long as the gate holds the
		// next A1 fire (after B's 5-tick countdown) refreshes dwell back to 5.
		const currentSlot = floorToSlot(carrier, car.currentFloor);
		const hasQueuedRider =
			currentSlot >= 0 && (car.destinationCountByFloor[currentSlot] ?? 0) > 0;
		const underCapacity = car.assignedCount !== getCarCapacity(carrier);

		if (
			car.targetFloor === car.currentFloor &&
			(hasQueuedRider || underCapacity)
		) {
			// A1
			// Inlined from former loadScheduleFlag helper (binary reloads the
			// scheduling_flag inline within advance_carrier_car_state when a car
			// sits at a terminal floor).
			if (
				car.currentFloor === carrier.topServedFloor ||
				car.currentFloor === carrier.bottomServedFloor
			) {
				car.scheduleFlag =
					carrier.expressDirectionFlags[getScheduleIndex(time)] ?? 0;
			}
			clearFloorRequestsOnArrival(carrier, car, car.currentFloor);
			car.dwellStartPendingAssignmentCount = car.pendingAssignmentCount;
			car.dwellCounter = DEPARTURE_SEQUENCE_TICKS;
			if (car.arrivalSeen === 0) {
				car.arrivalTick = time.dayTick;
			}
			car.arrivalSeen = 1;
			return;
		}

		// A2 — motion step. Binary 1098:06fb A2 calls cancel_stale_floor_assignment
		// (1098:12c9) FIRST (which may clear primary/secondary[floor] if assigned
		// to this car), THEN reads the queue + status to decide whether to re-assign.
		// Order matters: cancel runs before the up/down gate check.
		const departFloor = car.currentFloor;
		const departSlot = floorToSlot(carrier, departFloor);
		cancelStaleFloorAssignment(carrier, car, departFloor, carIndex);
		const queue = departSlot >= 0 ? carrier.floorQueues[departSlot] : null;
		const hasUpRequest =
			queue != null &&
			!queue.up.isEmpty &&
			(carrier.primaryRouteStatusByFloor[departSlot] ?? 0) === 0;
		const hasDownRequest =
			queue != null &&
			!queue.down.isEmpty &&
			(carrier.secondaryRouteStatusByFloor[departSlot] ?? 0) === 0;
		advanceCarPositionOneStep(carrier, car, carIndex);
		if (hasUpRequest) assignCarToFloorRequest(carrier, departFloor, 1);
		if (hasDownRequest) assignCarToFloorRequest(carrier, departFloor, 0);
		return;
	}

	// Branch B — dwell countdown. Decrement unconditionally; on 0 transition,
	// latch prevFloor, recompute target+direction, then check departure gate.
	// If the gate says "wait", pin dwell=1 so this path runs again next tick.
	// Binary 1098:06fb: shouldCarDepart runs AFTER recomputeCarTargetAndDirection
	// (see ROUTING-BINARY-MAP.md §4.5).
	car.dwellCounter--;
	if (car.dwellCounter === 0) {
		car.suppressDwellOppositeDirectionFlip = false;
		car.prevFloor = car.currentFloor;
		recomputeCarTargetAndDirection(carrier, car, carIndex);
		if (!shouldCarDepart(carrier, car, time, lobbyMode)) {
			car.dwellCounter = 1;
		}
	}
}
