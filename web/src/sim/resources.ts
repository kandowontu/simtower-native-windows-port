import { ORIGINAL_TUNING } from "./original-tuning";

export const STARTING_CASH = 2_000_000;

// ─── Tile registry ────────────────────────────────────────────────────────────

/** Width in grid cells for each placeable tile type. */
export const TILE_WIDTHS: Record<string, number> = {
	// Infrastructure
	floor: 1,
	lobby: 1,
	stairs: 8,
	elevator: 4,
	elevatorExpress: 6,
	elevatorService: 4,
	escalator: 8,
	// Hotels (families 3/4/5)
	hotelSingle: 4,
	hotelTwin: 6,
	hotelSuite: 10,
	// Commercial (families 6/0x0a/0x0c)
	restaurant: 24,
	fastFood: 16,
	retail: 12,
	// Office (family 7)
	office: 9,
	// Condo (family 9)
	condo: 16,
	// Entertainment (families 0x12/0x1d)
	cinema: 31,
	partyHall: 24,
	cathedral: 28,
	// Services
	recyclingCenter: 25, // virtual two-floor stack placement
	recyclingCenterUpper: 25, // family 0x14 upper slice
	recyclingCenterLower: 25, // family 0x15 lower slice
	parking: 4, // family 0x18
	parkingRamp: 16, // family 0x2c; confirmed by original .TDT footprints
	security: 16, // family 0x0e
	metro: 30, // families 0x1f/0x20/0x21, three-floor stack
	housekeeping: 15, // family 0x0f
	medical: 26, // family 0x0d
};

/** One-time construction cost in dollars. */
export const TILE_COSTS: Record<string, number> = {
	...ORIGINAL_TUNING.constructionCosts,
	recyclingCenterUpper: ORIGINAL_TUNING.constructionCosts.recyclingCenter,
	recyclingCenterLower: 0,
};

/** One-time cost to add an extra car to a carrier, indexed by carrier mode. */
export const CARRIER_CAR_CONSTRUCTION_COST: Record<number, number> = {
	0: 150_000,
	1: 80_000,
	2: 50_000,
};

/**
 * Per-floor cost charged when extending an existing carrier shaft up or
 * down by one floor. Binary path: `FUN_10a8_0819` (extend_up) /
 * `FUN_10a8_0b87` (extend_down) → `charge_floor_range_construction_cost`
 * (1180:02e5), which sums `width * tile_rate` per newly-served floor.
 * Observed delta in the mixed_elevator_delayed trace: $15,000 for a
 * 3-floor extension of a standard (width=4) shaft → $5,000/floor.
 */
export const CARRIER_EXTEND_FLOOR_COST: Record<number, number> = {
	0: 7_500, // express (width 6)
	1: 5_000, // standard (width 4)
	2: 5_000, // service (width 4)
};

/**
 * Minimum star rating required before the binary exposes a build-menu entry.
 * Tiles omitted from this table are treated as always available.
 */
export const TILE_STAR_REQUIREMENTS: Record<string, number> = {
	lobby: 1,
	floor: 1,
	stairs: 1,
	elevator: 1,
	office: 1,
	fastFood: 1,
	condo: 1,
	elevatorService: 2,
	hotelSingle: 2,
	hotelTwin: 2,
	hotelSuite: 2,
	housekeeping: 2,
	security: 2,
	escalator: 3,
	elevatorExpress: 3,
	restaurant: 3,
	retail: 3,
	partyHall: 3,
	cinema: 3,
	parking: 3,
	parkingRamp: 3,
	recyclingCenter: 3,
	recyclingCenterUpper: 3,
	recyclingCenterLower: 3,
	medical: 3,
	metro: 4,
	cathedral: 5,
};

export function getTileStarRequirement(tileType: string): number {
	return TILE_STAR_REQUIREMENTS[tileType] ?? 1;
}

export const VALID_TILE_TYPES = new Set(Object.keys(TILE_WIDTHS));

/**
 * Tiles that may be placed at or below the underground row. Everything else is
 * rejected by `handlePlaceTile` when `y >= UNDERGROUND_Y`. Mirrors the original
 * SimTower's build-menu restrictions: support/transport, parking/recycling,
 * selected public facilities, and entertainment venues may go underground.
 */
export const UNDERGROUND_ALLOWED_TILES = new Set([
	"floor",
	"stairs",
	"escalator",
	"elevator",
	"elevatorExpress",
	"elevatorService",
	"parking",
	"parkingRamp",
	"metro",
	"recyclingCenter",
	"recyclingCenterUpper",
	"recyclingCenterLower",
	"restaurant",
	"fastFood",
	"retail",
	"security",
	"partyHall",
	"cinema",
	"medical",
]);

// ─── Family codes (object-type codes from the spec) ─────────────────────────

