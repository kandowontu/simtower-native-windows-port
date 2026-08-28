// 1228:15a0 maybe_dispatch_queued_route_after_wait
//
// Fires when a queued sim has waited past the route-failure delay. The binary
// reaches this path via `refresh_object_family_office_state_handler` (1228:1cb5)
// AND `refresh_object_family_hotel_state_handler` (1228:2aec) when
// `state_code >= 0x40` AND `encoded_route_target >= 0x40` — i.e. the sim is
// still sitting on a carrier queue, not a segment. When elapsed ticks since
// enqueue exceed 300 (`g_route_failure_delay`), the dispatch is force-driven
// into the family's per-state timeout handler:
//   office: 1228:193d → sim[+5] = 0x26 (NIGHT_B), evict route
//   hotel:  1228:19f4 → sim[+5] = 0x26 (NIGHT_B), evict route, fail service eval
// Both share the same NIGHT_B + eviction effect; hotel additionally calls
// fail_office_service_evaluation (1248:017d) — not yet modeled.

import { floorToSlot } from "../carriers/slot";
import { syncAssignmentStatus } from "../carriers/sync";
import type { LedgerState } from "../ledger";
import { popUnitQueueRequest } from "../queue/dequeue";
import {
	FAMILY_FAST_FOOD,
	FAMILY_HOTEL_SINGLE,
	FAMILY_HOTEL_SUITE,
	FAMILY_HOTEL_TWIN,
	FAMILY_OFFICE,
	FAMILY_RESTAURANT,
	FAMILY_RETAIL,
} from "../resources";
import { isSimInTransit } from "../sim-access/state-bits";
import { releaseServiceRequest } from "../sims/index";
import { advanceOfficePresenceCounter } from "../sims/office";
import { clearSimRoute, findObjectForSim, simKey } from "../sims/population";
import {
	STATE_ACTIVE_TRANSIT,
	STATE_AT_WORK_TRANSIT,
	STATE_COMMUTE_TRANSIT,
	STATE_DEPARTURE,
	STATE_DEPARTURE_TRANSIT,
	STATE_DWELL_RETURN_TRANSIT,
	STATE_MORNING_TRANSIT,
	STATE_NIGHT_B,
	STATE_PARKED,
	STATE_VENUE_HOME_TRANSIT,
	STATE_VENUE_TRIP_TRANSIT,
} from "../sims/states";
import { rebaseSimElapsedFromClock } from "../stress/rebase-elapsed";
import { advanceSimTripCounters } from "../stress/trip-counters";
import { DAY_TICK_MAX, type TimeState } from "../time";
import type { SimRecord, WorldState } from "../world";

// Binary g_route_delay_table_base @ 1288:e5ee. Loaded by
// load_startup_tuning_resource_table (1198:0005) from PART #1000 word 0.
import { ORIGINAL_TUNING } from "../original-tuning";

const ROUTE_WAIT_TIMEOUT_TICKS = ORIGINAL_TUNING.routing.waitTimeoutTicks;

// Binary family-7 dispatch_sim_behavior jumptable at 1228:1c51. Entries for
// states {0x45, 0x60, 0x61, 0x62, 0x63} all point to handler 1228:193d, which
// unconditionally writes sim[+5] = 0x26 (NIGHT_B). Entries for {0x40, 0x41,
// 0x42} point to handler 1228:1989, which writes sim[+5] = 0x05 (DEPARTURE)
// after calling advance_stay_phase_or_wrap (1228:68c3). The 0x40/0x41/0x42
// timeout path is reached for FULL-stuck sims (sim+8 = 0xfe queue-full
// marker) once the 300-tick wait threshold passes.
const OFFICE_WAIT_TIMEOUT_TO_NIGHT_B_STATES = new Set<number>([
	STATE_DEPARTURE_TRANSIT,
	STATE_MORNING_TRANSIT,
	STATE_AT_WORK_TRANSIT,
	STATE_VENUE_HOME_TRANSIT,
	STATE_DWELL_RETURN_TRANSIT,
]);

// Binary family-3/4/5 dispatch_sim_behavior jumptable at cs:1c41. Only state
// 0x45 → 1228:19f4 writes NIGHT_B; entries for {0x41, 0x60, 0x62} point to
// 1228:1a4f (different handler — not yet decoded).
const HOTEL_WAIT_TIMEOUT_TO_NIGHT_B_STATES = new Set<number>([
	STATE_DEPARTURE_TRANSIT,
]);

