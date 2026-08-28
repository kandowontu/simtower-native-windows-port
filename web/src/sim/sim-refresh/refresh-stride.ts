// 1228:0d64 refresh_runtime_entities_for_tick_stride
//
// 1-of-16 stride over `g_sim_table`, dispatching per-family handlers for each
// active sim. Currently re-exports the Phase-1 implementation that still lives
// in `sims/index.ts` (`advanceSimRefreshStride`). Phase 5b will migrate the
// implementation into this file and route directly to the new families/
// dispatchers.
export {
	advanceSimRefreshStride,
	refreshRuntimeEntitiesForTickStride,
} from "../sims";
