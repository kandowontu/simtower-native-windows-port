#pragma once

#include "original_people.hpp"
#include "original_tables.hpp"
#include "original_tdt.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace simtower {

// World-coordinate bound read by 11f8:2e64 from DS:71de and divided by 8.
inline constexpr std::uint16_t kOriginalWorldGridWidth = 540;

enum class OriginalConstructionStatus : std::uint8_t {
  ok,
  invalid_span,
  invalid_lobby_height,
  lobby_already_initialized,
  lobby_not_initialized,
  occupied,
  insufficient_funds,
  invalid_floor,
  elevator_limit,
  elevator_car_limit,
  tenant_limit,
  pending_queue_full,
  parking_ramp_required,
  vertical_transport_limit,
  person_cleanup_required,
};

enum class OriginalConstructionInputMessage : std::uint8_t {
  mouse_move,
  button_down,
  button_up,
  double_click,
};

enum class OriginalConstructionInputAction : std::uint8_t {
  ignore,
  press,
  continuous_update,
  continuous_release,
  repeat_retained_placement,
};

// The only four types admitted by 11f8:07d8's move/up branches and captured
// at 0946. Type two occurs only in the later success-sound exclusion table;
// it is not a captured continuous tool.
[[nodiscard]] constexpr bool original_continuous_construction_type(
    std::uint16_t type) noexcept {
  return type == 0U || type == 11U || type == 24U || type == 44U;
}

// Literal 11f8:0ed8 table whose six successful command types rebuild the
// Lobby-transfer and transport route graphs in that order at 0e8f.
[[nodiscard]] constexpr bool original_construction_rebuilds_routes(
    std::uint16_t type) noexcept {
  return type == 1U || type == 22U || type == 24U || type == 27U ||
         type == 42U || type == 43U;
}

// Literal 11f8:0ef0 table. These five types skip the common WAVE/7000 call;
// Floor/Lobby own their separate balance-change sound in the release branch.
[[nodiscard]] constexpr bool original_construction_plays_general_success_wave(
    std::uint16_t type) noexcept {
  return type != 0U && type != 2U && type != 11U && type != 24U &&
         type != 44U;
}

struct OriginalConstructionSuccessAudioPlan {
  bool beep{};
  bool play_general_wave{};

  friend bool operator==(const OriginalConstructionSuccessAudioPlan&,
                         const OriginalConstructionSuccessAudioPlan&) = default;
};

// Exact shared success-audio tail at 11f8:0e21-0e67. BeepOnly is tested for
// equality with one by startup before it reaches this bool; when selected the
// system beep occurs before the type-filtered WAVE/7000 request. Captured
// Floor/Lobby/Parking/Ramp commands reach this tail only once, on button-up.
[[nodiscard]] constexpr OriginalConstructionSuccessAudioPlan
original_construction_success_audio_plan(std::uint16_t type,
                                         bool beep_only) noexcept {
  return {beep_only, original_construction_plays_general_success_wave(type)};
}

// Exact top-level message filter at 11f8:07f3-0955. Down reaches every
// construction type. Move requires MK_LBUTTON and one of the four continuous
// types; up reaches only those same types. The unlisted 0x0203 double-click
// falls through using the placement retained by the preceding down rather
// than recomputing the down-only 24c4/24c6/24c8 fields.
[[nodiscard]] constexpr OriginalConstructionInputAction
original_construction_input_action(
    OriginalConstructionInputMessage message,
    std::uint16_t type,
    bool left_button_down) noexcept {
  switch (message) {
    case OriginalConstructionInputMessage::button_down:
      return OriginalConstructionInputAction::press;
    case OriginalConstructionInputMessage::mouse_move:
      return left_button_down && original_continuous_construction_type(type)
          ? OriginalConstructionInputAction::continuous_update
          : OriginalConstructionInputAction::ignore;
    case OriginalConstructionInputMessage::button_up:
      return original_continuous_construction_type(type)
          ? OriginalConstructionInputAction::continuous_release
          : OriginalConstructionInputAction::ignore;
    case OriginalConstructionInputMessage::double_click:
      return OriginalConstructionInputAction::repeat_retained_placement;
  }
  return OriginalConstructionInputAction::ignore;
}

struct OriginalConstructionReleasePlan {
  bool handled{};
  bool complete_success_tail{};
  bool play_drag_success_wave{};
  bool play_failure_wave{};

  friend bool operator==(const OriginalConstructionReleasePlan&,
                         const OriginalConstructionReleasePlan&) = default;
};

