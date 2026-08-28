// 10a8:17ee floor_to_carrier_slot_index
//
// Maps (carrier, floor) → per-floor queue slot index. Returns -1 when the
// floor is outside the served range or not a lobby/express slot on an
// express carrier.
import type { CarrierRecord, LobbyMode } from "../world";

export function floorToSlot(carrier: CarrierRecord, floor: number): number {
	if (floor < carrier.bottomServedFloor || floor > carrier.topServedFloor) {
		return -1;
	}
	return floor - carrier.bottomServedFloor;
}

/**
 * Binary 10a8:17ee express branch. Only the "express queue slot" floors —
 * basement/ground (binary floors 1..10) and the sky-lobby cadence above.
 *   "perfect-parity": sky lobbies at (floor-10) % 15 == 14
 *     (binary 24, 39, 54, ... = logical 14, 29, 44, ...).
 *   "modern": sky lobbies at (floor-10) % 15 == 0 with floor > 10
 *     (binary 25, 40, 55, ... = logical 15, 30, 45, ...).
 * Any other floor on a mode-0 carrier cannot host a call or enqueue.
 * Used by the scorer and enqueue paths to gate floor acceptance without
 * changing `floorToSlot`'s linear slot arithmetic (which the per-car
 * destination-count and route-status tables rely on).
 */
export function isExpressStopFloor(floor: number, mode: LobbyMode): boolean {
	if (floor <= 10) return floor >= 1;
	const cycleOffset = mode === "modern" ? 0 : 14;
	return (floor - 10) % 15 === cycleOffset;
}

export function carrierServesFloor(
	carrier: CarrierRecord,
	floor: number,
): boolean {
	return floorToSlot(carrier, floor) >= 0;
}

/**
 * Geometric span check: `floor ∈ [bottomServedFloor, topServedFloor]`. NOT a
 * substitute for binary `served_floor_flags[f]`, which on express carriers is
 * gated by FUN_10a8_1296 to set the byte only at express-stop floors (binary
 * 1..10 and the sky-lobby cadence). Use this where a pure span test is wanted
 * (e.g. resetting out-of-range cars); use `carrierEligibleFloor` /
 * `isExpressStopFloor` for the served-flag-equivalent gate the binary applies
 * to route scoring and transfer-floor remapping.
 */
export function carrierSpansFloor(
	carrier: CarrierRecord,
	floor: number,
): boolean {
	return floor >= carrier.bottomServedFloor && floor <= carrier.topServedFloor;
}
