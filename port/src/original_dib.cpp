#include "original_dib.hpp"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace simtower {

OriginalDibView original_dib_view(std::span<const std::byte> resource) {
  if (resource.size() < sizeof(BITMAPINFOHEADER)) {
    throw std::runtime_error("Truncated original bitmap header");
  }

  BITMAPINFOHEADER header{};
  std::memcpy(&header, resource.data(), sizeof(header));
  if (header.biSize != sizeof(BITMAPINFOHEADER) || header.biPlanes != 1U ||
      header.biWidth <= 0 || header.biHeight == 0 ||
      header.biCompression != BI_RGB) {
    throw std::runtime_error("Unsupported original bitmap format");
  }

  std::uint32_t color_count = header.biClrUsed;
  if (color_count == 0U && header.biBitCount <= 8U) {
    color_count = 1U << header.biBitCount;
  }
  const std::size_t pixel_offset =
      static_cast<std::size_t>(header.biSize) +
      static_cast<std::size_t>(color_count) * sizeof(RGBQUAD);
  if (pixel_offset > resource.size()) {
    throw std::runtime_error("Truncated original bitmap palette");
  }

  const std::uint64_t row_bits =
      static_cast<std::uint64_t>(header.biWidth) * header.biBitCount;
  const std::uint64_t row_bytes = ((row_bits + 31U) / 32U) * 4U;
  const std::uint64_t pixel_size =
      row_bytes * static_cast<std::uint64_t>(std::abs(header.biHeight));
  if (pixel_size > resource.size() - pixel_offset) {
    throw std::runtime_error("Truncated original bitmap scanlines");
  }

  return {
      reinterpret_cast<const BITMAPINFO*>(resource.data()),
      resource.subspan(pixel_offset, static_cast<std::size_t>(pixel_size)),
      header.biWidth,
      header.biHeight,
      header.biBitCount,
  };
}

void draw_original_dib(HDC destination,
                       std::span<const std::byte> resource,
                       int x,
                       int y) {
  const auto dib = original_dib_view(resource);
  const int height = std::abs(dib.height);
  const int result = SetDIBitsToDevice(
      destination, x, y, static_cast<DWORD>(dib.width),
      static_cast<DWORD>(height), 0, 0, 0, static_cast<UINT>(height),
      dib.pixels.data(), dib.info, DIB_RGB_COLORS);
  if (result == 0) {
    throw std::runtime_error("SetDIBitsToDevice failed for original bitmap");
  }
}

void draw_original_dib_region(HDC destination,
                              std::span<const std::byte> resource,
                              int destination_x,
                              int destination_y,
                              int source_left,
                              int source_top,
                              int width,
                              int height) {
  const auto dib = original_dib_view(resource);
  const int dib_height = std::abs(dib.height);
  if (width <= 0 || height <= 0 || source_left < 0 || source_top < 0 ||
      source_left > dib.width - width || source_top > dib_height - height) {
    throw std::runtime_error("Original bitmap source rectangle is invalid");
  }
  const int source_y = dib.height > 0
      ? dib_height - source_top - height
      : source_top;
  const int result = StretchDIBits(
      destination, destination_x, destination_y, width, height,
      source_left, source_y, width, height, dib.pixels.data(), dib.info,
      DIB_RGB_COLORS, SRCCOPY);
  if (result == 0 || result == GDI_ERROR) {
    throw std::runtime_error(
        "StretchDIBits failed for original bitmap region");
  }
}

void draw_original_dib_region_scaled(HDC destination,
                                     std::span<const std::byte> resource,
                                     int destination_x,
                                     int destination_y,
                                     int destination_width,
                                     int destination_height,
                                     int source_left,
                                     int source_top,
                                     int source_width,
                                     int source_height) {
  // Native 32-bit DIB presentation equivalent of 1100:37a9/37d1's direct
  // WinGStretchBlt/WinGBitBlt paths. The source rectangle, destination
  // rectangle, opaque SRCCOPY behavior, clipping, and COLORONCOLOR scaling
  // are retained; only the obsolete WinG surface transport is eliminated.
  const auto dib = original_dib_view(resource);
  const int dib_height = std::abs(dib.height);
  if (destination_width <= 0 || destination_height <= 0 ||
      source_width <= 0 || source_height <= 0 || source_left < 0 ||
      source_top < 0 || source_left > dib.width - source_width ||
      source_top > dib_height - source_height) {
    throw std::runtime_error("Original bitmap scaled source rectangle is invalid");
  }
  const int source_y = dib.height > 0
      ? dib_height - source_top - source_height
      : source_top;
  const int old_mode = SetStretchBltMode(destination, COLORONCOLOR);
  const int result = StretchDIBits(
      destination, destination_x, destination_y,
      destination_width, destination_height,
      source_left, source_y, source_width, source_height,
      dib.pixels.data(), dib.info, DIB_RGB_COLORS, SRCCOPY);
  if (old_mode != 0) SetStretchBltMode(destination, old_mode);
  if (result == 0 || result == GDI_ERROR) {
    throw std::runtime_error(
        "StretchDIBits failed for scaled original bitmap region");
  }
}

}  // namespace simtower
