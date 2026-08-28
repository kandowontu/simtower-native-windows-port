// 1098:10e4 advance_car_position_one_step
//
// Steps the car one position toward its target. When already at target on
// entry, recomputes target+direction first (with prevFloor = cur latched
// before the recompute). Then computes motion mode, possibly arms the settle
// counter, steps by ±1 or ±3 (the ±3 is express-only — see motion.ts quirk),
// and clears arrivalSeen.
import type { CarrierCar, CarrierRecord } from "../world";
import { computeCarMotionMode } from "./motion";
import { recomputeCarTargetAndDirection } from "./target";

const DEPARTURE_SEQUENCE_TICKS = 5;

export function advanceCarPositionOneStep(
	carrier: CarrierRecord,
	car: CarrierCar,
	carIndex: number,
): void {
	if (car.currentFloor === car.targetFloor) {
		car.prevFloor = car.currentFloor;
		recomputeCarTargetAndDirection(carrier, car, carIndex);
	}

	const motionMode = computeCarMotionMode(carrier, car);
	if (motionMode === 0) car.settleCounter = DEPARTURE_SEQUENCE_TICKS;
	else if (motionMode === 1) car.settleCounter = 2;

	// Binary convention: direction==0 → down (cur -= step); direction!=0 → up.
	// Binary quirk: mode 3 (±3) is express-only — standard/service never hit
	// the 3-floor step path. Preserved in computeCarMotionMode.
	const stepSize = motionMode === 3 ? 3 : 1;
	const stepDir = car.directionFlag === 0 ? -1 : 1;
	car.currentFloor += stepDir * stepSize;

	if (car.arrivalSeen !== 0) car.arrivalSeen = 0;
}
