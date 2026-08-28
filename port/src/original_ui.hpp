#pragma once

#include "original_palette_frame.hpp"
#include "original_resources.hpp"

#include <windows.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace simtower {

// Exact null-terminated DS:3016 cursor bank populated by 11e0:0cfb and
// destroyed by 11e0:0d44. Native fixed ownership omits only the sentinel.
inline constexpr std::array<std::uint16_t, 4>
    kOriginalCustomCursorResourceIds{1002U, 1003U, 1004U, 1005U};

enum class OriginalWindowClassProcedure : std::uint8_t {
  main,
  map,
  info,
  command,
};

enum class OriginalWindowClassIcon : std::uint8_t {
  none,
  application_resource,
  system_application,
};

enum class OriginalWindowClassCursor : std::uint8_t {
  none,
  arrow,
};

enum class OriginalWindowClassMenu : std::uint8_t {
  none,
  tower_menu,
  empty_string,
};

struct OriginalWindowClassSpec {
  std::uint16_t style{};
  std::uint16_t window_extra_bytes{};
  OriginalWindowClassProcedure procedure{};
  OriginalWindowClassIcon icon{};
  OriginalWindowClassCursor cursor{};
  OriginalWindowClassMenu menu{};
  std::wstring_view class_name{};

  friend bool operator==(const OriginalWindowClassSpec&,
                         const OriginalWindowClassSpec&) = default;
};

// Complete 1258:0345 registration table in original call order. All four
// classes use WHITE_BRUSH, and Main's 1258:0354 style is CS_DBLCLKS. Main's
// TOWER_MENU is supplied explicitly by the native resource bridge when its
// window is created, but remains part of the registered class metadata here;
// Command preserves the non-null empty menu.
[[nodiscard]] constexpr std::array<OriginalWindowClassSpec, 4>
original_window_class_specs() noexcept {
  return {{
      {0x0008U, 0U, OriginalWindowClassProcedure::main,
       OriginalWindowClassIcon::application_resource,
       OriginalWindowClassCursor::none,
       OriginalWindowClassMenu::tower_menu, L"Tower_MainWClass"},
      {0U, 4U, OriginalWindowClassProcedure::map,
       OriginalWindowClassIcon::none, OriginalWindowClassCursor::arrow,
       OriginalWindowClassMenu::none, L"Tower_MapWClass"},
      {0U, 4U, OriginalWindowClassProcedure::info,
       OriginalWindowClassIcon::none, OriginalWindowClassCursor::arrow,
       OriginalWindowClassMenu::none, L"Tower_InfoWClass"},
      {0U, 4U, OriginalWindowClassProcedure::command,
       OriginalWindowClassIcon::system_application,
       OriginalWindowClassCursor::arrow,
       OriginalWindowClassMenu::empty_string, L"CmdBtnWClass"},
  }};
}

enum class OriginalMessageLoopTarget : std::uint8_t {
  none,
  main_window,
  elevator_control,
  active_modal,
};

struct OriginalMessageLoopRoutePlan {
  OriginalMessageLoopTarget target{OriginalMessageLoopTarget::none};
  bool try_accelerator{};
  bool try_dialog_navigation{};
  bool translate_and_dispatch_if_unhandled{};

  friend bool operator==(const OriginalMessageLoopRoutePlan&,
                         const OriginalMessageLoopRoutePlan&) = default;
};

enum class OriginalRenameDialogFocusPhase : std::uint8_t {
  initialize,
  after_paint,
};

struct OriginalRenameDialogFocusPlan {
  bool focus_edit{};
  bool select_all{};
  bool handled_result{};

  friend bool operator==(const OriginalRenameDialogFocusPlan&,
                         const OriginalRenameDialogFocusPlan&) = default;
};

// NAMEPEPLEDIALOGFILTER 1100:3a69-3b8f/3c60-3c7f and
// NAMETENANTDIALOGFILTER 1100:3df4-3f06/3fd7-3ff6 both return TRUE from
// WM_INITDIALOG without setting focus or changing the edit selection. Their
// WM_PAINT tails later enable and focus edit item 4, still without selecting
// its existing text. This deliberately differs from a native select-all
// convenience commonly added to rename dialogs.
[[nodiscard]] constexpr OriginalRenameDialogFocusPlan
original_rename_dialog_focus_plan(
    OriginalRenameDialogFocusPhase phase) noexcept {
  if (phase == OriginalRenameDialogFocusPhase::after_paint) {
    return {true, false, true};
  }
  return {false, false, true};
}

enum class OriginalRenameDialogCommandAction : std::uint8_t {
  none,
  save,
  cancel,
  remove,
  refresh_edit_gate,
};

struct OriginalRenameDialogCommandPlan {
  OriginalRenameDialogCommandAction action{
      OriginalRenameDialogCommandAction::none};
  bool consume{};
  std::int16_t close_result{};

  friend bool operator==(const OriginalRenameDialogCommandPlan&,
                         const OriginalRenameDialogCommandPlan&) = default;
};

// NAMEPEPLEDIALOGFILTER 1100:3c82-3d2d and NAMETENANTDIALOGFILTER
// 1100:3ff9-40a7 index only the Win16 control ID. Notifications are ignored.
// Save, Cancel, and Remove all share the same cleanup and EndDialog(1) tail;
// edit item 4 refreshes the nonempty OK-button gate without closing.
[[nodiscard]] constexpr OriginalRenameDialogCommandPlan
original_rename_dialog_command_plan(std::uint16_t control) noexcept {
  using Action = OriginalRenameDialogCommandAction;
  if (control == 1U) return {Action::save, true, 1};
  if (control == 2U) return {Action::cancel, true, 1};
  if (control == 3U) return {Action::remove, true, 1};
  if (control == 4U) return {Action::refresh_edit_gate, true, 0};
  return {Action::none, false, 0};
}

// Exact 1258:00bc-015c target precedence for a fetched message. DS:31a0's
// modeless Elevator Control outranks DS:31a4's active modal; either receives
// TranslateAccelerator before IsDialogMessage. Main receives only the
// accelerator attempt, and a missing target causes the message to be dropped.
[[nodiscard]] constexpr OriginalMessageLoopRoutePlan
original_message_loop_route_plan(bool main_window_exists,
                                 bool elevator_control_exists,
                                 bool active_modal_exists) noexcept {
  if (elevator_control_exists) {
    return {OriginalMessageLoopTarget::elevator_control, true, true, true};
  }
  if (active_modal_exists) {
    return {OriginalMessageLoopTarget::active_modal, true, true, true};
  }
  if (main_window_exists) {
    return {OriginalMessageLoopTarget::main_window, true, false, true};
  }
  return {};
}

enum class OriginalMainShutdownMessage : std::uint8_t {
  close,
  query_end_session,
  exit_command,
  destroy,
};

struct OriginalMainShutdownPlan {
  bool request_confirmation{};
  bool set_closing_latch{};
  bool stop_audio_channels{};
  bool destroy_main_window{};
  bool clear_main_window_handle{};
  bool post_quit_message{};
  std::intptr_t result{};

  friend bool operator==(const OriginalMainShutdownPlan&,
                         const OriginalMainShutdownPlan&) = default;
};

