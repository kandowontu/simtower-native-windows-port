# carriers/ — Per-car state machine (binary segment 1098)

One binary function per file; each file header carries its `SEG:OFFSET name`.

## Files

### `advance.ts`
`advanceCarrierCarState` (1098:06fb). Per-tick car-state branch on settle/dwell counters.

### `position.ts`
`advanceCarPositionOneStep` (1098:10e4). One-step move; arms settle per motion mode.

### `target.ts`
`recomputeCarTargetAndDirection` (1098:0bcf), `selectNextTargetFloor` (1098:1553), `updateCarDirectionFlag` (1098:1d2f), `findNearestWorkFloor` (1098:1f4c).

### `motion.ts`
`computeCarMotionMode` (1098:209f). Picks mode 0/1/2/3 by distance; preserves express-only mode-3 quirk.

### `depart.ts`
`shouldCarDepart` (1098:23a5). Departure gate at dwell expiry.

### `assign.ts`
`assignCarToFloorRequest` (1098:0a4c), `findBestAvailableCarForFloor` (1098:0dfc). Preserves car-index-0 degenerate fallback and idle-home equality tiebreak.

### `arrival.ts`
`clearFloorRequestsOnArrival` (1098:13cc), `cancelStaleFloorAssignment` (1098:12c9 — clears this car's primary/secondary slot when leaving a floor), `resetOutOfRangeCar` (1098:0192).

### `pending.ts`
`decrementCarPendingAssignmentCount` (1098:0b10). TODO stub — TS currently folds this into the inline decrement inside `clearFloorRequestsOnArrival`.

### `slot.ts`
`floorToSlot` (10a8:17ee) + `carrierServesFloor` helper. Used internally to avoid a cycle with the `carriers.ts` hub.

### `sync.ts`
Derived-state sync helpers (`syncAssignmentStatus`, `syncRouteSlots`, `syncPendingRouteIds`, `addRouteSlot`, `hasActiveSlot`, `normalizeInactiveSlots`). Lives here so `queue/*.ts` can call them without a cycle through `carriers.ts`.

### `index.ts`
Re-export barrel so `carriers.ts` and other modules can import the state-machine surface.
