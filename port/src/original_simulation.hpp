#pragma once

#include "original_tables.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace simtower {

struct OriginalTdtDocument;

// A host-dispatched far-call target from the exact dispatcher at 1200:0196.
// Arguments are stored in the source-level order reconstructed from its
// pushes. Keeping the boundary explicit lets the native host preserve the
// recovered call order around audio, modal, window, and other process effects.
struct OriginalSimulationCall {
  std::uint16_t selector{};
  std::uint16_t offset{};
  std::vector<std::int32_t> arguments{};

  bool operator==(const OriginalSimulationCall&) const = default;
};

// Exact selector:offset/signature set that 1200:0196 can emit. The native host
// checks this contract before dispatching, while the full-cycle scheduler test
// proves that every recovered emission remains inside it.
[[nodiscard]] constexpr bool original_simulation_call_supported(
    std::uint16_t selector,
    std::uint16_t offset,
    std::size_t argument_count) noexcept {
  const auto key = (static_cast<std::uint32_t>(selector) << 16U) | offset;
  switch (key) {
    case 0x118006a8U:
    case 0x11800826U:
    case 0x1180090aU:
      return argument_count == 2U;
    case 0x10880000U:
      return argument_count == 1U;
    case 0x11c80167U:
      return argument_count == 3U;

    case 0x10200dcbU:
    case 0x10200e0bU:
    case 0x10400000U:
    case 0x10400179U:
    case 0x1060003aU:
    case 0x108800deU:
    case 0x108801d1U:
    case 0x10c8006eU:
    case 0x10e80029U:
    case 0x11300000U:
    case 0x11300109U:
    case 0x113001e2U:
    case 0x1170011fU:
    case 0x11780b44U:
    case 0x118005afU:
    case 0x11880977U:
    case 0x11880a20U:
    case 0x119801abU:
    case 0x11a80184U:
    case 0x11a80250U:
    case 0x11a80554U:
    case 0x11a80603U:
    case 0x11b80028U:
    case 0x11c803abU:
    case 0x11e80273U:
    case 0x12200000U:
    case 0x12201059U:
    case 0x1228086bU:
    case 0x12280968U:
    case 0x12280b59U:
    case 0x124001deU:
      return argument_count == 0U;
    default:
      return false;
  }
}

[[nodiscard]] inline bool original_simulation_call_supported(
    const OriginalSimulationCall& call) noexcept {
  return original_simulation_call_supported(
      call.selector, call.offset, call.arguments.size());
}

struct OriginalSimulationState {
  std::uint16_t frame_time{0x09e5};
  std::int32_t current_day{};
  std::int8_t day_phase{6};
  std::uint8_t calendar_phase{};
  std::uint32_t last_tick{};
};

inline constexpr std::uint32_t kOriginalSimulationGateTicks = 6U;
inline constexpr std::uint32_t kOriginalSimulationGateNominalMs =
    kOriginalSimulationGateTicks << 4U;

struct OriginalSimulationStep {
  bool advanced{};
  bool day_changed{};
  std::vector<OriginalSimulationCall> calls{};
};

struct OriginalMetroPulseResult {
  std::size_t touched{};
  // 11e8:0273 requests WAVE/10010 only when at least one Metro part changes
  // from variant word zero to two. A two-to-zero-only pulse is silent.
  bool play_transition_sound{};
};

struct OriginalAnnualEffectStartResult {
  bool started{};
  // 11b8:0028 passes information-panel code seven only when it starts a new
  // effect. Already-active calls are exact no-ops.
  std::uint8_t notification_code{};
};

// Ordered arguments passed to 1118:0a49 by the shared 1178:126c income
// routine. Several facilities can close in one scheduled pass, and each call
// immediately replaces the shared transient Info text.
struct OriginalIncomeStatusResult {
  std::vector<std::uint8_t> codes{};
};

// Pure UI request emitted by the original 1068:0000 modal-dialog wrapper.
// Resource IDs refer to the embedded DIALOG/DTMP pair; the wave is submitted
// before the modal opens. Keeping it as data makes the gameplay path fully
// headless-testable and prevents the simulation layer from opening windows.
struct OriginalEventDialogRequest {
  std::uint16_t dialog_id{};
  std::int32_t argument{};
  std::int32_t wave_resource{};

  [[nodiscard]] constexpr bool valid() const noexcept {
    return dialog_id != 0U;
  }