// Exact MAINWNDPROC 1158:049e-04fb and 1158:071a shutdown split. WM_CLOSE
// confirms action 4, sets DS:31c6, stops both mixer channels through
// 11c8:0135(1), destroys Main, and clears DS:3258. WM_QUERYENDSESSION delegates
// the same confirmation/latch/stop sequence to 10d0:0604 but returns TRUE only
// when that call changes the latch from zero to one. File/Exit uses that same
// callee and destroys Main when the latch is set, but deliberately omits the
// WM_CLOSE branch's DS:3258 clear. WM_DESTROY itself only posts WM_QUIT.
[[nodiscard]] constexpr OriginalMainShutdownPlan original_main_shutdown_plan(
    OriginalMainShutdownMessage message,
    bool already_closing,
    bool confirmation_accepted = false) noexcept {
  if (message == OriginalMainShutdownMessage::destroy) {
    return {false, false, false, false, false, true, 0};
  }
  if (already_closing) {
    if (message == OriginalMainShutdownMessage::exit_command) {
      return {false, false, false, true, false, false, 0};
    }
    return {};
  }

  if (!confirmation_accepted) {
    return {true, false, false, false, false, false, 0};
  }
  if (message == OriginalMainShutdownMessage::close) {
    return {true, true, true, true, true, false, 0};
  }
  if (message == OriginalMainShutdownMessage::exit_command) {
    return {true, true, true, true, false, false, 0};
  }
  return {true, true, true, false, false, false, 1};
}

// Native equivalents of the original Win16 MENU/ACCELERATOR/GROUP_ICON
// resource consumers. The input remains the exact bytes embedded in the NE.
[[nodiscard]] HMENU create_original_menu(std::span<const std::byte> resource);
[[nodiscard]] HACCEL create_original_accelerators(
    std::span<const std::byte> resource);
[[nodiscard]] HICON create_original_icon(const OriginalResources& resources,
                                          std::string_view group_name);

// Exact 1208:05a9 auxiliary-surface clearing wrapper. The Win16 routine
// creates a temporary COLORREF 0x00ffffff solid brush, fills the caller's
// complete rectangle, and deletes that brush before returning.
void fill_original_white_rect(HDC dc, const RECT& rectangle) noexcept;

// Exact custom-cursor resource path initialized by 11e0:0cfb. Win16
// GROUP_CURSOR entries use WORD width/height fields and point at one of the
// four CURSOR resources; the returned Win32 cursor owns the same image and
// hotspot and must be released with DestroyCursor.
[[nodiscard]] HCURSOR create_original_cursor(
    const OriginalResources& resources,
    std::uint16_t group_id);

// Exact main-client selector at 1258:0505 followed by the system/custom
// dispatch at 11e0:0d80. Values 0 and 1000 are the Win16 Arrow and vertical
// resize selectors; values 1002..1005 are the original Magnifier,
// construction, Finger, and Bulldozer cursor resources.
[[nodiscard]] constexpr std::uint16_t select_original_main_cursor(
    bool build_mode,
    std::uint16_t command_mode,
    bool mouse_capture_active) noexcept {
  if (build_mode) {
    if (command_mode == 0U) return 1005U;
    if (command_mode == 1U) {
      return mouse_capture_active ? 1000U : 1004U;
    }
    if (command_mode == 2U) return 1003U;
    return 1002U;
  }
  return command_mode == 2U ? 1003U : 0U;
}

struct OriginalMainCursorPointPlan {
  bool set_cursor{};
  std::uint16_t selector{};

  friend bool operator==(const OriginalMainCursorPointPlan&,
                         const OriginalMainCursorPointPlan&) = default;
};

// Complete 1258:0505 screen-point decision surrounding the edit selector.
// An enabled Map, Info, or Command palette forces Arrow before the main-client
// test. A point in a non-iconic main client applies the mode/capture selector;
// every other point deliberately leaves the current cursor unchanged.
[[nodiscard]] constexpr OriginalMainCursorPointPlan
original_main_cursor_point_plan(bool point_in_map_window,
                                bool map_window_enabled,
                                bool point_in_info_window,
                                bool info_window_enabled,
                                bool point_in_command_window,
                                bool command_window_enabled,
                                bool point_in_main_client,
                                bool main_window_iconic,
                                bool build_mode,
                                std::uint16_t command_mode,
                                bool mouse_capture_active) noexcept {
  if ((point_in_map_window && map_window_enabled) ||
      (point_in_info_window && info_window_enabled) ||
      (point_in_command_window && command_window_enabled)) {
    return {true, 0U};
  }
  if (!point_in_main_client || main_window_iconic) return {};
  return {true, select_original_main_cursor(
                    build_mode, command_mode, mouse_capture_active)};
}

struct OriginalMainMouseMovePlan {
  bool refresh_cursor{};
  bool forward_to_world_dispatch{};

  friend bool operator==(const OriginalMainMouseMovePlan&,
                         const OriginalMainMouseMovePlan&) = default;
};

// MAINWNDPROC 1158:0314 calls GetCursorPos/1258:0505 before consulting
// DS:24b8. A modal lock suppresses only the subsequent 1058:0000 dispatch.
[[nodiscard]] constexpr OriginalMainMouseMovePlan
original_main_mouse_move_plan(bool modal_input_locked) noexcept {
  return {true, !modal_input_locked};
}

enum class OriginalIdleAudioTransition : std::uint8_t {
  none,
  activate,
  deactivate,
};

enum class OriginalConstructionTogglePath : std::uint8_t {
  exit_map_overlay,
  enable_construction,
  disable_construction,
};

struct OriginalConstructionTogglePlan {
  OriginalConstructionTogglePath path{
      OriginalConstructionTogglePath::enable_construction};
  bool build_mode_after{};
  bool clear_map_mode{};
  bool force_command_mode_two{};
  bool reset_find_marker{};
  OriginalIdleAudioTransition audio{OriginalIdleAudioTransition::none};
  bool refresh_command_synchronously{};
  bool restore_preview_scratch{};
  bool present_main_synchronously{};

  friend bool operator==(const OriginalConstructionTogglePlan&,
                         const OriginalConstructionTogglePlan&) = default;
};

// Exact 1058:033c split. A disabled construction mode with a live Map overlay
// exits through 11d0:0000 and does not enter the ordinary audio branch.
// Otherwise the toggle deactivates WAVMIX while disabling or resets Find and
// activates WAVMIX while enabling, then refreshes Command and preview scratch.
[[nodiscard]] constexpr OriginalConstructionTogglePlan
original_construction_toggle_plan(bool build_mode_enabled,
                                  std::uint16_t map_mode) noexcept {
  if (!build_mode_enabled && map_mode != 0U) {
    return {OriginalConstructionTogglePath::exit_map_overlay,
            true, true, true, true,
            OriginalIdleAudioTransition::none,
            false, false, false};
  }
  if (build_mode_enabled) {
    return {OriginalConstructionTogglePath::disable_construction,
            false, false, true, false,
            OriginalIdleAudioTransition::deactivate,
            true, true, true};
  }
  return {OriginalConstructionTogglePath::enable_construction,
          true, false, false, true,
          OriginalIdleAudioTransition::activate,
          true, true, false};
}

// Exact 1258:0195-01c3 empty-queue reconciliation around DS:31a6 (the shared
// Main/Elevator-Control activation word), IsIconic, and DS:0252 (WAVMIX
// active latch).
// The host retries activation while the latch remains clear, matching
// 11c8:0aab's master/initialization gates, and deactivates only a set latch.
[[nodiscard]] constexpr OriginalIdleAudioTransition
original_idle_audio_transition(bool game_window_active,
                               bool main_window_iconic,
                               bool audio_active) noexcept {
  const bool should_be_active =
      game_window_active && !main_window_iconic;
  if (should_be_active && !audio_active) {
    return OriginalIdleAudioTransition::activate;
  }
  if (!should_be_active && audio_active) {
    return OriginalIdleAudioTransition::deactivate;
  }
  return OriginalIdleAudioTransition::none;
}

enum class OriginalIdleElevatorWindowOrder : std::uint8_t {
  none,
  promote_topmost_and_activate,
  insert_behind_main,
};

struct OriginalIdleWindowOrderPlan {
  bool promote_command_topmost{};
  OriginalIdleElevatorWindowOrder elevator{
      OriginalIdleElevatorWindowOrder::none};

  friend bool operator==(const OriginalIdleWindowOrderPlan&,
                         const OriginalIdleWindowOrderPlan&) = default;
};

