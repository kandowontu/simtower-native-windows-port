# sim-access/ — Sim-level selector + state-bit helpers

Binary-aligned accessors for `SimRecord` that mirror segment 1228's selector functions (3.6 in ROUTING-BINARY-MAP.md).

## Files

### `selectors.ts`
Binary selector accessors: `getCurrentSimType` (1228:681d), `getCurrentSimVariant` (1228:6854), `getCurrentSimStateWord` (1228:688c), `resolveFamilyParkingSelectorValue` (1228:6700), `resolveFamilyRecyclingCenterLowerSelectorValue` (1228:65c1), `getHousekeepingRoomClaimSelector` (1228:6757), `dispatchEntertainmentGuestSubstate` (1228:662a), `maybeStartHousekeepingRoomClaim` (1228:640c), `computeObjectOccupantRuntimeIndex` (1228:67d7).

### `state-bits.ts`
Bit helpers over `sim.stateCode`. Constants: `SIM_STATE_WAITING_BIT` (0x20), `SIM_STATE_IN_TRANSIT_BIT` (0x40), `SIM_STATE_BASE_MASK` (0x1f), `SIM_STATE_MODE_MASK` (0x60). Readers: `isSimWaiting`, `isSimInTransit`, `simBaseState`. Setters: `setSimWaiting`, `setSimInTransit`, `clearSimRouteBits`, `setSimState`, `setSimBaseState`.

Phase 5b invariant: TS state constants in `sims/states.ts` pre-encode the 0x20 waiting bit for states in the 0x20..0x27 range (MORNING_GATE, AT_WORK, VENUE_TRIP, NIGHT_B, etc.) and the 0x40 in-transit bit for TRANSIT-suffixed states. The authoritative source for routing-mode branching is `isSimInTransit(sim.stateCode)` via `dispatch_sim_behavior` / `maybe_dispatch_queued_route_after_wait`. The `setSim*` helpers are wired at every `sim.route = …` write site in `queue/resolve.ts` (in-transit / waiting) and at bit-strip sites in `queue/process-travel.ts` + `families/force-dispatch.ts` + `families/finalize.ts`.

Note: `clearSimRouteBits` (strip 0x60) is unsafe for arbitrary state bytes because it would convert e.g. MORNING_GATE (0x20) into STATE_COMMUTE (0x00). Prefer `setSimInTransit(sim, false)` (strip 0x40 only) or overwrite `sim.stateCode` with an explicit low-phase constant for idle-park transitions.
