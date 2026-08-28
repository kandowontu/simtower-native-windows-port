import { addCashflowFromFamilyResource, type LedgerState } from "../ledger";
import { addToPopulationBucket } from "../progression";
import { FAMILY_FAST_FOOD, FAMILY_OFFICE } from "../resources";
import type { TimeState } from "../time";
import {
	type PlacedObjectRecord,
	type SimRecord,
	sampleRng,
	type WorldState,
} from "../world";
import {
	advanceSimTripCounters,
	clearSimRoute,
	dispatchCommercialVenueVisit,
	findCommercialVenueAtFloor,
	findObjectForSim,
	releaseOfficeVenueSlot,
	releaseServiceRequest,
	resetFacilitySimTripCounters,
	resolveSimRouteBetweenFloors,
	tryAcquireOfficeVenueSlot,
	tryAssignParkingService,
	VENUE_SLOT_FULL,
} from "./index";
import { tryStartMedicalTrip } from "./medical";
import {
	COMMERCIAL_DWELL_STATE,
	COMMERCIAL_VENUE_DWELL_TICKS,
	LOBBY_FLOOR,
	NO_EVAL_ENTITY,
	STATE_ACTIVE,
	STATE_ACTIVE_ALT,
	STATE_ACTIVE_TRANSIT,
	STATE_AT_WORK,
	STATE_AT_WORK_TRANSIT,
	STATE_COMMUTE,
	STATE_COMMUTE_TRANSIT,
	STATE_DEPARTURE,
	STATE_DEPARTURE_TRANSIT,
	STATE_DWELL_RETURN_TRANSIT,
	STATE_MORNING_GATE,
	STATE_MORNING_TRANSIT,
	STATE_NIGHT_A,
	STATE_NIGHT_B,
	STATE_PARKED,
	STATE_VENUE_HOME_TRANSIT,
	STATE_VENUE_TRIP,
	STATE_VENUE_TRIP_TRANSIT,
	UNIT_STATUS_OFFICE_OCCUPIED,
} from "./states";

export function advanceOfficePresenceCounter(object: PlacedObjectRecord): void {
	if (object.objectTypeCode !== FAMILY_OFFICE) return;
	if (object.unitStatus > UNIT_STATUS_OFFICE_OCCUPIED) return;
	object.unitStatus = object.unitStatus >= 8 ? 1 : object.unitStatus + 1;
}

function decrementOfficePresenceCounter(
	object: PlacedObjectRecord,
	time: TimeState,
): void {
	if (object.objectTypeCode !== FAMILY_OFFICE) return;
	if (object.unitStatus > UNIT_STATUS_OFFICE_OCCUPIED) return;
	object.unitStatus = Math.max(0, object.unitStatus - 1);
	if (object.unitStatus === 0 && time.daypartIndex >= 4) {
		object.unitStatus = 8;
	}
}

/**
 * Mirrors binary `activate_office_cashflow` (1180:0d2e) for the
 * `is_reopening==0` path called from the per-tick state-0x20 handler at
 * 1228:2329 / 1228:23a5. The cashflow side (`add_cashflow_from_family_resource`)
 * is owned in TS by the 3-day rollover guard in `handleOfficeMorningGate`,
 * so this routine only flips `unitStatus`/`dirtyFlag` and contributes
 * +6 to the primary family ledger total used by star advancement.
 */
function activateOfficeCashflow(
	world: WorldState,
	object: PlacedObjectRecord,
	sim: SimRecord,
): void {
	if (object.unitStatus <= UNIT_STATUS_OFFICE_OCCUPIED) return;
	object.unitStatus = 0;
	// Binary 1180:0d93: MOV byte ptr ES:[BX+0x13],1 (dirty flag set).
	object.dirtyFlag = 1;
	// Binary: add_to_primary_family_ledger_bucket(7, 6) — population
	// contribution that feeds compute_tower_tier_from_ledger and gates
	// star advancement (e.g. 1→2 at total >= 300).
	addToPopulationBucket(world, FAMILY_OFFICE, 6);

	resetFacilitySimTripCounters(world, sim);
}

function routeFailureStateForOffice(object: PlacedObjectRecord): number {
	return object.unitStatus > UNIT_STATUS_OFFICE_OCCUPIED
		? STATE_MORNING_GATE
		: STATE_NIGHT_A;
}

function failOfficeRoute(
	world: WorldState,
	sim: SimRecord,
	failureState: number,
): void {
	releaseServiceRequest(world, sim);
	sim.stateCode = failureState;
}

function finalizeOfficeFloorArrival(
	sim: SimRecord,
	object: PlacedObjectRecord | undefined,
	nextState: number,
): void {
	if (object) advanceOfficePresenceCounter(object);
	sim.destinationFloor = -1;
	sim.selectedFloor = sim.floorAnchor;
	sim.venueReturnState = 0;
	sim.stateCode = nextState;
}