const HOTEL_FAMILY_CODES = new Set<number>([
	FAMILY_HOTEL_SINGLE,
	FAMILY_HOTEL_TWIN,
	FAMILY_HOTEL_SUITE,
]);

// Binary `gate_object_family_restaurant_fast_food_state_handler` (1228:466d)
// calls `maybe_dispatch_queued_route_after_wait` (1228:15a0) for sims with
// state_code >= 0x40 AND sim+8 (encoded_route_target) >= 0x40 — same pattern
// as office/hotel. The retail/restaurant/fast-food state-0x45 jump table at
// 1228:4d37 maps rc=-1 AND rc=3 to 1228:4c9e, which writes sim+5 = 0x27
// (PARKED). So on timeout, the commercial state-0x45 path becomes PARKED,
// not NIGHT_B.
const COMMERCIAL_FAMILY_CODES = new Set<number>([
	FAMILY_RESTAURANT,
	FAMILY_RETAIL,
	FAMILY_FAST_FOOD,
]);

const COMMERCIAL_WAIT_TIMEOUT_TO_PARKED_STATES = new Set<number>([
	STATE_DEPARTURE_TRANSIT,
]);

function dispatchTimedOutOfficeQueueEntry(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	rebaseSimElapsedFromClock(sim, time);
	advanceSimTripCounters(sim);

	if (
		sim.stateCode === STATE_COMMUTE_TRANSIT ||
		sim.stateCode === STATE_ACTIVE_TRANSIT ||
		sim.stateCode === STATE_VENUE_TRIP_TRANSIT
	) {
		const object = findObjectForSim(world, sim);
		if (object) advanceOfficePresenceCounter(object);
		sim.selectedFloor = sim.floorAnchor;
		sim.destinationFloor = -1;
		sim.stateCode = STATE_DEPARTURE;
		clearSimRoute(sim);
		return;
	}

	if (OFFICE_WAIT_TIMEOUT_TO_NIGHT_B_STATES.has(sim.stateCode)) {
		sim.stateCode = STATE_NIGHT_B;
	}
	sim.destinationFloor = -1;
	clearSimRoute(sim);
}

function dispatchTimedOutHotelQueueEntry(
	_world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	// Mirror dispatch_sim_behavior (1228:186c) prologue for non-housekeeping
	// families before the per-state handler fires.
	rebaseSimElapsedFromClock(sim, time);
	advanceSimTripCounters(sim);
	if (HOTEL_WAIT_TIMEOUT_TO_NIGHT_B_STATES.has(sim.stateCode)) {
		sim.stateCode = STATE_NIGHT_B;
	}
	sim.destinationFloor = -1;
	clearSimRoute(sim);
}

function dispatchTimedOutCommercialQueueEntry(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	// Binary 1228:186c dispatch_sim_behavior prologue.
	rebaseSimElapsedFromClock(sim, time);
	advanceSimTripCounters(sim);
	// Binary 1228:4c9e: state-0x45 rc=-1/rc=3 writer for restaurant/retail/
	// fast-food — transitions to PARKED (0x27) and releases the venue slot.
	if (COMMERCIAL_WAIT_TIMEOUT_TO_PARKED_STATES.has(sim.stateCode)) {
		sim.stateCode = STATE_PARKED;
		releaseServiceRequest(world, sim);
	}
	sim.destinationFloor = -1;
	clearSimRoute(sim);
}