// Exact 1258:01c3-023a empty-queue z-order maintenance. DS:325a/31a8 are the
// Command palette handle/enabled word: when main is active and no modeless
// elevator control exists, an inactive enabled Command is restored TOPMOST
// without activation. An existing elevator control suppresses that branch;
// it is made TOPMOST/active while main is active, or inserted immediately
// behind main without activation while main is inactive.
[[nodiscard]] constexpr OriginalIdleWindowOrderPlan
original_idle_window_order_plan(bool game_window_active,
                                bool elevator_window_exists,
                                bool active_window_is_command,
                                bool command_window_enabled,
                                bool active_window_is_elevator) noexcept {
  OriginalIdleWindowOrderPlan plan{};
  if (game_window_active && !elevator_window_exists &&
      !active_window_is_command && command_window_enabled) {
    plan.promote_command_topmost = true;
  }
  if (!elevator_window_exists) return plan;
  if (!game_window_active) {
    plan.elevator = OriginalIdleElevatorWindowOrder::insert_behind_main;
  } else if (!active_window_is_elevator) {
    plan.elevator =
        OriginalIdleElevatorWindowOrder::promote_topmost_and_activate;
  }
  return plan;
}

struct OriginalIdleWorldPassPlan {
  bool sample_cursor{};
  bool run_scheduler{};
  bool run_full_world_pass{};
  bool run_preview_only_pass{};

  friend bool operator==(const OriginalIdleWorldPassPlan&,
                         const OriginalIdleWorldPassPlan&) = default;
};

// Exact 1258:0244-02d1 gate and 1090:03ab argument choice. The empty-queue
// host runs neither scheduler nor world presentation while construction mode
// is disabled or DS:02a6's Elevator-Finger capture is active. Otherwise it
// samples the live cursor before scheduling; an advanced six-coarse-tick
// (~96-ms) interval requests the full frame and every non-advanced tick enters
// 1090:03ab(0).
// That callee—not this outer gate—compares the old and newly derived preview
// rectangles before deciding whether a changed rectangle must be presented.
[[nodiscard]] constexpr OriginalIdleWorldPassPlan original_idle_world_pass_plan(
    bool construction_mode_enabled,
    bool elevator_finger_capture_active,
    bool scheduler_advanced) noexcept {
  if (!construction_mode_enabled || elevator_finger_capture_active) return {};
  return {
      true,
      true,
      scheduler_advanced,
      !scheduler_advanced,
  };
}

enum class OriginalMainSurfacePass : std::uint8_t {
  // MAINWNDPROC 1158:00da -> 1158:0a3c: present the already-rendered backing
  // bitmap only. In particular, an exposure paint cannot advance RNG-backed
  // sky/facility presentation state or consume Elevator transfer visuals.
  window_paint,
  // 1090:03ab(0): restore/redraw only the changed construction preview.
  preview_repaint,
  // 1090:03ab(1): advance the visible facility-person presentation and draw
  // the dynamic layers into the backing bitmap, without rerunning the sky.
  simulation_frame,
  // 1080:0a1e(0): rebuild the world backing without 1048:03a3.
  rebuild_without_sky,
  // 1080:0a1e(1): rebuild the world backing including 1048:03a3.
  rebuild_with_sky,
  // 1158:0c29's direct palette-repaint path: recolor/present current backing
  // contents without replaying simulation-owned presentation work.
  palette_repaint,
};

struct OriginalMainSurfacePassPlan {
  bool rebuild_native_backing{};
  bool draw_scroll_floor_label{};
  bool update_construction_preview_rect{};
  bool advance_sky_decorations{};
  bool advance_visible_facility_people{};
  bool clear_elevator_transfer_visuals_before_rebuild{};
  bool consume_elevator_transfer_visuals{};

  friend bool operator==(const OriginalMainSurfacePassPlan&,
                         const OriginalMainSurfacePassPlan&) = default;
};

// Static translation of the distinct rendering boundaries at 1080:0a1e,
// 1090:03ab, and 1158:00da/0a3c. The original retains an indexed WinG backing
// bitmap; native's RGB cache must additionally rebuild for preview/palette
// changes, but those transport-only rebuilds remain mutation-free.
[[nodiscard]] constexpr OriginalMainSurfacePassPlan
original_main_surface_pass_plan(OriginalMainSurfacePass pass) noexcept {
  switch (pass) {
    case OriginalMainSurfacePass::window_paint:
      return {};
    case OriginalMainSurfacePass::preview_repaint:
      return {true, false, true, false, false, false, false};
    case OriginalMainSurfacePass::palette_repaint:
      return {true, false, false, false, false, false, false};
    case OriginalMainSurfacePass::simulation_frame:
      return {true, false, true, false, true, false, true};
    case OriginalMainSurfacePass::rebuild_without_sky:
      return {true, true, true, false, true, true, false};
    case OriginalMainSurfacePass::rebuild_with_sky:
      return {true, true, true, true, true, true, false};
  }
  return {};
}

enum class OriginalFullViewRefreshStep : std::uint8_t {
  main_rebuild_with_sky,
  map_repaint,
  command_repaint,
};

// Exact synchronous presentation order in 1080:0a02. Camera publication via
// 1080:0000/0054 completes the Main rebuild first, then forces Map WM_PAINT,
// and finally forces Command WM_PAINT before its separate focus-rectangle
// adjustment at 1080:055d.
[[nodiscard]] constexpr std::array<OriginalFullViewRefreshStep, 3>
original_full_view_refresh_order() noexcept {
  return {
      OriginalFullViewRefreshStep::main_rebuild_with_sky,
      OriginalFullViewRefreshStep::map_repaint,
      OriginalFullViewRefreshStep::command_repaint,
  };
}

enum class OriginalScrollbarRefreshStep : std::uint8_t {
  main_rebuild_with_sky,
  map_focus_adjustment,
};

// Exact visible tail at 1058:05f8 after it publishes both scrollbar
// positions and rebuilds the transient world/elevator caches. It calls
// 1080:0a1e(1), then 1080:055d's direct XOR focus transaction. Unlike the
// broader 1080:0a02 camera path, it does not repaint Map or Command.
[[nodiscard]] constexpr std::array<OriginalScrollbarRefreshStep, 2>
original_scrollbar_refresh_order() noexcept {
  return {
      OriginalScrollbarRefreshStep::main_rebuild_with_sky,
      OriginalScrollbarRefreshStep::map_focus_adjustment,
  };
}

enum class OriginalCameraRefreshStep : std::uint8_t {
  main_rebuild_with_sky,
  map_repaint,
  command_repaint,
  map_focus_adjustment,
};

// Exact visible tail shared by 1080:0000 and 1080:0054 after their scroll
// publication/geometry work. Both call bare 1080:0a02 first, then execute
// 1080:055d's separate old/new XOR focus adjustment.
[[nodiscard]] constexpr std::array<OriginalCameraRefreshStep, 4>
original_camera_refresh_order() noexcept {
  return {
      OriginalCameraRefreshStep::main_rebuild_with_sky,
      OriginalCameraRefreshStep::map_repaint,
      OriginalCameraRefreshStep::command_repaint,
      OriginalCameraRefreshStep::map_focus_adjustment,
  };
}

enum class OriginalDocumentTransitionRefreshStep : std::uint8_t {
  derived_map_focus_adjustment,
  fire_menu_update,
  main_rebuild_with_sky,
  map_repaint,
  command_repaint,
  info_repaint,
};

