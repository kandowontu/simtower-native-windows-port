import { describe, expect, it } from "vitest";
import { FAMILY_HOTEL_SINGLE, FAMILY_HOUSEKEEPING } from "../resources";
import { createTimeState, type TimeState } from "../time";
import {
	createCommercialVenueBuckets,
	createEventState,
	createGateFlags,
	createMedicalServiceSlots,
	floorToY,
	GRID_HEIGHT,
	GRID_WIDTH,
	type PlacedObjectRecord,
	type SimRecord,
	type WorldState,
} from "../world";
import {
	handleHousekeepingSimArrival,
	processHousekeepingSim,
} from "./housekeeping";
import {
	HK_POST_CLAIM_COUNTDOWN,
	HK_SEARCHING_SENTINEL,
	HK_STATE_COUNTDOWN,
	HK_STATE_ROUTE_TO_CANDIDATE_TRANSIT,
	HK_STATE_ROUTE_TO_TARGET,
	HK_STATE_SEARCH,
} from "./states";

function makeWorld(): WorldState {
	return {
		towerId: "t",
		name: "T",
		width: GRID_WIDTH,
		height: GRID_HEIGHT,
		lobbyHeight: 1,
		lobbyMode: "perfect-parity",
		starCount: 1,
		currentPopulation: 0,
		currentPopulationBuckets: {},
		commercialVenueBuckets: createCommercialVenueBuckets(),
		gateFlags: createGateFlags(),
		cells: {},
		cellToAnchor: {},
		overlays: {},
		overlayToAnchor: {},
		placedObjects: {},
		sidecars: [],
		sims: [],
		carriers: [],
		specialLinks: [],
		specialLinkRecords: [],
		floorWalkabilityFlags: new Array(GRID_HEIGHT).fill(0),
		transferGroupEntries: [],
		transferGroupCache: new Array(GRID_HEIGHT).fill(0),
		rngState: 1,
		rngCallCount: 0,
		eventState: createEventState(),
		parkingDemandLog: [],
		medicalServiceSlots: createMedicalServiceSlots(),
		pendingNotifications: [],
		pendingPrompts: [],
	};
}

function addHotelRoom(
	world: WorldState,
	x: number,
	floor: number,
	unitStatus: number,
): PlacedObjectRecord {
	const y = floorToY(floor);
	const object: PlacedObjectRecord = {
		leftTileIndex: x,
		rightTileIndex: x + 3,
		objectTypeCode: FAMILY_HOTEL_SINGLE,
		unitStatus,
		auxValueOrTimer: 0,
		linkedRecordIndex: -1,
		dirtyFlag: 0,
		occupiedFlag: 0,
		evalLevel: 0,
		evalScore: -1,
		rentLevel: 1,
		activationTickCount: 0,
		housekeepingClaimedFlag: 0,
	};
	world.placedObjects[`${x},${y}`] = object;
	return object;
}

function makeHousekeeper(floor: number, homeColumn = 0): SimRecord {
	return {
		floorAnchor: floor,
		homeColumn,
		baseOffset: 0,
		facilitySlot: 0,
		familyCode: FAMILY_HOUSEKEEPING,
		stateCode: HK_STATE_SEARCH,
		route: { mode: "idle" },
		selectedFloor: floor,
		originFloor: floor,
		destinationFloor: -1,
		venueReturnState: 0,
		queueTick: 0,
		elapsedTicks: 0,
		transitTicksRemaining: 0,
		lastDemandTick: -1,
		tripCount: 0,
		accumulatedTicks: 0,
		targetRoomFloor: HK_SEARCHING_SENTINEL,
		targetRoomColumn: -1,
		spawnFloor: floor,
		postClaimCountdown: 0,
		encodedTargetFloor: 0,
		commercialVenueSlot: -1,
	};
}

function timeAt(dayTick: number): TimeState {
	return { ...createTimeState(), dayTick };
}

