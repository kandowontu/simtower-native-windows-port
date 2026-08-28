#pragma once

#include "original_tdt.hpp"
#include "original_time.hpp"
#include "original_world.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace simtower {

enum class OriginalFindMode : std::uint8_t {
  tenant,
  person,
};

struct OriginalFindLauncherContract {
  std::uint16_t dialog_resource_id{};
  std::int16_t dialog_parameter{};
  bool main_window_owner{};
  bool preserves_dialog_result{};

  friend bool operator==(const OriginalFindLauncherContract&,
                         const OriginalFindLauncherContract&) = default;
};

// Exact 10d8:0000 selector and DialogBoxParam arguments. Nonzero selects the
// Person resource 510 and is forwarded as one; zero selects Tenant resource
// 520. The wrapper discards DialogBox's return after freeing its proc instance.
[[nodiscard]] constexpr OriginalFindLauncherContract
original_find_launcher_contract(OriginalFindMode mode) noexcept {
  return mode == OriginalFindMode::person
             ? OriginalFindLauncherContract{510U, 1, true, false}
             : OriginalFindLauncherContract{520U, 0, true, false};
}

struct OriginalFindSelectionQuery {
  std::uint16_t control_id{};
  std::uint16_t win16_message{};
  std::uint16_t wparam{};
  std::uint32_t lparam{};

  friend bool operator==(const OriginalFindSelectionQuery&,
                         const OriginalFindSelectionQuery&) = default;
};

// Exact 10d8:0487 Find-list query. Win16 message 0x0420 is LB_GETCURSEL;
// production substitutes the native constant while retaining item and params.
[[nodiscard]] constexpr OriginalFindSelectionQuery
original_find_selection_query() noexcept {
  return {5U, 0x0420U, 0U, 0U};
}

enum class OriginalFindPresentationPhase : std::uint8_t {
  initialization,
  paint,
};

struct OriginalFindPresentationPlan {
  bool realize_palette{};
  bool render_positive_dtmp{};
  bool draw_generic_chrome{};
  std::uint8_t font_pixels{};

  friend bool operator==(const OriginalFindPresentationPlan&,
                         const OriginalFindPresentationPlan&) = default;
};

struct OriginalFindInitializationFocusPlan {
  bool focus_list{};
  bool handled_result{};

  friend bool operator==(const OriginalFindInitializationFocusPlan&,
                         const OriginalFindInitializationFocusPlan&) = default;
};

// FINDDIALOGFILTER 10d8:006f takes its WM_INITDIALOG branch at 00a1-014b,
// initializes and paints the list, then returns TRUE without calling SetFocus.
// The Win16 dialog manager therefore chooses the initial control. Do not add a
// native convenience that forces list item 5 and returns FALSE.
[[nodiscard]] constexpr OriginalFindInitializationFocusPlan
original_find_initialization_focus_plan() noexcept {
  return {false, true};
}

enum class OriginalFindDialogCommandAction : std::uint8_t {
  none,
  close,
  remove,
  resolve,
  enable_actions,
  disable_actions,
};

struct OriginalFindDialogCommandPlan {
  OriginalFindDialogCommandAction action{
      OriginalFindDialogCommandAction::none};
  bool consume{};

  friend bool operator==(const OriginalFindDialogCommandPlan&,
                         const OriginalFindDialogCommandPlan&) = default;
};

// Complete FINDDIALOGFILTER 10d8:017b-031c WM_COMMAND routing. IDs 3 and 4
// ignore notification codes. List item 5 maps notifications 1/2/3 to
// selection/change, double-click resolution, and selection-cancel; every other
// item-5 notification is still consumed. ID 1 closes, while absent item 2 and
// unknown control IDs fall through FALSE.
[[nodiscard]] constexpr OriginalFindDialogCommandPlan
original_find_dialog_command_plan(std::uint16_t control,
                                  std::uint16_t notification) noexcept {
  using Action = OriginalFindDialogCommandAction;
  if (control == 1U) return {Action::close, true};
  if (control == 3U) return {Action::remove, true};
  if (control == 4U) return {Action::resolve, true};
  if (control == 5U) {
    if (notification == 1U) return {Action::enable_actions, true};
    if (notification == 2U) return {Action::resolve, true};
    if (notification == 3U) return {Action::disable_actions, true};
    return {Action::none, true};
  }
  return {Action::none, false};
}

// FINDDIALOGFILTER 10d8:00ec-0146 and 0323-0360 both pass the positive
// DTMP/510-or-520 ID to 1070:0231. That path draws BITMAP/510 and replays the
// control rectangles but does not enter 1068:0567's generic bevel painter.
// Only the immediate initialization phase selects the 12-pixel font.
[[nodiscard]] constexpr OriginalFindPresentationPlan
original_find_presentation_plan(OriginalFindPresentationPhase phase) noexcept {
  return {
      .realize_palette = true,
      .render_positive_dtmp = true,
      .draw_generic_chrome = false,
      .font_pixels = static_cast<std::uint8_t>(
          phase == OriginalFindPresentationPhase::initialization ? 12U : 0U),
  };
}

enum class OriginalFindFocusRefreshStep : std::uint8_t {
  command_repaint,
  map_repaint,
  restore_preview_scratch,
  camera_transform,
};

