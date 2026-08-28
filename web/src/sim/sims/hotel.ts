import { addCashflowFromFamilyResource, type LedgerState } from "../ledger";
import { addToPopulationBucket } from "../progression";
import {
	FAMILY_HOTEL_SINGLE,
	FAMILY_HOTEL_SUITE,
	FAMILY_HOTEL_TWIN,
} from "../resources";
import { setSimInTransit } from "../sim-access/state-bits";
import { advanceSimTripCounters } from "../stress/trip-counters";
import { DAY_TICK_NEW_DAY, type TimeState } from "../time";
import {
	GRID_HEIGHT,
	type PlacedObjectRecord,
	type SimRecord,
	sampleRng,
	type WorldState,
} from "../world";
import {
	dispatchCommercialVenueVisit,
	findObjectForSim,
	finishCommercialVenueDwell,
	resolveSimRouteBetweenFloors,
	tryAssignParkingService,
} from "./index";
import {
	ACTIVATION_TICK_CAP,
	COMMERCIAL_DWELL_STATE,
	HOTEL_FAMILIES,
	HOTEL_ROOM_SELECTOR,
	LOBBY_FLOOR,
	STATE_ACTIVE,
	STATE_ACTIVE_TRANSIT,
	STATE_CHECKOUT_QUEUE,
	STATE_DEPARTURE,
	STATE_DEPARTURE_TRANSIT,
	STATE_HOTEL_PARKED,
	STATE_MORNING_GATE,
	STATE_MORNING_TRANSIT,
	STATE_NIGHT_B,
	STATE_TRANSITION,
	STATE_VENUE_TRIP,
} from "./states";

/**
 * Binary's `subtype_index` (sim record byte 1) = floor-local ROOM rank in
 * column-ascending order, confirmed via emulator watchpoint on the IDIV at
 * 1228:3493 (every occupant of a given room shares the same subtype; the
 * per-occupant differentiator is the `occupant` word at sim+2 = baseOffset).
 * The IDIV parity therefore splits *rooms*, not individual sims.
 */
function floorLocalRoomRank(world: WorldState, sim: SimRecord): number {
	const floorY = GRID_HEIGHT - 1 - sim.floorAnchor;
	const columns: number[] = [];
	for (const [key, obj] of Object.entries(world.placedObjects)) {
		if (!HOTEL_FAMILIES.has(obj.objectTypeCode)) continue;
		const [x, y] = key.split(",").map(Number);
		if (y !== floorY) continue;
		columns.push(x);
	}
	columns.sort((a, b) => a - b);
	return Math.max(0, columns.indexOf(sim.homeColumn));
}

function hotelArrivalState(world: WorldState, sim: SimRecord): number {
	return (floorLocalRoomRank(world, sim) & 1) === 0
		? STATE_ACTIVE
		: STATE_CHECKOUT_QUEUE;
}

/** activate_family_345_unit @ 1180:0e72: writes occupied-band base 0x00 / 0x08
 * only when the room was in the vacant band (>= 0x18). Also bumps the
 * population bucket by +1 (single) or +2 (twin/suite). */
function activateHotelUnit(
	world: WorldState,
	object: PlacedObjectRecord,
	time: TimeState,
): void {
	if (object.unitStatus >= 0x18) {
		object.unitStatus = time.daypartIndex < 4 ? 0 : 8;
		object.activationTickCount = 0;
		const amount = object.objectTypeCode === FAMILY_HOTEL_SINGLE ? 1 : 2;
		addToPopulationBucket(world, object.objectTypeCode, amount);
	}
}

/** increment_stay_phase_345 @ 1228:6a56: rotates the 0x10 sentinel back to
 * 1 / 9, otherwise increments by 1 (mod 256). */
function incrementStayPhase345(
	object: PlacedObjectRecord,
	time: TimeState,
): void {
	if (object.unitStatus === 0x10) {
		object.unitStatus = time.daypartIndex < 4 ? 1 : 9;
	} else {
		object.unitStatus = (object.unitStatus + 1) & 0xff;
	}
}