  friend bool operator==(const OriginalEventDialogRequest&,
                         const OriginalEventDialogRequest&) = default;
};

// Process-visible effects of the exact per-frame rating gate at 1140:002d.
// A present notification value of zero is significant: 1148:007e explicitly
// clears STRL/1010 before an allowed promotion. An absent value means that the
// prerequisite path left the current Info message untouched.
struct OriginalRatingProgressResult {
  std::uint16_t desired_rating{1U};
  bool promoted{};
  std::optional<std::uint16_t> notification_code{};
  OriginalEventDialogRequest dialog{};
};

// Exact 1148:0163 post-construction requirement/treasure result. The rating
// prerequisite flags and money are persisted by the translation; only the
// modal request remains for the native host to consume.
struct OriginalRatingConstructionResult {
  bool changed{};
  bool treasure_awarded{};
  std::int32_t treasure_value{};
  OriginalEventDialogRequest dialog{};
};

struct OriginalBombEventOffer {
  bool offered{};
  std::int16_t floor{-1};
  std::int16_t x{-1};
  std::int16_t ransom{};
  OriginalEventDialogRequest dialog{};
};

struct OriginalBombEventResolution {
  bool paid{};
  bool started{};
  // The pay path requests WAVE/10015 directly. Zero means no direct sound.
  std::int32_t direct_wave_resource{};
  // The search path immediately shows DIALOG/3021 or 3022.
  OriginalEventDialogRequest followup_dialog{};
};

struct OriginalFireEventOffer {
  bool offered{};
  std::int16_t floor{-1};
  std::int16_t x{-1};
  OriginalEventDialogRequest dialog{};
};

enum class OriginalEventDeferredCompletion : std::uint8_t {
  none,
  bomb,
  fire,
};

struct OriginalFacilityDamageRequest {
  std::int16_t floor{};
  std::int16_t x{};
  std::uint16_t flags{};

  friend bool operator==(const OriginalFacilityDamageRequest&,
                         const OriginalFacilityDamageRequest&) = default;
};

struct OriginalEventSoundRequest {
  std::int32_t resource{};
  std::uint16_t repeat{};
  std::uint16_t priority{};
  // 10e8:0450 uses 11c8:0100, which submits only when the reserved channel
  // is idle. Other event sounds call 11c8:0167 directly.
  bool reserved_if_idle{};

  friend bool operator==(const OriginalEventSoundRequest&,
                         const OriginalEventSoundRequest&) = default;
};

struct OriginalFacilityDamageResult {
  bool allowed{};
  bool changed{};
  std::int8_t original_type{};
  std::int16_t alert_code{};
  std::size_t converted_records{};
  std::vector<std::int16_t> rebuilt_floors{};
  std::vector<std::uint16_t> notification_codes{};
  std::vector<OriginalEventSoundRequest> sound_requests{};
};

struct OriginalFacilityDamageSequenceResult {
  std::size_t attempts{};
  std::size_t allowed{};
  std::size_t changed{};
  std::size_t converted_records{};
  std::vector<std::uint16_t> notification_codes{};
  std::vector<OriginalEventSoundRequest> sound_requests{};
};

struct OriginalFacilityHit {
  bool hit{};
  std::int16_t floor{-1};
  std::int16_t x{-1};
  std::size_t tenant_index{};
};

struct OriginalFacilityClickDamageResult {
  OriginalFacilityHit hit{};
  OriginalFacilityDamageResult damage{};
};

struct OriginalReplacementDemolitionResult {
  bool completed{true};
  std::size_t attempts{};
  std::size_t changed{};
  std::vector<std::int16_t> alert_codes{};
  std::vector<std::uint16_t> notification_codes{};
  std::vector<OriginalEventSoundRequest> sound_requests{};
};

struct OriginalEventActionResult {
  bool changed{};
  bool completed{};
  bool focus_requested{};
  std::int16_t focus_floor{-1};
  std::int16_t focus_x{-1};
  // Bomb explosion emits its 11f8:3528 calls before 10f8:033d(0). The
  // caller applies these damage requests in order, then performs this pending
  // Security dispatch through dispatch_original_security_response.
  bool security_dispatch_pending{};
  std::uint16_t security_dispatch_flags{};
  // Present only when the original enables/disables menu item 0x9c48.
  std::optional<bool> fire_menu_enabled{};
  OriginalEventDialogRequest dialog{};
  // 10c8:0254 and 10e8:029f both perform additional mutations only after
  // their modal returns. The host must call complete_original_event_action
  // after dispatching dialog so repaint/re-entrancy observes the same state.
  OriginalEventDeferredCompletion deferred_completion{
      OriginalEventDeferredCompletion::none};
  std::vector<OriginalFacilityDamageRequest> damage_requests{};
  std::vector<OriginalEventSoundRequest> sound_requests{};
};

