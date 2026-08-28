import {
	recomputeCommercialVenueBuckets,
	recomputeRestaurantVenueBuckets,
} from "../families/entertainment";
import {
	addCashflowFromFamilyResource,
	type LedgerState,
	removeCashflowFromFamilyResource,
} from "../ledger";
import { addToPopulationBucket, clearPopulationBucket } from "../progression";
import {
	COMMERCIAL_CAPACITY_CAPS,
	COMMERCIAL_CLOSURE_BANDS,
	COMMERCIAL_CLOSURE_PAYOUTS,
	FAMILY_CODE_TO_TILE,
	FAMILY_CONDO,
	FAMILY_FAST_FOOD,
	FAMILY_RESTAURANT,
	FAMILY_RETAIL,
	OP_SCORE_THRESHOLDS,
} from "../resources";
import type { TimeState } from "../time";
import {
	type CommercialVenueRecord,
	type PlacedObjectRecord,
	type SimRecord,
	VENUE_AVAILABLE,
	VENUE_CLOSED,
	VENUE_DORMANT,
	VENUE_PARTIAL,
	type WorldState,
	yToFloor,
} from "../world";
import { clearSimRoute } from "./population";
import {
	STATE_PARKED,
	UNIT_STATUS_CONDO_VACANT,
	UNIT_STATUS_CONDO_VACANT_EVENING,
} from "./states";

/**
 * Mirrors binary `select_facility_progress_slot` (11b0:1870). Selects
 * which seed slot to use: override (5) when facilityProgressOverride is
 * active, phaseB (4) when calendarPhaseFlag is set (weekendFlag, i.e.
 * dayCounter % 3 === 2), otherwise phaseA (3).
 */
export function selectFacilityProgressSlot(
	world: WorldState,
	time: TimeState,
): 3 | 4 | 5 {
	if (world.gateFlags.facilityProgressOverride !== 0) return 5;
	if (time.weekendFlag !== 0) return 4;
	return 3;
}

function readSeedForSlot(
	record: CommercialVenueRecord,
	slot: 3 | 4 | 5,
): number {
	if (slot === 5) return record.overrideSeed;
	if (slot === 4) return record.phaseBSeed;
	return record.phaseASeed;
}

function writeSeedForSlot(
	record: CommercialVenueRecord,
	slot: 3 | 4 | 5,
	value: number,
): void {
	if (slot === 5) record.overrideSeed = value;
	else if (slot === 4) record.phaseBSeed = value;
	else record.phaseASeed = value;
}

/**
 * Mirrors binary `rebuild_linked_facility_records` (11b0:0184) →
 * `recompute_facility_runtime_state` (11b0:02f2), invoked at checkpoint
 * 0x0f0 (dayTick 240 / daypart 0). For each valid fast-food or retail
 * venue, computes the daily capacity from the active phase seed, caps it
 * at the type-specific tuning limit, floors at 10, and writes the
 * eligibility threshold. Restaurants are excluded — they use a separate
 * per-cycle mechanism at tick 1600.
 */
