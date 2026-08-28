import { describe, expect, it } from "vitest";
import {
	PART_1000_SHA256,
	PART_1000_U16_HEAD,
	PART_1000_U16_TAIL,
	PART_1000_U32_THRESHOLDS,
	YEN_1000_SHA256,
	YEN_1001_SHA256,
	YEN_1002_SHA256,
} from "./original-tuning.generated";
import { ORIGINAL_TUNING } from "./original-tuning";

describe("decoded original custom tuning resources", () => {
	it("keeps the binary-verified PART layout and source identity", () => {
		expect(PART_1000_SHA256).toBe(
			"af0c7cd015d0e96388c4b0eda20fa630d43c21a9177027206e028479af36353a",
		);
		expect(PART_1000_U16_HEAD).toHaveLength(33);
		expect(PART_1000_U32_THRESHOLDS).toEqual([300, 1_000, 5_000, 10_000]);
		expect(PART_1000_U16_TAIL).toHaveLength(46);
	});

	it("maps route, event, and score tuning without copied literals", () => {
		expect(ORIGINAL_TUNING.routing).toEqual({
			waitTimeoutTicks: 300,
			waitingDelayTicks: 5,
			requeueFailureDelayTicks: 0,
			routeFailureDelayTicks: 300,
			venueUnavailableDelayTicks: 0,
			escalatorStopDelayTicks: 16,
			stairsStopDelayTicks: 35,
		});
		expect(ORIGINAL_TUNING.events.bombRansomByStars2To4).toEqual([
			200_000, 300_000, 1_000_000,
		]);
		expect(ORIGINAL_TUNING.events.helicopterRescueCost).toBe(500_000);
	});

	it("maps original construction costs and economy tables", () => {
		expect(YEN_1000_SHA256).toHaveLength(64);
		expect(YEN_1001_SHA256).toHaveLength(64);
		expect(YEN_1002_SHA256).toHaveLength(64);
		expect(ORIGINAL_TUNING.constructionCosts).toMatchObject({
			lobby: 5_000,
			parking: 3_000,
			elevatorExpress: 400_000,
			elevatorService: 100_000,
			metro: 1_000_000,
		});
		expect(ORIGINAL_TUNING.incomePayoutUnits.hotelSingle).toEqual([3, 2, 1.5, 0.5]);
		expect(ORIGINAL_TUNING.quarterlyExpenseUnitsByFamily[31]).toBe(100);
	});
});
