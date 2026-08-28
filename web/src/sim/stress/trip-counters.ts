// 11e0:0000 advance_sim_trip_counters
//
// Captures a completed trip into the sim's trip counters and resets the
// per-trip elapsed window. Binary: trip_count += 1, accumulated_elapsed +=
// (elapsed_packed & 0x3ff), clear low 10 bits of elapsed_packed, clear
// last_trip_tick. Housekeeping (family 0x0f) is excluded — the binary
// gates stress tracking on dispatch-family membership and HK never hits
// the path.

import { FAMILY_HOUSEKEEPING } from "../resources";
import type { SimRecord } from "../world";

export function advanceSimTripCounters(sim: SimRecord): void {
	if (sim.familyCode === FAMILY_HOUSEKEEPING) return;
	sim.tripCount += 1;
	sim.accumulatedTicks += sim.elapsedTicks;
	sim.elapsedTicks = 0;
	sim.lastDemandTick = -1;
}
