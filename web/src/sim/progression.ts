// Star advancement (binary `check_and_advance_star_rating` @ 1148:002d /
// `compute_tower_tier_from_ledger` @ 1148:041d / `reset_star_gate_state`).
//
// Runs every tick from `FUN_1098_03ab` (the per-tick scheduler that we mirror
// in `tick/carrier-tick.ts`). Two independent checks must both pass for
// starCount to advance:
//
// 1. `computeTowerTierFromLedger()` > current starCount — the ledger total
//    `g_primary_family_ledger_total` (1288:c13a, modeled here as
//    `world.currentPopulation`) crosses the next tier threshold.
//    Thresholds (binary DS:e630..e63c + hardcoded 15000): [300, 1000, 5000,
//    10000, 15000]. Comparison is `>=` (binary uses `< threshold` for the
//    lower-tier branch).
// 2. `checkStarAdvancementConditions()` — all qualitative gates for the
//    current-tier transition are satisfied.
//
// On success, starCount is incremented (capped at 5 here — rank 6 "Tower"
// is reached exclusively via the cathedral evaluation path in cathedral.ts),
// the per-day star-gate flags are reset, and a `star_advanced` notification
// is queued.

import { STAR_THRESHOLDS } from "./resources";
import type { TimeState } from "./time";
import type { WorldState } from "./world";

export function computeStarCountFromPopulation(world: WorldState): number {
	const total = world.currentPopulation;
	let tier = 1;
	for (let index = 0; index < STAR_THRESHOLDS.length; index++) {
		// Binary compares `total < THRESHOLD[index]` to keep tier == index+1;
		// the inverse here (>=) advances tier to index+2.
		if (total >= STAR_THRESHOLDS[index]) tier = index + 2;
	}
	return tier;
}

/**
 * Mirrors binary `add_to_primary_family_ledger_bucket` (1068:07f7).
 * Binary 1068:0812-082b: bucket[family] += amount AND total += amount, with
 * the total update unconditional. We mirror both so the running total stays
 * consistent with the per-family bucket sums and so
 * `clearPopulationBucket` can later subtract the right amount.
 */
export function addToPopulationBucket(
	world: WorldState,
	familyCode: number,
	amount: number,
): void {
	world.currentPopulationBuckets[familyCode] =
		(world.currentPopulationBuckets[familyCode] ?? 0) + amount;
	world.currentPopulation += amount;
}

/**
 * Mirrors binary `clear_primary_family_ledger_bucket` (1068:07b3) at
 * 1068:07d3-07e2: subtract the bucket's current value from the running total,
 * then zero the bucket. Used by daily reseed paths (e.g.
 * `rebuild_linked_facility_records` for non-restaurant commercial families)
 * so that re-adding the new yesterday-visit-count yields a NET delta of
 * `(newVisits - oldVisits)` on the total.
 */
export function clearPopulationBucket(
	world: WorldState,
	familyCode: number,
): void {
	const current = world.currentPopulationBuckets[familyCode] ?? 0;
	world.currentPopulation -= current;
	world.currentPopulationBuckets[familyCode] = 0;
}

export function checkStarAdvancementConditions(
	world: WorldState,
	time: TimeState,
): boolean {
	const flags = world.gateFlags;
	switch (world.starCount) {
		case 1:
			return true;
		case 2:
			return flags.securityPlaced !== 0;
		case 3:
			return (
				flags.officePlaced !== 0 &&
				flags.recyclingAdequate !== 0 &&
				flags.officeServiceOk !== 0 &&
				flags.officeServiceOkMedical !== 0 &&
				flags.routesViable !== 0 &&
				time.daypartIndex >= 4 &&
				time.weekendFlag === 0
			);
		case 4:
			return (
				flags.metroPlaced !== 0 &&
				flags.recyclingAdequate !== 0 &&
				flags.officeServiceOkMedical !== 0 &&
				flags.routesViable !== 0 &&
				time.daypartIndex >= 4 &&
				time.weekendFlag === 0
			);
		default:
			return false;
	}
}

export function resetStarGateState(world: WorldState): void {
	world.gateFlags.officeServiceOk = 0;
}

export function tryAdvanceStarCount(
	world: WorldState,
	time: TimeState,
): boolean {
	if (world.starCount >= 5) return false;
	if (computeStarCountFromPopulation(world) <= world.starCount) return false;
	if (!checkStarAdvancementConditions(world, time)) return false;

	world.starCount += 1;
	resetStarGateState(world);
	world.pendingNotifications.push({
		kind: "star_advanced",
		message: String(world.starCount),
	});
	return true;
}