/**
 * Binary no-venue path for hotel state-0x01 (route_sim_to_commercial_venue
 * 1238:0000): when `pickAvailableVenue` returns null, origin=home floor,
 * destination defaults to lobby. Resolver return drives the state:
 *   1/2 → STATE_ACTIVE_TRANSIT (stairs / carrier ride to lobby)
 *   3   → STATE_VENUE_TRIP directly (acquire_slot(0xb0)=3 fall-through)
 *   0/-1 → STATE_CHECKOUT_QUEUE (Phase 4 will replace with sim+8=0xff retry)
 * `venueReturnState = STATE_CHECKOUT_QUEUE` acts as the no-venue marker so
 * the arrival + dwell handlers steer through 0x22 → 0x62 → 0x04.
 */
function routeHotelToLobbyNoVenue(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	const directionFlag = sim.floorAnchor > LOBBY_FLOOR ? 0 : 1;
	const result = resolveSimRouteBetweenFloors(
		world,
		sim,
		sim.floorAnchor,
		LOBBY_FLOOR,
		directionFlag,
		time,
	);
	if (result === -1 || result === 0) {
		sim.stateCode = STATE_CHECKOUT_QUEUE;
		return;
	}
	sim.venueReturnState = STATE_CHECKOUT_QUEUE;
	if (result === 3) {
		sim.destinationFloor = -1;
		sim.selectedFloor = LOBBY_FLOOR;
		sim.stateCode = STATE_VENUE_TRIP;
		sim.queueTick = time.dayTick;
		return;
	}
	sim.originFloor = sim.floorAnchor;
	// Phase 4: resolve owns sim.selectedFloor / destinationFloor (per-leg).
	sim.stateCode = STATE_ACTIVE_TRANSIT;
}

function activateHotelStay(
	world: WorldState,
	sim: SimRecord,
	time: TimeState,
): void {
	const object = findObjectForSim(world, sim);
	if (!object) return;

	// Route requirement: actual route must succeed, not just structural check.
	const directionFlag = sim.floorAnchor > LOBBY_FLOOR ? 1 : 0;
	const result = resolveSimRouteBetweenFloors(
		world,
		sim,
		LOBBY_FLOOR,
		sim.floorAnchor,
		directionFlag,
		time,
	);
	if (result === -1 || result === 0) {
		return;
	}

	sim.originFloor = LOBBY_FLOOR;

	// Binary 1228:33ba (en-route) + 1228:3434 (same-floor): both branches call
	// activate_family_345_unit when the room is still vacant.
	activateHotelUnit(world, object, time);

	if (result === 3) {
		// Same-floor arrival: increment stay phase, then parity-split into
		// STATE_ACTIVE (even) or STATE_CHECKOUT_QUEUE (odd).
		incrementStayPhase345(object, time);
		sim.stateCode = hotelArrivalState(world, sim);
		sim.selectedFloor = sim.floorAnchor;
	} else {
		// Phase 4: resolve owns sim.selectedFloor / destinationFloor (per-leg).
		sim.stateCode = STATE_MORNING_TRANSIT;
	}
}