export const FAMILY_ELEVATOR = 1;
export const FAMILY_ESCALATOR = 2;
export const FAMILY_HOTEL_SINGLE = 3;
export const FAMILY_HOTEL_TWIN = 4;
export const FAMILY_HOTEL_SUITE = 5;
export const FAMILY_RESTAURANT = 6;
export const FAMILY_OFFICE = 7;
export const FAMILY_CONDO = 9;
export const FAMILY_RETAIL = 10;
export const FAMILY_FAST_FOOD = 12;
export const FAMILY_SECURITY = 14;
export const FAMILY_CINEMA = 18;
export const FAMILY_CINEMA_LOWER = 19;
export const FAMILY_RECYCLING_CENTER_UPPER = 20;
export const FAMILY_RECYCLING_CENTER_LOWER = 21;
export const FAMILY_PARKING = 24;
export const FAMILY_PARKING_RAMP = 44; // 0x2c
export const FAMILY_METRO_TOP = 0x1f;
export const FAMILY_METRO_MIDDLE = 0x20;
export const FAMILY_METRO_BOTTOM = 0x21;
export const FAMILY_PARTY_HALL = 29;
export const FAMILY_PARTY_HALL_LOWER = 30;
export const FAMILY_CINEMA_STAIRS_UPPER = 34;
export const FAMILY_CINEMA_STAIRS_LOWER = 35;
export const FAMILY_CATHEDRAL_ANCHOR = 0x21;
export const FAMILY_HOUSEKEEPING = 15;
export const FAMILY_MEDICAL = 13;
// Cathedral placed objects occupy 5 type codes (one per cathedral floor slice).
// Runtime visitors all use family 0x24 and share the parking-style state
// machine at 1228:5b5a / 1228:5cd2, routing through 1228:5ddd / 1228:5e7e.
export const FAMILY_CATHEDRAL_BASE = 0x24;
export const FAMILY_CATHEDRAL_MAX = 0x28;

// ─── Family code ↔ tile name mappings ────────────────────────────────────────

/** Maps SimTower family/object-type codes to internal tile name strings. */
export const FAMILY_CODE_TO_TILE: Record<number, string> = {
	[FAMILY_ELEVATOR]: "elevator",
	[FAMILY_ESCALATOR]: "escalator",
	[FAMILY_HOTEL_SINGLE]: "hotelSingle",
	[FAMILY_HOTEL_TWIN]: "hotelTwin",
	[FAMILY_HOTEL_SUITE]: "hotelSuite",
	[FAMILY_RESTAURANT]: "restaurant",
	[FAMILY_OFFICE]: "office",
	[FAMILY_CONDO]: "condo",
	[FAMILY_FAST_FOOD]: "fastFood",
	[FAMILY_RETAIL]: "retail",
	[FAMILY_SECURITY]: "security",
	[FAMILY_CINEMA]: "cinema",
	[FAMILY_RECYCLING_CENTER_UPPER]: "recyclingCenterUpper",
	[FAMILY_RECYCLING_CENTER_LOWER]: "recyclingCenterLower",
	[FAMILY_PARKING]: "parking",
	[FAMILY_PARKING_RAMP]: "parkingRamp",
	[FAMILY_PARTY_HALL]: "partyHall",
	[FAMILY_HOUSEKEEPING]: "housekeeping",
	[FAMILY_MEDICAL]: "medical",
	[FAMILY_METRO_TOP]: "metro",
	[FAMILY_METRO_MIDDLE]: "metro",
	[FAMILY_CATHEDRAL_ANCHOR]: "cathedral",
};

export const LEGACY_VIP_TILE_TO_STANDARD: Record<string, string> = {
	vipSingle: "hotelSingle",
	vipTwin: "hotelTwin",
	vipSuite: "hotelSuite",
};

export const LEGACY_TILE_ALIASES: Record<string, string> = {};

export const TILE_TO_FAMILY_CODE: Record<string, number> = {
	...Object.fromEntries(
		Object.entries(FAMILY_CODE_TO_TILE).map(([k, v]) => [v, Number(k)]),
	),
	lobby: FAMILY_PARKING,
};

// ─── Cinema movie titles ─────────────────────────────────────────────────────
// Resource RT_TYPE_32518 (name 0x81a4, file offset 0xb9a00): 15 Pascal strings.
// Index 14 ("Under the Apple Tree") is unreachable: placement seeds with
// rand()%14 (0..13) and the picker formulas wrap inside [0..6] and [7..13].
// See specs/facility/ENTERTAINMENT.md → Movie Identity.

export const MOVIE_TITLES: readonly string[] = [
	"Revenge of the Big Spider",
	"Northwest Romance",
	"Samurai Cop",
	"Big Wave",
	"Farewell to Morocco",
	"Fear of Shark Teeth",
	"Western Sheriff",
	"Dino Wars",
	"The Making of a Star",
	"Love in N.Y.",
	"Waikiki Moon",
	"My Man of War",
	"Christmas for Both of Us",
	"Casual Friends",
];

export const CINEMA_NEW_MOVIE_COST = 300_000;
export const CINEMA_CLASSIC_MOVIE_COST = 150_000;