// Exact 10e0:0cea post-dialog order after construction is disabled and edit
// mode two is selected. Command and Map are presented first, 11f8:3b94 then
// restores any pending construction-preview scratch, and only afterward does
// 1080:0000 run the focused camera transform (including its own full refresh).
[[nodiscard]] constexpr std::array<OriginalFindFocusRefreshStep, 4>
original_find_focus_refresh_order() noexcept {
  return {
      OriginalFindFocusRefreshStep::command_repaint,
      OriginalFindFocusRefreshStep::map_repaint,
      OriginalFindFocusRefreshStep::restore_preview_scratch,
      OriginalFindFocusRefreshStep::camera_transform,
  };
}

struct OriginalFindEntry {
  std::uint32_t link{};
  std::string name{};

  friend bool operator==(const OriginalFindEntry&,
                         const OriginalFindEntry&) = default;
};

enum class OriginalFindResolutionKind : std::uint8_t {
  invalid,
  focus,
  not_in_tower_alert,
  lobby_alert,
};

struct OriginalFindResolution {
  OriginalFindResolutionKind kind{OriginalFindResolutionKind::invalid};
  std::int16_t floor{-1};
  std::int16_t x{-1};
  OriginalWorldPoint view{};

  [[nodiscard]] constexpr bool focused() const noexcept {
    return kind == OriginalFindResolutionKind::focus;
  }

  [[nodiscard]] constexpr bool alerts() const noexcept {
    return kind == OriginalFindResolutionKind::not_in_tower_alert ||
           kind == OriginalFindResolutionKind::lobby_alert;
  }

  friend bool operator==(const OriginalFindResolution&,
                         const OriginalFindResolution&) = default;
};

// Process-local target retained by FINDDIALOGFILTER at DS:77b4..77c0.
// 10d8:02ce records the selected person (or FFFFFFFF for a tenant) and the
// current 1208:05e6 coarse tick after a successful resolution; 10e0:051d
// expires it only after the first signed-magnitude delta above 300 units
// (nominally 4.8 seconds, because each unit is 16 milliseconds).
struct OriginalFindMarkerState {
  std::int16_t cell_x{-1};
  std::int16_t floor{-1};
  std::uint32_t selected_person{std::numeric_limits<std::uint32_t>::max()};
  std::uint32_t started_tick{};
  std::uint16_t phase{};

  [[nodiscard]] constexpr bool active() const noexcept {
    return cell_x != -1 && floor != -1;
  }
};

// Exact post-dialog predicate in 1100:0000. After Person Information closes,
// 10e0:0cc9 must report an active coordinate and the selected Find-person
// dword at DS:77b8 must equal the dialog's person before DS:77c0 is set.
[[nodiscard]] constexpr bool
original_person_information_sets_find_exit_latch(
    const OriginalFindMarkerState& state,
    std::size_t person_index) noexcept {
  return state.active() &&
         state.selected_person == static_cast<std::uint32_t>(person_index);
}

void start_original_find_marker(
    OriginalFindMarkerState& state,
    const OriginalFindResolution& resolution,
    std::uint32_t selected_person,
    std::uint32_t now_tick) noexcept;

void reset_original_find_marker(OriginalFindMarkerState& state) noexcept;

[[nodiscard]] bool expire_original_find_marker(
    OriginalFindMarkerState& state,
    std::uint32_t now_tick) noexcept;

// 1188:05a7/05e3 expose the NUL-terminated ANSI prefix of each fixed
// sixteen-byte saved name record.
[[nodiscard]] std::string original_find_name_text(
    const OriginalTdtLinkName& name);

// Exact 10e0:06cd floor text for ALRT/1003. Stored floor 10 is Floor 1;
// stored floor 9 is B1 and each lower stored floor advances the basement.
[[nodiscard]] std::string original_find_floor_text(std::int16_t floor);

// Exact 10d8:038e list order: active dce4 people or active dd34 tenants,
// paired with the corresponding process/saved name table at the same index.
[[nodiscard]] std::vector<OriginalFindEntry> original_find_entries(
    const OriginalTdtDocument& document,
    OriginalFindMode mode);

// Exact 10d8:0438 -> 1188:0793/0884 removal and compaction transaction.
[[nodiscard]] bool remove_original_find_entry(
    OriginalTdtDocument& document,
    OriginalFindMode mode,
    std::size_t selected_index) noexcept;

// Exact 10e0:0000/078d tenant selection and the 1080:0000 viewport transform.
[[nodiscard]] OriginalFindResolution resolve_original_find_tenant(
    const OriginalTdtDocument& document,
    std::uint16_t tenant_link,
    int client_width,
    int client_height) noexcept;

// Static translation of 10e0:0042/0814 and its facility, retail, medical,
// 10e0:09ce/0aa0/0ad8 Stair/Escalator, Elevator-car, and Elevator-waiting
// focus paths plus the exact 10e0:0669/06cd ALRT/1002 and ALRT/1003
// fallbacks. Alert resolutions do not synthesize a viewport target.
[[nodiscard]] OriginalFindResolution resolve_original_find_person(
    const OriginalTdtDocument& document,
    std::uint32_t person_index,
    int client_width,
    int client_height) noexcept;

}  // namespace simtower
