import {
	FAMILY_CINEMA,
	FAMILY_CINEMA_LOWER,
	FAMILY_CINEMA_STAIRS_LOWER,
	FAMILY_CINEMA_STAIRS_UPPER,
	FAMILY_CONDO,
	FAMILY_FAST_FOOD,
	FAMILY_HOTEL_SINGLE,
	FAMILY_HOTEL_SUITE,
	FAMILY_HOTEL_TWIN,
	FAMILY_OFFICE,
	FAMILY_PARTY_HALL,
	FAMILY_PARTY_HALL_LOWER,
	FAMILY_RESTAURANT,
	FAMILY_RETAIL,
	PARKING_EXPENSE_RATE_BY_STAR,
	QUARTERLY_EXPENSES,
	YEN_1001,
} from "./resources";
import { ENTITY_POPULATION_BY_TYPE } from "./sims/states";
import {
	UNDERGROUND_FLOORS,
	VENUE_DORMANT,
	type WorldState,
	yToFloor,
} from "./world";

// ─── Three-ledger money model ─────────────────────────────────────────────────
//
// cashBalance          — live balance, capped at 99,999,999
// populationLedger[f]  — live per-family active-unit counts
// incomeLedger[f]      — income accumulated since last 3-day rollover
// expenseLedger[f]     — expenses accumulated since last 3-day rollover
// cashBalanceCycleBase — balance saved at each rollover (net delta reporting)
//
// YEN #1001 / #1002 values are in units of ¥1,000.

const YEN_UNIT = 1_000;
const CASH_CAP = 99_999_999;

const LEDGER_OFFICE = 0;
const LEDGER_HOTEL_SINGLE = 1;
const LEDGER_HOTEL_TWIN = 2;
const LEDGER_HOTEL_SUITE = 3;
const LEDGER_RETAIL = 4;
const LEDGER_FAST_FOOD = 5;
const LEDGER_RESTAURANT = 6;
const LEDGER_PARTY_HALL = 7;
const LEDGER_CINEMA = 8;
const LEDGER_CONDO = 9;

export interface LedgerState {
	cashBalance: number;
	/** Live per-family active-unit counts indexed by objectTypeCode. */
	populationLedger: number[];
	/** Income since last 3-day rollover, indexed by objectTypeCode. */
	incomeLedger: number[];
	/** Expenses since last 3-day rollover, indexed by objectTypeCode. */
	expenseLedger: number[];
	/** Balance saved at last rollover. */
	cashBalanceCycleBase: number;
}

export function createLedgerState(startingCash: number): LedgerState {
	return {
		cashBalance: startingCash,
		populationLedger: new Array(10).fill(0),
		incomeLedger: new Array(10).fill(0),
		expenseLedger: new Array(256).fill(0),
		cashBalanceCycleBase: startingCash,
	};
}

export function familyToLedgerIndex(familyCode: number): number {
	switch (familyCode) {
		case FAMILY_OFFICE:
			return LEDGER_OFFICE;
		case FAMILY_HOTEL_SINGLE:
			return LEDGER_HOTEL_SINGLE;
		case FAMILY_HOTEL_TWIN:
			return LEDGER_HOTEL_TWIN;
		case FAMILY_HOTEL_SUITE:
			return LEDGER_HOTEL_SUITE;
		case FAMILY_RETAIL:
			return LEDGER_RETAIL;
		case FAMILY_FAST_FOOD:
			return LEDGER_FAST_FOOD;
		case FAMILY_RESTAURANT:
			return LEDGER_RESTAURANT;
		case FAMILY_PARTY_HALL:
		case FAMILY_PARTY_HALL_LOWER:
			return LEDGER_PARTY_HALL;
		case FAMILY_CINEMA:
		case FAMILY_CINEMA_LOWER:
		case FAMILY_CINEMA_STAIRS_UPPER:
		case FAMILY_CINEMA_STAIRS_LOWER:
			return LEDGER_CINEMA;
		case FAMILY_CONDO:
			return LEDGER_CONDO;
		default:
			return -1;
	}
}

// ─── Income ───────────────────────────────────────────────────────────────────

