// 11e0:02f7 add_delay_to_current_sim
//
// Adds a fixed tick penalty to the current-trip elapsed window. Binary:
//   elapsed = (elapsed_packed & 0x3ff) + delta
//   clamp to 300 (g_route_failure_delay)
//   store back, clear last_trip_tick
// Used for: no-route delay (300), distance penalties, queue-full waiting
// delay (5), stair/escalator per-stop delay. Housekeeping excluded.

import { FAMILY_HOUSEKEEPING } from "../resources";
import type { SimRecord } from "../world";

export function addDelayToCurrentSim(sim: SimRecord, delta: number): void {
	if (sim.familyCode === FAMILY_HOUSEKEEPING) return;
	sim.elapsedTicks = Math.min(300, sim.elapsedTicks + delta);
	sim.lastDemandTick = -1;
}
