// Housekeeping (family 0x0f) route selector wrapper.
//
// In the binary, housekeeping calls `resolve_sim_route_between_floors`
// (1218:0000) with `is_passenger_route = 0` (service-mode routing). See
// `update_object_family_housekeeping_connection_state` (1228:602b) call sites
// at 1228:620f and 1228:6320, both of which pass 0 for the first argument.
//
// `resolve_sim_route_between_floors` then forwards to `select_best_route_candidate`
// (11b8:1484) with `prefer_local_mode = 0`. There is NO per-family customization
// inside the route scorer itself — the only family-dependent input is the
// `prefer_local_mode` boolean, which housekeeping passes as false.
//
// (The "custom selectors" dispatched from `assign_request_to_runtime_route`
// (1218:0d4e, jump table at 1218:0f4b) are *target-floor* selectors used by
// the carrier-queue boarding path, not route selectors. They feed the chosen
// floor into `choose_transfer_floor_from_carrier_reachability` (11b8:0e41),
// not into `select_best_route_candidate`.)

import type { WorldState } from "../world";
import {
	type RouteCandidate,
	selectBestRouteCandidate,
} from "./select-candidate";

export function selectHousekeepingRoute(
	world: WorldState,
	fromFloor: number,
	toFloor: number,
	targetHeightMetric: number,
): RouteCandidate | null {
	return selectBestRouteCandidate(
		world,
		fromFloor,
		toFloor,
		/* preferLocalMode = */ false,
		targetHeightMetric,
	);
}