/**
 * Credit checkout/activation income for a placed object using YEN #1001.
 * Called by sim checkout handlers (Phase 4). tileName is the canonical
 * string key (e.g. "hotelSingle", "office").
 */
export function addCashflowFromFamilyResource(
	ledger: LedgerState,
	tileName: string,
	rentLevel: number,
	familyCode: number,
): void {
	const payouts = YEN_1001[tileName];
	if (!payouts) return;
	const amount = payouts[Math.min(rentLevel, 3)] * YEN_UNIT;
	ledger.cashBalance = Math.min(CASH_CAP, ledger.cashBalance + amount);
	if (familyCode >= 0 && familyCode < 256) {
		const index = familyToLedgerIndex(familyCode);
		if (index === -1) return;
		ledger.incomeLedger[index] += amount;
	}
}

export function removeCashflowFromFamilyResource(
	ledger: LedgerState,
	tileName: string,
	rentLevel: number,
	familyCode: number,
): void {
	const payouts = YEN_1001[tileName];
	if (!payouts) return;
	const amount = payouts[Math.min(rentLevel, 3)] * YEN_UNIT;
	ledger.cashBalance = Math.max(0, ledger.cashBalance - amount);
	if (familyCode >= 0 && familyCode < 256) {
		const index = familyToLedgerIndex(familyCode);
		if (index === -1) return;
		ledger.incomeLedger[index] = Math.max(
			0,
			ledger.incomeLedger[index] - amount,
		);
	}
}

// ─── Expense sweep ────────────────────────────────────────────────────────────

/**
 * Charge operating expenses for all placed tiles (YEN #1002).
 * Called at checkpoint 0x09e5 every 3 days.
 */
export function doExpenseSweep(ledger: LedgerState, world: WorldState): void {
	const parkingRate =
		PARKING_EXPENSE_RATE_BY_STAR[Math.min(world.starCount, 5)] ?? 0;

	const lobbyFloor = UNDERGROUND_FLOORS; // internal floor index of ground lobby
	const lobbyHeight = Math.max(1, world.lobbyHeight ?? 1);

	for (const [key, obj] of Object.entries(world.placedObjects)) {
		const code = obj.objectTypeCode;

		// Parking: star-dependent rate × width / 10
		if (code === 0x18) {
			if (parkingRate > 0) {
				// Skip upper floors of multi-floor lobby
				const [, y] = key.split(",").map(Number);
				const floor = yToFloor(y);
				if (floor >= lobbyFloor + 1 && floor < lobbyFloor + lobbyHeight) {
					continue;
				}
				const width = obj.rightTileIndex - obj.leftTileIndex + 1;
				const amount = Math.trunc((width * parkingRate) / 10) * YEN_UNIT;
				ledger.cashBalance = Math.max(0, ledger.cashBalance - amount);
				ledger.expenseLedger[code] += amount;
			}
			continue;
		}

		const rate = QUARTERLY_EXPENSES[code];
		if (!rate) continue;
		const amount = rate * YEN_UNIT;
		ledger.cashBalance = Math.max(0, ledger.cashBalance - amount);
		if (code >= 0 && code < 256) {
			ledger.expenseLedger[code] += amount;
		}
	}

	for (const carrier of world.carriers) {
		const activeCarCount = carrier.cars.filter((car) => car.active).length;
		if (activeCarCount === 0) continue;
		const familyCode =
			carrier.carrierMode === 0 ? 42 : carrier.carrierMode === 2 ? 43 : 1;
		const rate = QUARTERLY_EXPENSES[familyCode];
		if (!rate) continue;
		const amount = rate * YEN_UNIT * activeCarCount;
		ledger.cashBalance = Math.max(0, ledger.cashBalance - amount);
		ledger.expenseLedger[familyCode] += amount;
	}

	for (const segment of world.specialLinks) {
		if (!segment.active) continue;
		const units = Math.max(1, segment.flags >> 1);
		// Binary: bit 0 clear → escalator (family 27); bit 0 set → stairs (family 22, rate 0).
		const familyCode = (segment.flags & 1) === 1 ? 22 : 27;
		const rate = QUARTERLY_EXPENSES[familyCode];
		if (!rate) continue;
		const amount = rate * YEN_UNIT * units;
		ledger.cashBalance = Math.max(0, ledger.cashBalance - amount);
		ledger.expenseLedger[familyCode] += amount;
	}
}

