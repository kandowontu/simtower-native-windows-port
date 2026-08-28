#pragma once

#include "original_resources.hpp"
#include "original_tdt.hpp"
#include "original_world.hpp"

#include <cstdint>
#include <optional>

namespace simtower {

// 1160:0000 creates a 200x306 WinG backing bitmap. 1168:02be presents that
// bitmap at client y=8, making the complete map palette window 200x314.
inline constexpr int kOriginalMapWidth = 200;
inline constexpr int kOriginalMapBackingHeight = 306;
inline constexpr int kOriginalMapClientTop = 8;
inline constexpr int kOriginalMapToolbarHeight = 18;
inline constexpr int kOriginalMapContentHeight = 288;

// Exact thirteen-entry MAPWNDPROC parallel message table at 1168:0015/028a.
// WM_COMMAND is deliberately present even though its target is DefWindowProc.
[[nodiscard]] constexpr bool original_map_window_handles_message(
    std::uint16_t message) noexcept {
  switch (message) {
    case 0x0001:  // WM_CREATE
    case 0x0002:  // WM_DESTROY
    case 0x0006:  // WM_ACTIVATE
    case 0x000f:  // WM_PAINT
    case 0x001c:  // WM_ACTIVATEAPP
    case 0x0084:  // WM_NCHITTEST
    case 0x0086:  // WM_NCACTIVATE
    case 0x0111:  // WM_COMMAND -> DefWindowProc
    case 0x0200:  // WM_MOUSEMOVE
    case 0x0201:  // WM_LBUTTONDOWN
    case 0x0202:  // WM_LBUTTONUP
    case 0x030f:  // WM_QUERYNEWPALETTE
    case 0x0311:  // WM_PALETTECHANGED
      return true;
    default:
      return false;
  }
}

// Exact 1058:01d6 toolbar hit gate. Rating one exposes only modes 0..2 even
// though 1080:093a paints the fourth 50-pixel cell with its disabled frame;
// later ratings expose all four modes. Coordinates are in the 200x306 map
// backing surface after MAPWNDPROC removes its eight-pixel frame inset.
[[nodiscard]] constexpr std::optional<std::uint16_t>
original_map_toolbar_mode_at(int x, int y, std::uint16_t rating) noexcept {
  if (x < 0 || x >= kOriginalMapWidth ||
      y < 0 || y >= kOriginalMapToolbarHeight) {
    return std::nullopt;
  }
  const auto mode = static_cast<std::uint16_t>(x / 50);
  if (rating == 1U && mode == 3U) {
    return std::nullopt;
  }
  return mode;
}

enum class OriginalMapPointerMessage : std::uint8_t {
  button_down,
  mouse_move,
  button_up,
  capture_changed,
};

struct OriginalMapPointerMessagePlan {
  bool consume_message{};
  bool update_drag{};
  bool set_pointer_down{};
  bool clear_pointer_down{};
  bool clear_drag{};
  bool release_capture{};

  friend bool operator==(const OriginalMapPointerMessagePlan&,
                         const OriginalMapPointerMessagePlan&) = default;
};

// Exact MAPWNDPROC table behavior at 1168:0125/013a/0156. A non-close button
// down sets DS:0248 before 1058:01d6. Every WM_MOUSEMOVE is consumed, although
// 1058:0284 runs only while both MK_LBUTTON and DS:3216 are set. Every
// WM_LBUTTONUP clears DS:0248 and DS:3216 and releases capture. Win16 has no
// WM_CAPTURECHANGED entry in the recovered 13-message table, so the native
// adapter leaves both words untouched and passes that host-only message on.
[[nodiscard]] constexpr OriginalMapPointerMessagePlan
original_map_pointer_message_plan(OriginalMapPointerMessage message,
                                  bool drag_active,
                                  bool left_button_down) noexcept {
  switch (message) {
    case OriginalMapPointerMessage::button_down:
      return {true, false, true, false, false, false};
    case OriginalMapPointerMessage::mouse_move:
      return {true, drag_active && left_button_down,
              false, false, false, false};
    case OriginalMapPointerMessage::button_up:
      return {true, false, false, true, true, true};
    case OriginalMapPointerMessage::capture_changed:
      return {};
  }
  return {};
}

struct OriginalMapRect {
  int left{};
  int top{};
  int right{};
  int bottom{};

  friend bool operator==(const OriginalMapRect&,
                         const OriginalMapRect&) = default;
};

// Exact 1080:0209 aspect-preserving rectangle fit used by 1128:08d6 while
// deriving the world-to-Map transform. The original never enlarges content
// that already fits; otherwise it constrains the dominant percentage and
// centers the result with signed division truncated toward zero.
[[nodiscard]] OriginalMapRect original_aspect_fit_rect(
    OriginalMapRect container,
    OriginalMapRect content) noexcept;

// Static translation of 1160:0000, 1080:093a, 1160:01dc, 11d0:0254,
// and 11d0:0363. Artwork and palette indices come directly from the original
// BITMAP/310..315, BITMAP/352, and CLUT/1000 resources.
[[nodiscard]] OriginalWorldRaster render_original_map(
    const OriginalResources& resources,
    const OriginalTdtDocument* document,
    std::uint16_t mode,
    bool disabled = false,
    const OriginalWorldPalette* palette_override = nullptr);

// Exact 1090:046f-047c cadence for 1080:09c3's synchronous Map repaint.
// BITMAP/352's horizontal phase advances only at these sixteen-tick edges.
[[nodiscard]] constexpr bool original_map_animation_refresh_due(
    std::uint16_t frame_time) noexcept {
  return frame_time % 16U == 0U;
}

// Exact 1080:038e world-client to map transform. The returned rectangle is in
// the 200x306 backing bitmap's coordinates; 1058:094c adds client y=8 before
// passing it to DrawFocusRect.
[[nodiscard]] OriginalMapRect original_map_view_rect(
    int view_x,
    int view_y,
    int main_client_width,
    int main_client_height) noexcept;

// Exact 1080:0054 keep-pointer-visible transform used after a successful
// captured construction drag. A nonzero horizontal_axis adjusts only x;
// otherwise only y changes. Points on either client boundary are visible and
// therefore leave the view unchanged. The caller performs 1080:00d7's range
// clamp when it commits the returned scrollbar position.
[[nodiscard]] OriginalWorldPoint original_keep_pointer_visible(
    OriginalWorldPoint view,
    OriginalMapRect client,
    OriginalWorldPoint pointer,
    bool horizontal_axis) noexcept;

// Exact 1058:064e -> 11e0:0ab2 -> 1080:0440 map-drag transform. map_y is
// in backing-bitmap coordinates (the caller removes the client y=8 inset).
[[nodiscard]] OriginalWorldPoint original_map_centered_view(
    int map_x,
    int map_y,
    int current_view_x,
    int current_view_y,
    int main_client_width,
    int main_client_height) noexcept;

}  // namespace simtower
