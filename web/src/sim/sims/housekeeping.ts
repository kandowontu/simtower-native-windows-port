import { FAMILY_HOUSEKEEPING } from "../resources";
import type { TimeState } from "../time";
import {
	type PlacedObjectRecord,
	type SimRecord,
	type WorldState,
	yToFloor,
} from "../world";
import { resolveSimRouteBetweenFloors } from "./index";
import {
	HK_CLAIM_DAY_TICK_CUTOFF,
	HK_FLOOR_CLASS_MOD,
	HK_POST_CLAIM_COUNTDOWN,
	HK_SEARCHING_SENTINEL,
	HK_STATE_COUNTDOWN,
	HK_STATE_ROUTE_TO_CANDIDATE,
	HK_STATE_ROUTE_TO_CANDIDATE_TRANSIT,
	HK_STATE_ROUTE_TO_TARGET,
	HK_STATE_SEARCH,
	HOTEL_FAMILIES,
	STATE_ARRIVED,
	STATE_HOTEL_PARKED,
} from "./states";

const HOTEL_TURNOVER_STATUS = new Set([0x28, 0x30]);

interface FoundRoom {
	floor: number;
	object: PlacedObjectRecord;
	subtypeByte: number;
}

function isClaimableHotelRoom(object: PlacedObjectRecord): boolean {
	if (!HOTEL_FAMILIES.has(object.objectTypeCode)) return false;
	return HOTEL_TURNOVER_STATUS.has(object.unitStatus);
}

/**
 * Scan upward from `spawnFloor` to the top of the tower, then downward from
 * `spawnFloor - 1`. Within each floor satisfying `floor % 6 === floorClass`,
 * scan room slots in ascending subtype/slot (x) order and return the first
 * turnover-band (`0x28`/`0x30`) hotel-family slot. Returns null if none found.
 */
function findVacantHotelRoomForClaim(
	world: WorldState,
	spawnFloor: number,
	floorClass: number,
): FoundRoom | null {
	const entries = Object.entries(world.placedObjects);
	const byFloor = new Map<number, Array<[number, PlacedObjectRecord]>>();
	for (const [key, object] of entries) {
		const [x, y] = key.split(",").map(Number);
		const floor = yToFloor(y);
		const list = byFloor.get(floor) ?? [];
		list.push([x, object]);
		byFloor.set(floor, list);
	}
	for (const list of byFloor.values()) list.sort((a, b) => a[0] - b[0]);

	const tryFloor = (floor: number): FoundRoom | null => {
		if (floor < 0 || floor >= world.height) return null;
		if (
			((floor % HK_FLOOR_CLASS_MOD) + HK_FLOOR_CLASS_MOD) %
				HK_FLOOR_CLASS_MOD !==
			floorClass
		) {
			return null;
		}
		const list = byFloor.get(floor);
		if (!list) return null;
		for (const [x, object] of list) {
			if (isClaimableHotelRoom(object)) {
				return { floor, object, subtypeByte: x & 0xff };
			}
		}
		return null;
	};

	for (let floor = spawnFloor; floor < world.height; floor++) {
		const hit = tryFloor(floor);
		if (hit) return hit;
	}
	for (let floor = spawnFloor - 1; floor >= 0; floor--) {
		const hit = tryFloor(floor);
		if (hit) return hit;
	}
	return null;
}

function attemptRouteToFloor(
	world: WorldState,
	sim: SimRecord,
	targetFloor: number,
	time: TimeState,
): number {
	// Binary `sim+0` is the HK helper's *current* floor (updated on arrival).
	// TS splits that into two fields: `floorAnchor` stays fixed as the tile
	// linkage key (required by `rebuildRuntimeSims`), while `selectedFloor`
	// tracks current position as the HK helper roams.
	const direction = targetFloor > sim.selectedFloor ? 1 : 0;
	// Binary 1228:620f / 1228:6320: housekeeping passes `is_passenger_route = 0`
	// AND `emit_distance_feedback = 0`. The post-resolve delay/trip-counter
	// writes are no-ops for housekeeping anyway (advanceSimTripCounters and
	// addDelayToCurrentSim early-return for FAMILY_HOUSEKEEPING), but pass the
	// flags explicitly to mirror the binary call shape.
	return resolveSimRouteBetweenFloors(
		world,
		sim,
		sim.selectedFloor,
		targetFloor,
		direction,
		time,
		{ isPassengerRoute: false, emitDistanceFeedback: false },
	);
}