export function rebuildCommercialVenueRuntime(
	world: WorldState,
	time: TimeState,
): void {
	const slot = selectFacilityProgressSlot(world, time);
	// Binary `rebuild_linked_facility_records` (11b0:0184) opens by clearing
	// the per-family ledger buckets for non-restaurant commercial families
	// before iterating records and re-adding each record's yesterday-visit-
	// count. The clear-then-add yields a NET delta of (new - old) on the
	// running total, matching the binary's daily population accounting.
	clearPopulationBucket(world, FAMILY_FAST_FOOD);
	clearPopulationBucket(world, FAMILY_RETAIL);
	for (const obj of Object.values(world.placedObjects)) {
		const code = obj.objectTypeCode;
		if (code !== FAMILY_FAST_FOOD && code !== FAMILY_RETAIL) continue;
		if (obj.linkedRecordIndex < 0) continue;
		const record = world.sidecars[obj.linkedRecordIndex];
		if (!record || record.kind !== "commercial_venue") continue;
		if (record.availabilityState !== VENUE_DORMANT) {
			record.availabilityState = VENUE_AVAILABLE;
		}

		// Binary recompute_facility_runtime_state: capacity always comes from
		// phaseASeed (offset +3), regardless of the active calendar slot.
		// The active slot determines which seed is CLEARED (and which is
		// incremented by visits via clamp_object_type_limit).
		const caps = COMMERCIAL_CAPACITY_CAPS[code];
		let cap = record.phaseASeed;
		if (caps && cap > caps[0]) cap = caps[0];
		if (cap < 10) cap = 10;

		record.remainingCapacity = cap;
		record.eligibilityThreshold = -(cap + 1);

		// Roll visit counters (binary: record[8] = record[7], then clear).
		// Binary recompute_facility_runtime_state @ 11b0:0461 calls
		// add_to_primary_family_ledger_bucket(family_code, record[+8]) AFTER
		// the roll, so yesterday's daily visit count contributes to the
		// running primary-family-ledger total used by star advancement.
		record.yesterdayVisitCount = record.todayVisitCount;
		addToPopulationBucket(world, code, record.yesterdayVisitCount);
		record.todayVisitCount = 0;
		record.acquireCount = 0;
		record.currentPopulation = 0;
		record.visitCount = 0;

		// Clear the active calendar slot (not necessarily phaseA).
		writeSeedForSlot(record, slot, 0);
	}
	recomputeCommercialVenueBuckets(world);
}

/**
 * Mirrors binary `rebuild_type6_facility_records` (11b0:0250) — type 6 is
 * the restaurant family. Invoked at checkpoint 0x640 (dayTick 1600 / midday):
 * restaurants use a per-cycle seeding that fires at midday instead of daypart
 * 0. For each restaurant venue, reopens it (availabilityState → AVAILABLE if
 * not DORMANT), refills remainingCapacity (floor 10), resets eligibility
 * threshold to -(cap+1), and rolls visit counters.
 */
export function rebuildRestaurantFacilityRecords(
	world: WorldState,
	time: TimeState,
): void {
	const slot = selectFacilityProgressSlot(world, time);
	// Binary `rebuild_type6_facility_records` (11b0:0250) clears the
	// per-family bucket then iterates restaurants through
	// `recompute_facility_runtime_state`, which calls
	// `add_to_primary_family_ledger_bucket(6, record[+8])` after rolling the
	// day counter. Mirrors the fast-food/retail clear+add pattern.
	clearPopulationBucket(world, FAMILY_RESTAURANT);
	for (const obj of Object.values(world.placedObjects)) {
		if (obj.objectTypeCode !== FAMILY_RESTAURANT) continue;
		if (obj.linkedRecordIndex < 0) continue;
		const record = world.sidecars[obj.linkedRecordIndex] as
			| CommercialVenueRecord
			| undefined;
		if (!record || record.kind !== "commercial_venue") continue;
		if (record.availabilityState !== VENUE_DORMANT) {
			record.availabilityState = VENUE_AVAILABLE;
		}

		// Capacity always from phaseASeed; active slot is cleared.
		const caps = COMMERCIAL_CAPACITY_CAPS[FAMILY_RESTAURANT];
		let cap = record.phaseASeed;
		if (caps && cap > caps[0]) cap = caps[0];
		if (cap < 10) cap = 10;

		record.remainingCapacity = cap;
		record.eligibilityThreshold = -(cap + 1);
		record.yesterdayVisitCount = record.todayVisitCount;
		addToPopulationBucket(world, FAMILY_RESTAURANT, record.yesterdayVisitCount);
		record.todayVisitCount = 0;
		record.acquireCount = 0;
		record.currentPopulation = 0;
		record.visitCount = 0;
		writeSeedForSlot(record, slot, 0);
	}
	recomputeRestaurantVenueBuckets(world);
}

