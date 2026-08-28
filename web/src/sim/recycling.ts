import type { LedgerState } from "./ledger";
import {
	FAMILY_RECYCLING_CENTER_LOWER,
	FAMILY_RECYCLING_CENTER_UPPER,
} from "./resources";
import type { WorldState } from "./world";

export function resetRecyclingCenterDutyTier(world: WorldState): void {
	for (const object of Object.values(world.placedObjects)) {
		if (
			object.objectTypeCode === FAMILY_RECYCLING_CENTER_LOWER &&
			object.unitStatus === 6
		) {
			object.unitStatus = 0;
		}
	}
}

export function updateRecyclingCenterState(
	world: WorldState,
	ledger: LedgerState,
	param: number,
): void {
	if (world.starCount <= 2) {
		world.gateFlags.recyclingAdequate = 0;
		return;
	}

	if (param === 0) {
		world.gateFlags.recyclingAdequate = 0;
		if (world.gateFlags.recyclingCenterCount === 0) return;
		for (const object of Object.values(world.placedObjects)) {
			if (
				object.objectTypeCode === FAMILY_RECYCLING_CENTER_UPPER ||
				object.objectTypeCode === FAMILY_RECYCLING_CENTER_LOWER
			) {
				if (object.unitStatus === 5) continue;
				object.unitStatus = 0;
			}
		}
		return;
	}

	const scale = Math.max(0, world.gateFlags.recyclingCenterCount);
	if (scale === 0) {
		world.gateFlags.recyclingAdequate = 0;
		return;
	}

	const primaryTotal = ledger.populationLedger.reduce(
		(sum, value) => sum + value,
		0,
	);
	const scaled = Math.trunc(primaryTotal / scale);
	const requiredTier =
		scaled < 500
			? 1
			: scaled < 1000
				? 2
				: scaled < 1500
					? 3
					: scaled < 2000
						? 4
						: scaled < 2500
							? 5
							: 6;
	const adequate = param >= requiredTier ? 1 : 0;
	const dutyTier = adequate ? Math.min(requiredTier, param) : param;

	world.gateFlags.recyclingAdequate = adequate;
	for (const object of Object.values(world.placedObjects)) {
		if (
			object.objectTypeCode === FAMILY_RECYCLING_CENTER_UPPER ||
			object.objectTypeCode === FAMILY_RECYCLING_CENTER_LOWER
		) {
			if (!adequate && object.unitStatus === 5) continue;
			object.unitStatus = dutyTier;
		}
	}
}
