import { FAMILY_PARKING, FAMILY_PARKING_RAMP } from "../resources";
import { rebaseSimElapsedFromClock } from "../stress/rebase-elapsed";
import type { TimeState } from "../time";
import {
	type ServiceRequestEntry,
	type SimRecord,
	sampleRng,
	type WorldState,
} from "../world";

/**
 * Rebuild parking-space coverage flags from parking-ramp anchors.
 *
 * Per spec/PARKING.md "Coverage Propagation": for each floor, walk left/right
 * from each ramp anchor across same-floor tiles, crossing empty runs of at
 * most 3 tiles. Stop at any wider gap or any non-empty/non-parking object.
 * Reachable parking-space tiles are marked covered (state 1); all others
 * default to uncovered (state 0).
 */
export function rebuildParkingCoverage(world: WorldState): void {
	const parkingByKey = new Map<
		string,
		{ x: number; y: number; sidecarIndex: number }
	>();
	const rampAnchors: { x: number; y: number; right: number }[] = [];

	for (const [key, obj] of Object.entries(world.placedObjects)) {
		const [x, y] = key.split(",").map(Number);
		if (obj.objectTypeCode === FAMILY_PARKING) {
			parkingByKey.set(key, { x, y, sidecarIndex: obj.linkedRecordIndex });
		} else if (obj.objectTypeCode === FAMILY_PARKING_RAMP) {
			rampAnchors.push({ x, y, right: obj.rightTileIndex });
		}
	}

	for (const space of parkingByKey.values()) {
		const rec = world.sidecars[space.sidecarIndex];
		if (rec?.kind === "service_request") rec.coverageFlag = 0;
	}

	const isParkingTile = (x: number, y: number): boolean => {
		const k = `${x},${y}`;
		const anchorKey = world.cellToAnchor[k] ?? k;
		return parkingByKey.has(anchorKey);
	};
	const isEmpty = (x: number, y: number): boolean => {
		const k = `${x},${y}`;
		return !world.cells[k] && !world.cellToAnchor[k];
	};
	const cover = (x: number, y: number): void => {
		const anchorKey = world.cellToAnchor[`${x},${y}`] ?? `${x},${y}`;
		const space = parkingByKey.get(anchorKey);
		if (!space) return;
		const rec = world.sidecars[space.sidecarIndex];
		if (rec?.kind === "service_request") rec.coverageFlag = 1;
	};

	const MAX_EMPTY_GAP = 3;

	const walk = (
		startX: number,
		y: number,
		direction: -1 | 1,
		bound: number,
	): void => {
		let x = startX;
		while (direction === 1 ? x <= bound : x >= bound) {
			if (isParkingTile(x, y)) {
				cover(x, y);
				x += direction;
				continue;
			}
			if (isEmpty(x, y)) {
				let run = 0;
				while (
					(direction === 1 ? x <= bound : x >= bound) &&
					isEmpty(x, y) &&
					run <= MAX_EMPTY_GAP
				) {
					x += direction;
					run += 1;
				}
				if (run > MAX_EMPTY_GAP) return;
				continue;
			}
			return;
		}
	};

	for (const ramp of rampAnchors) {
		// Cover the ramp footprint's parking neighbors immediately to the sides.
		walk(ramp.x - 1, ramp.y, -1, 0);
		walk(ramp.right + 1, ramp.y, 1, world.width - 1);
	}
}

export function rebuildParkingDemandLog(world: WorldState): void {
	world.parkingDemandLog = [];
	for (let i = 0; i < world.sidecars.length; i++) {
		const rec = world.sidecars[i];
		if (rec.kind !== "service_request") continue;
		if (rec.ownerSubtypeIndex === 0xff) continue;
		if (rec.floorIndex === undefined) continue;
		if (rec.coverageFlag === 1) continue;

		let isParking = false;
		for (const obj of Object.values(world.placedObjects)) {
			if (
				obj.objectTypeCode === FAMILY_PARKING &&
				obj.linkedRecordIndex === i
			) {
				isParking = true;
				break;
			}
		}
		if (isParking) {
			world.parkingDemandLog.push(i);
		}
	}
}

export function tryAssignParkingService(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): boolean {
	if (world.parkingDemandLog.length === 0) return false;
	const idx =
		world.parkingDemandLog[sampleRng(world) % world.parkingDemandLog.length];
	const rec = world.sidecars[idx] as ServiceRequestEntry | undefined;
	if (!rec || rec.kind !== "service_request") return false;
	// Binary parity: process_family_parking_destination_arrival (1048:00f0)
	// is NOT one of the 6 advance_sim_trip_counters call sites. The previous
	// advanceSimTripCounters call here did not correspond to any binary site
	// and has been removed.
	rebaseSimElapsedFromClock(sim, time);
	return true;
}
