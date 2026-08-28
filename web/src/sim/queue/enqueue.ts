// 1218:1002 enqueue_request_into_route_queue
//
// Append a route request to the (carrier, floor, direction) ring. The first
// enqueue onto a previously-empty direction queue triggers
// `assign_car_to_floor_request` to pick a serving car; subsequent enqueues
// onto the same ring ride along that assignment until the car services it
// and `clear_floor_requests_on_arrival` zeroes the route-status byte.

import { assignCarToFloorRequest } from "../carriers/assign";
import { floorToSlot, isExpressStopFloor } from "../carriers/slot";
import { syncAssignmentStatus } from "../carriers/sync";
import type { CarrierFloorQueue, CarrierRecord, LobbyMode } from "../world";
import type { RouteRequestRing } from "./route-record";

function getQueueState(
	carrier: CarrierRecord,
	floor: number,
): CarrierFloorQueue | null {
	const slot = floorToSlot(carrier, floor);
	if (slot < 0 || slot >= carrier.floorQueues.length) return null;
	return carrier.floorQueues[slot] ?? null;
}

function getDirectionQueue(
	queue: CarrierFloorQueue,
	directionFlag: number,
): RouteRequestRing {
	return directionFlag === 1 ? queue.up : queue.down;
}

/**
 * Binary `enqueue_request_into_route_queue` (1218:1002).
 *
 * Pushes `simId` onto the direction ring for `(carrier, sourceFloor,
 * directionFlag)`. If the ring was previously empty, triggers
 * `assign_car_to_floor_request` so a car is dispatched to service the new
 * demand. Also adds a matching `CarrierPendingRoute` entry (TS-native
 * bookkeeping; the binary stores this inside the carrier's
 * `active_request_refs` table lazily via `assign_request_to_runtime_route`).
 *
 * Deduplicates on `simId` so a sim that calls resolve twice does not
 * double-enqueue.
 *
 * Binary quirk: size-40 ring silently overwrites head on 41st enqueue —
 * see `RouteRequestRing.push`. This function always returns true (the
 * binary has no full flag).
 */
export function enqueueRequestIntoRouteQueue(
	carrier: CarrierRecord,
	simId: string,
	sourceFloor: number,
	destinationFloor: number,
	directionFlag: number,
	lobbyMode: LobbyMode,
): boolean {
	const traceOn =
		(globalThis as { __DRAIN_TRACE__?: boolean }).__DRAIN_TRACE__ === true;
	const dup = carrier.pendingRoutes.some((route) => route.simId === simId);
	if (traceOn) {
		console.log(
			`[enq] sim=${simId} src=${sourceFloor} dst=${destinationFloor} dir=${directionFlag}${dup ? " DEDUP" : ""}`,
		);
	}
	if (dup) return true;
	// Binary 1218:1002: enqueue's body is gated on floor_to_carrier_slot_index
	// returning a valid slot — express (mode=0) at intermediate (non-lobby)
	// floors silently no-ops while `resolve_sim_route_between_floors` still
	// returns 2 and writes sim+8. Mirror that here so the sim lands in a
	// logical-only "queued on express" state without actually occupying a
	// ring or triggering assign_car_to_floor_request.
	if (
		carrier.carrierMode === 0 &&
		!isExpressStopFloor(sourceFloor, lobbyMode)
	) {
		return true;
	}
	const floorQueue = getQueueState(carrier, sourceFloor);
	if (!floorQueue) return false;
	const directionQueue = getDirectionQueue(floorQueue, directionFlag);
	// Binary: resolve_sim_route_between_floors checks count==40 upstream and
	// returns 0 (queue-full) without invoking enqueue. Mirror that by failing
	// the push here so the caller takes the queue-full path.
	if (directionQueue.isFull) return false;
	const wasDirectionQueueEmpty = directionQueue.isEmpty;
	directionQueue.push(simId);
	carrier.pendingRoutes.push({
		simId,
		sourceFloor,
		destinationFloor,
		boarded: false,
		directionFlag,
		assignedCarIndex: -1,
	});
	if (wasDirectionQueueEmpty) {
		assignCarToFloorRequest(carrier, sourceFloor, directionFlag);
	}
	syncAssignmentStatus(carrier);
	return true;
}
