import { describe, expect, it } from "vitest";

import { exportLegacyTdt } from "./export";
import { importLegacyTdt } from "./import";
import { TDT_HEADER_SIZE } from "./tdtFormat";

class FixtureWriter {
	private readonly bytes: number[] = [];

	u8(value: number): void {
		this.bytes.push(value & 0xff);
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

function oneOfficeFixture(): Uint8Array {
	const writer = new FixtureWriter();
	writer.u16(0x2400);
	writer.u16(3);
	writer.i32(123_456);
	writer.zero(12);
	writer.u16(777);
	writer.i32(9);
	writer.zero(TDT_HEADER_SIZE - writer.length);
	for (let floor = 0; floor < 120; floor++) {
		const lobby = floor === 10;
		const officeFloor = floor === 11;
		const count = lobby ? 1 : officeFloor ? 2 : 0;
		writer.u16(count);
		writer.u16(count ? 100 : 0);
		writer.u16(count ? 140 : 0);
		if (lobby) {
			writer.u16(100);
			writer.u16(140);
			writer.u8(24);
			writer.zero(11);
			writer.u8(4);
			writer.u8(0);
		}
		if (officeFloor) {
			for (const tenant of [
				{ left: 100, right: 140, type: 0, status: 0, rent: 4 },
				{ left: 105, right: 114, type: 7, status: 6, rent: 2 },
			]) {
				writer.u16(tenant.left);
				writer.u16(tenant.right);
				writer.u8(tenant.type);
				writer.u8(tenant.status);
				writer.u8(0);
				writer.zero(9);
				writer.u8(tenant.rent);
				writer.u8(0);
			}
		}
		writer.zero(94 * 2);
	}
	return writer.finish();
}

function asBuffer(bytes: Uint8Array): ArrayBuffer {
	return bytes.buffer.slice(
		bytes.byteOffset,
		bytes.byteOffset + bytes.byteLength,
	) as ArrayBuffer;
}

describe("legacy SimTower TDT export", () => {
	it("round-trips an imported tower through a complete original-format tail", () => {
		const imported = importLegacyTdt(asBuffer(oneOfficeFixture()), "OFFICE.TDT");
		const exported = exportLegacyTdt(imported.snapshot);
		const roundTrip = importLegacyTdt(asBuffer(exported.bytes), "ROUNDTRIP.TDT");

		expect(exported.bytes[0]).toBe(0x00);
		expect(exported.bytes[1]).toBe(0x24);
		expect(exported.report.roomsWritten).toBe(1);
		expect(exported.report.roomsDropped).toBe(0);
		expect(exported.report.warnings).toEqual([]);
		expect(roundTrip.report.roomsPlaced).toBe(1);
		expect(roundTrip.report.roomsSkipped).toBe(0);
		expect(roundTrip.report.warnings).toEqual([]);
		expect(roundTrip.snapshot.world.placedObjects["105,108"].rentLevel).toBe(1);
		expect(roundTrip.snapshot.ledger.cashBalance).toBe(12_345_600);
		expect(roundTrip.snapshot.time.dayTick).toBe(777);
		expect(roundTrip.snapshot.time.dayCounter).toBe(9);
	});
});