function dispatchQueuedRouteUntilRequest(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	const routeState = sim.route;
	if (routeState.mode !== "carrier") {
		if (sim.familyCode === FAMILY_OFFICE) {
			dispatchTimedOutOfficeQueueEntry(world, time, sim);
		} else if (HOTEL_FAMILY_CODES.has(sim.familyCode)) {
			dispatchTimedOutHotelQueueEntry(world, time, sim);
		} else if (COMMERCIAL_FAMILY_CODES.has(sim.familyCode)) {
			dispatchTimedOutCommercialQueueEntry(world, time, sim);
		}
		return;
	}
	const carrier = world.carriers.find(
		(c) => c.carrierId === routeState.carrierId,
	);
	if (!carrier) return;
	const routeId = simKey(sim);
	const route = carrier.pendingRoutes.find(
		(candidate) => candidate.simId === routeId,
	);
	if (!route || route.boarded) return;
	const slot = floorToSlot(carrier, route.sourceFloor);
	if (slot < 0) return;
	const queue = carrier.floorQueues[slot];
	if (!queue) return;

	while (true) {
		const poppedRouteId = popUnitQueueRequest(queue, route.directionFlag);
		if (!poppedRouteId) break;
		carrier.pendingRoutes = carrier.pendingRoutes.filter(
			(candidate) => candidate.simId !== poppedRouteId,
		);
		const poppedSim = world.sims.find(
			(candidate) => simKey(candidate) === poppedRouteId,
		);
		if (poppedSim?.familyCode === FAMILY_OFFICE) {
			dispatchTimedOutOfficeQueueEntry(world, time, poppedSim);
		} else if (poppedSim && HOTEL_FAMILY_CODES.has(poppedSim.familyCode)) {
			dispatchTimedOutHotelQueueEntry(world, time, poppedSim);
		} else if (poppedSim && COMMERCIAL_FAMILY_CODES.has(poppedSim.familyCode)) {
			dispatchTimedOutCommercialQueueEntry(world, time, poppedSim);
		}
		if (poppedRouteId === routeId) break;
	}
	syncAssignmentStatus(carrier);
}

export function maybeDispatchQueuedRouteAfterWait(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
): void {
	// Binary gate: only fires for sims marked in-transit on a carrier queue.
	// The 0x40 bit check is the state_code half; the sim.route.mode === "carrier"
	// check is the encoded_route_target >= 0x40 half.
	if (!isSimInTransit(sim.stateCode)) return;
	if (sim.route.mode !== "carrier") return;
	if (sim.familyCode === FAMILY_OFFICE) {
		// Binary 1228:15a0 itself has no office-state filter; the caller reaches
		// it for queued office transit aliases 0x40/0x41/0x42/0x45/0x60..0x63.
		// The per-state dispatch result is decided after the queue pop.
	} else if (HOTEL_FAMILY_CODES.has(sim.familyCode)) {
		if (!HOTEL_WAIT_TIMEOUT_TO_NIGHT_B_STATES.has(sim.stateCode)) return;
	} else if (COMMERCIAL_FAMILY_CODES.has(sim.familyCode)) {
		// Binary gate_object_family_restaurant_fast_food_state_handler
		// (1228:466d) reaches maybe_dispatch_queued_route_after_wait for
		// state_code >= 0x40 (the in-transit aliases). state 0x45 is the
		// fast-food / retail / restaurant DEPARTURE_TRANSIT.
		if (!COMMERCIAL_WAIT_TIMEOUT_TO_PARKED_STATES.has(sim.stateCode)) return;
	} else {
		return;
	}
	if (sim.lastDemandTick < 0) return;
	// Day-tick wraps 0..DAY_TICK_MAX-1; the binary uses 16-bit unsigned
	// subtraction so a stamp from before rollover compares correctly.
	const elapsed =
		(time.dayTick - sim.lastDemandTick + DAY_TICK_MAX) % DAY_TICK_MAX;
	if (elapsed <= ROUTE_WAIT_TIMEOUT_TICKS) return;
	const carrierId = sim.route.carrierId;
	const carrier = world.carriers.find((c) => c.carrierId === carrierId);
	if (!carrier) return;
	// Binary: only fires for sims still waiting in the floor queue — once the
	// carrier picks them up, the arrival handler transitions state naturally.
	const route = carrier.pendingRoutes.find((r) => r.simId === simKey(sim));
	if (!route || route.boarded) return;
	// Binary 1228:15a0 unconditionally calls dispatch_queued_route_until_request
	// (1218:1981) for both office and hotel families: pop the floor queue in
	// FIFO order and dispatch each popped sim through its family handler until
	// the requesting sim itself is popped. Without this drain the floor queue
	// keeps stale entries even though `pendingRoutes`/`sim.route` were cleared,
	// which causes the next dwell-1 drain to over-board the late-comers (e.g.
	// dense_hotel d1 t398 carrier@84 over-boarded 7 sims that the binary had
	// already cancelled via this timeout path).
	dispatchQueuedRouteUntilRequest(world, time, sim);
}
