import { describe, expect, it } from "vitest";
import {
	classifyNewsSlotSubject,
	selectNewsSoundForSubject,
} from "./events";
import { createInitialSnapshot } from "./snapshot";
import { createTimeState } from "./time";
import type { PlacedObjectRecord } from "./world";

function record(family: number, unitStatus = 1): PlacedObjectRecord {
	return {
		leftTileIndex: 10,
		rightTileIndex: 13,
		objectTypeCode: family,
		unitStatus,
		auxValueOrTimer: 0,
		linkedRecordIndex: -1,
		dirtyFlag: 1,
		occupiedFlag: 1,
		evalLevel: 1,
		evalScore: 100,
		rentLevel: 0,
		activationTickCount: 0,
		housekeepingClaimedFlag: 0,
	};
}

describe("original random-news audio classifier", () => {
	it("classifies active hotel rooms and suppresses inactive rooms", () => {
		const { world } = createInitialSnapshot("test", "Tower", 2_000_000);
		world.cells["10,100"] = "hotelSingle";
		world.placedObjects["10,100"] = record(3, 1);
		expect(classifyNewsSlotSubject(world, 10, 100)).toBe(3);
		world.placedObjects["10,100"].unitStatus = 0;
		expect(classifyNewsSlotSubject(world, 10, 100)).toBe(-2);
	});

	it("keeps the original empty-tile above/below-ground distinction", () => {
		const { world } = createInitialSnapshot("test", "Tower", 2_000_000);
		expect(classifyNewsSlotSubject(world, 10, 100)).toBe(-1);
		expect(classifyNewsSlotSubject(world, 10, 115)).toBe(-2);
	});

	it("maps subjects and calendar fallbacks to supplied WAVE resources", () => {
		const snapshot = createInitialSnapshot("test", "Tower", 2_000_000);
		const time = createTimeState();
		expect(selectNewsSoundForSubject(snapshot.world, time, 3)).toBe(0x629);
		expect(selectNewsSoundForSubject(snapshot.world, time, 7)).toBe(0x5a8);
		expect(selectNewsSoundForSubject(snapshot.world, time, 0x1d)).toBe(0xb28);
		time.dayCounter = 6;
		time.daypartIndex = 3;
		expect(selectNewsSoundForSubject(snapshot.world, time, -1)).toBe(0x271c);
		time.dayCounter = 9;
		time.daypartIndex = 4;
		expect(selectNewsSoundForSubject(snapshot.world, time, -1)).toBe(0x271b);
	});
});