// Exact visible tail shared by New at 10d0:001d and successful Open at
// 10d0:062a. Their 10d0:0ac2 rebuild first presents the derived Map focus and
// updates the Fire Crew menu; the caller then runs 1080:0a02's Main/Map/
// Command transaction and finally 1118:0000's synchronous Info repaint.
[[nodiscard]] constexpr std::array<OriginalDocumentTransitionRefreshStep, 6>
original_document_transition_refresh_order() noexcept {
  return {
      OriginalDocumentTransitionRefreshStep::derived_map_focus_adjustment,
      OriginalDocumentTransitionRefreshStep::fire_menu_update,
      OriginalDocumentTransitionRefreshStep::main_rebuild_with_sky,
      OriginalDocumentTransitionRefreshStep::map_repaint,
      OriginalDocumentTransitionRefreshStep::command_repaint,
      OriginalDocumentTransitionRefreshStep::info_repaint,
  };
}

enum class OriginalDerivedMapFocusStep : std::uint8_t {
  erase_previous_focus,
  recompute_focus,
  draw_recomputed_focus,
};

// Exact direct-DC transaction in 1080:055d. The two calls to 1058:094c are
// XOR DrawFocusRect operations around 1080:038e's focus derivation; this is
// deliberately not a full Map-window repaint.
[[nodiscard]] constexpr std::array<OriginalDerivedMapFocusStep, 3>
original_derived_map_focus_order() noexcept {
  return {
      OriginalDerivedMapFocusStep::erase_previous_focus,
      OriginalDerivedMapFocusStep::recompute_focus,
      OriginalDerivedMapFocusStep::draw_recomputed_focus,
  };
}

struct OriginalGdiPosition {
  std::int16_t x{};
  std::int16_t y{};

  friend bool operator==(const OriginalGdiPosition&,
                         const OriginalGdiPosition&) = default;
};

// Exact 1208:0cf5 current-position offset. GETCURRENTPOSITION supplies the
// signed Win16 x/y words; both additions wrap as 16-bit ADD instructions
// before MOVETO receives the resulting pair.
[[nodiscard]] constexpr OriginalGdiPosition original_relative_gdi_position(
    std::int16_t current_x,
    std::int16_t current_y,
    std::int16_t delta_x,
    std::int16_t delta_y) noexcept {
  const auto x = static_cast<std::uint16_t>(
      static_cast<std::uint16_t>(current_x) +
      static_cast<std::uint16_t>(delta_x));
  const auto y = static_cast<std::uint16_t>(
      static_cast<std::uint16_t>(current_y) +
      static_cast<std::uint16_t>(delta_y));
  return {std::bit_cast<std::int16_t>(x), std::bit_cast<std::int16_t>(y)};
}

struct OriginalFatalExitPlan {
  std::uint16_t beep_type{};
  std::uint16_t exit_code{};

  friend bool operator==(const OriginalFatalExitPlan&,
                         const OriginalFatalExitPlan&) = default;
};

// Exact 1208:0dfc fatal boundary. It first emits MessageBeep(0x30), then
// passes code zero and the caller's message directly to FatalAppExit.
[[nodiscard]] constexpr OriginalFatalExitPlan
original_fatal_exit_plan() noexcept {
  return {0x30U, 0U};
}

// MAINWNDPROC 1158:00da still brackets BeginPaint/EndPaint and DS:0244 while
// iconic, but its IsIconic branch skips 1158:0a3c backing presentation.
[[nodiscard]] constexpr bool original_main_wm_paint_presents_backing(
    bool main_window_iconic) noexcept {
  return !main_window_iconic;
}

struct OriginalFullFrameAuxiliaryPresentationPlan {
  bool present_main_direct{};
  bool invalidate_command{};
  bool present_info_direct{};
  bool invalidate_map{};

  friend bool operator==(const OriginalFullFrameAuxiliaryPresentationPlan&,
                         const OriginalFullFrameAuxiliaryPresentationPlan&) =
      default;
};

struct OriginalPreviewOnlyPresentationPlan {
  bool present_main_direct{};
  bool invalidate_main{};

  friend bool operator==(const OriginalPreviewOnlyPresentationPlan&,
                         const OriginalPreviewOnlyPresentationPlan&) =
      default;
};

struct OriginalConstructionPreviewScratchState {
  bool backing_saved{};

  friend bool operator==(const OriginalConstructionPreviewScratchState&,
                         const OriginalConstructionPreviewScratchState&) =
      default;
};

struct OriginalConstructionPreviewScratchPlan {
  std::uint8_t audio_checkpoints{};
  bool clear_previous_rectangle{};
  bool derive_current_rectangle{};

  friend bool operator==(const OriginalConstructionPreviewScratchPlan&,
                         const OriginalConstructionPreviewScratchPlan&) =
      default;
};

// Exact state/host boundary of 11f8:3b94. Outside DS:b3ae isolation, a
// nonempty retained outline whose backing has not already been saved enters
// 11e0:0e84 before and after restoring the old pixels, latches DS:025c, and
// clears DS:77ac. Every failed gate is a complete no-op.
[[nodiscard]] constexpr OriginalConstructionPreviewScratchPlan
original_construction_preview_restore_plan(
    OriginalConstructionPreviewScratchState& state,
    bool isolation_active,
    bool previous_rectangle_nonempty) noexcept {
  if (isolation_active || !previous_rectangle_nonempty ||
      state.backing_saved) {
    return {};
  }
  state.backing_saved = true;
  return {2U, true, false};
}

// Exact outer gates and persistent-latch transition of 11f8:3c13. With
// construction enabled and command mode >= 3, the routine derives DS:77ac,
// enters 11e0:0e84 three times around scratch capture/outline drawing, and
// clears DS:025c. It does not consult DS:b3ae.
[[nodiscard]] constexpr OriginalConstructionPreviewScratchPlan
original_construction_preview_draw_plan(
    OriginalConstructionPreviewScratchState& state,
    bool construction_mode_enabled,
    std::uint16_t command_mode) noexcept {
  if (!construction_mode_enabled || command_mode < 3U) return {};
  state.backing_saved = false;
  return {3U, false, true};
}

// Exact 1090:03e7-0427 host decision after restoring and rederiving DS:77ac.
// Equal rectangles do nothing. A changed rectangle is presented immediately
// through an owned Main DC and 1158:0ae5; no queued WM_PAINT is involved.
[[nodiscard]] constexpr OriginalPreviewOnlyPresentationPlan
original_preview_only_presentation_plan(bool rectangles_equal) noexcept {
  return {
      .present_main_direct = !rectangles_equal,
      .invalidate_main = false,
  };
}

// Exact window-level split at 1090:061f-06dc. A dirty world causes a direct
// 1158:0a3c Main blit followed by Command invalidation. Info receives its
// direct clock/conditional-field DC pass on every full frame. Map is not part
// of this dirty-world fan-out; 1090:046f-047c refreshes it separately only on
// the recovered sixteen-tick cadence.
[[nodiscard]] constexpr OriginalFullFrameAuxiliaryPresentationPlan
original_full_frame_auxiliary_presentation_plan(
    bool world_surface_dirty) noexcept {
  return {
      world_surface_dirty,
      world_surface_dirty,
      true,
      false,
  };
}

enum class OriginalFullFrameTailStep : std::uint8_t {
  present_main_if_dirty,
  invalidate_command_if_dirty,
  present_info_direct,
  expire_info_status,
  step_final_effect_palette,
  final_audio_checkpoint,
};

// Literal 1090:061f-06ed visible/host tail. In particular, 1118:08f3 cannot
// expire the transient Info text until after the unconditional Info DC pass,
// and the fourth 1020:00cb palette step is later still.
[[nodiscard]] constexpr std::array<OriginalFullFrameTailStep, 6>
original_full_frame_tail_order() noexcept {
  return {
      OriginalFullFrameTailStep::present_main_if_dirty,
      OriginalFullFrameTailStep::invalidate_command_if_dirty,
      OriginalFullFrameTailStep::present_info_direct,
      OriginalFullFrameTailStep::expire_info_status,
      OriginalFullFrameTailStep::step_final_effect_palette,
      OriginalFullFrameTailStep::final_audio_checkpoint,
  };
}