describe("housekeeping helper", () => {
	it("stays searching when no turnover-band hotel rooms exist", () => {
		const world = makeWorld();
		const sim = makeHousekeeper(12);
		world.sims.push(sim);
		processHousekeepingSim(world, timeAt(100), sim);
		// Binary state-0: find_matching returns -1 → sim+6 = -1 → state = 1.
		// Observable effect: no room claim occurred and target remains sentinel.
		expect(sim.targetRoomFloor).toBe(HK_SEARCHING_SENTINEL);
		expect(sim.postClaimCountdown).toBe(0);
	});

	it("skips rooms whose floor does not match the claimant's floor-class (%6)", () => {
		const world = makeWorld();
		// baseOffset=0 → floorClass = 0. Add a turnover room on floor 13 (class 1).
		const room = addHotelRoom(world, 20, 13, 0x28);
		const sim = makeHousekeeper(12);
		world.sims.push(sim);
		processHousekeepingSim(world, timeAt(100), sim);
		// No matching candidate → state transitions to HK_STATE_ROUTE_TO_CANDIDATE
		// but room is not claimed.
		expect(room.housekeepingClaimedFlag).toBe(0);
		expect(sim.targetRoomFloor).toBe(HK_SEARCHING_SENTINEL);
	});

	it("claims a same-floor turnover room immediately before the cutoff", () => {
		const world = makeWorld();
		const room = addHotelRoom(world, 20, 12, 0x28);
		const sim = makeHousekeeper(12);
		world.sims.push(sim);
		processHousekeepingSim(world, timeAt(100), sim);

		expect(sim.stateCode).toBe(HK_STATE_COUNTDOWN);
		expect(sim.targetRoomFloor).toBe(12);
		expect(sim.postClaimCountdown).toBe(HK_POST_CLAIM_COUNTDOWN);
		expect(room.housekeepingClaimedFlag).toBe(1);
		// Binary `activate_selected_vacant_unit` (1158:02e2): unit_status is 0x18
		// pre-daypart-4, 0x20 otherwise. The test's timeAt(100) is daypart 0.
		expect(room.unitStatus).toBe(0x18);
		expect(sim.targetRoomColumn).toBe(20);
	});

	it("refuses to promote once day_tick has reached the cutoff (1500)", () => {
		const world = makeWorld();
		const room = addHotelRoom(world, 20, 12, 0x28);
		const sim = makeHousekeeper(12);
		world.sims.push(sim);
		processHousekeepingSim(world, timeAt(1500), sim);
		expect(sim.stateCode).toBe(HK_STATE_SEARCH);
		expect(room.housekeepingClaimedFlag).toBe(0);
		expect(room.unitStatus).toBe(0x28);
	});

	it("upward scan reaches the spawn floor before any downward candidate", () => {
		const world = makeWorld();
		// Both floors are class 0 (%6 == 0). Spawn-floor candidate should win
		// because the upward scan starts at spawn_floor inclusive.
		const spawnFloorRoom = addHotelRoom(world, 10, 12, 0x28);
		const downwardRoom = addHotelRoom(world, 10, 6, 0x28);
		const sim = makeHousekeeper(12);
		world.sims.push(sim);
		processHousekeepingSim(world, timeAt(50), sim);
		expect(sim.stateCode).toBe(HK_STATE_COUNTDOWN);
		expect(spawnFloorRoom.housekeepingClaimedFlag).toBe(1);
		expect(downwardRoom.housekeepingClaimedFlag).toBe(0);
	});

	it("within a matching floor, picks the leftmost turnover-band slot", () => {
		const world = makeWorld();
		addHotelRoom(world, 40, 12, 0x28);
		const leftmost = addHotelRoom(world, 10, 12, 0x30);
		const sim = makeHousekeeper(12);
		world.sims.push(sim);
		processHousekeepingSim(world, timeAt(50), sim);
		expect(sim.stateCode).toBe(HK_STATE_COUNTDOWN);
		expect(leftmost.housekeepingClaimedFlag).toBe(1);
	});

	it("post-claim countdown decrements from 3 to 0 then returns to search", () => {
		const world = makeWorld();
		const room = addHotelRoom(world, 20, 12, 0x28);
		const sim = makeHousekeeper(12);
		world.sims.push(sim);
		processHousekeepingSim(world, timeAt(50), sim);
		expect(sim.postClaimCountdown).toBe(3);

		// Binary state-2 is check-before-decrement, so 4 strides in state 2 for
		// a starting countdown of 3 (values 3→2→1→0 then exit on the 4th).
		for (let i = 1; i <= 4; i++) {
			processHousekeepingSim(world, timeAt(50 + i), sim);
		}
		expect(sim.stateCode).toBe(HK_STATE_SEARCH);
		expect(sim.postClaimCountdown).toBe(0);
		// Flag stays set — hotel deactivation clears it, not housekeeping itself.
		expect(room.housekeepingClaimedFlag).toBe(1);
	});

	it("cross-floor candidate transitions through route-to-candidate states", () => {
		const world = makeWorld();
		addHotelRoom(world, 20, 18, 0x28);
		const sim = makeHousekeeper(12);
		world.sims.push(sim);
		processHousekeepingSim(world, timeAt(50), sim);
		// No carriers/routes available in this test world → route-fail → reset.
		expect(sim.stateCode).toBe(HK_STATE_SEARCH);
	});

	it("arrival handler promotes once sim reaches the target room floor", () => {
		const world = makeWorld();
		const room = addHotelRoom(world, 20, 12, 0x28);
		const sim = makeHousekeeper(18);
		// Simulate mid-commute state: searched upward, committed floor 12, now en route.
		sim.stateCode = HK_STATE_ROUTE_TO_CANDIDATE_TRANSIT;
		sim.spawnFloor = 12;
		sim.targetRoomFloor = 12;
		sim.targetRoomColumn = 20;
		sim.floorAnchor = 12; // arrival snaps floorAnchor forward in tests
		world.sims.push(sim);
		handleHousekeepingSimArrival(world, timeAt(50), sim, 12);
		expect(sim.stateCode).toBe(HK_STATE_COUNTDOWN);
		expect(room.housekeepingClaimedFlag).toBe(1);
	});

	it("ignores arrivals at unrelated floors", () => {
		const world = makeWorld();
		addHotelRoom(world, 20, 12, 0x28);
		const sim = makeHousekeeper(18);
		sim.stateCode = HK_STATE_ROUTE_TO_CANDIDATE_TRANSIT;
		sim.spawnFloor = 12;
		sim.targetRoomFloor = 12;
		world.sims.push(sim);
		handleHousekeepingSimArrival(world, timeAt(50), sim, 30);
		expect(sim.stateCode).toBe(HK_STATE_ROUTE_TO_CANDIDATE_TRANSIT);
	});

	it("route-to-target state ignores expired day-tick and resets", () => {
		const world = makeWorld();
		addHotelRoom(world, 20, 12, 0x28);
		const sim = makeHousekeeper(12);
		sim.stateCode = HK_STATE_ROUTE_TO_TARGET;
		sim.targetRoomFloor = 12;
		world.sims.push(sim);
		processHousekeepingSim(world, timeAt(1600), sim);
		expect(sim.stateCode).toBe(HK_STATE_SEARCH);
	});
});
