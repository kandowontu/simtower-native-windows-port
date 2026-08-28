#pragma once

#include "original_palette_frame.hpp"
#include "original_resources.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace simtower {

inline constexpr int kOriginalCommandSurfaceWidth = 64;
inline constexpr int kOriginalCommandSurfaceHeight = 213;
inline constexpr int kOriginalCommandPaintOffsetY = 8;
inline constexpr std::array<std::uint16_t, 3>
    kOriginalCommandSurfaceResourceIds{300U, 301U, 302U};
// 1050:03aa owns one staging DIB plus the three resource-derived icon DIBs;
// 1050:0503 deletes those four objects in reverse resource order.
inline constexpr std::size_t kOriginalCommandSurfaceObjectCount = 4U;

// Exact eleven-entry CMDBTNWNDPROC parallel message table at
// 1050:0015/037e. Messages not present here fall through to DefWindowProc.
[[nodiscard]] constexpr bool original_command_window_handles_message(
    std::uint16_t message) noexcept {
  switch (message) {
    case 0x0001:  // WM_CREATE
    case 0x0002:  // WM_DESTROY
    case 0x0006:  // WM_ACTIVATE
    case 0x000f:  // WM_PAINT
    case 0x001c:  // WM_ACTIVATEAPP
    case 0x0084:  // WM_NCHITTEST
    case 0x0086:  // WM_NCACTIVATE
    case 0x0201:  // WM_LBUTTONDOWN
    case 0x0202:  // WM_LBUTTONUP
    case 0x030f:  // WM_QUERYNEWPALETTE
    case 0x0311:  // WM_PALETTECHANGED
      return true;
    default:
      return false;
  }
}

// Exact seven-entry CMDBTNSUBWNDPROC parallel message table at
// 1050:05bc/095c. The selector deliberately has no WM_CLOSE route.
[[nodiscard]] constexpr bool original_command_selector_handles_message(
    std::uint16_t message) noexcept {
  switch (message) {
    case 0x000f:  // WM_PAINT
    case 0x0110:  // WM_INITDIALOG
    case 0x0200:  // WM_MOUSEMOVE
    case 0x0201:  // WM_LBUTTONDOWN
    case 0x0202:  // WM_LBUTTONUP
    case 0x0204:  // WM_RBUTTONDOWN
    case 0x0311:  // WM_PALETTECHANGED
      return true;
    default:
      return false;
  }
}

struct OriginalCommandRect {
  int left{};
  int top{};
  int right{};
  int bottom{};

  [[nodiscard]] constexpr bool contains(int x, int y) const noexcept {
    return x >= left && y >= top && x < right && y < bottom;
  }

  friend bool operator==(const OriginalCommandRect&,
                         const OriginalCommandRect&) = default;
};

enum class OriginalCommandHitKind : std::uint8_t {
  none,
  build_toggle,
  edit_mode,
  facility,
};

struct OriginalCommandHit {
  OriginalCommandHitKind kind{OriginalCommandHitKind::none};
  std::uint16_t mode_index{};
  std::uint16_t facility_type{};

  friend bool operator==(const OriginalCommandHit&,
                         const OriginalCommandHit&) = default;
};

enum class OriginalCommandPointerPhase : std::uint8_t {
  button_down,
  button_up,
};

struct OriginalCommandPointerPlan {
  bool close_palette{};
  bool press_toggle{};
  bool restore_toggle{};
  bool activate_point{};
  bool sample_coarse_tick{};

  friend bool operator==(const OriginalCommandPointerPlan&,
                         const OriginalCommandPointerPlan&) = default;
};

struct OriginalCommandRaster {
  int width{kOriginalCommandSurfaceWidth};
  int height{kOriginalCommandSurfaceHeight};
  // Top-down 32-bit BI_RGB pixels in 0x00RRGGBB form.
  std::vector<std::uint32_t> pixels{};

  [[nodiscard]] std::uint32_t at(int x, int y) const;
};

// The Win16 code writes the chosen one-based TABM index back into the low
// byte of the active TABL word (1140:0394). Keep an equivalent mutable copy
// instead of altering the embedded, read-only resource pack.
struct OriginalCommandRatingState {
  std::uint16_t rating{1U};
  std::vector<std::uint16_t> encoded_entries{};
};

