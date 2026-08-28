/**
 * Determinism tests for the lockstep architecture.
 *
 * The multiplayer flow assumes:
 *   1. `saveState()` → `fromSnapshot()` → `saveState()` round-trip is identity
 *      (hydration must not mutate state).
 *   2. After hydration, two sims started from the same snapshot must produce
 *      bit-equal state when stepped the same number of ticks (deterministic
 *      forward sim).
 *
 * Either property failing causes a new joiner's local TowerSim to drift from
 * the server's, manifesting as missing sims and out-of-sync unitStatus
 * (e.g. for-sale banner timing).
 */

// @ts-expect-error vitest runs in Node; not in CF worker types
import { readFileSync } from "node:fs";
// @ts-expect-error vitest runs in Node; not in CF worker types
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { TowerSim } from "./index";
import type { SimSnapshot } from "./snapshot";
import { GROUND_Y } from "./world";

const fixtureDir = `${fileURLToPath(new URL(".", import.meta.url))}fixtures`;

interface FacilitySpec {
	type: string;
	floor?: number;
	left: number;
	right?: number;
	bottom?: number;
	top?: number;
	cars?: number;
}
interface BuildSpec {
	floor_extent: Record<string, { left: number; right: number }>;
	facilities: FacilitySpec[];
}

const FIXTURE_TILE_MAP: Record<string, string> = {
	office: "office",
	condo: "condo",
	restaurant: "restaurant",
	"fast-food": "fastFood",
	retail: "retail",
	cinema: "cinema",
	"party-hall": "partyHall",
	single: "hotelSingle",
	twin: "hotelTwin",
	suite: "hotelSuite",
	stairs: "stairs",
	security: "security",
	housekeeping: "housekeeping",
	medical: "medical",
	lobby: "lobby",
};

function place(sim: TowerSim, x: number, y: number, tileType: string): void {
	const result = sim.submitCommand({ type: "place_tile", x, y, tileType });
	if (!result.accepted) {
		throw new Error(`place ${tileType}@${x},${y}: ${result.reason}`);
	}
}

function placeFacilityList(sim: TowerSim, facilities: FacilitySpec[]): void {
	for (const fac of facilities) {
		if (
			fac.type === "elevator" ||
			fac.type === "elevatorExpress" ||
			fac.type === "elevatorService"
		) {
			const bottom = fac.bottom ?? fac.floor ?? 0;
			const top = fac.top ?? fac.floor ?? 0;
			for (let f = bottom; f <= top; f++) {
				sim.submitCommand({
					type: "place_tile",
					x: fac.left,
					y: GROUND_Y - f,
					tileType: fac.type,
				});
			}
			const numCars = fac.cars ?? 1;
			const span = top - bottom;
			for (let i = 1; i < numCars; i++) {
				const homeFloor =
					numCars <= 1
						? bottom
						: bottom + Math.floor((span * i) / (numCars - 1));
				sim.submitCommand({
					type: "add_elevator_car",
					x: fac.left,
					y: GROUND_Y - homeFloor,
				});
			}
			continue;
		}
		const tileType = FIXTURE_TILE_MAP[fac.type];
		if (!tileType) throw new Error(`Unknown fixture tile type: ${fac.type}`);
		if (fac.floor === undefined) {
			throw new Error(`Facility ${fac.type} missing 'floor'`);
		}
		const y = GROUND_Y - fac.floor;
		if (tileType === "lobby" && fac.right !== undefined) {
			for (let x = fac.left; x < fac.right; x++) {
				sim.submitCommand({ type: "place_tile", x, y, tileType });
			}
			continue;
		}
		sim.submitCommand({ type: "place_tile", x: fac.left, y, tileType });
	}
}

