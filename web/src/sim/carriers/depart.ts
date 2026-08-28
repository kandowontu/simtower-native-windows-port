// 1098:23a5 should_car_depart
//
// Gate invoked at dwell-expiry inside `advance_carrier_car_state`. Forces
// departure when full, when schedule multiplier is 0, or when stopped at a
// non-home, non-lobby/express floor. Otherwise waits until the arrival-to-now
// delta exceeds multiplier * 30 ticks.
import type { TimeState } from "../time";
import type { CarrierCar, CarrierRecord, LobbyMode } from "../world";

function getCarCapacity(carrier: CarrierRecord): number {
	return carrier.assignmentCapacity;
}

function getScheduleIndex(time: TimeState): number {
	return time.weekendFlag * 7 + time.daypartIndex;
}

export function shouldCarDepart(
	carrier: CarrierRecord,
	car: CarrierCar,
	time: TimeState,
	lobbyMode: LobbyMode,
): boolean {
	// Binary 1098:23a5.
	if (car.assignedCount >= getCarCapacity(carrier)) return true;
	const multiplier = carrier.dwellDelay[getScheduleIndex(time)] ?? 1;
	if (multiplier === 0) return true;
	if (car.currentFloor !== car.homeFloor) {
		// perfect-parity: dwell at floor 10 (ground) and 25, 40, 55, ...
		// (binary's `(currentFloor-10)%15 == 0` express-stop dwell rule).
		// modern: shift the cadence by +1 to follow lobbies that moved up
		// one floor (15, 30, 45, ... above ground).
		const cycleOffset = lobbyMode === "modern" ? 1 : 0;
		const isLobbyOrExpress =
			car.currentFloor === 10 ||
			(car.currentFloor > 10 && (car.currentFloor - 10) % 15 === cycleOffset);
		if (!isLobbyOrExpress) return true;
	}
	return Math.abs(time.dayTick - car.arrivalTick) > multiplier * 30;
}
