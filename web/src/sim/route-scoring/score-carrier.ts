// 11b8:168e scoreCarrierTransferRoute
//
// Cost functions for carrier (elevator) routes. The binary map lists a
// single `scoreCarrierTransferRoute` entry, but the current TS scorer
// splits into direct vs. transfer variants — both kept here and exported
// under the binary name suffixes so the selector can call them independently.
//
// TODO(11b8:168e): confirm whether the binary folds the direct and
// transfer paths into one function. If so, merge these two; otherwise
// keep them split and treat both as helpers of the binary entry.

import { carrierSpansFloor } from "../carriers";
import { isExpressStopFloor } from "../carriers/slot";
import { testCarrierTransferReachability } from "../reachability/mask-tests";
import type { WorldState } from "../world";
import {
	DIRECT_ROUTE_BASE_COST,
	DIRECT_ROUTE_FULL_QUEUE_COST,
	QUEUE_FULL_COUNT,
	ROUTE_COST_INFINITE,
	TRANSFER_ROUTE_BASE_COST,
	TRANSFER_ROUTE_FULL_QUEUE_COST,
} from "./constants";

/**
 * Binary `score_carrier_transfer_route` (11b8:168e) gates eligibility on
 * `served_floor_flags[floor] != 0`. In the binary that bit is the union of
 * "this floor is in the carrier's served set" (set when the shaft is built/
 * extended) and "the player has not toggled this floor off" (cleared by the
 * per-floor stop-dialog button at FUN_10a8_0085). TS splits these into
 * `carrierSpansFloor` (geometric [bottom, top] coverage) and
 * `stopFloorEnabled[slot]` (player toggle), so the gate must AND both.
 *
 * Express carriers additionally restrict to lobby/sky-lobby slots even within
 * their span, so chain `isExpressStopFloor` for mode-0.
 */
function carrierEligibleFloor(
	carrier: WorldState["carriers"][number],
	floor: number,
	lobbyMode: WorldState["lobbyMode"],
): boolean {
	if (!carrierSpansFloor(carrier, floor)) return false;
	const slot = floor - carrier.bottomServedFloor;
	if ((carrier.stopFloorEnabled[slot] ?? 1) === 0) return false;
	if (carrier.carrierMode === 0) {
		// Express: only stops at internal/binary floors 1..10 (basement/ground)
		// and the lobby cadence above. The `floor` parameter is in internal
		// numbering — same convention `isExpressStopFloor` expects.
		return isExpressStopFloor(floor, lobbyMode);
	}
	return true;
}

function getFloorQueueCount(
	carrier: WorldState["carriers"][number],
	floor: number,
	directionFlag: number,
): number {
	const slot = floor - carrier.bottomServedFloor;
	if (slot < 0 || slot >= carrier.floorQueues.length) return 0;
	const queue = carrier.floorQueues[slot];
	if (!queue) return 0;
	const directionQueue = directionFlag === 1 ? queue.up : queue.down;
	return Math.min(directionQueue.size, QUEUE_FULL_COUNT);
}

/**
 * Scores a carrier route where the carrier directly serves both endpoints.
 * Mirrors the "carrier directly serves floor" branch of the binary's
 * `scoreCarrierTransferRoute` (11b8:168e).
 */
export function scoreCarrierDirectRoute(
	world: WorldState,
	carrierId: number,
	fromFloor: number,
	toFloor: number,
	targetHeightMetric: number,
): number {
	const carrier = world.carriers.find(
		(candidate) => candidate.carrierId === carrierId,
	);
	if (!carrier) return ROUTE_COST_INFINITE;
	if (!carrierEligibleFloor(carrier, fromFloor, world.lobbyMode))
		return ROUTE_COST_INFINITE;
	if (!carrierEligibleFloor(carrier, toFloor, world.lobbyMode))
		return ROUTE_COST_INFINITE;
	const qCount = getFloorQueueCount(
		carrier,
		fromFloor,
		toFloor > fromFloor ? 1 : 0,
	);
	// 11b8:168e direct branch. mode==0 (express) ignores distance and adds the
	// integer queue count directly: cost = qCount + 640. mode!=0 uses hypot
	// distance plus a step-function penalty that replaces the base with 1000
	// only when the queue is saturated (==40).
	if (carrier.carrierMode === 0) {
		return qCount + DIRECT_ROUTE_BASE_COST;
	}
	const distance = Math.abs(carrier.column - targetHeightMetric) * 8;
	return (
		distance +
		(qCount === QUEUE_FULL_COUNT
			? DIRECT_ROUTE_FULL_QUEUE_COST
			: DIRECT_ROUTE_BASE_COST)
	);
}

/**
 * 11b8:168e scoreCarrierTransferRoute
 *
 * Scores a multi-leg carrier route: the carrier covers the source floor,
 * and a transfer group exists that reaches the destination through a
 * peer carrier or derived special-link record.
 */
export function scoreCarrierTransferRoute(
	world: WorldState,
	carrierId: number,
	fromFloor: number,
	toFloor: number,
	preferLocalMode: boolean,
	targetHeightMetric: number,
): number {
	const carrier = world.carriers.find(
		(candidate) => candidate.carrierId === carrierId,
	);
	if (!carrier) return ROUTE_COST_INFINITE;
	if (!carrierEligibleFloor(carrier, fromFloor, world.lobbyMode))
		return ROUTE_COST_INFINITE;
	if (
		!testCarrierTransferReachability(world, carrierId, toFloor, preferLocalMode)
	) {
		return ROUTE_COST_INFINITE;
	}
	const qCount = getFloorQueueCount(
		carrier,
		fromFloor,
		toFloor > fromFloor ? 1 : 0,
	);
	// 11b8:168e transfer branch. Same shape as direct branch with base=3000,
	// full-queue penalty=6000.
	if (carrier.carrierMode === 0) {
		return qCount + TRANSFER_ROUTE_BASE_COST;
	}
	const distance = Math.abs(carrier.column - targetHeightMetric) * 8;
	return (
		distance +
		(qCount === QUEUE_FULL_COUNT
			? TRANSFER_ROUTE_FULL_QUEUE_COST
			: TRANSFER_ROUTE_BASE_COST)
	);
}