export function checkoutHotelStay(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	_sim: SimRecord,
	object: PlacedObjectRecord,
): void {
	const tileName =
		object.objectTypeCode === FAMILY_HOTEL_SINGLE
			? "hotelSingle"
			: object.objectTypeCode === FAMILY_HOTEL_TWIN
				? "hotelTwin"
				: "hotelSuite";
	addCashflowFromFamilyResource(
		ledger,
		tileName,
		object.rentLevel,
		object.objectTypeCode,
	);
	world.gateFlags.family345SaleCount += 1;
	const saleCount = world.gateFlags.family345SaleCount;
	if (
		(saleCount < 20 && saleCount % 2 === 0) ||
		(saleCount >= 20 && saleCount % 8 === 0)
	) {
		world.gateFlags.newspaperTrigger = 1;
	} else {
		world.gateFlags.newspaperTrigger = 0;
	}
	// Binary 1228:2fa7 (dispatch state-0x05) does NOT touch sibling state
	// bytes at checkout. The base-0→PARKED / others→MORNING_GATE rewrite lives
	// in the per-sim NIGHT_B (state 0x26) handler at 1228:2bc5, applied only
	// to the sim being dispatched. Iterating siblings here clobbers any twin
	// already mid-trip on the carrier (state 0x45 → 0x20 anomaly).
	object.unitStatus = time.daypartIndex < 4 ? 0x28 : 0x30;
	// Binary deactivate_family_hotel_unit_with_income (1180:0f24):
	//   1180:0f7d: MOV byte ptr ES:[BX+0x13],1  (dirty)
	//   1180:0f8e: MOV byte ptr ES:[BX+0x14],0  (occupied cleared)
	object.dirtyFlag = 1;
	object.occupiedFlag = 0;
	object.activationTickCount = 0;
	const populationAmount =
		object.objectTypeCode === FAMILY_HOTEL_SINGLE ? 1 : 2;
	addToPopulationBucket(world, object.objectTypeCode, -populationAmount);
}

// --- Per-state handlers ---

/** hotel_refresh_0x24 — parked, awaiting guest room assignment. */
function handleHotelParked(
	_world: WorldState,
	_ledger: LedgerState,
	_time: TimeState,
	_sim: SimRecord,
	_object: PlacedObjectRecord,
): void {
	// Binary: state 0x24 is NOT in the hotel jump table — it's a no-op.
}

/** hotel_refresh_0x20 — morning activation gate (STATE_MORNING_GATE). */
function handleHotelMorningGate(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	object: PlacedObjectRecord,
): void {
	// Binary 1228:2c8c: CMP byte ptr ES:[BX+0x14],0x0 — reads occupied flag.
	if (object.occupiedFlag === 0) return;
	if (time.daypartIndex === 4) {
		if (sampleRng(world) % 12 !== 0) return;
		if (sim.familyCode === FAMILY_HOTEL_SUITE && world.starCount <= 2) {
			sim.stateCode = STATE_NIGHT_B;
			return;
		}
		activateHotelStay(world, sim, time);
		return;
	}
	// Binary refresh jumptable state 0x20 (1228:2c63): `daypart > 4 AND
	// day_tick < 2300` dispatches the state-0x20 handler unconditionally
	// (no RNG). The prior code converted to CHECKOUT_QUEUE here, which
	// then rolled the CHECKOUT_QUEUE 1/12 gate — an extra LCG sample.
	if (time.daypartIndex > 4 && time.dayTick < DAY_TICK_NEW_DAY) {
		if (sim.familyCode === FAMILY_HOTEL_SUITE && world.starCount <= 2) {
			sim.stateCode = STATE_NIGHT_B;
			return;
		}
		activateHotelStay(world, sim, time);
		return;
	}
}

/** hotel_refresh_0x01 — active, route to commercial venue (STATE_ACTIVE). */
function handleHotelActive(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	object: PlacedObjectRecord,
): void {
	if (time.daypartIndex <= 3) return;
	if (time.daypartIndex > 4) {
		sim.stateCode = STATE_CHECKOUT_QUEUE;
		return;
	}
	if (sampleRng(world) % 6 !== 0) return;
	if (
		sim.familyCode === FAMILY_HOTEL_SUITE &&
		world.starCount > 2 &&
		object.unitStatus !== 0
	) {
		tryAssignParkingService(world, time, sim);
	}
	const dispatched = dispatchCommercialVenueVisit(world, time, sim, {
		venueFamilies: HOTEL_ROOM_SELECTOR,
		returnState: STATE_ACTIVE,
		tripState: STATE_ACTIVE_TRANSIT,
		skipPenaltyOnUnavailable: true,
		advanceBeforeSameFloorDwell: true,
		onVenueReserved: () => {
			object.activationTickCount = Math.min(
				ACTIVATION_TICK_CAP,
				object.activationTickCount + 1,
			);
		},
	});
	if (dispatched && sim.stateCode === COMMERCIAL_DWELL_STATE) {
		// Hotel same-floor venue: binary writes state=0x22, not 0x62.
		sim.stateCode = STATE_VENUE_TRIP;
		sim.queueTick = time.dayTick;
		sim.lastDemandTick = -1;
		return;
	}
	if (!dispatched) {
		routeHotelToLobbyNoVenue(world, time, sim);
	}
}

