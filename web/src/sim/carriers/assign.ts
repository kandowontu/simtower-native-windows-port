// 1098:0a4c assign_car_to_floor_request
// 1098:0dfc find_best_available_car_for_floor
//
// Floor-call assignment: picks the best car for a floor-call and writes the
// primary (up) / secondary (down) route-status slot with car+1.
import type { CarrierCar, CarrierRecord } from "../world";
import { floorToSlot } from "./slot";
import { recomputeCarTargetAndDirection } from "./target";

// Mirrors binary find_best_available_car_for_floor (1098:0dfc).
// Binary returns (int return_value, int *param_4). The caller
// assign_car_to_floor_request gates all its writes on `return_value != 0`.
// Cases that return 0 (fullAssign=false): A/B (same-floor, doors closed,
// scheduleFlag||dirMatch), C (idle-home candidate already at floor), D (dead).
// Normal path and degenerate: return 1 with param_4 set (fullAssign=true).
//
// The wrap-cost "turn floor" uses the car's `nearestWorkFloor` field (binary
// -0x51), refreshed at the tail of every `recomputeCarTargetAndDirection`.
// The idle-home test uses the static per-car `homeFloor` field (binary
// reachability_masks_by_floor[carIdx-8]), NOT `nearestWorkFloor` — this is
// what determines whether a parked car at its home is treated as available
// for an idle-home dispatch (binary 1098:0eef..0efe).
//
// The binary has NO assignedCount-vs-capacity early-skip — the per-leg
// capacity gate lives inside the per-direction phase scans of
// `selectNextTargetFloor` instead.
export function findBestAvailableCarForFloor(
	carrier: CarrierRecord,
	floor: number,
	directionFlag: number,
): { carIndex: number; fullAssign: boolean } {
	let bestIdleHomeCost = Number.POSITIVE_INFINITY;
	let bestIdleHomeIndex = -1;
	let bestForwardCost = Number.POSITIVE_INFINITY;
	let bestForwardIndex = -1;
	let bestWrapCost = Number.POSITIVE_INFINITY;
	let bestWrapIndex = -1;

	for (const [carIndex, car] of carrier.cars.entries()) {
		if (!car.active) continue;

		// Cases A/B: car at floor with doors closed, and either scheduleFlag is
		// set or direction already matches. Binary returns 0 immediately.
		if (car.currentFloor === floor && car.settleCounter === 0) {
			if (car.scheduleFlag !== 0 || car.directionFlag === directionFlag) {
				return { carIndex, fullAssign: false };
			}
			// else fall through to idle-home / forward / wrap accumulation
		}

		const turnFloor = car.nearestWorkFloor;
		const distance = Math.abs(floor - car.currentFloor);

		// Idle-home candidate: parked at static home floor with no work pending.
		// Binary 1098:0ee9..0efe: reads reachability_masks_by_floor[carIdx-8]
		// (== car.homeFloor) and compares to currentFloor. NOT nearestWorkFloor.
		const isIdleHome =
			car.pendingAssignmentCount === 0 &&
			car.nonemptyDestinationCount === 0 &&
			car.settleCounter === 0 &&
			car.currentFloor === car.homeFloor;

		if (isIdleHome) {
			// Case C: idle-home candidate already at floor
			if (distance === 0) return { carIndex, fullAssign: false };
			if (distance < bestIdleHomeCost) {
				bestIdleHomeCost = distance;
				bestIdleHomeIndex = carIndex;
			}
			continue;
		}

		if (car.directionFlag === directionFlag) {
			const forward =
				directionFlag === 1
					? floor - car.currentFloor
					: car.currentFloor - floor;
			if (forward >= 0) {
				if (forward === 0 && car.settleCounter === 0) {
					// Case C alternate: same-floor same-direction with doors open.
					return { carIndex, fullAssign: false };
				}
				if (forward < bestForwardCost) {
					bestForwardCost = forward;
					bestForwardIndex = carIndex;
				}
				continue;
			}
			// Past sweep end: wrap, with nearest_work_floor as turn floor.
			const cost =
				directionFlag === 1
					? turnFloor * 2 - car.currentFloor - floor
					: car.currentFloor + floor - turnFloor * 2;
			if (cost < bestWrapCost) {
				bestWrapCost = cost;
				bestWrapIndex = carIndex;
			}
			continue;
		}

		// Opposite direction: wrap via nearest_work as turn floor. Binary 1098:0fe0
		// branches on (request floor vs turn), not (request floor vs current): if
		// the request lies on the car's return leg past its reversal point, cost
		// is the wrap distance; otherwise the car passes the request going the
		// wrong way and the cost is the direct separation.
		let cost: number;
		if (directionFlag === 1) {
			if (turnFloor < floor) {
				cost = car.currentFloor + floor - 2 * turnFloor;
			} else {
				cost = car.currentFloor - floor;
			}
		} else {
			if (floor < turnFloor) {
				cost = 2 * turnFloor - car.currentFloor - floor;
			} else {
				cost = floor - car.currentFloor;
			}
		}
		if (cost < bestWrapCost) {
			bestWrapCost = cost;
			bestWrapIndex = carIndex;
		}
	}

	let bestMovingCost: number;
	let bestMovingIndex: number;
	if (bestForwardIndex >= 0) {
		bestMovingCost = bestForwardCost;
		bestMovingIndex = bestForwardIndex;
	} else {
		bestMovingCost = bestWrapCost;
		bestMovingIndex = bestWrapIndex;
	}

	if (bestIdleHomeIndex >= 0 && bestMovingIndex >= 0) {
		// Binary quirk: equality breaks toward the idle-home car — the
		// comparator is strict `(moving - idle) < threshold`, so equal costs
		// fall through to the idle-home branch below.
		if (
			bestMovingCost - bestIdleHomeCost <
			carrier.waitingCarResponseThreshold
		) {
			return { carIndex: bestMovingIndex, fullAssign: true };
		}
		return { carIndex: bestIdleHomeIndex, fullAssign: true };
	}
	if (bestMovingIndex >= 0)
		return { carIndex: bestMovingIndex, fullAssign: true };

	// Binary quirk (1098:10d0): degenerate fallback writes car index 0 and
	// returns fullAssign=true whenever there is NO forward and NO wrap
	// candidate — **even when** an idle-home candidate is tracked. The
	// binary's tail at 1098:1057 / 1098:108e only consults the idle-home
	// cost when paired with a moving candidate (threshold compare against
	// forward or wrap). Standalone idle-home is never used; the tail falls
	// through to 1098:10d0 which hardcodes index 0. Observed at
	// dense_office tick 1975: all 8 cars are idle-home for a direction-0
	// pickup at floor 18, wrap branch never fires (car.dir was opposite
	// but idle-home short-circuited the loop via JMP 0feb before reaching
	// 0fee), so forward=wrap=9999 and the scorer returns 0 — picking car 0
	// instead of the obvious car 6 at distance 1. Mirror the quirk.
	return { carIndex: 0, fullAssign: true };
}

