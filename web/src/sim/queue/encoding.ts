// 1218:1b96 decode_runtime_route_target
//
// Decode the byte at `sim+0x08` (`encoded_route_target`) into a (slot,
// direction) pair:
//   byte < 0x40   -> special-link slot, slot = byte
//   0x40..0x57   -> carrier slot, direction = up,   slot = byte - 0x40
//   0x58..0x6f   -> carrier slot, direction = down, slot = byte - 0x58
//
// Phase 3 still stores route state on `sim.route` as a discriminated union;
// Phase 5 flips storage to the byte. This helper lets queue code speak the
// binary encoding today without touching sim.route.

import type { SimRecord } from "../world";

export const ROUTE_TARGET_SPECIAL_LINK_MAX = 0x40;
export const ROUTE_TARGET_CARRIER_UP_BASE = 0x40;
export const ROUTE_TARGET_CARRIER_DOWN_BASE = 0x58;

export interface RuntimeRouteTarget {
	/** "special-link" = stairs/escalator segment; "carrier" = elevator. */
	kind: "special-link" | "carrier";
	/** Segment index or carrier index, depending on kind. */
	slot: number;
	/** 1 = up, 0 = down. Always 0 for special-link targets. */
	directionFlag: number;
}

/**
 * Decode a raw encoded-route-target byte. Returns null for 0xff (unrouted).
 */
export function decodeEncodedRouteTargetByte(
	byte: number,
): RuntimeRouteTarget | null {
	if (byte === 0xff) return null;
	if (byte < ROUTE_TARGET_SPECIAL_LINK_MAX) {
		return { kind: "special-link", slot: byte, directionFlag: 0 };
	}
	if (byte < ROUTE_TARGET_CARRIER_DOWN_BASE) {
		return {
			kind: "carrier",
			slot: byte - ROUTE_TARGET_CARRIER_UP_BASE,
			directionFlag: 1,
		};
	}
	return {
		kind: "carrier",
		slot: byte - ROUTE_TARGET_CARRIER_DOWN_BASE,
		directionFlag: 0,
	};
}

/**
 * Encode a (kind, slot, direction) tuple into the binary's one-byte form.
 */
export function encodeRuntimeRouteTarget(target: RuntimeRouteTarget): number {
	if (target.kind === "special-link") return target.slot & 0xff;
	const base =
		target.directionFlag === 1
			? ROUTE_TARGET_CARRIER_UP_BASE
			: ROUTE_TARGET_CARRIER_DOWN_BASE;
	return (base + target.slot) & 0xff;
}

/**
 * Binary `decode_runtime_route_target` (1218:1b96). Reads sim's route state
 * (currently stored on `sim.route`, not yet on the encoded byte — that's
 * Phase 5) and returns the decoded (slot, direction) view, or null when
 * the sim is idle / queued / not yet assigned a concrete target.
 */
export function decodeRuntimeRouteTarget(
	sim: SimRecord,
): RuntimeRouteTarget | null {
	if (sim.route.mode === "carrier") {
		return {
			kind: "carrier",
			slot: sim.route.carrierId,
			directionFlag: sim.route.direction === "up" ? 1 : 0,
		};
	}
	if (sim.route.mode === "segment") {
		return {
			kind: "special-link",
			slot: sim.route.segmentId,
			directionFlag: 0,
		};
	}
	return null;
}