/** Per-stride in-transit handler for STATE_ACTIVE_TRANSIT (0x41) and other
 * transit states whose binary base handler does NOT call resolve. The binary's
 * 0x41 handler (0x3126, alias of 0x01) uses the venue selector, not resolve;
 * for in-segment sims here we must still re-resolve the segment so the leg
 * progresses (carrier-routed sims are gated out by the caller). */
function handleHotelActiveTransit(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	_object: PlacedObjectRecord,
): void {
	if (sim.route.mode === "carrier") return;
	if (sim.destinationFloor < 0) return;
	const sourceFloor = sim.selectedFloor;
	const targetFloor = sim.destinationFloor;
	// Alias state 0x41 (ACTIVE_TRANSIT): in the binary `emit_distance_feedback`
	// is `0` here, so distance feedback was already applied by the base state
	// when the trip began.
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
		// Arrived. Trip counter already advanced inside resolve's same-floor
		// branch (1218:0046), which also clears `lastDemandTick`. Mirror the
		// binary's `handle_hotel_guest_venue_acquisition` (1228:4fab) inline
		// state transition: rc=3 → acquire_slot → state=0x22. Don't go through
		// `dispatchSimArrival` → `handleHotelSimArrival` here because that path
		// fires its own advance (matching binary's `dispatch_destination_queue_entries`
		// arrival site). Routing per-stride arrivals back through the carrier-
		// arrival handler causes a double-advance for the same trip.
		void ledger;
		sim.destinationFloor = -1;
		sim.selectedFloor = targetFloor;
		sim.stateCode = STATE_VENUE_TRIP;
		sim.queueTick = time.dayTick;
	}
}

/** Per-stride STATE_MORNING_TRANSIT (0x60) handler. Binary alias of state 0x20
 * (handler 0x317b @ 0x327d): src=lookup 11a0:0650 (assigned target floor),
 * tgt=arg [BP+0xa]. Post-resolve transitions:
 *   rc=-1 → clear or state=0x04 (CHECKOUT_QUEUE)
 *   rc=0/1/2 → state=0x60 (stay in transit; next stride re-resolves)
 *   rc=3 → state=0x01 / 0x04 via hotelArrivalState (parity-split). */
function handleHotelMorningTransit(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	object: PlacedObjectRecord,
): void {
	const sourceFloor = sim.selectedFloor;
	const targetFloor = sim.floorAnchor;
	// Alias state 0x60 (MORNING_TRANSIT): in the binary `emit_distance_feedback`
	// is `0` here (current_state 0x60 != base_state 0x20). Distance feedback
	// was already applied by the base state 0x20 dispatch.
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
		sim.stateCode = STATE_CHECKOUT_QUEUE;
		return;
	}
	if (routeResult === 3) {
		// Same-floor success. Binary 1228:3434 path: activate (if vacant), bump
		// stay phase, then parity-split via hotelArrivalState.
		activateHotelUnit(world, object, time);
		incrementStayPhase345(object, time);
		sim.destinationFloor = -1;
		sim.selectedFloor = sim.floorAnchor;
		sim.stateCode = hotelArrivalState(world, sim);
		return;
	}
	// rc=0/1/2: stay in STATE_MORNING_TRANSIT; next stride will re-resolve.
}

