import {
	FAMILY_CINEMA,
	FAMILY_FAST_FOOD,
	FAMILY_PARTY_HALL,
	FAMILY_PARKING,
	FAMILY_RECYCLING_CENTER_UPPER,
	FAMILY_RESTAURANT,
	FAMILY_RETAIL,
	FAMILY_SECURITY,
} from "../resources";
import type { SimSnapshot } from "../snapshot";
import { GRID_HEIGHT, GRID_WIDTH, yToFloor } from "../world";
import { ByteWriter } from "./tdtByteWriter";
import {
	builtShaftPayloadSize,
	TDT_DEFAULT_VIEW_X,
	TDT_DEFAULT_VIEW_Y,
	TDT_ELEVATOR_HEADER_SIZE,
	TDT_ELEVATOR_SCHEDULE_DEFAULT,
	TDT_ELEVATOR_SLOTS,
	TDT_FLOOR_COUNT,
	TDT_FLOOR_INDEX_ENTRIES,
	TDT_HEADER_SIZE,
	TDT_MAGIC,
	TDT_MAX_CENSUS,
	TDT_MAX_TENANTS_PER_FLOOR,
	TDT_PARKING_SIZE,
	TDT_PERSON_RECORD_SIZE,
	TDT_RETAIL_RECORD_SIZE,
	TDT_RETAIL_SLOTS,
	TDT_ROUTING_TAIL_SIZE,
	TDT_STAIR_RECORD_SIZE,
	TDT_STAIR_SLOTS,
} from "./tdtConstants";

const LAST_REGULAR_ORIGINAL_FLOOR = 109;
const LAST_CATHEDRAL_FLOOR = 113;
const CLASSIC_BEFORE_FINANCE_SIZE = 88;
const CLASSIC_AFTER_FINANCE_SIZE = 12 + 42;
const CLASSIC_BEFORE_STAIRS_SIZE = 22;

interface EncodedTenant {
	left: number;
	right: number;
	type: number;
	status: number;
	variant: number;
	rentRate: number;
}

interface StairRecord {
	type: number;
	x: number;
	floor: number;
}

export interface LegacyTdtExportReport {
	bytes: number;
	roomsWritten: number;
	peopleWritten: number;
	elevatorsWritten: number;
	stairsWritten: number;
	roomsDropped: number;
	transportsDropped: number;
	warnings: string[];
}

export interface ExportedLegacyTdt {
	bytes: Uint8Array;
	report: LegacyTdtExportReport;
}

export class LegacyExportError extends Error {
	constructor(message: string) {
		super(message);
		this.name = "LegacyExportError";
	}
}

function clamp(value: number, min: number, max: number): number {
	return Math.max(min, Math.min(max, Math.trunc(value)));
}

function toStoredMoney(value: number): number {
	return clamp(Math.round((Number.isFinite(value) ? value : 0) / 100), -0x80000000, 0x7fffffff);
}

function tenantVariant(snapshot: SimSnapshot, linkedRecordIndex: number): number {
	if (linkedRecordIndex < 0) return 0;
	const sidecar = snapshot.world.sidecars[linkedRecordIndex];
	if (sidecar?.kind === "commercial_venue") return clamp(sidecar.ownerSubtypeIndex, 0, 255);
	if (sidecar?.kind === "entertainment_link") {
		return clamp(sidecar.familySelectorOrSingleLinkFlag, 0, 255);
	}
	return 0;
}

