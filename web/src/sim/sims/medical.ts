import { FAMILY_MEDICAL } from "../resources";
import { addDelayToCurrentSim } from "../stress/add-delay";
import { advanceSimTripCounters } from "../stress/trip-counters";
import type { TimeState } from "../time";
import {
	MAX_MEDICAL_SERVICE_SLOTS,
	MEDICAL_RETRY_OVERFLOW,
	type MedicalCenterRecord,
	type SimRecord,
	sampleRng,
	type WorldState,
	yToFloor,
} from "../world";
import {
	clearSimRoute,
	releaseServiceRequest,
	resolveSimRouteBetweenFloors,
} from "./index";
import { simKey } from "./population";
import {
	LOBBY_FLOOR,
	STATE_DEPARTURE,
	STATE_DEPARTURE_TRANSIT,
	STATE_NIGHT_B,
	STATE_PARKED,
} from "./states";

export const MEDICAL_NOTIFICATION_MESSAGE =
	"Medical Center demanded near Lobby";

/** Office worker state codes used during a medical trip. */
export const STATE_MEDICAL_TRIP = 0x30;
export const STATE_MEDICAL_TRIP_TRANSIT = 0x70;
export const STATE_MEDICAL_DWELL = 0x31;

interface MedicalCenterCandidate {
	sidecarIndex: number;
	floor: number;
	record: MedicalCenterRecord;
}

/** Enumerate placed medical centers, bucketed by zone key `(floor - 9) / 15`. */
function collectMedicalCenters(world: WorldState): {
	byZone: Map<number, MedicalCenterCandidate[]>;
	all: MedicalCenterCandidate[];
} {
	const byZone = new Map<number, MedicalCenterCandidate[]>();
	const all: MedicalCenterCandidate[] = [];
	for (const [key, object] of Object.entries(world.placedObjects)) {
		if (object.objectTypeCode !== FAMILY_MEDICAL) continue;
		if (object.linkedRecordIndex < 0) continue;
		const sidecar = world.sidecars[object.linkedRecordIndex];
		if (!sidecar || sidecar.kind !== "medical_center") continue;
		if (sidecar.ownerSubtypeIndex === 0xff) continue;
		const [, y] = key.split(",").map(Number);
		const floor = yToFloor(y);
		const zoneKey = Math.floor((floor - 9) / 15);
		const candidate: MedicalCenterCandidate = {
			sidecarIndex: object.linkedRecordIndex,
			floor,
			record: sidecar,
		};
		const bucket = byZone.get(zoneKey) ?? [];
		bucket.push(candidate);
		byZone.set(zoneKey, bucket);
		all.push(candidate);
	}
	return { byZone, all };
}

function pickMedicalCenter(
	world: WorldState,
	sourceFloor: number,
): MedicalCenterCandidate | null {
	const { byZone, all } = collectMedicalCenters(world);
	if (all.length === 0) return null;
	const zoneKey = Math.floor((sourceFloor - 9) / 15);
	const zoneBucket = byZone.get(zoneKey);
	const bucket = zoneBucket && zoneBucket.length > 0 ? zoneBucket : all;
	return bucket[sampleRng(world) % bucket.length];
}

function allocMedicalSlot(
	world: WorldState,
	sim: SimRecord,
	targetSidecarIndex: number,
	sourceFloor: number,
): number {
	for (let i = 0; i < MAX_MEDICAL_SERVICE_SLOTS; i++) {
		const slot = world.medicalServiceSlots[i];
		if (slot.active) continue;
		slot.active = true;
		slot.simId = simKey(sim);
		slot.sourceFloor = sourceFloor;
		slot.targetSidecarIndex = targetSidecarIndex;
		slot.retryCounter = 0;
		return i;
	}
	return -1;
}

function findMedicalSlotForSim(world: WorldState, sim: SimRecord): number {
	const key = simKey(sim);
	for (let i = 0; i < world.medicalServiceSlots.length; i++) {
		const slot = world.medicalServiceSlots[i];
		if (slot.active && slot.simId === key) return i;
	}
	return -1;
}

