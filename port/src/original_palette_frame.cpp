#include "original_palette_frame.hpp"

#include <stdexcept>

namespace simtower {

void draw_original_palette_frame(HDC destination, int width, bool active) {
  if (!destination || width <= 0) return;
  const RECT bar{0, 0, width, kOriginalPaletteFrameHeight};
  const auto close_geometry = original_palette_frame_close_rect();
  const RECT close{close_geometry.left, close_geometry.top,
                   close_geometry.right, close_geometry.bottom};

  if (!active) {
    FillRect(destination, &bar,
             static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    FrameRect(destination, &close,
              static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    return;
  }

  HBRUSH caption = CreateSolidBrush(GetSysColor(COLOR_ACTIVECAPTION));
  if (!caption) {
    throw std::runtime_error("CreateSolidBrush failed for palette frame");
  }
  FillRect(destination, &bar, caption);
  FillRect(destination, &close,
           static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
  DeleteObject(caption);
}

}  // namespace simtower