function nextOfficeMorningState(world: WorldState, sim: SimRecord): number {
	// Binary 1228:213c AX=3 branch (same-floor arrival) at 1228:23bb:
	//   if (base_offset == 0) state := 0x00 (STATE_COMMUTE)   ; 1228:2618+2639
	//   else if roll_office_sim_medical_trip_today() → 0x02    ; 1228:23f5
	//   else → 0x01                                            ; 1228:241e
	// roll is `starCount >= 3 && sample_lcg15() % 10 == 0` (1178:0635).
	// RNG is only consumed on the non-base-0 branch, mirroring the binary.
	if (sim.baseOffset === 0) return STATE_COMMUTE;
	if (world.starCount >= 3 && sampleRng(world) % 10 === 0) {
		return STATE_ACTIVE_ALT;
	}
	return STATE_ACTIVE;
}

export function nextOfficeReturnState(sim: SimRecord): number {
	return sim.baseOffset === 1 ? STATE_COMMUTE : STATE_DEPARTURE;
}

function runOfficeServiceEvaluation(
	world: WorldState,
	time: TimeState,
	sim?: SimRecord,
	object?: PlacedObjectRecord,
): void {
	if (world.starCount !== 3 || time.dayCounter % 9 !== 3) return;
	if (world.gateFlags.officeServiceOk !== 0) return;
	if (
		world.gateFlags.evalSimIndex >= 0 &&
		world.gateFlags.evalSimIndex !== NO_EVAL_ENTITY
	) {
		return;
	}
	if (!sim || !object) return;
	if (sim.familyCode !== FAMILY_OFFICE || sim.stateCode !== STATE_ACTIVE)
		return;
	if (object.evalLevel <= 0) return;
	world.gateFlags.officeServiceOk = 1;
}

// --- Per-state handlers (1228:1e45 etc.) ---

/** 1228:1e45 office_refresh_0x00 — STATE_COMMUTE handler.
 *
 * Binary state-0 dispatch issues an INBOUND (lobby → anchor) route via
 * 1228:2644 with direction=1, but the workaround for baseOffset=1 keeps
 * an OUTBOUND path here for post-lunch returns (treating state 0x00 via
 * the 0x62 alias that reaches this refresh). The specific sky_office
 * regression was the OUTBOUND path firing for baseOffset=0 at daypart 0
 * (sim on floor 11 enqueuing src=11 → dst=10). Restrict the OUTBOUND
 * shortcut to baseOffset=1 only; baseOffset=0 takes the INBOUND path,
 * matching the binary's state-0 handler.
 */
function handleOfficeCommute(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	facility: PlacedObjectRecord,
): void {
	if (time.daypartIndex >= 4) {
		sim.stateCode = STATE_DEPARTURE;
		return;
	}
	if (sim.baseOffset === 0) {
		if (time.daypartIndex === 0 && sampleRng(world) % 12 !== 0) return;
	} else {
		if (time.daypartIndex < 3) return;
		if (sampleRng(world) % 12 !== 0) return;
	}
	// Binary 1228:1e45 / 1228:266b: state-0 dispatch calls 1218:0000 with
	// source=floor_anchor, target=LOBBY (hardcoded 10) — i.e. OUTBOUND.
	// Confirmed via dynamic trace of dense_office tick=150 sim=150 fA=13
	// (state_before=0x00 family=7 source_floor=13 target_floor=10).
	const routeResult = resolveSimRouteBetweenFloors(
		world,
		sim,
		sim.floorAnchor,
		LOBBY_FLOOR,
		LOBBY_FLOOR > sim.floorAnchor ? 1 : 0,
		time,
	);
	if (routeResult === -1) {
		sim.stateCode = STATE_NIGHT_B;
		return;
	}
	decrementOfficePresenceCounter(facility, time);
	if (routeResult === 3) {
		advanceOfficePresenceCounter(facility);
		sim.destinationFloor = -1;
		sim.selectedFloor = sim.floorAnchor;
		sim.stateCode = STATE_AT_WORK;
	} else {
		sim.stateCode = STATE_COMMUTE_TRANSIT;
	}
}

