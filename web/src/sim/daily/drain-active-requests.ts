// 1190:0977 dispatch_active_requests_by_family
//
// Fires once per day at the 0x9c4 checkpoint. The binary iterates
// `g_active_request_table` and re-dispatches each stuck request through
// its family handler. TS analog: iterate sims whose `stateCode` has the
// 0x40 in-transit bit set and route each through `dispatchSimBehavior`
// (1228:186c), which forwards in-transit sims into
// `maybeDispatchQueuedRouteAfterWait` so the route-failure timeout can
// fire.
//
// Runs once per day; does NOT run every tick.

import { dispatchSimBehavior } from "../families/dispatch-sim-behavior";
import type { LedgerState } from "../ledger";
import { isSimInTransit } from "../sim-access/state-bits";
import type { TimeState } from "../time";
import type { WorldState } from "../world";

export function dispatchActiveRequestsByFamily(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
): void {
	for (const sim of world.sims) {
		if (!isSimInTransit(sim.stateCode)) continue;
		dispatchSimBehavior(world, ledger, time, sim);
	}
}
