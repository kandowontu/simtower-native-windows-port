import type { LedgerState } from "../ledger";
import { FAMILY_RESTAURANT, FAMILY_RETAIL } from "../resources";
import { advanceSimTripCounters } from "../stress/trip-counters";
import type { TimeState } from "../time";
import {
	type CommercialVenueRecord,
	type SimRecord,
	sampleRng,
	VENUE_CLOSED,
	VENUE_DORMANT,
	type WorldState,
} from "../world";
import { activateRetailShop, incrementVenueSeed } from "./facility-refunds";
import {
	clearSimRoute,
	findObjectForSim,
	releaseServiceRequest,
	resolveSimRouteBetweenFloors,
	tryAcquireOfficeVenueSlot,
	VENUE_SLOT_FULL,
} from "./index";
import {
	COMMERCIAL_VENUE_DWELL_TICKS,
	LOBBY_FLOOR,
	STATE_DEPARTURE,
	STATE_DEPARTURE_TRANSIT,
	STATE_MORNING_GATE,
	STATE_MORNING_TRANSIT,
	STATE_NIGHT_A,
	STATE_NIGHT_B,
	STATE_PARKED,
} from "./states";

export function processCommercialSim(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
): void {
	const object = findObjectForSim(world, sim);
	if (!object) return;

	const state = sim.stateCode;

	// --- Night / parked states ---
	if (
		state === STATE_NIGHT_A ||
		state === STATE_NIGHT_B ||
		state === STATE_PARKED
	) {
		if (time.dayTick > 2300) {
			sim.stateCode = STATE_MORNING_GATE;
		}
		return;
	}

	// --- Morning activation gate ---
	// Binary: gate_object_family_retail_state_handler (1228:3ed9) and
	// gate_object_family_restaurant_fast_food_state_handler (1228:466d).
	// The two gates differ: retail has an early DORMANT+occupiedFlag exit
	// *before* any RNG; restaurant/fast-food has no occupied-flag check.
	if (state === STATE_MORNING_GATE) {
		if (sim.familyCode === FAMILY_RETAIL) {
			// Binary 1228:4014/4044 retail gate: return early when the venue is
			// dormant AND the object has not yet been marked operationally
			// occupied (+0x14). Non-dormant venues always fall through to RNG
			// gates. 1228:4044: CMP byte ptr ES:[BX+0x14],0x0.
			if (object.linkedRecordIndex >= 0) {
				const venue = world.sidecars[object.linkedRecordIndex] as
					| CommercialVenueRecord
					| undefined;
				if (
					venue?.kind === "commercial_venue" &&
					venue.availabilityState === VENUE_DORMANT &&
					object.occupiedFlag === 0
				) {
					return;
				}
			}
		}

		if (sim.familyCode === FAMILY_RESTAURANT) {
			// Binary 1228:466d restaurant branch:
			// dp4 → 1/12 RNG gate → dispatch; dp<5 → return;
			// dp5 with dayTick<=2199 → dispatch; else return.
			if (time.daypartIndex === 4) {
				if (sampleRng(world) % 12 !== 0) return;
			} else if (time.daypartIndex < 5) {
				return;
			} else if (time.dayTick > 2199) {
				return;
			}
		} else {
			// Binary 1228:3ed9 / 1228:466d fast-food+retail branch:
			// dp>=5 → return; dayTick<241 → return;
			// dp0-3 → 1/36 RNG gate; dp4 → 1/6 RNG gate.
			if (time.daypartIndex >= 5) return;
			if (time.dayTick < 0xf1) return;
			if (time.daypartIndex <= 3) {
				if (sampleRng(world) % 36 !== 0) return;
			} else {
				if (sampleRng(world) % 6 !== 0) return;
			}
		}

		// Dispatch. Restaurant/fast-food share dispatch_object_family_6_0c
		// (1228:4851) which silent-parks when availabilityState == CLOSED before
		// calling try_consume_commercial_venue_capacity. Retail's dispatch
		// (1228:40c0) has no silent-park branch — it only consumes capacity.
		if (object.linkedRecordIndex < 0) return;
		const record = world.sidecars[object.linkedRecordIndex] as
			| CommercialVenueRecord
			| undefined;
		if (!record || record.kind !== "commercial_venue") return;

		if (sim.familyCode !== FAMILY_RETAIL) {
			// Restaurant/fast-food state-0x20 silent-park (1228:499c):
			// if venue.availabilityState == VENUE_CLOSED, set sim to PARKED and
			// return — RNG already consumed by the gate, no transition emitted.
			if (record.availabilityState === VENUE_CLOSED) {
				sim.stateCode = STATE_PARKED;
				return;
			}
		}

		// try_consume_commercial_venue_capacity (11b0:1150). Eligibility-threshold
		// check only fires when the threshold is signed-negative; otherwise the
		// baseOffset comparison is skipped.
		if (record.remainingCapacity <= 0) return;
		if (
			record.eligibilityThreshold < 0 &&
			sim.baseOffset > 1 - record.eligibilityThreshold
		) {
			return;
		}

		// Binary `try_consume_commercial_venue_capacity` (11b0:1150) decrements
		// record+6 (remainingCapacity), increments record+7 (todayVisitCount,
		// staff-emit count), and increments record+0x10 (acquireCount). It
		// does NOT touch record+9 (currentPopulation) — that field is
		// incremented only by `acquire_commercial_venue_slot` (11b0:0d92),
		// which the binary calls at the per-stride state-0x20/0x60 handler
		// ONLY when route_result == 3 (i.e. the sim has actually arrived at
		// the venue floor). Mirror that here: at MORNING_GATE we run
		// try_consume but defer the currentPopulation increment to the
		// arrival site (handleCommercialMorningTransit /
		// handleCommercialSimArrival).
		// The phaseA/phaseB seed grow (binary `clamp_object_type_limit`) is
		// NOT called here — it fires only on the successful departure path
		// (state→0x27 PARKED via rc=3 in DEPARTURE / DEPARTURE_TRANSIT /
		// carrier arrival). See those sites below.
		record.remainingCapacity -= 1;
		record.lastAcquireTick = time.dayTick;
		record.todayVisitCount += 1;
		record.acquireCount += 1;
		record.visitCount += 1;

		// Route to home floor. Binary state-0x20 dispatch (1228:41cb) calls
		// try_consume first, then resolve_sim_route_between_floors, then
		// activates the retail shop on the success paths (route_result 0..3)
		// only when the venue is still DORMANT. On route_result == -1 the
		// binary reverts sim state fields and skips activation — capacity
		// stays decremented but no cashflow is added.
		const routeResult = resolveSimRouteBetweenFloors(
			world,
			sim,
			LOBBY_FLOOR,
			sim.floorAnchor,
			sim.floorAnchor > LOBBY_FLOOR ? 1 : 0,
			time,
		);
		if (routeResult === -1) {
			// Binary 1228:4297 route_result==-1 handler. Branch on venue[+0x02]:
			//   UNTENANTED (availabilityState == 0xff / VENUE_DORMANT, 1228:42d8):
			//     clear sim[+9]/sim[+0xc]/sim[+0xe] and CALLF 11b0:11de, which
			//     reverts try_consume (rec[+6]++, rec[+7]--, rec[+0x10]--). Sim
			//     stays in MORNING_GATE to retry next tick.
			//   TENANTED (1228:45dd): sim[+5]=0x27 (PARKED), clamp_object_type_limit.
			if (record.availabilityState === VENUE_DORMANT) {
				sim.tripCount = 0;
				sim.elapsedTicks = 0;
				sim.accumulatedTicks = 0;
				record.remainingCapacity += 1;
				record.todayVisitCount -= 1;
				record.acquireCount -= 1;
				record.visitCount -= 1;
			} else {
				sim.stateCode = STATE_PARKED;
			}
			return;
		}
		if (
			sim.familyCode === FAMILY_RETAIL &&
			record.availabilityState === VENUE_DORMANT
		) {
			activateRetailShop(world, object, record, ledger);
		}
		if (routeResult === 3) {
			// Same-floor venue (LOBBY-anchored): binary state-0x20 handler at
			// rc=3 (1228:4415 retail / 1228:4b37 ff) immediately calls
			// `acquire_commercial_venue_slot` (11b0:0d92):
			//   acquire returns -1 (UNAVAILABLE) → state 0x05 (DEPARTURE)
			//   acquire returns 2 (FULL)        → state 0x60 (MORNING_TRANSIT,
			//                                     re-attempt acquire next stride)
			//   acquire returns 3 (ACQUIRED)    → state 0x05 (DEPARTURE)
			// Acquire stamps sim+0xa = g_day_tick on success/full paths; that's
			// the dwell-start latch read by release.
			sim.destinationFloor = -1;
			sim.selectedFloor = sim.floorAnchor;
			// Owner sim arriving at their own venue: pass sim.familyCode as the
			// venue owner family so the type/variant gate suppresses the
			// acquireCount bump (binary 11b0:0f3a–0f55: same family ⇒ skip).
			// The owner's visit was already counted by
			// try_consume_commercial_venue_capacity at MORNING_GATE above.
			const acquireResult = tryAcquireOfficeVenueSlot(
				record,
				sim,
				time,
				sim.familyCode,
			);
			if (acquireResult === VENUE_SLOT_FULL) {
				sim.stateCode = STATE_MORNING_TRANSIT;
			} else {
				sim.stateCode = STATE_DEPARTURE;
				sim.elapsedTicks = 0;
				sim.lastDemandTick = time.dayTick;
			}
		} else {
			sim.stateCode = STATE_MORNING_TRANSIT;
		}
		return;
	}

	// --- Departure ---
	// Binary state-5 handler (1228:4517 retail / 1228:4bd7 ff+restaurant).
	// CALLF release_commercial_venue_slot (11b0:0fae) gate:
	//   g_day_tick - sim[+0x0a] < service_duration → return 0 (still dwelling).
	// sim[+0x0a] is stamped ONCE by acquire_commercial_venue_slot at service
	// start (handleCommercialSimArrival); binary never re-stamps it during
	// dwell and never rebases elapsed on state 0x05. Using dayTick - ldt as
	// the gate preserves elapsed=0 across the dwell so the return-leg stair
	// penalty of 35 is the only accumulated stress contribution.
	if (state === STATE_DEPARTURE) {
		if (time.dayTick - sim.lastDemandTick < COMMERCIAL_VENUE_DWELL_TICKS) {
			return;
		}
		// Binary retail/restaurant state-0x05 handler (1228:454e / 1228:4c0e)
		// calls `release_commercial_venue_slot` (11b0:0fae) which decrements
		// record+9 (currentPopulation) on the success path. Mirror that here
		// so the venue's effective capacity gate is restored for visiting
		// office workers / hotel guests / entertainment guests.
		if (object.linkedRecordIndex >= 0) {
			const venue = world.sidecars[object.linkedRecordIndex] as
				| CommercialVenueRecord
				| undefined;
			if (
				venue?.kind === "commercial_venue" &&
				venue.availabilityState !== VENUE_DORMANT &&
				venue.availabilityState !== VENUE_CLOSED &&
				venue.currentPopulation > 0
			) {
				venue.currentPopulation -= 1;
			}
		}
		// Binary retail/restaurant state-0x05 handler (1228:4517 / 1228:4bd7):
		// src = sim+7 lookup (current/home floor), tgt = LOBBY (0xa).
		// rc=-1 → fail; rc=0/1/2 → ok (in-transit); rc=3 → fail (binary collapses
		// the same-floor return into the fail tail at 1228:45a8 / 1228:4c68).
		const sourceFloor = sim.selectedFloor;
		const routeResult = resolveSimRouteBetweenFloors(
			world,
			sim,
			sourceFloor,
			LOBBY_FLOOR,
			LOBBY_FLOOR > sourceFloor ? 1 : 0,
			time,
		);
		if (routeResult === -1 || routeResult === 3) {
			// Binary 1228:4c9e: state-0x05/0x45 jump table routes rc=-1 and rc=3
			// to the PARKED (0x27) writer, not NIGHT_B (0x26).
			releaseServiceRequest(world, sim);
			sim.stateCode = STATE_PARKED;
			// Binary `clamp_object_type_limit` fires after `[BX+5]=0x27` on the
			// rc=3 (successful arrival at lobby) path only — see 1228:4cbf →
			// 1228:4d24. rc=-1 (route fail) sets PARKED but does NOT call clamp.
			if (routeResult === 3 && object.linkedRecordIndex >= 0) {
				const venue = world.sidecars[object.linkedRecordIndex] as
					| CommercialVenueRecord
					| undefined;
				if (venue?.kind === "commercial_venue") {
					incrementVenueSeed(venue, sim.familyCode, sim, world, time);
				}
			}
			return;
		}
		// Phase 1d-ii parity: resolve owns sim.selectedFloor/destinationFloor.
		// rc=0/1/2 → in-transit; next stride re-enters via STATE_DEPARTURE_TRANSIT.
		sim.stateCode = STATE_DEPARTURE_TRANSIT;
		return;
	}

	// --- Per-stride in-transit handlers ---
	// Binary jump table aliases: 0x60 (MORNING_TRANSIT) → state 0x20 handler;
	// 0x45 (DEPARTURE_TRANSIT) → state 0x05 handler. variantFlag=0 (distance
	// feedback off), but our resolver applies the penalty unconditionally.
	if (state === STATE_MORNING_TRANSIT) {
		handleCommercialMorningTransit(world, ledger, time, sim);
		return;
	}
	if (state === STATE_DEPARTURE_TRANSIT) {
		handleCommercialDepartureTransit(world, time, sim);
		return;
	}
}