/** 1228:1ed5 office_refresh_0x01/0x02 — venue selection (STATE_ACTIVE / STATE_ACTIVE_ALT). */
function handleOfficeActive(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	facility: PlacedObjectRecord,
): void {
	runOfficeServiceEvaluation(world, time, sim, facility);
	if (time.daypartIndex >= 4) {
		if (tryStartMedicalTrip(world, time, sim)) return;
		sim.stateCode = STATE_DEPARTURE;
		sim.destinationFloor = LOBBY_FLOOR;
		sim.selectedFloor = sim.floorAnchor;
		return;
	}
	if (time.daypartIndex === 0) return;
	if (time.daypartIndex === 1 && sampleRng(world) % 12 !== 0) return;

	const dispatched = dispatchCommercialVenueVisit(world, time, sim, {
		venueFamilies: new Set([FAMILY_FAST_FOOD]),
		returnState: STATE_AT_WORK,
		tripState: STATE_ACTIVE_TRANSIT,
		skipPenaltyOnUnavailable: true,
		advanceBeforeSameFloorDwell: true,
	});
	if (dispatched && sim.stateCode === COMMERCIAL_DWELL_STATE) {
		// Binary route_sim_to_commercial_venue (1238:022a) writes state 0x22
		// (STATE_VENUE_TRIP) when same-floor route + acquire_slot both succeed.
		// dispatchCommercialVenueVisit's same-floor success path now calls
		// `tryAcquireOfficeVenueSlot` itself (mirroring the binary state-0x01
		// resolve-rc=3 acquire), and on FULL it writes STATE_ACTIVE_TRANSIT
		// directly. Here we only need to override the post-acquire dwell state
		// from 0x62 to 0x22 to match the binary's same-floor success target.
		sim.stateCode = STATE_VENUE_TRIP;
		sim.queueTick = time.dayTick;
		return;
	}
	if (!dispatched) {
		// Spec §No Fast Food Available: route to lobby for fake lunch round-trip.
		// Worker travels to lobby, dwells, returns to office — never gets stuck.
		const routeResult = resolveSimRouteBetweenFloors(
			world,
			sim,
			sim.floorAnchor,
			LOBBY_FLOOR,
			LOBBY_FLOOR > sim.floorAnchor ? 1 : 0,
			time,
		);
		if (routeResult === -1) {
			// Spec §Route to Lobby Fails: fake-transit sentinel → eventually
			// advance_office_presence_counter → STATE_DEPARTURE (0x05).
			advanceOfficePresenceCounter(facility);
			sim.stateCode = STATE_DEPARTURE;
			return;
		}
		// Phase 1d-ii: resolve owns sim.selectedFloor/destinationFloor.
		if (routeResult === 3) {
			// Office on lobby floor — start venue dwell immediately.
			sim.venueReturnState = 0;
			sim.stateCode = STATE_VENUE_TRIP;
			sim.selectedFloor = LOBBY_FLOOR;
			sim.destinationFloor = -1;
			sim.lastDemandTick = time.dayTick;
			clearSimRoute(sim);
		} else {
			// In-transit to lobby for fake lunch; arrival promotes to 0x22.
			sim.stateCode = STATE_ACTIVE_TRANSIT;
		}
	}
}

/** 1228:1fac office_refresh_0x05 — evening departure (STATE_DEPARTURE). */
function handleOfficeDeparture(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	facility: PlacedObjectRecord,
): void {
	if (time.daypartIndex < 4) return;
	if (time.daypartIndex === 4 && sampleRng(world) % 6 !== 0) {
		return;
	}
	decrementOfficePresenceCounter(facility, time);
	const routeResult = resolveSimRouteBetweenFloors(
		world,
		sim,
		sim.floorAnchor,
		LOBBY_FLOOR,
		0,
		time,
	);
	if (routeResult === -1) {
		failOfficeRoute(world, sim, STATE_NIGHT_B);
		return;
	}
	// Phase 1d-ii: resolve owns sim.selectedFloor/destinationFloor.
	if (routeResult === 3) {
		sim.destinationFloor = -1;
		sim.selectedFloor = LOBBY_FLOOR;
		sim.stateCode = STATE_PARKED;
		releaseServiceRequest(world, sim);
	} else {
		sim.stateCode = STATE_DEPARTURE_TRANSIT;
	}
}

