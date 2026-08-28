#include "original_dtmp_runtime.hpp"

#include "original_dib.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace simtower {
namespace {

// RAII counterpart of 11e0:04c0/05d7: create and select the drawing objects,
// then restore the previous objects and delete the temporary ones.
class SelectedPen {
 public:
  SelectedPen(HDC dc, COLORREF color) : dc_(dc) {
    pen_ = CreatePen(PS_SOLID, 1, color);
    if (!pen_) {
      throw std::runtime_error("Could not create original dialog pen");
    }
    previous_ = SelectObject(dc_, pen_);
  }
  ~SelectedPen() {
    if (previous_) {
      SelectObject(dc_, previous_);
    }
    if (pen_) {
      DeleteObject(pen_);
    }
  }

  SelectedPen(const SelectedPen&) = delete;
  SelectedPen& operator=(const SelectedPen&) = delete;

 private:
  HDC dc_ = nullptr;
  HPEN pen_ = nullptr;
  HGDIOBJ previous_ = nullptr;
};

void line(HDC dc, int x1, int y1, int x2, int y2) {
  MoveToEx(dc, x1, y1, nullptr);
  LineTo(dc, x2, y2);
}

void select_gray_and_draw_outer_top_left(HDC dc, const RECT& rect,
                                         BYTE gray) {
  SelectedPen pen(dc, RGB(gray, gray, gray));
  for (int inset = 0; inset < 3; ++inset) {
    MoveToEx(dc, rect.right - 2 - inset, rect.top + inset, nullptr);
    LineTo(dc, rect.left + inset, rect.top + inset);
    LineTo(dc, rect.left + inset, rect.bottom - 2 - inset);
  }
}

void select_gray_and_draw_outer_bottom_right(HDC dc, const RECT& rect,
                                              BYTE gray) {
  SelectedPen pen(dc, RGB(gray, gray, gray));
  for (int inset = 0; inset < 3; ++inset) {
    MoveToEx(dc, rect.left + inset, rect.bottom - 1 - inset, nullptr);
    LineTo(dc, rect.right - 1 - inset, rect.bottom - 1 - inset);
    LineTo(dc, rect.right - 1 - inset, rect.top + inset);
  }
}

void select_gray_and_draw_inner_top_left(HDC dc, const RECT& rect,
                                         BYTE gray) {
  SelectedPen pen(dc, RGB(gray, gray, gray));
  for (int inset = 0; inset < 3; ++inset) {
    MoveToEx(dc, rect.right - 1 - inset, rect.top + inset, nullptr);
    LineTo(dc, rect.left + inset, rect.top + inset);
    LineTo(dc, rect.left + inset, rect.bottom - 1 - inset);
  }
}

void select_gray_and_draw_inner_bottom_right(HDC dc, const RECT& rect,
                                              BYTE gray) {
  SelectedPen pen(dc, RGB(gray, gray, gray));
  for (int inset = 0; inset < 3; ++inset) {
    MoveToEx(dc, rect.left + inset + 1, rect.bottom - 1 - inset, nullptr);
    LineTo(dc, rect.right - 1 - inset, rect.bottom - 1 - inset);
    LineTo(dc, rect.right - 1 - inset, rect.top + inset + 1);
  }
}

void fill_gray(HDC dc, const RECT& rect, BYTE gray) {
  // Native direct-color equivalent of 11e0:0e22/0e60's palette-aware
  // create/fill/restore sequence.
  HBRUSH brush = CreateSolidBrush(RGB(gray, gray, gray));
  if (!brush) {
    throw std::runtime_error("Could not create original dialog brush");
  }
  FillRect(dc, &rect, brush);
  DeleteObject(brush);
}

// Direct translation of 11e0:06d9. The color arguments passed through
// 11e0:0e00 resolve to 179, 140, 102, 230, and 204 in that order.
void draw_original_dialog_frame(HDC dc, RECT rect) {
  select_gray_and_draw_outer_top_left(dc, rect, 179);
  select_gray_and_draw_outer_bottom_right(dc, rect, 140);
  // 1208:00dc is the original negative-argument InflateRect wrapper.
  InflateRect(&rect, -3, -3);
  select_gray_and_draw_inner_top_left(dc, rect, 102);
  select_gray_and_draw_inner_bottom_right(dc, rect, 230);
  InflateRect(&rect, -3, -3);
  fill_gray(dc, rect, 204);
}

// Direct translation of 11e0:0950, used only when a DTMP rectangle does not
// correspond to an actual dialog child.
void draw_original_placeholder_frame(HDC dc, RECT rect) {
  {
    SelectedPen pen(dc, RGB(89, 89, 89));
    for (int inset = 0; inset < 2; ++inset) {
      MoveToEx(dc, rect.right - 1 - inset, rect.top + inset, nullptr);
      LineTo(dc, rect.left + inset, rect.top + inset);
      LineTo(dc, rect.left + inset, rect.bottom - 2 - inset);
    }
  }
  {
    SelectedPen pen(dc, RGB(255, 255, 255));
    for (int inset = 0; inset < 2; ++inset) {
      MoveToEx(dc, rect.left + inset, rect.bottom - 1 - inset, nullptr);
      LineTo(dc, rect.right - 1 - inset, rect.bottom - 1 - inset);
      LineTo(dc, rect.right - 1 - inset, rect.top + inset + 1);
    }
  }
  InflateRect(&rect, -2, -2);
  fill_gray(dc, rect, 230);
}

RECT native_rect(const OriginalDtmpRect& source) {
  return {static_cast<LONG>(source.left), static_cast<LONG>(source.top),
          static_cast<LONG>(source.right), static_cast<LONG>(source.bottom)};
}

}  // namespace

