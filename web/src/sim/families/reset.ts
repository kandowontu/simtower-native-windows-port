// 1228:0000 reset_sim_runtime_state
//
// Fired from the 0x9c4 checkpoint (day scheduler) to reset per-sim runtime
// state. The current TS equivalent is `resetSimRuntimeState` in
// `sims/population.ts` (re-exported through `sims/index.ts`). Phase 5a
// re-exports under the binary-aligned alias and places it in families/ so
// future callers can import from the binary-aligned location.

export { resetSimRuntimeState } from "../sims/population";