/** Per-stride STATE_DEPARTURE_TRANSIT (0x45) handler. Binary alias of state
 * 0x05 (handler 0x2fa7 @ 0x2fd9): src=lookup 11a0:0650, tgt=arg [BP+0xa].
 * Post-resolve transitions:
 *   rc=-1 → state=0x20 (MORNING_GATE) + service-eval-fail
 *   rc=0/1/2 → state=0x45 (stay in transit; next stride re-resolves)
 *   rc=3 → state=0x20 (MORNING_GATE)
 *
 * NOTE: The binary's shared 0x05/0x45 handler at 1228:2fa7 has a
 * `CMP [BP-0x4], 0x5; JNZ 0x3503` at 1228:30bd that gates the
 * unit_status decrement + checkoutHotelStay block on state == 0x05.
 * State 0x45 SKIPS the checkout — payment was already booked by the
 * base 0x05 dispatch. Mirror that here: do NOT call checkoutHotelStay
 * from this transit handler. */
function handleHotelDepartureTransit(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	_object: PlacedObjectRecord,
): void {
	const sourceFloor = sim.selectedFloor;
	const targetFloor =
		sim.destinationFloor >= 0 ? sim.destinationFloor : LOBBY_FLOOR;
	// Alias state 0x45 (DEPARTURE_TRANSIT): in the binary `emit_distance_feedback`
	// is `0` here. Distance feedback was already applied by the base state
	// 0x05 dispatch.
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
		sim.stateCode = STATE_MORNING_GATE;
		return;
	}
	if (routeResult === 3) {
		// Lobby arrived. Binary 0x45 handler skips the checkout/unit_status
		// decrement block (CMP/JNZ at 1228:30bd) — cash was already booked
		// by the state-0x05 dispatch that initiated this trip. Just transition
		// to MORNING_GATE (or HOTEL_PARKED for baseOffset==0).
		sim.destinationFloor = -1;
		sim.selectedFloor = LOBBY_FLOOR;
		sim.stateCode =
			sim.baseOffset === 0 ? STATE_HOTEL_PARKED : STATE_MORNING_GATE;
		return;
	}
	// rc=0/1/2: stay in STATE_DEPARTURE_TRANSIT; next stride will re-resolve.
}

/** hotel_refresh_0x04 — checkout queue (STATE_CHECKOUT_QUEUE). */
function handleHotelCheckoutQueue(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	_object: PlacedObjectRecord,
): void {
	if (time.daypartIndex < 5) return;
	if (time.dayTick <= 2400) {
		if (sampleRng(world) % 12 !== 0) return;
	}
	sim.stateCode = STATE_TRANSITION;
}

/** hotel_refresh_0x10 — transition (STATE_TRANSITION). */
function handleHotelTransition(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	object: PlacedObjectRecord,
): void {
	if (time.daypartIndex >= 5) {
		if (time.dayTick <= 2566) return;
		if (sampleRng(world) % 12 !== 0) return;
	}
	if (object.unitStatus === 0x10) {
		object.unitStatus = object.objectTypeCode === FAMILY_HOTEL_SINGLE ? 1 : 2;
	}
	sim.stateCode = STATE_DEPARTURE;
}

