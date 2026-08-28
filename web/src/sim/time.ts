// Day cycle constants
export const DAY_TICK_MAX = 2600; // 2600 ticks per day
export const DAY_TICK_NEW_DAY = 2300; // 2300: checkpoint where dayCounter increments
export const DAY_COUNTER_WRAP = 11988; // 12 days/year * 999 years

/**
 * Starting dayTick for a new game (from new_game_initializer at 0x10d8_07f6).
 * Value 2533 = daypartIndex 6 — game starts mid-day; first full daily checkpoint
 * sequence (0x000..0xa27) runs only on the second sim day.
 */
export const NEW_GAME_DAY_TICK = 0x9e5; // 2533

export interface TimeState {
	/** Current position within the day (0–2599). */
	dayTick: number;
	/** dayTick / 400, integer (0–6). */
	daypartIndex: number;
	/** Increments at checkpoint 0x08fc each day. Used for calendar logic. */
	dayCounter: number;
	/** dayCounter % 3 === 2 ? 1 : 0 */
	weekendFlag: number;
	/** Monotonically increasing tick counter since game start (for broadcast). */
	totalTicks: number;
}

/** Zero-based time state for unit tests and generic initialization. */
export function createTimeState(): TimeState {
	return {
		dayTick: 0,
		daypartIndex: 0,
		dayCounter: 0,
		weekendFlag: 0,
		totalTicks: 0,
	};
}

/**
 * New-game time state matching new_game_initializer at 0x10d8_07f6.
 * Starts at tick 0x9e5 (daypart 6) so the first full day cycle begins on day 2.
 */
export function createNewGameTimeState(): TimeState {
	return {
		dayTick: NEW_GAME_DAY_TICK,
		daypartIndex: Math.floor(NEW_GAME_DAY_TICK / 400), // = 6
		dayCounter: 0,
		weekendFlag: 0,
		totalTicks: 0,
	};
}

/**
 * Advance time by one tick. Returns the new state and whether the
 * DAY_TICK_INCOME checkpoint was just crossed (triggers day change).
 */
export function advanceOneTick(t: TimeState): {
	time: TimeState;
	incomeCheckpoint: boolean;
} {
	const totalTicks = t.totalTicks + 1;
	let dayTick = t.dayTick + 1;
	let dayCounter = t.dayCounter;
	let weekendFlag = t.weekendFlag;
	let incomeCheckpoint = false;

	if (dayTick >= DAY_TICK_MAX) {
		dayTick = 0;
	}

	if (dayTick === DAY_TICK_NEW_DAY) {
		dayCounter = t.dayCounter + 1;
		if (dayCounter >= DAY_COUNTER_WRAP) {
			dayCounter = 0;
		}
		weekendFlag = dayCounter % 3 === 2 ? 1 : 0;
		incomeCheckpoint = true;
	}

	return {
		time: {
			dayTick,
			daypartIndex: Math.floor(dayTick / 400),
			dayCounter,
			weekendFlag,
			totalTicks: totalTicks,
		},
		incomeCheckpoint,
	};
}

export function preDay4(t: TimeState): boolean {
	return t.daypartIndex < 4;
}