// Net host-visible tail of 1140:010d after it replaces the active
// TABL/(1000+rating), refreshes the rating-dependent Info thresholds and
// facility sheets, and resizes Command. New/Open reconstruction supplies zero;
// both 1140:002d and 1140:00a8 promotions supply one. The latter distinction
// is significant: every promotion changes DS:783c to edit mode two before the
// synchronous 1080:05a1 Command presentation. 11f8:3b94 restores a pending
// construction outline only while Elevator Control isolation (DS:b3ae) is
// inactive, then 1038:0000 always rebuilds the visible-row tile scratch.
struct OriginalRatingCommandRefreshPlan {
  bool force_command_mode_two{};
  bool present_command_synchronously{true};
  bool restore_preview_scratch{};
  bool rebuild_visible_tile_scratch{true};

  friend bool operator==(const OriginalRatingCommandRefreshPlan&,
                         const OriginalRatingCommandRefreshPlan&) = default;
};

[[nodiscard]] constexpr OriginalRatingCommandRefreshPlan
original_rating_command_refresh_plan(std::uint16_t argument,
                                     bool elevator_isolation_active,
                                     bool preview_scratch_pending) noexcept {
  return {
      .force_command_mode_two = argument != 0U,
      .present_command_synchronously = true,
      .restore_preview_scratch =
          !elevator_isolation_active && preview_scratch_pending,
      .rebuild_visible_tile_scratch = true,
  };
}

struct OriginalCommandGroup {
  std::uint16_t tabm_number{};
  std::uint16_t selection_index{};  // one-based, as stored by the original
  std::vector<std::uint16_t> catalog_icons{};
};

// 1058:0517-053f temporarily clears/restores the system button swap and then
// samples the physical primary button. This is semantically equivalent to
// choosing VK_LBUTTON for the normal setting and VK_RBUTTON for the swapped
// setting, without transiently changing the user's system preference.
[[nodiscard]] constexpr std::uint16_t
original_command_primary_button_virtual_key(bool buttons_swapped) noexcept {
  return buttons_swapped ? 0x02U : 0x01U;  // VK_RBUTTON / VK_LBUTTON
}

struct OriginalCommandSelectorTransactionPlan {
  bool show_modal{};
  bool write_choice{};

  friend bool operator==(const OriginalCommandSelectorTransactionPlan&,
                         const OriginalCommandSelectorTransactionPlan&) =
      default;
};

// Exact host-visible branch at 1058:053d-05e3. A grouped command opens its
// selector only while the physical primary button is still held. An accepted
// modal writes the one-based choice; cancel and the button-up re-entry both
// retain the existing low byte before the command's build type is resolved.
[[nodiscard]] constexpr OriginalCommandSelectorTransactionPlan
original_command_selector_transaction_plan(bool primary_button_down,
                                           bool modal_accepted) noexcept {
  return {
      .show_modal = primary_button_down,
      .write_choice = primary_button_down && modal_accepted,
  };
}

// Exact geometry from 1058:071f/0798/0828 after the command parent adds its
// fixed eight-pixel presentation offset.
[[nodiscard]] constexpr OriginalCommandRect original_command_toggle_rect() {
  return {20, 12, 41, 34};
}

[[nodiscard]] constexpr OriginalCommandRect original_command_mode_rect(
    std::uint16_t mode) {
  return {static_cast<int>(mode) * 21,
          40,
          static_cast<int>(mode) * 21 + 21,
          61};
}

[[nodiscard]] constexpr OriginalCommandRect original_command_facility_rect(
    std::uint16_t catalog_index) {
  return {static_cast<int>(catalog_index & 1U) * 32,
          61 + static_cast<int>(catalog_index / 2U) * 32,
          static_cast<int>(catalog_index & 1U) * 32 + 32,
          93 + static_cast<int>(catalog_index / 2U) * 32};
}