function buildFromFixture(name: string): TowerSim {
	const spec: BuildSpec = JSON.parse(
		readFileSync(`${fixtureDir}/build_${name}.json`, "utf-8"),
	);
	// Fixtures dump the binary's floor 14, 29, 44, ... sky-lobby cadence.
	const sim = TowerSim.create(
		"determinism-fixture",
		"Determinism Fixture",
		"perfect-parity",
	);
	sim.freeBuild = true;

	const groundExtent = spec.floor_extent["0"];
	if (groundExtent) {
		for (let x = groundExtent.left; x < groundExtent.right; x++) {
			sim.submitCommand({
				type: "place_tile",
				x,
				y: GROUND_Y,
				tileType: "lobby",
			});
		}
	}

	const floors = Object.keys(spec.floor_extent)
		.map(Number)
		.filter((f) => f !== 0)
		.sort((a, b) => a - b);

	for (const floor of floors) {
		const extent = spec.floor_extent[String(floor)];
		const y = GROUND_Y - floor;
		const tileType = floor % 15 === 14 ? "lobby" : "floor";
		for (let x = extent.left; x < extent.right; x++) {
			sim.submitCommand({ type: "place_tile", x, y, tileType });
		}
	}

	placeFacilityList(sim, spec.facilities);

	sim.freeBuild = false;
	sim.setStarCount(3);
	return sim;
}

function buildSmallMixedTower(): TowerSim {
	const sim = TowerSim.create("determinism", "Determinism");
	sim.freeBuild = true;

	// Ground floor lobby across x=50..110.
	for (let x = 50; x <= 110; x++) place(sim, x, GROUND_Y, "lobby");

	// Floor support for floors 1..3.
	for (let floor = 1; floor <= 3; floor++) {
		const y = GROUND_Y - floor;
		for (let x = 50; x <= 110; x++) place(sim, x, y, "floor");
	}

	// Floor 1: hotel rooms + a stair pair from ground.
	place(sim, 60, GROUND_Y - 1, "hotelSingle");
	place(sim, 64, GROUND_Y - 1, "hotelSingle");
	place(sim, 68, GROUND_Y - 1, "hotelTwin");

	// Floor 2: offices.
	place(sim, 60, GROUND_Y - 2, "office");
	place(sim, 70, GROUND_Y - 2, "office");

	// Floor 3: condo.
	place(sim, 60, GROUND_Y - 3, "condo");

	// Stairs from ground up through floor 3.
	for (let floor = 1; floor <= 3; floor++) {
		place(sim, 84, GROUND_Y - floor, "stairs");
	}

	// Elevator shaft from ground up through floor 3 with one car.
	for (let floor = 0; floor <= 3; floor++) {
		place(sim, 100, GROUND_Y - floor, "elevator");
	}

	sim.freeBuild = false;
	sim.setStarCount(3);
	return sim;
}

function stepN(sim: TowerSim, ticks: number): void {
	for (let i = 0; i < ticks; i++) sim.step();
}

/**
 * Returns the first JSON path where `a` and `b` differ, or null if equal.
 * Both inputs are expected to be JSON-equivalent (the snapshot serializer
 * already normalizes via JSON.parse(JSON.stringify(...)) for nested fields).
 */
function findFirstDiff(a: unknown, b: unknown, path = ""): string | null {
	if (Object.is(a, b)) return null;
	if (typeof a !== typeof b || a === null || b === null) {
		return `${path}: ${preview(a)} !== ${preview(b)}`;
	}
	if (Array.isArray(a) || Array.isArray(b)) {
		if (!Array.isArray(a) || !Array.isArray(b)) {
			return `${path}: array vs non-array`;
		}
		if (a.length !== b.length) {
			return `${path}.length: ${a.length} !== ${b.length}`;
		}
		for (let i = 0; i < a.length; i++) {
			const sub = findFirstDiff(a[i], b[i], `${path}[${i}]`);
			if (sub) return sub;
		}
		return null;
	}
	if (typeof a === "object") {
		const aKeys = Object.keys(a as object);
		const bKeys = Object.keys(b as object);
		const all = new Set([...aKeys, ...bKeys]);
		for (const k of all) {
			const av = (a as Record<string, unknown>)[k];
			const bv = (b as Record<string, unknown>)[k];
			const sub = findFirstDiff(av, bv, `${path}.${k}`);
			if (sub) return sub;
		}
		return null;
	}
	return `${path}: ${preview(a)} !== ${preview(b)}`;
}

