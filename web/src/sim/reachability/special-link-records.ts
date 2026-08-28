// 11b8:06a4 rebuildSpecialLinkRouteRecords
// 11b8:0763 scanSpecialLinkSpanBound
//
// Rebuilds the eight derived `specialLinkRecord` transfer zones from the
// set of stairs/escalator overlays, and scans a center floor outward to
// find the top or bottom of a walkable span.

import {
	GRID_HEIGHT,
	type LobbyMode,
	MAX_SPECIAL_LINK_RECORDS,
	MAX_SPECIAL_LINKS,
	type WorldState,
	yToFloor,
} from "../world";

// Lobby/sky-lobby centers track the express-stop cadence (see
// `isExpressStopFloor`): perfect-parity mode places them at 10, 24, 39, 54, ...
// (step 15 starting at logical offset 14); modern mode at 10, 25, 40, 55, ...
function derivedRecordCenters(lobbyMode: LobbyMode): number[] {
	const cycleOffset = lobbyMode === "modern" ? 15 : 14;
	return [
		10,
		10 + cycleOffset,
		25 + cycleOffset,
		40 + cycleOffset,
		55 + cycleOffset,
		70 + cycleOffset,
		85 + cycleOffset,
	];
}

// Per-tile placement matches binary 1200:149c. flags = ((extent_minus_one) << 1)
// | stairsBit; for 2-floor tiles extent_minus_one = 0. entryFloor is the lower
// landing (overlay coordinate is the upper landing in this codebase).
export function rebuildSpecialLinkRouteRecords(world: WorldState): void {
	world.specialLinks = Array.from({ length: MAX_SPECIAL_LINKS }, () => ({
		active: false,
		flags: 0,
		heightMetric: 0,
		entryFloor: 0,
		reservedByte: 0,
		descendingLoadCounter: 0,
		ascendingLoadCounter: 0,
	}));
	world.specialLinkRecords = Array.from(
		{ length: MAX_SPECIAL_LINK_RECORDS },
		() => ({
			active: false,
			lowerFloor: 0,
			upperFloor: 0,
			reachabilityMasksByFloor: new Array(GRID_HEIGHT).fill(0),
		}),
	);

	const tileEntries: Array<{
		column: number;
		entryFloor: number;
		type: "stairs" | "escalator";
	}> = [];

	for (const [key, type] of Object.entries(world.overlays)) {
		if (type !== "stairs" && type !== "escalator") continue;
		const [xStr, yStr] = key.split(",");
		const column = Number(xStr);
		const upperFloor = yToFloor(Number(yStr));
		// Overlay coordinate is the upper landing; the binary stores entry_floor
		// as the bottom landing — for a tile at floor F that's F - 1.
		tileEntries.push({ column, entryFloor: upperFloor - 1, type });
	}

	tileEntries.sort((a, b) =>
		a.column === b.column ? a.entryFloor - b.entryFloor : a.column - b.column,
	);

	let segmentIndex = 0;
	for (const tile of tileEntries) {
		if (segmentIndex >= MAX_SPECIAL_LINKS) break;
		const stairsBit = tile.type === "stairs" ? 1 : 0;
		// extent_minus_one = 0 for a 2-floor tile, so flags == stairsBit.
		world.specialLinks[segmentIndex++] = {
			active: true,
			flags: stairsBit,
			heightMetric: tile.column,
			entryFloor: tile.entryFloor,
			reservedByte: 0,
			descendingLoadCounter: 0,
			ascendingLoadCounter: 0,
		};
	}

	// Refresh floorWalkabilityFlags from the just-rebuilt specialLinks so the
	// span scan below sees up-to-date walkability. Without this, the scan reads
	// flags left over from a prior segment set: the server's incremental rebuild
	// path produces records derived from N-1 flags, while a fresh hydrate
	// derives them from the snapshot's current flags — same overlays, different
	// records, breaking the lockstep round-trip.
	world.floorWalkabilityFlags = new Array(GRID_HEIGHT).fill(0);
	for (const segment of world.specialLinks) {
		if (!segment.active) continue;
		const bit = (segment.flags & 1) !== 0 ? 2 : 1;
		const extentMinusOne = segment.flags >> 1;
		// Binary encoding: top_floor = entry_floor + (flags >> 1) + 1.
		const top = segment.entryFloor + extentMinusOne + 1;
		for (let floor = segment.entryFloor; floor <= top; floor++) {
			if (floor >= 0 && floor < GRID_HEIGHT) {
				world.floorWalkabilityFlags[floor] |= bit;
			}
		}
	}

	for (const [recordIndex, center] of derivedRecordCenters(
		world.lobbyMode,
	).entries()) {
		if (recordIndex >= MAX_SPECIAL_LINK_RECORDS) break;
		const lowerFloor = scanSpecialLinkSpanBound(world, center, 0);
		const upperFloor = scanSpecialLinkSpanBound(world, center, 1);
		if (lowerFloor >= upperFloor) continue;
		world.specialLinkRecords[recordIndex] = {
			active: true,
			lowerFloor,
			upperFloor,
			reachabilityMasksByFloor: new Array(GRID_HEIGHT).fill(0),
		};
	}
}

export function scanSpecialLinkSpanBound(
	world: WorldState,
	centerFloor: number,
	dir: 0 | 1,
): number {
	let seenGap = false;

	if (dir !== 0) {
		for (let floor = centerFloor; floor < centerFloor + 6; floor++) {
			const flags = world.floorWalkabilityFlags[floor] ?? 0;
			if (flags === 0) return floor;
			if ((flags & 1) === 0) seenGap = true;
			if (seenGap && floor >= centerFloor + 3) return floor;
		}
		return centerFloor + 6;
	}

	// Binary 11b8:0763 starts at centerFloor-1 (not centerFloor) and returns
	// floor+1 (not floor) on the zero-flag exit, so the lower bound stops one
	// floor above the first non-walkable floor below the center.
	for (let floor = centerFloor - 1; floor > centerFloor - 6; floor--) {
		const flags = world.floorWalkabilityFlags[floor] ?? 0;
		if (flags === 0) return floor + 1;
		if ((flags & 1) === 0) seenGap = true;
		if (seenGap && floor <= centerFloor - 3) return floor;
	}
	return centerFloor - 5;
}
