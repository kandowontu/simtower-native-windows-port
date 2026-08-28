// 1228:3ed9 gate_object_family_retail_state_handler (family 10)
// 1228:40c0 dispatch_object_family_retail_state_handler (family 10)
//
// Retail state machine (family 10). Phase 5b: the gate splits retail off
// from the shared commercial processor in `sims/commercial.ts`. The
// dispatch body (state-0x20 route to home, state-0x05 depart, etc.)
// currently remains in processCommercialSim, which internally gates on
// `sim.familyCode === FAMILY_RETAIL` to apply retail-specific quirks
// (occupiable-flag early exit, no silent-park on VENUE_CLOSED, etc.).
//
// TODO: binary 1228:3ed9 — the full retail gate/dispatch bodies remain
// intertwined with restaurant/fast-food inside processCommercialSim. A
// later phase can fully split them; for Phase 5b we preserve the
// binary-aligned entry-point shape here.

import type { LedgerState } from "../ledger";
import { FAMILY_RETAIL } from "../resources";
import { isSimInTransit } from "../sim-access/state-bits";
import {
	handleCommercialSimArrival as _handleCommercialSimArrival,
	processCommercialSim as _processCommercialSim,
} from "../sims/commercial";
import type { TimeState } from "../time";
import type { SimRecord, WorldState } from "../world";
import { maybeDispatchQueuedRouteAfterWait } from "./maybe-dispatch-after-wait";

export {
	handleCommercialSimArrival,
	processCommercialSim,
} from "../sims/commercial";

/**
 * 1228:3ed9 gate_object_family_retail_state_handler. Gates on
 * familyCode === FAMILY_RETAIL and delegates to the shared commercial
 * processor; retail-specific early exit (DORMANT+occupiedFlag, binary +0x14)
 * is already encoded inside processCommercialSim.
 */
export function gateObjectFamilyRetailStateHandler(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
): void {
	if (sim.familyCode !== FAMILY_RETAIL) return;
	if (isSimInTransit(sim.stateCode)) {
		// Commercial families (6/10/0xc) do NOT dispatch through
		// dispatch_sim_behavior in the binary; the queued-wait timeout is
		// not wired for these families. Callers that reach here for a
		// transit sim are expected to be the arrival path.
		maybeDispatchQueuedRouteAfterWait(world, ledger, time, sim);
		return;
	}
	_processCommercialSim(world, ledger, time, sim);
}

/**
 * 1228:40c0 dispatch_object_family_retail_state_handler. Arrival entry
 * point; delegates to handleCommercialSimArrival.
 */
export function dispatchObjectFamilyRetailStateHandler(
	world: WorldState,
	sim: SimRecord,
	arrivalFloor: number,
	time: TimeState,
): void {
	_handleCommercialSimArrival(world, sim, arrivalFloor, time);
}
