#pragma once

#include "original_construction.hpp"
#include "original_dtmp.hpp"
#include "original_resources.hpp"
#include "original_tables.hpp"
#include "original_tdt.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace simtower {

inline constexpr std::uint16_t kOriginalElevatorControlDialogId = 400U;
inline constexpr int kOriginalElevatorControlClientWidth = 200;
inline constexpr int kOriginalElevatorControlClientHeight = 428;
inline constexpr int kOriginalElevatorControlVisibleFloors = 15;
inline constexpr int kOriginalElevatorControlCellSize = 13;
inline constexpr int kOriginalElevatorControlPopupWidth = 82;
inline constexpr int kOriginalElevatorControlPopupHeight = 67;
inline constexpr std::uint16_t kOriginalElevatorControlCarBitmapBase =
    0x4f20U;  // 11e0:0430 adds 0x4e20 to selectors 0x100..0x109.

struct OriginalElevatorControlLaunchContract {
  std::size_t elevator_index{};
  std::int16_t initial_pointer_x{};
  std::int16_t initial_pointer_y{};
  std::uint16_t dialog_resource_id{};
  std::int16_t requested_left{};
  bool ownerless{};

  friend bool operator==(const OriginalElevatorControlLaunchContract&,
                         const OriginalElevatorControlLaunchContract&) = default;
};

// Exact 1098:0000 parameter block and modeless launch constants. The caller's
// packed Main-client point is copied beside the elevator index and becomes the
// initial ELVDLOGMAIN pointer state; initialization later positions the dialog
// through 11e0:0b52 with the literal requested-left value eight.
[[nodiscard]] constexpr OriginalElevatorControlLaunchContract
original_elevator_control_launch_contract(std::size_t elevator_index,
                                          std::int16_t pointer_x,
                                          std::int16_t pointer_y) noexcept {
  return {elevator_index, pointer_x, pointer_y,
          kOriginalElevatorControlDialogId, 8, true};
}

// Exact five-entry ELVPOPUP parallel message table at 1098:230d/27a5.
[[nodiscard]] constexpr bool original_elevator_popup_handles_message(
    std::uint16_t message) noexcept {
  switch (message) {
    case 0x000f:  // WM_PAINT
    case 0x0110:  // WM_INITDIALOG
    case 0x0200:  // WM_MOUSEMOVE
    case 0x0202:  // WM_LBUTTONUP
    case 0x0205:  // WM_RBUTTONUP
      return true;
    default:
      return false;
  }
}

// Exact eight-entry ELVDLOGMAIN parallel message table at 1098:066c/12c9.
[[nodiscard]] constexpr bool original_elevator_control_handles_message(
    std::uint16_t message) noexcept {
  switch (message) {
    case 0x0006:  // WM_ACTIVATE
    case 0x000f:  // WM_PAINT
    case 0x0019:  // Win16 WM_CTLCOLOR
    case 0x0110:  // WM_INITDIALOG
    case 0x0115:  // WM_VSCROLL
    case 0x0200:  // WM_MOUSEMOVE
    case 0x0201:  // WM_LBUTTONDOWN
    case 0x0202:  // WM_LBUTTONUP
      return true;
    default:
      return false;
  }
}

enum class OriginalElevatorControlScrollCommand : std::uint8_t {
  line_up,
  line_down,
  page_up,
  page_down,
  thumb_position,
  thumb_track,
};

enum class OriginalElevatorControlGridKind : std::uint8_t {
  none,
  service_floor,
  car,
};

enum class OriginalElevatorControlActivationInsertAfter : std::uint8_t {
  topmost,
  first_child,
  main,
};

enum class OriginalElevatorControlStockBrush : std::uint8_t {
  white = 0,
  null = 5,
};

enum class OriginalElevatorControlCloseWindow : std::uint8_t {
  command,
  map,
  info,
  main,
};

struct OriginalElevatorControlClosePlan {
  bool resume_isolation{};
  bool clear_published_window_before_destroy{};
  std::array<OriginalElevatorControlCloseWindow, 4> enable_order{};
  bool explicitly_activate_main{};

  friend bool operator==(const OriginalElevatorControlClosePlan&,
                         const OriginalElevatorControlClosePlan&) = default;
};

