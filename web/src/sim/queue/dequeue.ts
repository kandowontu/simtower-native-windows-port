// 1218:1172 pop_unit_queue_request
//
// Pops the head request id from a (carrier, floor, direction) ring. This
// file is the pure pop — `reduce_elapsed_for_lobby_boarding` fires inside
// the boarding loop (`queue/process-travel.ts#boardWaitingRoutes`,
// promoted into the inline boarding path by Phase 7).

import type { CarrierFloorQueue } from "../world";
import type { RouteRequestRing } from "./route-record";

/**
 * Binary `pop_unit_queue_request` (1218:1172). Pops the head of the ring
 * for the given direction and returns the request id. Returns undefined on
 * an empty ring. The binary follows the pop with
 * `reduce_elapsed_for_lobby_boarding`; that helper now runs inline inside
 * `boardWaitingRoutes` (Phase 7).
 */
export function popUnitQueueRequest(
	queue: CarrierFloorQueue,
	directionFlag: number,
): string | undefined {
	const ring: RouteRequestRing = directionFlag === 1 ? queue.up : queue.down;
	return ring.pop();
}