// Exact 1258:0285-02c9 gate after a successful scheduler/full 1090:03ab(1)
// pass. The original refreshes Elevator Control whenever its HWND,
// initialization word (DS:31a2), and backing bitmap (DS:4786) all exist; it
// does not consult an elevator-changed flag. Native's direct painter uses its
// live dialog context as the initialization/backing-resource equivalent.
[[nodiscard]] constexpr bool original_idle_elevator_refresh_required(
    bool full_world_pass_completed,
    bool elevator_window_exists,
    bool elevator_control_initialized,
    bool elevator_backing_available) noexcept {
  return full_world_pass_completed && elevator_window_exists &&
         elevator_control_initialized && elevator_backing_available;
}

enum class OriginalAuxiliaryWindow : std::uint8_t {
  command,
  info,
  map,
};

enum class OriginalStartupAuxiliaryInsertAfter : std::uint8_t {
  top,
  topmost,
};

struct OriginalStartupAuxiliaryWindowSpec {
  OriginalAuxiliaryWindow window{OriginalAuxiliaryWindow::command};
  int x{};
  int y{};
  int rectangle_width{};
  int rectangle_height{};
  int create_outer_width{};
  int create_outer_height{};
  int set_position_width{};
  int set_position_height{};
  std::uint16_t window_id{};
  OriginalStartupAuxiliaryInsertAfter insert_after{
      OriginalStartupAuxiliaryInsertAfter::top};
  std::uint16_t set_position_flags{};
  std::int16_t font_pixel_height{};
  std::uint16_t text_alignment{};
  std::uint16_t background_mode{};

  friend bool operator==(const OriginalStartupAuxiliaryWindowSpec&,
                         const OriginalStartupAuxiliaryWindowSpec&) = default;
};

// Exact auxiliary-window construction transaction at 1128:05eb-08b2.
// CREATEWINDOW receives each SETRECT extent plus two SM_CXBORDER/
// SM_CYBORDER metrics. The following SETWINDOWPOS is deliberately different
// for Command: flags 0x000a omit SWP_NOSIZE, so its inflated outer size is
// replaced with the raw 63x100 rectangle while it enters the topmost band.
// Info and Map use 0x000b and therefore retain their inflated outer sizes.
// SETWINDOWWORD then publishes IDs 1000..1002 before the shared palette,
// nine-pixel font, TA_UPDATECP, and TRANSPARENT DC initialization.
[[nodiscard]] constexpr std::array<OriginalStartupAuxiliaryWindowSpec, 3>
original_startup_auxiliary_window_specs(int border_width,
                                        int border_height) noexcept {
  return {{
      {
          OriginalAuxiliaryWindow::command,
          134,
          178,
          63,
          100,
          63 + border_width * 2,
          100 + border_height * 2,
          63,
          100,
          1000U,
          OriginalStartupAuxiliaryInsertAfter::topmost,
          0x000aU,
          9,
          1U,
          1U,
      },
      {
          OriginalAuxiliaryWindow::info,
          204,
          4,
          431,
          49,
          431 + border_width * 2,
          49 + border_height * 2,
          0,
          0,
          1001U,
          OriginalStartupAuxiliaryInsertAfter::top,
          0x000bU,
          9,
          1U,
          1U,
      },
      {
          OriginalAuxiliaryWindow::map,
          2,
          4,
          200,
          314,
          200 + border_width * 2,
          314 + border_height * 2,
          0,
          0,
          1002U,
          OriginalStartupAuxiliaryInsertAfter::top,
          0x000bU,
          9,
          1U,
          1U,
      },
  }};
}

// Exact 10b8:0000 process-teardown sequence before 10b8:0039 releases the
// resource banks. Each DestroyWindow is immediately followed by clearing its
// persisted visibility word at DS:31ac, DS:31aa, or DS:31a8 respectively.
inline constexpr std::array<OriginalAuxiliaryWindow, 3>
    kOriginalAuxiliaryShutdownOrder{
        OriginalAuxiliaryWindow::map,
        OriginalAuxiliaryWindow::info,
        OriginalAuxiliaryWindow::command,
    };

enum class OriginalProcessTeardownAction : std::uint8_t {
  release_command_and_world_surfaces,
  release_font_bank,
  release_cursor_bank,
  shutdown_audio,
  release_palette,
  release_runtime_owned_storage,
};

// Exact observable group order in 10b8:0039, ending in 10b8:011e's remaining
// resource/table storage release. Native value ownership collapses individual
// Win16 GlobalUnlock/GlobalFree calls but retains the same lifetime boundary.
[[nodiscard]] constexpr std::array<OriginalProcessTeardownAction, 6>
original_process_teardown_plan() noexcept {
  using Action = OriginalProcessTeardownAction;
  return {Action::release_command_and_world_surfaces,
          Action::release_font_bank,
          Action::release_cursor_bank,
          Action::shutdown_audio,
          Action::release_palette,
          Action::release_runtime_owned_storage};
}

enum class OriginalMainPointerPressPhase : std::uint8_t {
  button_down,
  double_click,
};

struct OriginalWorldInputModifiers {
  bool control{};
  bool shift{};

  friend bool operator==(const OriginalWorldInputModifiers&,
                         const OriginalWorldInputModifiers&) = default;
};

// 1058:0028-0037 publishes both modifier words before consulting DS:0242,
// DS:b3ae, the emergency flags, or the selected tool. Win16 MK_CONTROL and
// MK_SHIFT retain the same 0x0008/0x0004 bit values at the native boundary.
[[nodiscard]] constexpr OriginalWorldInputModifiers
original_world_input_modifiers(std::uint16_t key_state) noexcept {
  return {(key_state & 0x0008U) != 0U, (key_state & 0x0004U) != 0U};
}

enum class OriginalWorldInputMessage : std::uint8_t {
  mouse_move,
  button_down,
  button_up,
  double_click,
};

enum class OriginalWorldInputAction : std::uint8_t {
  none,
  emergency_feedback,
  bulldozer,
  elevator_finger,
  magnifier,
  construction,
};

struct OriginalWorldInputPlan {
  OriginalWorldInputAction action{OriginalWorldInputAction::none};
  bool check_find_exit_latch{};

  friend bool operator==(const OriginalWorldInputPlan&,
                         const OriginalWorldInputPlan&) = default;
};

// Complete 1058:003a-01c3 routing after modifier publication. The armed
// DS:0242 gate comes first, then Elevator Control isolation (DS:b3ae), then
// Bomb/Fire b406 bits 0/3. Emergency input produces WAVE/7002 only for a
// button-down. Bulldozer and Magnifier likewise act only on button-down;
// Magnifier still reaches the post-action DS:77c0 check after every routed
// mouse phase. That latch may be set by the Person Information modal during
// the action, so the host must read it after dispatch rather than snapshot it
// while constructing this plan.
// Finger owns down/up/double-click and only an armed left-button move. All
// remaining construction tools receive every message while Build is enabled.
[[nodiscard]] constexpr OriginalWorldInputPlan original_world_input_plan(
    OriginalWorldInputMessage message,
    bool interaction_armed,
    bool elevator_isolation_active,
    bool emergency_active,
    std::uint16_t command_mode,
    bool construction_enabled,
    bool left_button_down) noexcept {
  if (!interaction_armed || elevator_isolation_active) return {};
  if (emergency_active) {
    return {message == OriginalWorldInputMessage::button_down
                ? OriginalWorldInputAction::emergency_feedback
                : OriginalWorldInputAction::none,
            false};
  }
  if (command_mode == 0U) {
    return {message == OriginalWorldInputMessage::button_down
                ? OriginalWorldInputAction::bulldozer
                : OriginalWorldInputAction::none,
            false};
  }
  if (command_mode == 1U) {
    if (message == OriginalWorldInputMessage::mouse_move &&
        !left_button_down) {
      return {};
    }
    return {OriginalWorldInputAction::elevator_finger, false};
  }
  if (command_mode == 2U) {
    return {message == OriginalWorldInputMessage::button_down
                ? OriginalWorldInputAction::magnifier
                : OriginalWorldInputAction::none,
            true};
  }
  return construction_enabled
      ? OriginalWorldInputPlan{OriginalWorldInputAction::construction, false}
      : OriginalWorldInputPlan{};
}

