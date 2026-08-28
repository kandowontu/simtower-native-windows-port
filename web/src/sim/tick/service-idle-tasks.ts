// 1268:01a6 service_idle_tasks
//
// Win16 idle pass: between messages the binary calls
// `run_simulation_day_scheduler` followed by `carrier_tick`. This function
// mirrors that orchestration and serves as the entry point for
// `TowerSim.step()` (which, in Phase 1, is a thin wrapper plus UI-facing
// cell-patch + notification draining).
//
// Phase 7: the `onArrival` / `onBoarding` callback plumbing has been
// removed — family dispatch and stress accumulation happen inline inside
// the queue path, matching the binary's call graph.
import type { LedgerState } from "../ledger";
import type { TimeState } from "../time";
import type { WorldState } from "../world";
import { carrierTick } from "./carrier-tick";
import { runSimulationDayScheduler } from "./day-scheduler";

/**
 * Mutable context passed into `serviceIdleTasks`. The function updates
 * `ctx.time` in place after the day scheduler advances the tick so the
 * inline arrival/boarding paths observe the advanced time (matching the
 * binary, which updates `g_day_tick` before `carrier_tick` runs).
 */
export interface ServiceIdleTasksContext {
	world: WorldState;
	ledger: LedgerState;
	time: TimeState;
}

export function serviceIdleTasks(ctx: ServiceIdleTasksContext): void {
	ctx.time = runSimulationDayScheduler(ctx.world, ctx.ledger, ctx.time);
	carrierTick(ctx.world, ctx.ledger, ctx.time);
}