enum class OriginalEventHostOperation : std::uint8_t {
  play_sounds,
  apply_damage,
  dispatch_security,
  focus_coordinate,
  update_fire_menu,
  show_dialog,
  complete_deferred_action,
};

struct OriginalEventHostPlan {
  std::array<OriginalEventHostOperation, 7> operations{};
  std::size_t operation_count{};

  [[nodiscard]] constexpr std::span<const OriginalEventHostOperation>
  sequence() const noexcept {
    return {operations.data(), operation_count};
  }
};

// Exact host-effect ordering at 10c8:01f7 and 10e8:025a. Bomb submits its
// explosion before applying the literal damage list; Fire spreads/deletes
// first and only then submits the reserved crew sound. Optional process
// effects retain their recovered order after that fixed pair.
[[nodiscard]] OriginalEventHostPlan original_bomb_event_host_plan(
    const OriginalEventActionResult& action) noexcept;
[[nodiscard]] OriginalEventHostPlan original_fire_event_host_plan(
    const OriginalEventActionResult& action) noexcept;

struct OriginalFireCrewResolution {
  bool handled{};
  bool hired{};
  bool focus_requested{};
  std::int16_t focus_floor{-1};
  std::int16_t focus_x{-1};
  std::optional<bool> fire_menu_enabled{};
  OriginalEventDialogRequest followup_dialog{};
  // The scheduled decline calls 10f8:033d(8) only after DIALOG/3014 returns.
  bool security_dispatch_pending{};
  std::uint16_t security_dispatch_flags{};
};

[[nodiscard]] constexpr std::int16_t original_day_phase(
    std::uint16_t frame_time) noexcept {
  // Exact 1200:0543 quotient: CWD followed by signed IDIV 0x0190. The
  // quotient range of a sixteen-bit dividend fits in the signed phase byte
  // written by 1200:0013/01ce/01e4.
  return static_cast<std::int16_t>(
      std::bit_cast<std::int16_t>(frame_time) / 0x0190);
}

[[nodiscard]] std::uint8_t original_calendar_phase(
    std::int32_t current_day) noexcept;

// Exact b406 bit-four test used by 1200:02f1/030b to gate WAVE/5005 and
// WAVE/5003 at frames 0x0050 and 0x0078. The field moves in pre-0x23 save
// headers, so the native host must not replace this with a fixed byte offset.
[[nodiscard]] bool original_special_event_audio_active(
    const OriginalTdtDocument& document) noexcept;

// Exact low-byte b406 test at 1090:0452. Bomb bit zero or Fire bit three
// selects the emergency `1220:0f85` Security-person pass; with both clear the
// frame selects the ordinary `1220:0daf` family pass instead.
[[nodiscard]] bool original_emergency_people_pass_active(
    const OriginalTdtDocument& document) noexcept;

// Exact scheduled flag pair at 1020:0dcb/0e0b. The first raises b406 bit 4
// only on day remainder four below rating five and sets runtime b3e4 to one;
// the second removes that bit while preserving every other flag.
[[nodiscard]] bool raise_original_periodic_b406_flag(
    OriginalTdtDocument& document) noexcept;
[[nodiscard]] bool clear_original_periodic_b406_flag(
    OriginalTdtDocument& document) noexcept;

// Exact frame-0640 call 1240:01de. On every day except remainder three modulo
// nine it lowers b928 and restores the b924 sentinel; remainder-three days are
// byte-exact no-ops.
[[nodiscard]] bool reset_original_periodic_b924_state(
    OriginalTdtDocument& document) noexcept;

// Exact 1140:0411 population bands plus 1148:007e/003d one-rating-per-frame
// prerequisite and promotion path. This includes b922/b923/b924/b928/b929
// resets and the DIALOG/(3028+new-rating), WAVE/10000 request.
[[nodiscard]] OriginalRatingProgressResult
step_original_rating_progress(OriginalTdtDocument& document,
                              const OriginalPartTable& part,
                              std::uint8_t calendar_phase,
                              std::int8_t day_phase) noexcept;

