import { makeCarrierCar } from "../carriers";
import { handlePlaceTile, runGlobalRebuilds } from "../commands";
import {
	FAMILY_CINEMA,
	FAMILY_CONDO,
	FAMILY_FAST_FOOD,
	FAMILY_HOTEL_SINGLE,
	FAMILY_HOTEL_SUITE,
	FAMILY_HOTEL_TWIN,
	FAMILY_OFFICE,
	FAMILY_PARTY_HALL,
	FAMILY_RESTAURANT,
	FAMILY_RETAIL,
	TILE_WIDTHS,
} from "../resources";
import { createInitialSnapshot, type SimSnapshot } from "../snapshot";
import {
	floorToY,
	GRID_HEIGHT,
	GRID_WIDTH,
	type PlacedObjectRecord,
	yToFloor,
} from "../world";
import {
	LegacyImportError,
	parseTdtBinary,
	TDT_MAGIC,
	type TdtElevator,
	type TdtTenant,
	type TdtTower,
} from "./tdtFormat";

const MAX_ORIGINAL_FLOOR_SLOT = 113;
const ORIGINAL_DAY_WRAP = 11_988;

const SINGLE_TENANT_TILES: Readonly<Record<number, string>> = {
	3: "hotelSingle",
	4: "hotelTwin",
	5: "hotelSuite",
	6: "restaurant",
	7: "office",
	9: "condo",
	10: "retail",
	11: "parking",
	12: "fastFood",
	13: "medical",
	14: "security",
	15: "housekeeping",
	17: "security",
	44: "parkingRamp",
};

type PartKind =
	| "cinema"
	| "recyclingCenter"
	| "partyHall"
	| "metro"
	| "cathedral";

const PART_KIND: Readonly<Record<number, PartKind>> = {
	18: "cinema",
	19: "cinema",
	20: "recyclingCenter",
	21: "recyclingCenter",
	29: "partyHall",
	30: "partyHall",
	31: "metro",
	32: "metro",
	33: "metro",
	34: "cinema",
	35: "cinema",
	36: "cathedral",
	37: "cathedral",
	38: "cathedral",
	39: "cathedral",
	40: "cathedral",
};

const PART_STORIES: Readonly<Record<PartKind, number>> = {
	cinema: 2,
	recyclingCenter: 2,
	partyHall: 2,
	metro: 3,
	cathedral: 5,
};

const LEDGER_FAMILIES = [
	FAMILY_OFFICE,
	FAMILY_HOTEL_SINGLE,
	FAMILY_HOTEL_TWIN,
	FAMILY_HOTEL_SUITE,
	FAMILY_RETAIL,
	FAMILY_FAST_FOOD,
	FAMILY_RESTAURANT,
	FAMILY_PARTY_HALL,
	FAMILY_CINEMA,
	FAMILY_CONDO,
] as const;

interface PartRecord {
	kind: PartKind;
	typeId: number;
	floor: number;
	left: number;
	right: number;
	construction: boolean;
	tenant: TdtTenant;
}

interface MergedPart {
	kind: PartKind;
	bottom: number;
	top: number;
	left: number;
	right: number;
	parts: PartRecord[];
}

export interface LegacyTdtImportReport {
	filename: string;
	starCount: number;
	cash: number;
	dayCounter: number;
	dayTick: number;
	roomsPlaced: number;
	roomsSkipped: number;
	constructionConverted: number;
	elevatorsPlaced: number;
	stairsPlaced: number;
	populationImported: number | null;
	warnings: string[];
}

export interface ImportedLegacyTdt {
	snapshot: SimSnapshot;
	report: LegacyTdtImportReport;
}

function clamp(value: number, min: number, max: number): number {
	return Math.max(min, Math.min(max, value));
}

