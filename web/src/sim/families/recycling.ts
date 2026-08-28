// 1228:4d5b gate_object_family_recycling_center_lower_state_handler (family 33)
// 1228:4ea0 dispatch_object_family_recycling_center_lower_state_handler (family 33)
//
// Recycling-center lower-slice (family 33 / 0x21) state machine. The sims
// for this family use the same state machine as entertainment guests but with
// a coarser gate: state 0x01 only dispatches during dayparts 0–3, after
// dayTick >= 0xf1, and only when RNG % 36 == 0 (1-in-36). The dispatch
// delegates to the entertainment guest dispatch table (0x5b3a via
// dispatch_entertainment_guest_state at 1228:53ad), which runs the
// service-acquisition / linked-half / phase-consumption / release-return
// cycle.
//
// Binary table at cs:0x5221 (4 entries):
//   state 0x01 / 0x41 → handler at 0x530d (daypart+RNG gate then entertainment dispatch)
//   state 0x22 / 0x62 → handler at 0x537f (unconditional entertainment dispatch)
//
// Binary quirk: the state-0x01 recycling dispatch handler (body at 0x530d)
// applies an ADDITIONAL daypart+RNG gate (daypart 0–3, dayTick > 0xf0, RNG%6==0)
// before delegating to entertainment dispatch, and unconditionally forces state
// 0x27 (parked) when daypart >= 4. This is separate from the gate check in
// gate_object_family_recycling_center_lower_state_handler.

import type { LedgerState } from "../ledger";
import { FAMILY_RECYCLING_CENTER_LOWER } from "../resources";
import { isSimInTransit } from "../sim-access/state-bits";
import type { TimeState } from "../time";
import type { SimRecord, WorldState } from "../world";
import { sampleRng } from "../world";
import { dispatchEntertainmentGuestState } from "./entertainment";
import { maybeDispatchQueuedRouteAfterWait } from "./maybe-dispatch-after-wait";

// Binary state constants for family 33 (match the entertainment guest machine).
const STATE_RECYCLING_ACTIVE = 0x01; // outbound / venue-seeking
const STATE_RECYCLING_VENUE_DWELL = 0x22; // at venue (pre-release)
const STATE_RECYCLING_ACTIVE_TRANSIT = 0x41; // 0x01 + in-transit bit
const STATE_RECYCLING_DWELL_TRANSIT = 0x62; // 0x22 + in-transit bit
const STATE_RECYCLING_PARKED = 0x27; // parked / idle

// Timing constants from the binary (g_day_tick thresholds).
const RECYCLING_GATE_DAY_TICK_MIN = 0xf1; // dayTick must be >= this for state-0x01 to fire
const RECYCLING_PARK_DAY_TICK = 0x8fd; // dayTick >= this resets state-0x27 to 0x01

// RNG thresholds (from gate and dispatch bodies).
const RECYCLING_GATE_RNG_MODULO = 0x24; // 36: gate fires on RNG % 36 == 0
const RECYCLING_DISPATCH_RNG_MODULO = 6; // inner dispatch gate for state-0x01

/**
 * 1228:4d5b gate_object_family_recycling_center_lower_state_handler.
 *
 * Two-tier gate:
 *   - stateCode < 0x40: switch on state; apply timing/RNG gate for state 0x01;
 *     reset state 0x27 to 0x01 at day end; fall through for state 0x22.
 *   - stateCode >= 0x40: if encodedRouteTarget >= 0x40, delegate to
 *     maybeDispatchQueuedRouteAfterWait; else fall through to dispatch.
 */
export function gateObjectFamilyRecyclingCenterLowerStateHandler(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
): void {
	if (sim.familyCode !== FAMILY_RECYCLING_CENTER_LOWER) return;

	if (!isSimInTransit(sim.stateCode)) {
		const state = sim.stateCode;

		if (state === STATE_RECYCLING_ACTIVE) {
			// Binary quirk: gate on daypart 0–3, dayTick >= 0xf1, and 1-in-36 RNG.
			if (time.daypartIndex < 0 || time.daypartIndex > 3) return;
			if (time.dayTick < RECYCLING_GATE_DAY_TICK_MIN) return;
			if (sampleRng(world) % RECYCLING_GATE_RNG_MODULO !== 0) return;
		} else if (state === STATE_RECYCLING_PARKED) {
			// Binary quirk: state 0x27 resets itself to 0x01 once dayTick >= 0x8fd.
			if (time.dayTick < RECYCLING_PARK_DAY_TICK) return;
			sim.stateCode = STATE_RECYCLING_ACTIVE;
			return;
		} else if (state !== STATE_RECYCLING_VENUE_DWELL) {
			return;
		}
		// state 0x22 falls through to dispatch.
	} else {
		// In-transit: if still waiting on a carrier queue, delegate to the
		// route-failure timeout handler. Otherwise fall through to dispatch.
		if (sim.route.mode === "carrier") {
			maybeDispatchQueuedRouteAfterWait(world, ledger, time, sim);
			return;
		}
	}

	dispatchObjectFamilyRecyclingCenterLowerStateHandler(world, time, sim);
}

/**
 * 1228:4ea0 dispatch_object_family_recycling_center_lower_state_handler.
 *
 * Jump table at cs:0x5221 (4 entries, states 0x01/0x22/0x41/0x62):
 *   - 0x01 / 0x41 → handler at 0x530d: daypart 0–3, dayTick > 0xf0, RNG%6==0
 *     → delegate to entertainment dispatch; if daypart >= 4, force state 0x27.
 *   - 0x22 / 0x62 → handler at 0x537f: unconditionally delegate to entertainment
 *     dispatch.
 *
 * Binary prologue: when stateCode >= 0x40 AND encodedRouteTarget is a
 * carrier index (>= 0 and < 0x40), call decrement_route_queue_direction_load
 * before entering the table. TS: carried implicitly by `evictCarrierRoute` in
 * the cancel path; no explicit call needed here.
 */
export function dispatchObjectFamilyRecyclingCenterLowerStateHandler(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	const state = sim.stateCode;

	if (
		state === STATE_RECYCLING_ACTIVE ||
		state === STATE_RECYCLING_ACTIVE_TRANSIT
	) {
		// Binary quirk (handler at 0x530d): the dispatch applies a second daypart +
		// dayTick > 0xf0 + RNG%6==0 gate before delegating to entertainment dispatch.
		// If daypart is already >= 4, skip the dispatch entirely and park the sim.
		if (
			time.daypartIndex >= 0 &&
			time.daypartIndex <= 3 &&
			time.dayTick > 0xf0 &&
			sampleRng(world) % RECYCLING_DISPATCH_RNG_MODULO === 0
		) {
			dispatchEntertainmentGuestState(world, time, sim);
		}
		if (time.daypartIndex > 3) {
			sim.stateCode = STATE_RECYCLING_PARKED;
		}
		return;
	}

	if (
		state === STATE_RECYCLING_VENUE_DWELL ||
		state === STATE_RECYCLING_DWELL_TRANSIT
	) {
		// Binary quirk (handler at 0x537f): unconditional delegation.
		dispatchEntertainmentGuestState(world, time, sim);
		return;
	}
}