export function closeCommercialVenues(world: WorldState): void {
	for (const record of world.sidecars) {
		if (record.kind !== "commercial_venue") continue;
		record.availabilityState = VENUE_CLOSED;
	}
}

/**
 * Close commercial venues of a single family and accrue closure income.
 * Mirrors `seed_facility_runtime_link_state` for non-type-6 (tick 2000)
 * and type-6 (tick 2200) sweeps. Restaurant/fast-food payouts come from
 * `derive_commercial_venue_state_code`; retail returns 0.
 */
export function closeCommercialVenuesByFamily(
	world: WorldState,
	ledger: LedgerState,
	familyCode: number,
): void {
	const tileName = FAMILY_CODE_TO_TILE[familyCode];
	const payouts = tileName ? COMMERCIAL_CLOSURE_PAYOUTS[tileName] : undefined;

	for (const object of Object.values(world.placedObjects)) {
		if (object.objectTypeCode !== familyCode) continue;
		if (object.linkedRecordIndex < 0) continue;
		const record = world.sidecars[object.linkedRecordIndex] as
			| CommercialVenueRecord
			| undefined;
		if (!record || record.kind !== "commercial_venue") continue;
		if (record.availabilityState === VENUE_DORMANT) {
			continue;
		}

		if (payouts) {
			// Binary `derive_commercial_venue_state_code` (11b0:1731) is called
			// from `seed_facility_runtime_link_state` with `record+0x10`
			// (acquireCount, the visitor-acquisition word incremented by both
			// try_consume_commercial_venue_capacity at MORNING_GATE AND
			// acquire_commercial_venue_slot on visitor arrivals). Read
			// `acquireCount` here (NOT `todayVisitCount`, which is the
			// staff-emit byte at +0x7 that feeds the population/star ledger
			// instead) so closure cash bands match the binary's input.
			const visits = record.acquireCount;
			let band = 0;
			for (const threshold of COMMERCIAL_CLOSURE_BANDS) {
				if (visits >= threshold) band += 1;
			}
			const yenK = payouts[band] ?? 0;
			if (yenK !== 0) {
				applyClosureCash(ledger, familyCode, yenK);
			}
		}

		record.availabilityState = VENUE_CLOSED;
	}
}

function applyClosureCash(
	ledger: LedgerState,
	familyCode: number,
	yenK: number,
): void {
	const amount = yenK * 1_000;
	if (amount > 0) {
		ledger.cashBalance = Math.min(99_999_999, ledger.cashBalance + amount);
		if (familyCode >= 0 && familyCode < 256) {
			ledger.incomeLedger[familyCode] += amount;
		}
	} else {
		const debit = -amount;
		ledger.cashBalance = Math.max(0, ledger.cashBalance - debit);
		if (familyCode >= 0 && familyCode < 256) {
			ledger.expenseLedger[familyCode] += debit;
		}
	}
}

export function activateRetailShop(
	world: WorldState,
	object: PlacedObjectRecord,
	record: CommercialVenueRecord,
	ledger: LedgerState,
): void {
	record.availabilityState = VENUE_PARTIAL;

	addCashflowFromFamilyResource(
		ledger,
		"retail",
		object.rentLevel,
		object.objectTypeCode,
	);
	addToPopulationBucket(world, FAMILY_RETAIL, 10);
}

function deactivateRetailShop(
	world: WorldState,
	object: PlacedObjectRecord,
	record: CommercialVenueRecord,
	ledger: LedgerState,
): void {
	record.availabilityState = VENUE_DORMANT;
	// Binary deactivate_retail_shop_cashflow (1180:1255):
	//   1180:1296: MOV byte ptr ES:[BX+0x13],1  (dirty)
	//   1180:12a6: MOV byte ptr ES:[BX+0x14],0  (occupied cleared)
	object.dirtyFlag = 1;
	object.occupiedFlag = 0;
	object.activationTickCount = 0;

	removeCashflowFromFamilyResource(
		ledger,
		"retail",
		object.rentLevel,
		object.objectTypeCode,
	);
	addToPopulationBucket(world, FAMILY_RETAIL, -10);
}

