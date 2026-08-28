// Sim state_code bit helpers — binary layout at SimRecord+5:
//   bits 0..3 : phase (0..7)
//   bit 5 (0x20) : currently waiting
//   bit 6 (0x40) : route queued / in transit
//
// Phase 5a introduced the readers; Phase 5b adds setters and wires them into
// every `sim.route = ...` transition site so `sim.stateCode` bits are the
// authoritative routing-mode flags (matching the binary's
// `dispatch_object_family_*_state_handler` two-tier switch on family_code +
// state_code).

import type { SimRecord } from "../world";

export const SIM_STATE_WAITING_BIT = 0x20;
export const SIM_STATE_IN_TRANSIT_BIT = 0x40;
export const SIM_STATE_BASE_MASK = 0x1f;
export const SIM_STATE_MODE_MASK =
	SIM_STATE_WAITING_BIT | SIM_STATE_IN_TRANSIT_BIT;

export function isSimWaiting(stateCode: number): boolean {
	return (stateCode & SIM_STATE_WAITING_BIT) !== 0;
}

export function isSimInTransit(stateCode: number): boolean {
	return (stateCode & SIM_STATE_IN_TRANSIT_BIT) !== 0;
}

/** Strip the mode bits to expose just the base phase (0..7). */
export function simBaseState(stateCode: number): number {
	return stateCode & SIM_STATE_BASE_MASK;
}

/**
 * Set or clear the waiting bit (0x20) on `sim.stateCode` without disturbing the
 * in-transit bit or base-phase bits.
 */
export function setSimWaiting(sim: SimRecord, on: boolean): void {
	if (on) {
		sim.stateCode = (sim.stateCode | SIM_STATE_WAITING_BIT) & 0xff;
	} else {
		sim.stateCode = sim.stateCode & ~SIM_STATE_WAITING_BIT & 0xff;
	}
}

/**
 * Set or clear the in-transit bit (0x40) on `sim.stateCode` without disturbing
 * the waiting bit or base-phase bits.
 */
export function setSimInTransit(sim: SimRecord, on: boolean): void {
	if (on) {
		sim.stateCode = (sim.stateCode | SIM_STATE_IN_TRANSIT_BIT) & 0xff;
	} else {
		sim.stateCode = sim.stateCode & ~SIM_STATE_IN_TRANSIT_BIT & 0xff;
	}
}

/** Clear both mode bits (0x60). Used when returning a sim to idle. */
export function clearSimRouteBits(sim: SimRecord): void {
	sim.stateCode = sim.stateCode & ~SIM_STATE_MODE_MASK & 0xff;
}

/** Overwrite the entire state_code byte (phase + mode bits). */
export function setSimState(sim: SimRecord, newCode: number): void {
	sim.stateCode = newCode & 0xff;
}

/**
 * Replace just the base-phase bits (0..3) on `sim.stateCode`, preserving
 * the waiting/in-transit mode bits. Phase 5b helper requested by the
 * migration plan; not yet used by the TS state machines which currently
 * overwrite the entire byte, but exposed for future binary-faithful
 * handlers that mutate phase without disturbing mode.
 */
export function setSimBaseState(sim: SimRecord, phase: number): void {
	sim.stateCode =
		((sim.stateCode & SIM_STATE_MODE_MASK) | (phase & SIM_STATE_BASE_MASK)) &
		0xff;
}
