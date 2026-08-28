import {
	PART_1000_U16_HEAD,
	PART_1000_U16_TAIL,
	PART_1000_U32_THRESHOLDS,
	YEN_1000_U32,
	YEN_1001_U32,
	YEN_1002_U32,
} from "./original-tuning.generated";

const dollarsFromHundreds = (value: number): number => value * 100;
const yenPayoutUnits = (value: number): number => value / 10;

/**
 * Semantic views over the original PART #1000 and YEN #1000..1002 tables.
 *
 * The generated module preserves every raw entry.  This layer only names
 * mappings corroborated by the Win16 consumers and the original help/strings.
 */
export const ORIGINAL_TUNING = {
	routing: {
		waitTimeoutTicks: PART_1000_U16_HEAD[0],
		waitingDelayTicks: PART_1000_U16_HEAD[1],
		requeueFailureDelayTicks: PART_1000_U16_HEAD[2],
		routeFailureDelayTicks: PART_1000_U16_HEAD[3],
		venueUnavailableDelayTicks: PART_1000_U16_HEAD[4],
		escalatorStopDelayTicks: PART_1000_U16_HEAD[31],
		stairsStopDelayTicks: PART_1000_U16_HEAD[32],
	},
	operationalScoreThresholds: {
		oneStar: [PART_1000_U16_HEAD[5], PART_1000_U16_HEAD[8]],
		twoStar: [PART_1000_U16_HEAD[6], PART_1000_U16_HEAD[9]],
		threePlusStars: [PART_1000_U16_HEAD[7], PART_1000_U16_HEAD[10]],
	},
	commercialCapacityCaps: {
		restaurant: [
			PART_1000_U16_HEAD[15],
			PART_1000_U16_HEAD[16],
			PART_1000_U16_HEAD[17],
		],
		fastFood: [
			PART_1000_U16_HEAD[22],
			PART_1000_U16_HEAD[23],
			PART_1000_U16_HEAD[24],
		],
		retail: [
			PART_1000_U16_HEAD[28],
			PART_1000_U16_HEAD[29],
			PART_1000_U16_HEAD[30],
		],
	},
	ratingScoreThresholds: PART_1000_U32_THRESHOLDS,
	events: {
		fireSpreadRate: PART_1000_U16_TAIL[2],
		fireVerticalDelay: PART_1000_U16_TAIL[3],
		helicopterExtinguishRate: PART_1000_U16_TAIL[4],
		helicopterPromptDelay: PART_1000_U16_TAIL[5],
		rescueCountdownWithSecurity: PART_1000_U16_TAIL[6],
		helicopterRescueCost: dollarsFromHundreds(PART_1000_U16_TAIL[36]),
		bombRansomByStars2To4: [
			dollarsFromHundreds(PART_1000_U16_TAIL[40]),
			dollarsFromHundreds(PART_1000_U16_TAIL[41]),
			dollarsFromHundreds(PART_1000_U16_TAIL[42]),
		],
	},
	parkingExpenseRatesFromStar2: [
		PART_1000_U16_TAIL[37],
		PART_1000_U16_TAIL[38],
		PART_1000_U16_TAIL[39],
	],
	constructionCosts: {
		floor: dollarsFromHundreds(YEN_1000_U32[0]),
		elevator: dollarsFromHundreds(YEN_1000_U32[1]),
		hotelSingle: dollarsFromHundreds(YEN_1000_U32[3]),
		hotelTwin: dollarsFromHundreds(YEN_1000_U32[4]),
		hotelSuite: dollarsFromHundreds(YEN_1000_U32[5]),
		restaurant: dollarsFromHundreds(YEN_1000_U32[6]),
		office: dollarsFromHundreds(YEN_1000_U32[7]),
		condo: dollarsFromHundreds(YEN_1000_U32[9]),
		retail: dollarsFromHundreds(YEN_1000_U32[10]),
		parking: dollarsFromHundreds(YEN_1000_U32[11]),
		fastFood: dollarsFromHundreds(YEN_1000_U32[12]),
		medical: dollarsFromHundreds(YEN_1000_U32[13]),
		security: dollarsFromHundreds(YEN_1000_U32[14]),
		housekeeping: dollarsFromHundreds(YEN_1000_U32[15]),
		cinema: dollarsFromHundreds(YEN_1000_U32[18]),
		recyclingCenter: dollarsFromHundreds(YEN_1000_U32[20]),
		lobby: dollarsFromHundreds(YEN_1000_U32[22]),
		stairs: dollarsFromHundreds(YEN_1000_U32[24]),
		escalator: dollarsFromHundreds(YEN_1000_U32[27]),
		partyHall: dollarsFromHundreds(YEN_1000_U32[29]),
		metro: dollarsFromHundreds(YEN_1000_U32[31]),
		cathedral: dollarsFromHundreds(YEN_1000_U32[36]),
		elevatorExpress: dollarsFromHundreds(YEN_1000_U32[42]),
		elevatorService: dollarsFromHundreds(YEN_1000_U32[43]),
		parkingRamp: dollarsFromHundreds(YEN_1000_U32[44]),
	},
	incomePayoutUnits: {
		hotelSingle: YEN_1001_U32.slice(12, 16).map(yenPayoutUnits),
		hotelTwin: YEN_1001_U32.slice(16, 20).map(yenPayoutUnits),
		hotelSuite: YEN_1001_U32.slice(20, 24).map(yenPayoutUnits),
		office: YEN_1001_U32.slice(28, 32).map(yenPayoutUnits),
		condo: YEN_1001_U32.slice(36, 40).map(yenPayoutUnits),
		retail: YEN_1001_U32.slice(40, 44).map(yenPayoutUnits),
	},
	quarterlyExpenseUnitsByFamily: Object.fromEntries(
		YEN_1002_U32.map((value, family) => [family, yenPayoutUnits(value)]).filter(
			([, value]) => value !== 0,
		),
	) as Record<number, number>,
} as const;