function promoteClaim(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
	object: PlacedObjectRecord | undefined,
): void {
	// Binary `activate_selected_vacant_unit` (1158:02e2) bails on:
	//   - room_family ∉ [3,5] (1158:0381 / 03b3)
	//   - room.unitStatus ∉ {0x28, 0x30} (1158:03e5 / 0415)
	// When it bails, no fields are written. The caller (state-1 result-3 arm
	// at 1228:6264) unconditionally sets sim+5 = 2 / sim+0xa = 3 afterward,
	// so the helper still enters state-2 with countdown 3. Mirror that here.
	const claimable = object !== undefined && isClaimableHotelRoom(object);
	if (claimable && object !== undefined) {
		// Binary `activate_selected_vacant_unit` (1158:02e2): the cleaned-room
		// unit_status is 0x18 while daypart_index < 4, and 0x20 otherwise, i.e.
		// the same morning/evening VACANT bucket `place_object` would have picked
		// when the room was first placed. The room is *not* re-randomized to the
		// occupied trip-counter band — that happens when the new guest arrives.
		object.unitStatus = time.daypartIndex < 4 ? 0x18 : 0x20;
		object.housekeepingClaimedFlag = 1;
		// The binary also flips the first guest occupant of the cleaned room to
		// state 3 (ARRIVED) so the hotel-family state machine picks up the new
		// occupancy on its next stride.
		const newOccupant = world.sims.find(
			(candidate) =>
				candidate.familyCode === object.objectTypeCode &&
				candidate.floorAnchor === sim.targetRoomFloor &&
				candidate.homeColumn === object.leftTileIndex &&
				candidate.baseOffset === 0,
		);
		if (newOccupant) newOccupant.stateCode = STATE_ARRIVED;
	}
	sim.postClaimCountdown = HK_POST_CLAIM_COUNTDOWN;
	sim.stateCode = HK_STATE_COUNTDOWN;
}

function resetToSearch(sim: SimRecord): void {
	sim.stateCode = HK_STATE_SEARCH;
	sim.targetRoomFloor = HK_SEARCHING_SENTINEL;
	sim.targetRoomColumn = -1;
	sim.postClaimCountdown = 0;
}

/**
 * Binary `flag_selected_unit_unavailable` (1158:04e2): called when the HK
 * helper's post-claim countdown (sim+10) reaches 0 in state 2. Looks up the
 * first occupant (base_offset=0) of the claimed hotel-family room and writes
 * stateCode = 0x24 (STATE_HOTEL_PARKED) so the hotel state machine picks up
 * the room on its next stride, and sets the object's "dirty" byte to 1.
 */
function flagSelectedUnitUnavailable(world: WorldState, sim: SimRecord): void {
	if (sim.targetRoomFloor < 0 || sim.targetRoomColumn < 0) return;
	const y = world.height - 1 - sim.targetRoomFloor;
	const room = world.placedObjects[`${sim.targetRoomColumn},${y}`];
	if (!room) return;
	if (!HOTEL_FAMILIES.has(room.objectTypeCode)) return;
	const firstOccupant = world.sims.find(
		(candidate) =>
			candidate.familyCode === room.objectTypeCode &&
			candidate.floorAnchor === sim.targetRoomFloor &&
			candidate.homeColumn === room.leftTileIndex &&
			candidate.baseOffset === 0,
	);
	if (firstOccupant) firstOccupant.stateCode = STATE_HOTEL_PARKED;
}

