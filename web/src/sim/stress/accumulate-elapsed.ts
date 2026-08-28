// 11e0:01f1 accumulate_elapsed_delay_into_current_sim
//
// Called from `assign_request_to_runtime_route` (1218:0d4e) when a sim
// boards a non-service carrier. Binary body:
//   elapsed = (elapsed_packed & 0x3ff) + g_day_tick - last_trip_tick
//   scale_delay_for_speed_mode(elapsed, source_floor)  // lobby reduction
//   clamp to 300, store back, clear last_trip_tick
//
// Current TS status: the boarding path in `queue/process-travel.ts`
// (`applyBoardingStressUpdate`) composes this behavior out of
// `rebaseSimElapsedFromClock` + `reduceElapsedForLobbyBoarding` rather
// than calling a single `accumulateElapsedDelayIntoCurrentSim` primitive.
// This wrapper exposes the binary-named entry point so future callers
// (e.g. a faithful `assignRequestToRuntimeRoute` port) can invoke it
// directly.

import type { TimeState } from "../time";
import type { SimRecord, WorldState } from "../world";
import { reduceElapsedForLobbyBoarding } from "./lobby-reduction";
import { rebaseSimElapsedFromClock } from "./rebase-elapsed";

export function accumulateElapsedDelayIntoCurrentSim(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
	sourceFloor: number,
): void {
	rebaseSimElapsedFromClock(sim, time);
	reduceElapsedForLobbyBoarding(sim, sourceFloor, world);
}