/** 1228:1dc1 office_refresh_0x20 — morning activation gate (STATE_MORNING_GATE). */
function handleOfficeMorningGate(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	facility: PlacedObjectRecord,
): void {
	if (time.weekendFlag !== 0) return;
	// Binary 1228:1df3: CMP byte ptr ES:[BX+0x14],0x0 — reads the occupied
	// flag (set only by the scoring sweep's first eval_level>0 pass). Sims
	// whose home facility has never been scored must NOT sample RNG here.
	if (facility.occupiedFlag === 0) return;

	if (time.daypartIndex >= 3) return;
	if (time.daypartIndex === 0) {
		if (sampleRng(world) % 12 !== 0) return;
	}

	if (
		world.starCount > 2 &&
		(sim.floorAnchor + sim.homeColumn) % 4 === 1 &&
		facility.unitStatus === 2
	) {
		if (!tryAssignParkingService(world, time, sim)) {
			world.pendingNotifications.push({
				kind: "route_failure",
				message: "Office workers demand Parking",
			});
		}
	}

	const routeResult = resolveSimRouteBetweenFloors(
		world,
		sim,
		LOBBY_FLOOR,
		sim.floorAnchor,
		sim.floorAnchor > LOBBY_FLOOR ? 1 : 0,
		time,
	);
	if (routeResult === -1) {
		// Binary 1228:225f/2285/22ac: the office state-0x20 handler's rc=-1
		// branch zeroes tripCount / elapsedTicks / accumulatedTicks before
		// exiting. `resolve_sim_route_between_floors` on no-route fires the
		// 300-tick penalty + advanceSimTripCounters inside, and the handler
		// immediately wipes the side effects. Without this reset, stuck
		// morning-gate sims accrue spurious stress.
		sim.tripCount = 0;
		sim.elapsedTicks = 0;
		sim.accumulatedTicks = 0;
		sim.stateCode = routeFailureStateForOffice(facility);
		return;
	}
	// Binary 1228:213c/22de+2329 (cases 1/2/3) and 235a+23a5 (case 4): after
	// a non-failure route result, call activate_office_cashflow (1180:0d2e)
	// which pays rent when stayPhase (unitStatus) >= 0x10 and zeros it.
	// The 3-day cycle gate is implicit in the stayPhase counter: rent zeroes
	// it, and sync_stay_phase_if_all_siblings_ready brings it back to 0x10
	// over the following days. The auxValueOrTimer + dayCounter%3 guard on
	// the paired activateThreeDayCashflow (ledger.ts) still catches facilities
	// that skipped rent here because their per-sim route failed earlier in
	// the cycle.
	if (
		facility.auxValueOrTimer !== time.dayCounter + 1 &&
		time.dayCounter % 3 === 0
	) {
		facility.auxValueOrTimer = time.dayCounter + 1;
		// Binary activate_office_cashflow 1180:0d93 sets +0x13 (dirty).
		facility.dirtyFlag = 1;
		addCashflowFromFamilyResource(
			ledger,
			"office",
			facility.rentLevel,
			facility.objectTypeCode,
		);
	}
	activateOfficeCashflow(world, facility, sim);
	// Phase 1d-ii: resolve owns sim+7/sim+0x12. Handler sets state only.
	if (routeResult === 0 || routeResult === 1 || routeResult === 2) {
		sim.stateCode = STATE_MORNING_TRANSIT;
		return;
	}
	advanceOfficePresenceCounter(facility);
	sim.destinationFloor = -1;
	sim.selectedFloor = sim.floorAnchor;
	sim.stateCode = nextOfficeMorningState(world, sim);
}

/** 1228:1f33 office_refresh_0x21 — at office, ready for venue visits (STATE_AT_WORK). */
function handleOfficeAtWork(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	facility: PlacedObjectRecord,
): void {
	if (time.daypartIndex >= 4) {
		sim.stateCode = STATE_PARKED;
		sim.destinationFloor = -1;
		clearSimRoute(sim);
		releaseServiceRequest(world, sim);
		return;
	}
	if (time.daypartIndex === 3) {
		if (sampleRng(world) % 12 !== 0) return;
	} else {
		return;
	}

	const routeResult = resolveSimRouteBetweenFloors(
		world,
		sim,
		LOBBY_FLOOR,
		sim.floorAnchor,
		sim.floorAnchor > LOBBY_FLOOR ? 1 : 0,
		time,
	);
	if (routeResult === -1) {
		failOfficeRoute(world, sim, STATE_NIGHT_B);
		return;
	}
	// Phase 1d-ii: resolve owns sim+7/sim+0x12.
	if (routeResult === 3) {
		finalizeOfficeFloorArrival(sim, facility, STATE_DEPARTURE);
	} else {
		sim.stateCode = STATE_AT_WORK_TRANSIT;
	}
}

/** 1228:1f62 office_refresh_0x22/0x23 — at venue / routing home
 * (STATE_VENUE_TRIP / STATE_VENUE_HOME_TRANSIT). */