// Exact 11f8:0815-085e/0e12-0ecf completion decision. Every continuous
// release clears capture. Parking and Parking Ramp succeed when any helper
// call incremented 24cc. Floor/Lobby additionally require the balance to
// differ from the down-time snapshot; only those two play WAVE/7000 at this
// early release boundary. A zero effective count plays WAVE/7002 and skips
// the common post-construction/rating tail.
[[nodiscard]] constexpr OriginalConstructionReleasePlan
original_construction_release_plan(
    std::uint16_t type,
    std::uint16_t successful_steps,
    std::int32_t balance_at_press,
    std::int32_t balance_at_release) noexcept {
  if (!original_continuous_construction_type(type)) return {};
  bool succeeded = successful_steps != 0U;
  bool play_success = false;
  if (succeeded && (type == 0U || type == 24U)) {
    succeeded = balance_at_press != balance_at_release;
    play_success = succeeded;
  }
  return {true, succeeded, play_success, !succeeded};
}

struct OriginalParkingDragRunState {
  bool initialized{};
  std::int32_t built_left{};      // DS:24d2
  std::int32_t built_right{};     // DS:24d4
  std::int32_t retained_left{};   // DS:24d6
  std::int32_t retained_right{};  // DS:24d8

  friend bool operator==(const OriginalParkingDragRunState&,
                         const OriginalParkingDragRunState&) = default;
};

struct OriginalParkingDragRunPlan {
  OriginalParkingDragRunState next_state{};
  std::vector<std::int32_t> unit_lefts{};
};

// Exact four-cell attempt sequence at 11f8:240d. Construction consumes the
// retained pointer interval from the preceding helper call, then publishes the
// current message's pointer interval for the next call. The initial snapped
// interval can differ from the current pointer only on the abnormal retained
// double-click path.
[[nodiscard]] OriginalParkingDragRunPlan original_parking_drag_run_plan(
    OriginalParkingDragRunState state,
    std::int32_t initial_left,
    std::int32_t initial_right,
    std::int32_t current_left,
    std::int32_t current_right);

struct OriginalParkingRampDragRunState {
  bool initialized{};
  std::int16_t built_upper_exclusive{};     // DS:24da
  std::int16_t built_lower{};               // DS:24dc
  std::int16_t retained_upper_exclusive{};  // DS:24de
  std::int16_t retained_floor{};            // DS:24e0

  friend bool operator==(const OriginalParkingRampDragRunState&,
                         const OriginalParkingRampDragRunState&) = default;
};

struct OriginalParkingRampDragAttempt {
  std::int16_t floor{};
  std::int32_t left{};

  friend bool operator==(const OriginalParkingRampDragAttempt&,
                         const OriginalParkingRampDragAttempt&) = default;
};

struct OriginalParkingRampDragRunPlan {
  OriginalParkingRampDragRunState next_state{};
  std::vector<OriginalParkingRampDragAttempt> attempts{};
};

// Exact vertical attempt sequence at 11f8:25a2. Like Parking, it consumes the
// prior pointer floor and only publishes the current mouse floor at the tail.
[[nodiscard]] OriginalParkingRampDragRunPlan
original_parking_ramp_drag_run_plan(
    OriginalParkingRampDragRunState state,
    std::int16_t initial_floor,
    std::int16_t current_floor,
    std::int32_t construction_left);

enum class OriginalCapturedHelperSound : std::uint8_t {
  none,
  priority_five,
  reserved_if_idle,
};

struct OriginalCapturedHelperCompletionPlan {
  bool returned_success{};
  bool increment_successful_step{};
  OriginalCapturedHelperSound sound{OriginalCapturedHelperSound::none};
  bool clear_priority_sound_latch{};

  friend bool operator==(const OriginalCapturedHelperCompletionPlan&,
                         const OriginalCapturedHelperCompletionPlan&) = default;
};

// 240d/25a2 retain only the final 17fd return in DX/BX. Earlier accepted
// units still mutate the tower, but they do not make this helper call count as
// successful when the final attempt fails. An accepted final attempt emits
// exactly one construction sound and increments 24cc once.
[[nodiscard]] constexpr OriginalCapturedHelperCompletionPlan
original_captured_helper_completion_plan(
    bool attempted,
    bool final_attempt_succeeded,
    bool priority_sound_latch_armed) noexcept {
  if (!attempted || !final_attempt_succeeded) return {};
  return {
      true,
      true,
      priority_sound_latch_armed
          ? OriginalCapturedHelperSound::priority_five
          : OriginalCapturedHelperSound::reserved_if_idle,
      priority_sound_latch_armed,
  };
}

struct OriginalFloorLobbyHelperCompletionPlan {
  bool returned_success{};
  bool increment_successful_step{};
  bool keep_pointer_visible{};
  OriginalCapturedHelperSound sound{OriginalCapturedHelperSound::none};
  bool clear_priority_sound_latch{};

  friend bool operator==(const OriginalFloorLobbyHelperCompletionPlan&,
                         const OriginalFloorLobbyHelperCompletionPlan&) = default;
};

struct OriginalWorldMutationPresentationPlan {
  bool mark_document_dirty{};
  bool invalidate_main_surface{};
  bool repaint_info_balance_synchronously{};
  bool invalidate_map_surface{};

  friend bool operator==(
      const OriginalWorldMutationPresentationPlan&,
      const OriginalWorldMutationPresentationPlan&) = default;
};

