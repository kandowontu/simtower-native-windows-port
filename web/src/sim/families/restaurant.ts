// 1228:466d gate_object_family_restaurant_fast_food_state_handler (family 6/12)
// 1228:4851 dispatch_object_family_restaurant_fast_food_state_handler (family 6/12)
//
// Restaurant / fast-food state machine (family 6 restaurant, 12 fast food).
// Phase 5b: the gate splits restaurant / fast-food off from the shared
// commercial processor in `sims/commercial.ts`. The dispatch body remains
// in processCommercialSim which internally gates on familyCode to apply
// restaurant/fast-food-specific quirks (silent-park on VENUE_CLOSED,
// different RNG gates per daypart, etc.).

import type { LedgerState } from "../ledger";
import { FAMILY_FAST_FOOD, FAMILY_RESTAURANT } from "../resources";
import { isSimInTransit } from "../sim-access/state-bits";
import {
	handleCommercialSimArrival as _handleCommercialSimArrival,
	processCommercialSim as _processCommercialSim,
} from "../sims/commercial";
import type { TimeState } from "../time";
import type { SimRecord, WorldState } from "../world";
import { maybeDispatchQueuedRouteAfterWait } from "./maybe-dispatch-after-wait";

/**
 * 1228:466d gate_object_family_restaurant_fast_food_state_handler. Gates on
 * familyCode ∈ {6, 12} and delegates to the shared commercial processor
 * (which holds the restaurant-specific RNG gates: daypart-4 1/12, daypart-5
 * before dayTick 2199, etc.).
 */
export function gateObjectFamilyRestaurantFastFoodStateHandler(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
	sim: SimRecord,
): void {
	if (
		sim.familyCode !== FAMILY_RESTAURANT &&
		sim.familyCode !== FAMILY_FAST_FOOD
	)
		return;
	if (isSimInTransit(sim.stateCode)) {
		maybeDispatchQueuedRouteAfterWait(world, ledger, time, sim);
		return;
	}
	_processCommercialSim(world, ledger, time, sim);
}

/** 1228:4851 dispatch_object_family_restaurant_fast_food_state_handler. */
export function dispatchObjectFamilyRestaurantFastFoodStateHandler(
	world: WorldState,
	sim: SimRecord,
	arrivalFloor: number,
	time: TimeState,
): void {
	_handleCommercialSimArrival(world, sim, arrivalFloor, time);
}