// Mirrors binary assign_car_to_floor_request (1098:0a4c). Top gate: for
// direction=1 (UP) requires primary[floor]==0; for direction=0 (DOWN)
// requires secondary[floor]==0. On fullAssign, writes table slot, increments
// pac, and recomputes target+direction — all unconditionally.
export function assignCarToFloorRequest(
	carrier: CarrierRecord,
	floor: number,
	directionFlag: number,
): void {
	const slot = floorToSlot(carrier, floor);
	if (slot < 0) return;
	const table =
		directionFlag === 1
			? carrier.primaryRouteStatusByFloor
			: carrier.secondaryRouteStatusByFloor;
	if ((table[slot] ?? 0) !== 0) return;

	const result = findBestAvailableCarForFloor(carrier, floor, directionFlag);
	if (result.carIndex < 0) return;
	if (!result.fullAssign) return;

	const carIndex = result.carIndex;
	// Binary 1098:0a4c body is straight-line — no iteration over a per-request
	// "assigned_car" field (there is none; floor-call ownership lives only in
	// `primary/secondary_route_status_by_floor`). TS used to seed the per-route
	// `assignedCarIndex` here so boardWaitingRoutes could gate on it, but that
	// cache goes stale after cancel-then-reassign (e.g., sky_office d3: a car
	// leaves f12 → cancelStaleFloorAssignment clears secondary[2] → next
	// assignCarToFloorRequest picks a new owner, yet pendingRoutes still point
	// at the departed car). The per-route field is now populated lazily by
	// `assignRequestToRuntimeRoute` at drain time (mirroring binary 1218:0d4e's
	// store_request_in_active_route_slot), which is the only place the binary
	// pairs a sim with a specific car.
	const car = carrier.cars[carIndex] as CarrierCar;
	table[slot] = carIndex + 1;
	car.pendingAssignmentCount += 1;
	recomputeCarTargetAndDirection(carrier, car, carIndex);
}
