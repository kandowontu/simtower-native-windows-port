import type { LedgerState } from "./ledger";
import { addToPopulationBucket, clearPopulationBucket } from "./progression";
import {
	FAMILY_CINEMA,
	FAMILY_CINEMA_LOWER,
	FAMILY_CINEMA_STAIRS_LOWER,
	FAMILY_CINEMA_STAIRS_UPPER,
	FAMILY_PARTY_HALL,
	FAMILY_PARTY_HALL_LOWER,
} from "./resources";
import {
	STATE_ACTIVE,
	STATE_ARRIVED,
	STATE_DEPARTURE,
	STATE_MORNING_GATE,
} from "./sims/states";
import type { EntertainmentLinkRecord, WorldState } from "./world";

// Family codes emitted by cinema placement (upper stairway, upper theater,
// lower stairway, lower theater). All 4 share one sidecar.
const CINEMA_FAMILY_CODES = new Set([
	FAMILY_CINEMA,
	FAMILY_CINEMA_LOWER,
	FAMILY_CINEMA_STAIRS_UPPER,
	FAMILY_CINEMA_STAIRS_LOWER,
]);

/**
 * Paired-link budget tiers indexed by `linkAgeCounter / 3`.
 * Selectors 0..6 -> [40, 40, 40, 20], selectors 7..13 -> [60, 60, 40, 20].
 */
function pairedBudget(linkAgeCounter: number, selector: number): number {
	const ageTier = Math.min(3, Math.trunc(linkAgeCounter / 3));
	const lowSelectorTable = [40, 40, 40, 20];
	const highSelectorTable = [60, 60, 40, 20];
	const table =
		selector >= 0 && selector < 7 ? lowSelectorTable : highSelectorTable;
	return table[ageTier] ?? table[table.length - 1] ?? 20;
}

/**
 * A sidecar is "paired" (cinema) iff its `familySelectorOrSingleLinkFlag`
 * is a real selector bucket (0..13). Party-hall records store 0xff.
 */
function isPairedSidecar(sidecar: EntertainmentLinkRecord): boolean {
	return sidecar.familySelectorOrSingleLinkFlag !== 0xff;
}

function simMatchesEntertainmentSidecar(
	sim: { homeColumn: number },
	sidecar: EntertainmentLinkRecord,
): boolean {
	return (
		sim.homeColumn === sidecar.ownerSubtypeIndex ||
		sim.homeColumn === sidecar.ownerSubtypeIndex + 7
	);
}

/**
 * Binary `rebuild_entertainment_family_ledger` @ 1188:05af.
 * Seed entertainment link budgets, increment link age, clear cycle counters.
 * Called at tick 0x0F0 (240) as part of the facility ledger rebuild.
 *
 * Iterates sidecars directly: each placement owns exactly one sidecar,
 * shared by its 4 (cinema) or 2 (party hall) sub-records.
 *
 * Note: the binary does NOT clear `link_phase_state` here — that happens at
 * the previous day's lower-half advance pass. We mirror that.
 */
export function seedEntertainmentBudgets(world: WorldState): void {
	let cinemaPopulation = 0;
	let partyHallPopulation = 0;
	for (const sidecar of world.sidecars) {
		if (sidecar.kind !== "entertainment_link") continue;
		if (sidecar.ownerSubtypeIndex === 0xff) continue;

		sidecar.attendanceCounter = 0;
		sidecar.activeRuntimeCount = 0;
		sidecar.pendingTransitionFlag = 0;
		sidecar.linkAgeCounter = Math.min(0x7f, sidecar.linkAgeCounter + 1);

		if (isPairedSidecar(sidecar)) {
			const budget = pairedBudget(
				sidecar.linkAgeCounter,
				sidecar.familySelectorOrSingleLinkFlag,
			);
			sidecar.upperBudget = budget;
			sidecar.lowerBudget = budget;
			cinemaPopulation += sidecar.upperBudget + sidecar.lowerBudget;
		} else {
			sidecar.upperBudget = 0;
			sidecar.lowerBudget = 50;
			partyHallPopulation += sidecar.lowerBudget;
		}
	}

	clearPopulationBucket(world, FAMILY_CINEMA);
	if (cinemaPopulation > 0) {
		addToPopulationBucket(world, FAMILY_CINEMA, cinemaPopulation);
	}
	clearPopulationBucket(world, FAMILY_PARTY_HALL);
	if (partyHallPopulation > 0) {
		addToPopulationBucket(world, FAMILY_PARTY_HALL, partyHallPopulation);
	}
}