enum class OriginalElevatorFingerPressPath : std::uint8_t {
  service_floor,
  capture_only,
  capture_upper_cap,
  capture_lower_cap,
};

// Exact 1058:00d9-0104 split between 10a0:0000 and 10a0:0544. An initialized
// in-span shaft consumes the press in the service-floor path. Every other
// press enters 0544, sets DS:02a6, and captures Main before the optional cap
// direction is resolved; an empty-space press therefore remains capture-only.
[[nodiscard]] constexpr OriginalElevatorFingerPressPath
original_elevator_finger_press_path(bool shaft_hit,
                                    bool shaft_initialized,
                                    int floor,
                                    int bottom_floor,
                                    int top_floor) noexcept {
  if (shaft_hit && shaft_initialized && floor >= bottom_floor &&
      floor <= top_floor) {
    return OriginalElevatorFingerPressPath::service_floor;
  }
  if (shaft_hit && floor == top_floor + 1) {
    return OriginalElevatorFingerPressPath::capture_upper_cap;
  }
  if (shaft_hit && floor == bottom_floor - 1) {
    return OriginalElevatorFingerPressPath::capture_lower_cap;
  }
  return OriginalElevatorFingerPressPath::capture_only;
}

struct OriginalElevatorFingerDoubleClickPlan {
  bool open_control{};
  bool clear_capture_latch{};

  friend bool operator==(const OriginalElevatorFingerDoubleClickPlan&,
                         const OriginalElevatorFingerDoubleClickPlan&) =
      default;
};

// Exact 10a0:05bf-062c split inside 10a0:0544. Both paths restore the Finger
// cursor, release Windows capture, and clear DS:0080. A shaft hit also opens
// Elevator Control and clears DS:02a6; a miss deliberately preserves 02a6
// until the following button-up message.
[[nodiscard]] constexpr OriginalElevatorFingerDoubleClickPlan
original_elevator_finger_double_click_plan(bool shaft_hit) noexcept {
  return {shaft_hit, shaft_hit};
}

// MAINWNDPROC 1158:029f arms DS:0242 only for WM_LBUTTONDOWN with MK_LBUTTON.
// Its separate 1158:028c WM_LBUTTONDBLCLK branch merely forwards an already-
// armed interaction; it never begins a new one. In the normal Windows double-
// click sequence the preceding button-up has cleared DS:0242, suppressing a
// duplicate edit or construction action.
[[nodiscard]] constexpr bool original_main_pointer_begins_interaction(
    OriginalMainPointerPressPhase phase,
    bool left_button_down) noexcept {
  return phase == OriginalMainPointerPressPhase::button_down &&
         left_button_down;
}

enum class OriginalMainPointerMessagePhase : std::uint8_t {
  button_down,
  double_click,
  button_up,
};

struct OriginalMainPointerMessagePlan {
  bool forward_to_world_dispatch{};
  bool arm_interaction{};
  bool clear_interaction{};

  friend bool operator==(const OriginalMainPointerMessagePlan&,
                         const OriginalMainPointerMessagePlan&) = default;
};

// Exact MAINWNDPROC 1158:028c/029f/02d5 gate around DS:0242. DS:24b8
// (modal-input lock) and DS:0244 (WM_PAINT reentrancy latch) suppress all
// three messages without changing the armed state. Double-click forwards but
// never arms; 1058:0000 performs the subsequent DS:0242 effectiveness check.
[[nodiscard]] constexpr OriginalMainPointerMessagePlan
original_main_pointer_message_plan(OriginalMainPointerMessagePhase phase,
                                   bool modal_input_locked,
                                   bool paint_active,
                                   bool left_button_down,
                                   bool interaction_armed) noexcept {
  if (modal_input_locked || paint_active) return {};
  switch (phase) {
    case OriginalMainPointerMessagePhase::button_down:
      return left_button_down
          ? OriginalMainPointerMessagePlan{true, true, false}
          : OriginalMainPointerMessagePlan{};
    case OriginalMainPointerMessagePhase::double_click:
      return {true, false, false};
    case OriginalMainPointerMessagePhase::button_up:
      return !left_button_down && interaction_armed
          ? OriginalMainPointerMessagePlan{true, false, true}
          : OriginalMainPointerMessagePlan{};
  }
  return {};
}

// Exact position passed to SetScrollPos by MAINWNDPROC's WM_HSCROLL and
// WM_VSCROLL branches at 1158:015b/01e7. Codes 0..3 apply the original
// 16-pixel line or client-minus-16 page delta and clamp it in game code.
// Codes 4 and 5 pass the raw 16-bit position carried by the Win16 message;
// SetScrollPos itself then enforces the range installed for the scrollbar.
[[nodiscard]] std::optional<int> original_main_scroll_request_position(
    std::uint16_t scroll_code,
    int current_position,
    std::uint16_t message_position,
    int client_extent,
    int maximum_position) noexcept;

enum class OriginalMainSizeDisposition : std::uint8_t {
  ignore,
  invalidate_only,
  restore_or_maximize,
  minimize,
};

// MAINWNDPROC 1158:041c ignores every WM_SIZE until 1258:04e2 publishes the
// successful 1128:0005 initialization result at DS:02a4. Once initialized,
// SIZE_RESTORED (0), SIZE_MINIMIZED (1), and SIZE_MAXIMIZED (2) take their
// dedicated branches; other size codes perform only the leading invalidation.
[[nodiscard]] constexpr OriginalMainSizeDisposition
original_main_size_disposition(bool runtime_initialized,
                               std::uint16_t size_code) noexcept {
  if (!runtime_initialized) {
    return OriginalMainSizeDisposition::ignore;
  }
  if (size_code == 0U || size_code == 2U) {
    return OriginalMainSizeDisposition::restore_or_maximize;
  }
  if (size_code == 1U) {
    return OriginalMainSizeDisposition::minimize;
  }
  return OriginalMainSizeDisposition::invalidate_only;
}

struct OriginalMainScrollbarResizeState {
  int minimum{};
  int maximum{};
  std::uint32_t native_page_size{};
  int position{};

  friend bool operator==(const OriginalMainScrollbarResizeState&,
                         const OriginalMainScrollbarResizeState&) = default;
};

// Exact range and saved-position clamp rebuilt by 1158:05ef -> 1080:00d7.
// Win16 SetScrollRange has no page-size input, so the equivalent Win32
// SCROLLINFO uses nPage=0 and nMax=world-client to retain the fixed system
// thumb rather than introducing a proportional native thumb.
[[nodiscard]] OriginalMainScrollbarResizeState
original_main_scrollbar_resize_state(int saved_position,
                                     int client_extent,
                                     int world_extent) noexcept;

struct OriginalMainNonclientMetrics {
  int frame_width{};
  int frame_height{};
  int vertical_scroll_width{};
  int horizontal_scroll_height{};
  int menu_height{};
  int caption_height{};
  int border_width{};
  int border_height{};

  friend bool operator==(const OriginalMainNonclientMetrics&,
                         const OriginalMainNonclientMetrics&) = default;
};

struct OriginalMainWindowGeometry {
  int initial_x{};
  int initial_y{};
  int initial_width{};
  int initial_height{};
  int minimum_track_width{};
  int minimum_track_height{};
  int maximum_track_width{};
  int maximum_track_height{};

  friend bool operator==(const OriginalMainWindowGeometry&,
                         const OriginalMainWindowGeometry&) = default;
};

