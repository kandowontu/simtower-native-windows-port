// 1288:e62c g_per_stop_even_parity_delay
// 1288:e62e g_per_stop_odd_parity_delay
//
// Parity-based per-stop delay lookup used by `resolveSimRouteBetweenFloors`
// when a sim walks a special-link segment. Indexed by
// `segment.modeAndSpan & 1` (bit 0 = stairs-cost parity).
//
// Values confirmed by emulator capture (simtower/capture_parity_delays.py):
//   even-parity (escalator) = 16
//   odd-parity  (stairs)    = 35

/**
 * `perStopParityDelay[0]` = even-parity (escalator) per-stop stress.
 * `perStopParityDelay[1]` = odd-parity (stairs) per-stop stress.
 */
export const perStopParityDelay: readonly [number, number] = [16, 35];