// Exact successful-construction callback at 1148:0163. Type 14 satisfies the
// Security prerequisite, type 5 satisfies Hotel Suites, every construction
// clears the prerequisite-message latch, and ratings one through three can
// receive their one-time PART-selected buried-treasure award.
[[nodiscard]] OriginalRatingConstructionResult
complete_original_rating_construction(OriginalTdtDocument& document,
                                      const OriginalPartTable& part,
                                      std::uint16_t facility_type) noexcept;

// Exact direct 1148:020f boundary retained by hidden WM_COMMAND 9001. It
// raises b922 before validating rating 1..3, selects PART +a8/+aa/+ac,
// applies the shared positive-balance cap, credits other income, and emits
// DIALOG/3040 with WAVE/10001.
[[nodiscard]] OriginalRatingConstructionResult
award_original_rating_treasure(OriginalTdtDocument& document,
                               const OriginalPartTable& part) noexcept;

// Exact three-day accounting rollover at 1060:003a: snapshot the balance,
// zero both eleven-dword income/maintenance bands, and clear the two header
// period accumulators while leaving population accounting untouched.
void reset_original_quarter_finance(OriginalTdtDocument& document) noexcept;

// Exact category selectors at 1060:08be and 1060:0958. A negative result is
// the original unmapped sentinel. The latter's ten keys come directly from
// the executable's segment-13 parallel lookup table at CS:09c1.
[[nodiscard]] std::int16_t original_finance_category_for_type(
    std::uint16_t facility_type) noexcept;
[[nodiscard]] std::int16_t original_maintenance_category_for_type(
    std::uint16_t facility_type) noexcept;

// Exact accounting primitives at 1060:07b3/07f7/0837/0880. Arithmetic wraps
// as the original 32-bit x86 ADD/SUB instructions did. Population and
// maintenance totals are updated even for unmapped types; unmapped income is
// instead accumulated in the header's other-income field.
[[nodiscard]] bool clear_original_population_for_type(
    OriginalTdtDocument& document,
    std::uint16_t facility_type) noexcept;
void add_original_population_for_type(OriginalTdtDocument& document,
                                      std::uint16_t facility_type,
                                      std::int16_t amount) noexcept;
void add_original_income_for_type(OriginalTdtDocument& document,
                                  std::uint16_t facility_type,
                                  std::int32_t amount) noexcept;
void add_original_maintenance_for_type(OriginalTdtDocument& document,
                                       std::uint16_t facility_type,
                                       std::int32_t amount) noexcept;

// Exact YEN/1001 rent/income helpers at 1178:0854 and 1178:08ec. Rent tier
// four is the original no-charge sentinel. The add path applies the shared
// 1178:1377 signed balance cap; the removal path deliberately does not.
// Arithmetic and the removal-side 32-bit NEG retain x86 wrapping behavior.
void add_original_rent_income(OriginalTdtDocument& document,
                              const OriginalYenTable& rent_income,
                              std::uint16_t facility_type,
                              std::uint16_t rent_tier) noexcept;
void remove_original_rent_income(OriginalTdtDocument& document,
                                 const OriginalYenTable& rent_income,
                                 std::uint16_t facility_type,
                                 std::uint16_t rent_tier) noexcept;

// Exact random Metro visual pulse at 11e8:0273, including the b406/b3e8
// gates, Microsoft C runtime RNG advance and one-in-100 selection. It toggles
// the complete tenant variant word for types 31..33 and returns the original
// sound request without opening an audio device.
[[nodiscard]] OriginalMetroPulseResult pulse_original_metro_effects(
    OriginalTdtDocument& document) noexcept;

// Exact persisted annual moving-effect state at 11b8:0028/0060. Static data
// verification proves DS:775a is zero-initialized and has no writers, so the
// starting coordinates are 3000 and 4320-2124. The per-frame update preserves
// 16-bit wrap and clears the complete eight-byte state once x reaches zero.
[[nodiscard]] OriginalAnnualEffectStartResult start_original_annual_effect(
    OriginalTdtDocument& document) noexcept;
[[nodiscard]] bool advance_original_annual_effect(
    OriginalTdtDocument& document) noexcept;

