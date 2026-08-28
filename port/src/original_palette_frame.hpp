#pragma once

#include <windows.h>

#include <cstdint>

namespace simtower {

inline constexpr int kOriginalPaletteFrameHeight = 8;

struct OriginalPaletteFrameRect {
  int left{};
  int top{};
  int right{};
  int bottom{};

  [[nodiscard]] constexpr bool contains(int x, int y) const noexcept {
    return x >= left && y >= top && x < right && y < bottom;
  }

  friend bool operator==(const OriginalPaletteFrameRect&,
                         const OriginalPaletteFrameRect&) = default;
};

enum class OriginalPaletteFrameHit : std::uint8_t {
  client,
  close,
  drag,
};

// 1078:00c6 offsets the 0,0,width,8 bar by (4,1), then truncates that
// temporary rectangle to a 6x6 close box.
[[nodiscard]] constexpr OriginalPaletteFrameRect
original_palette_frame_close_rect() noexcept {
  return {4, 1, 10, 7};
}

// Exact WM_NCHITTEST split shared by CMDBTNWNDPROC, INFOWNDPROC, and
// MAPWNDPROC: the close box remains HTCLIENT, the rest of y<8 is HTCAPTION,
// and content is left to the ordinary client procedure.
[[nodiscard]] constexpr OriginalPaletteFrameHit
original_palette_frame_hit_test(int x, int y) noexcept {
  if (y >= kOriginalPaletteFrameHeight) {
    return OriginalPaletteFrameHit::client;
  }
  return original_palette_frame_close_rect().contains(x, y)
             ? OriginalPaletteFrameHit::close
             : OriginalPaletteFrameHit::drag;
}

// Exact active/inactive eight-pixel frame painter at 1078:00c6. Active uses
// COLOR_ACTIVECAPTION plus a white close square; inactive uses a white bar and
// a one-pixel black outline around the close square.
void draw_original_palette_frame(HDC destination, int width, bool active);

}  // namespace simtower
