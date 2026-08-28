// 1218:1a86 cancel_runtime_route_request
// 1218:1981 dispatch_queued_route_until_request
// 1218:0fc4 decrement_route_queue_direction_load
//
// Cancellation path: pulls a request out of (a) the floor ring for its
// source floor and (b) any car's active-slot ring, and clears the sim's
// per-route bookkeeping. Used when a sim times out waiting or the family
// dispatcher decides to abandon a route mid-flight.

import { syncAssignmentStatus } from "../carriers/sync";
import type { CarrierRecord } from "../world";
import {
	removeRequestFromActiveRouteSlots,
	removeRequestFromUnitQueue,
} from "./scan";

function findRoute(carrier: CarrierRecord, simId: string) {
	return carrier.pendingRoutes.find((route) => route.simId === simId);
}

/**
 * Binary `decrement_route_queue_direction_load` (1218:0fc4). Decrements
 * the per-direction "queued load" counters that feed into the route
 * scorer's prefer-less-loaded-direction tiebreak. The current TS scorer
 * does not yet consult these counters; this stub exists so the call sites
 * in cancel + the family prologue match the binary graph.
 *
 * TODO(1218:0fc4): wire to an actual load counter once the TS scorer
 * learns to read it (companion change for the parity-based per-stop delay
 * lookup).
 */
export function decrementRouteQueueDirectionLoad(
	_carrier: CarrierRecord,
	_sourceFloor: number,
	_directionFlag: number,
): void {
	// Intentionally empty. See TODO above.
}

/**
 * Binary `dispatch_queued_route_until_request` (1218:1981). Pops ring
 * entries in FIFO order until a specific request id is dequeued; each
 * popped (preceding) entry is routed through the family dispatcher as if
 * it had been picked up. In practice the binary uses this to flush the
 * queue ahead of a cancel so ordering stays consistent.
 *
 * The clean-room sim does not yet have a family-dispatch-from-queue path
 * (that lands in Phases 5–6), so today this function only drains entries
 * preceding the target into a no-op pass and returns true iff the target
 * was reached.
 *
 * TODO(1218:1981): once family dispatchers own demand origination, pipe
 * each popped preceding request into the appropriate handler.
 */
export function dispatchQueuedRouteUntilRequest(
	carrier: CarrierRecord,
	simId: string,
	sourceFloor: number,
	directionFlag: number,
): boolean {
	return removeRequestFromUnitQueue(carrier, simId, sourceFloor, directionFlag);
}

/**
 * Binary `cancel_runtime_route_request` (1218:1a86). Full cancel path:
 * scans the floor ring and every car's active-slot ring, removes the
 * request id from both, then drops it from `pendingRoutes` and resyncs
 * derived per-car state. Called from
 * `maybe_dispatch_queued_route_after_wait` (1228:15a0) when an office sim
 * times out waiting for its queued elevator.
 *
 */
export function cancelRuntimeRouteRequest(
	carrier: CarrierRecord,
	simId: string,
): void {
	const route = findRoute(carrier, simId);
	if (!route) return;
	const clearedSlots = removeRequestFromActiveRouteSlots(carrier, simId);
	if (clearedSlots === 0) {
		removeRequestFromUnitQueue(
			carrier,
			simId,
			route.sourceFloor,
			route.directionFlag,
		);
	}
	carrier.pendingRoutes = carrier.pendingRoutes.filter(
		(candidate) => candidate.simId !== simId,
	);
	syncAssignmentStatus(carrier);
}
