// Barrel for the queue/ submodule. Each export maps 1:1 to a binary
// function at segment 1218 (see file headers for SEG:OFFSET).

export {
	cancelRuntimeRouteRequest,
	decrementRouteQueueDirectionLoad,
	dispatchQueuedRouteUntilRequest,
} from "./cancel";
export { popUnitQueueRequest } from "./dequeue";
export {
	dispatchCarrierCarArrivals,
	dispatchDestinationQueueEntries,
} from "./dispatch-arrivals";
export {
	decodeEncodedRouteTargetByte,
	decodeRuntimeRouteTarget,
	encodeRuntimeRouteTarget,
	ROUTE_TARGET_CARRIER_DOWN_BASE,
	ROUTE_TARGET_CARRIER_UP_BASE,
	ROUTE_TARGET_SPECIAL_LINK_MAX,
	type RuntimeRouteTarget,
} from "./encoding";
export { enqueueRequestIntoRouteQueue } from "./enqueue";
export {
	assignRequestToRuntimeRoute,
	processUnitTravelQueue,
} from "./process-travel";
export {
	type ResolveSimRouteOptions,
	type RouteResolution,
	resolveSimRouteBetweenFloors,
} from "./resolve";
export { ROUTE_QUEUE_CAPACITY_CONST, RouteRequestRing } from "./route-record";
export {
	popActiveRouteSlotRequest,
	removeRequestFromActiveRouteSlots,
	removeRequestFromUnitQueue,
	storeRequestInActiveRouteSlot,
} from "./scan";
