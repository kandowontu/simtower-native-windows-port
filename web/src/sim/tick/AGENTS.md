# tick/ — Per-tick orchestration

Mirrors the binary's top-level tick call graph (segments 1268, 1208, 1098).

## Files

### `service-idle-tasks.ts`
`serviceIdleTasks` (1268:01a6). Win16 idle pass: day scheduler then carrier tick. Entry point for `TowerSim.step()`. Phase 7: dropped `onArrival`/`onBoarding` callback plumbing — family dispatch and stress accumulation happen inline in the queue path.

### `day-scheduler.ts`
`runSimulationDayScheduler` (1208:0196). Advances `g_day_tick`, fires random-news/VIP/bomb/fire events, and runs checkpoint handlers including the tick-2533 all-object eval sweep. Hosts `runCheckpoints` + `SimState`; the 0x9c4 checkpoint additionally invokes the daily-sweep `dispatchActiveRequestsByFamily` (1190:0977) from `daily/drain-active-requests.ts`.

### `carrier-tick.ts`
`carrierTick` (1098:03ab). Runs `refreshRuntimeEntitiesForTickStride` → per-carrier `advanceCarrierCarState`/`dispatchCarrierCarArrivals`/`processUnitTravelQueue` → `reconcileSimTransport`.
