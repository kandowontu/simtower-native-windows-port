// 1218:142a remove_request_from_unit_queue
// 1218:173a remove_request_from_active_route_slots
// 1218:187b store_request_in_active_route_slot
// 1218:1905 pop_active_route_slot_request
//
// Per-car active-slot ring ops + ring-scan removal. The active-slot ring
// is the 42-entry `active_request_refs` table on each TowerUnitRouteRecord;
// it holds currently-assigned passengers that the car is either en route
// to pick up or carrying toward a destination.

import { floorToSlot } from "../carriers/slot";
import {
	activeSlotLimitFor,
	hasActiveSlot,
	normalizeInactiveSlots,
	syncPendingRouteIds,
} from "../carriers/sync";
import { recomputeCarTargetAndDirection } from "../carriers/target";
import type { CarrierCar, CarrierRecord } from "../world";
import type { RouteRequestRing } from "./route-record";

function getDirectionRing(
	carrier: CarrierRecord,
	sourceFloor: number,
	directionFlag: number,
): RouteRequestRing | null {
	const slot = floorToSlot(carrier, sourceFloor);
	if (slot < 0 || slot >= carrier.floorQueues.length) return null;
	const queue = carrier.floorQueues[slot];
	if (!queue) return null;
	return directionFlag === 1 ? queue.up : queue.down;
}

/**
 * Binary `remove_request_from_unit_queue` (1218:142a). Scans a floor's
 * direction ring and compacts out the entry whose request id matches
 * `simId`, preserving the relative order of the remaining entries. Returns
 * true on hit, false otherwise.
 */
export function removeRequestFromUnitQueue(
	carrier: CarrierRecord,
	simId: string,
	sourceFloor: number,
	directionFlag: number,
): boolean {
	const ring = getDirectionRing(carrier, sourceFloor, directionFlag);
	if (!ring) return false;
	return ring.removeFirst(simId);
}

/**
 * Binary `store_request_in_active_route_slot` (1218:187b). Writes the
 * request id + source/destination floors into the first free slot of the
 * car's `active_request_refs` ring. Returns true when stored, false when
 * every slot up to `assignment_capacity` is occupied.
 *
 * Re-uses a slot the caller already owns (via `hasActiveSlot`) to avoid
 * duplicate slot inserts.
 */
export function storeRequestInActiveRouteSlot(
	carrier: CarrierRecord,
	car: CarrierCar,
	route: CarrierRecord["pendingRoutes"][number],
): boolean {
	if (hasActiveSlot(car, route.simId)) return true;
	const limit = activeSlotLimitFor(carrier);
	for (let index = 0; index < limit; index++) {
		const slot = car.activeRouteSlots[index];
		if (!slot || slot.active) continue;
		slot.routeId = route.simId;
		slot.sourceFloor = route.sourceFloor;
		slot.destinationFloor = route.destinationFloor;
		slot.boarded = route.boarded;
		slot.active = true;
		syncPendingRouteIds(car);
		return true;
	}
	return false;
}

/**
 * Binary `pop_active_route_slot_request` (1218:1905). Clears the active
 * slot identified by its slot index and returns the request id that was
 * there. This is the consumer the car's arrival path uses to mark a
 * rider as disembarked before clearing `assigned_count`.
 */
export function popActiveRouteSlotRequest(
	car: CarrierCar,
	slotIndex: number,
): string | undefined {
	const slot = car.activeRouteSlots[slotIndex];
	if (!slot?.active) return undefined;
	const id = slot.routeId;
	slot.active = false;
	slot.routeId = "";
	slot.sourceFloor = 0xff;
	slot.destinationFloor = 0xff;
	slot.boarded = false;
	syncPendingRouteIds(car);
	return id;
}

/**
 * Binary `remove_request_from_active_route_slots` (1218:173a). Scans cars
 * in index order; on the FIRST car/slot whose routeId matches, clears the
 * slot, then invokes `recompute_car_target_and_direction` for THAT car and
 * returns 1. Returns 0 if no slot matched.
 *
 * The recompute call is how the binary re-evaluates a car's sweep direction
 * the moment a rider/route is pulled out of its active set — most notably,
 * it lets `update_car_direction_flag` (1098:1d2f) flip an express car at
 * the top served floor (arrivalSeen==1, dir==1) to dir==0 when a cancel
 * runs during the dwell-5 arrival path.
 *
 * Note on counter decrements: the binary also decrements `assignedCount`
 * (-0x5b) and `destinationCountByFloor[+0xc]` for the slot's destination
 * floor inline here. The TS counter model treats `assignedCount` as
 * boarded-riders-only (incremented in `boardWaitingRoutes`), so the cancel
 * path leaves `assignedCount` alone — counter resync runs through
 * `syncAssignmentStatus` after the cancel completes. Mirroring the binary's
 * unconditional decrement here would mis-balance non-boarded slot removals,
 * so we keep that out of this routine.
 */
export function removeRequestFromActiveRouteSlots(
	carrier: CarrierRecord,
	simId: string,
): number {
	for (let carIndex = 0; carIndex < carrier.cars.length; carIndex++) {
		const car = carrier.cars[carIndex];
		if (!car) continue;
		const limit = activeSlotLimitFor(carrier);
		for (let slotIndex = 0; slotIndex < limit; slotIndex++) {
			const slot = car.activeRouteSlots[slotIndex];
			if (!slot?.active || slot.routeId !== simId) continue;
			slot.active = false;
			normalizeInactiveSlots(car);
			syncPendingRouteIds(car);
			recomputeCarTargetAndDirection(carrier, car, carIndex);
			return 1;
		}
	}
	return 0;
}
