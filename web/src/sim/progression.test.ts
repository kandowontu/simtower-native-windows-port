import { describe, expect, it } from "vitest";
import {
	checkStarAdvancementConditions,
	computeStarCountFromPopulation,
	tryAdvanceStarCount,
} from "./progression";
import { STAR_THRESHOLDS } from "./resources";
import { createTimeState, type TimeState } from "./time";
import {
	createCommercialVenueBuckets,
	createEventState,
	createGateFlags,
	createMedicalServiceSlots,
	GRID_HEIGHT,
	GRID_WIDTH,
	type PlacedObjectRecord,
	type WorldState,
} from "./world";

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

function addObjectWithActivation(
	world: WorldState,
	key: string,
	ledgerContribution: number,
): PlacedObjectRecord {
	const object: PlacedObjectRecord = {
		leftTileIndex: 0,
		rightTileIndex: 0,
		objectTypeCode: 0,
		unitStatus: 0,
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
	world.placedObjects[key] = object;
	// Mirrors `add_to_primary_family_ledger_bucket` (1068:07f7): the binary
	// stores the ledger total in g_primary_family_ledger_total; this test
	// helper bumps the same field so star-tier checks see the contribution.
	world.currentPopulation += ledgerContribution;
	return object;
}

function lateAfternoonTime(): TimeState {
	const t = createTimeState();
	t.dayTick = 2000;
	t.daypartIndex = 5;
	t.dayCounter = 0;
	t.weekendFlag = 0;
	return t;
}

describe("computeTowerTierFromLedger", () => {
	it("returns tier 1 for empty tower", () => {
		const world = makeWorld();
		expect(computeStarCountFromPopulation(world)).toBe(1);
	});

	it("returns tier 2 once first threshold is met", () => {
		const world = makeWorld();
		// Binary `compute_tower_tier_from_ledger` (1148:041d) compares
		// `total < THRESHOLD` for the lower-tier branch, so reaching the
		// threshold value (e.g. 300) is enough to advance.
		addObjectWithActivation(world, "a", STAR_THRESHOLDS[0]);
		expect(computeStarCountFromPopulation(world)).toBe(2);
	});

	it("stays in lower tier just below the threshold", () => {
		const world = makeWorld();
		addObjectWithActivation(world, "a", STAR_THRESHOLDS[0] - 1);
		expect(computeStarCountFromPopulation(world)).toBe(1);
	});

	it("sums contributions to the primary ledger total", () => {
		const world = makeWorld();
		addObjectWithActivation(world, "a", 200);
		expect(computeStarCountFromPopulation(world)).toBe(1);
		addObjectWithActivation(world, "b", 100);
		expect(computeStarCountFromPopulation(world)).toBe(2);
	});

	it("returns tier 6 when ledger reaches the top threshold", () => {
		const world = makeWorld();
		addObjectWithActivation(world, "a", STAR_THRESHOLDS[4]);
		expect(computeStarCountFromPopulation(world)).toBe(6);
	});
});

describe("checkStarAdvancementConditions", () => {
	it("1→2 has no qualitative gate", () => {
		const world = makeWorld();
		expect(checkStarAdvancementConditions(world, lateAfternoonTime())).toBe(
			true,
		);
	});

	it("2→3 requires securityPlaced", () => {
		const world = makeWorld();
		world.starCount = 2;
		expect(checkStarAdvancementConditions(world, lateAfternoonTime())).toBe(
			false,
		);
		world.gateFlags.securityPlaced = 1;
		expect(checkStarAdvancementConditions(world, lateAfternoonTime())).toBe(
			true,
		);
	});

	it("3→4 requires all office-tier gates + daypart + weekendFlag", () => {
		const world = makeWorld();
		world.starCount = 3;
		world.gateFlags.officePlaced = 1;
		world.gateFlags.recyclingAdequate = 1;
		world.gateFlags.officeServiceOk = 1;
		world.gateFlags.officeServiceOkMedical = 1;
		world.gateFlags.routesViable = 1;

		const time = lateAfternoonTime();
		expect(checkStarAdvancementConditions(world, time)).toBe(true);

		// Missing any flag blocks the gate
		world.gateFlags.officeServiceOk = 0;
		expect(checkStarAdvancementConditions(world, time)).toBe(false);
		world.gateFlags.officeServiceOk = 1;

		// daypart < 4 blocks
		const earlyTime = { ...time, daypartIndex: 3 };
		expect(checkStarAdvancementConditions(world, earlyTime)).toBe(false);

		// weekendFlag != 0 blocks
		const weekendTime = { ...time, weekendFlag: 1 };
		expect(checkStarAdvancementConditions(world, weekendTime)).toBe(false);
	});

	it("4→5 requires metroPlaced (not officeServiceOk)", () => {
		const world = makeWorld();
		world.starCount = 4;
		world.gateFlags.metroPlaced = 1;
		world.gateFlags.recyclingAdequate = 1;
		world.gateFlags.officeServiceOkMedical = 1;
		world.gateFlags.routesViable = 1;

		const time = lateAfternoonTime();
		expect(checkStarAdvancementConditions(world, time)).toBe(true);

		world.gateFlags.metroPlaced = 0;
		expect(checkStarAdvancementConditions(world, time)).toBe(false);
	});

	it("returns false at starCount >= 5 (Tower rank is cathedral-only)", () => {
		const world = makeWorld();
		world.starCount = 5;
		expect(checkStarAdvancementConditions(world, lateAfternoonTime())).toBe(
			false,
		);
	});
});

describe("tryAdvanceStarCount", () => {
	it("advances 1→2 once the activity threshold is met", () => {
		const world = makeWorld();
		addObjectWithActivation(world, "a", STAR_THRESHOLDS[0]);
		expect(tryAdvanceStarCount(world, lateAfternoonTime())).toBe(true);
		expect(world.starCount).toBe(2);
		expect(world.pendingNotifications).toEqual([
			{ kind: "star_advanced", message: "2" },
		]);
	});

	it("does nothing if activity threshold not met", () => {
		const world = makeWorld();
		addObjectWithActivation(world, "a", 10);
		expect(tryAdvanceStarCount(world, lateAfternoonTime())).toBe(false);
		expect(world.starCount).toBe(1);
		expect(world.pendingNotifications).toHaveLength(0);
	});

	it("does nothing if qualitative gate fails (2→3 without security)", () => {
		const world = makeWorld();
		world.starCount = 2;
		addObjectWithActivation(world, "a", STAR_THRESHOLDS[1]);
		expect(tryAdvanceStarCount(world, lateAfternoonTime())).toBe(false);
		expect(world.starCount).toBe(2);
	});

	it("resets officeServiceOk on advancement", () => {
		const world = makeWorld();
		world.starCount = 3;
		addObjectWithActivation(world, "a", STAR_THRESHOLDS[2]);
		world.gateFlags.officePlaced = 1;
		world.gateFlags.recyclingAdequate = 1;
		world.gateFlags.officeServiceOk = 1;
		world.gateFlags.officeServiceOkMedical = 1;
		world.gateFlags.routesViable = 1;
		expect(tryAdvanceStarCount(world, lateAfternoonTime())).toBe(true);
		expect(world.starCount).toBe(4);
		expect(world.gateFlags.officeServiceOk).toBe(0);
	});

	it("caps at 5 stars (Tower rank reached via cathedral only)", () => {
		const world = makeWorld();
		world.starCount = 5;
		addObjectWithActivation(world, "a", STAR_THRESHOLDS[4]);
		expect(tryAdvanceStarCount(world, lateAfternoonTime())).toBe(false);
		expect(world.starCount).toBe(5);
	});
});