export function processHousekeepingSim(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	if (sim.familyCode !== FAMILY_HOUSEKEEPING) return;

	switch (sim.stateCode) {
		case HK_STATE_SEARCH: {
			// Binary state-0 handler only progresses while day_tick < 1500.
			if (time.dayTick >= HK_CLAIM_DAY_TICK_CUTOFF) return;
			if (sim.spawnFloor < 0) sim.spawnFloor = sim.floorAnchor;

			// Binary `find_matching_vacant_unit_floor` (1158:0000) uses
			// `get_current_sim_state_word(sim)` — the u16 at sim+2, populated by
			// `initialize_runtime_entities_for_object_span` with the occupant's
			// span index (0..population-1). For housekeeping (pop=6), this is the
			// TS `baseOffset` field, which ranges 0..5 and acts as the floor-class
			// filter: each of the 6 HK helpers in a tile services a distinct
			// `floor % 6` residue.
			const floorClass =
				((sim.baseOffset % HK_FLOOR_CLASS_MOD) + HK_FLOOR_CLASS_MOD) %
				HK_FLOOR_CLASS_MOD;
			const candidate = findVacantHotelRoomForClaim(
				world,
				sim.spawnFloor,
				floorClass,
			);
			if (!candidate) {
				// Binary state-0: find_matching returns -1 → sim+6 = -1 → state = 1.
				// State 1 will next stride route from current_floor to sim+7 (spawn);
				// same-floor → reset → state 0, producing the observed 0↔1 oscillation.
				sim.targetRoomFloor = HK_SEARCHING_SENTINEL;
				sim.stateCode = HK_STATE_ROUTE_TO_CANDIDATE;
				return;
			}
			// Binary falls through to the state-0/3 shared route resolution in the
			// same stride: state becomes 3 and the route to the target floor is
			// initiated immediately (result 0/1/2 → state 3, result 3 → claim,
			// result -1 → reset). Mirror that by advancing to state 3 and running
			// the state-3 body on the current stride.
			// Binary `find_matching_vacant_unit_floor` (1158:0000) writes
			// entity[+0xc] to identify the chosen room (slot rank within the
			// floor). On arrival the state-3 handler hands that stored
			// identifier to `activate_selected_vacant_unit`, which bails if the
			// specific room's family/phase don't match. Stash the room's
			// column at search time and look it up directly on arrival —
			// re-scanning the floor for *any* claimable room lets us steal a
			// room another helper is already routing to.
			sim.targetRoomFloor = candidate.floor;
			sim.targetRoomColumn = candidate.object.leftTileIndex;
			sim.stateCode = HK_STATE_ROUTE_TO_TARGET;
			tryClaimOnCurrentFloor(world, time, sim);
			return;
		}

		case HK_STATE_ROUTE_TO_CANDIDATE:
		case HK_STATE_ROUTE_TO_CANDIDATE_TRANSIT: {
			// Binary state-1/4 handler (jump table @ 1228:6404 / 1228:640a both
			// → 0x62ec): both call resolve(sim+7=spawn, sim+0a=arg) every
			// stride. rc=0/1/2 → state=4 (stay in transit; per-stride re-resolve
			// advances the leg). rc=-1/3 → state=0 (reset to search).
			if (sim.selectedFloor === sim.spawnFloor) {
				if (sim.targetRoomFloor === HK_SEARCHING_SENTINEL) {
					resetToSearch(sim);
					return;
				}
				sim.stateCode = HK_STATE_ROUTE_TO_TARGET;
				tryClaimOnCurrentFloor(world, time, sim);
				return;
			}
			const result = attemptRouteToFloor(world, sim, sim.spawnFloor, time);
			if (result === -1) {
				resetToSearch(sim);
				return;
			}
			if (result === 3) {
				sim.stateCode = HK_STATE_ROUTE_TO_TARGET;
				tryClaimOnCurrentFloor(world, time, sim);
				return;
			}
			if (result !== 0) {
				sim.stateCode = HK_STATE_ROUTE_TO_CANDIDATE_TRANSIT;
			}
			return;
		}

		case HK_STATE_ROUTE_TO_TARGET:
			tryClaimOnCurrentFloor(world, time, sim);
			return;

		case HK_STATE_COUNTDOWN: {
			// Binary state-2 (1228:602b case 2): `if (sim+10 != 0) { sim+10--;
			// return; } else { flag_unavailable; sim+5 = 0; }`. The check-first
			// order gives 4 strides in state 2 for a starting value of 3, not 3.
			if (sim.postClaimCountdown !== 0) {
				sim.postClaimCountdown -= 1;
				return;
			}
			flagSelectedUnitUnavailable(world, sim);
			resetToSearch(sim);
			return;
		}

		default:
			resetToSearch(sim);
	}
}

function tryClaimOnCurrentFloor(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	if (sim.selectedFloor !== sim.targetRoomFloor) {
		const result = attemptRouteToFloor(world, sim, sim.targetRoomFloor, time);
		if (result === -1) {
			resetToSearch(sim);
			return;
		}
		// Queued / in-transit: stay in HK_STATE_ROUTE_TO_TARGET.
		return;
	}
	if (time.dayTick >= HK_CLAIM_DAY_TICK_CUTOFF) {
		resetToSearch(sim);
		return;
	}
	// Look up the specific room targeted at search time, not whatever's
	// currently claimable on this floor. promoteClaim handles the case where
	// the room's status no longer qualifies (binary's activate bails, but the
	// caller still transitions the helper to state 2).
	const y = world.height - 1 - sim.targetRoomFloor;
	const room =
		sim.targetRoomColumn >= 0
			? world.placedObjects[`${sim.targetRoomColumn},${y}`]
			: undefined;
	promoteClaim(world, time, sim, room);
}

export function handleHousekeepingSimArrival(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
	arrivalFloor: number,
): void {
	sim.selectedFloor = arrivalFloor;
	sim.destinationFloor = -1;

	if (
		sim.stateCode === HK_STATE_ROUTE_TO_CANDIDATE ||
		sim.stateCode === HK_STATE_ROUTE_TO_CANDIDATE_TRANSIT
	) {
		if (arrivalFloor !== sim.spawnFloor) return;
		sim.stateCode = HK_STATE_ROUTE_TO_TARGET;
		tryClaimOnCurrentFloor(world, time, sim);
		return;
	}

	if (sim.stateCode === HK_STATE_ROUTE_TO_TARGET) {
		if (arrivalFloor !== sim.targetRoomFloor) return;
		tryClaimOnCurrentFloor(world, time, sim);
		return;
	}
}
