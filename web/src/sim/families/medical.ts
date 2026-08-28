// families/medical.ts — Medical-center (family 0x0d) service queue + office-worker trip state machine.
//
// TODO: binary — no direct 1228:* function has been mapped to the medical
// subsystem yet. Once the medical dispatch handler is located, add the
// `SEG:OFFSET` header + binary-aligned names here.

export {
	handleMedicalSimArrival,
	invalidateMedicalSlotsForSidecar,
	MEDICAL_NOTIFICATION_MESSAGE,
	processMedicalSim,
	STATE_MEDICAL_DWELL,
	STATE_MEDICAL_TRIP,
	STATE_MEDICAL_TRIP_TRANSIT,
	tryStartMedicalTrip,
} from "../sims/medical";
