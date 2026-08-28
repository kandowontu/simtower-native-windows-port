// 1098:209f compute_car_motion_mode
//
// Returns 0=stop (seeds settle=5), 1=slow (seeds settle=2), 2=normal, 3=fast
// (±3 floors, express only). Binary quirk: mode 3 (±3) is gated to express
// carriers (mode==0) — standard/service carriers cannot reach it even when
// distances qualify. Do NOT "fix" this.
import type { CarrierCar, CarrierRecord } from "../world";

export function computeCarMotionMode(
	carrier: CarrierRecord,
	car: CarrierCar,
): 0 | 1 | 2 | 3 {
	const distToTarget = Math.abs(car.currentFloor - car.targetFloor);
	const distFromPrev = Math.abs(car.currentFloor - car.prevFloor);

	// Binary 1098:209f. No firstLeg override — when distFromPrev < 2
	// (including 0 on departure), binary returns mode 0 (stop, stab=5).
	if (carrier.carrierMode === 0) {
		// Express: stop within 2, fast (±3) when both > 4, normal otherwise.
		// Binary quirk: express-only ±3 mode is not available to standard/service.
		if (distToTarget < 2 || distFromPrev < 2) return 0;
		if (distToTarget > 4 && distFromPrev > 4) return 3;
		return 2;
	}

	// Standard (1) and Service (2): stop within 2, slow-stop within 4.
	// Binary quirk: express-only mode 3 (±3 floors) stays disabled here.
	if (distToTarget < 2 || distFromPrev < 2) return 0;
	if (distToTarget < 4 || distFromPrev < 4) return 1;
	return 2;
}
