# daily/ — Once-per-day sweeps

Binary sweeps fired from specific day-tick checkpoints.

## Files

### `drain-active-requests.ts`
`dispatchActiveRequestsByFamily` (1190:0977). Fired from the 0x9c4 checkpoint in `tick/day-scheduler.ts`; iterates in-transit sims and re-runs them through `dispatchSimBehavior` to unstick queued routes.