function handleOfficeVenueTrip(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	facility: PlacedObjectRecord,
): void {
	if (time.daypartIndex >= 4) {
		// Late-day forced park: release the venue slot so the venue's
		// currentPopulation tracks workers leaving (binary
		// release_commercial_venue_slot, 11b0:0fae, called from the same
		// state handler before the park transition).
		const venue = findCommercialVenueAtFloor(
			world,
			sim.selectedFloor,
			new Set([FAMILY_FAST_FOOD]),
		);
		if (venue) releaseOfficeVenueSlot(venue, sim, time, true);
		sim.stateCode = STATE_PARKED;
		releaseServiceRequest(world, sim);
		return;
	}
	if (time.daypartIndex < 2) {
		return;
	}
	const isFakeLunch = sim.selectedFloor === LOBBY_FLOOR;
	if (
		sim.stateCode === STATE_VENUE_TRIP &&
		!isFakeLunch &&
		time.dayTick - sim.queueTick < COMMERCIAL_VENUE_DWELL_TICKS
	) {
		return;
	}
	// Binary route_sim_back_from_commercial_venue (1238:0244) calls
	// release_commercial_venue_slot (11b0:0fae) BEFORE resolving the return
	// route. Mirror the decrement here. Skip the dwell gate inside
	// releaseOfficeVenueSlot because the caller's `dayTick - queueTick >=
	// COMMERCIAL_VENUE_DWELL_TICKS` check above already enforces it.
	if (sim.stateCode === STATE_VENUE_TRIP && !isFakeLunch) {
		const venue = findCommercialVenueAtFloor(
			world,
			sim.selectedFloor,
			new Set([FAMILY_FAST_FOOD]),
		);
		if (venue) releaseOfficeVenueSlot(venue, sim, time, true);
	}
	const routeResult = resolveSimRouteBetweenFloors(
		world,
		sim,
		sim.selectedFloor,
		sim.floorAnchor,
		sim.floorAnchor > sim.selectedFloor ? 1 : 0,
		time,
	);
	if (routeResult === -1) {
		failOfficeRoute(world, sim, STATE_NIGHT_B);
		return;
	}
	// Phase 1d-ii: resolve owns sim.selectedFloor/destinationFloor.
	// Binary 1238:0244 (route_sim_back_from_commercial_venue): after the
	// resolve_sim_route_between_floors call, only the state byte (sim+5) is
	// written; sim+0xa (last_trip_tick, our `lastDemandTick`) is left at the
	// dayTick value the resolver's carrier-enqueue branch (returning 2)
	// stamped. The boarding-time `accumulate_elapsed_delay_into_current_sim`
	// later folds (boardingTick - dayTick) into elapsed_packed so the wait
	// time on the carrier queue is counted into stress. Don't clobber lt
	// here — the previous `sim.lastDemandTick = -1;` made the boarding rebase
	// a no-op and lost ~4 ticks per return trip on dense_office (sim 2 at
	// d0t802→d0t806 was missing 4 ticks vs the binary trace).
	if (routeResult === 3) {
		finalizeOfficeFloorArrival(sim, facility, nextOfficeReturnState(sim));
	} else {
		sim.stateCode = STATE_VENUE_HOME_TRANSIT;
	}
}

/** 1228:1d8e office_refresh_0x25/0x26/0x27 — night/failure park states. */
function handleOfficeNightPark(
	_world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	_facility: PlacedObjectRecord,
): void {
	if (time.dayTick > 2300) {
		sim.stateCode = STATE_MORNING_GATE;
	}
}

/** Per-stride in-transit handler. Binary state-handler jump table for the +0x40
 * transit aliases (0x40/0x42/0x45/0x61/0x63) reuses the base handler (0x00/0x02/
 * 0x05/0x21/0x23), each of which calls resolve_sim_route_between_floors per
 * stride. For an in-segment sim, that resolve advances sim+7 by one leg; when
 * sim+7 reaches the target, resolve returns 3 (same-floor, advance fires inside
 * resolve) and the handler triggers state transition. We pass
 * `alreadyAdvanced=true` to `handleOfficeSimArrival` because the resolve call
 * above already invoked `advance_sim_trip_counters`. */
function handleOfficeTransit(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	_facility: PlacedObjectRecord,
): void {
	if (sim.route.mode === "carrier") return;
	if (sim.destinationFloor < 0) return;
	const sourceFloor = sim.selectedFloor;
	const targetFloor = sim.destinationFloor;
	// Alias state (+0x40 transit handler): in the binary `emit_distance_feedback`
	// is `0` (the comparison `current_state == base_state` is false here), so
	// distance feedback was already applied by the base state when the trip
	// began. Suppress here to avoid double-counting per-leg. `is_passenger_route`
	// is still `1` for office.
	const routeResult = resolveSimRouteBetweenFloors(
		world,
		sim,
		sourceFloor,
		targetFloor,
		targetFloor > sourceFloor ? 1 : 0,
		time,
		{ emitDistanceFeedback: false },
	);
	if (routeResult === 3) {
		// Arrived. Trip counter already advanced inside resolve (same-floor
		// branch at 1218:0046). Skip `dispatchSimArrival` so we can pass
		// `alreadyAdvanced=true` and avoid double-counting; mirror the small
		// pieces dispatchSimArrival does (selectedFloor, clearSimRoute) inline.
		sim.selectedFloor = targetFloor;
		clearSimRoute(sim);
		handleOfficeSimArrival(world, time, sim, targetFloor, true);
	}
}

/** 1228:213c morning-transit handler (STATE_MORNING_TRANSIT 0x60). Alias of
 * state 0x20 in the binary jump table, but with `variantFlag=0` (distance
 * feedback off). Per binary AX=3 branch (1228:23bb), the same medical-roll
 * dispatch fires for both 0x20 and 0x60: rc=3 → ACTIVE_ALT or ACTIVE (or
 * STATE_COMMUTE for baseOffset==0).
 *   rc=-1 → release + state per routeFailureStateForOffice
 *   rc=0/1/2 → state 0x60 (stays in transit; next stride re-resolves)
 *   rc=3 → nextOfficeMorningState
 * The variant flag affects `add_delay_to_current_sim` distance feedback, but
 * our resolver applies the penalty unconditionally — accept the slight stress
 * mismatch for now (Phase 4 task constraint). */