export function refundUnhappyFacilities(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
): void {
	for (const [key, object] of Object.entries(world.placedObjects)) {
		if (object.objectTypeCode === FAMILY_CONDO) {
			if (object.evalLevel !== 0) continue;
			if (object.unitStatus >= UNIT_STATUS_CONDO_VACANT) continue;
			removeCashflowFromFamilyResource(
				ledger,
				"condo",
				object.rentLevel,
				object.objectTypeCode,
			);
			addToPopulationBucket(world, FAMILY_CONDO, -3);
			object.unitStatus =
				time.daypartIndex < 4
					? UNIT_STATUS_CONDO_VACANT
					: UNIT_STATUS_CONDO_VACANT_EVENING;
			// Binary revert_condo_to_unsold (1180:1102):
			//   1180:115b: MOV byte ptr ES:[BX+0x13],1  (dirty)
			//   1180:116c: MOV byte ptr ES:[BX+0x14],0  (occupied cleared)
			object.dirtyFlag = 1;
			object.occupiedFlag = 0;
			object.activationTickCount = 0;

			const [x, y] = key.split(",").map(Number);
			for (const sim of world.sims) {
				if (sim.homeColumn === x && sim.floorAnchor === yToFloor(y)) {
					sim.stateCode = STATE_PARKED;
					sim.selectedFloor = sim.floorAnchor;
					sim.destinationFloor = -1;
					sim.venueReturnState = 0;
					clearSimRoute(sim);
				}
			}
			continue;
		}

		if (object.objectTypeCode === FAMILY_RETAIL) {
			if (object.evalLevel !== 0) continue;
			if (object.linkedRecordIndex < 0) continue;
			const record = world.sidecars[object.linkedRecordIndex] as
				| CommercialVenueRecord
				| undefined;
			if (!record || record.kind !== "commercial_venue") continue;
			if (record.availabilityState === VENUE_DORMANT) continue;
			deactivateRetailShop(world, object, record, ledger);
		}
	}
}

/**
 * Mirrors binary `clamp_object_type_limit` (11b0:121c). After each sim
 * visit acquisition, increments the active phase seed by +2 (low stress)
 * or +1 (medium stress), capped at the type-specific tuning limit.
 * The active slot is selected by `selectFacilityProgressSlot`.
 *
 * Stress is `compute_runtime_tile_stress_average` (1138:037b): average
 * elapsed ticks per trip = accumulatedTicks / tripCount (0 when no trips).
 * Thresholds from DS:0xe5ea/0xe5ec match OP_SCORE_THRESHOLDS: [80, 150]
 * for stars 1–3, [80, 200] for stars 4–5.
 */
export function incrementVenueSeed(
	record: CommercialVenueRecord,
	familyCode: number,
	sim: SimRecord,
	world: WorldState,
	time: TimeState,
): void {
	const caps = COMMERCIAL_CAPACITY_CAPS[familyCode];
	if (!caps) return;
	const slot = selectFacilityProgressSlot(world, time);
	const current = readSeedForSlot(record, slot);

	// Binary: stress = accumulated_elapsed / sample_count, or 0 if no trips.
	const stress =
		sim.tripCount > 0 ? Math.trunc(sim.accumulatedTicks / sim.tripCount) : 0;
	const [lower, upper] = OP_SCORE_THRESHOLDS[Math.min(world.starCount, 5)] ?? [
		80, 150,
	];
	const increment = stress < lower ? 2 : stress < upper ? 1 : 0;

	writeSeedForSlot(record, slot, Math.min(current + increment, caps[0]));
}
