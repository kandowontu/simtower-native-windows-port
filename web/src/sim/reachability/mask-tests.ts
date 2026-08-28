// 11b8:0f33 testCarrierTransferReachability
// 11b8:0fe6 testSpecialLinkTransferReachability
// 11b8:0e41 chooseTransferFloorFromCarrierReachability
//
// Bit-mask tests against `transferGroupEntries` + `specialLinkRecords`,
// plus the transfer-floor picker used when a carrier doesn't directly
// serve the target floor.

import { carrierSpansFloor } from "../carriers";
import { isExpressStopFloor } from "../carriers/slot";
import { MAX_SPECIAL_LINK_RECORDS, type WorldState } from "../world";

/**
 * 11b8:0f33 testCarrierTransferReachability
 *
 * Walks the transfer-group entries for any entry whose carrier mask
 * includes `carrierId` AND can reach `toFloor` (directly via a peer
 * carrier, or transitively via a peer special-link record).
 */
export function testCarrierTransferReachability(
	world: WorldState,
	carrierId: number,
	toFloor: number,
	preferLocalMode: boolean,
): boolean {
	// Binary 11b8:0f33: walks the 16-entry transfer_group_cache. Each entry's
	// membership mask is tested for the input carrier's bit, that bit is
	// cleared, and the remainder is AND'd against the carrier's per-floor
	// reachability mask for `toFloor`. No pre-loop gate on special_link_records
	// exists in the binary — peer-carrier reachability through a shared
	// lobby/sky-lobby tile is sufficient.
	for (const entry of world.transferGroupEntries) {
		if (!entry.active) continue;
		if ((entry.carrierMask & (1 << carrierId)) === 0) continue;
		if (entryReachesDestinationFloor(world, entry, toFloor, preferLocalMode)) {
			return true;
		}
	}
	return false;
}

/**
 * 11b8:0fe6 testSpecialLinkTransferReachability
 *
 * Probe whether any peer special-link record covers `toFloor`.
 */
export function testSpecialLinkTransferReachability(
	world: WorldState,
	entry: WorldState["transferGroupEntries"][number],
	toFloor: number,
): boolean {
	for (
		let recordIndex = 0;
		recordIndex < MAX_SPECIAL_LINK_RECORDS;
		recordIndex++
	) {
		if ((entry.carrierMask & (1 << (24 + recordIndex))) === 0) continue;
		const record = world.specialLinkRecords[recordIndex];
		if (!record?.active) continue;
		if (derivedRecordReachesFloor(record, toFloor)) return true;
	}
	return false;
}

/**
 * 11b8:0e41 chooseTransferFloorFromCarrierReachability
 *
 * Resolve a transfer floor for a carrier route where the carrier doesn't
 * directly serve the target floor. Scans transfer-group entries to find
 * the first valid transfer floor in the travel direction.
 *
 * Returns the transfer floor, or -1 if no valid transfer found.
 */