function handleOfficeMorningTransitRetry(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	facility: PlacedObjectRecord,
): void {
	// Source is the sim's current floor (binary sim+7). For idle/queue-full sims,
	// selectedFloor is the home floor; for in-segment sims, selectedFloor is the
	// last leg endpoint.
	const sourceFloor = sim.selectedFloor;
	const targetFloor = sim.floorAnchor;
	// Alias state 0x60 (MORNING_TRANSIT): in the binary `emit_distance_feedback`
	// is `0` here (current_state 0x60 != base_state 0x20). Distance feedback was
	// already applied by the base state 0x20 dispatch.
	const routeResult = resolveSimRouteBetweenFloors(
		world,
		sim,
		sourceFloor,
		targetFloor,
		targetFloor > sourceFloor ? 1 : 0,
		time,
		{ emitDistanceFeedback: false },
	);
	if (routeResult === -1) {
		sim.stateCode = routeFailureStateForOffice(facility);
		releaseServiceRequest(world, sim);
		return;
	}
	if (routeResult === 3) {
		// Trip arrived at target floor; resolve already advanced trip counters.
		advanceOfficePresenceCounter(facility);
		sim.destinationFloor = -1;
		sim.selectedFloor = sim.floorAnchor;
		sim.stateCode = nextOfficeMorningState(world, sim);
	}
	// rc 0/1/2: stay in STATE_MORNING_TRANSIT; next stride will re-resolve.
}

export type OfficeHandler = (
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	facility: PlacedObjectRecord,
) => void;

/** cs:2005 refresh-state dispatch table (state_code → handler). */
export const OFFICE_REFRESH_HANDLER_TABLE: ReadonlyMap<number, OfficeHandler> =
	new Map([
		[STATE_COMMUTE, handleOfficeCommute], // 0x00 → 1228:1e45
		[STATE_ACTIVE, handleOfficeActive], // 0x01 → 1228:1ed5
		[STATE_ACTIVE_ALT, handleOfficeActive], // 0x02 → 1228:1ed5 (same handler)
		[STATE_DEPARTURE, handleOfficeDeparture], // 0x05 → 1228:1fac
		[STATE_MORNING_GATE, handleOfficeMorningGate], // 0x20 → 1228:1dc1
		[STATE_AT_WORK, handleOfficeAtWork], // 0x21 → 1228:1f33
		[STATE_VENUE_TRIP, handleOfficeVenueTrip], // 0x22 → 1228:1f62
		[STATE_NIGHT_A, handleOfficeNightPark], // 0x25 → 1228:1d8e
		[STATE_NIGHT_B, handleOfficeNightPark], // 0x26 → 1228:1d8e (same handler)
		[STATE_PARKED, handleOfficeNightPark], // 0x27 → 1228:1d8e (same handler)
		[STATE_COMMUTE_TRANSIT, handleOfficeTransit], // 0x40
		[STATE_ACTIVE_TRANSIT, handleOfficeTransit], // 0x41
		[STATE_VENUE_TRIP_TRANSIT, handleOfficeTransit], // 0x42
		[STATE_DEPARTURE_TRANSIT, handleOfficeTransit], // 0x45
		[STATE_MORNING_TRANSIT, handleOfficeMorningTransitRetry], // 0x60 → 1228:213c alias
		[STATE_AT_WORK_TRANSIT, handleOfficeTransit], // 0x61
		[STATE_VENUE_HOME_TRANSIT, handleOfficeTransit], // 0x62
		[STATE_DWELL_RETURN_TRANSIT, handleOfficeTransit], // 0x63
	]);

export function processOfficeSim(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
): void {
	const facility = findObjectForSim(world, sim);
	if (!facility) return;

	const handler = OFFICE_REFRESH_HANDLER_TABLE.get(sim.stateCode);
	handler?.(world, ledger, time, sim, facility);
}