// Exact presentation ownership around 1038:002f and the construction debit
// helpers at 1178:01db/027c/0697. A persisted construction mutation dirties
// only the world/tile transport; a nonzero balance change is painted directly
// through 1118:0143. Neither path invalidates Map: 1080:09c3 retains that
// window's independent sixteen-clock-tick refresh at 1090:046f-047c.
[[nodiscard]] constexpr OriginalWorldMutationPresentationPlan
original_world_mutation_presentation_plan(
    bool document_changed,
    bool balance_changed) noexcept {
  if (!document_changed) return {};
  return {true, true, balance_changed, false};
}

// 11f8:26dd returns success and invokes 1080:0054 whenever its final 284d call
// succeeds, including a request already covered by an existing Floor/Lobby.
// 284d's WAVE/7001 boundary is narrower: the overlapping-record path must
// compute a nonzero construction cost. The disjoint/empty-floor path delegates
// to 17fd and never reaches this sound boundary even when it charges money.
// The first requested sound consumes 24ca; later requests use the
// reserved-channel-if-idle wrapper. Because 26dd overwrites its return after
// every story, an earlier story's sound request remains observable even when
// the final story fails and suppresses 24cc/auto-scroll.
[[nodiscard]] constexpr OriginalFloorLobbyHelperCompletionPlan
original_floor_lobby_helper_completion_plan(
    bool final_constructor_succeeded,
    bool any_constructor_requested_sound,
    bool priority_sound_latch_armed) noexcept {
  OriginalCapturedHelperSound sound = OriginalCapturedHelperSound::none;
  if (any_constructor_requested_sound) {
    sound = priority_sound_latch_armed
        ? OriginalCapturedHelperSound::priority_five
        : OriginalCapturedHelperSound::reserved_if_idle;
  }
  return {
      final_constructor_succeeded,
      final_constructor_succeeded,
      final_constructor_succeeded,
      sound,
      any_constructor_requested_sound && priority_sound_latch_armed,
  };
}

struct OriginalLobbyPlacement {
  std::int16_t floor{};
  std::int32_t left{};
  std::int32_t right{};

  friend bool operator==(const OriginalLobbyPlacement&,
                         const OriginalLobbyPlacement&) = default;
};

struct OriginalVerticalTransportHit {
  bool hit{};
  std::size_t transport_index{};
  std::int16_t floor{};
  std::int16_t x{};

  friend bool operator==(const OriginalVerticalTransportHit&,
                         const OriginalVerticalTransportHit&) = default;
};

struct OriginalElevatorHit {
  bool hit{};
  std::size_t elevator_index{};
  std::int16_t floor{};
  std::int16_t car_index{-1};

  friend bool operator==(const OriginalElevatorHit&,
                         const OriginalElevatorHit&) = default;
};

// Exact top-level outcomes of 10a0:0201 after 1397 has reported an Elevator
// hit. A no-car shaft-body hit inside the stored span returns word_3c
// verbatim: a visible shaft consumes the click, while a hidden shaft returns
// zero so 1058:0093 continues to Stair/Escalator and facility demolition.
enum class OriginalElevatorBulldozerAction : std::uint8_t {
  miss,
  pass_through,
  consume,
  remove_car,
  remove_shaft,
};

[[nodiscard]] constexpr OriginalElevatorBulldozerAction
original_elevator_bulldozer_action(
    bool hit,
    std::int16_t car_index,
    std::uint8_t car_count,
    std::int16_t floor,
    std::int16_t bottom_floor,
    std::int16_t top_floor,
    std::uint16_t show) noexcept {
  if (!hit) return OriginalElevatorBulldozerAction::miss;
  if (car_index >= 0 && car_count != 1U) {
    return OriginalElevatorBulldozerAction::remove_car;
  }
  if (car_index < 0 && floor >= bottom_floor && floor <= top_floor) {
    return show != 0U ? OriginalElevatorBulldozerAction::consume
                      : OriginalElevatorBulldozerAction::pass_through;
  }
  return OriginalElevatorBulldozerAction::remove_shaft;
}

enum class OriginalElevatorServiceFloorGate : std::uint8_t {
  eligible,
  invalid_elevator,
  invalid_floor,
  inactive_shaft,
  outside_shaft,
  active_car_home,
  forbidden_new_stop,
};