/** Per-stride in-transit handler for STATE_MORNING_TRANSIT (0x60).
 * Binary alias of state 0x20 (1228:41cb retail / 1228:495c ff+restaurant) with
 * variantFlag=0. src = sim+7 (last leg endpoint or home), tgt = floorAnchor.
 *   rc=-1 → fail (park).
 *   rc=0/1/2 → stay in MORNING_TRANSIT; next stride re-resolves the next leg.
 *   rc=3 → arrived at home venue; activate retail (DORMANT case) and call
 *          acquire_commercial_venue_slot. ACQUIRED/UNAVAILABLE → state 0x05;
 *          FULL → stay in 0x60 (re-attempt acquire next stride).
 */
function handleCommercialMorningTransit(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
): void {
	const object = findObjectForSim(world, sim);
	const sourceFloor = sim.selectedFloor;
	const targetFloor = sim.floorAnchor;
	// Alias state 0x60 (MORNING_TRANSIT): in the binary `emit_distance_feedback`
	// is `0` here. Distance feedback was already applied by the base state
	// 0x20 dispatch.
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
		// Binary 1228:4297 -1 path: TENANTED venue → STATE_PARKED.
		sim.stateCode = STATE_PARKED;
		releaseServiceRequest(world, sim);
		return;
	}
	if (routeResult === 3) {
		// Arrived at home venue floor. Trip counter advanced inside resolve.
		// Binary state-0x60 handler at rc=3 (1228:4415 retail / 1228:4b37 ff)
		// calls `acquire_commercial_venue_slot` (11b0:0d92):
		//   acquire returns -1 (UNAVAILABLE) → state 0x05 (DEPARTURE)
		//   acquire returns 2  (FULL)        → stay in 0x60, retry next stride
		//   acquire returns 3  (ACQUIRED)    → state 0x05 (DEPARTURE)
		// Acquire stamps sim+0xa = g_day_tick on success/full paths; that's
		// the dwell-start latch read by release.
		sim.destinationFloor = -1;
		sim.selectedFloor = sim.floorAnchor;
		// Activate retail shop on arrival when venue is still DORMANT (binary
		// 1228:42d0 fires activate on success paths). This must happen before
		// the acquire call because acquire short-circuits to UNAVAILABLE on
		// dormant venues; activation flips availabilityState to OPEN first.
		let venue: CommercialVenueRecord | undefined;
		if (object && object.linkedRecordIndex >= 0) {
			const candidate = world.sidecars[object.linkedRecordIndex];
			if (candidate?.kind === "commercial_venue") {
				venue = candidate;
				if (
					sim.familyCode === FAMILY_RETAIL &&
					venue.availabilityState === VENUE_DORMANT
				) {
					activateRetailShop(world, object, venue, ledger);
				}
			}
		}
		if (!venue) {
			sim.stateCode = STATE_DEPARTURE;
			sim.elapsedTicks = 0;
			sim.lastDemandTick = time.dayTick;
			return;
		}
		// Owner sim arriving at their own venue via MORNING_TRANSIT: pass
		// sim.familyCode as the venue owner family so the type/variant gate
		// suppresses the acquireCount bump (already counted at MORNING_GATE).
		const acquireResult = tryAcquireOfficeVenueSlot(
			venue,
			sim,
			time,
			sim.familyCode,
		);
		if (acquireResult === VENUE_SLOT_FULL) {
			// Stay in STATE_MORNING_TRANSIT; next stride retries acquire.
			return;
		}
		sim.stateCode = STATE_DEPARTURE;
		sim.elapsedTicks = 0;
		sim.lastDemandTick = time.dayTick;
	}
	// rc=0/1/2: stay in STATE_MORNING_TRANSIT; next stride re-resolves.
}

