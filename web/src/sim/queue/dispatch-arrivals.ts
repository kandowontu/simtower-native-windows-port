// 1218:07a6 dispatch_carrier_car_arrivals
// 1218:0883 dispatch_destination_queue_entries
//
// Per-tick arrival pass: at `dwellCounter == 5` (first dwell tick after
// stop), walks the car's active-slot ring and drops any slot whose
// destination matches the current floor. For each dropped slot, the binary
// calls the matching family's `dispatch_object_family_*_state_handler`
// inline — see `dispatchSimArrival` in ../sims/index.ts for the TS-side
// family dispatch. Phase 7 removed the `onArrival` callback trampoline;
// arrival now invokes the family handler directly from this file.

import { floorToSlot } from "../carriers/slot";
import {
	activeSlotLimitFor,
	syncAssignmentStatus,
	syncPendingRouteIds,
} from "../carriers/sync";
import type { LedgerState } from "../ledger";
import { dispatchSimArrival } from "../sims";
import { simKey } from "../sims/population";
import type { TimeState } from "../time";
import type { CarrierCar, CarrierRecord, WorldState } from "../world";

const DEPARTURE_SEQUENCE_TICKS = 5;

/**
 * Binary `dispatch_destination_queue_entries` (1218:0883). Iterates the
 * car's active-slot ring up to `assignment_capacity`; for each boarded
 * slot whose destinationFloor matches the car's currentFloor, the binary
 * runs the per-slot sequence:
 *
 *   1. `pop_active_route_slot_request` (1218:1905) — clears the slot in
 *      place and yields the routeId.
 *   2. Family dispatch: switch on the sim's family code and invoke
 *      `dispatch_object_family_*_state_handler` (or the
 *      housekeeping/entertainment/recycling/parking variants). The handler
 *      may itself call `cancel_runtime_route_request` (1218:1a86), whose
 *      inner `remove_request_from_active_route_slots` (1218:173a) ends
 *      with `recompute_car_target_and_direction` — the mechanism by which
 *      an express car at the top served floor flips its directionFlag
 *      1→0 the moment its first/last disembarking rider is processed.
 *   3. Decrement `assignedCount` (-0x5b) and the per-floor
 *      `destinationCountByFloor[+0xc]` slot. The
 *      `nonemptyDestinationCount` (-0x52) decrement runs once at
 *      loop-exit when the per-floor counter has reached 0.
 *
 * Order matters: the family handler must run BEFORE the counter
 * decrements so that the in-handler recompute observes the same
 * `assignedCount` / `destinationCountByFloor` state the binary did.
 */
export function dispatchDestinationQueueEntries(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	carrier: CarrierRecord,
	car: CarrierCar,
): boolean {
	let changed = false;
	const assignedBeforeArrivals = car.assignedCount;
	const limit = activeSlotLimitFor(carrier);
	const destinationSlot = floorToSlot(carrier, car.currentFloor);

	for (let index = 0; index < limit; index++) {
		const slot = car.activeRouteSlots[index];
		if (!slot?.active || !slot.boarded) continue;
		if (slot.destinationFloor !== car.currentFloor) continue;

		// Step 1: pop_active_route_slot_request — clear the slot in place,
		// matching binary order so the family handler sees the slot as
		// already consumed if it walks the ring (e.g., via
		// remove_request_from_active_route_slots inside cancel).
		const arrivedRouteId = slot.routeId;
		const arrivedFloor = slot.destinationFloor;
		slot.active = false;
		slot.routeId = "";
		slot.sourceFloor = 0xff;
		slot.destinationFloor = 0xff;
		slot.boarded = false;

		// Step 2: family dispatch. Look up the sim and invoke the
		// per-family state handler. This may recursively touch the same
		// car's slots/direction via cancel_runtime_route_request.
		const sim = world.sims.find((s) => simKey(s) === arrivedRouteId);
		if (sim) {
			dispatchSimArrival(world, ledger, time, sim, arrivedFloor);
		}

		// Step 3: decrement counters. Done AFTER the handler so the in-
		// handler recompute sees the binary's unmodified count snapshot.
		car.assignedCount = Math.max(0, car.assignedCount - 1);
		if (destinationSlot >= 0) {
			const prev = car.destinationCountByFloor[destinationSlot] ?? 0;
			car.destinationCountByFloor[destinationSlot] = Math.max(0, prev - 1);
			if (prev === 1) {
				car.nonemptyDestinationCount = Math.max(
					0,
					car.nonemptyDestinationCount - 1,
				);
			}
		}

		carrier.pendingRoutes = carrier.pendingRoutes.filter(
			(candidate) => candidate.simId !== arrivedRouteId,
		);
		if (!carrier.completedRouteIds.includes(arrivedRouteId)) {
			carrier.completedRouteIds.push(arrivedRouteId);
		}
		changed = true;
	}

	if (changed) {
		car.arrivalDispatchThisTick = true;
		car.arrivalDispatchStartingAssignedCount = assignedBeforeArrivals;
		syncPendingRouteIds(car);
		syncAssignmentStatus(carrier);
	}
	return changed;
}

/**
 * Binary `dispatch_carrier_car_arrivals` (1218:07a6). Gate on
 * `dwellCounter == 5` (first dwell tick after arrival); delegates to
 * `dispatch_destination_queue_entries` for the actual slot-unload work.
 */
export function dispatchCarrierCarArrivals(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	carrier: CarrierRecord,
	car: CarrierCar,
): void {
	if (!car.active) return;
	if (car.dwellCounter !== DEPARTURE_SEQUENCE_TICKS) return;
	dispatchDestinationQueueEntries(world, ledger, time, carrier, car);
}
