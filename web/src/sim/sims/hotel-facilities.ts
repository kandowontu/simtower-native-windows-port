import { FAMILY_CONDO, FAMILY_OFFICE } from "../resources";
import type { TimeState } from "../time";
import type { PlacedObjectRecord, WorldState } from "../world";
import { findObjectForSim } from "./population";
import {
	recomputeObjectOperationalStatus,
	refreshOccupiedFlagAndTripCounters,
} from "./scoring";
import {
	HOTEL_FAMILIES,
	UNIT_STATUS_CONDO_VACANT,
	UNIT_STATUS_CONDO_VACANT_EVENING,
} from "./states";

/**
 * End-of-day object-state floor pass.
 * Called after resetSimRuntimeState to normalize unit status bands:
 * - Occupied hotels (< 0x18) clamped to sync sentinel 0x10
 * - Sold condos (< 0x18) clamped to 0x10
 * - Non-occupied hotel bands toggle: 0x18 <-> 0x20, 0x28 <-> 0x30, 0x38 <-> 0x40
 * - Unsold condo bands toggle: 0x18 <-> 0x20
 */
export function normalizeUnitStatusEndOfDay(world: WorldState): void {
	for (const object of Object.values(world.placedObjects)) {
		if (HOTEL_FAMILIES.has(object.objectTypeCode)) {
			if (object.unitStatus < 0x18) {
				// Occupied: clamp to sync sentinel
				object.unitStatus = 0x10;
			} else if (object.unitStatus >= 0x18 && object.unitStatus < 0x28) {
				// Vacant band toggle: 0x18 <-> 0x20
				object.unitStatus = object.unitStatus < 0x20 ? 0x20 : 0x18;
			} else if (object.unitStatus >= 0x28 && object.unitStatus < 0x38) {
				// Checkout/turnover band toggle: 0x28 <-> 0x30
				object.unitStatus = object.unitStatus < 0x30 ? 0x30 : 0x28;
			} else if (object.unitStatus >= 0x38 && object.unitStatus <= 0x40) {
				// Extended band toggle: 0x38 <-> 0x40
				object.unitStatus = object.unitStatus < 0x40 ? 0x40 : 0x38;
			}
		} else if (object.objectTypeCode === FAMILY_CONDO) {
			if (object.unitStatus < 0x18) {
				// Sold: clamp to sync sentinel
				object.unitStatus = 0x10;
			} else {
				// Unsold band toggle: 0x18 <-> 0x20
				object.unitStatus =
					object.unitStatus < 0x20
						? UNIT_STATUS_CONDO_VACANT_EVENING
						: UNIT_STATUS_CONDO_VACANT;
			}
		}
	}
}

/**
 * Spread existing cockroach infestations to adjacent hotel rooms on the same floor.
 * Spec: runs at checkpoint 0x640 before the expiry check.
 */
export function spreadCockroachInfestation(
	world: WorldState,
	time: TimeState,
): void {
	const objectsByKey = Object.entries(world.placedObjects);
	for (const [key, object] of objectsByKey) {
		if (!HOTEL_FAMILIES.has(object.objectTypeCode)) continue;
		if (object.unitStatus < 0x38) continue;

		const [x, y] = key.split(",").map(Number);
		for (const [nKey, neighbor] of objectsByKey) {
			if (neighbor === object) continue;
			if (!HOTEL_FAMILIES.has(neighbor.objectTypeCode)) continue;
			const [nx, ny] = nKey.split(",").map(Number);
			if (ny !== y) continue;
			if (nx < x && neighbor.rightTileIndex >= object.leftTileIndex - 1) {
				infectHotelRoom(neighbor, time);
			}
			if (
				nx > x &&
				neighbor.leftTileIndex <= object.rightTileIndex + 1 &&
				neighbor.unitStatus < 0x38
			) {
				infectHotelRoom(neighbor, time);
			}
		}
	}
}

function infectHotelRoom(neighbor: PlacedObjectRecord, time: TimeState): void {
	neighbor.unitStatus = time.daypartIndex < 4 ? 0x40 : 0x38;
	neighbor.evalLevel = 0xff;
	neighbor.housekeepingClaimedFlag = 0;
}

/**
 * Three-strikes expiry: hotel rooms in checkout/turnover band without housekeeping
 * become infested after 3 consecutive checkpoint passes.
 * Spec: handle_extended_vacancy_expiry, runs at checkpoint 0x640.
 */
export function handleExtendedVacancyExpiry(
	world: WorldState,
	time: TimeState,
): void {
	for (const object of Object.values(world.placedObjects)) {
		if (!HOTEL_FAMILIES.has(object.objectTypeCode)) continue;
		if (object.unitStatus <= 0x27) continue;
		if (object.unitStatus >= 0x38) continue;

		if (object.housekeepingClaimedFlag !== 0) {
			object.evalLevel = 0xff;
			object.activationTickCount = 0;
			object.housekeepingClaimedFlag = 0;
		} else {
			object.activationTickCount += 1;
			if (object.activationTickCount >= 3) {
				object.unitStatus = time.daypartIndex < 4 ? 0x40 : 0x38;
			}
		}
	}
}

/**
 * Binary `advance_object_stay_phase_tiers` @ 1230:0b5f, invoked at checkpoint
 * 0x640 (dayTick 1600). Iterates all placed objects and raises unit_status
 * bands by +8 in specific residue classes. Critically, hotels at unit_status
 * 0x18 → 0x20 here triggers `refresh_occupied_flag_and_trip_counters` to
 * actually reset sim trip counters when the later midday pass runs.
 */
export function advanceObjectStayPhaseTiers(world: WorldState): void {
	for (const object of Object.values(world.placedObjects)) {
		const status = object.unitStatus;
		if (HOTEL_FAMILIES.has(object.objectTypeCode)) {
			if (status === 0x18) object.unitStatus = 0x20;
			else if (status === 0x28) object.unitStatus = 0x30;
			else if (status === 0x38) object.unitStatus = 0x40;
		} else if (object.objectTypeCode === FAMILY_OFFICE) {
			if (status === 0x00) object.unitStatus = 0x08;
			else if (status === 0x10) object.unitStatus = 0x18;
		} else if (object.objectTypeCode === FAMILY_CONDO) {
			if (status === 0x18) object.unitStatus = 0x20;
			else if ((status & 0xf8) === 0) object.unitStatus = (status & 7) + 0x08;
		}
	}
}

export function updateHotelOperationalAndOccupancy(
	world: WorldState,
	time: TimeState,
): void {
	for (const sim of world.sims) {
		const object = findObjectForSim(world, sim);
		if (!object || !HOTEL_FAMILIES.has(object.objectTypeCode)) continue;
		if (sim.baseOffset !== 0) continue;
		recomputeObjectOperationalStatus(world, sim, object);
	}
	handleExtendedVacancyExpiry(world, time);
	for (const sim of world.sims) {
		const object = findObjectForSim(world, sim);
		if (!object || !HOTEL_FAMILIES.has(object.objectTypeCode)) continue;
		if (sim.baseOffset !== 0) continue;
		// Binary update_hotel_operational_and_pairings pass 2: rooms with
		// unitStatus >= 0x28 (dormant / post-checkout) keep their current
		// occupiedFlag (+0x14). Fresh rooms run through the donor/flag update
		// path unconditionally — the refresh function handles the evalLevel
		// branches.
		if (object.unitStatus >= 0x28) continue;
		refreshOccupiedFlagAndTripCounters(world, sim, object);
	}
}