function preview(value: unknown): string {
	const str = JSON.stringify(value);
	if (str === undefined) return String(value);
	return str.length > 80 ? `${str.slice(0, 77)}...` : str;
}

function jsonRoundTrip(snapshot: SimSnapshot): SimSnapshot {
	return JSON.parse(JSON.stringify(snapshot)) as SimSnapshot;
}

function expectSnapshotsEqual(a: SimSnapshot, b: SimSnapshot, label: string) {
	const diff = findFirstDiff(jsonRoundTrip(a), jsonRoundTrip(b));
	if (diff !== null) {
		throw new Error(`${label}: snapshots diverge at ${diff}`);
	}
}

describe("snapshot hydration is idempotent", () => {
	for (const warmupTicks of [0, 50, 500, 2_500]) {
		it(`saveState → fromSnapshot → saveState equals original (warmup=${warmupTicks})`, () => {
			const server = buildSmallMixedTower();
			stepN(server, warmupTicks);

			const before = server.saveState();
			const rehydrated = TowerSim.fromSnapshot(before);
			const after = rehydrated.saveState();

			expectSnapshotsEqual(before, after, `warmup=${warmupTicks}`);
		});
	}
});

describe("two sims hydrated from the same snapshot stay in lockstep", () => {
	for (const warmupTicks of [0, 50, 500, 2_500]) {
		for (const stepTicks of [1, 50, 500]) {
			it(`server vs client diverge after ${stepTicks} ticks (warmup=${warmupTicks})`, () => {
				const server = buildSmallMixedTower();
				stepN(server, warmupTicks);

				// "Client" hydrates from server's snapshot — same state lockstep
				// architecture sends to a new joiner.
				const snapshot = server.saveState();
				const client = TowerSim.fromSnapshot(snapshot);

				// Both step forward independently. With no inputs, they must
				// produce bit-equal state at every tick.
				stepN(server, stepTicks);
				stepN(client, stepTicks);

				expectSnapshotsEqual(
					server.saveState(),
					client.saveState(),
					`warmup=${warmupTicks} step=${stepTicks}`,
				);
			});
		}
	}

	it("rng advances in lockstep", () => {
		const server = buildSmallMixedTower();
		stepN(server, 200);

		const snapshot = server.saveState();
		const client = TowerSim.fromSnapshot(snapshot);

		for (let i = 0; i < 250; i++) {
			server.step();
			client.step();
			const a = server.saveState();
			const b = client.saveState();
			expect(b.world.rngState).toBe(a.world.rngState);
			expect(b.world.rngCallCount).toBe(a.world.rngCallCount);
			expect(b.time.totalTicks).toBe(a.time.totalTicks);
		}
	});
});

// Fixture-based determinism: bigger towers exercise more code paths
// (parking, sky lobby, dense hotels/offices) where snapshot drift is
// most likely to surface. Larger fixtures step slowly, so cap warmup.
describe.each([
	"mixed",
	"hotel",
	"offices",
	"condo",
	"commercial",
	"elevator",
	"mixed_elevator",
])("fixture: %s", (fixture) => {
	it("hydration is idempotent after warmup", () => {
		const server = buildFromFixture(fixture);
		stepN(server, 1_000);
		const before = server.saveState();
		const after = TowerSim.fromSnapshot(before).saveState();
		expectSnapshotsEqual(before, after, `${fixture} idempotent`);
	});

	it("server vs hydrated client stay in lockstep", () => {
		const server = buildFromFixture(fixture);
		stepN(server, 1_000);
		const client = TowerSim.fromSnapshot(server.saveState());

		stepN(server, 500);
		stepN(client, 500);

		expectSnapshotsEqual(
			server.saveState(),
			client.saveState(),
			`${fixture} lockstep`,
		);
	});
});