// Exact ELVDLOGMAIN item-one close tail at 1098:0ece-0f63. The original
// performs all state and owner-window restoration before DestroyWindow; its
// eight-message table contains neither WM_DESTROY nor WM_NCDESTROY.
[[nodiscard]] constexpr OriginalElevatorControlClosePlan
original_elevator_control_close_plan() noexcept {
  using Window = OriginalElevatorControlCloseWindow;
  return {
      true,
      true,
      {Window::map, Window::command, Window::info, Window::main},
      false,
  };
}

// ELVDLOGMAIN's Win16 WM_CTLCOLOR branch at 1098:091e-0936 returns
// WHITE_BRUSH for CTLCOLOR_SCROLLBAR (type five) and NULL_BRUSH for every
// other control type. Win32 splits those types into separate messages, so the
// native boundary passes whether the message is WM_CTLCOLORSCROLLBAR.
[[nodiscard]] constexpr OriginalElevatorControlStockBrush
original_elevator_control_stock_brush(bool scrollbar) noexcept {
  return scrollbar ? OriginalElevatorControlStockBrush::white
                   : OriginalElevatorControlStockBrush::null;
}

struct OriginalElevatorControlActivationPlan {
  bool shared_activation_latch{};
  OriginalElevatorControlActivationInsertAfter insert_after{
      OriginalElevatorControlActivationInsertAfter::main};
  bool no_activate{};

  friend bool operator==(const OriginalElevatorControlActivationPlan&,
                         const OriginalElevatorControlActivationPlan&) =
      default;
};

// Exact ELVDLOGMAIN WM_ACTIVATE branch at 1098:08c9-0916. It writes the same
// DS:31a6 latch as MAINWNDPROC. Active promotes the modeless control TOPMOST
// and permits activation; inactive inserts it after GetTopWindow(control), or
// after main when no such window exists, with the literal NOACTIVATE flags.
[[nodiscard]] constexpr OriginalElevatorControlActivationPlan
original_elevator_control_activation_plan(bool active,
                                          bool first_child_exists) noexcept {
  if (active) {
    return {
        true,
        OriginalElevatorControlActivationInsertAfter::topmost,
        false,
    };
  }
  return {
      false,
      first_child_exists
          ? OriginalElevatorControlActivationInsertAfter::first_child
          : OriginalElevatorControlActivationInsertAfter::main,
      true,
  };
}

struct OriginalElevatorControlGridHit {
  OriginalElevatorControlGridKind kind{
      OriginalElevatorControlGridKind::none};
  std::int16_t floor{-1};
  std::int16_t row{-1};
  std::int16_t visual_column{-1};
  std::int16_t car_index{-1};

  [[nodiscard]] constexpr bool hit() const noexcept {
    return kind != OriginalElevatorControlGridKind::none;
  }

  friend bool operator==(const OriginalElevatorControlGridHit&,
                         const OriginalElevatorControlGridHit&) = default;
};

// Process-local state held by ELVDLOGMAIN at DS:b3a6..b3ae and by the
// 10f0 Simulate/Resume helper. None of this state belongs in the TDT stream.
struct OriginalElevatorControlState {
  bool valid{};
  std::size_t elevator_index{static_cast<std::size_t>(-1)};
  std::uint8_t schedule_bank{};   // DS:b3a6, WD=0 / WE=1
  std::uint8_t day_phase{};       // DS:b3a7, visible phases 0..5
  std::int16_t scroll_min{};
  std::int16_t scroll_max{};
  std::int16_t scroll_position{};
  bool isolation_active{};        // DS:b3ae
  bool saved_build_mode{};        // DS:783e argument passed to 10f0:009c
  std::array<std::uint8_t, 24> saved_elevator_used{};  // DS:b3b4
  // The original allocates exactly 0x345a bytes and copies the selected
  // in-memory shaft record after applying the one-shaft used mask.
  std::optional<OriginalTdtElevator> saved_elevator_record{};
};

struct OriginalElevatorControlPreviewResult {
  bool prepared{};
  std::size_t frames{};
  std::size_t cars_changed{};
  std::size_t movement_sound_requests{};
};