/**
 * Carrier/segment arrival dispatch for office sims (called from
 * `dispatch_destination_queue_entries` 1218:0883 → office state handler, and
 * also from `handleOfficeTransit` after a per-stride segment-leg arrival).
 *
 * Binary behavior: at carrier arrival, the office state-code dispatcher
 * (1228:2031) jumps to the per-state handler for the alias state (0x40-0x45,
 * 0x60-0x63). Each handler invokes `resolve_sim_route_between_floors` with
 * `is_passenger_route=1` (every binary call site pushes 0x1). For alias
 * states, `source = sim+7 = arrival_floor` and `target = anchor / lobby /
 * venue floor` matching the trip's destination. When source == target, the
 * resolve same-floor branch (1218:0046) calls `advance_sim_trip_counters`
 * because is_passenger_route != 0. We mirror that advance directly here
 * for the carrier-arrival path (which bypasses the per-state-handler
 * resolve).
 *
 * `alreadyAdvanced=true` is passed by `handleOfficeTransit` because the
 * per-stride resolve already invoked `advance_sim_trip_counters` inside its
 * same-floor (rc=3) branch. The caller passes `true` to avoid a double
 * advance. The carrier-arrival path leaves it `false` so this routine
 * fires the advance itself.
 */
export function handleOfficeSimArrival(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
	arrivalFloor: number,
	alreadyAdvanced = false,
): void {
	const object = findObjectForSim(world, sim);
	const advanceIfNeeded = (): void => {
		if (!alreadyAdvanced) advanceSimTripCounters(sim);
	};

	if (
		sim.stateCode === STATE_MORNING_TRANSIT &&
		arrivalFloor === sim.floorAnchor
	) {
		// 1228:213c alias 0x60 of 0x20 — resolve same-floor advance fires.
		advanceIfNeeded();
		finalizeOfficeFloorArrival(sim, object, nextOfficeMorningState(world, sim));
		return;
	}

	if (
		sim.stateCode === STATE_AT_WORK_TRANSIT &&
		arrivalFloor === sim.floorAnchor
	) {
		// 1228:2429 alias 0x61 of 0x21 — resolve same-floor advance fires.
		advanceIfNeeded();
		finalizeOfficeFloorArrival(sim, object, STATE_DEPARTURE);
		return;
	}

	if (
		(sim.stateCode === STATE_VENUE_HOME_TRANSIT ||
			sim.stateCode === STATE_DWELL_RETURN_TRANSIT) &&
		arrivalFloor === sim.floorAnchor
	) {
		// 1228:24cd / 1228:2505 alias 0x62/0x63 — route_sim_back_from_commercial_venue
		// (1238:0244) calls resolve with `is_passenger_route=1` (1238:02fa PUSH 0x1).
		advanceIfNeeded();
		finalizeOfficeFloorArrival(sim, object, nextOfficeReturnState(sim));
		return;
	}

	if (
		sim.stateCode === STATE_DEPARTURE_TRANSIT &&
		arrivalFloor === LOBBY_FLOOR
	) {
		// 1228:2980 alias 0x45 of 0x05 — resolve same-floor advance fires.
		advanceIfNeeded();
		sim.stateCode = STATE_PARKED;
		sim.selectedFloor = LOBBY_FLOOR;
		releaseServiceRequest(world, sim);
		return;
	}

	if (
		sim.stateCode === STATE_COMMUTE_TRANSIT &&
		arrivalFloor === sim.floorAnchor
	) {
		// 1228:2644 alias 0x40 of 0x00 — resolve same-floor advance fires when
		// source (sim+7=arrival) == target (lobby for baseOffset==0, anchor for
		// baseOffset>0).
		advanceIfNeeded();
		finalizeOfficeFloorArrival(sim, object, STATE_AT_WORK);
		return;
	}

	if (
		sim.stateCode === STATE_COMMUTE_TRANSIT &&
		sim.baseOffset <= 1 &&
		arrivalFloor === LOBBY_FLOOR
	) {
		// 1228:2644 alias 0x40 of 0x00 outbound lobby leg for occupants 0/1
		// reaches the post-commute office path.
		advanceIfNeeded();
		sim.destinationFloor = -1;
		sim.selectedFloor = LOBBY_FLOOR;
		sim.stateCode = STATE_AT_WORK;
		return;
	}

	// Arrival while in ACTIVE_TRANSIT (0x41) or VENUE_TRIP_TRANSIT (0x42) —
	// binary re-runs the 0x01/0x41 (or 0x02/0x42) state handler at arrival via
	// `dispatch_destination_queue_entries` (1218:0883), which calls
	// `route_sim_to_commercial_venue` (1238:0000). That helper resolves
	// floor→floor (rc=3 same-floor since arrival floor == venue floor), then
	// calls `acquire_commercial_venue_slot` (11b0:0d92):
	//   - rc=3 (slot acquired): writes entity[+5] = 0x22 (STATE_VENUE_TRIP)
	//   - rc=2 (full, currentPopulation > 39): JMP caseD_0 → entity[+5] = 0x41
	//     (stays in STATE_ACTIVE_TRANSIT; the per-stride 0x41 handler will
	//     re-attempt acquire next stride until a slot opens up)
	//   - rc=-1 (closed/dormant): falls through to writing 0x22, but the
	//     subsequent 0x22 release handler short-circuits via the same gates.
	//
	// queueTick records the arrival time so the 60-tick dwell gate in
	// processOfficeSim can block correctly. We use queueTick rather than
	// lastDemandTick because the stride rebase clears lastDemandTick to -1
	// on every call, which would break the dwell gate.
	//
	// Both aliases advance trip counters at arrival via the resolve same-floor
	// branch (1218:0046, is_passenger_route=1).
	if (
		(sim.stateCode === STATE_ACTIVE_TRANSIT ||
			sim.stateCode === STATE_VENUE_TRIP_TRANSIT) &&
		(sim.destinationFloor < 0 || arrivalFloor === sim.destinationFloor)
	) {
		advanceIfNeeded();
		sim.selectedFloor = arrivalFloor;
		// Real fast-food venue lookup. The lobby fallback path (sim being
		// routed to LOBBY_FLOOR because no venue was available) is identified
		// by selectedFloor == LOBBY_FLOOR; for that path the binary's
		// `acquire_commercial_venue_slot` short-circuits at
		// `facility_slot_index < 0` and returns 3 (no capacity check). Mirror
		// that by skipping the lookup and going straight to STATE_VENUE_TRIP.
		const venue =
			arrivalFloor === LOBBY_FLOOR
				? null
				: findCommercialVenueAtFloor(
						world,
						arrivalFloor,
						new Set([FAMILY_FAST_FOOD]),
					);
		if (venue) {
			// Office worker arriving at a fast-food venue: pass the venue
			// owner's family (FAMILY_FAST_FOOD) so the type/variant gate fires
			// (sim.familyCode = FAMILY_OFFICE differs) and acquireCount is
			// bumped on success — mirroring binary 11b0:0f3a–0f55.
			const result = tryAcquireOfficeVenueSlot(
				venue,
				sim,
				time,
				FAMILY_FAST_FOOD,
			);
			if (result === VENUE_SLOT_FULL) {
				// Stay in STATE_ACTIVE_TRANSIT; the per-stride 0x41 handler
				// will re-attempt acquire next stride. Don't clear
				// destinationFloor — the binary keeps entity[+6] (venue index)
				// intact and re-runs the handler.
				return;
			}
			// VENUE_SLOT_ACQUIRED or VENUE_SLOT_UNAVAILABLE → state 0x22.
		}
		sim.destinationFloor = -1;
		sim.stateCode = STATE_VENUE_TRIP;
		sim.queueTick = time.dayTick;
		return;
	}

	// Carrier dropped the sim at a transfer point (e.g. sky lobby) on the way
	// to the anchor / lobby. Binary 1228:213c (and the analogous handlers for
	// other transit aliases) re-resolves the next leg from the arrival floor.
	// Skip distance feedback — the base state already applied it on trip start.
	if (
		sim.stateCode === STATE_MORNING_TRANSIT ||
		sim.stateCode === STATE_AT_WORK_TRANSIT ||
		sim.stateCode === STATE_VENUE_HOME_TRANSIT ||
		sim.stateCode === STATE_DWELL_RETURN_TRANSIT ||
		sim.stateCode === STATE_DEPARTURE_TRANSIT ||
		sim.stateCode === STATE_COMMUTE_TRANSIT ||
		sim.stateCode === STATE_ACTIVE_TRANSIT ||
		sim.stateCode === STATE_VENUE_TRIP_TRANSIT
	) {
		const targetFloor =
			sim.stateCode === STATE_MORNING_TRANSIT ||
			sim.stateCode === STATE_AT_WORK_TRANSIT ||
			sim.stateCode === STATE_VENUE_HOME_TRANSIT ||
			sim.stateCode === STATE_DWELL_RETURN_TRANSIT
				? sim.floorAnchor
				: sim.stateCode === STATE_ACTIVE_TRANSIT ||
						sim.stateCode === STATE_VENUE_TRIP_TRANSIT
					? sim.destinationFloor
					: LOBBY_FLOOR;
		sim.selectedFloor = arrivalFloor;
		const routeResult = resolveSimRouteBetweenFloors(
			world,
			sim,
			arrivalFloor,
			targetFloor,
			targetFloor > arrivalFloor ? 1 : 0,
			time,
			{ emitDistanceFeedback: false },
		);
		if (routeResult === -1) {
			failOfficeRoute(world, sim, STATE_NIGHT_B);
		}
		// rc 0/1/2: stay in transit; next leg's arrival re-enters here.
		// rc 3 can't happen — arrivalFloor != target by precondition.
		return;
	}

	if (sim.stateCode === STATE_COMMUTE && arrivalFloor === sim.floorAnchor) {
		finalizeOfficeFloorArrival(sim, object, STATE_ACTIVE);
		return;
	}

	if (sim.stateCode === STATE_DEPARTURE && arrivalFloor === LOBBY_FLOOR) {
		sim.destinationFloor = -1;
		sim.selectedFloor = sim.floorAnchor;
		sim.stateCode = STATE_PARKED;
	}
}
