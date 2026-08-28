// Trip-counter facade. The binary's stress accessors (11e0:*) moved to
// `sim/stress/*.ts` in Phase 8; this module keeps the family-scoped reset
// helpers that have no direct 11e0 counterpart (they live outside the
// stress subsystem) and re-exports the moved accessors so legacy call
// sites compile unchanged.

import {
	FAMILY_HOTEL_SINGLE,
	FAMILY_HOTEL_SUITE,
	FAMILY_HOTEL_TWIN,
} from "../resources";
import type { SimRecord, WorldState } from "../world";
import { findSiblingSims } from "./population";

export { addDelayToCurrentSim } from "../stress/add-delay";
export { rebaseSimElapsedFromClock } from "../stress/rebase-elapsed";
export { advanceSimTripCounters } from "../stress/trip-counters";

/** Clear trip counters. Spec: reset_sim_trip_counters. */
export function resetSimTripCounters(sim: SimRecord): void {
	sim.tripCount = 0;
	sim.accumulatedTicks = 0;
}

// Binary reset_facility_sim_trip_counters @ 1138:0d07: iterates occupants of a
// facility via compute_object_occupant_runtime_index(floor, slot, startOccupant).
// startOccupant=1 for hotel families (3/4/5) — i.e. the baseOffset=0 occupant
// is intentionally skipped. For all other families startOccupant=0 (full sweep).
const HOTEL_FAMILY_CODES = new Set<number>([
	FAMILY_HOTEL_SINGLE,
	FAMILY_HOTEL_TWIN,
	FAMILY_HOTEL_SUITE,
]);

export function resetFacilitySimTripCounters(
	world: WorldState,
	sim: SimRecord,
): void {
	const skipPrimary = HOTEL_FAMILY_CODES.has(sim.familyCode);
	for (const sibling of findSiblingSims(world, sim)) {
		if (skipPrimary && sibling.baseOffset === 0) continue;
		resetSimTripCounters(sibling);
	}
}