// Exact CMDBTNWNDPROC mouse phase split at 1050:0219/02b3. Mouse-down closes
// the palette or presents the build-toggle pressed frame; all other command
// points activate immediately. Mouse-up always restores the toggle frame and
// activates any point outside the close box. A grouped facility selector
// captures that release itself, so its parent naturally receives only the
// original mouse-down activation. Every non-close mouse-down then samples
// 1208:05e6 into DS:31b0/31b2; close and all mouse-up paths do not.
[[nodiscard]] OriginalCommandPointerPlan original_command_pointer_plan(
    OriginalCommandPointerPhase phase,
    int x,
    int y) noexcept;

// Exact vertical placement from CMDBTNSUBWNDPROC at 1050:05d6. The selected
// row is aligned with the clicked palette cell, then the popup is clamped to
// the desktop's top and bottom edges in the original order.
[[nodiscard]] int original_command_selector_top(
    int anchor_top,
    std::uint16_t one_based_choice,
    std::size_t icon_count,
    int desktop_bottom) noexcept;

// 1140:022c over the active TABL/(1000+rating) and any TABM indirections.
[[nodiscard]] std::vector<std::uint16_t> original_command_catalog(
    const OriginalResources& resources,
    std::uint16_t rating);

[[nodiscard]] OriginalCommandRatingState original_command_rating_state(
    const OriginalResources& resources,
    std::uint16_t rating);

[[nodiscard]] std::vector<std::uint16_t> original_command_catalog(
    const OriginalResources& resources,
    const OriginalCommandRatingState& state);

[[nodiscard]] std::optional<OriginalCommandGroup> original_command_group(
    const OriginalResources& resources,
    const OriginalCommandRatingState& state,
    std::size_t catalog_index);

void original_command_select_group_choice(
    const OriginalResources& resources,
    OriginalCommandRatingState& state,
    std::size_t catalog_index,
    std::uint16_t one_based_choice);

// 11f8:0f63 reads one raw byte from TABL/1000 at the selected catalog icon
// index. It turns that byte into the construction type later dispatched by
// 11f8:07d8. TABL/1000 is not a count-prefixed word table.
[[nodiscard]] std::uint16_t original_command_build_type(
    const OriginalResources& resources,
    std::uint16_t catalog_icon);

// 1058:03a9 hit order: toggle, three edit modes, then rating catalog.
[[nodiscard]] OriginalCommandHit original_command_hit_test(
    const OriginalResources& resources,
    std::uint16_t rating,
    bool build_enabled,
    int x,
    int y);

[[nodiscard]] OriginalCommandHit original_command_hit_test(
    const OriginalResources& resources,
    const OriginalCommandRatingState& state,
    bool build_enabled,
    int x,
    int y);

// Exact static composition rooted at 1080:05a1. 1030:0000's DIB loader and
// 1050:03aa's one-time preload and 1050:0503's four-object GDI teardown are
// replaced by value views over the embedded resource pack. BITMAP/600..606 supply the header/mode strip and
// BITMAP/300..302 supply resource-driven facility icons.
[[nodiscard]] OriginalCommandRaster render_original_command_palette(
    const OriginalResources& resources,
    std::uint16_t rating,
    bool build_enabled,
    std::uint16_t selected_mode,
    int client_width,
    int client_height,
    bool build_toggle_pressed = false);

[[nodiscard]] OriginalCommandRaster render_original_command_palette(
    const OriginalResources& resources,
    const OriginalCommandRatingState& state,
    bool build_enabled,
    std::uint16_t selected_mode,
    int client_width,
    int client_height,
    bool build_toggle_pressed = false);

// Exact 1050:0978 selector composition. selected_icon_plus_one is deliberate:
// WM_MOUSEMOVE stores TABM(icon)+1 for highlighting, while WM_INITDIALOG
// initially passes the encoded one-based choice index (an original quirk).
[[nodiscard]] OriginalCommandRaster render_original_command_selector(
    const OriginalResources& resources,
    const OriginalCommandGroup& group,
    std::uint16_t selected_icon_plus_one,
    bool build_enabled = true);

void draw_original_command_raster(HDC destination,
                                  const OriginalCommandRaster& raster,
                                  int x = 0,
                                  int y = kOriginalCommandPaintOffsetY);

}  // namespace simtower