/** hotel_refresh_0x05 — departure (STATE_DEPARTURE). */
function handleHotelDeparture(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	object: PlacedObjectRecord,
): void {
	// Binary state 0x05 refresh handler at 1228:2c43:
	// daypart 0: 1/12 RNG gate, daypart >= 6: no-op, else: dispatch.
	if (time.daypartIndex === 0) {
		if (sampleRng(world) % 12 !== 0) return;
	}
	if (time.daypartIndex >= 6) return;
	const routeResult = resolveSimRouteBetweenFloors(
		world,
		sim,
		sim.floorAnchor,
		LOBBY_FLOOR,
		sim.floorAnchor > LOBBY_FLOOR ? 0 : 1,
		time,
	);
	if (routeResult === 0) return;
	if (routeResult === -1) {
		// Binary mapping: rc=-1 → state=0x20 (MORNING_GATE) + service-eval-fail.
		sim.stateCode = STATE_MORNING_GATE;
		return;
	}
	sim.originFloor = sim.floorAnchor;
	// Phase 4: resolve owns sim.selectedFloor / destinationFloor.
	// trip_prep_for_checkout: decrement unit_status; when it hits the
	// final countdown boundary, take the payout immediately (binary
	// books cash at dispatch, not lobby arrival).
	object.unitStatus -= 1;
	const ready = (object.unitStatus & 0x07) === 0;
	if (routeResult === 3) {
		sim.destinationFloor = -1;
		sim.selectedFloor = LOBBY_FLOOR;
		if (ready) {
			checkoutHotelStay(world, ledger, time, sim, object);
		} else {
			sim.stateCode =
				sim.baseOffset === 0 ? STATE_HOTEL_PARKED : STATE_MORNING_GATE;
		}
	} else {
		if (ready) {
			checkoutHotelStay(world, ledger, time, sim, object);
		}
		sim.stateCode = STATE_DEPARTURE_TRANSIT;
	}
}

/** hotel_refresh_0x22 — venue trip dwell (STATE_VENUE_TRIP).
 *
 * Binary shared 0x22/0x62 handler (1228:50ef): release_commercial_venue_slot
 * returns 1 immediately for fake-lunch (sim+6 < 0), with no dwell gate. Then
 * resolve_route(lobby, home) runs; the carrier-enqueue branch ORs in the 0x40
 * transit bit, producing state 0x62. The sim physically rides home; arrival
 * drops it to STATE_CHECKOUT_QUEUE via the dispatch handler.
 */
function handleHotelVenueTrip(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	_object: PlacedObjectRecord,
): void {
	if (sim.venueReturnState === STATE_CHECKOUT_QUEUE) {
		const dir = sim.floorAnchor > LOBBY_FLOOR ? 1 : 0;
		const result = resolveSimRouteBetweenFloors(
			world,
			sim,
			LOBBY_FLOOR,
			sim.floorAnchor,
			dir,
			time,
		);
		if (result === 3) {
			sim.stateCode = STATE_CHECKOUT_QUEUE;
			sim.venueReturnState = 0;
			return;
		}
		// Binary 1228:50ef maps resolve returns 0/1/2 all to state 0x62
		// (commercial dwell). Resolve already flips 0x62 on success (1/2);
		// queue-full (0) only sets the waiting bit, so promote here.
		if (result === 0) setSimInTransit(sim, true);
		return;
	}
	// Real-venue dwell: stay in 0x22 until service_duration elapsed.
	if (time.dayTick - sim.queueTick < 64) return;
	if (sim.selectedFloor === sim.floorAnchor) {
		// Binary: release_venue_slot → resolve(floor→floor)=3 → advanceSimTripCounters
		advanceSimTripCounters(sim);
		sim.stateCode = STATE_CHECKOUT_QUEUE;
		sim.venueReturnState = 0;
		return;
	}
	sim.stateCode = COMMERCIAL_DWELL_STATE;
	sim.queueTick = time.dayTick;
}

/** hotel_refresh_0x62 — commercial dwell state (COMMERCIAL_DWELL_STATE).
 *
 * For the no-venue return path, this fires when the sim is in 0x62 with no
 * active carrier route — i.e. the previous resolve attempt returned queue-full.
 * Retry from current floor back home; when the carrier eventually picks the
 * sim up, refresh skips it (route.mode !== 'idle') until arrival fires the
 * dispatch handler.
 */