function gatherFloorMap(snapshot: SimSnapshot): {
	tenants: Map<number, EncodedTenant[]>;
	extents: Map<number, { left: number; right: number }>;
	roomsWritten: number;
	roomsDropped: number;
} {
	const tenants = new Map<number, EncodedTenant[]>();
	const extents = new Map<number, { left: number; right: number }>();
	let roomsWritten = 0;
	let roomsDropped = 0;
	for (const [key, record] of Object.entries(snapshot.world.placedObjects)) {
		const [x, y] = key.split(",").map(Number);
		const floor = yToFloor(y);
		const cathedralSlice =
			record.objectTypeCode >= 36 && record.objectTypeCode <= 40;
		if (
			floor < 0 ||
			floor > (cathedralSlice ? LAST_CATHEDRAL_FLOOR : LAST_REGULAR_ORIGINAL_FLOOR)
		) {
			roomsDropped++;
			continue;
		}
		const encodedType =
			record.objectTypeCode === FAMILY_PARKING &&
			snapshot.world.cells[`${x},${y}`] === "parking"
				? 11
				: record.objectTypeCode;
		const row = tenants.get(floor) ?? [];
		row.push({
			left: clamp(record.leftTileIndex, 0, GRID_WIDTH),
			right: clamp(record.rightTileIndex + 1, 0, GRID_WIDTH),
			type: clamp(encodedType, 0, 127),
			status: clamp(record.unitStatus, 0, 255),
			variant: tenantVariant(snapshot, record.linkedRecordIndex),
			rentRate:
				record.rentLevel >= 0 && record.rentLevel <= 3
					? 3 - record.rentLevel
					: 4,
		});
		tenants.set(floor, row);
		roomsWritten++;
	}

	for (let floor = 0; floor < TDT_FLOOR_COUNT; floor++) {
		const y = GRID_HEIGHT - 1 - floor;
		let extentLeft = GRID_WIDTH;
		let extentRight = 0;
		for (let x = 0; x < GRID_WIDTH; x++) {
			if (snapshot.world.cells[`${x},${y}`]) {
				extentLeft = Math.min(extentLeft, x);
				extentRight = Math.max(extentRight, x + 1);
			}
		}
		if (extentRight > extentLeft) {
			extents.set(floor, { left: extentLeft, right: extentRight });
		}

		if (floor > LAST_REGULAR_ORIGINAL_FLOOR) continue;
		let x = 0;
		while (x < GRID_WIDTH) {
			const cell = snapshot.world.cells[`${x},${y}`];
			if (cell !== "floor" && cell !== "lobby") {
				x++;
				continue;
			}
			const left = x;
			while (x < GRID_WIDTH && snapshot.world.cells[`${x},${y}`] === cell) x++;
			const row = tenants.get(floor) ?? [];
			row.push({
				left,
				right: x,
				type: cell === "lobby" ? 24 : 0,
				status: 0,
				variant: 0,
				rentRate: 4,
			});
			tenants.set(floor, row);
		}
	}
	return { tenants, extents, roomsWritten, roomsDropped };
}

function writeFloorMap(
	w: ByteWriter,
	tenants: Map<number, EncodedTenant[]>,
	extents: Map<number, { left: number; right: number }>,
): void {
	for (let floor = 0; floor < TDT_FLOOR_COUNT; floor++) {
		const row = [...(tenants.get(floor) ?? [])].sort(
			(a, b) => a.left - b.left || a.type - b.type,
		);
		if (row.length > TDT_MAX_TENANTS_PER_FLOOR) {
			throw new LegacyExportError(
				`Floor slot ${floor} contains too many records for an original SimTower save.`,
			);
		}
		const extent = extents.get(floor);
		w.u16(row.length);
		w.u16(extent?.left ?? 0);
		w.u16(extent?.right ?? 0);
		for (const tenant of row) {
			w.u16(tenant.left);
			w.u16(Math.max(tenant.left, tenant.right));
			w.u8(tenant.type);
			w.u8(tenant.status);
			w.u8(tenant.variant);
			w.pad(9);
			w.u8(tenant.rentRate);
			w.u8(0);
		}
		w.pad(TDT_FLOOR_INDEX_ENTRIES * 2);
	}
}

function expressPotentialFloors(bottom: number, top: number): number {
	let count = 0;
	for (let floor = bottom; floor <= top; floor++) {
		if (floor <= 10 || (floor >= 24 && (floor - 24) % 15 === 0)) count++;
	}
	return Math.max(1, count);
}

function gatherStairs(snapshot: SimSnapshot): StairRecord[] {
	const flights = Object.entries(snapshot.world.overlays)
		.filter(([, type]) => type === "stairs" || type === "escalator")
		.map(([key, type]) => {
			const [x, y] = key.split(",").map(Number);
			return { type, x, floor: yToFloor(y) };
		})
		.filter(
			(flight) =>
				flight.floor >= 0 && flight.floor < LAST_REGULAR_ORIGINAL_FLOOR,
		)
		.sort(
			(a, b) =>
				a.type.localeCompare(b.type) || a.x - b.x || a.floor - b.floor,
		);
	const records: StairRecord[] = [];
	for (let index = 0; index < flights.length; ) {
		const first = flights[index];
		let run = 1;
		while (
			index + run < flights.length &&
			flights[index + run].type === first.type &&
			flights[index + run].x === first.x &&
			flights[index + run].floor === first.floor + run
		) {
			run++;
		}
		let floor = first.floor;
		for (let remaining = run; remaining > 0; ) {
			const stories = Math.min(3, remaining);
			records.push({
				type: (stories - 1) * 2 + (first.type === "stairs" ? 1 : 0),
				x: first.x,
				floor,
			});
			floor += stories;
			remaining -= stories;
		}
		index += run;
	}
	return records;
}