/**
 * Activate paired-link upper-half sims.
 * Sets upper phase to 1 for all paired entertainment links that are idle.
 */
export function activateEntertainmentUpperHalf(world: WorldState): void {
	for (const sidecar of world.sidecars) {
		if (sidecar.kind !== "entertainment_link") continue;
		if (sidecar.ownerSubtypeIndex === 0xff) continue;
		if (!isPairedSidecar(sidecar)) continue;
		if (sidecar.linkPhaseState === 0) {
			sidecar.linkPhaseState = 1;
		}
		for (const sim of world.sims) {
			if (sim.familyCode !== FAMILY_CINEMA) continue;
			if (!simMatchesEntertainmentSidecar(sim, sidecar)) continue;
			sim.stateCode = STATE_MORNING_GATE;
		}
	}
}

/**
 * Tick 0x4B0 (1200) action.
 * Promote paired (cinema) links phase 2 → 3, AND activate party hall lower
 * half: link 0 → 1 plus seed every lower-half occupant sim's stateCode to
 * STATE_MORNING_GATE (0x20).
 *
 * Binary `activate_entertainment_link_half_runtime_phase(half=1, paired=0)`
 * @ 1188:06a8 walks the lower-half occupant span (40 slots) and writes
 * sim[+5] = 0x20 unconditionally. We approximate the per-record span by
 * matching `homeColumn` + family code on the lower half.
 */
export function promoteCinemaAndActivatePartyHall(world: WorldState): void {
	for (const sidecar of world.sidecars) {
		if (sidecar.kind !== "entertainment_link") continue;
		if (sidecar.ownerSubtypeIndex === 0xff) continue;

		if (isPairedSidecar(sidecar)) {
			if (sidecar.linkPhaseState === 2) {
				sidecar.linkPhaseState = 3;
			}
		} else {
			if (sidecar.linkPhaseState === 0) {
				sidecar.linkPhaseState = 1;
			}
		}
	}
}

/**
 * Binary `promote_entertainment_links_to_ready_phase(half=1, paired=1)` @
 * 1188:0826 — checkpoint 0x640 (1600). For every cinema-paired link with
 * `linkPhaseState >= 2`, advances `linkPhaseState` 2 → 3 (the "READY" /
 * post-show phase). Also writes byte +0x13 = 1 on the 4 audience-sim
 * records bound to the link (2 per half), but that flag write is not yet
 * mirrored here. This partial implementation captures the linkPhaseState
 * transition only.
 */
export function promoteCinemaLinksToReadyPhase(world: WorldState): void {
	for (const sidecar of world.sidecars) {
		if (sidecar.kind !== "entertainment_link") continue;
		if (sidecar.ownerSubtypeIndex === 0xff) continue;
		if (!isPairedSidecar(sidecar)) continue;
		if (sidecar.linkPhaseState >= 2) {
			sidecar.linkPhaseState = 3;
		}
	}
}

/**
 * Activate paired-link lower-half sims still in phase 1.
 */
export function activateEntertainmentLowerHalf(world: WorldState): void {
	for (const sidecar of world.sidecars) {
		if (sidecar.kind !== "entertainment_link") continue;
		if (sidecar.ownerSubtypeIndex === 0xff) continue;
		if (!isPairedSidecar(sidecar)) continue;
		if (sidecar.linkPhaseState === 1) {
			sidecar.linkPhaseState = 2;
		}
	}
}

/** Movie-theater (paired) attendance-tiered payout. */
function movieTheaterPayout(attendance: number): number {
	if (attendance >= 100) return 15_000;
	if (attendance >= 80) return 10_000;
	if (attendance >= 40) return 2_000;
	return 0;
}

/**
 * Advance paired-link upper phase.
 * Decrements active runtime count and accrues income for completed upper phases.
 */
export function advanceEntertainmentUpperPhase(world: WorldState): void {
	for (const sidecar of world.sidecars) {
		if (sidecar.kind !== "entertainment_link") continue;
		if (sidecar.ownerSubtypeIndex === 0xff) continue;
		if (!isPairedSidecar(sidecar)) continue;
		if (sidecar.linkPhaseState < 1) continue;

		sidecar.activeRuntimeCount = Math.max(
			0,
			sidecar.activeRuntimeCount - sidecar.upperBudget,
		);
		sidecar.linkPhaseState = sidecar.activeRuntimeCount === 0 ? 1 : 2;

		for (const sim of world.sims) {
			if (!CINEMA_FAMILY_CODES.has(sim.familyCode)) continue;
			if (!simMatchesEntertainmentSidecar(sim, sidecar)) continue;
			if (sim.stateCode === STATE_ARRIVED) {
				sim.stateCode = STATE_ACTIVE;
			}
		}
	}
}