export function chooseTransferFloorFromCarrierReachability(
	world: WorldState,
	carrierId: number,
	currentFloor: number,
	targetFloor: number,
): number {
	const carrier = world.carriers.find(
		(candidate) => candidate.carrierId === carrierId,
	);
	if (!carrier) return -1;

	// Binary 11b8:0e41 opens with `carrier.served_floor_flags[target] != 0`.
	// `served_floor_flags` is written by FUN_10a8_1296 as `(carrier_mode != 0
	// || floor <= 10 || isSkyLobbyFloor(floor)) && !insideMultiFloorLobby`, so
	// on express carriers the byte is set ONLY at express-stop floors (binary
	// 1..10 and the sky-lobby cadence) — non-lobby intermediate floors fall
	// through into the transfer-cache loop. The per-floor stop-enable bit
	// cleared by the carrier-dialog toggle (FUN_10a8_0085) also clears this
	// flag, matching the "force transfer routing" branch.
	if (carrierSpansFloor(carrier, targetFloor)) {
		const targetSlot = targetFloor - carrier.bottomServedFloor;
		const stopEnabled = (carrier.stopFloorEnabled[targetSlot] ?? 1) !== 0;
		const expressOk =
			carrier.carrierMode !== 0 ||
			isExpressStopFloor(targetFloor, world.lobbyMode);
		if (stopEnabled && expressOk) return targetFloor;
	}

	// 11b8:0e41 binary loop — scans the 16-slot transfer_group_cache directly.
	// For each entry that includes this carrier, clears the carrier's own bit
	// and requires at least one peer (carrier or derived special-link record)
	// in the remaining mask to reach targetFloor. First entry in cache order
	// whose tagged floor lies in the travel direction wins.
	const directionUp = targetFloor > currentFloor;
	const carrierBit = 1 << carrierId;

	for (const entry of world.transferGroupEntries) {
		if (!entry.active) continue;
		if ((entry.carrierMask & carrierBit) === 0) continue;
		if (entry.taggedFloor === currentFloor) continue;
		if (directionUp && entry.taggedFloor <= currentFloor) continue;
		if (!directionUp && entry.taggedFloor >= currentFloor) continue;

		const peersMask = entry.carrierMask & ~carrierBit;
		if (!peersMaskReachesFloor(world, peersMask, targetFloor)) continue;

		return entry.taggedFloor;
	}

	return -1;
}

function carrierStopsAtFloor(
	carrier: WorldState["carriers"][number],
	floor: number,
	lobbyMode: WorldState["lobbyMode"],
): boolean {
	if (!carrierSpansFloor(carrier, floor)) return false;
	// Binary served_floor_flags[floor] != 0 — the player toggle in the
	// carrier stop-floor dialog (FUN_10a8_0085) clears this bit, removing
	// the floor from the carrier's effective served set for routing.
	const slot = floor - carrier.bottomServedFloor;
	if ((carrier.stopFloorEnabled[slot] ?? 1) === 0) return false;
	if (carrier.carrierMode === 0) return isExpressStopFloor(floor, lobbyMode);
	return true;
}

function peersMaskReachesFloor(
	world: WorldState,
	mask: number,
	targetFloor: number,
): boolean {
	for (let carrierIndex = 0; carrierIndex < 24; carrierIndex++) {
		if ((mask & (1 << carrierIndex)) === 0) continue;
		const peer = world.carriers.find((c) => c.carrierId === carrierIndex);
		if (peer && carrierStopsAtFloor(peer, targetFloor, world.lobbyMode))
			return true;
	}
	for (
		let recordIndex = 0;
		recordIndex < MAX_SPECIAL_LINK_RECORDS;
		recordIndex++
	) {
		if ((mask & (1 << (24 + recordIndex))) === 0) continue;
		const record = world.specialLinkRecords[recordIndex];
		if (record?.active && derivedRecordReachesFloor(record, targetFloor)) {
			return true;
		}
	}
	return false;
}

// Shared helper used by both `testCarrierTransferReachability` and the
// derived-record scanners in `route-scoring/select-candidate.ts`.
export function derivedRecordReachesFloor(
	record: WorldState["specialLinkRecords"][number],
	targetFloor: number,
): boolean {
	if (targetFloor >= record.lowerFloor && targetFloor <= record.upperFloor)
		return true;
	return (record.reachabilityMasksByFloor[targetFloor] ?? 0) !== 0;
}

function entryReachesDestinationFloor(
	world: WorldState,
	entry: WorldState["transferGroupEntries"][number],
	toFloor: number,
	preferLocalMode: boolean,
): boolean {
	for (let carrierIndex = 0; carrierIndex < 24; carrierIndex++) {
		if ((entry.carrierMask & (1 << carrierIndex)) === 0) continue;
		const carrier = world.carriers.find(
			(candidate) => candidate.carrierId === carrierIndex,
		);
		if (!carrier) continue;
		if (
			preferLocalMode ? carrier.carrierMode === 2 : carrier.carrierMode !== 2
		) {
			continue;
		}
		if (carrierStopsAtFloor(carrier, toFloor, world.lobbyMode)) return true;
	}
	return testSpecialLinkTransferReachability(world, entry, toFloor);
}
