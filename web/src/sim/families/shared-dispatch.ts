// 1228:650e dispatch_object_family_hotel_restaurant_office_condo_retail_fast_food_state_handler
//
// Shared destination-floor resolver called from assign_request_to_runtime_route
// (1218:0d4e) for families 3/4/5/6/7/9/10/0c. Indexed through an 8-entry
// jump table at CS:65a1 keyed on sim.state_code:
//
//   0x40 → 0x0a (lobby floor 10)              — outbound to lobby/work
//   0x41 → commercial venue owner floor       — venue trip outbound
//   0x42 → medical service source floor       — medical trip outbound
//   0x45 → service-assigned floor             — end-of-day / service return
//   0x60 → sim[+0] (floorAnchor)             — return home (MORNING_TRANSIT)
//   0x61 → sim[+0] (floorAnchor)             — return home (AT_WORK_TRANSIT)
//   0x62 → sim[+0] (floorAnchor)             — return home (VENUE_HOME_TRANSIT)
//   0x63 → sim[+0] (floorAnchor)             — return home (DWELL_RETURN_TRANSIT)
//   else → -1 (no match; caller falls through)
//
// Binary implementation: reads sim[+5] (state_code) via g_sim_table, walks
// the 8-entry key table at CS:65a1, then dispatches through the jump-address
// table at CS:65b1:
//   0x40: MOV [BP-4], 0xa                          — hardcoded lobby
//   0x41: CALLF 11b0:10fe get_current_commercial_venue_destination_floor
//   0x42: CALLF 1178:0522 lookup_medical_service_source_floor
//   0x45: CALLF 11a0:0650 get_sim_assigned_floor
//   0x60-0x63: CALL 1228:681d get_current_sim_type  — returns sim[+0]
//
// TS field-mapping notes:
//   - Binary 0x41 uses sim[+6] as a venue-record slot index; in TS
//     destinationFloor already holds the resolved venue floor set at dispatch.
//   - Binary 0x42 uses sim[+6] as the medical slot index into
//     g_medical_service_request_table; in TS the slot is located by simKey.
//   - Binary 0x45 calls get_sim_assigned_floor which reads sim[+0xc] >> 10;
//     in TS destinationFloor holds the assigned floor.

import { simKey } from "../sims/population";
import { LOBBY_FLOOR } from "../sims/states";
import type { SimRecord, WorldState } from "../world";

// State codes from the binary jump table at CS:65a1.
const STATE_COMMUTE_TRANSIT = 0x40; // outbound to lobby
const STATE_ACTIVE_TRANSIT = 0x41; // outbound to commercial venue
const STATE_MEDICAL_TRANSIT = 0x42; // outbound for medical
const STATE_DEPARTURE_TRANSIT = 0x45; // end-of-day return / service
const STATE_HOME_TRANSIT_BASE = 0x60; // 0x60–0x63: inbound to home floor

/**
 * 1228:650e — shared destination-floor resolver for families 3/4/5/6/7/9/10/0c.
 *
 * Returns the logical destination floor for the sim's current in-transit state.
 * Called by assign_request_to_runtime_route to feed choose_transfer_floor_from_carrier_reachability.
 */
export function dispatchObjectFamilyHotelRestaurantOfficeCondoRetailFastFoodStateHandler(
	world: WorldState,
	sim: SimRecord,
): number {
	const stateCode = sim.stateCode;

	// 0x40: hardcoded lobby — binary writes [BP-4] = 0x0a directly.
	if (stateCode === STATE_COMMUTE_TRANSIT) {
		return LOBBY_FLOOR;
	}

	// 0x41: get_current_commercial_venue_destination_floor (11b0:10fe) — binary
	// reads sim[+6] as signed byte (venue-record index); negative → lobby (10).
	// In TS, destinationFloor holds the target venue floor set at dispatch time.
	if (stateCode === STATE_ACTIVE_TRANSIT) {
		return sim.destinationFloor >= 0 ? sim.destinationFloor : LOBBY_FLOOR;
	}

	// 0x42: lookup_medical_service_source_floor (1178:0522) — binary reads sim[+6]
	// as the medical slot index; returns slot.sourceFloor or lobby (10) if inactive.
	// In TS, locate the active medical slot by simKey and return its sourceFloor.
	if (stateCode === STATE_MEDICAL_TRANSIT) {
		const id = simKey(sim);
		const slot = world.medicalServiceSlots.find(
			(s) => s.active && s.simId === id,
		);
		return slot ? slot.sourceFloor : LOBBY_FLOOR;
	}

	// 0x45: get_sim_assigned_floor (11a0:0650) — binary calls sim_has_service_assignment;
	// on success reads 10 - (sim[+0xc] >> 10). In TS destinationFloor holds the
	// assigned floor written at dispatch time.
	if (stateCode === STATE_DEPARTURE_TRANSIT) {
		return sim.destinationFloor >= 0 ? sim.destinationFloor : LOBBY_FLOOR;
	}

	// 0x60–0x63: get_current_sim_type (1228:681d) — returns sim[+0] = floorAnchor.
	if (
		stateCode >= STATE_HOME_TRANSIT_BASE &&
		stateCode <= STATE_HOME_TRANSIT_BASE + 3
	) {
		return sim.floorAnchor;
	}

	// No match: binary falls through without writing the output variable,
	// leaving the caller's local at its initialised value (-1 / 0xffff).
	return -1;
}
