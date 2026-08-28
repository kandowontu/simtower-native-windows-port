// 11e0:0423 reduce_elapsed_for_lobby_boarding
//
// Discounts accumulated elapsed when a sim boards a non-service carrier
// from the lobby floor, keyed on g_lobby_height:
//   source_floor != LOBBY: no-op
//   lobby_height <= 1: no-op
//   lobby_height == 2: −25 ticks (min 0)
//   lobby_height >= 3: −50 ticks (min 0)
// Service-carrier exclusion is enforced by the CALLER
// (`applyBoardingStressUpdate`), matching the binary: service carriers
// skip `accumulate_elapsed_delay_into_current_sim` entirely, so this
// function never runs for them.

import { LOBBY_FLOOR } from "../sims/states";
import type { WorldState } from "../world";

export function reduceElapsedForLobbyBoarding(
	sim: { elapsedTicks: number },
	sourceFloor: number,
	world: WorldState,
): void {
	if (sourceFloor !== LOBBY_FLOOR) return;
	const lobbyHeight = Math.max(1, world.lobbyHeight ?? 1);
	const discount = lobbyHeight >= 3 ? 50 : lobbyHeight === 2 ? 25 : 0;
	if (discount === 0) return;
	sim.elapsedTicks = Math.max(0, sim.elapsedTicks - discount);
}
