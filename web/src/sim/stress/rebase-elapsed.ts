// 11e0:00fc rebase_sim_elapsed_from_clock
//
// Syncs per-trip elapsed ticks with the day clock. Binary:
//   elapsed = (elapsed_packed & 0x3ff) + g_day_tick - last_trip_tick
//   clamp to 300 (g_route_failure_delay)
//   elapsed_packed = (elapsed_packed & 0xfc00) | elapsed
//   last_trip_tick = 0
// The `lastDemandTick >= 0` sentinel reflects the binary's "no timestamp
// set" convention: dayTick == 0 is valid, so -1 is the cleared marker.
// Housekeeping (family 0x0f) is excluded from stress tracking.

import { FAMILY_HOUSEKEEPING } from "../resources";
import type { TimeState } from "../time";
import type { SimRecord } from "../world";

export function rebaseSimElapsedFromClock(
	sim: SimRecord,
	time: TimeState,
): void {
	if (sim.familyCode === FAMILY_HOUSEKEEPING) return;
	if (sim.lastDemandTick >= 0) {
		sim.elapsedTicks = Math.min(
			300,
			sim.elapsedTicks + time.dayTick - sim.lastDemandTick,
		);
	}
	sim.lastDemandTick = -1;
}