function towerNameFromFilename(filename: string): string {
	const leaf = filename.replace(/^.*[\\/]/, "").replace(/\.tdt$/i, "");
	const clean = leaf.replace(/[_-]+/g, " ").replace(/[^\w .']/g, "").trim();
	return clean.slice(0, 40) || "Imported Tower";
}

function hashBytes(bytes: Uint8Array): number {
	let hash = 0x811c9dc5;
	for (const byte of bytes) {
		hash ^= byte;
		hash = Math.imul(hash, 0x01000193);
	}
	return hash >>> 0;
}

function rentLevelFromTdt(rentRate: number): number {
	return rentRate >= 0 && rentRate <= 3 ? 3 - rentRate : 4;
}

function overlaps(a: PartRecord, b: PartRecord): boolean {
	const stories = PART_STORIES[a.kind];
	const strict = a.left < b.right && b.left < a.right;
	const screenTouch =
		a.kind === "cinema" &&
		a.floor === b.floor &&
		(a.right === b.left || b.right === a.left) &&
		([34, 35].includes(a.typeId) !== [34, 35].includes(b.typeId));
	return (strict && Math.abs(a.floor - b.floor) < stories) || screenTouch;
}

function mergePartRecords(parts: readonly PartRecord[]): MergedPart[] {
	const merged: MergedPart[] = [];
	for (const kind of Object.keys(PART_STORIES) as PartKind[]) {
		const records = parts.filter((part) => part.kind === kind);
		const parent = records.map((_, index) => index);
		const find = (index: number): number => {
			while (parent[index] !== index) {
				parent[index] = parent[parent[index]];
				index = parent[index];
			}
			return index;
		};
		for (let left = 0; left < records.length; left++) {
			for (let right = left + 1; right < records.length; right++) {
				if (!overlaps(records[left], records[right])) continue;
				const a = find(left);
				const b = find(right);
				if (a !== b) parent[a] = b;
			}
		}
		const clusters = new Map<number, PartRecord[]>();
		for (let index = 0; index < records.length; index++) {
			const root = find(index);
			const cluster = clusters.get(root) ?? [];
			cluster.push(records[index]);
			clusters.set(root, cluster);
		}
		for (const cluster of clusters.values()) {
			cluster.sort((a, b) => a.floor - b.floor || a.left - b.left);
			const stories = PART_STORIES[kind];
			let group: PartRecord[] = [];
			const flush = (): void => {
				if (!group.length) return;
				// A vertical cluster may contain multiple flush, side-by-side
				// buildings joined through an overlapping neighbor on another floor.
				// Split it again by horizontal connectivity before emitting units.
				const componentParent = group.map((_, index) => index);
				const componentFind = (index: number): number => {
					while (componentParent[index] !== index) {
						componentParent[index] =
							componentParent[componentParent[index]];
						index = componentParent[index];
					}
					return index;
				};
				for (let left = 0; left < group.length; left++) {
					for (let right = left + 1; right < group.length; right++) {
						if (!overlaps(group[left], group[right])) continue;
						const a = componentFind(left);
						const b = componentFind(right);
						if (a !== b) componentParent[a] = b;
					}
				}
				const components = new Map<number, PartRecord[]>();
				for (let index = 0; index < group.length; index++) {
					const root = componentFind(index);
					const component = components.get(root) ?? [];
					component.push(group[index]);
					components.set(root, component);
				}
				for (const component of components.values()) {
					const componentLeft = Math.min(
						...component.map((part) => part.left),
					);
					const componentRight = Math.max(
						...component.map((part) => part.right),
					);
					const canonicalWidth = TILE_WIDTHS[kind];
					const count = (componentRight - componentLeft) / canonicalWidth;
					let emitComponents: PartRecord[][] = [component];
					if (Number.isInteger(count) && count > 1) {
						const partitions = Array.from(
							{ length: count },
							() => [] as PartRecord[],
						);
						let cleanPartition = true;
						for (const part of component) {
							const index = Math.floor(
								(part.left - componentLeft) / canonicalWidth,
							);
							if (
								index < 0 ||
								index >= count ||
								part.right > componentLeft + (index + 1) * canonicalWidth
							) {
								cleanPartition = false;
								break;
							}
							partitions[index].push(part);
						}
						if (cleanPartition && partitions.every((part) => part.length > 0)) {
							emitComponents = partitions;
						}
					}
					for (const unit of emitComponents) {
						merged.push({
							kind,
							bottom: Math.min(...unit.map((part) => part.floor)),
							top: Math.max(...unit.map((part) => part.floor)),
							left: Math.min(...unit.map((part) => part.left)),
							right: Math.max(...unit.map((part) => part.right)),
							parts: unit,
						});
					}
				}
				group = [];
			};
			for (const part of cluster) {
				if (group.length && part.floor - group[0].floor >= stories) flush();
				group.push(part);
			}
			flush();
		}
	}
	return merged;
}

function patchRecordFromTenant(
	record: PlacedObjectRecord,
	tenant: TdtTenant,
	snapshot: SimSnapshot,
): void {
	record.unitStatus = tenant.status;
	record.rentLevel = rentLevelFromTdt(tenant.rentRate);
	const sidecar =
		record.linkedRecordIndex >= 0
			? snapshot.world.sidecars[record.linkedRecordIndex]
			: undefined;
	if (sidecar?.kind === "commercial_venue") {
		sidecar.ownerSubtypeIndex = tenant.variant;
	}
	if (sidecar?.kind === "entertainment_link" && tenant.variant < 14) {
		sidecar.familySelectorOrSingleLinkFlag = tenant.variant;
	}
}

function addElevatorOverlay(
	elevator: TdtElevator,
	snapshot: SimSnapshot,
	warnings: string[],
): boolean {
	const tileType = ["elevatorExpress", "elevator", "elevatorService"][
		elevator.type
	];
	if (!tileType) return false;
	const width = TILE_WIDTHS[tileType];
	const bottom = clamp(elevator.bottomFloor, 0, GRID_HEIGHT - 1);
	const top = clamp(elevator.topFloor, 0, GRID_HEIGHT - 1);
	if (
		top <= bottom ||
		elevator.x < 0 ||
		elevator.x + width > GRID_WIDTH
	) {
		warnings.push(
			`Skipped a malformed elevator at column ${elevator.x} (${bottom}-${top}).`,
		);
		return false;
	}
	for (let floor = bottom; floor <= top; floor++) {
		const y = floorToY(floor);
		const anchor = `${elevator.x},${y}`;
		for (let dx = 0; dx < width; dx++) {
			const key = `${elevator.x + dx},${y}`;
			if (snapshot.world.overlays[key] || snapshot.world.overlayToAnchor[key]) {
				warnings.push(
					`Skipped an overlapping elevator at column ${elevator.x}.`,
				);
				return false;
			}
			if (!snapshot.world.cells[key] && !snapshot.world.cellToAnchor[key]) {
				snapshot.world.cells[key] = floor === 10 ? "lobby" : "floor";
			}
		}
		snapshot.world.overlays[anchor] = tileType;
		for (let dx = 1; dx < width; dx++) {
			snapshot.world.overlayToAnchor[`${elevator.x + dx},${y}`] = anchor;
		}
	}
	return true;
}

function addStairOverlay(
	type: number,
	x: number,
	baseFloor: number,
	snapshot: SimSnapshot,
	warnings: string[],
): number {
	if (type < 0 || type > 5) return 0;
	const tileType = type % 2 === 1 ? "stairs" : "escalator";
	const stories = type <= 1 ? 1 : type <= 3 ? 2 : 3;
	const width = TILE_WIDTHS[tileType];
	let placed = 0;
	for (let story = 0; story < stories; story++) {
		const floor = baseFloor + story;
		if (floor < 0 || floor >= GRID_HEIGHT - 1 || x + width > GRID_WIDTH) {
			continue;
		}
		const y = floorToY(floor);
		const anchor = `${x},${y}`;
		let blocked = false;
		for (let dx = 0; dx < width; dx++) {
			const key = `${x + dx},${y}`;
			if (snapshot.world.overlays[key] || snapshot.world.overlayToAnchor[key]) {
				blocked = true;
				break;
			}
		}
		if (blocked) {
			warnings.push(`Skipped an overlapping ${tileType} at column ${x}.`);
			continue;
		}
		snapshot.world.overlays[anchor] = tileType;
		for (let dx = 0; dx < width; dx++) {
			const key = `${x + dx},${y}`;
			if (!snapshot.world.cells[key] && !snapshot.world.cellToAnchor[key]) {
				snapshot.world.cells[key] = floor === 10 ? "lobby" : "floor";
			}
			if (dx > 0) snapshot.world.overlayToAnchor[key] = anchor;
		}
		placed++;
	}
	return placed;
}

function restoreElevatorCars(
	elevators: readonly TdtElevator[],
	snapshot: SimSnapshot,
): void {
	for (const elevator of elevators) {
		const carrier = snapshot.world.carriers.find(
			(candidate) =>
				candidate.column === elevator.x &&
				candidate.carrierMode === elevator.type &&
				candidate.bottomServedFloor === elevator.bottomFloor &&
				candidate.topServedFloor === elevator.topFloor,
		);
		if (!carrier) continue;
		const slots = carrier.topServedFloor - carrier.bottomServedFloor + 1;
		const cars = clamp(elevator.cars, 1, 8);
		carrier.stopFloorEnabled = Array.from({ length: slots }, (_, index) => {
			const floor = carrier.bottomServedFloor + index;
			return elevator.serviced[floor] ? 1 : 0;
		});
		carrier.stopFloorEnabled[0] = 1;
		carrier.stopFloorEnabled[slots - 1] = 1;
		carrier.cars = Array.from({ length: cars }, (_, index) => {
			const home = clamp(
				elevator.carHomes[index] ?? carrier.bottomServedFloor,
				carrier.bottomServedFloor,
				carrier.topServedFloor,
			);
			const car = makeCarrierCar(slots, home);
			car.currentFloor = home;
			car.targetFloor = home;
			car.prevFloor = home;
			car.homeFloor = home;
			car.nearestWorkFloor = home;
			return car;
		});
	}
}

function applyFinance(tdt: TdtTower, snapshot: SimSnapshot): number | null {
	if (!tdt.finance) return null;
	const population = clamp(tdt.finance.towerPopulation, 0, 1_000_000);
	snapshot.world.currentPopulation = population;
	snapshot.world.currentPopulationBuckets = {};
	snapshot.ledger.populationLedger = new Array(256).fill(0);
	snapshot.ledger.incomeLedger = new Array(256).fill(0);
	snapshot.ledger.expenseLedger = new Array(256).fill(0);
	for (let index = 0; index < 10; index++) {
		const family = LEDGER_FAMILIES[index];
		const familyPopulation = Math.max(
			0,
			Math.trunc(tdt.finance.tenantPopulation[index] ?? 0),
		);
		snapshot.world.currentPopulationBuckets[family] = familyPopulation;
		snapshot.ledger.populationLedger[index] = familyPopulation;
		snapshot.ledger.incomeLedger[index] =
			Math.trunc(tdt.finance.tenantIncome[index] ?? 0) * 100;
		snapshot.ledger.expenseLedger[index] =
			Math.trunc(tdt.finance.tenantMaintenance[index] ?? 0) * 100;
	}
	const bucketTotal = Object.values(
		snapshot.world.currentPopulationBuckets,
	).reduce((sum, value) => sum + value, 0);
	if (bucketTotal !== population) {
		snapshot.world.currentPopulationBuckets[255] = population - bucketTotal;
	}
	return population;
}

export function looksLikeLegacyTdt(bytes: Uint8Array): boolean {
	return (
		bytes.byteLength >= 2 &&
		(bytes[0] | (bytes[1] << 8)) === TDT_MAGIC
	);
}

export function importLegacyTdt(
	buffer: ArrayBuffer,
	filename = "TOWER.TDT",
): ImportedLegacyTdt {
	const bytes = new Uint8Array(buffer);
	const tdt = parseTdtBinary(bytes);
	const warnings = [...tdt.warnings];
	const starCount = clamp(Math.trunc(tdt.header.level), 1, 6);
	const dayCounter = clamp(
		Math.trunc(tdt.header.currentDay),
		0,
		ORIGINAL_DAY_WRAP - 1,
	);
	const dayTick = clamp(Math.trunc(tdt.header.frameTime), 0, 2599);
	const cash = Math.max(0, Math.trunc(tdt.header.balance) * 100);
	const towerName = towerNameFromFilename(filename);
	const snapshot = createInitialSnapshot(
		`tdt-${hashBytes(bytes).toString(16).padStart(8, "0")}`,
		towerName,
		cash,
		"perfect-parity",
	);
	snapshot.world.rngState = hashBytes(bytes) | 0;
	snapshot.world.starCount = starCount;
	snapshot.world.lobbyHeight = clamp(
		Math.trunc(tdt.header.lobbyHeight),
		1,
		3,
	);
	snapshot.time.dayTick = dayTick;
	snapshot.time.daypartIndex = Math.floor(dayTick / 400);
	snapshot.time.dayCounter = dayCounter;
	snapshot.time.weekendFlag = dayCounter % 3 === 2 ? 1 : 0;
	snapshot.time.totalTicks = dayCounter * 2600 + dayTick;
	snapshot.ledger.cashBalance = cash;
	snapshot.ledger.cashBalanceCycleBase = cash;

	const structural = new Map<number, Map<number, "floor" | "lobby">>();
	const setStructure = (
		floor: number,
		left: number,
		right: number,
		tileType: "floor" | "lobby",
	): void => {
		if (floor < 0 || floor > MAX_ORIGINAL_FLOOR_SLOT) return;
		const lo = clamp(Math.trunc(left), 0, GRID_WIDTH);
		const hi = clamp(Math.trunc(right), 0, GRID_WIDTH);
		if (hi <= lo) return;
		const row = structural.get(floor) ?? new Map();
		for (let x = lo; x < hi; x++) row.set(x, tileType);
		structural.set(floor, row);
	};

	const singles: Array<{ floor: number; tenant: TdtTenant; tileType: string }> =
		[];
	const parts: PartRecord[] = [];
	for (const floor of tdt.floors) {
		if (floor.index <= 109) {
			setStructure(floor.index, floor.leftEdge, floor.rightEdge, "floor");
		}
		for (const tenant of floor.tenants) {
			const typeId = Math.abs(tenant.type);
			if (typeId === 0 || typeId === 24) {
				setStructure(
					floor.index,
					tenant.left,
					tenant.right,
					typeId === 24 ? "lobby" : "floor",
				);
				continue;
			}
			if (typeId === 45 || typeId === 48) continue;
			const kind = PART_KIND[typeId];
			if (kind) {
				parts.push({
					kind,
					typeId,
					floor: floor.index,
					left: tenant.left,
					right: tenant.right,
					construction: tenant.type < 0,
					tenant,
				});
				continue;
			}
			const tileType = SINGLE_TENANT_TILES[typeId];
			if (tileType) singles.push({ floor: floor.index, tenant, tileType });
			else warnings.push(`Ignored unknown tenant type ${typeId} on slot ${floor.index}.`);
		}
	}

	for (const [floor, row] of structural) {
		const y = floorToY(floor);
		for (const [x, tileType] of row) {
			snapshot.world.cells[`${x},${y}`] = tileType;
		}
	}

	let roomsPlaced = 0;
	let roomsSkipped = 0;
	let constructionConverted = 0;
	for (const { floor, tenant, tileType } of singles) {
		if (floor < 0 || floor > 109) {
			roomsSkipped++;
			continue;
		}
		const width = tenant.right - tenant.left;
		if (width !== TILE_WIDTHS[tileType]) {
			warnings.push(
				`Skipped ${tileType} at slot ${floor}, column ${tenant.left}: width ${width} is not ${TILE_WIDTHS[tileType]}.`,
			);
			roomsSkipped++;
			continue;
		}
		const before = new Set(Object.keys(snapshot.world.placedObjects));
		const result = handlePlaceTile(
			tenant.left,
			floorToY(floor),
			tileType,
			snapshot.world,
			snapshot.ledger,
			true,
			snapshot.time,
			true,
			true,
		);
		if (!result.accepted) {
			warnings.push(
				`Skipped ${tileType} at slot ${floor}, column ${tenant.left}: ${result.reason ?? "invalid placement"}.`,
			);
			roomsSkipped++;
			continue;
		}
		for (const [key, record] of Object.entries(snapshot.world.placedObjects)) {
			if (!before.has(key)) patchRecordFromTenant(record, tenant, snapshot);
		}
		if (tenant.type < 0) constructionConverted++;
		roomsPlaced++;
	}

	for (const merged of mergePartRecords(parts)) {
		const width = merged.right - merged.left;
		if (width !== TILE_WIDTHS[merged.kind]) {
			warnings.push(
				`Skipped ${merged.kind} near slot ${merged.bottom}: width ${width} is not ${TILE_WIDTHS[merged.kind]}.`,
			);
			roomsSkipped++;
			continue;
		}
		const expectedStories = PART_STORIES[merged.kind];
		if (merged.top - merged.bottom + 1 !== expectedStories) {
			warnings.push(
				`${merged.kind} near slot ${merged.bottom} had incomplete story records; the canonical ${expectedStories}-story structure was restored.`,
			);
		}
		const anchorFloor =
			merged.kind === "metro" || merged.kind === "cathedral"
				? merged.top
				: merged.bottom;
		const before = new Set(Object.keys(snapshot.world.placedObjects));
		const result = handlePlaceTile(
			merged.left,
			floorToY(anchorFloor),
			merged.kind,
			snapshot.world,
			snapshot.ledger,
			true,
			snapshot.time,
			true,
			true,
		);
		if (!result.accepted) {
			warnings.push(
				`Skipped ${merged.kind} near slot ${merged.bottom}: ${result.reason ?? "invalid placement"}.`,
			);
			roomsSkipped++;
			continue;
		}
		const added = Object.entries(snapshot.world.placedObjects).filter(
			([key]) => !before.has(key),
		);
		for (const part of merged.parts) {
			const exact = added.find(
				([key, record]) =>
					record.objectTypeCode === part.typeId &&
					yToFloor(Number(key.split(",")[1])) === part.floor,
			);
			if (exact) patchRecordFromTenant(exact[1], part.tenant, snapshot);
		}
		if (merged.parts.some((part) => part.construction)) constructionConverted++;
		roomsPlaced++;
	}

	let elevatorsPlaced = 0;
	for (const elevator of tdt.elevators ?? []) {
		if (addElevatorOverlay(elevator, snapshot, warnings)) elevatorsPlaced++;
	}
	let stairsPlaced = 0;
	for (const stair of tdt.stairs ?? []) {
		stairsPlaced += addStairOverlay(
			stair.type,
			stair.x,
			stair.floor,
			snapshot,
			warnings,
		);
	}

	runGlobalRebuilds(snapshot.world, snapshot.ledger);
	restoreElevatorCars(tdt.elevators ?? [], snapshot);
	const populationImported = applyFinance(tdt, snapshot);
	if (populationImported === null) {
		warnings.push(
			"The finance tail was unavailable, so the imported tower starts with a rebuilt population ledger.",
		);
	}
	if (constructionConverted > 0) {
		warnings.push(
			`${constructionConverted} under-construction record(s) were restored as completed facilities because the native simulation does not persist construction timers.`,
		);
	}

	return {
		snapshot,
		report: {
			filename,
			starCount,
			cash,
			dayCounter,
			dayTick,
			roomsPlaced,
			roomsSkipped,
			constructionConverted,
			elevatorsPlaced,
			stairsPlaced,
			populationImported,
			warnings,
		},
	};
}

export { LegacyImportError };
