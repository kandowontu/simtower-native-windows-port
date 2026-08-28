#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <span>

namespace simtower {

struct OriginalDibView {
  const BITMAPINFO* info = nullptr;
  std::span<const std::byte> pixels{};
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::uint16_t bit_count = 0;
};

struct OriginalOpaqueBlitPlan {
  int source_x{};
  int source_y{};
  int destination_x{};
  int destination_y{};
  int width{};
  int height{};
  bool valid{};

  friend bool operator==(const OriginalOpaqueBlitPlan&,
                         const OriginalOpaqueBlitPlan&) = default;
};

// 1208:069a converts a source RECT and destination POINT into the equal-size
// opaque (-1 marker) 1248:0000/1250:0114 byte-copy path. 1248 clips only the
// destination rectangle and advances the source origin by the clipped-away
// left/top amount; its callers supply an already-valid source rectangle.
[[nodiscard]] constexpr OriginalOpaqueBlitPlan original_opaque_blit_plan(
    int destination_width,
    int destination_height,
    int source_left,
    int source_top,
    int width,
    int height,
    int destination_x,
    int destination_y) noexcept {
  const int clipped_left = destination_x < 0 ? 0 : destination_x;
  const int clipped_top = destination_y < 0 ? 0 : destination_y;
  const int requested_right = destination_x + width;
  const int requested_bottom = destination_y + height;
  const int clipped_right = requested_right < destination_width
                                ? requested_right
                                : destination_width;
  const int clipped_bottom = requested_bottom < destination_height
                                 ? requested_bottom
                                 : destination_height;
  if (width <= 0 || height <= 0 || clipped_right <= clipped_left ||
      clipped_bottom <= clipped_top) {
    return {};
  }
  return {
      source_left + clipped_left - destination_x,
      source_top + clipped_top - destination_y,
      clipped_left,
      clipped_top,
      clipped_right - clipped_left,
      clipped_bottom - clipped_top,
      true,
  };
}

// Exact resource shape consumed by 1030:0043 and 1208:049d/0529: a Win3
// BITMAPINFOHEADER, its palette, then packed DIB scanlines.
[[nodiscard]] OriginalDibView original_dib_view(
    std::span<const std::byte> resource);

// Native SetDIBitsToDevice equivalent of original routine 1208:0529.
void draw_original_dib(HDC destination,
                       std::span<const std::byte> resource,
                       int x,
                       int y);

// Source rectangle is expressed in the original bitmap's top-left client
// coordinates. This is the native partial-blit equivalent needed by the
// Finance dialog's BITMAP/500 -> BITMAP/501 pressed-button transition.
void draw_original_dib_region(HDC destination,
                              std::span<const std::byte> resource,
                              int destination_x,
                              int destination_y,
                              int source_left,
                              int source_top,
                              int width,
                              int height);

// WinGStretchBlt/WinGBitBlt-equivalent source-region path used by the
// original Person Information portrait painter at 1100:364a through its
// direct 1100:37a9/37d1 calls. Source coordinates remain in top-left DIB
// space while destination dimensions may differ.
void draw_original_dib_region_scaled(HDC destination,
                                     std::span<const std::byte> resource,
                                     int destination_x,
                                     int destination_y,
                                     int destination_width,
                                     int destination_height,
                                     int source_left,
                                     int source_top,
                                     int source_width,
                                     int source_height);

}  // namespace simtower