OriginalDtmpWindowSize original_dtmp_window_size(
    const OriginalDtmp& dtmp,
    const OriginalResources& resources) {
  if (dtmp.bitmap_resource_id >= 0) {
    const auto dib = original_dib_view(
        resources.find("BITMAP", dtmp.bitmap_resource_id));
    return {static_cast<int>(dib.width), std::abs(static_cast<int>(dib.height))};
  }
  return {static_cast<int>(dtmp.width_or_header),
          static_cast<int>(dtmp.height_or_header)};
}

void configure_original_dtmp_window(HWND dialog,
                                    const OriginalDtmp& dtmp,
                                    const OriginalResources& resources,
                                    HPALETTE logical_palette) {
  const auto size = original_dtmp_window_size(dtmp, resources);
  const auto plan = original_dtmp_configuration_plan(dtmp);
  const auto resize_window = [&] {
    if (size.width == 0) return;
    RECT current{};
    GetWindowRect(dialog, &current);
    MoveWindow(dialog, current.left, current.top, size.width, size.height, FALSE);
  };

  if (plan.resize_before_dc) resize_window();

  HDC dc = GetDC(dialog);
  if (dc) {
    if (plan.realize_palette && logical_palette) {
      // 1070:0185-019b selects DS:795e, runs 11e0:0e84's obsolete WAVMIX
      // message pump, then realizes the palette. waveOut needs no pump.
      SelectPalette(dc, logical_palette, FALSE);
      RealizePalette(dc);
    }
    if (plan.set_update_current_position) {
      SetTextAlign(dc, GetTextAlign(dc) | TA_UPDATECP);
    }
    ReleaseDC(dialog, dc);
  }

  if (plan.resize_after_dc) resize_window();
}

void render_original_dtmp(HWND dialog,
                          HDC destination,
                          const OriginalDtmp& dtmp,
                          const OriginalResources& resources) {
  if (dtmp.bitmap_resource_id >= 0 && destination) {
    draw_original_dib(destination,
                      resources.find("BITMAP", dtmp.bitmap_resource_id), 0, 0);
  }

  for (std::size_t index = 0; index < dtmp.rectangles.size(); ++index) {
    const auto& rect = dtmp.rectangles[index];
    if (rect.left == 0) {
      continue;
    }
    if (rect.bottom == 0) {
      if (destination) {
        draw_original_dib(destination, resources.find("BITMAP", rect.right),
                          rect.left, rect.top);
      }
      continue;
    }

    HWND child = GetDlgItem(dialog, static_cast<int>(index + 1U));
    if (!IsWindow(child)) {
      continue;
    }
    MoveWindow(child, rect.left, rect.top,
               static_cast<int>(rect.right) - rect.left,
               static_cast<int>(rect.bottom) - rect.top, TRUE);
    ShowWindow(child, SW_SHOW);
  }
}

void paint_original_dialog_chrome(HWND dialog,
                                  HDC destination,
                                  const OriginalDtmp& dtmp) {
  RECT client{};
  GetClientRect(dialog, &client);
  draw_original_dialog_frame(destination, client);

  for (std::size_t index = 0; index < dtmp.rectangles.size(); ++index) {
    RECT rect = native_rect(dtmp.rectangles[index]);
    if (IsRectEmpty(&rect)) {
      continue;
    }
    if (!IsWindow(GetDlgItem(dialog, static_cast<int>(index + 1U)))) {
      draw_original_placeholder_frame(destination, rect);
    }
  }
  // Exact result of the palette-aware 11e0:0633 SetBkColor wrapper used by
  // the 1068:0567 DTMP renderer: the dialog drawing background is white.
  SetBkColor(destination, RGB(255, 255, 255));
}

}  // namespace simtower