// Exact WM_INITDIALOG selectors and scrollbar range at 1098:0780-083f.
[[nodiscard]] OriginalElevatorControlState
make_original_elevator_control_state(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::uint8_t calendar_phase,
    std::int8_t day_phase) noexcept;

// STRL/400[type+1], assigned after DIALOG/400 has been created.
[[nodiscard]] std::string original_elevator_control_title(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    std::size_t elevator_index);

[[nodiscard]] constexpr std::size_t
original_elevator_control_schedule_index(std::size_t base,
                                         std::uint8_t bank,
                                         std::uint8_t phase) noexcept {
  return base + static_cast<std::size_t>(bank) * 7U + phase;
}

// 1098:27bd/2893 load the persisted schedule bytes through MOV AL followed
// by CBW, so both displayed values are signed even though valid UI edits keep
// them in positive ranges.
[[nodiscard]] std::int16_t original_elevator_control_waiting_value(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state) noexcept;
[[nodiscard]] std::int16_t original_elevator_control_departure_units(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state) noexcept;
[[nodiscard]] std::uint8_t original_elevator_control_floor_mode(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state) noexcept;
[[nodiscard]] std::string original_elevator_control_waiting_text(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state);
[[nodiscard]] std::string original_elevator_control_departure_text(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state);

[[nodiscard]] bool original_elevator_control_select_bank(
    OriginalElevatorControlState& state,
    std::uint8_t bank) noexcept;
[[nodiscard]] bool original_elevator_control_select_phase(
    OriginalElevatorControlState& state,
    std::uint8_t phase) noexcept;
[[nodiscard]] bool original_elevator_control_adjust_waiting(
    OriginalTdtDocument& document,
    const OriginalElevatorControlState& state,
    int delta) noexcept;
[[nodiscard]] bool original_elevator_control_adjust_departure(
    OriginalTdtDocument& document,
    const OriginalElevatorControlState& state,
    int delta) noexcept;
[[nodiscard]] bool original_elevator_control_set_floor_mode(
    OriginalTdtDocument& document,
    const OriginalElevatorControlState& state,
    std::uint8_t mode) noexcept;

// Exact 1098:15c6 button selection: BITMAP/407 is the nonzero SHOW state;
// BITMAP/408 is zero.
[[nodiscard]] std::uint16_t original_elevator_control_show_bitmap(
    const OriginalTdtElevator& elevator) noexcept;
[[nodiscard]] bool original_elevator_control_toggle_show(
    OriginalTdtDocument& document,
    std::size_t elevator_index) noexcept;

[[nodiscard]] bool original_elevator_control_has_scrollbar(
    const OriginalElevatorControlState& state) noexcept;
[[nodiscard]] bool original_elevator_control_scroll(
    OriginalElevatorControlState& state,
    OriginalElevatorControlScrollCommand command,
    std::int16_t thumb_position = 0) noexcept;
[[nodiscard]] std::int16_t original_elevator_control_visible_lowest_floor(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state) noexcept;
[[nodiscard]] std::int16_t original_elevator_control_visible_floor(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state,
    std::int16_t row) noexcept;

// Exact 1098:1644 cell rectangle in DIALOG/400 client coordinates. Column
// -1 is the service-floor label; zero and above are contiguous active-car
// visual columns. Row zero is the lowest displayed floor.
[[nodiscard]] OriginalDtmpRect original_elevator_control_cell_rect(
    std::int16_t visual_column,
    std::int16_t row) noexcept;

// Exact 1098:1e33 current-car outline rectangle after the control's inverted
// scrollbar transform. The original paints this rectangle black when a car
// is current and white before moving it; an empty result means the car's
// current floor is outside the fifteen visible rows.
[[nodiscard]] std::optional<OriginalDtmpRect>
original_elevator_control_current_car_frame(
    const OriginalTdtDocument& document,
    const OriginalElevatorControlState& state,
    std::size_t car_index,
    std::int16_t visual_column) noexcept;

[[nodiscard]] OriginalElevatorControlGridHit
original_elevator_control_grid_hit(
    const OriginalTdtDocument& document,
    const OriginalElevatorControlState& state,
    int x,
    int y) noexcept;