/** Per-stride in-transit handler for STATE_DEPARTURE_TRANSIT (0x45).
 * Binary alias of state 0x05 (1228:4517 retail / 1228:4bd7 ff+restaurant) with
 * variantFlag=0. src = sim+7 (last leg endpoint), tgt = LOBBY.
 *   rc=-1 → fail (park).
 *   rc=0/1/2 → stay in DEPARTURE_TRANSIT; next stride re-resolves next leg.
 *   rc=3 → arrived at LOBBY; binary collapses to fail tail per state 0x05 table,
 *          but at the transit alias the same-floor result represents arrival —
 *          park the sim and release the service request.
 */
function handleCommercialDepartureTransit(
	world: WorldState,
	_time: TimeState,
	sim: SimRecord,
): void {
	const sourceFloor = sim.selectedFloor;
	// Alias state 0x45 (DEPARTURE_TRANSIT): in the binary `emit_distance_feedback`
	// is `0` here. Distance feedback was already applied by the base state
	// 0x05 dispatch.
	const routeResult = resolveSimRouteBetweenFloors(
		world,
		sim,
		sourceFloor,
		LOBBY_FLOOR,
		LOBBY_FLOOR > sourceFloor ? 1 : 0,
		_time,
		{ emitDistanceFeedback: false },
	);
	if (routeResult === -1) {
		// Binary 1228:4c9e: state-0x45 rc=-1 writes PARKED (0x27), not NIGHT_B.
		releaseServiceRequest(world, sim);
		sim.stateCode = STATE_PARKED;
		return;
	}
	if (routeResult === 3) {
		// Arrived at lobby. Trip counter advanced inside resolve.
		sim.destinationFloor = -1;
		sim.selectedFloor = LOBBY_FLOOR;
		sim.stateCode = STATE_PARKED;
		releaseServiceRequest(world, sim);
		// Binary `clamp_object_type_limit` fires after the rc=3 state→0x27
		// transition (1228:4cbf → 1228:4d24).
		const object = findObjectForSim(world, sim);
		if (object && object.linkedRecordIndex >= 0) {
			const venue = world.sidecars[object.linkedRecordIndex] as
				| CommercialVenueRecord
				| undefined;
			if (venue?.kind === "commercial_venue") {
				incrementVenueSeed(venue, sim.familyCode, sim, world, _time);
			}
		}
	}
	// rc=0/1/2: stay in STATE_DEPARTURE_TRANSIT; next stride re-resolves.
}

