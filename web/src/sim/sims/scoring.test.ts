import { describe, expect, it } from "vitest";
import { createLedgerState } from "../ledger";
import { FAMILY_OFFICE } from "../resources";
import { runSimulationDayScheduler } from "../tick/day-scheduler";
import { createTimeState } from "../time";
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
import { recomputeAllObjectOperationalStatus } from "./scoring";
import { STATE_MORNING_GATE } from "./states";

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

function addOffice(
	world: WorldState,
	x: number,
	floor: number,
): PlacedObjectRecord {
	const y = floorToY(floor);
	const object: PlacedObjectRecord = {
		leftTileIndex: x,
		rightTileIndex: x + 5,
		objectTypeCode: FAMILY_OFFICE,
		unitStatus: 0,
		auxValueOrTimer: 1,
		linkedRecordIndex: -1,
		dirtyFlag: 1,
		occupiedFlag: 1,
		evalLevel: 0xff,
		evalScore: -1,
		rentLevel: 1,
		activationTickCount: 0,
		housekeepingClaimedFlag: 0,
	};
	world.cells[`${x},${y}`] = "office";
	world.placedObjects[`${x},${y}`] = object;
	return object;
}

function addOfficeSim(
	world: WorldState,
	x: number,
	floor: number,
	tripCount: number,
	accumulatedTicks: number,
): SimRecord {
	const sim: SimRecord = {
		floorAnchor: floor,
		homeColumn: x,
		baseOffset: 0,
		facilitySlot: 0,
		familyCode: FAMILY_OFFICE,
		stateCode: STATE_MORNING_GATE,
		route: { mode: "idle" },
		selectedFloor: floor,
		originFloor: floor,
		destinationFloor: -1,
		venueReturnState: 0,
		queueTick: 0,
		elapsedTicks: 0,
		transitTicksRemaining: 0,
		lastDemandTick: -1,
		tripCount,
		accumulatedTicks,
		targetRoomFloor: -1,
		targetRoomColumn: -1,
		spawnFloor: floor,
		postClaimCountdown: 0,
		encodedTargetFloor: 0,
		commercialVenueSlot: -1,
	};
	world.sims.push(sim);
	return sim;
}

describe("operational scoring cadence", () => {
	it("keeps occupied offices unscored until they have trip data", () => {
		const world = makeWorld();
		const office = addOffice(world, 10, 1);
		addOfficeSim(world, 10, 1, 0, 0);

		recomputeAllObjectOperationalStatus(world);

		expect(office.evalScore).toBe(-1);
		expect(office.evalLevel).toBe(0xff);
	});

	it("runs the all-object eval sweep at the tick-2533 checkpoint", () => {
		const world = makeWorld();
		const office = addOffice(world, 10, 1);
		addOfficeSim(world, 10, 1, 1, 20);
		const ledger = createLedgerState(2_000_000);
		const time = { ...createTimeState(), dayTick: 0x9e4 };

		runSimulationDayScheduler(world, ledger, time);

		expect(office.evalScore).toBe(3);
		expect(office.evalLevel).toBe(2);
	});
});