function freeMedicalSlot(world: WorldState, slotIndex: number): void {
	const slot = world.medicalServiceSlots[slotIndex];
	if (!slot?.active) return;
	const sidecar = world.sidecars[slot.targetSidecarIndex];
	if (sidecar?.kind === "medical_center") {
		sidecar.pendingVisitorsCount = Math.max(
			0,
			sidecar.pendingVisitorsCount - 1,
		);
	}
	slot.active = false;
	slot.simId = "";
	slot.sourceFloor = -1;
	slot.targetSidecarIndex = -1;
	slot.retryCounter = 0;
}

function fireMedicalFailureBanner(world: WorldState): void {
	world.gateFlags.officeServiceOkMedical = 0;
	world.pendingNotifications.push({
		kind: "route_failure",
		message: MEDICAL_NOTIFICATION_MESSAGE,
	});
}

function routeOfficeWorkerHome(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	const result = resolveSimRouteBetweenFloors(
		world,
		sim,
		sim.selectedFloor,
		LOBBY_FLOOR,
		LOBBY_FLOOR > sim.selectedFloor ? 1 : 0,
		time,
	);
	if (result === -1) {
		sim.stateCode = STATE_NIGHT_B;
		sim.destinationFloor = -1;
		clearSimRoute(sim);
		releaseServiceRequest(world, sim);
		return;
	}
	if (result === 3) {
		sim.destinationFloor = -1;
		sim.selectedFloor = LOBBY_FLOOR;
		sim.stateCode = STATE_PARKED;
		releaseServiceRequest(world, sim);
		return;
	}
	sim.destinationFloor = LOBBY_FLOOR;
	sim.stateCode = STATE_DEPARTURE_TRANSIT;
}

/**
 * Attempt a medical trip for `sim` (office worker at end of workday).
 * Returns true iff the sim has been routed onto a medical trip; the caller
 * should fall through to its normal departure path when false. The 1-in-10
 * RNG sample is only consumed at `starCount >= 3`, matching the binary's
 * gated check — pre-3 towers never enter this branch.
 */
export function tryStartMedicalTrip(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): boolean {
	if (world.starCount < 3) return false;
	const roll = sampleRng(world) % 10;
	if (roll !== 0) return false;

	const target = pickMedicalCenter(world, sim.floorAnchor);
	if (!target) {
		fireMedicalFailureBanner(world);
		return false;
	}

	const slotIndex = allocMedicalSlot(
		world,
		sim,
		target.sidecarIndex,
		sim.floorAnchor,
	);
	if (slotIndex < 0) {
		fireMedicalFailureBanner(world);
		return false;
	}

	target.record.pendingVisitorsCount += 1;

	if (target.floor === sim.floorAnchor) {
		sim.stateCode = STATE_MEDICAL_DWELL;
		sim.selectedFloor = target.floor;
		sim.destinationFloor = -1;
		clearSimRoute(sim);
		return true;
	}

	const result = resolveSimRouteBetweenFloors(
		world,
		sim,
		sim.floorAnchor,
		target.floor,
		target.floor > sim.floorAnchor ? 1 : 0,
		time,
	);
	if (result === -1) {
		freeMedicalSlot(world, slotIndex);
		fireMedicalFailureBanner(world);
		return false;
	}
	sim.selectedFloor = sim.floorAnchor;
	sim.destinationFloor = target.floor;
	if (result === 3) {
		sim.selectedFloor = target.floor;
		sim.destinationFloor = -1;
		sim.stateCode = STATE_MEDICAL_DWELL;
		clearSimRoute(sim);
		return true;
	}
	sim.stateCode = STATE_MEDICAL_TRIP_TRANSIT;
	return true;
}