function handleHotelCommercialDwell(
	world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	_object: PlacedObjectRecord,
): void {
	if (sim.venueReturnState === STATE_CHECKOUT_QUEUE) {
		const dir = sim.floorAnchor > LOBBY_FLOOR ? 1 : 0;
		const result = resolveSimRouteBetweenFloors(
			world,
			sim,
			sim.selectedFloor,
			sim.floorAnchor,
			dir,
			time,
		);
		if (result === 3) {
			sim.stateCode = STATE_CHECKOUT_QUEUE;
			sim.venueReturnState = 0;
		}
		return;
	}
	finishCommercialVenueDwell(sim, time, STATE_ACTIVE);
}

/** hotel_refresh_0x26 — night park (STATE_NIGHT_B). */
function handleHotelNightB(
	_world: WorldState,
	_ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	_object: PlacedObjectRecord,
): void {
	// Binary: dayTick <= 2300 → no-op; dayTick > 2300 → reset.
	if (time.dayTick <= DAY_TICK_NEW_DAY) return;
	sim.stateCode =
		sim.baseOffset === 0 ? STATE_HOTEL_PARKED : STATE_MORNING_GATE;
}

export type HotelHandler = (
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	object: PlacedObjectRecord,
) => void;

/** Hotel refresh dispatch table (state_code → handler). */
export const HOTEL_REFRESH_HANDLER_TABLE: ReadonlyMap<number, HotelHandler> =
	new Map([
		[STATE_HOTEL_PARKED, handleHotelParked], // 0x24
		[STATE_MORNING_GATE, handleHotelMorningGate], // 0x20
		[STATE_ACTIVE, handleHotelActive], // 0x01
		[STATE_MORNING_TRANSIT, handleHotelMorningTransit], // 0x60 → 0x317b alias of 0x20
		[STATE_ACTIVE_TRANSIT, handleHotelActiveTransit], // 0x41 → 0x3126 alias of 0x01
		[STATE_DEPARTURE_TRANSIT, handleHotelDepartureTransit], // 0x45 → 0x2fa7 alias of 0x05
		[STATE_CHECKOUT_QUEUE, handleHotelCheckoutQueue], // 0x04
		[STATE_TRANSITION, handleHotelTransition], // 0x10
		[STATE_DEPARTURE, handleHotelDeparture], // 0x05
		[STATE_VENUE_TRIP, handleHotelVenueTrip], // 0x22
		[COMMERCIAL_DWELL_STATE, handleHotelCommercialDwell], // 0x62
		[STATE_NIGHT_B, handleHotelNightB], // 0x26
	]);

export function processHotelSim(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
): void {
	const object = findObjectForSim(world, sim);
	if (!object) return;

	// Binary `refresh_runtime_entities_for_tick_stride` (1228:0d64) gates the
	// family 3/4/5 handler on `base_offset > 0` — the first occupant (sim+2==0)
	// is never refreshed, so its state persists.
	if (sim.baseOffset === 0) return;

	const handler = HOTEL_REFRESH_HANDLER_TABLE.get(sim.stateCode);
	if (handler) {
		handler(world, ledger, time, sim, object);
	} else {
		sim.stateCode = STATE_HOTEL_PARKED;
	}
}