/**
 * Binary check at the entry of `accrue_facility_income_by_family` @ 1180:12e7:
 * skip the cash payout when the day counter lands on a calendar edge. The
 * record-level state reset (link_phase_state = 0) still happens.
 */
function isCalendarEdgePayoutSkipDay(dayCounter: number): boolean {
	return dayCounter % 60 === 59 || dayCounter % 84 === 83;
}

/**
 * Binary `advance_entertainment_facility_phase(half=1, paired=0)` — checkpoint 0x640.
 * Drain party hall attendees one at a time, accrue $20k income (gated by
 * the calendar-edge skip), reset link phase. The drain walks each occupant
 * slot in the lower-half span: for each sim in STATE_ARRIVED, decrement
 * `activeRuntimeCount` and set the sim's stateCode to STATE_DEPARTURE (0x05)
 * so the linked-half routing handler routes it back to the lobby. Sims in
 * any other state are left alone.
 */
export function advancePartyHallPhaseAndAccrue(
	world: WorldState,
	ledger: LedgerState,
	dayCounter: number,
): void {
	for (const sidecar of world.sidecars) {
		if (sidecar.kind !== "entertainment_link") continue;
		if (sidecar.ownerSubtypeIndex === 0xff) continue;
		if (isPairedSidecar(sidecar)) continue;

		const previousPhaseState = sidecar.linkPhaseState;
		sidecar.linkPhaseState = 0;

		for (const sim of world.sims) {
			if (sim.familyCode !== FAMILY_PARTY_HALL_LOWER) continue;
			if (!simMatchesEntertainmentSidecar(sim, sidecar)) continue;
			if (sim.stateCode === STATE_ARRIVED) {
				sim.stateCode = STATE_DEPARTURE;
				sidecar.activeRuntimeCount = Math.max(
					0,
					sidecar.activeRuntimeCount - 1,
				);
			}
		}

		if (previousPhaseState >= 1 && sidecar.attendanceCounter > 0) {
			if (isCalendarEdgePayoutSkipDay(dayCounter)) continue;
			const payout = 20_000;
			ledger.cashBalance = Math.min(99_999_999, ledger.cashBalance + payout);
			ledger.incomeLedger[FAMILY_PARTY_HALL] =
				(ledger.incomeLedger[FAMILY_PARTY_HALL] ?? 0) + payout;
		}
	}
}

/**
 * Binary `advance_entertainment_facility_phase(half=1, paired=1)` — checkpoint 0x76c.
 * Advance lower phase for paired (cinema) links, accrue income with the
 * calendar-edge skip, reset link phase. Payout is attendance-tiered.
 *
 * Cinema and party-hall guest sims are both spawned from their binary-aligned
 * occupant spans, so the per-sim drain semantics here mirror the binary path.
 */
export function advanceEntertainmentLowerPairedPhaseAndAccrue(
	world: WorldState,
	ledger: LedgerState,
	dayCounter: number,
): void {
	for (const sidecar of world.sidecars) {
		if (sidecar.kind !== "entertainment_link") continue;
		if (sidecar.ownerSubtypeIndex === 0xff) continue;
		if (!isPairedSidecar(sidecar)) continue;

		const previousPhaseState = sidecar.linkPhaseState;
		sidecar.linkPhaseState = 0;

		for (const sim of world.sims) {
			if (!CINEMA_FAMILY_CODES.has(sim.familyCode)) continue;
			if (!simMatchesEntertainmentSidecar(sim, sidecar)) continue;
			if (sim.stateCode === STATE_ARRIVED) {
				sim.stateCode = STATE_DEPARTURE;
				sidecar.activeRuntimeCount = Math.max(
					0,
					sidecar.activeRuntimeCount - 1,
				);
			}
		}

		if (previousPhaseState >= 1) {
			if (isCalendarEdgePayoutSkipDay(dayCounter)) continue;
			const payout = movieTheaterPayout(sidecar.attendanceCounter);
			if (payout > 0) {
				ledger.cashBalance = Math.min(99_999_999, ledger.cashBalance + payout);
				ledger.incomeLedger[FAMILY_CINEMA] =
					(ledger.incomeLedger[FAMILY_CINEMA] ?? 0) + payout;
			}
		}
	}
}