/**
 * Per-tick state-machine advance for an office worker on a medical trip.
 * Handles dwell timeout, demolished-target detection, and home routing.
 */
export function processMedicalSim(
	world: WorldState,
	time: TimeState,
	sim: SimRecord,
): void {
	if (sim.stateCode === STATE_MEDICAL_TRIP_TRANSIT) return;

	if (sim.stateCode !== STATE_MEDICAL_DWELL) return;

	const slotIndex = findMedicalSlotForSim(world, sim);
	if (slotIndex < 0) {
		sim.stateCode = STATE_DEPARTURE;
		return;
	}
	const slot = world.medicalServiceSlots[slotIndex];

	const sidecar = world.sidecars[slot.targetSidecarIndex];
	const targetGone =
		!sidecar ||
		sidecar.kind !== "medical_center" ||
		sidecar.ownerSubtypeIndex === 0xff;
	if (targetGone) {
		// Binary 1178:02e1 (site 6 of 6 add_or_update_sim_trip_counters callers):
		// office_sim_check_medical_service_slot fires
		// add_delay_to_current_sim(g_venue_unavailable_delay) followed by
		// advance_sim_trip_counters in the target-gone (slot.target == -1) branch.
		addDelayToCurrentSim(sim, 300);
		advanceSimTripCounters(sim);
		freeMedicalSlot(world, slotIndex);
		fireMedicalFailureBanner(world);
		routeOfficeWorkerHome(world, time, sim);
		return;
	}

	slot.retryCounter += 1;
	if (slot.retryCounter >= MEDICAL_RETRY_OVERFLOW) {
		freeMedicalSlot(world, slotIndex);
		routeOfficeWorkerHome(world, time, sim);
	}
}

/** Arrival handler for office workers en route to a medical center. */
export function handleMedicalSimArrival(
	world: WorldState,
	sim: SimRecord,
	arrivalFloor: number,
): void {
	if (sim.stateCode !== STATE_MEDICAL_TRIP_TRANSIT) return;
	const slotIndex = findMedicalSlotForSim(world, sim);
	if (slotIndex < 0) {
		sim.stateCode = STATE_DEPARTURE;
		sim.destinationFloor = -1;
		return;
	}
	const slot = world.medicalServiceSlots[slotIndex];
	const sidecar = world.sidecars[slot.targetSidecarIndex];
	if (
		!sidecar ||
		sidecar.kind !== "medical_center" ||
		sidecar.ownerSubtypeIndex === 0xff
	) {
		freeMedicalSlot(world, slotIndex);
		fireMedicalFailureBanner(world);
		sim.stateCode = STATE_DEPARTURE;
		sim.selectedFloor = arrivalFloor;
		sim.destinationFloor = -1;
		clearSimRoute(sim);
		return;
	}
	sim.selectedFloor = arrivalFloor;
	sim.destinationFloor = -1;
	sim.stateCode = STATE_MEDICAL_DWELL;
	clearSimRoute(sim);
}

/**
 * Invalidate any in-flight medical slots whose target is `removedSidecarIndex`.
 * Called from the placement command layer after freeSidecar marks the target
 * as demolished. Each invalidated slot fires the failure banner and clears the
 * daily flag. Attached sims are put back into STATE_DEPARTURE so they route
 * home on the next refresh stride.
 */
export function invalidateMedicalSlotsForSidecar(
	world: WorldState,
	removedSidecarIndex: number,
): void {
	for (let i = 0; i < world.medicalServiceSlots.length; i++) {
		const slot = world.medicalServiceSlots[i];
		if (!slot.active) continue;
		if (slot.targetSidecarIndex !== removedSidecarIndex) continue;
		const sim = world.sims.find((s) => simKey(s) === slot.simId);
		freeMedicalSlot(world, i);
		fireMedicalFailureBanner(world);
		if (!sim) continue;
		sim.stateCode = STATE_DEPARTURE;
		sim.destinationFloor = -1;
		clearSimRoute(sim);
	}
}
