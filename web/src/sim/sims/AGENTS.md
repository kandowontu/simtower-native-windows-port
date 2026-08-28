# sims/ — Runtime sim sims

Family-specific state machines and shared runtime helpers for the placed-object-derived sim population. No I/O, Cloudflare, or Phaser dependencies.

## Files

### `index.ts`
Runtime sim facade: refresh stride orchestration (`advanceSimRefreshStride` / `refreshRuntimeEntitiesForTickStride`), venue visits, transport routing plumbing, compatibility aliases, and public re-exports for the split sim modules. Family-specific arrival handling lives with each family module. `resolveSimRouteBetweenFloors` moved to `queue/resolve.ts`; this module re-exports it for back-compat. Hosts `findCommercialVenueAtFloor`, `tryAcquireOfficeVenueSlot`, and `releaseOfficeVenueSlot` mirroring binary `acquire_commercial_venue_slot` (11b0:0d92) / `release_commercial_venue_slot` (11b0:0fae) for office workers visiting commercial venues.

### `states.ts`
Shared runtime sim state codes, transit-bit helpers (`0x40` flag + base-code mask), family sets, floor sentinels, route idle value, population tables, and unit-status thresholds.

### `population.ts`
Population construction and cleanup for placed-object-derived sims, sim-key lookup helpers, route clearing, binary-aligned runtime reset including daily stress-counter clears for reset families, and legacy sim-named compatibility aliases. Phase 5b: `clearSimRoute` intentionally does NOT touch `sim.stateCode` bits because the _TRANSIT phase byte is still needed by family arrival handlers to select the right branch. Bit-strip sites pair explicit `setSimInTransit(false)` or byte-overwrites.

### `trip-counters.ts`
Back-compat facade. The binary's per-sim stress accessors (11e0:*) moved to `sim/stress/*.ts` in Phase 8; this file re-exports them and keeps the family-scoped `resetSimTripCounters` / `resetFacilitySimTripCounters` helpers (no 11e0 counterpart).

### `scoring.ts`
Operational scoring, all-object daily eval sweep, nearby-noise checks, distance feedback, occupied flag refreshes, binary-style current-trip/average-trip stress metrics, and wire-facing sim state projection records.

### `scoring.test.ts`
Focused tests for operational eval timing and unscored occupied-room behavior.

### `parking.ts`
Parking coverage propagation from ramps, demand log rebuild, and assignment of parking-service requests to eligible hotel and office sims.

### `facility-refunds.ts`
Commercial venue day-cycle reset/close handling (with per-family closure income accrual), retail activation/deactivation, and unhappy condo/retail facility refunds.

### `hotel-facilities.ts`
Hotel/condo end-of-day unit status normalization, cockroach infestation spread, vacancy expiry, and hotel operational/occupancy refresh.

### `hotel.ts`
Hotel-family sim state machine: check-in routing, active-stay venue visits, checkout queues, sale accounting, room turnover, and hotel-specific arrival handling.

### `housekeeping.ts`
Housekeeping helper (family 0x0f) state machine: vacant-room search, route-to-candidate/target legs, and the post-claim countdown.

### `commercial.ts`
Restaurant/fast-food/retail family state machine: morning activation gates, per-stride state-0x20/0x60/0x05/0x45 handlers, and commercial-specific arrival handling. Mirrors binary `dispatch_object_family_retail_state_handler` (1228:40c0) and `dispatch_object_family_restaurant_fast_food_state_handler` (1228:4851); calls `acquire_commercial_venue_slot` (11b0:0d92) only at venue arrival (rc=3) so `currentPopulation` matches the binary's accounting.

### `office.ts`
Office-family sim state machine: morning activation, worker commute/service-demand handling, presence counters, venue trips, evening departure, office service evaluation entry points, and office-specific arrival handling.

### `condo.ts`
Condo-family sim state machine: occupant activation, restaurant/fast-food venue selection, sale accounting, operational refresh, and condo-specific arrival handling.

### `medical.ts`
Medical-center (family 0x0d) service-request queue and office-worker trip state machine: zone-bucketed target selection, slot allocation, daily flag gating, retry overflow, and demolition invalidation.