export function handleHotelSimArrival(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
	arrivalFloor: number,
): void {
	const object = findObjectForSim(world, sim);

	if (
		sim.stateCode === STATE_MORNING_TRANSIT &&
		arrivalFloor === sim.floorAnchor
	) {
		// Binary `dispatch_destination_queue_entries` (1218:0883) writes
		// sim+7 = arrival_floor then invokes the family handler with
		// arg = arrival_floor. For state 0x60 (alias of 0x20) at handler
		// 0x317b, the resolve call is src=sim+7, tgt=arg — both equal to
		// the arrival floor. Resolve's same-floor branch (1218:0046,
		// gated on is_passenger_route=1 which 0x317b passes) advances the
		// sim trip counters before returning rc=3. We mirror that advance
		// here because dispatchSimArrival shortcuts the per-stride re-entry.
		advanceSimTripCounters(sim);
		sim.destinationFloor = -1;
		sim.selectedFloor = sim.floorAnchor;
		if (object) {
			// Binary state-0x60 "in-transit arrived" (1228:3434): activate if
			// vacant, bump stay phase, then parity-split into 0x01 / 0x04.
			activateHotelUnit(world, object, time);
			incrementStayPhase345(object, time);
			sim.stateCode = hotelArrivalState(world, sim);
		}
		return;
	}

	if (
		sim.stateCode === STATE_ACTIVE_TRANSIT &&
		arrivalFloor === LOBBY_FLOOR &&
		sim.venueReturnState === STATE_CHECKOUT_QUEUE
	) {
		// Binary state-0x41 reentry on arrival at lobby with no venue reserved
		// (sim+6 = 0xb0 sentinel): handle_hotel_guest_venue_acquisition (1228:4fab)
		// calls resolve_sim_route_between_floors(is_passenger_route=1, src=lobby,
		// dst=lobby) → same-floor rc=3 → advance_sim_trip_counters at 1218:0046.
		// Always fire advance (binary is unconditional). Per-stride
		// handleHotelActiveTransit's resolve+advance path is suppressed below
		// (we no longer call dispatchSimArrival from there).
		advanceSimTripCounters(sim);
		sim.destinationFloor = -1;
		sim.selectedFloor = LOBBY_FLOOR;
		sim.stateCode = STATE_VENUE_TRIP;
		sim.queueTick = time.dayTick;
		return;
	}

	if (
		sim.stateCode === STATE_ACTIVE_TRANSIT &&
		arrivalFloor === sim.destinationFloor
	) {
		// Hotel arrival at a real commercial venue (binary 1228:4fab sets
		// state=0x22 with queueTick = dayTick as phase-start, not 0x62).
		sim.destinationFloor = -1;
		sim.selectedFloor = arrivalFloor;
		sim.stateCode = STATE_VENUE_TRIP;
		sim.queueTick = time.dayTick;
		return;
	}

	if (
		sim.stateCode === COMMERCIAL_DWELL_STATE &&
		arrivalFloor === sim.floorAnchor &&
		sim.venueReturnState === STATE_CHECKOUT_QUEUE
	) {
		// Binary 1228:50ef state-0x62 dispatch (jumptable entry shared with 0x22):
		// release_commercial_venue_slot returns 1, resolve(home→home)=3, then
		// advance_sim_trip_counters fires inside resolve at 1218:0046.
		// Unconditional to match binary; double-firing is prevented by NOT
		// calling dispatchSimArrival from the per-stride handler below.
		advanceSimTripCounters(sim);
		sim.destinationFloor = -1;
		sim.selectedFloor = arrivalFloor;
		sim.stateCode = STATE_CHECKOUT_QUEUE;
		sim.venueReturnState = 0;
		return;
	}

	if (
		sim.stateCode === STATE_DEPARTURE_TRANSIT &&
		arrivalFloor === LOBBY_FLOOR
	) {
		// Binary state-0x45 dispatch jumptable entry → handler invokes
		// resolve(rc=3, isPassengerRoute=1) on same-floor lobby arrival, which
		// advances trip counters at 1218:0046. Unconditional to match binary.
		advanceSimTripCounters(sim);
		sim.destinationFloor = -1;
		sim.stateCode =
			sim.baseOffset === 0 ? STATE_HOTEL_PARKED : STATE_MORNING_GATE;
		return;
	}

	if (sim.stateCode === STATE_CHECKOUT_QUEUE && arrivalFloor === LOBBY_FLOOR) {
		sim.destinationFloor = -1;
		if (object) {
			object.unitStatus -= 1;
			if ((object.unitStatus & 0x07) === 0) {
				checkoutHotelStay(world, ledger, time, sim, object);
			} else {
				sim.stateCode =
					sim.baseOffset === 0 ? STATE_HOTEL_PARKED : STATE_MORNING_GATE;
			}
		}
	}
}
