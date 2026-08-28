#include "original_map.hpp"

#include "original_dib.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <stdexcept>

namespace simtower {
namespace {

struct IndexedDib {
  OriginalDibView view{};
  int height{};
  std::size_t row_bytes{};

  explicit IndexedDib(std::span<const std::byte> resource)
      : view(original_dib_view(resource)),
        height(std::abs(view.height)),
        row_bytes((static_cast<std::size_t>(view.width) + 3U) & ~3U) {
    if (view.bit_count != 8U) {
      throw std::runtime_error("Original map bitmap is not 8-bit indexed");
    }
  }

  [[nodiscard]] std::uint8_t sample_index(int x, int y) const {
    if (x < 0 || y < 0 || x >= view.width || y >= height) return 0U;
    const int source_y = view.height > 0 ? height - 1 - y : y;
    return std::to_integer<std::uint8_t>(
        view.pixels[static_cast<std::size_t>(source_y) * row_bytes +
                    static_cast<std::size_t>(x)]);
  }
};

int signed_scale(int value) noexcept {
  // 1080:04b0 uses signed 32-bit IDIV by DS:71e0 (4320) after multiplying by
  // DS:7794-DS:7790 (288). C++ integer division has the same truncation toward
  // zero required here.
  return static_cast<int>((static_cast<std::int64_t>(value) *
                           kOriginalMapContentHeight) /
                          kOriginalWorldHeight);
}

void put(OriginalWorldRaster& raster, int x, int y, std::uint32_t color) {
  if (x < 0 || y < 0 || x >= raster.width || y >= raster.height) return;
  raster.pixels[static_cast<std::size_t>(y) * raster.width + x] = color;
}

void fill_rect(OriginalWorldRaster& raster,
               const OriginalMapRect& rectangle,
               std::uint32_t color) {
  const int left = std::clamp(rectangle.left, 0, raster.width);
  const int top = std::clamp(rectangle.top, 0, raster.height);
  const int right = std::clamp(rectangle.right, 0, raster.width);
  const int bottom = std::clamp(rectangle.bottom, 0, raster.height);
  for (int y = top; y < bottom; ++y) {
    std::fill_n(raster.pixels.begin() +
                    static_cast<std::size_t>(y) * raster.width + left,
                std::max(0, right - left), color);
  }
}

OriginalMapRect map_world_rect(int left, int top, int right, int bottom) {
  return {signed_scale(left),
          signed_scale(top) + kOriginalMapToolbarHeight,
          signed_scale(right),
          signed_scale(bottom) + kOriginalMapToolbarHeight};
}

void blit_indexed(const IndexedDib& source,
                  int source_x,
                  int source_y,
                  int width,
                  int height,
                  int destination_x,
                  int destination_y,
                  const std::array<std::uint32_t, 256>& palette,
                  OriginalWorldRaster& raster) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      put(raster, destination_x + x, destination_y + y,
          palette[source.sample_index(source_x + x, source_y + y)]);
    }
  }
}

std::uint16_t load_runtime_word(std::span<const std::byte> bytes,
                                std::size_t offset,
                                bool byte_swapped) noexcept {
  if (offset + 1U >= bytes.size()) return 0U;
  const auto first = std::to_integer<std::uint8_t>(bytes[offset]);
  const auto second = std::to_integer<std::uint8_t>(bytes[offset + 1U]);
  return byte_swapped
      ? static_cast<std::uint16_t>((first << 8U) | second)
      : static_cast<std::uint16_t>(first | (second << 8U));
}

void render_background(const OriginalResources& resources,
                       std::uint16_t frame_time,
                       const std::array<std::uint32_t, 256>& palette,
                       OriginalWorldRaster& raster) {
  const IndexedDib background(resources.find("BITMAP", 352));
  if (background.view.width != kOriginalMapWidth ||
      background.height != kOriginalMapContentHeight) {
    throw std::runtime_error("Original BITMAP/352 has an unexpected shape");
  }

  // 1160:01e9-0340: ((signed b3de >> 4) % 20) * 10 cyclically shifts the
  // upper 264 rows. The bottom 24 rows remain fixed.
  const auto signed_clock = std::bit_cast<std::int16_t>(frame_time);
  const int phase = ((signed_clock >> 4) % 20) * 10;
  for (int y = 0; y < kOriginalMapContentHeight; ++y) {
    for (int x = 0; x < kOriginalMapWidth; ++x) {
      int source_x = x;
      if (y < kOriginalMapContentHeight - 24) {
        source_x = (x + phase) % kOriginalMapWidth;
        if (source_x < 0) source_x += kOriginalMapWidth;
      }
      put(raster, x, y + kOriginalMapToolbarHeight,
          palette[background.sample_index(source_x, y)]);
    }
  }
}

