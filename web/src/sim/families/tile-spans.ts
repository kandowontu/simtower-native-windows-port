// 1228:0fc2 rebuild_all_sim_tile_spans
// 1228:1018 update_sim_tile_span
//
// Binary iterates every (floor, object-type) slot in the global object table
// and finalizes any stuck in-transit sims. In TS, sims are a flat array; we
// map the binary's per-slot loop onto a flat world.sims sweep.
//
// update_sim_tile_span(floor, type, apply_updates=0):
//   - Most families: finalize if stateCode & 0x40 (in-transit).
//   - Family 7 (office): also release service request (unconditional).
//   - Family 15 (housekeeping): finalize if stateCode > 2.
//   apply_updates=1 is only used by refresh_entertainment_link_tile_spans
//   (an edit-time path) and is not modeled in the daily-sweep call.

import { FAMILY_HOUSEKEEPING, FAMILY_OFFICE } from "../resources";
import { isSimInTransit } from "../sim-access/state-bits";
import { releaseServiceRequest } from "../sims";
import type { SimRecord, WorldState } from "../world";
import { finalizeRuntimeRouteState } from "./finalize";

export function updateSimTileSpan(world: WorldState, sim: SimRecord): void {
	if (sim.familyCode === FAMILY_HOUSEKEEPING) {
		if (sim.stateCode > 2) {
			finalizeRuntimeRouteState(world, sim);
		}
	} else {
		if (isSimInTransit(sim.stateCode)) {
			finalizeRuntimeRouteState(world, sim);
		}
		if (sim.familyCode === FAMILY_OFFICE) {
			releaseServiceRequest(world, sim);
		}
	}
}

export function rebuildAllSimTileSpans(world: WorldState): void {
	for (const sim of world.sims) {
		updateSimTileSpan(world, sim);
	}
}