// ─── YEN #1001 — payout table ─────────────────────────────────────────────────
// Income per checkout/activation event, indexed by variant tier (0=best, 3=worst).

export const YEN_1001: Record<string, number[]> = {
	...ORIGINAL_TUNING.incomePayoutUnits,
};

// ─── Commercial closure payouts (derive_commercial_venue_state_code) ────────
// Per-venue payout at daily closure, keyed by visitor-count band.
// Bands: <25, 25..34, 35..49, >=50. Values in YEN_UNIT (×1000).
// Source: 11b0:1731 — restaurant (==6) and fast food (==0xc); retail returns 0.

export const COMMERCIAL_CLOSURE_PAYOUTS: Record<string, number[]> = {
	restaurant: [-6, 4, 6, 10],
	fastFood: [-3, 2, 3, 5],
	retail: [0, 0, 0, 0],
};

export const COMMERCIAL_CLOSURE_BANDS = [25, 35, 50] as const;

// ─── Per-type capacity tuning caps ──────────────────────────────────────────
// Source: FUN_11b0_17d3 — returns per-type per-phase capacity ceiling.
// [phaseA, phaseB, override]. The daily rebuild caps the seed at this value,
// then floors the result at 10.

export const COMMERCIAL_CAPACITY_CAPS: Record<
	number,
	[number, number, number]
> = {
	[FAMILY_RESTAURANT]: [...ORIGINAL_TUNING.commercialCapacityCaps.restaurant],
	[FAMILY_FAST_FOOD]: [...ORIGINAL_TUNING.commercialCapacityCaps.fastFood],
	[FAMILY_RETAIL]: [...ORIGINAL_TUNING.commercialCapacityCaps.retail],
};

// ─── YEN #1002 — expense table ────────────────────────────────────────────────
// Operating expenses charged every 3 days, indexed by family code.

// Binary-verified YEN resource #1002. Raw table (family code → value):
// [1]=100, [14]=200, [15]=100, [20]=500, [22]=0, [27]=50, [31]=1000,
// [42]=200, [43]=100, [44]=100 (all others 0). Values here are raw/10 so
// that `value * YEN_UNIT (=1000)` matches the binary's cash_balance × 100
// trace scale (YEN_1001 uses the same raw/10 convention).
export const QUARTERLY_EXPENSES: Record<number, number> = {
	...ORIGINAL_TUNING.quarterlyExpenseUnitsByFamily,
};

// ─── Operational score thresholds ─────────────────────────────────────────────
// [low_threshold, high_threshold] → pairing_status 0/1/2 (C/B/A)

export const OP_SCORE_THRESHOLDS: Record<number, [number, number]> = {
	1: [...ORIGINAL_TUNING.operationalScoreThresholds.oneStar],
	2: [...ORIGINAL_TUNING.operationalScoreThresholds.twoStar],
	3: [...ORIGINAL_TUNING.operationalScoreThresholds.twoStar],
	4: [...ORIGINAL_TUNING.operationalScoreThresholds.threePlusStars],
	5: [...ORIGINAL_TUNING.operationalScoreThresholds.threePlusStars],
};

// ─── Parking expense rates ──────────────────────────────────────────────────
// Per-star tier rate in $100 units: expense = (width) * rate / 10.
// Stars <3 → 0, star 3 → 30, stars >=4 → 100.

export const PARKING_EXPENSE_RATE_BY_STAR: Record<number, number> = {
	1: 0,
	2: ORIGINAL_TUNING.parkingExpenseRatesFromStar2[0],
	3: ORIGINAL_TUNING.parkingExpenseRatesFromStar2[1],
	4: ORIGINAL_TUNING.parkingExpenseRatesFromStar2[2],
	5: ORIGINAL_TUNING.parkingExpenseRatesFromStar2[2],
};

// ─── Activity score star thresholds ──────────────────────────────────────────
// score must exceed STAR_THRESHOLDS[star - 1] to advance from star → star+1

export const STAR_THRESHOLDS = [
	...ORIGINAL_TUNING.ratingScoreThresholds,
	15_000,
];

// ─── Route delay values ───────────────────────────────────────────────────────
// All confirmed from original custom tuning resource PART #1000.

export const DELAY_WAITING = ORIGINAL_TUNING.routing.waitingDelayTicks;
export const DELAY_REQUEUE_FAIL = ORIGINAL_TUNING.routing.requeueFailureDelayTicks;
export const DELAY_ROUTE_FAIL = ORIGINAL_TUNING.routing.routeFailureDelayTicks;
export const DELAY_VENUE_UNAVAIL = ORIGINAL_TUNING.routing.venueUnavailableDelayTicks;
export const DELAY_STOP_ESCALATOR = ORIGINAL_TUNING.routing.escalatorStopDelayTicks;
export const DELAY_STOP_STAIRS = ORIGINAL_TUNING.routing.stairsStopDelayTicks;