void render_toolbar(const OriginalResources& resources,
                    std::uint16_t mode,
                    bool disabled,
                    const std::array<std::uint32_t, 256>& palette,
                    OriginalWorldRaster& raster) {
  const IndexedDib ordinary(resources.find("BITMAP", 310));
  const IndexedDib selected(resources.find("BITMAP", 311));
  const IndexedDib inactive(resources.find("BITMAP", 312));
  for (int button = 0; button < 4; ++button) {
    const IndexedDib& source = mode == static_cast<std::uint16_t>(button)
        ? selected
        : (disabled ? inactive : ordinary);
    blit_indexed(source, button * 50, 0, 50, kOriginalMapToolbarHeight,
                 button * 50, 0, palette, raster);
  }
}

void render_floor_bands(const OriginalTdtDocument& document,
                        OriginalWorldRaster& raster) {
  constexpr std::uint32_t kOccupiedFloor = 0x00ccccccU;
  for (std::size_t floor_index = 0; floor_index < document.floors.size();
       ++floor_index) {
    const auto& floor = document.floors[floor_index];
    if (floor.tenants.empty()) continue;
    const int top = (119 - static_cast<int>(floor_index)) *
                    kOriginalFloorHeight;
    fill_rect(raster,
              map_world_rect(static_cast<int>(floor.left_edge) *
                                 kOriginalCellWidth,
                             top,
                             static_cast<int>(floor.right_edge) *
                                 kOriginalCellWidth,
                             top + kOriginalFloorHeight),
              kOccupiedFloor);
  }
}

std::uint32_t tenant_mode_color(const OriginalTdtTenant& tenant,
                                std::uint16_t mode) noexcept {
  constexpr std::uint32_t kRed = 0x00e60000U;
  constexpr std::uint32_t kYellow = 0x00cccc00U;
  constexpr std::uint32_t kGreen = 0x0000ff00U;
  constexpr std::uint32_t kCyan = 0x0000ffffU;
  if (mode == 1U) {
    const auto state = std::bit_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(tenant.preserved_07_to_0f[8]));
    switch (state) {
      case 0: return kRed;
      case 1: return kYellow;
      case 2:
      case 3: return kCyan;
      default: return 0U;
    }
  }
  if (mode == 2U) {
    switch (tenant.rent_rate) {
      case 0: return kRed;
      case 1: return kYellow;
      case 2: return kGreen;
      case 3: return kCyan;
      default: return 0U;
    }
  }
  if (mode == 3U && tenant.type >= 3 && tenant.type <= 5 &&
      std::bit_cast<std::int8_t>(tenant.status) >= 0x28) {
    return kRed;
  }
  return 0U;
}

void render_tenant_mode(const OriginalTdtDocument& document,
                        std::uint16_t mode,
                        OriginalWorldRaster& raster) {
  if (mode == 0U) return;
  for (std::size_t floor_index = 0; floor_index < document.floors.size();
       ++floor_index) {
    const int top = (119 - static_cast<int>(floor_index)) *
                    kOriginalFloorHeight;
    for (const auto& tenant : document.floors[floor_index].tenants) {
      const std::uint32_t color = tenant_mode_color(tenant, mode);
      if (color == 0U) continue;
      fill_rect(raster,
                map_world_rect(static_cast<int>(tenant.left) *
                                   kOriginalCellWidth,
                               top,
                               static_cast<int>(tenant.right) *
                                   kOriginalCellWidth,
                               top + kOriginalFloorHeight),
                color);
    }
  }
}

void render_elevators(const OriginalTdtDocument& document,
                      OriginalWorldRaster& raster) {
  for (const auto& elevator : document.elevators) {
    if (elevator.used == 0U) continue;
    const int world_x = static_cast<int>(elevator.x) * kOriginalCellWidth;
    const int world_top =
        (120 - static_cast<int>(elevator.top_floor) - 2) *
        kOriginalFloorHeight;
    const int world_bottom =
        (120 - static_cast<int>(elevator.bottom_floor)) *
        kOriginalFloorHeight;
    const auto line = map_world_rect(world_x, world_top, world_x,
                                     world_bottom);
    const std::uint32_t color = elevator.type == 0U
        ? 0x000000ffU
        : (elevator.type == 2U ? 0x00ff0000U : 0x00000000U);
    // Win16 LineTo draws the initial point but excludes the final endpoint.
    for (int y = line.top; y < line.bottom; ++y) {
      put(raster, line.left, y, color);
    }
  }
}

