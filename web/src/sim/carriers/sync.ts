// Carrier-side derived-state sync helpers.
//
// The binary maintains per-car counters (waiting_count[], destination_count[],
// pending_route_ids, slot_destination_floors) incrementally as requests move
// through the queue. The current TS model keeps `pendingRoutes` as the source
// of truth and resyncs the derived counters after each mutation through
// `syncAssignmentStatus`. These helpers live here (not in carriers.ts) so the
// queue/ modules can call them without an import cycle.

import type { CarrierCar, CarrierRecord } from "../world";
import { floorToSlot } from "./slot";

const ACTIVE_SLOT_CAPACITY = 42;

function activeSlotLimit(carrier: CarrierRecord): number {
	return Math.min(ACTIVE_SLOT_CAPACITY, carrier.assignmentCapacity);
}

function findRoute(carrier: CarrierRecord, routeId: string) {
	return carrier.pendingRoutes.find((route) => route.simId === routeId);
}

export function syncPendingRouteIds(car: CarrierCar): void {
	car.pendingRouteIds = car.activeRouteSlots
		.filter((slot) => slot.active)
		.map((slot) => slot.routeId);
}

// Binary preserves slot indices across syncs: `pop_active_route_slot_request`
// (1218:1905) clears in place, and `store_request_in_active_route_slot`
// (1218:187b) scans from index 0 and reuses the first freed slot. Compacting
// here would shift later slots forward and reorder boarding in
// `dispatchDestinationQueueEntries`, so we walk in place instead — clearing
// inactive/orphaned slots to the sentinel, refreshing live slot fields from
// the route table, and only padding when the array is shorter than capacity
// (snapshot hydration), never by appending after a compaction.
export function syncRouteSlots(carrier: CarrierRecord, car: CarrierCar): void {
	for (let i = 0; i < car.activeRouteSlots.length; i++) {
		const slot = car.activeRouteSlots[i];
		if (!slot) continue;
		if (!slot.active) {
			slot.routeId = "";
			slot.sourceFloor = 0xff;
			slot.destinationFloor = 0xff;
			slot.boarded = false;
			continue;
		}
		const route = findRoute(carrier, slot.routeId);
		if (!route) {
			slot.active = false;
			slot.routeId = "";
			slot.sourceFloor = 0xff;
			slot.destinationFloor = 0xff;
			slot.boarded = false;
			continue;
		}
		slot.sourceFloor = route.sourceFloor;
		slot.destinationFloor = route.destinationFloor;
		slot.boarded = route.boarded;
	}
	while (car.activeRouteSlots.length < ACTIVE_SLOT_CAPACITY) {
		car.activeRouteSlots.push({
			routeId: "",
			sourceFloor: 0xff,
			destinationFloor: 0xff,
			boarded: false,
			active: false,
		});
	}
	syncPendingRouteIds(car);
}

export function hasActiveSlot(car: CarrierCar, routeId: string): boolean {
	return car.activeRouteSlots.some(
		(slot) => slot.active && slot.routeId === routeId,
	);
}

export function addRouteSlot(
	carrier: CarrierRecord,
	car: CarrierCar,
	route: CarrierRecord["pendingRoutes"][number],
): boolean {
	if (hasActiveSlot(car, route.simId)) return true;
	const limit = activeSlotLimit(carrier);
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

function syncWaitingCount(
	carrier: CarrierRecord,
	car: CarrierCar,
	carIndex: number,
): void {
	car.waitingCount.fill(0);
	car.destinationCountByFloor.fill(0);
	car.nonemptyDestinationCount = 0;

	for (let slot = 0; slot < carrier.floorQueues.length; slot++) {
		const queue = carrier.floorQueues[slot];
		if (!queue) continue;
		car.waitingCount[slot] = queue.up.size + queue.down.size;
	}

	const limit = activeSlotLimit(carrier);
	for (let index = 0; index < limit; index++) {
		const slotRef = car.activeRouteSlots[index];
		if (!slotRef?.active || !slotRef.boarded) continue;
		const slot = floorToSlot(carrier, slotRef.destinationFloor);
		if (slot < 0 || slot >= car.destinationCountByFloor.length) continue;
		const prev = car.destinationCountByFloor[slot] ?? 0;
		car.destinationCountByFloor[slot] = prev + 1;
		if (prev === 0) car.nonemptyDestinationCount += 1;
	}

	for (const route of carrier.pendingRoutes) {
		if (!route.boarded || route.assignedCarIndex !== carIndex) continue;
		const slot = floorToSlot(carrier, route.destinationFloor);
		if (slot < 0 || slot >= car.destinationCountByFloor.length) continue;
		const prev = car.destinationCountByFloor[slot] ?? 0;
		if (prev === 0) car.nonemptyDestinationCount += 1;
		car.destinationCountByFloor[slot] = prev + 1;
	}
}

// Rebuilds per-car derived state from the authoritative route/status tables.
// The binary mutates pendingAssignmentCount incrementally with the route-slot
// ownership tables, so if a TS path misses one of those inline decrements the
// count drifts. Recompute it here from primary/secondary ownership so target
// selection and dwell gates observe the same ownership state as the tables.
export function syncAssignmentStatus(carrier: CarrierRecord): void {
	for (const car of carrier.cars) {
		car.pendingAssignmentCount = 0;
	}
	for (const owner of carrier.primaryRouteStatusByFloor) {
		if (owner > 0) {
			const car = carrier.cars[owner - 1];
			if (car) car.pendingAssignmentCount += 1;
		}
	}
	for (const owner of carrier.secondaryRouteStatusByFloor) {
		if (owner > 0) {
			const car = carrier.cars[owner - 1];
			if (car) car.pendingAssignmentCount += 1;
		}
	}
	for (const [carIndex, car] of carrier.cars.entries()) {
		syncRouteSlots(carrier, car);
		syncWaitingCount(carrier, car, carIndex);
	}
}

export function normalizeInactiveSlots(car: CarrierCar): void {
	for (const slot of car.activeRouteSlots) {
		if (!slot.active) {
			slot.routeId = "";
			slot.sourceFloor = 0xff;
			slot.destinationFloor = 0xff;
			slot.boarded = false;
		}
	}
}

export const ACTIVE_SLOT_CAPACITY_CONST = ACTIVE_SLOT_CAPACITY;
export const activeSlotLimitFor = activeSlotLimit;
export const findRouteById = findRoute;