// Exact desktop-dependent client caps from 1128:02aa, startup rectangle from
// 1128:08d6, and MINMAXINFO formulas from 1158:0334. The 816x576 maximum is a
// cap, not an unconditional size: smaller desktops first lose the original
// scrollbar/menu/caption extents before the tracking and startup bounds are
// derived.
[[nodiscard]] OriginalMainWindowGeometry original_main_window_geometry(
    int desktop_right,
    int desktop_bottom,
    OriginalMainNonclientMetrics metrics) noexcept;

enum class OriginalAuxiliaryWindowOperation : std::uint8_t {
  hide,
  show,
};

// Literal parallel-lookup message words at 1158:0597, 1120:01ed, and
// 1168:028a. Keeping each procedure's identity explicit prevents the native
// adapters from accidentally consuming host-only messages that the original
// window procedure left to DefWindowProc.
inline constexpr std::array<std::uint16_t, 22>
    kOriginalMainWindowMessages = {
        0x0001U, 0x0002U, 0x0005U, 0x0006U, 0x000fU, 0x0010U,
        0x0011U, 0x001cU, 0x0024U, 0x0084U, 0x0104U, 0x0105U,
        0x0111U, 0x0114U, 0x0115U, 0x0200U, 0x0201U, 0x0202U,
        0x0203U, 0x030fU, 0x0311U, 0x03bdU,
    };

// Literal 27-entry command lookup at 1158:09d0. IDs outside this table still
// enter the 3000..4001 generic process-dialog range when applicable, then
// fall through to DefWindowProc at 1158:09b1 rather than being consumed.
inline constexpr std::array<std::uint16_t, 27>
    kOriginalMainCommandIds = {
        3000U,  3001U,  3002U,  9000U,  9001U,  9002U,  9003U,
        40001U, 40002U, 40003U, 40004U, 40005U, 40007U, 40008U,
        40009U, 40010U, 40011U, 40012U, 40013U, 40014U, 40015U,
        40016U, 40017U, 40018U, 40019U, 40020U, 40021U,
    };

[[nodiscard]] constexpr bool original_main_command_is_table_entry(
    std::uint16_t command) noexcept {
  for (const auto candidate : kOriginalMainCommandIds) {
    if (candidate == command) return true;
  }
  return false;
}

enum class OriginalMainCommandRoute : std::uint8_t {
  table_entry,
  generic_dialog_then_def_window_proc,
  def_window_proc,
};

// Exact 1158:06b9 dispatcher fallthrough. Non-table command IDs 3000..4001
// run the generic 1068:0000 process dialog and then still reach DefWindowProc;
// every other non-table ID reaches DefWindowProc directly.
[[nodiscard]] constexpr OriginalMainCommandRoute original_main_command_route(
    std::uint16_t command) noexcept {
  if (original_main_command_is_table_entry(command)) {
    return OriginalMainCommandRoute::table_entry;
  }
  if (command >= 3000U && command <= 4001U) {
    return OriginalMainCommandRoute::generic_dialog_then_def_window_proc;
  }
  return OriginalMainCommandRoute::def_window_proc;
}

inline constexpr std::array<std::uint16_t, 10>
    kOriginalInfoWindowMessages = {
        0x0002U, 0x0006U, 0x000fU, 0x001cU, 0x0084U,
        0x0086U, 0x0111U, 0x0201U, 0x030fU, 0x0311U,
    };

// The active, non-modal WM_ACTIVATE paths at CMDBTNWNDPROC 1050:008d,
// INFOWNDPROC 1120:007d, and MAPWNDPROC 1168:009f call GetClientRect and then
// overwrite only RECT.bottom with the literal 8 before ValidateRect. Thus the
// title strip is validated while any pending content-area update survives.
[[nodiscard]] constexpr RECT original_palette_activation_validation_rect(
    RECT client) noexcept {
  client.bottom = kOriginalPaletteFrameHeight;
  return client;
}
inline constexpr std::array<std::uint16_t, 13>
    kOriginalMapWindowMessages = {
        0x0001U, 0x0002U, 0x0006U, 0x000fU, 0x001cU,
        0x0084U, 0x0086U, 0x0111U, 0x0200U, 0x0201U,
        0x0202U, 0x030fU, 0x0311U,
    };

enum class OriginalAuxiliaryInsertAfter : std::uint8_t {
  top,
  topmost,
  command,
};

struct OriginalAuxiliaryWindowAction {
  OriginalAuxiliaryWindow target{OriginalAuxiliaryWindow::command};
  OriginalAuxiliaryWindowOperation operation{
      OriginalAuxiliaryWindowOperation::hide};
  OriginalAuxiliaryInsertAfter insert_after{
      OriginalAuxiliaryInsertAfter::top};

  friend bool operator==(const OriginalAuxiliaryWindowAction&,
                         const OriginalAuxiliaryWindowAction&) = default;
};

enum class OriginalAuxiliaryVisibilityTrigger : std::uint8_t {
  menu_command,
  close_box,
};

struct OriginalAuxiliaryVisibilityPlan {
  bool visible{};
  OriginalAuxiliaryWindowOperation operation{
      OriginalAuxiliaryWindowOperation::hide};
  bool update_menu_check{};

  friend bool operator==(const OriginalAuxiliaryVisibilityPlan&,
                         const OriginalAuxiliaryVisibilityPlan&) = default;
};

// The View commands at 1158:0886/08a6/08c4 toggle only DS:31a8/31aa/31ac
// and the window itself. Likewise the close-box branches at 1050:024e,
// 1120:0111, and 1168:0112 clear the word and hide the palette. None of these
// paths calls CheckMenuItem, so the three marks remain in their initialized
// state even while a palette is hidden.
[[nodiscard]] constexpr OriginalAuxiliaryVisibilityPlan
original_auxiliary_visibility_plan(
    OriginalAuxiliaryVisibilityTrigger trigger,
    bool currently_visible) noexcept {
  const bool visible = trigger == OriginalAuxiliaryVisibilityTrigger::menu_command
      ? !currently_visible
      : false;
  return {
      .visible = visible,
      .operation = visible ? OriginalAuxiliaryWindowOperation::show
                           : OriginalAuxiliaryWindowOperation::hide,
      .update_menu_check = false,
  };
}

enum class OriginalAuxiliaryActivationInsertAfter : std::uint8_t {
  top,
  topmost,
  main,
};

struct OriginalAuxiliaryActivationAction {
  OriginalAuxiliaryWindow target{OriginalAuxiliaryWindow::command};
  OriginalAuxiliaryActivationInsertAfter insert_after{
      OriginalAuxiliaryActivationInsertAfter::top};

  friend bool operator==(const OriginalAuxiliaryActivationAction&,
                         const OriginalAuxiliaryActivationAction&) = default;
};

struct OriginalPaletteAppActivationPlan {
  bool promote_main{};
  bool forwarded_active{};

  friend bool operator==(const OriginalPaletteAppActivationPlan&,
                         const OriginalPaletteAppActivationPlan&) = default;
};

// Each palette procedure repeats the same WM_ACTIVATEAPP prefix at
// 1050:0032, 1120:002f, and 1168:0032: on activation, an existing Main window
// is raised and shown with Win16 flag word 0x53 before the state is forwarded
// to 1078:01e8. Deactivation skips only that prefix.
[[nodiscard]] constexpr OriginalPaletteAppActivationPlan
original_palette_app_activation_plan(bool active,
                                     bool main_window_available) noexcept {
  return {
      .promote_main = active && main_window_available,
      .forwarded_active = active,
  };
}

struct OriginalPaletteWindowActivationPlan {
  bool insert_behind_modal{};
  bool activate_modal{};
  bool focus_modal{};
  bool validate_client{};
  bool promote_command_topmost{};
  bool command_promotion_no_activate{};
  bool write_shared_activation_latch{};
  bool shared_activation_latch{};

