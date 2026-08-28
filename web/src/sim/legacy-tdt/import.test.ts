import { describe, expect, it } from "vitest";

import { importLegacyTdt, looksLikeLegacyTdt } from "./import";
import { LegacyImportError, TDT_HEADER_SIZE } from "./tdtFormat";

interface FixtureTenant {
	left: number;
	right: number;
	type: number;
	status?: number;
	variant?: number;
	rent?: number;
}

class Writer {
	private readonly bytes: number[] = [];

	u8(value: number): void {
		this.bytes.push(value & 0xff);
	}

	i8(value: number): void {
		this.u8(value);
	}

	u16(value: number): void {
		this.u8(value);
		this.u8(value >>> 8);
	}

	i32(value: number): void {
		this.u8(value);
		this.u8(value >>> 8);
		this.u8(value >>> 16);
		this.u8(value >>> 24);
	}

	zero(count: number): void {
		for (let index = 0; index < count; index++) this.u8(0);
	}

	get length(): number {
		return this.bytes.length;
	}

	finish(): Uint8Array {
		return Uint8Array.from(this.bytes);
	}
}

function buildFixture(): Uint8Array {
	const writer = new Writer();
	writer.u16(0x2400);
	writer.u16(3);
	writer.i32(123_456);
	writer.zero(12);
	writer.u16(777);
	writer.i32(9);
	writer.zero(TDT_HEADER_SIZE - writer.length);

	const floors = new Map<number, FixtureTenant[]>([
		[10, [{ left: 100, right: 140, type: 24 }]],
		[
			11,
			[
				{ left: 100, right: 140, type: 0 },
				{ left: 105, right: 114, type: 7, status: 6, rent: 2 },
			],
		],
	]);
	for (let floor = 0; floor < 120; floor++) {
		const tenants = floors.get(floor) ?? [];
		writer.u16(tenants.length);
		writer.u16(tenants.length ? Math.min(...tenants.map((t) => t.left)) : 0);
		writer.u16(tenants.length ? Math.max(...tenants.map((t) => t.right)) : 0);
		for (const tenant of tenants) {
			writer.u16(tenant.left);
			writer.u16(tenant.right);
			writer.i8(tenant.type);
			writer.u8(tenant.status ?? 0);
			writer.u8(tenant.variant ?? 0);
			writer.zero(9);
			writer.u8(tenant.rent ?? 4);
			writer.u8(0);
		}
		writer.zero(94 * 2);
	}
	return writer.finish();
}

describe("legacy SimTower TDT import", () => {
	it("imports the original header, structure, room state, and rent class", () => {
		const bytes = buildFixture();
		expect(looksLikeLegacyTdt(bytes)).toBe(true);
		const arrayBuffer = bytes.buffer.slice(
			bytes.byteOffset,
			bytes.byteOffset + bytes.byteLength,
		) as ArrayBuffer;
		const { snapshot, report } = importLegacyTdt(arrayBuffer, "MY_TOWER.TDT");

		expect(snapshot.world.name).toBe("MY TOWER");
		expect(snapshot.world.starCount).toBe(3);
		expect(snapshot.ledger.cashBalance).toBe(12_345_600);
		expect(snapshot.time.dayTick).toBe(777);
		expect(snapshot.time.dayCounter).toBe(9);
		expect(snapshot.world.cells["100,109"]).toBe("lobby");
		expect(snapshot.world.cells["105,108"]).toBe("office");
		expect(snapshot.world.placedObjects["105,108"].unitStatus).toBe(6);
		expect(snapshot.world.placedObjects["105,108"].rentLevel).toBe(1);
		expect(report.roomsPlaced).toBe(1);
		expect(report.roomsSkipped).toBe(0);
	});

	it("rejects a file without the original 0x2400 magic", () => {
		const bytes = buildFixture();
		bytes[1] = 0;
		expect(looksLikeLegacyTdt(bytes)).toBe(false);
		expect(() =>
			importLegacyTdt(bytes.buffer as ArrayBuffer, "BROKEN.TDT"),
		).toThrow(LegacyImportError);
	});
});
