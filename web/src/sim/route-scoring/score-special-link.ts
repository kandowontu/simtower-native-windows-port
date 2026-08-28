// 11b8:0be2 scoreSpecialLinkRoute
//
// Pass/fail viability check for one `specialLinkRecords[recordIndex]`. Returns
// `{ cost: 0, direction }` when the record can shuttle from `fromFloor` to
// `toFloor`, otherwise `{ cost: ROUTE_COST_INFINITE, direction: 0 }`.
//
// Binary structure:
//   1. Reject inactive records or records whose span does not cover src.
//   2. If span covers dst too, succeed with `direction = src < dst ? 1 : 0`.
//   3. Otherwise consult the per-floor reachability table. The src entry of
//      that table is a 1-based index into `transferGroupEntries` for floors
//      inside the span, or a peer-bitmask for floors outside. The dst entry
//      is a peer-bitmask (the floor lives outside the span here).
//      A direct match requires `mask[dst] != 0`, `mask[src] != 0`, and the
//      carriers reachable via `transferGroupEntries[mask[src]-1]` to be a
//      superset of `mask[dst]`. Failing that, fall back to
//      `testSpecialLinkTransferReachability`.

import { testSpecialLinkTransferReachability } from "../reachability/mask-tests";
import type { WorldState } from "../world";
import { ROUTE_COST_INFINITE } from "./constants";

export interface SpecialLinkRouteResult {
	cost: number;
	direction: number;
}

const FAIL: SpecialLinkRouteResult = {
	cost: ROUTE_COST_INFINITE,
	direction: 0,
};

export function scoreSpecialLinkRoute(
	world: WorldState,
	recordIndex: number,
	fromFloor: number,
	toFloor: number,
): SpecialLinkRouteResult {
	const record = world.specialLinkRecords[recordIndex];
	if (!record?.active) return FAIL;

	const srcInSpan =
		fromFloor >= record.lowerFloor && fromFloor <= record.upperFloor;
	if (!srcInSpan) return FAIL;

	const dstInSpan =
		toFloor >= record.lowerFloor && toFloor <= record.upperFloor;
	if (dstInSpan) {
		return { cost: 0, direction: fromFloor < toFloor ? 1 : 0 };
	}

	const dstMask = record.reachabilityMasksByFloor[toFloor] ?? 0;
	if (dstMask !== 0) {
		const srcMask = record.reachabilityMasksByFloor[fromFloor] ?? 0;
		if (srcMask !== 0) {
			// Inside the span the table stores a 1-based index into
			// transferGroupEntries; use that entry's carrierMask as the
			// reachable-peer set and check whether dstMask is a subset.
			const srcEntry = world.transferGroupEntries[srcMask - 1];
			if (srcEntry?.active && (srcEntry.carrierMask & dstMask) === dstMask) {
				return { cost: 0, direction: 0 };
			}
		}
	}

	// Final fallback: peer-record reachability scan via the transfer-group
	// cache. testSpecialLinkTransferReachability operates on a transferGroupEntry,
	// not a record — match the binary by trying every entry that this record
	// participates in.
	const recordBit = 1 << (24 + recordIndex);
	for (const entry of world.transferGroupEntries) {
		if (!entry.active) continue;
		if ((entry.carrierMask & recordBit) === 0) continue;
		if (testSpecialLinkTransferReachability(world, entry, toFloor)) {
			return { cost: 0, direction: 0 };
		}
	}
	return FAIL;
}