  friend bool operator==(const OriginalPaletteWindowActivationPlan&,
                         const OriginalPaletteWindowActivationPlan&) = default;
};

struct OriginalAuxiliaryPaintPlan {
  bool invalidate_entire_client{};
  bool realize_palette{};
  bool draw_content{};

  friend bool operator==(const OriginalAuxiliaryPaintPlan&,
                         const OriginalAuxiliaryPaintPlan&) = default;
};

// CMDBTNWNDPROC 1050:010d always expands a paint to the full client and
// selects/realizes DS:795e before its visibility/startup/closing content gates.
// The shared Info painter at 1120:0215 invalidates only while visible and Map's
// 1168:016a path keeps the incoming update rectangle; both select/realize only
// after their three content gates pass. All three still begin/end the paint.
[[nodiscard]] constexpr OriginalAuxiliaryPaintPlan
original_auxiliary_paint_plan(OriginalAuxiliaryWindow window,
                              bool visible,
                              bool runtime_initialized,
                              bool closing) noexcept {
  return {
      .invalidate_entire_client =
          window == OriginalAuxiliaryWindow::command ||
          (window == OriginalAuxiliaryWindow::info && visible),
      .realize_palette =
          window == OriginalAuxiliaryWindow::command ||
          (visible && runtime_initialized && !closing),
      .draw_content = visible && runtime_initialized && !closing,
  };
}

enum class OriginalPaletteMessageKind : std::uint8_t {
  query_new_palette,
  palette_changed,
};

struct OriginalPaletteMessagePlan {
  bool realize{};
  std::intptr_t result{};

  friend bool operator==(const OriginalPaletteMessagePlan&,
                         const OriginalPaletteMessagePlan&) = default;
};

// MAINWNDPROC 1158:04fe/0508 ignores self-sent WM_PALETTECHANGED but otherwise
// realizes the logical palette. Unlike the usual Win32 convention, its
// WM_QUERYNEWPALETTE branch always returns zero even when entries changed.
[[nodiscard]] constexpr OriginalPaletteMessagePlan
original_main_palette_message_plan(OriginalPaletteMessageKind kind,
                                   bool source_is_self) noexcept {
  return {
      .realize = kind == OriginalPaletteMessageKind::query_new_palette ||
                 !source_is_self,
      .result = 0,
  };
}

// CMDBTNWNDPROC 1050:030a/0300, INFOWNDPROC 1120:017a/0170, and MAPWNDPROC
// 1168:0216/020c handle QUERYNEWPALETTE/PALETTECHANGED. Query always realizes;
// Changed suppresses a self message. Both honor the New/Open (DS:31c4) and
// closing (DS:31c6) gates.
[[nodiscard]] constexpr OriginalPaletteMessagePlan
original_auxiliary_palette_message_plan(OriginalPaletteMessageKind kind,
                                        bool source_is_self,
                                        bool runtime_initialized,
                                        bool closing) noexcept {
  return {
      .realize = runtime_initialized && !closing &&
                 (kind == OriginalPaletteMessageKind::query_new_palette ||
                  !source_is_self),
      .result = 0,
  };
}

[[nodiscard]] constexpr OriginalPaletteMessagePlan
original_auxiliary_palette_changed_plan(bool source_is_self,
                                        bool runtime_initialized,
                                        bool closing) noexcept {
  return original_auxiliary_palette_message_plan(
      OriginalPaletteMessageKind::palette_changed, source_is_self,
      runtime_initialized, closing);
}

struct OriginalCommandSelectorPaletteChangedPlan {
  bool realize{};
  bool update_colors_when_changed{};
  std::intptr_t result{};

  friend bool operator==(const OriginalCommandSelectorPaletteChangedPlan&,
                         const OriginalCommandSelectorPaletteChangedPlan&) =
      default;
};

// CMDBTNSUBWNDPROC's final table branch at 1050:090a ignores a self-sent
// WM_PALETTECHANGED but still returns TRUE. A non-self message realizes the
// palette and calls GDI UpdateColors on that same DC only if entries changed;
// unlike the three top-level auxiliary procedures, it has no startup/close
// gate and does not enter 1158:0c29's four-window repaint fan-out.
[[nodiscard]] constexpr OriginalCommandSelectorPaletteChangedPlan
original_command_selector_palette_changed_plan(
    bool source_is_self) noexcept {
  return {
      .realize = !source_is_self,
      .update_colors_when_changed = !source_is_self,
      .result = 1,
  };
}

enum class OriginalPaletteSurface : std::uint8_t {
  map,
  info,
  command,
  main,
};

enum class OriginalPaletteRepaintMechanism : std::uint8_t {
  direct_dc,
  update_window,
};

struct OriginalPaletteRepaintAction {
  OriginalPaletteSurface surface{OriginalPaletteSurface::map};
  OriginalPaletteRepaintMechanism mechanism{
      OriginalPaletteRepaintMechanism::direct_dc};
  bool invalidate{};
  bool reuse_source_dc{};
  bool select_and_realize{};
  bool release_dc{};

  friend bool operator==(const OriginalPaletteRepaintAction&,
                         const OriginalPaletteRepaintAction&) = default;
};

// 1158:0c29 performs its synchronous palette-change fan-out only after the
// successful-startup gate and only when RealizePalette reports a change.
[[nodiscard]] std::vector<OriginalPaletteSurface>
original_palette_repaint_order(bool runtime_initialized,
                               bool realized_entries_changed);

// Exact per-surface mechanism and DC ownership inside 1158:0c29. Map, Command,
// and Main draw synchronously through a supplied/acquired DC; Info alone uses
// InvalidateRect(NULL)+UpdateWindow. Map and Main invalidate their client
// rectangles before the direct call, while Command only blits its content.
[[nodiscard]] std::vector<OriginalPaletteRepaintAction>
original_palette_repaint_actions(bool runtime_initialized,
                                 bool realized_entries_changed,
                                 OriginalPaletteSurface source);

// Exact 1078:0000 auxiliary-palette minimize/restore plan. A restore shows
// only the palettes whose View-menu state remains enabled: Command is made
// topmost first, then Info and Map are inserted immediately behind Command
// (or at HWND_TOP when Command is disabled). A minimize hides enabled
// palettes in the original Info, Command, Map order.
[[nodiscard]] std::vector<OriginalAuxiliaryWindowAction>
original_auxiliary_window_actions(
    bool restore,
    bool command_visible,
    bool info_visible,
    bool map_visible);

// Exact 1078:01e8 z-order plan for WM_ACTIVATEAPP. Deactivation places each
// enabled palette immediately behind the main window so a TOPMOST Command
// palette does not remain above other applications. Reactivation preserves
// the executable's unusual repeated Command promotion before TOPMOST.
[[nodiscard]] std::vector<OriginalAuxiliaryActivationAction>
original_auxiliary_activation_actions(
    bool active,
    bool command_visible,
    bool info_visible,
    bool map_visible);

// Exact per-window WM_ACTIVATE branches from CMDBTNWNDPROC 1050:0063,
// INFOWNDPROC 1120:005e, and MAPWNDPROC 1168:006a. DS:31a4 is the currently
// active modal dialog, not Main: an active palette redirects to that dialog
// (Command also redirects focus), while Map first inserts behind it. Without
// a modal, Command preserves its TOPMOST state when enabled and the preceding
// shared activation latch was nonzero. The procedures' distinct paths decide
// whether the new activation word is actually written to DS:31a6. Every
// active non-modal path also validates only client rows 0..7 before that
// write, preserving pending content-area paints exactly as the Win16
// procedures do.
[[nodiscard]] OriginalPaletteWindowActivationPlan
original_palette_window_activation_plan(
    OriginalAuxiliaryWindow window,
    bool active,
    bool modal_dialog_available,
    bool command_window_enabled,
    bool previous_shared_activation_latch) noexcept;

}  // namespace simtower