void render_annual_effect(const OriginalTdtDocument& document,
                          OriginalWorldRaster& raster) {
  const auto& state = document.post_elevator.version_18_dd6c;
  if (state.size() < 8U || state[0] == std::byte{0}) return;
  const int world_x = std::bit_cast<std::int16_t>(
      load_runtime_word(state, 4U, document.header.byte_swapped));
  const int world_y = std::bit_cast<std::int16_t>(
      load_runtime_word(state, 6U, document.header.byte_swapped));
  auto rectangle = map_world_rect(world_x, world_y, world_x, world_y);
  --rectangle.left;
  --rectangle.top;
  rectangle.right += 2;
  rectangle.bottom += 2;
  fill_rect(raster, rectangle, 0x00ff0000U);
}

void render_mode_legend(const OriginalResources& resources,
                        std::uint16_t mode,
                        const std::array<std::uint32_t, 256>& palette,
                        OriginalWorldRaster& raster) {
  if (mode == 0U || mode > 3U) return;
  const IndexedDib legend(resources.find("BITMAP", 312 + mode));
  // 1160:059f-060a builds RECT(0,0,biWidth,biHeight), then applies
  // OffsetRect(200 - rect.right, 18). BITMAP/313 and /314 therefore cover the
  // full 200-pixel row, while the 81-pixel BITMAP/315 starts at x=119.
  blit_indexed(legend, 0, 0, legend.view.width, legend.height,
               kOriginalMapWidth - legend.view.width,
               kOriginalMapToolbarHeight, palette, raster);
}

}  // namespace

OriginalMapRect original_aspect_fit_rect(OriginalMapRect container,
                                         OriginalMapRect content) noexcept {
  // 1080:0218-025a retains the container origin, then normalizes both RECTs;
  // shared 1208:00b5 returns the paired signed dimensions represented here by
  // the normalized native rectangle.
  const int origin_x = container.left;
  const int origin_y = container.top;
  const int container_width = container.right - container.left;
  const int container_height = container.bottom - container.top;
  const int content_width = content.right - content.left;
  const int content_height = content.bottom - content.top;
  if (container_width <= 0 || container_height <= 0 ||
      content_width < 0 || content_height < 0) {
    return container;
  }

  container = {0, 0, container_width, container_height};
  content = {0, 0, content_width, content_height};

  // 1080:025f-02b1 compares integer percentages. The valid original call
  // domain has nonzero content dimensions; a zero dimension already fits and
  // therefore takes the literal 100 branch without division.
  const int horizontal_percent = container_width >= content_width
      ? 100
      : static_cast<int>((static_cast<std::int64_t>(content_width) * 100) /
                         container_width);
  const int vertical_percent = container_height >= content_height
      ? 100
      : static_cast<int>((static_cast<std::int64_t>(content_height) * 100) /
                         container_height);

  OriginalMapRect result = content;
  if (horizontal_percent > vertical_percent) {
    // 1080:02ce-02fa constrains width and inversely scales height.
    result.right = container_width;
    result.bottom = static_cast<int>(
        (static_cast<std::int64_t>(content_height) * 100) /
        horizontal_percent);
  } else if (horizontal_percent < vertical_percent) {
    // 1080:0306-0332 constrains height and inversely scales width.
    result.bottom = container_height;
    result.right = static_cast<int>(
        (static_cast<std::int64_t>(content_width) * 100) /
        vertical_percent);
  } else if (horizontal_percent != 100) {
    // Equal non-100 percentages mean that both axes fill the container.
    result = container;
  }

  // 1080:0354-0380 centers the fitted rectangle at the retained origin.
  // Native signed division truncates toward zero, matching the x86
  // CWD/SUB/SAR sequence for both positive and negative odd deltas.
  const int offset_x = origin_x + (container_width - result.right) / 2;
  const int offset_y = origin_y + (container_height - result.bottom) / 2;
  result.left += offset_x;
  result.right += offset_x;
  result.top += offset_y;
  result.bottom += offset_y;
  return result;
}

