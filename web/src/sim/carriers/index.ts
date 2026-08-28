// Barrel for the carriers/ submodule. Each export maps 1:1 to a binary
// function (see file headers for SEG:OFFSET). Listed in call-graph order:
// target/selection first, then motion/departure, then per-floor assignment
// and arrival bookkeeping, then the top-level per-tick state machine.

export { advanceCarrierCarState } from "./advance";
export {
	cancelStaleFloorAssignment,
	clearFloorRequestsOnArrival,
	resetOutOfRangeCar,
} from "./arrival";
export {
	assignCarToFloorRequest,
	findBestAvailableCarForFloor,
} from "./assign";
export { shouldCarDepart } from "./depart";
export { computeCarMotionMode } from "./motion";
export { decrementCarPendingAssignmentCount } from "./pending";
export { advanceCarPositionOneStep } from "./position";
export { carrierServesFloor, carrierSpansFloor, floorToSlot } from "./slot";
export {
	findNearestWorkFloor,
	recomputeCarTargetAndDirection,
	selectNextTargetFloor,
	updateCarDirectionFlag,
} from "./target";