// Exact type-0x18 specialization of 11f8:3da4 and the placement fields
// initialized at 11f8:08ce-090b. The lobby pointer is centered on its
// four-cell (32-pixel) footprint before snapping to the 8x36 world grid.
[[nodiscard]] OriginalLobbyPlacement original_lobby_placement_from_client(
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept;

// Type 0 is the one-cell Floor tool selected through the Lobby command
// group. 11f8:0000 gives it an eight-pixel footprint, and 11f8:3da4 centers
// that footprint on the pointer before applying the common 8x36 grid snap.
[[nodiscard]] OriginalLobbyPlacement original_floor_placement_from_client(
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept;

// Hidden 11f8:0955-098c fresh-tower transaction. A construction press at the
// exact floor-zero/cell-zero sentinel doubles the initial 20,000 balance via
// 1178:076f only before a Lobby or any floor-zero tenant exists.
[[nodiscard]] bool apply_original_initial_balance_bonus(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::int32_t left) noexcept;

// Type 7 uses the hard-coded nine-cell width selected by 11f8:0000. The
// common pointer conversion at 11f8:3da4 centers that 72-pixel shape before
// snapping it to the original 8x36 world grid.
[[nodiscard]] OriginalLobbyPlacement original_office_placement_from_client(
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept;

// The shape widths loaded by 11f8:0000 and used by 11f8:3da4 for the three
// elevator commands (1, 42, 43), ordinary facilities, Stairs (22),
// Escalator (27), and Parking Ramp (0x2c). Zero denotes a type without this
// centered placement path.
[[nodiscard]] std::uint16_t original_facility_width_cells(
    std::uint16_t type) noexcept;

// Exact common pointer conversion for a type-3..15 facility: center the
// resource-backed footprint on the cursor, then snap to the 8x36 grid.
[[nodiscard]] OriginalLobbyPlacement original_facility_placement_from_client(
    std::uint16_t type,
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept;

// Exact 10c0:0606 Stair/Escalator hit test used by both Bulldozer and
// Magnifying Glass. Tall lobby-spanning transports use their rectangular
// story range; ordinary one-story records additionally apply the original
// diagonal 24-pixel band within the eight-cell footprint.
[[nodiscard]] OriginalVerticalTransportHit
original_vertical_transport_hit_from_client(
    const OriginalTdtDocument& document,
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept;

// Exact shaft and car hit geometry at 10a0:1397 -> 1090:227b. Type-zero
// express shafts are six cells wide; other shafts are four. The returned
// car is the last active one whose inset 31-pixel rectangle contains the
// pointer, matching the original loop's overwrite behavior.
[[nodiscard]] OriginalElevatorHit original_elevator_hit_from_client(
    const OriginalTdtDocument& document,
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept;

// Exact pre-routing gates of the mode-one Finger click at 10a0:0000/0085,
// including the active-car home-floor scan at 102d and the type-zero/Lobby
// new-stop exclusions at 1296/12e0/133b. An eligible result still precedes
// 11b0:0b8b's route-loss warning and the toggle/cleanup commit.
[[nodiscard]] OriginalElevatorServiceFloorGate
original_elevator_service_floor_gate(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept;

// Exact 11b0:0cfe predicate used by 10a0:0179 while deciding whether full
// shaft removal needs DIALOG/1005 confirmation, followed by the complete
// bottom-to-top serviced-floor scan from 0179.
[[nodiscard]] bool original_elevator_floor_connected_for_shaft_removal(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept;
[[nodiscard]] bool original_elevator_shaft_demolition_requires_confirmation(
    const OriginalTdtDocument& document,
    std::size_t elevator_index) noexcept;

// Exact ordered `11b0:049f` then `11b0:00f2` transport-graph rebuild used by
// Elevator Finger edits. It reconstructs the sixteen db9c Lobby-transfer
// masks, then every floor dword in the 24 elevator block_c2 graphs and eight
// routes_bff0 graphs. Bit numbering follows 1208's MSB-first convention.
void rebuild_original_transport_route_graphs(
    OriginalTdtDocument& document) noexcept;

// Exact 11b0:0763 six-floor boundary scan used by the bff0 route-summary
// rebuild, including its three-floor cutoff after crossing a non-bit-zero
// vertical link.
[[nodiscard]] int original_route_boundary(
    const std::array<std::byte, 0x78>& links,
    int floor,
    bool upward) noexcept;

// Exact `11b0:0b8b` value passed as the argument to DIALOG/1005 before an
// Elevator Finger toggle. Zero means no confirmation; the original nonzero
// codes are one (ordinary loss), two (type-2 loss), and three (same-type
// Lobby-transfer condition).
[[nodiscard]] std::uint16_t original_elevator_service_floor_warning_code(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept;

// Exact non-destructive half of `10a0:0085`: after the translated gates,
// adding a previously absent stop writes the literal boolean one and invokes
// the ordered `11b0:049f`/`00f2` transport-graph rebuild. Removal is kept out
// of this helper because the original must first run `10a0:14cc`'s complete
// passenger and waiting-ring cleanup.
[[nodiscard]] bool add_original_elevator_service_floor(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept;

// Exact preflight over the `10a0:14fa/1625` sources: active-car destination
// occupancy and the two forty-entry waiting-ring counts for this mapped floor.
// A true result means removal must use the person-bearing cleanup overload
// before committing the structural stop mutation.
[[nodiscard]] bool original_elevator_service_floor_has_people(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept;

// Exact zero-person removal path of `10a0:0085/14cc`: toggle the serviced
// byte off, rebuild both transport graphs in caller order, release matching
// up/down ownership for every active car, decrement word 10 per released
// lane, and run `1090:0bcf`. This specialization intentionally refuses any
// floor with people; the native cleanup overload in original_people.hpp owns
// the now-translated `1210:0883/1220:16ab` family callbacks.
[[nodiscard]] bool remove_original_elevator_service_floor_without_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept;

// Complete person-bearing `10a0:0085 -> 14cc` removal transaction. It clears
// the service byte, rebuilds routing, then performs 14fa's car arrivals before
// 1625's up/down waiting rings with the native 0883/16ab family dispatcher.
// Invalid/malformed cleanup leaves the source document unchanged.
[[nodiscard]] OriginalNativeElevatorFloorPeopleCleanupResult
remove_original_elevator_service_floor(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{}) noexcept;

// Exact post-10a0:154a mutation in the multi-car branch of 10a0:036e. The
// caller must first perform the selected car's per-floor passenger and queue
// cleanup. The last remaining car is protected by 10a0:0201.
[[nodiscard]] bool commit_original_elevator_car_demolition(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index) noexcept;

struct OriginalElevatorDemolitionResult {
  bool removed{};
  bool removed_entire_shaft{};
  std::size_t car_passengers{};
  std::size_t waiting_passengers{};
  std::vector<OriginalPersonFamilyDispatchResult> family_dispatches{};
};

// Complete `10a0:036e` selected-car transaction after the caller's 0201
// last-car gate. It runs 154a on every shaft floor, clears the selected car,
// reassigns nonempty unowned up/down rings in floor order, and commits only
// after every native family callback succeeds.
[[nodiscard]] OriginalElevatorDemolitionResult remove_original_elevator_car(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{});

// Complete post-confirmation full-shaft branch of `10a0:0201`: clear all
// service bytes, rebuild routing, run 14cc bottom-to-top, reset 1090's
// assignment/floor/car state, and finally clear the used byte. The native
// caller owns the exact DIALOG/1005 confirmation boundary from 10a0:0179.
[[nodiscard]] OriginalElevatorDemolitionResult
remove_original_elevator_shaft(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{});

// Exact post-person-cleanup mutation at 10c0:050d-05a4. The caller must
// first perform 1218:0000's family-specific cleanup for people currently
// using this record. This clears the selected bd70 record, reconstructs all
// 120 cf10 direction flags from the remaining 64 records, and rebuilds the
// eight bff0 route summaries.
[[nodiscard]] bool commit_original_vertical_transport_demolition(
    OriginalTdtDocument& document,
    std::size_t transport_index) noexcept;

struct OriginalVerticalTransportDemolitionResult {
  bool removed{};
  std::vector<OriginalPersonFamilyDispatchResult> family_dispatches{};
};

// Complete `10c0:04e0` persisted transaction: run `1218:0000` against the
// still-live transport, clear/reinitialize its bd70 record, and reconstruct
// cf10 plus bff0 from the remaining records. The source document changes only
// after the entire native person pass and structural commit succeed.
[[nodiscard]] OriginalVerticalTransportDemolitionResult
remove_original_vertical_transport(
    OriginalTdtDocument& document,
    std::size_t transport_index,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{});

struct OriginalConstructionResult {
  OriginalConstructionStatus status{OriginalConstructionStatus::ok};
  std::int32_t cost{};
  // One-based STRL/1003 entry passed to 1118:0933 by the rejection path.
  // Zero is deliberate: several original helpers fail silently and leave
  // the current Info message untouched.
  std::uint16_t construction_status_code{};
  // True when a successful Floor/Lobby command crossed 284d's nonzero-cost
  // WAVE/7001 boundary. A charged empty/disjoint-floor construction runs
  // through 17fd instead and deliberately leaves this false.
  bool construction_sound_requested{};
  // True when this call published any persistent state, even if a later
  // story overwrote 26dd's final return with failure. This preserves the
  // original helper's non-transactional partial mutations in the native UI.
  bool document_changed{};
  // 11f8:0fea reaches its DS:783c=1 / synchronous 1080:05a1 tail only after
  // allocating a brand-new shaft. Adding a car to an existing shaft jumps
  // from 122f directly to the success return and leaves the selected command
  // mode/surface untouched.
  bool new_elevator_shaft_created{};

  [[nodiscard]] bool succeeded() const noexcept {
    return status == OriginalConstructionStatus::ok;
  }
};

struct OriginalElevatorShaftExtensionResult {
  OriginalConstructionStatus status{OriginalConstructionStatus::ok};
  std::int32_t cost{};
  std::int16_t target_floor{};
  bool span_clamped{};

  [[nodiscard]] bool succeeded() const noexcept {
    return status == OriginalConstructionStatus::ok;
  }
};

struct OriginalElevatorShaftShrinkResult {
  OriginalElevatorShaftExtensionResult shaft{};
  std::vector<OriginalPersonFamilyDispatchResult> family_dispatches{};
  std::size_t waiting_passengers{};
  std::size_t car_passengers{};

  [[nodiscard]] bool succeeded() const noexcept {
    return shaft.succeeded();
  }
};

enum class OriginalElevatorShaftEnd : std::uint8_t {
  upper,
  lower,
};

// Exact extension-only portion of the captured Elevator Finger paths at
// `10a0:0819/0b87`. Collision and funds preflight use the unbounded drag
// target before ordinary/service shafts are clamped to 29 floors, matching
// the executable. The commit updates service bytes, both transport graphs,
// the serialized 324-byte floor-record sequence, construction accounting,
// shaft endpoint, and `11f8:15f7` floor coverage. Targets inside the current
// shaft are rejected because those are shrinking operations handled by the
// separate shrink transaction.
[[nodiscard]] OriginalElevatorShaftExtensionResult
extend_original_elevator_shaft(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t target_floor,
    const OriginalYenTable& construction_costs);

// Exact zero-person shrink transaction from `10a0:0819/0b87`. It preflights
// every removed car destination and waiting ring before mutation, then clears
// service bytes, clamps active-car home/current state, rebuilds route graphs,
// compacts the serialized floor-record sequence, moves the endpoint, and runs
// the structural `154a` owner-release/recompute tail. Passenger-bearing ranges
// return person_cleanup_required without changing the document.
[[nodiscard]] OriginalElevatorShaftExtensionResult
shrink_original_elevator_shaft_without_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    OriginalElevatorShaftEnd shaft_end,
    std::int16_t target_floor);

// Complete passenger-bearing shrink transaction from `10a0:0819/0b87`.
// It preserves the executable's unusual global order: clear stops/clamp cars
// and rebuild routing, drain `1625` for every removed floor, compact/remap
// floor records and publish the endpoint, then run `14fa` for every removed
// floor. All work is committed atomically and every native family-dispatch
// request is returned to the host.
[[nodiscard]] OriginalElevatorShaftShrinkResult
shrink_original_elevator_shaft(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    OriginalElevatorShaftEnd shaft_end,
    std::int16_t target_floor,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{});

enum class OriginalPendingStepStatus : std::uint8_t {
  no_pending,
  advanced,
  activated,
  unsupported_pending_type,
  malformed_queue,
};

// Exact type-0 path at 11f8:0a39 -> 11f8:26dd -> 11f8:284d. A click starts
// with one cell and drag calls pass the union of the anchor and current cell.
// New coverage is stored as status-2 type-0 records, contiguous gaps outside
// an existing floor are filled automatically, adjacent type-0 records are
// coalesced, and 1178:0583 charges only newly exposed floor-edge cells.
[[nodiscard]] OriginalConstructionResult build_original_floor(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    std::uint16_t right,
    const OriginalYenTable& construction_costs);

// Exact state mutation for the first lobby placement through
// 11f8:26dd -> 11f8:284d -> 11f8:17fd. The caller has already selected a
// one-, two-, or three-story lobby, as 11f8:0955-09bb does before this path.
// It includes the executable's YEN/1000 construction charge, the automatic
// floor 11/12 lobby records, 1228:0103 lobby post-initialization, and the
// persisted 94-word tenant lookup update. Height publication and story calls
// are deliberately non-transactional: 07d8 writes b3e6 first, 26dd attempts
// every selected story in order, and only the final story's status is returned.
// Rendering and UI invalidation are outside this pure state transition.
[[nodiscard]] OriginalConstructionResult build_original_initial_lobby(
    OriginalTdtDocument& document,
    std::uint16_t left,
    std::uint16_t right,
    std::uint16_t lobby_height,
    const OriginalYenTable& construction_costs);

// Exact constrained continuation of the type-0x18 path through 11f8:26dd
// and 11f8:284d after the first segment exists. Desired bounds are the union
// of the press anchor and current pointer supplied by the caller. Each story
// commits independently; moving back preserves outlying Lobby edge records
// rather than recoalescing or demolishing them, and only the final story's
// status controls 26dd's return.
[[nodiscard]] OriginalConstructionResult extend_original_lobby(
    OriginalTdtDocument& document,
    std::uint16_t desired_left,
    std::uint16_t desired_right,
    const OriginalYenTable& construction_costs);

// Exact non-ground type-24 branch of 11f8:26dd/284d. Runtime floors
// 24,39,54,69,84,99 are the original 15-floor sky-lobby landings.
[[nodiscard]] OriginalConstructionResult build_original_sky_lobby(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    std::uint16_t right,
    const OriginalYenTable& construction_costs);

// Exact shared 11f8:09f7/09fd/0a03 -> 0fea elevator constructor. Raw command
// 1 creates a standard type-1/capacity-21/four-cell shaft; command 42 creates
// an express type-0/capacity-42/six-cell shaft; command 43 creates a service
// type-2/capacity-21/four-cell shaft. A click on an existing matching shaft
// follows 10a0:1080 and charges the command's distinct PART/1000 car word.
// A new shaft follows 10a0:10e8, 1090:00d9/0192, and 11f8:15f7.
[[nodiscard]] OriginalConstructionResult build_original_elevator(
    OriginalTdtDocument& document,
    std::uint16_t command_type,
    std::int16_t floor,
    std::uint16_t x,
    const OriginalYenTable& construction_costs,
    const OriginalPartTable& part);

// Compatibility specialization for the raw type-1 standard command.
[[nodiscard]] OriginalConstructionResult build_original_standard_elevator(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t x,
    const OriginalYenTable& construction_costs,
    const OriginalPartTable& part);

// Exact shared type-22/type-27 path at 11f8:1452. Stairs use shape 1 and
// require covered floor at both ends. Escalators use shape 0 and additionally
// require the executable's ten-type commercial/public landing whitelist.
// A multi-story lobby rewrites either shape into its lobby-spanning form.
// Successful construction writes one of the 64 persisted bd70 records,
// updates cf10's directional-link bits and rebuilds the eight bff0 route
// summaries before applying only the type's YEN/1000 charge.
[[nodiscard]] OriginalConstructionResult build_original_vertical_transport(
    OriginalTdtDocument& document,
    std::uint8_t type,
    std::int16_t floor,
    std::uint16_t x,
    const OriginalYenTable& construction_costs);

// Exact type-7 specialization of the generic 11f8:17fd constructor and its
// 1228:0000 post-initializer. An Office is nine cells wide, cycles variants
// 0..5, allocates six 16-byte simulation records, receives a floor lookup
// key, and enters the ten-slot deferred activation queue at 11f0:004b.
[[nodiscard]] OriginalConstructionResult build_original_office(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    std::uint8_t variant,
    const OriginalYenTable& construction_costs);

// Exact shared 11f8:17fd -> 1228:0000 construction path for facility types
// 3, 4, and 5 (Single Room, Twin Room, and Hotel Suite). The original
// variant cycles contain 2, 4, and 2 appearances respectively; activation
// allocates 2, 3, and 3 guest records through the common deferred queue.
[[nodiscard]] OriginalConstructionResult build_original_hotel_room(
    OriginalTdtDocument& document,
    std::uint8_t type,
    std::int16_t floor,
    std::uint16_t left,
    std::uint8_t variant,
    const OriginalYenTable& construction_costs);

// Exact type-9 path selected at 11f8:0bd7. A Condo is sixteen cells wide,
// cycles three appearances through DS:7956, and allocates three simulation
// records through 1220:0be3 when the shared deferred queue activates it.
[[nodiscard]] OriginalConstructionResult build_original_condo(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    std::uint8_t variant,
    const OriginalYenTable& construction_costs);

// Exact type-10 path at 11f8:0c92. Retail Shop is twelve cells wide, has no
// appearance selector, is capped at 512 instances, and allocates 48 simulation
// records through 1220:0c4b. Its reversed basement support rules are included.
[[nodiscard]] OriginalConstructionResult build_original_retail_shop(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs);

// Exact type-6 and type-12 branches sharing 1228:030c. Both use the common
// commercial floor rules, allocate 48 people, then reserve and initialize an
// 18-byte service slot through 11a8:07d3 when construction activates.
[[nodiscard]] OriginalConstructionResult build_original_restaurant(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs);

[[nodiscard]] OriginalConstructionResult build_original_fast_food(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs);

// Exact type-14 path at 11f8:0c0d. Security is sixteen cells wide, is capped
// at ten instances, initializes six state-1 people, and registers its floor
// and lookup key in the first free persisted cf88 word during activation.
[[nodiscard]] OriginalConstructionResult build_original_security(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs);

// Exact type-15 default branch at 11f8:0dc4. Housekeeping is fifteen cells
// wide, initializes six type-15 simulation records through 1220:0cc5, and
// registers its floor/key pair in the same persisted cf88 table as Security.
[[nodiscard]] OriginalConstructionResult build_original_housekeeping(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs);

// Exact type-17 default branch at 11f8:0dc4. The two-cell SECOM Center uses
// the generic deferred constructor, reserves six records, then 1228:075b
// deliberately leaves its lookup key and those reserved records inactive.
[[nodiscard]] OriginalConstructionResult build_original_secom_center(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs);

// Exact type-18 composite path at 11f8:1fa5. A Movie Theater constructs
// paired 24-cell type-18/type-19 records on adjacent floors. Activation via
// 1180:0352 expands each into a seven-cell type-34/type-35 entrance followed
// by its 24-cell body and links both floors through a persisted dc24 record.
[[nodiscard]] OriginalConstructionResult build_original_movie_theater(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs);

// Exact type-20 composite path at 11f8:0d9a -> 11f8:1fa5. A Recycling Center is
// a vertically paired 25-cell type-20/type-21 facility. After the first pair,
// 1088:02c8 requires each new pair to be x-aligned and vertically adjacent
// to an already activated center. Both halves retain lookup key 0xff and their
// six negative reservation records when deferred construction activates.
[[nodiscard]] OriginalConstructionResult build_original_recycling_center(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs);

// Exact type-29 composite path at 11f8:0ccb -> 11f8:1fa5. A Party Hall is
// a paired 24-cell type-29/type-30 facility on adjacent floors, shares the
// sixteen-entry b400/dc24 capacity with Movie Theater, initializes forty
// type-29 people per half, and links both tenants through one dc24 record.
[[nodiscard]] OriginalConstructionResult build_original_party_hall(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs);

// Exact unique type-31 path at 11f8:0d04 -> 11f8:20e7. Metro Station is a
// thirty-cell, three-floor type-31/type-32/type-33 stack fixed to runtime
// floors 2/1/0. Its bottom type owns 240 initialized passenger records while
// the upper two retain six inactive reservations apiece.
[[nodiscard]] OriginalConstructionResult build_original_metro_station(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs);

// Exact unique type-36 path at 11f8:0d4b -> 11f8:2291. Cathedral is a
// 28-cell, five-floor type-36..40 stack fixed to runtime floors 113..109.
[[nodiscard]] OriginalConstructionResult build_original_cathedral(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs);

// Exact type-13 path at 11f8:0c46. Medical Center is 26 cells wide, cycles
// three construction selectors, is capped at ten, and activation replaces the
// selector with an allocated dbfc service index registered in bd5c.
[[nodiscard]] OriginalConstructionResult build_original_medical_center(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    std::uint8_t variant,
    const OriginalYenTable& construction_costs);

// Exact type-0x2c path at 11f8:25a2. A Parking Ramp is sixteen cells wide,
// basement-only, immediate (it never enters the deferred construction queue),
// and the first ramp must begin on B1 (runtime floor 9). Subsequent ramps use
// the same x coordinate and rebuild the original ramp/parking connectivity
// state through 1198:07e6.
[[nodiscard]] OriginalConstructionResult build_original_parking_ramp(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs);

// Exact single four-cell segment created by 11f8:240d. Parking is
// basement-only, requires a type-0x2c ramp on the same floor, is capped at
// 512 segments, immediately allocates a persisted cf9c parking record, and
// rebuilds connectivity and the persisted b958 index table.
[[nodiscard]] OriginalConstructionResult build_original_parking(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs);

// Exact constructible-family specialization of 1090:042f -> 11f0:0211. Each
// simulation pass decrements byte 17 for every queued tenant until the first
// zero activates the queue head through 11f0:00a0. Type 7 includes the
// common record reset, day-phase-dependent status, lookup-key reassignment,
// and its exact Office people records. Every ordinary and composite family
// accepted by the construction dispatcher is supported; `unsupported_type`
// is reserved for a malformed or unknown type imported into a pending slot.
[[nodiscard]] OriginalPendingStepStatus step_original_pending_construction(
    OriginalTdtDocument& document);

// Exact 11f0:0016 queue flush used by several day-start scheduler families.
// It captures the entry count and activates that many queue heads through the
// already translated 11f0:00a0 family dispatch.
void activate_all_original_pending_facilities_for_schedule(
    OriginalTdtDocument& document);

// Complete non-visual reset chain rooted at 10b0:0000 for New/Open: flush
// 11f0 pending construction, run the 0072 tenant reset, clear 1198 parking
// counters and 11a8 commercial tables, reset all 1090 Elevator and 10c0
// Stair/Escalator transients, then run the 031a people reset.
void reset_original_loaded_simulation_state(OriginalTdtDocument& document);

// Exact 1198:01ab parking day-start rebuild: complete deferred construction,
// clear and repopulate b958, discard orphaned cf9c records, and refresh the
// persisted parking count/index state.
void refresh_original_parking_for_day(OriginalTdtDocument& document);

// Exact 1198:07e6 connectivity rebuild used after deleting a Parking segment
// or ramp. Unlike the day-start entry above, this also reconstructs the ramp
// chain/status bytes before rebuilding b958.
void rebuild_original_parking_after_facility_change(
    OriginalTdtDocument& document);

// Exact 1170:011f Medical Center day-start rebuild: complete deferred
// construction, clear bd5a and the process-local seven-group route index,
// discard orphaned dbfc records, reset their byte-2 timers, and repopulate
// both index tables. At ratings three and above it also raises DS:b92d.
void refresh_original_medical_for_day(OriginalTdtDocument& document);

// Exact scheduler calls 1040:0000 and 1040:0179 for the Cathedral family.
// The first completes the current deferred queue and resets all forty
// Cathedral people to state 0x20; the second clears facility frames and
// advances ceremony participants from state 3 to state 5.
[[nodiscard]] std::size_t reset_original_cathedral_for_day(
    OriginalTdtDocument& document);
[[nodiscard]] std::size_t close_original_cathedral_for_day(
    OriginalTdtDocument& document);

}  // namespace simtower
