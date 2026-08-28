import { describe, expect, it } from "vitest";
import {
	flushCarriersEndOfDay,
	makeCarrier,
	rebuildCarrierList,
} from "./carriers";
import { floorToY, type WorldState } from "./world";

describe("flushCarriersEndOfDay", () => {
	it("preserves route-ring heads while clearing queue counts", () => {
		const carrier = makeCarrier(0, 100, 1, 10, 19, 1);
		const queue = carrier.floorQueues[0].up;
		for (let i = 0; i < 15; i++) queue.push(`r${i}`);
		for (let i = 0; i < 11; i++) queue.pop();

		expect(queue.head).toBe(11);
		expect(queue.size).toBe(4);

		flushCarriersEndOfDay({
			carriers: [carrier],
		} as WorldState);

		expect(queue.head).toBe(11);
		expect(queue.size).toBe(0);

		queue.push("next");
		expect(queue.head).toBe(11);
		expect(queue.peekAll()).toEqual(["next"]);
	});
});

describe("rebuildCarrierList", () => {
	it("preserves different shaft modes in separated runs at one column", () => {
		const world = {
			overlays: {
				[`10,${floorToY(20)}`]: "elevatorExpress",
				[`10,${floorToY(21)}`]: "elevatorExpress",
				[`10,${floorToY(30)}`]: "elevator",
				[`10,${floorToY(31)}`]: "elevator",
			},
			overlayToAnchor: {},
			carriers: [],
		} as unknown as WorldState;

		rebuildCarrierList(world);

		expect(world.carriers).toHaveLength(2);
		expect(
			world.carriers.map((carrier) => ({
				mode: carrier.carrierMode,
				bottom: carrier.bottomServedFloor,
				top: carrier.topServedFloor,
			})),
		).toEqual([
			{ mode: 0, bottom: 20, top: 21 },
			{ mode: 1, bottom: 30, top: 31 },
		]);
	});
});
