// 1098:03ab carrier_tick (FUN_1098_03ab)
//
// Per-tick driver for the elevator + sim subsystem. Order matches the binary:
//   check_and_advance_star_rating              (1148:002d)
//   refresh_runtime_entities_for_tick_stride   (1228:0d64)
//   for each carrier:
//     for each active car: advance_carrier_car_state     (1098:06fb)
//     for each active car: dispatch_carrier_car_arrivals (1218:07a6)
//     for each active car: process_unit_travel_queue     (1218:0351)
//
import {
	advanceCarrierCarState,
	dispatchCarrierCarArrivals,
	processUnitTravelQueue,
	resetCarrierTickBookkeeping,
} from "../carriers";
import type { LedgerState } from "../ledger";
import { tryAdvanceStarCount } from "../progression";
import {
	reconcileSimTransport,
	refreshRuntimeEntitiesForTickStride,
} from "../sims";
import type { TimeState } from "../time";
import type { WorldState } from "../world";

export function carrierTick(
	world: WorldState,
	ledger: LedgerState,
	time: TimeState,
): void {
	// Binary `check_and_advance_star_rating` runs at the top of FUN_1098_03ab,
	// before refresh_runtime_entities_for_tick_stride. It compares the
	// per-tick-updated `g_primary_family_ledger_total` against the tier
	// thresholds and may bump `starCount` mid-tick.
	tryAdvanceStarCount(world, time);

	refreshRuntimeEntitiesForTickStride(world, ledger, time);

	for (const carrier of world.carriers) {
		resetCarrierTickBookkeeping(carrier);
		for (const [carIndex, car] of carrier.cars.entries()) {
			advanceCarrierCarState(car, carrier, carIndex, time, world.lobbyMode);
		}
		for (const [, car] of carrier.cars.entries()) {
			dispatchCarrierCarArrivals(world, ledger, time, carrier, car);
		}
		for (const [carIndex, car] of carrier.cars.entries()) {
			processUnitTravelQueue(world, carrier, car, carIndex, time);
		}
	}

	reconcileSimTransport(world, ledger, time);
}
