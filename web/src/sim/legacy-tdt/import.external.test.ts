import { existsSync, readFileSync } from "node:fs";
import { resolve } from "node:path";

import { describe, expect, it } from "vitest";

import { exportLegacyTdt } from "./export";
import { importLegacyTdt } from "./import";

const samples = [
	{
		path: "../references/tdt-samples/ElDlux/ElDlux.TDT",
		rooms: 1239,
		elevators: 23,
		stairs: 55,
		population: 10_540,
		lobbyHeight: 2,
	},
	{
		path: "../references/tdt-samples/RoyalAbode/RoyalA.TDT",
		rooms: 2324,
		elevators: 23,
		stairs: 68,
		population: 16_558,
		lobbyHeight: 3,
	},
	{
		path: "../references/tdt-samples/SimEmpire/SimEmpire.TDT",
		rooms: 2575,
		elevators: 24,
		stairs: 72,
		population: 20_003,
		lobbyHeight: 3,
	},
].map((sample) => ({ ...sample, path: resolve(process.cwd(), sample.path) }));

describe("legacy TDT compatibility sample", () => {
	for (const sample of samples) {
		it.skipIf(!existsSync(sample.path))(
			`imports ${sample.path.split(/[\\/]/).pop()}`,
			() => {
			const bytes = readFileSync(sample.path);
			const buffer = bytes.buffer.slice(
				bytes.byteOffset,
				bytes.byteOffset + bytes.byteLength,
			) as ArrayBuffer;
			const filename = sample.path.split(/[\\/]/).pop() ?? "TOWER.TDT";
			const { snapshot, report } = importLegacyTdt(buffer, filename);

			expect(report.filename).toBe(filename);
			expect(report.roomsPlaced).toBe(sample.rooms);
			expect(report.roomsSkipped).toBe(0);
			expect(report.elevatorsPlaced).toBe(sample.elevators);
			expect(report.stairsPlaced).toBe(sample.stairs);
			expect(report.populationImported).toBe(sample.population);
			expect(report.warnings).toEqual([]);
			expect(snapshot.world.lobbyHeight).toBe(sample.lobbyHeight);
			expect(snapshot.world.starCount).toBeGreaterThanOrEqual(1);
			expect(snapshot.ledger.cashBalance).toBeGreaterThan(0);
			expect(Object.keys(snapshot.world.cells).length).toBeGreaterThan(0);

			const exported = exportLegacyTdt(snapshot);
			const roundTrip = importLegacyTdt(
				exported.bytes.buffer.slice(
					exported.bytes.byteOffset,
					exported.bytes.byteOffset + exported.bytes.byteLength,
				) as ArrayBuffer,
				`ROUNDTRIP-${filename}`,
			);
			expect(exported.report.warnings).toEqual([]);
			expect(roundTrip.report.roomsPlaced).toBe(sample.rooms);
			expect(roundTrip.report.roomsSkipped).toBe(0);
			expect(roundTrip.report.elevatorsPlaced).toBe(sample.elevators);
			expect(roundTrip.report.stairsPlaced).toBe(sample.stairs);
			expect(roundTrip.report.populationImported).toBe(sample.population);
			expect(roundTrip.report.warnings).toEqual([]);
			},
			15_000,
		);
	}
});