OriginalWorldRaster render_original_map(const OriginalResources& resources,
                                        const OriginalTdtDocument* document,
                                        std::uint16_t mode,
                                        bool disabled,
                                        const OriginalWorldPalette*
                                            palette_override) {
  OriginalWorldRaster raster{};
  raster.width = kOriginalMapWidth;
  raster.height = kOriginalMapBackingHeight;
  raster.pixels.assign(
      static_cast<std::size_t>(raster.width) * raster.height, 0U);
  const auto derived_palette = palette_override
      ? OriginalWorldPalette{}
      : original_world_palette(resources, document);
  const auto& palette = palette_override ? *palette_override : derived_palette;
  render_background(resources, document ? document->header.frame_time : 0U,
                    palette, raster);
  render_toolbar(resources, mode, disabled, palette, raster);
  if (document) {
    render_floor_bands(*document, raster);
    render_tenant_mode(*document, mode, raster);
    render_elevators(*document, raster);
    render_annual_effect(*document, raster);
  }
  render_mode_legend(resources, mode, palette, raster);
  return raster;
}

OriginalMapRect original_map_view_rect(int view_x,
                                       int view_y,
                                       int main_client_width,
                                       int main_client_height) noexcept {
  return {
      signed_scale(view_x),
      signed_scale(view_y) + kOriginalMapToolbarHeight,
      signed_scale(view_x + main_client_width),
      signed_scale(view_y + main_client_height) + kOriginalMapToolbarHeight,
  };
}

OriginalWorldPoint original_keep_pointer_visible(
    OriginalWorldPoint view,
    OriginalMapRect client,
    OriginalWorldPoint pointer,
    bool horizontal_axis) noexcept {
  if (horizontal_axis) {
    if (pointer.x < client.left) {
      view.x -= client.left - pointer.x;
    } else if (pointer.x > client.right) {
      view.x += pointer.x - client.right;
    }
  } else if (pointer.y < client.top) {
    view.y -= client.top - pointer.y;
  } else if (pointer.y > client.bottom) {
    view.y += pointer.y - client.bottom;
  }
  return view;
}

OriginalWorldPoint original_map_centered_view(int map_x,
                                              int map_y,
                                              int current_view_x,
                                              int current_view_y,
                                              int main_client_width,
                                              int main_client_height) noexcept {
  const auto current_rectangle = original_map_view_rect(
      current_view_x, current_view_y, main_client_width, main_client_height);
  auto rectangle = current_rectangle;
  const int width = rectangle.right - rectangle.left;
  const int height = rectangle.bottom - rectangle.top;
  rectangle.left = map_x - width / 2;
  rectangle.right = rectangle.left + width;
  rectangle.top = map_y - height / 2;
  rectangle.bottom = rectangle.top + height;

  // 11e0:0ab2 clips by offsetting the complete rectangle, preserving its
  // transformed size instead of clipping an edge independently.
  if (rectangle.left < 0) {
    rectangle.right -= rectangle.left;
    rectangle.left = 0;
  }
  if (rectangle.right > kOriginalMapWidth) {
    const int delta = kOriginalMapWidth - rectangle.right;
    rectangle.left += delta;
    rectangle.right += delta;
  }
  if (rectangle.top < kOriginalMapToolbarHeight) {
    const int delta = kOriginalMapToolbarHeight - rectangle.top;
    rectangle.top += delta;
    rectangle.bottom += delta;
  }
  if (rectangle.bottom > kOriginalMapBackingHeight) {
    const int delta = kOriginalMapBackingHeight - rectangle.bottom;
    rectangle.top += delta;
    rectangle.bottom += delta;
  }

  // 1058:06df calls EQUALRECT against DS:7796 before committing the drag.
  // Retain the exact raw scroll position when the map focus rectangle did not
  // move; applying 1080:0440's inverse transform here would otherwise round an
  // odd-pixel view down to a 15-pixel map quantum and provoke an extra paint.
  if (rectangle == current_rectangle) {
    return {current_view_x, current_view_y};
  }

  // 1080:0440 applies the inverse ratios 3000/200 and 4320/288.
  return {
      rectangle.left * (kOriginalWorldWidth / kOriginalMapWidth),
      (rectangle.top - kOriginalMapToolbarHeight) *
          (kOriginalWorldHeight / kOriginalMapContentHeight),
  };
}

}  // namespace simtower