// ─── Facility ledger rebuild ──────────────────────────────────────────────────

/**
 * Rebuild populationLedger count by sweeping all placedObjects.
 * Maps to rebuild_facility_ledger in the binary.
 * Called at checkpoint 0x00f0 (start of day).
 */
export function rebuildFacilityLedger(
	ledger: LedgerState,
	world: WorldState,
): void {
	ledger.populationLedger.fill(0);
	for (const obj of Object.values(world.placedObjects)) {
		const code = obj.objectTypeCode;
		if (code >= 0 && code < 256) {
			const index = familyToLedgerIndex(code);
			if (index === -1) continue;
			const pop = ENTITY_POPULATION_BY_TYPE[code] ?? 0;
			ledger.populationLedger[index] += pop;
		}
	}
}

// ─── 3-day rollover ───────────────────────────────────────────────────────────

/**
 * Called at checkpoint 0x09e5.
 * If this is a 3-day boundary (dayCounter % 3 === 0), run the full expense
 * sweep, save the cycle base, and reset rolling ledgers.
 */
export function doLedgerRollover(
	ledger: LedgerState,
	_world: WorldState,
	dayCounter: number,
): void {
	if (dayCounter % 3 !== 0) return;
	ledger.cashBalanceCycleBase = ledger.cashBalance;
	ledger.incomeLedger.fill(0);
	ledger.expenseLedger.fill(0);
}

/**
 * Mirrors binary `recompute_all_operational_status_and_cashflow` at the 3-day
 * checkpoint. Offices activate unconditionally; retail activates only once its
 * occupied flag (+0x14) has been set (by the scoring sweep's first
 * eval_level>0 pass). Both share auxValueOrTimer with the per-sim handlers as
 * a once-per-cycle guard.
 *
 * Offices here only re-pay if they're currently operational
 * (`occupiedFlag === 1`, binary +0x14) AND have been rented at least once
 * (`auxValueOrTimer !== 0`, flipped by a prior per-sim morning-gate dispatch).
 * Never-rented offices (unreachable from the lobby, e.g. above a sky lobby
 * without a transfer) stay at the initial `auxValueOrTimer === 0`, and
 * stress-vacated offices get `occupiedFlag === 0` via
 * `refreshOccupiedFlagAndTripCounters` branch C. Both skip here, matching the
 * binary where office rent only credits through `activate_office_cashflow`
 * (1180:0d2e) from the per-sim state-0x20 path.
 */
export function activateThreeDayCashflow(
	world: WorldState,
	ledger: LedgerState,
	dayCounter: number,
): void {
	const guard = dayCounter + 1;
	for (const obj of Object.values(world.placedObjects)) {
		if (obj.objectTypeCode === FAMILY_OFFICE) {
			if (obj.auxValueOrTimer === guard) continue;
			if (obj.auxValueOrTimer === 0) continue;
			if (obj.occupiedFlag !== 1) continue;
			obj.auxValueOrTimer = guard;
			addCashflowFromFamilyResource(
				ledger,
				"office",
				obj.rentLevel,
				FAMILY_OFFICE,
			);
			continue;
		}
		if (obj.objectTypeCode === FAMILY_RETAIL) {
			if (obj.auxValueOrTimer === guard) continue;
			if (obj.occupiedFlag !== 1) continue;
			// Binary activate_family_cashflow_if_operational (1138:0bad) gates
			// family 10 on the linked CommercialVenueRecord.availability_state
			// being >= 0 (signed). VENUE_DORMANT (0xff → -1) is excluded.
			const sidecar =
				obj.linkedRecordIndex >= 0
					? world.sidecars[obj.linkedRecordIndex]
					: undefined;
			if (
				sidecar?.kind !== "commercial_venue" ||
				sidecar.availabilityState === VENUE_DORMANT
			) {
				continue;
			}
			obj.auxValueOrTimer = guard;
			addCashflowFromFamilyResource(
				ledger,
				"retail",
				obj.rentLevel,
				FAMILY_RETAIL,
			);
		}
	}
}
