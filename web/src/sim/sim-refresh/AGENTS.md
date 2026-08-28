# sim-refresh/ — Per-tick sim stride refresh

Hosts `refreshRuntimeEntitiesForTickStride` (binary 1228:0d64). Phase 5a re-exports the existing implementation in `sims/index.ts`; Phase 5b moves the implementation here.

## Files

### `refresh-stride.ts`
Exports `refreshRuntimeEntitiesForTickStride` and the legacy alias `advanceSimRefreshStride` from `sims`.