[[nodiscard]] std::string original_elevator_control_floor_label(
    std::int16_t floor);

struct OriginalElevatorControlFloorCellPlan {
  std::int16_t floor{};
  bool above_top{};
  bool serviced{};
  std::string label{};
  bool small_font{};
  std::int16_t horizontal_inset{};
  std::int16_t vertical_inset{};

  friend bool operator==(const OriginalElevatorControlFloorCellPlan&,
                         const OriginalElevatorControlFloorCellPlan&) =
      default;
};

// Exact visible decisions of 1098:1895's service-floor cell painter. Floors
// above the shaft are gray and unlabeled; remaining rows invert fill/text for
// serviced floors, skip displayed zero, and switch to the inset 9-point font
// at displayed floor 100.
[[nodiscard]] OriginalElevatorControlFloorCellPlan
original_elevator_control_floor_cell_plan(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state,
    std::int16_t row);

// Exact 1098:1a5b selector for an active car and floor, translated through
// 11e0:0430's +0x4e20 resource bias to BITMAP/20256..20265. Zero means no
// icon should be painted.
[[nodiscard]] std::uint16_t original_elevator_control_car_bitmap(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index,
    std::int16_t floor) noexcept;

// The embedded control calls 10a0:0085 without Finger's word_3c visibility
// gate. These helpers retain all remaining shaft/home/new-stop checks.
[[nodiscard]] OriginalElevatorServiceFloorGate
original_elevator_control_service_floor_gate(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept;
[[nodiscard]] bool original_elevator_control_add_service_floor(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept;
[[nodiscard]] OriginalNativeElevatorFloorPeopleCleanupResult
original_elevator_control_remove_service_floor(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{}) noexcept;
[[nodiscard]] bool original_elevator_control_set_car_home(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index,
    std::int16_t floor) noexcept;

// ELVPOPUP accepts points on the inclusive client bounds and divides y into
// Local / Express To Top / Express To Bottom with (y*3)/(height+1).
[[nodiscard]] std::optional<std::uint8_t>
original_elevator_control_popup_selection(int x,
                                          int y,
                                          int width,
                                          int height) noexcept;

// Exact 1098:26f2-2773 transparent highlight blit. Unlike the hit bands,
// which divide by height+1, every visual band is height/3 pixels tall and
// starts at mode*(height/3); the 67-pixel original therefore leaves row 66
// as the neutral base image.
[[nodiscard]] std::optional<OriginalDtmpRect>
original_elevator_control_popup_highlight(std::uint8_t mode,
                                          int width,
                                          int height) noexcept;

// Exact 10f0:0000/009c snapshot boundary: save all 24 used bytes and the
// selected 0x345a shaft record, leave only that shaft active, and force Build
// off. Resume carries the four 14-byte schedule arrays and word_3c (SHOW)
// into the saved record before restoring it, then restores all used bytes and
// the previous Build state.
[[nodiscard]] bool begin_original_elevator_control_isolation(
    OriginalTdtDocument& document,
    OriginalElevatorControlState& state,
    bool& build_mode) noexcept;

// Exact signed loop bound at 10f0:03d9. DS:dd78 is selected by rating from
// PART offsets 0x10/0x12/0x14, and the initializer advances twice that many
// isolated Elevator frames.
[[nodiscard]] std::size_t original_elevator_control_preview_frame_count(
    const OriginalPartTable& part,
    std::uint16_t rating) noexcept;

// Exact post-snapshot 10f0:0318 preview initializer. It advances only the
// selected shaft with b3ae semantics, restores the clock, both 120-byte owner
// maps, all floor-ring cursors/counts, and every car byte except its active
// byte. Queue dwords intentionally retain the original projected selectors
// until Resume restores the full saved shaft.
[[nodiscard]] OriginalElevatorControlPreviewResult
prepare_original_elevator_control_preview(
    OriginalTdtDocument& document,
    OriginalElevatorControlState& state,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{});

[[nodiscard]] bool resume_original_elevator_control_isolation(
    OriginalTdtDocument& document,
    OriginalElevatorControlState& state,
    bool& build_mode) noexcept;

}  // namespace simtower
