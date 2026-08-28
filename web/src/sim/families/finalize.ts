// 1228:1481 finalize_runtime_route_state
//
// Fired on route completion / cancellation: clears the state-code mode
// bits (0x40 in-transit, 0x20 waiting), resets the route bookkeeping
// struct, rebases elapsed time against the current clock, and advances
// trip counters. The binary wires this from the queue cancel path
// (cancel_runtime_route_request) and the arrival dispatch path.
//
// Phase 5b note: the TS state constants encode the 0x20 waiting bit as
// part of the phase byte for states in the 0x20..0x27 range (see
// families/force-dispatch.ts). finalizeRuntimeRouteState is therefore
// careful NOT to strip 0x20 — it only strips 0x40 (the true in-transit
// bit). Callers that wish to park a sim to a specific low-phase state
// should overwrite sim.stateCode explicitly.

import {
	isSimInTransit,
	isSimWaiting,
	setSimInTransit,
} from "../sim-access/state-bits";
import { clearSimRoute } from "../sims/population";
import { advanceSimTripCounters } from "../stress/trip-counters";
import type { SimRecord, WorldState } from "../world";

export function finalizeRuntimeRouteState(
	_world: WorldState,
	sim: SimRecord,
): void {
	// Binary: advance trip counters iff the sim was actually on a route.
	// Bit test rather than struct because state_code is authoritative
	// (per ROUTING-BINARY-MAP.md §4.1).
	const wasActive =
		isSimInTransit(sim.stateCode) ||
		isSimWaiting(sim.stateCode) ||
		sim.route.mode !== "idle";
	if (wasActive) advanceSimTripCounters(sim);
	clearSimRoute(sim);
	// Strip the 0x40 in-transit bit (see file-level comment for why we
	// do NOT strip 0x20 — that bit overlaps TS phase encodings).
	setSimInTransit(sim, false);
	sim.transitTicksRemaining = 0;
}