function writeFinance(w: ByteWriter, snapshot: SimSnapshot): void {
	const incomes = Array.from({ length: 10 }, (_, index) =>
		toStoredMoney(snapshot.ledger.incomeLedger[index] ?? 0),
	);
	const population = Array.from({ length: 10 }, (_, index) =>
		clamp(snapshot.ledger.populationLedger[index] ?? 0, -0x80000000, 0x7fffffff),
	);
	const expenses = Array.from({ length: 10 }, (_, index) =>
		toStoredMoney(snapshot.ledger.expenseLedger[index] ?? 0),
	);
	for (const value of incomes) w.i32(value);
	w.i32(clamp(snapshot.world.currentPopulation, 0, 0x7fffffff));
	for (const value of population) w.i32(value);
	w.i32(incomes.reduce((sum, value) => sum + value, 0));
	for (const value of expenses) w.i32(value);
	w.i32(expenses.reduce((sum, value) => sum + value, 0));
}

export function exportLegacyTdt(snapshot: SimSnapshot): ExportedLegacyTdt {
	const warnings: string[] = [];
	const floorMap = gatherFloorMap(snapshot);
	if (floorMap.roomsDropped > 0) {
		warnings.push(
			`${floorMap.roomsDropped} room record(s) above the original floor limit were omitted.`,
		);
	}
	const records = [...floorMap.tenants.values()].flat();
	const facilityRecords = records.filter((tenant) => tenant.type !== 0 && tenant.type !== 24);
	const w = new ByteWriter();
	w.u16(TDT_MAGIC);
	w.u16(clamp(snapshot.world.starCount, 1, 6));
	w.i32(toStoredMoney(snapshot.ledger.cashBalance));
	w.i32(0);
	w.i32(0);
	w.i32(toStoredMoney(snapshot.ledger.cashBalanceCycleBase));
	w.u16(clamp(snapshot.time.dayTick, 0, 2599));
	w.i32(clamp(snapshot.time.dayCounter, 0, 11_987));
	w.pad(TDT_HEADER_SIZE - w.length);
	w.setU16(0x1c, clamp(snapshot.world.lobbyHeight, 1, 3));
	w.setU16(0x26, TDT_DEFAULT_VIEW_X);
	w.setU16(0x28, TDT_DEFAULT_VIEW_Y);
	w.setU16(
		0x2a,
		facilityRecords.filter((tenant) => tenant.type === FAMILY_RECYCLING_CENTER_UPPER).length,
	);
	w.setU16(
		0x2e,
		facilityRecords.filter((tenant) =>
			[FAMILY_RETAIL, FAMILY_FAST_FOOD, FAMILY_RESTAURANT].includes(tenant.type),
		).length,
	);
	w.setU16(
		0x30,
		facilityRecords.filter((tenant) => tenant.type === FAMILY_SECURITY).length,
	);
	w.setU16(0x32, facilityRecords.filter((tenant) => tenant.type === 11).length);
	w.setU16(
		0x36,
		facilityRecords.filter((tenant) =>
			[18, FAMILY_PARTY_HALL].includes(tenant.type),
		).length,
	);

	writeFloorMap(w, floorMap.tenants, floorMap.extents);

	const peopleWritten = clamp(snapshot.world.currentPopulation, 0, TDT_MAX_CENSUS);
	w.i32(peopleWritten);
	w.pad(peopleWritten * TDT_PERSON_RECORD_SIZE);

	const retail = facilityRecords.filter((tenant) =>
		[FAMILY_RETAIL, FAMILY_FAST_FOOD, FAMILY_RESTAURANT].includes(tenant.type),
	);
	for (let slot = 0; slot < TDT_RETAIL_SLOTS; slot++) {
		const tenant = retail[slot];
		if (!tenant) {
			w.u8(0xff);
			w.pad(TDT_RETAIL_RECORD_SIZE - 1);
		} else {
			const floor = [...floorMap.tenants.entries()].find(([, row]) =>
				row.includes(tenant),
			)?.[0] ?? 10;
			w.u8(floor);
			w.u8(tenant.status);
			w.u8(tenant.variant);
			w.pad(TDT_RETAIL_RECORD_SIZE - 3);
		}
	}

	const carriers = snapshot.world.carriers
		.filter(
			(carrier) =>
				carrier.bottomServedFloor >= 0 &&
				carrier.topServedFloor <= LAST_REGULAR_ORIGINAL_FLOOR &&
				carrier.topServedFloor > carrier.bottomServedFloor,
		)
		.slice(0, TDT_ELEVATOR_SLOTS);
	const transportsDropped = Math.max(
		0,
		snapshot.world.carriers.length - carriers.length,
	);
	if (transportsDropped > 0) {
		warnings.push(
			`${transportsDropped} elevator shaft(s) outside original format limits were omitted.`,
		);
	}
	for (let slot = 0; slot < TDT_ELEVATOR_SLOTS; slot++) {
		const carrier = carriers[slot];
		if (!carrier) {
			w.pad(TDT_ELEVATOR_HEADER_SIZE);
			continue;
		}
		const cars = clamp(carrier.cars.length, 1, 8);
		w.u8(1);
		w.u8(carrier.carrierMode);
		w.u8([42, 21, 10][carrier.carrierMode]);
		w.u8(cars);
		for (const value of TDT_ELEVATOR_SCHEDULE_DEFAULT) w.u8(value);
		w.u8(1);
		w.u8(0);
		w.u16(clamp(carrier.column, 0, GRID_WIDTH - 1));
		w.u8(carrier.topServedFloor);
		w.u8(carrier.bottomServedFloor);
		for (let floor = 0; floor < TDT_FLOOR_COUNT; floor++) {
			const slotIndex = floor - carrier.bottomServedFloor;
			const inSpan = slotIndex >= 0 && floor <= carrier.topServedFloor;
			w.u8(inSpan && (carrier.stopFloorEnabled[slotIndex] ?? 1) !== 0 ? 1 : 0);
		}
		for (let car = 0; car < 8; car++) {
			w.u8(
				clamp(
					carrier.cars[car]?.homeFloor ?? carrier.bottomServedFloor,
					carrier.bottomServedFloor,
					carrier.topServedFloor,
				),
			);
		}
		const payload =
			carrier.carrierMode === 0
				? builtShaftPayloadSize(
						1,
						expressPotentialFloors(
							carrier.bottomServedFloor,
							carrier.topServedFloor,
						),
					)
				: builtShaftPayloadSize(
						carrier.bottomServedFloor,
						carrier.topServedFloor,
					);
		w.pad(payload);
	}

	w.pad(CLASSIC_BEFORE_FINANCE_SIZE);
	writeFinance(w, snapshot);
	w.pad(CLASSIC_AFTER_FINANCE_SIZE);
	const connectedParking = Object.values(snapshot.world.placedObjects).filter((record) => {
		if (record.objectTypeCode !== 11 || record.linkedRecordIndex < 0) return false;
		const sidecar = snapshot.world.sidecars[record.linkedRecordIndex];
		return sidecar?.kind === "service_request" && sidecar.coverageFlag === 1;
	}).length;
	w.u16(clamp(connectedParking, 0, 512));
	w.pad(TDT_PARKING_SIZE - 2);
	w.pad(CLASSIC_BEFORE_STAIRS_SIZE);

	const stairs = gatherStairs(snapshot);
	const stairsWritten = Math.min(stairs.length, TDT_STAIR_SLOTS);
	if (stairs.length > TDT_STAIR_SLOTS) {
		warnings.push(
			`${stairs.length - TDT_STAIR_SLOTS} stair/escalator record(s) exceeded the original table and were omitted.`,
		);
	}
	for (let slot = 0; slot < TDT_STAIR_SLOTS; slot++) {
		const stair = stairs[slot];
		if (!stair) {
			w.pad(TDT_STAIR_RECORD_SIZE);
			continue;
		}
		w.u8(1);
		w.u8(stair.type);
		w.u16(stair.x);
		w.u16(stair.floor);
		w.u16(0);
		w.u16(0);
	}
	w.padFF(TDT_ROUTING_TAIL_SIZE);

	const bytes = w.toBytes();
	return {
		bytes,
		report: {
			bytes: bytes.byteLength,
			roomsWritten: floorMap.roomsWritten,
			peopleWritten,
			elevatorsWritten: carriers.length,
			stairsWritten,
			roomsDropped: floorMap.roomsDropped,
			transportsDropped,
			warnings,
		},
	};
}