// Complete ambient-sound dispatcher at 11c8:03ab/03fb/0426/05e8/0671/06b6.
// It reconstructs the original six-point visible-world probe, facility/person
// lookup, contextual day sound, and conditional Microsoft-runtime RNG
// consumption. The returned resource is a pure request; this routine never
// opens an audio device.
[[nodiscard]] std::optional<std::int32_t> select_original_ambient_sound(
    OriginalTdtDocument& document,
    bool sound_enabled,
    std::int32_t view_x,
    std::int32_t view_y,
    std::int32_t client_width,
    std::int32_t client_height) noexcept;

// Direct 1100:03ac -> 11c8:03fb facility-information sound boundary. Unlike
// 03ab's random visible-world probe, this selects the clicked tenant directly;
// only 0426's resource variants consume the shared Microsoft-runtime RNG.
[[nodiscard]] std::optional<std::int32_t> select_original_facility_sound(
    OriginalTdtDocument& document,
    bool sound_enabled,
    std::int16_t floor_number,
    std::size_t tenant_index) noexcept;

// Complete scheduled Movie Theater/Party Hall day-start family at
// 1180:05af, including its leading 11f0:0016 pending-activation flush,
// population-category rebuild, PART/1000 capacity selection, and all dc24
// service-state byte updates.
void reset_original_entertainment_for_day(
    OriginalTdtDocument& document,
    const OriginalPartTable& part);

// Exact scheduled entertainment transitions at 1180:06a8, 0826, and 090a.
// The arguments retain the executable's side selector (0/1) and service-group
// selector (zero Party Hall, one Movie Theater). They update the exact dc24,
// person, tenant-dirty, population, balance, and income fields only.
void begin_original_entertainment_arrivals(
    OriginalTdtDocument& document,
    std::uint16_t side,
    std::uint16_t service_group) noexcept;
void advance_original_entertainment_show(
    OriginalTdtDocument& document,
    std::uint16_t unused_side,
    std::uint16_t service_group) noexcept;
[[nodiscard]] OriginalIncomeStatusResult finish_original_entertainment_phase(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::uint16_t side,
    std::uint16_t service_group);

// Complete scheduled commercial-service family at 11a8:0184/0250/0554/0603.
// The day-start pass flushes pending construction, clears all three persisted
// route-index blocks, discards orphaned 18-byte retail records, and rebuilds
// Fast Food/Retail population. The later passes initialize Restaurant service
// and close each type through the executable's signed PART/1000 bands.
[[nodiscard]] std::size_t original_commercial_lane(
    const OriginalTdtDocument& document) noexcept;
[[nodiscard]] std::int16_t original_commercial_capacity(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::uint16_t facility_type) noexcept;
[[nodiscard]] std::int32_t original_commercial_revenue(
    const OriginalPartTable& part,
    std::uint16_t facility_type,
    std::int16_t attendance) noexcept;
void reset_original_commercial_for_day(
    OriginalTdtDocument& document,
    const OriginalPartTable& part);
