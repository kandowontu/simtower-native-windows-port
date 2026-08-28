import { describe, expect, it } from "vitest";
import { RouteRequestRing } from "./route-record";

describe("RouteRequestRing", () => {
	it("holds up to 40 entries in FIFO order", () => {
		const ring = new RouteRequestRing();
		for (let i = 0; i < 40; i++) ring.push(`r${i}`);
		expect(ring.size).toBe(40);
		expect(ring.peekAll()[0]).toBe("r0");
		expect(ring.peekAll()[39]).toBe("r39");
	});

	it("silently overwrites head on the 41st enqueue (binary quirk)", () => {
		const ring = new RouteRequestRing();
		for (let i = 0; i < 40; i++) ring.push(`r${i}`);
		// Binary 1218:1002 computes (head+count)%40 and saturates count at 40.
		// The 41st push overwrites what used to be head (r0); head advances so
		// the new head is r1 and the new tail is the just-pushed r40.
		ring.push("r40");
		expect(ring.size).toBe(40);
		const items = ring.peekAll();
		expect(items[0]).toBe("r1");
		expect(items[39]).toBe("r40");
		// Further pushes keep wrapping and shifting head forward.
		ring.push("r41");
		expect(ring.peekAll()[0]).toBe("r2");
		expect(ring.peekAll()[39]).toBe("r41");
	});

	it("pop returns entries in FIFO order even after a wrap", () => {
		const ring = new RouteRequestRing();
		for (let i = 0; i < 41; i++) ring.push(`r${i}`);
		// After the wrap, popping yields r1..r40 (r0 was clobbered).
		const popped: string[] = [];
		while (!ring.isEmpty) {
			const item = ring.pop();
			if (item !== undefined) popped.push(item);
		}
		expect(popped.length).toBe(40);
		expect(popped[0]).toBe("r1");
		expect(popped[39]).toBe("r40");
	});

	it("removeFirst compacts the ring and preserves order", () => {
		const ring = new RouteRequestRing();
		for (const id of ["a", "b", "c", "d"]) ring.push(id);
		expect(ring.removeFirst("c")).toBe(true);
		expect(ring.peekAll()).toEqual(["a", "b", "d"]);
		expect(ring.removeFirst("z")).toBe(false);
	});

	it("removeFirst rebuilds the ring from the advanced head", () => {
		const ring = new RouteRequestRing();
		for (let i = 0; i < 8; i++) ring.push(`r${i}`);
		for (let i = 0; i < 5; i++) ring.pop();
		expect(ring.head).toBe(5);

		ring.push("r8");
		ring.push("r9");
		expect(ring.peekAll()).toEqual(["r5", "r6", "r7", "r8", "r9"]);

		expect(ring.removeFirst("r7")).toBe(true);
		expect(ring.head).toBe(10);
		expect(ring.peekAll()).toEqual(["r5", "r6", "r8", "r9"]);
	});
});
