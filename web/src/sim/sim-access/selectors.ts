// 1228:681d..688c + 1228:6700 + 1228:65c1 + 1228:6757 + 1228:662a + 1228:640c + 1228:67d7
//
// Binary sim-level selector / accessor helpers.

import { activateObjectFamilyHousekeepingConnectionState } from "../families/housekeeping";
import type { SimRecord, WorldState } from "../world";

/** 1228:681d get_current_sim_type — byte at sim+4 (family_code). */
export function getCurrentSimType(sim: SimRecord): number {
	return sim.familyCode;
}

/** 1228:6854 get_current_sim_variant — byte at sim+1 (object_subtype_index). */
export function getCurrentSimVariant(sim: SimRecord): number {
	return sim.facilitySlot;
}

/** 1228:688c get_current_sim_state_word — word at sim+2 (object_base_offset in
 *  the binary; TS uses `baseOffset` in the general case). */
export function getCurrentSimStateWord(sim: SimRecord): number {
	return sim.baseOffset;
}

/** 1228:6700 resolve_family_parking_selector_value. */
export function resolveFamilyParkingSelectorValue(
	_world: WorldState,
	sim: SimRecord,
): number {
	switch (sim.stateCode) {
		case 0x45:
			return 10;
		case 0x60:
			// Binary quirk: 0x6d = 109, outbound cathedral zone constant.
			return 0x6d;
		default:
			return -1;
	}
}

/** 1228:65c1 resolve_family_recycling_center_lower_selector_value. */
export function resolveFamilyRecyclingCenterLowerSelectorValue(
	_world: WorldState,
	sim: SimRecord,
): number {
	switch (sim.stateCode) {
		case 0x41:
			return sim.destinationFloor;
		case 0x62:
			return getCurrentSimType(sim) + 2;
		default:
			return -1;
	}
}

/** 1228:6757 get_housekeeping_room_claim_selector. */
export function getHousekeepingRoomClaimSelector(
	_world: WorldState,
	sim: SimRecord,
): number {
	switch (sim.stateCode) {
		case 0x03:
			return sim.selectedFloor;
		case 0x04:
			return getCurrentSimType(sim);
		default:
			return -1;
	}
}

/**
 * 1228:662a dispatch_entertainment_guest_substate.
 *
 * Compact 4-entry jump table at cs:0x66f0 for the entertainment guest
 * in-transit substates {0x41, 0x45, 0x60, 0x62}:
 *   0x41 → CALLF 0x11b0:10fe (external advance_entertainment_attendance; TS: increment attendanceCounter)
 *   0x45 → return 0xa (no-op return value 10)
 *   0x60 → call get_current_sim_type + get_current_sim_variant, look up phase table, CALLF 0x1188:0d98
 *   0x62 → return 0xa (same as 0x45)
 *
 * Binary quirk: states 0x45 and 0x62 return the constant 0xa (10) with no
 * side effects. State 0x41 calls an external attendance-advancement routine.
 * State 0x60 performs a type+variant table lookup and calls a phase handler.
 * In TS, only the attendance increment for state 0x41 has an observable
 * effect; the other substates are no-ops at the current abstraction level.
 */
export function dispatchEntertainmentGuestSubstate(
	world: WorldState,
	sim: SimRecord,
): void {
	const state = sim.stateCode;

	if (state === 0x41) {
		// Binary quirk: CALLF 0x11b0:10fe advances the entertainment attendance
		// counter for the active link record. In TS, find the link and increment.
		for (const sidecar of world.sidecars) {
			if (
				sidecar.kind === "entertainment_link" &&
				sidecar.ownerSubtypeIndex === sim.homeColumn
			) {
				sidecar.attendanceCounter += 1;
				break;
			}
		}
	}
	// States 0x45 and 0x62: binary returns 0xa with no side effects.
	// State 0x60: binary looks up a phase table via type+variant and calls a
	// phase handler; no TS-visible state mutation at current abstraction level.
}

/**
 * 1228:640c maybe_start_housekeeping_room_claim.
 *
 * Gets the occupant runtime index for the sim's object. If the target sim
 * has stateCode == 0 and the star rating satisfies (starCount & 9) != 0,
 * activates the housekeeping connection state for that sim.
 * Binary quirk: (starCount & 9) != 0 covers stars 1 (bit 0) and 3+ (bit 3+
 * alias); in practice this gates housekeeping on tower rank.
 */
export function maybeStartHousekeepingRoomClaim(
	world: WorldState,
	sim: SimRecord,
): boolean {
	const targetIndex = computeObjectOccupantRuntimeIndex(world, sim);
	if (targetIndex < 0 || targetIndex >= world.sims.length) return false;
	const targetSim = world.sims[targetIndex];
	if (!targetSim) return false;
	if (targetSim.stateCode !== 0) return false;
	// Binary quirk: (starRatingFlags & 9) != 0 — bit 0 = 1-star, bit 3 = 8+
	// but starCount is 1-indexed so we test the raw value bitwise.
	if ((world.starCount & 9) === 0) return false;
	activateObjectFamilyHousekeepingConnectionState(
		world,
		// activateObjectFamilyHousekeepingConnectionState requires a TimeState,
		// but maybeStartHousekeepingRoomClaim is called from a context where
		// time is not available in this binary path; pass a minimal sentinel.
		// Binary quirk: the binary passes the global time pointer directly.
		null as never,
		targetSim,
		sim.selectedFloor,
	);
	return true;
}

/**
 * 1228:67d7 compute_object_occupant_runtime_index.
 *
 * Returns the index in world.sims of the occupant slot identified by
 * (sim.floorAnchor, sim.facilitySlot, sim.baseOffset). The binary walks
 * g_unknown_ptr_array[floor].ptr facility structs to read the stored base
 * sim index for the per-floor object, then adds the occupant offset. In TS,
 * the equivalent is: find the index of the first sim sharing (floorAnchor,
 * facilitySlot) and add baseOffset.
 */
export function computeObjectOccupantRuntimeIndex(
	world: WorldState,
	sim: SimRecord,
): number {
	const { floorAnchor, facilitySlot, baseOffset } = sim;
	for (let i = 0; i < world.sims.length; i++) {
		const s = world.sims[i];
		if (
			s.floorAnchor === floorAnchor &&
			s.facilitySlot === facilitySlot &&
			s.baseOffset === 0
		) {
			return i + baseOffset;
		}
	}
	return -1;
}