void reset_original_restaurants_for_evening(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept;
[[nodiscard]] OriginalIncomeStatusResult close_original_nonrestaurant_commercial(
    OriginalTdtDocument& document,
    const OriginalPartTable& part);
[[nodiscard]] OriginalIncomeStatusResult close_original_restaurants_for_night(
    OriginalTdtDocument& document,
    const OriginalPartTable& part);

// Complete scheduled tenant evaluation family at 1130:0000 and 1130:0109,
// together with their 03f4/0360/0630/069e/06e9/09e5/0b92/0cec/0e5c/0f57
// callees and the directly used 1138 adjacency scan. Midnight recalculates
// satisfaction for every tenant and, each third day, performs the exact
// Office/Condo/Retail age, rent, population, service, and person-counter
// transitions. The evening pass performs both ordered Hotel loops.
void advance_original_tenants_at_midnight(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) noexcept;
void advance_original_hotels_for_evening(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept;

// Exact 1130:00b5 all-floor satisfaction refresh invoked by 11d0:0000 when
// the first Map overlay is selected. This deliberately omits the scheduled
// three-day and Hotel-only passes.
void refresh_original_map_tenant_satisfaction(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept;

// Public boundaries used by the exact Facility Information procedure. The
// original painter calls 1130:03f4 directly; changing its rent combo calls
// 1130:06e9 immediately after writing byte 16.
[[nodiscard]] std::int16_t original_tenant_information_performance(
    const OriginalTdtDocument& document,
    std::int16_t floor,
    std::size_t tenant_index) noexcept;
void refresh_original_tenant_information_satisfaction(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::int16_t floor,
    std::size_t tenant_index) noexcept;

// Complete scheduled three-day maintenance family rooted at 1178:0b44. It
// charges ordinary tenants, the rating/width-dependent Lobby parts, all used
// elevator cars, and all used Stair/Escalator records through the exact
// YEN/1002 and PART/1000 tables. The direct 1178:097c/09ee/0a6a wrapping
// debits intentionally bypass 1178:1377's positive-income balance cap.
void charge_original_three_day_maintenance(
    OriginalTdtDocument& document,
    const OriginalYenTable& maintenance_costs,
    const OriginalPartTable& part) noexcept;

// Complete scheduled terrorist-bomb starter at 10c8:006e. Preparation
// performs every gate, RNG advance, floor/span selection, and persisted
// b408/b40a write that precedes the modal. Resolution continues the same
// original stack after DIALOG/3020 returns (1 = Find the Bomb, 2 = Pay Them).
[[nodiscard]] OriginalBombEventOffer prepare_original_bomb_event(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept;
[[nodiscard]] OriginalBombEventResolution resolve_original_bomb_event(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::uint16_t dialog_result) noexcept;
// Search resolution shows DIALOG/3021-or-3022 before the original raises the
// active flag, installs deadline 1200, and dispatches Security. Call this only
// after a resolution whose started member is true and its follow-up modal has
// returned.
void commit_original_bomb_event(OriginalTdtDocument& document) noexcept;

// Literal 10c8:033e event-floor selector. The first constructed run must
// begin above floor zero and must end before floor 119; otherwise it returns
// -1 without consuming the random state.
[[nodiscard]] std::int16_t select_original_event_floor(
    OriginalTdtDocument& document,
    std::int16_t requested) noexcept;

// Complete scheduled fire starter at 10e8:0029, split at its modal boundary.
// Preparation performs the gates, RNG selection and b414/b416 coordinate
// writes that precede DIALOG/3010-or-3011. Commit performs the b412/b406/b410,
// spread-array, and crew writes that follow the modal. day_phase is DS:b3a1.
[[nodiscard]] OriginalFireEventOffer prepare_original_fire_event(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::int8_t day_phase) noexcept;
void commit_original_fire_event(OriginalTdtDocument& document,
                                const OriginalPartTable& part) noexcept;

// Complete 11f8:3528 -> 35ac facility-damage consumer used by both demolition
// and the event families. This includes the 3383 protected-type gate, dd34 and
// person cleanup, population/rent/service side effects, paired entertainment
// and Recycling conversions, 3959 debris/empty record bytes, floor/parking
// rebuilds, and the exact post-success WAVE selection.
[[nodiscard]] OriginalFacilityDamageResult apply_original_facility_damage(
    OriginalTdtDocument& document,
    const OriginalYenTable& rent_income,
    std::int16_t floor,
    std::int16_t x,
    std::uint16_t flags) noexcept;
[[nodiscard]] OriginalFacilityDamageSequenceResult
apply_original_facility_damage_sequence(
    OriginalTdtDocument& document,
    const OriginalYenTable& rent_income,
    std::span<const OriginalFacilityDamageRequest> requests) noexcept;

// Exact main-window point conversion and tenant hit scan at 11f8:3d2d/3e3e.
// The world point is the client point plus b3f0/b3f2; signed IDIV truncation,
// inclusive floor edges, and the first tenant whose right edge exceeds x are
// preserved.
[[nodiscard]] OriginalFacilityHit original_facility_hit_from_client(
    const OriginalTdtDocument& document,
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept;

// Exact facility leg at 11f8:0793: convert the client point through 3e3e and
// invoke 35ac with flags one only when that lookup succeeds. Elevator and
// Stair/Escalator precedence remains in the native caller at 1058:0077.
[[nodiscard]] OriginalFacilityClickDamageResult
apply_original_facility_click_damage(
    OriginalTdtDocument& document,
    const OriginalYenTable& rent_income,
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept;

// Exact Shift-replacement prepass at 11f8:3437. It walks the selected build
// rectangle with demolition flag one, adjusts the lower floor for the listed
// multi-floor types, skips unallocated floors, and aborts on the first
// protected/pending tenant just as the original wrapper does.
[[nodiscard]] OriginalReplacementDemolitionResult
apply_original_replacement_demolition(
    OriginalTdtDocument& document,
    const OriginalYenTable& rent_income,
    std::uint16_t selected_type,
    std::int16_t floor,
    std::int16_t left,
    std::int16_t right) noexcept;

// Complete persisted 10f8:033d Security/Housekeeping response mutation.
// This is public so damage consumers can preserve the original delete-then-
// dispatch ordering after applying OriginalFacilityDamageRequest entries.
void dispatch_original_security_response(OriginalTdtDocument& document,
                                         std::uint16_t flags) noexcept;

// Applies the post-modal tail emitted by advance_original_bomb_event or
// finish_original_fire_event. Calling with none is an exact no-op.
void complete_original_event_action(
    OriginalTdtDocument& document,
    OriginalEventDeferredCompletion completion) noexcept;

// Exact bomb follow-up family at 10c8:01c4/01f7/0254/02bd. Damage is emitted
// as the literal ordered 11f8:3528 call list; no GUI, audio backend, or
// facility-deletion policy is hidden inside the simulation layer.
[[nodiscard]] OriginalEventActionResult trigger_original_bomb_outcome(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    bool found) noexcept;
[[nodiscard]] OriginalEventActionResult check_original_bomb_coordinate(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::int16_t floor,
    std::int16_t x) noexcept;
[[nodiscard]] OriginalEventActionResult advance_original_bomb_event(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept;

// Exact fire follow-up state at 10e8:0147/01e2/025a/029f/0304/0450/076a/
// 07d6/0856. The scheduled offer treats result 1 as No and all other values
// as Yes; the later menu command hires only on result 2. Facility damage and
// reserved-channel audio remain explicit outputs for ordered host dispatch.
[[nodiscard]] bool original_fire_crew_offer_due(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept;
[[nodiscard]] bool original_fire_event_active(
    const OriginalTdtDocument& document) noexcept;
[[nodiscard]] OriginalEventDialogRequest original_fire_crew_offer(
    const OriginalPartTable& part) noexcept;
[[nodiscard]] bool original_fire_crew_menu_offer_available(
    const OriginalTdtDocument& document) noexcept;
// Exact 10d0:0b03-0b31 derived-state rebuild. Unlike the later 10e8:01e2
// command guard, the menu-state calculation requires both an active fire bit
// and a zero fire-crew x word.
[[nodiscard]] bool original_fire_crew_menu_enabled_after_rebuild(
    const OriginalTdtDocument& document) noexcept;
[[nodiscard]] OriginalFireCrewResolution resolve_original_fire_crew_offer(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::uint16_t dialog_result,
    bool scheduled_offer) noexcept;
[[nodiscard]] bool original_fire_covers_coordinate(
    const OriginalTdtDocument& document,
    std::int16_t floor,
    std::int16_t x) noexcept;
[[nodiscard]] bool extinguish_original_fire_at(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::int16_t x) noexcept;
[[nodiscard]] OriginalEventActionResult finish_original_fire_event(
    OriginalTdtDocument& document) noexcept;
[[nodiscard]] OriginalEventActionResult advance_original_fire_event(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept;

// Exact timing gate, frame/day transitions, and far-call schedule from
// 1200:0196. The returned list preserves the original cross-module dispatch
// order so the native host can execute each translated boundary around its
// modal, audio, window, and other process-only effects.
[[nodiscard]] OriginalSimulationStep step_original_simulation(
    OriginalSimulationState& state,
    std::uint32_t now_tick,
    bool fast_mode,
    bool special_tower_flag);

// Integrated 1200:0196 boundary used by the native host. In addition to the
// far-call schedule above, the original writes its clock/day globals, clears
// b3e4 before frame-zero callees, and clears DS:31b8 before frame-0x04b0
// callees. This overload derives the b406 sound gate from the revision-aware
// document and applies those direct mutations before returning the calls.
[[nodiscard]] OriginalSimulationStep step_original_simulation(
    OriginalSimulationState& state,
    OriginalTdtDocument& document,
    std::uint32_t now_tick,
    bool fast_mode);

// Exact 1200:0529 tail. The scheduler samples 1208:05e6 a second time only
// after every scheduled far call (including modal boundaries) has returned,
// then stores that post-dispatch coarse tick in DS:776e. The native host must
// call this immediately after consuming OriginalSimulationStep::calls and
// before entering the later 1090:03ab full-frame pass.
void finish_original_simulation_step(
    OriginalSimulationState& state,
    std::uint32_t completion_tick) noexcept;

}  // namespace simtower