export function handleCommercialSimArrival(
	world: WorldState,
	sim: SimRecord,
	arrivalFloor: number,
	time: TimeState,
): void {
	if (
		sim.stateCode === STATE_MORNING_TRANSIT &&
		arrivalFloor === sim.floorAnchor
	) {
		// Binary `dispatch_destination_queue_entries` (1218:0883) writes
		// sim+7 = arrival_floor then invokes the family handler with
		// arg = arrival_floor. For state 0x60 (alias of 0x20) the resolve
		// call's same-floor branch (1218:0046, gated on is_passenger_route=1
		// which restaurant/fast-food/retail pass) advances the trip counters
		// before returning rc=3. Mirror that advance here because
		// dispatchSimArrival shortcuts the per-stride re-entry.
		//
		// At rc=3 the binary state-0x60 handler (1228:4415 retail / 1228:4b37
		// ff) calls `acquire_commercial_venue_slot` (11b0:0d92):
		//   acquire returns -1 (UNAVAILABLE) → state 0x05 (DEPARTURE)
		//   acquire returns 2  (FULL)        → stay in 0x60, retry next stride
		//   acquire returns 3  (ACQUIRED)    → state 0x05 (DEPARTURE)
		// Acquire stamps sim+0xa = g_day_tick on success/full paths; that's
		// the dwell-start latch read by release.
		advanceSimTripCounters(sim);
		sim.destinationFloor = -1;
		sim.selectedFloor = sim.floorAnchor;
		const object = findObjectForSim(world, sim);
		let venue: CommercialVenueRecord | undefined;
		if (object && object.linkedRecordIndex >= 0) {
			const candidate = world.sidecars[object.linkedRecordIndex];
			if (candidate?.kind === "commercial_venue") {
				venue = candidate;
			}
		}
		if (!venue) {
			sim.stateCode = STATE_DEPARTURE;
			sim.elapsedTicks = 0;
			sim.lastDemandTick = time.dayTick;
			return;
		}
		// Owner sim arriving at their own venue via carrier (handleCommercialSimArrival):
		// pass sim.familyCode as the venue owner family so the type/variant gate
		// suppresses the acquireCount bump (already counted at MORNING_GATE).
		const acquireResult = tryAcquireOfficeVenueSlot(
			venue,
			sim,
			time,
			sim.familyCode,
		);
		if (acquireResult === VENUE_SLOT_FULL) {
			// Stay in STATE_MORNING_TRANSIT; next stride re-attempts acquire.
			// Restore route fields the per-stride handler expects.
			sim.destinationFloor = -1;
			return;
		}
		sim.stateCode = STATE_DEPARTURE;
		// Binary: sim[+0x0A] (dword last_activity_tick) is stamped at dwell
		// start; release_commercial_venue_slot uses it to gate the service
		// duration check. Track via elapsedTicks (rebased each stride).
		sim.elapsedTicks = 0;
		sim.lastDemandTick = time.dayTick;
		return;
	}

	if (
		sim.stateCode === STATE_DEPARTURE_TRANSIT &&
		arrivalFloor === LOBBY_FLOOR
	) {
		// Binary 1228:4bd7 (state 0x45 alias of 0x05) calls resolve with
		// `is_passenger_route=1`. At carrier arrival sim+7 == LOBBY (target),
		// so resolve hits the same-floor branch (1218:0046) which calls
		// advance_sim_trip_counters because is_passenger_route != 0.
		advanceSimTripCounters(sim);
		sim.stateCode = STATE_PARKED;
		sim.selectedFloor = LOBBY_FLOOR;
		releaseServiceRequest(world, sim);
		// Binary `clamp_object_type_limit` fires after the state→0x27 write on
		// the carrier-arrival rc=3 path (dispatch_sim_behavior 1228:1ba6 case
		// 6/10/12).
		const object = findObjectForSim(world, sim);
		if (object && object.linkedRecordIndex >= 0) {
			const venue = world.sidecars[object.linkedRecordIndex] as
				| CommercialVenueRecord
				| undefined;
			if (venue?.kind === "commercial_venue") {
				incrementVenueSeed(venue, sim.familyCode, sim, world, time);
			}
		}
		return;
	}

	// Fallback: park
	sim.stateCode = STATE_NIGHT_B;
	clearSimRoute(sim);
}
