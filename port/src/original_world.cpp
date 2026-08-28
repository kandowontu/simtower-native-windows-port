#include "original_world.hpp"

#include "original_dib.hpp"
#include "original_simulation.hpp"
#include "original_tables.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

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
      throw std::runtime_error("Original world bitmap is not 8-bit indexed");
    }
  }

  [[nodiscard]] std::uint32_t sample(int x, int y) const {
    if (x < 0 || y < 0 || x >= view.width || y >= height) {
      return 0x00ffffffU;
    }
    const auto index = sample_index(x, y);
    const RGBQUAD color = view.info->bmiColors[index];
    return (static_cast<std::uint32_t>(color.rgbRed) << 16U) |
           (static_cast<std::uint32_t>(color.rgbGreen) << 8U) |
           static_cast<std::uint32_t>(color.rgbBlue);
  }

  [[nodiscard]] std::uint8_t sample_index(int x, int y) const {
    if (x < 0 || y < 0 || x >= view.width || y >= height) {
      return 0U;
    }
    const int source_y = view.height > 0 ? height - 1 - y : y;
    return std::to_integer<std::uint8_t>(
        view.pixels[static_cast<std::size_t>(source_y) * row_bytes +
                    static_cast<std::size_t>(x)]);
  }
};

OriginalWorldPalette decode_original_clut(
    std::span<const std::byte> resource) {
  if (resource.size() < 256U * 8U) {
    throw std::runtime_error("Original CLUT/1000 is truncated");
  }
  std::array<std::uint32_t, 256> palette{};
  // 1020:0e29 creates entries 0..254, skips source record 184 when it
  // reaches destination entry 184, and then writes an all-zero entry 255.
  // The fourth byte is a PC_RESERVED/PC_NOCOLLAPSE flag rather than color.
  for (std::size_t index = 0; index < 255U; ++index) {
    const std::size_t source_index = index + (index >= 184U ? 1U : 0U);
    const std::size_t offset = source_index * 8U;
    const auto red = std::to_integer<std::uint8_t>(resource[offset + 2U]);
    const auto green = std::to_integer<std::uint8_t>(resource[offset + 4U]);
    const auto blue = std::to_integer<std::uint8_t>(resource[offset + 6U]);
    palette[index] = (static_cast<std::uint32_t>(red) << 16U) |
                     (static_cast<std::uint32_t>(green) << 8U) |
                     static_cast<std::uint32_t>(blue);
  }
  palette[255] = 0U;
  return palette;
}

std::uint16_t load_original_world_header_word(
    const OriginalTdtDocument& document,
    std::size_t version_20_offset);

std::uint8_t color_channel(std::uint32_t color, int shift) noexcept {
  return static_cast<std::uint8_t>(color >> shift);
}

std::uint8_t interpolate_palette_channel(std::uint8_t first,
                                         std::uint8_t second,
                                         std::uint16_t frame,
                                         std::uint16_t first_frame,
                                         std::uint16_t last_frame) noexcept {
  const std::uint32_t duration = last_frame - first_frame;
  if (second > first) {
    return static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(frame - first_frame) *
         static_cast<std::uint32_t>(second - first)) /
            duration +
        first);
  }
  return static_cast<std::uint8_t>(
      (static_cast<std::uint32_t>(last_frame - frame) *
       static_cast<std::uint32_t>(first - second)) /
          duration +
      second);
}

std::uint32_t interpolate_palette_color(std::uint32_t first,
                                        std::uint32_t second,
                                        std::uint16_t frame,
                                        std::uint16_t first_frame,
                                        std::uint16_t last_frame) noexcept {
  const auto red = interpolate_palette_channel(
      color_channel(first, 16), color_channel(second, 16), frame,
      first_frame, last_frame);
  const auto green = interpolate_palette_channel(
      color_channel(first, 8), color_channel(second, 8), frame,
      first_frame, last_frame);
  const auto blue = interpolate_palette_channel(
      color_channel(first, 0), color_channel(second, 0), frame,
      first_frame, last_frame);
  return (static_cast<std::uint32_t>(red) << 16U) |
         (static_cast<std::uint32_t>(green) << 8U) |
         static_cast<std::uint32_t>(blue);
}

struct OriginalPaletteTransition {
  std::int32_t first_resource{};
  std::int32_t second_resource{};
  std::uint16_t first_frame{};
  std::uint16_t last_frame{};
};

std::optional<OriginalPaletteTransition> original_palette_transition(
    const OriginalTdtDocument& document) noexcept {
  const std::uint16_t frame = document.header.frame_time;
  const bool special =
      (load_original_world_header_word(document, 60U) & 0x10U) != 0U;
  if (special) {
    if (frame <= 0x0050U) return OriginalPaletteTransition{1000, 1003, 0, 80};
    if (frame >= 0x05dcU && frame <= 0x0640U) {
      return OriginalPaletteTransition{1003, 1000, 1500, 1600};
    }
    return std::nullopt;
  }
  if (frame >= 0x09e5U && frame <= 0x0a06U) {
    return OriginalPaletteTransition{1002, 1001, 2533, 2566};
  }
  if (frame >= 0x0a06U && frame <= 0x0a28U) {
    return OriginalPaletteTransition{1001, 1000, 2566, 2600};
  }
  if (frame >= 0x0640U && frame <= 0x06a4U) {
    return OriginalPaletteTransition{1000, 1001, 1600, 1700};
  }
  if (frame >= 0x06a4U && frame <= 0x0708U) {
    return OriginalPaletteTransition{1001, 1002, 1700, 1800};
  }
  return std::nullopt;
}

std::int32_t original_static_palette_resource(
    const OriginalTdtDocument& document) noexcept {
  const auto frame = document.header.frame_time;
  const bool special =
      (load_original_world_header_word(document, 60U) & 0x10U) != 0U;
  if (special) return frame > 80U && frame < 1500U ? 1003 : 1000;
  return frame >= 1800U && frame < 2533U ? 1002 : 1000;
}

void store_dynamic_palette_color(OriginalWorldPalette& destination,
                                 std::size_t index,
                                 std::uint32_t color) noexcept {
  // Exact 1020:08b4 aliasing for animated CLUT entries 188..193: mirror each
  // selected color into the +19 and +25 palette lanes.
  destination[index] = color;
  destination[index + 19U] = color;
  destination[index + 25U] = color;
}

void apply_original_time_palette(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    OriginalWorldPalette& destination) {
  // Exact native equivalent of 1020:0853: select the current CLUT colors for
  // logical entries 188..193, then apply 1020:08b4's mirrored aliases.
  if (const auto transition = original_palette_transition(document)) {
    const auto first = decode_original_clut(
        resources.find("CLUT", transition->first_resource));
    const auto second = decode_original_clut(
        resources.find("CLUT", transition->second_resource));
    for (std::size_t index = 188U; index < 194U; ++index) {
      store_dynamic_palette_color(
          destination, index,
          interpolate_palette_color(
              first[index], second[index], document.header.frame_time,
              transition->first_frame, transition->last_frame));
    }
    return;
  }

  const auto selected = decode_original_clut(resources.find(
      "CLUT", original_static_palette_resource(document)));
  for (std::size_t index = 188U; index < 194U; ++index) {
    store_dynamic_palette_color(destination, index, selected[index]);
  }
}

bool original_effect_remainder_nonzero(std::uint16_t counter,
                                       std::uint16_t addend,
                                       std::int16_t divisor) noexcept {
  const auto shifted = static_cast<std::uint16_t>(counter + addend);
  return std::bit_cast<std::int16_t>(shifted) % divisor != 0;
}

void apply_original_effect_colors(OriginalWorldPalette& palette,
                                  std::uint16_t counter) noexcept {
  constexpr std::uint32_t kPulseBright = 0x00f3ffffU;
  constexpr std::uint32_t kPulseDim = 0x009eb8b8U;
  for (std::size_t ordinal = 0U; ordinal < 3U; ++ordinal) {
    const std::uint16_t addend =
        ordinal == 0U ? 1U : (ordinal == 1U ? 0U : 2U);
    palette[194U + ordinal] =
        original_effect_remainder_nonzero(counter, addend, 3)
            ? kPulseBright
            : kPulseDim;
  }

  const bool odd = original_effect_remainder_nonzero(counter, 0U, 2);
  palette[197] = odd ? 0x00cc786bU : 0x00cfc29cU;
  palette[198] = odd ? 0x00cfc29cU : 0x00cc786bU;
  palette[199] = odd ? 0x0002029cU : 0x00b5b582U;
  palette[200] = odd ? 0x00b5b582U : 0x0002029cU;

  constexpr std::uint32_t kGrayLight = 0x00828282U;
  constexpr std::uint32_t kGrayDark = 0x004f4f4fU;
  for (std::size_t ordinal = 0U; ordinal < 3U; ++ordinal) {
    const std::uint16_t addend =
        ordinal == 0U ? 1U : (ordinal == 1U ? 0U : 2U);
    palette[201U + ordinal] =
        original_effect_remainder_nonzero(counter, addend, 3)
            ? kGrayLight
            : kGrayDark;
  }
}

void apply_original_special_effect_colors(OriginalWorldPalette& palette,
                                          std::uint16_t counter) noexcept {
  constexpr std::array<std::uint32_t, 6> kActive = {
      0x00ceefffU, 0x00a5e7ffU, 0x008cd6ffU,
      0x0063ceffU, 0x0042c6ffU, 0x004ab4ffU};
  constexpr std::array<std::uint32_t, 6> kInactive = {
      0x004c5466U, 0x0057667fU, 0x00576e8cU,
      0x00616699U, 0x00595e7fU, 0x004a4c66U};
  const std::uint16_t parity = counter & 1U;
  for (std::size_t group = 0U; group < 2U; ++group) {
    const auto& colors = parity == group ? kActive : kInactive;
    const std::size_t first = group == 0U ? 207U : 213U;
    std::copy(colors.begin(), colors.end(), palette.begin() + first);
  }
}

int positive_mod(int value, int divisor) {
  const int result = value % divisor;
  return result < 0 ? result + divisor : result;
}

void render_original_background(const OriginalResources& resources,
                                const OriginalWorldPalette& palette,
                                int view_x,
                                int view_y,
                                OriginalWorldRaster& raster) {
  // 1048:0000 assembles BITMAP/850..857 into one 32x2880 WinG strip in
  // reverse resource order. Direct per-strip sampling is pixel-equivalent.
  const std::array<IndexedDib, 8> sky = {
      IndexedDib(resources.find("BITMAP", 857)),
      IndexedDib(resources.find("BITMAP", 856)),
      IndexedDib(resources.find("BITMAP", 855)),
      IndexedDib(resources.find("BITMAP", 854)),
      IndexedDib(resources.find("BITMAP", 853)),
      IndexedDib(resources.find("BITMAP", 852)),
      IndexedDib(resources.find("BITMAP", 851)),
      IndexedDib(resources.find("BITMAP", 850)),
  };
  const IndexedDib skyline(resources.find("BITMAP", 905));

  for (int y = 0; y < raster.height; ++y) {
    const int world_y = view_y + y;
    int strip_y = world_y;
    if (world_y >= 1080) {
      strip_y -= 1080;
    }
    for (int x = 0; x < raster.width; ++x) {
      const int world_x = view_x + x;
      std::uint32_t color = 0x00ffffffU;
      if (world_y >= 0 && world_y < kOriginalWorldHeight &&
          strip_y >= 0 && strip_y < 2880) {
        const auto bitmap = static_cast<std::size_t>(strip_y / 360);
        color = palette[sky[bitmap].sample_index(
            positive_mod(world_x, 32), strip_y % 360)];
      }
      // 1048:03a3 repeats the 96x55 BITMAP/905 opaquely with its bottom at
      // world y=3960. Its world-aligned repeat remains fixed while scrolling.
      if (world_y >= 3905 && world_y < 3960) {
        color = palette[skyline.sample_index(
            positive_mod(world_x, 96), world_y - 3905)];
      }
      raster.pixels[static_cast<std::size_t>(y) * raster.width + x] = color;
    }
  }
}

void render_original_sky_decorations(
    const OriginalResources& resources,
    const OriginalWorldPalette& palette,
    const OriginalSkyDecorationState& state,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster) {
  // 1048:00ad packs BITMAP/900..903 into a shared WinG sheet; 1048:05f0
  // copies the selected source rectangle through 1208:071f. Direct sampling
  // is pixel-equivalent and preserves palette index zero as transparent.
  for (const auto& placement : state.placements) {
    if (!placement.valid()) continue;
    const IndexedDib graphic(resources.find(
        "BITMAP", 900 + placement.bitmap_index));
    if (graphic.view.width != placement.right - placement.left ||
        graphic.height != placement.bottom - placement.top) {
      throw std::runtime_error(
          "Original sky-decoration placement has invalid dimensions");
    }
    const int destination_x = placement.left - view_x;
    const int destination_y = placement.top - view_y;
    for (int y = 0; y < graphic.height; ++y) {
      const int raster_y = destination_y + y;
      if (raster_y < 0 || raster_y >= raster.height) continue;
      for (int x = 0; x < graphic.view.width; ++x) {
        const int raster_x = destination_x + x;
        if (raster_x < 0 || raster_x >= raster.width) continue;
        const auto index = graphic.sample_index(x, y);
        if (index == 0U) continue;
        raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                      raster_x] = palette[index];
      }
    }
  }
}

}  // namespace

int original_lobby_graphics_variant(std::uint16_t rating) noexcept {
  if (rating <= 2U) {
    return 0;
  }
  if (rating == 3U) {
    return 1;
  }
  return 2;
}

namespace {

std::uint16_t original_lobby_tile(std::uint16_t world_cell_x,
                                  std::uint16_t left) {
  const std::uint16_t delta = world_cell_x - left;
  const std::uint16_t left_mod_8 = left & 7U;
  const std::uint16_t left_mod_32 = left & 31U;
  const std::uint16_t edge_phase = delta + left_mod_8;
  if (left_mod_8 != 0U && edge_phase < 8U) {
    return static_cast<std::uint16_t>(
        delta + (left_mod_8 < 6U ? 34U : 32U));
  }
  return static_cast<std::uint16_t>((delta + left_mod_32) & 31U);
}

void draw_cgpk_tile(std::span<const std::byte> cgpk,
                    std::size_t tile,
                    const std::array<std::uint32_t, 256>& palette,
                    int destination_x,
                    int destination_y,
                    OriginalWorldRaster& raster) {
  constexpr std::size_t kTileBytes =
      static_cast<std::size_t>(kOriginalCellWidth * kOriginalFloorHeight);
  if (tile > (std::numeric_limits<std::size_t>::max() / kTileBytes) ||
      (tile + 1U) * kTileBytes > cgpk.size()) {
    throw std::runtime_error("Original CGPK lobby tile is truncated");
  }
  const std::size_t source = tile * kTileBytes;
  for (int y = 0; y < kOriginalFloorHeight; ++y) {
    const int raster_y = destination_y + y;
    if (raster_y < 0 || raster_y >= raster.height) {
      continue;
    }
    for (int x = 0; x < kOriginalCellWidth; ++x) {
      const int raster_x = destination_x + x;
      if (raster_x < 0 || raster_x >= raster.width) {
        continue;
      }
      const auto index = std::to_integer<std::uint8_t>(
          cgpk[source + static_cast<std::size_t>(y * kOriginalCellWidth + x)]);
      raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                    raster_x] = palette[index];
    }
  }
}

void render_original_lobbies(const OriginalResources& resources,
                             const OriginalTdtDocument& document,
                             const OriginalWorldPalette& palette,
                             int view_x,
                             int view_y,
                             OriginalWorldRaster& raster) {
  // 11f8:06cd selects rating tiers 0/1/2 and reloads the three type-24..26
  // CGPK banks at 2536/2600/2664 plus that tier. Address them directly here.
  const int variant = original_lobby_graphics_variant(document.header.rating);
  const std::array<int, 3> ids = {
      2536 + variant,
      2600 + variant,
      2664 + variant,
  };
  const std::array<std::span<const std::byte>, 3> graphics = {
      resources.find("CGPK", ids[0]),
      resources.find("CGPK", ids[1]),
      resources.find("CGPK", ids[2]),
  };
  for (std::size_t floor_index = 0; floor_index < document.floors.size();
       ++floor_index) {
    const auto& floor = document.floors[floor_index];
    for (const auto& tenant : floor.tenants) {
      if (tenant.type != 0x18 || tenant.left >= tenant.right) {
        continue;
      }

      std::size_t graphic = 0;
      std::size_t band = 41U;
      if (floor_index == 10U) {
        graphic = 0;
        band = document.header.lobby_height >= 2U ? 82U : 0U;
      } else if (floor_index == 11U) {
        graphic = 1;
        band = document.header.lobby_height >= 3U ? 41U : 0U;
      } else if (floor_index == 12U) {
        graphic = 2;
        band = 0U;
      }

      const int destination_y =
          (119 - static_cast<int>(floor_index)) * kOriginalFloorHeight -
          view_y;
      for (std::uint32_t cell = tenant.left; cell < tenant.right; ++cell) {
        const auto cell16 = static_cast<std::uint16_t>(cell);
        const std::size_t tile = band + original_lobby_tile(cell16, tenant.left);
        draw_cgpk_tile(graphics[graphic], tile, palette,
                       static_cast<int>(cell) * kOriginalCellWidth - view_x,
                       destination_y, raster);
      }
    }
  }
}

std::int16_t tenant_variant_word(const OriginalTdtTenant& tenant) {
  const auto high =
      std::to_integer<std::uint8_t>(tenant.preserved_07_to_0f[0]);
  const auto raw = static_cast<std::uint16_t>(tenant.variant) |
                   (static_cast<std::uint16_t>(high) << 8U);
  return static_cast<std::int16_t>(raw);
}

std::int16_t arithmetic_shift_right_3(std::int16_t value) {
  const int wide = value;
  return static_cast<std::int16_t>(
      wide >= 0 ? wide / 8 : -((7 - wide) / 8));
}

int original_office_frame(const OriginalTdtTenant& tenant) {
  // 1038:0716-0781. The first twelve frames are six construction variants,
  // each with status bands 0 and 8. Once status reaches 16, the renderer
  // switches to the final two occupied frames and no longer uses variant.
  // Both CMP/JGE and the following SAR operate on the sign-extended byte.
  const auto signed_status = static_cast<std::int8_t>(tenant.status);
  if (signed_status < 16) {
    return static_cast<int>(tenant_variant_word(tenant)) * 2 +
           arithmetic_shift_right_3(signed_status);
  }
  return 12 + arithmetic_shift_right_3(
                  static_cast<std::int16_t>(signed_status) - 16);
}

struct OriginalFacilityGraphics {
  int type;
  int width_cells;
  int first_resource;
  int last_resource;
  int graphic_height{24};
};

// 11f8:3fb1 appended each facility frame into process-local 24/36-row global
// banks before painting. The native renderer addresses the same source DIBs
// directly, preserving its frame dimensions and pixel order without the
// Win16 GlobalAlloc staging copy.
constexpr std::array<OriginalFacilityGraphics, 31> kDirectFacilityGraphics = {{
    {3, 4, 1192, 1195},
    {4, 6, 1256, 1263},
    {5, 10, 1320, 1323},
    {6, 24, 1384, 1393},
    {7, 9, 1448, 1451},
    {8, 2, 1512, 1518},
    {9, 16, 1576, 1590},
    {10, 12, 1640, 1652},
    {11, 4, 1704, 1705},
    {12, 16, 1768, 1777},
    {13, 26, 1832, 1834},
    {14, 16, 1896, 1896},
    {15, 15, 1960, 1960},
    {17, 2, 2088, 2088, 36},
    {18, 24, 2152, 2152},
    {19, 24, 2216, 2216, 36},
    {20, 25, 2280, 2285},
    {21, 25, 2344, 2350, 36},
    {29, 24, 2856, 2856},
    {30, 24, 2920, 2920, 36},
    {31, 30, 2984, 2985},
    {32, 30, 3048, 3049, 36},
    {33, 30, 3112, 3113, 36},
    {34, 7, 3176, 3177},
    {35, 7, 3240, 3241, 36},
    {36, 28, 3304, 3305, 36},
    {37, 28, 3368, 3369, 36},
    {38, 28, 3432, 3433, 36},
    {39, 28, 3496, 3497, 36},
    {40, 28, 3560, 3562, 36},
    {0x2c, 16, 3816, 3818},
}};

const OriginalFacilityGraphics* direct_facility_graphics(int type) {
  for (const auto& graphics : kDirectFacilityGraphics) {
    if (graphics.type == type) {
      return &graphics;
    }
  }
  return nullptr;
}

const OriginalTdtRetailRecord* linked_retail(
    const OriginalTdtDocument& document,
    const OriginalTdtTenant& tenant) {
  const auto index = static_cast<std::uint16_t>(tenant_variant_word(tenant));
  if (index >= document.retail.size()) {
    return nullptr;
  }
  return &document.retail[index];
}

int original_direct_facility_frame(const OriginalTdtDocument& document,
                                   const OriginalTdtTenant& tenant) {
  // Exact 1038:06a8 frame-selection cases for all ordinary type-3..15
  // facility banks. Types 6, 10, and 12 dereference the linked retail record;
  // type 13 dereferences the ten-entry DS:dbfc table.
  const auto signed_status = static_cast<std::int8_t>(tenant.status);
  switch (tenant.type) {
    case 3:
    case 4:
    case 5:
      return static_cast<std::int16_t>(
          arithmetic_shift_right_3(signed_status) +
          static_cast<std::int16_t>(tenant_variant_word(tenant) * 9));
    case 6:
    case 12: {
      const auto* retail = linked_retail(document, tenant);
      if (!retail) {
        return -1;
      }
      return static_cast<std::int16_t>(
          static_cast<std::int8_t>(
              std::to_integer<std::uint8_t>(retail->exact_bytes[2])) +
          static_cast<std::int16_t>(
              static_cast<std::int8_t>(
                  std::to_integer<std::uint8_t>(retail->exact_bytes[11])) *
              4));
    }
    case 7:
      return original_office_frame(tenant);
    case 9:
      return static_cast<std::int16_t>(
          arithmetic_shift_right_3(signed_status) +
          static_cast<std::int16_t>(tenant_variant_word(tenant) * 5));
    case 10: {
      const auto* retail = linked_retail(document, tenant);
      if (!retail) {
        return -1;
      }
      const auto state = static_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(retail->exact_bytes[2]));
      if (state == -1) {
        return 0x21;
      }
      if (state == 3) {
        return 0x22;
      }
      return static_cast<std::int16_t>(
          state + static_cast<std::int16_t>(
                      static_cast<std::int8_t>(std::to_integer<std::uint8_t>(
                          retail->exact_bytes[11])) *
                      3));
    }
    case 8:
    case 11:
    case 15:
    case 17:
    case 20:
    case 21:
    case 0x2c:
      return signed_status;
    case 14:
      // Literal 1038:097f. Security always uses its sole frame regardless of
      // the tenant status byte.
      return 0;
    case 29:
    case 30: {
      // 1038:08a3 follows tenant +0x0c into the shared twelve-byte dc24
      // record. States below three are used directly; every signed state at
      // least three is clamped to frame two at 0994.
      const auto index =
          static_cast<std::uint16_t>(tenant_variant_word(tenant));
      if (index >= document.post_elevator.dc24_records.size()) {
        return -1;
      }
      const auto state = static_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(
              document.post_elevator.dc24_records[index][6]));
      return state >= 3 ? 2 : state;
    }
    case 31:
    case 32:
    case 33:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
      // 1038:0946 reads the persisted word at tenant +0x0c directly.
      return tenant_variant_word(tenant);
    case 18:
    case 19:
    case 34:
    case 35: {
      const auto index =
          static_cast<std::uint16_t>(tenant_variant_word(tenant));
      if (index >= document.post_elevator.dc24_records.size()) {
        return -1;
      }
      const auto& movie = document.post_elevator.dc24_records[index];
      const auto state = static_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(movie[6]));
      // 1038:08c7 gives transformed entrances their auditorium-specific
      // frame only in state three. The full-width bodies always use state.
      if ((tenant.type == 34 || tenant.type == 35) && state == 3) {
        return static_cast<std::int16_t>(
            state + static_cast<std::int8_t>(
                        std::to_integer<std::uint8_t>(movie[7])));
      }
      return state;
    }
    case 13: {
      const auto index =
          static_cast<std::uint16_t>(tenant_variant_word(tenant));
      if (index >= document.post_elevator.dbfc_dwords.size()) {
        return -1;
      }
      if (((document.post_elevator.dbfc_dwords[index] >> 16U) & 0xffU) !=
          0U) {
        return 0;
      }
      return original_day_phase(document.header.frame_time) < 4 ? 1 : 2;
    }
    default:
      return -1;
  }
}

struct OriginalFacilityFrameSource {
  IndexedDib strip;
  int source_x;
};

std::optional<OriginalFacilityFrameSource> original_facility_frame_source(
    const OriginalResources& resources,
    const OriginalFacilityGraphics& graphics,
    int frame) {
  if (frame < 0) {
    return std::nullopt;
  }
  const int frame_width = graphics.width_cells * kOriginalCellWidth;
  if (graphics.type == 20) {
    // Type 20 is the upper Recycling Center half. BITMAP/2280 is a deliberate
    // 200x60 composite: 11f8:033a pads and extracts its top 24 rows for the
    // upper status-zero frame, while its lower 36 rows are duplicated by
    // type-21 BITMAP/2344. BITMAP/2281..2285 are ordinary 200x24 frames.
    if (frame > graphics.last_resource - graphics.first_resource) {
      return std::nullopt;
    }
    IndexedDib strip(
        resources.find("BITMAP", graphics.first_resource + frame));
    const int expected_height = frame == 0 ? 60 : graphics.graphic_height;
    if (strip.view.width != frame_width ||
        strip.height != expected_height) {
      throw std::runtime_error(
          "Original Recycling Center upper graphic has invalid dimensions");
    }
    return OriginalFacilityFrameSource{strip, 0};
  }
  for (int resource_id = graphics.first_resource;
       resource_id <= graphics.last_resource; ++resource_id) {
    IndexedDib strip(resources.find("BITMAP", resource_id));
    if (strip.height != graphics.graphic_height || strip.view.width <= 0 ||
        strip.view.width % frame_width != 0) {
      throw std::runtime_error(
          "Original facility graphics strip has invalid dimensions");
    }
    const int frames_in_strip = strip.view.width / frame_width;
    if (frame < frames_in_strip) {
      return OriginalFacilityFrameSource{strip, frame * frame_width};
    }
    frame -= frames_in_strip;
  }
  return std::nullopt;
}

void render_original_direct_facilities(const OriginalResources& resources,
                                       const OriginalTdtDocument& document,
                                       const OriginalWorldPalette& palette,
                                       int view_x,
                                       int view_y,
                                       OriginalWorldRaster& raster) {
  // Exact 11a0:047c/060c cell-cache and 24-row facility compositors. Frame
  // selection is supplied by 1038:06a8 above. 1038:0def only chooses the
  // transient cache sentinel for types 18/20/21/29/34; the native direct
  // raster skips that disposable WinG cache while preserving its opaque cell
  // copy and layer position, even when the tenant status byte is zero.
  for (std::size_t floor_index = 0; floor_index < document.floors.size();
       ++floor_index) {
    for (const auto& tenant : document.floors[floor_index].tenants) {
      // A negative type is still owned by the 11f0 construction queue. The
      // original routes that record through its type-0x29 construction
      // overlay, not through the Office facility bank.
      const auto* graphics = direct_facility_graphics(tenant.type);
      if (!graphics || tenant.left >= tenant.right) {
        continue;
      }

      auto source = original_facility_frame_source(
          resources, *graphics,
          original_direct_facility_frame(document, tenant));
      if (!source) {
        continue;
      }
      const int graphic_width = graphics->width_cells * kOriginalCellWidth;
      const int graphic_height = graphics->graphic_height;
      const int top_blank_rows = kOriginalFloorHeight - graphic_height;

      const int destination_x =
          static_cast<int>(tenant.left) * kOriginalCellWidth - view_x;
      const int destination_y =
          (119 - static_cast<int>(floor_index)) * kOriginalFloorHeight +
          top_blank_rows - view_y;
      for (int y = 0; y < graphic_height; ++y) {
        const int raster_y = destination_y + y;
        if (raster_y < 0 || raster_y >= raster.height) {
          continue;
        }
        for (int x = 0; x < graphic_width; ++x) {
          const int raster_x = destination_x + x;
          if (raster_x < 0 || raster_x >= raster.width) {
            continue;
          }
          const auto index =
              source->strip.sample_index(source->source_x + x, y);
          raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                        raster_x] = palette[index];
        }
      }
    }
  }
}

void render_original_pending_facilities(const OriginalResources& resources,
                                        const OriginalTdtDocument& document,
                                        const OriginalWorldPalette& palette,
                                        int view_x,
                                        int view_y,
                                        OriginalWorldRaster& raster) {
  // 11f8:033a registers type 0x29 with 41 eight-pixel cells. Its second
  // source bank is BITMAP/3625 (328x24), bottom-aligned in a 36-pixel band
  // over the staging surface's RGB(64,64,64) fill. GDI maps that fill to
  // CLUT/1000 entry 14, RGB(63,63,63).
  const IndexedDib construction(resources.find("BITMAP", 3625));
  if (construction.height != 24 || construction.view.width != 41 * 8) {
    throw std::runtime_error(
        "Original pending-construction graphics strip has invalid dimensions");
  }
  constexpr std::size_t kConstructionFillPaletteIndex = 14U;
  const std::uint32_t blank = palette[kConstructionFillPaletteIndex];

  for (std::size_t floor_index = 0; floor_index < document.floors.size();
       ++floor_index) {
    for (const auto& tenant : document.floors[floor_index].tenants) {
      if (tenant.type >= 0 || tenant.left >= tenant.right) {
        continue;
      }

      const int destination_y =
          (119 - static_cast<int>(floor_index)) * kOriginalFloorHeight -
          view_y;
      for (std::uint32_t cell = tenant.left; cell < tenant.right; ++cell) {
        // 1038:00a9 stores this exact value in the per-view byte at 7964 for
        // all ordinary (non-type-0/24/45) tenants. 1038:09c0 then selects
        // bank 1 of type 0x29 and 11a0:088f copies one 8x36 cell opaquely.
        const int frame = static_cast<int>(cell - tenant.left);
        if (frame < 0 || frame >= construction.view.width / kOriginalCellWidth) {
          throw std::runtime_error(
              "Original pending-construction frame is outside its strip");
        }
        const int destination_x =
            static_cast<int>(cell) * kOriginalCellWidth - view_x;
        for (int y = 0; y < kOriginalFloorHeight; ++y) {
          const int raster_y = destination_y + y;
          if (raster_y < 0 || raster_y >= raster.height) {
            continue;
          }
          for (int x = 0; x < kOriginalCellWidth; ++x) {
            const int raster_x = destination_x + x;
            if (raster_x < 0 || raster_x >= raster.width) {
              continue;
            }
            const std::uint32_t color = y < 12
                ? blank
                : palette[construction.sample_index(
                      frame * kOriginalCellWidth + x, y - 12)];
            raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                          raster_x] = color;
          }
        }
      }
    }
  }
}

void draw_original_horizontal_atlas(const OriginalResources& resources,
                                    const OriginalWorldPalette& palette,
                                    int first_resource,
                                    int last_resource,
                                    int source_cell,
                                    int width_cells,
                                    int destination_x,
                                    int destination_y,
                                    OriginalWorldRaster& raster) {
  constexpr int kAtlasHeight = 36;
  int source_x = source_cell * kOriginalCellWidth;
  int remaining = width_cells * kOriginalCellWidth;
  int output_x = destination_x;
  for (int resource_id = first_resource;
       resource_id <= last_resource && remaining > 0; ++resource_id) {
    const IndexedDib strip(resources.find("BITMAP", resource_id));
    if (strip.height != kAtlasHeight || strip.view.width <= 0) {
      throw std::runtime_error(
          "Original horizontal graphics atlas has invalid dimensions");
    }
    if (source_x >= strip.view.width) {
      source_x -= strip.view.width;
      continue;
    }
    if (source_x < 0) {
      return;
    }
    const int chunk = std::min(remaining, strip.view.width - source_x);
    for (int y = 0; y < kAtlasHeight; ++y) {
      const int raster_y = destination_y + y;
      if (raster_y < 0 || raster_y >= raster.height) {
        continue;
      }
      for (int x = 0; x < chunk; ++x) {
        const int raster_x = output_x + x;
        if (raster_x < 0 || raster_x >= raster.width) {
          continue;
        }
        raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                      raster_x] = palette[strip.sample_index(source_x + x, y)];
      }
    }
    remaining -= chunk;
    output_x += chunk;
    source_x = 0;
  }
  if (remaining != 0) {
    throw std::runtime_error("Original horizontal graphics atlas is truncated");
  }
}

template <std::size_t N>
std::uint8_t sample_original_horizontal_atlas_index(
    const std::array<IndexedDib, N>& atlas,
    int source_x,
    int source_y) {
  if (source_x < 0) {
    throw std::runtime_error("Original horizontal atlas source is negative");
  }
  for (const auto& strip : atlas) {
    if (source_x < strip.view.width) {
      if (source_y < 0 || source_y >= strip.height) {
        throw std::runtime_error(
            "Original horizontal atlas row is out of range");
      }
      return strip.sample_index(source_x, source_y);
    }
    source_x -= strip.view.width;
  }
  throw std::runtime_error("Original horizontal atlas source is truncated");
}

void render_original_empty_floors(const OriginalResources& resources,
                                  const OriginalTdtDocument& document,
                                  const OriginalWorldPalette& palette,
                                  int view_x,
                                  int view_y,
                                  OriginalWorldRaster& raster) {
  // 11f8:033a's resource formula is 1000 + type*64 + frame. Type zero is
  // therefore BITMAP/1000..1003; BITMAP/3944..3949 is type 46 (the fire
  // band). 1038:06ad passes tenant byte +0x0b to 11a0:0000 as one fixed source
  // cell and repeats that cell across the represented span. Fresh and
  // bulldozed Floor records both have status/source cell two.
  constexpr int kFirstResource = 1000;
  constexpr int kLastResource = 1003;
  const int view_left_cell = view_x / kOriginalCellWidth;
  const int view_right_cell =
      (view_x + raster.width + kOriginalCellWidth - 1) /
      kOriginalCellWidth;

  for (std::size_t floor_index = 0; floor_index < document.floors.size();
       ++floor_index) {
    const int destination_y =
        (119 - static_cast<int>(floor_index)) * kOriginalFloorHeight -
        view_y;
    if (destination_y >= raster.height ||
        destination_y + kOriginalFloorHeight <= 0) {
      continue;
    }
    for (const auto& tenant : document.floors[floor_index].tenants) {
      if (tenant.type != 0 || tenant.left >= tenant.right) {
        continue;
      }
      const int first_cell =
          std::max<int>(tenant.left, view_left_cell);
      const int last_cell =
          std::min<int>(tenant.right, view_right_cell);
      if (first_cell >= last_cell) {
        continue;
      }
      const int source_cell = static_cast<int>(
          static_cast<std::int8_t>(tenant.status));
      for (int cell = first_cell; cell < last_cell; ++cell) {
        draw_original_horizontal_atlas(
            resources, palette, kFirstResource, kLastResource, source_cell, 1,
            cell * kOriginalCellWidth - view_x, destination_y, raster);
      }
    }
  }
}

void render_original_damaged_facilities(const OriginalResources& resources,
                                          const OriginalTdtDocument& document,
                                          const OriginalWorldPalette& palette,
                                          int view_x,
                                          int view_y,
                                          OriginalWorldRaster& raster) {
  // 11f8:3959 uses type 47 for disaster damage. 11f8:033a assigns that type
  // BITMAP/4008 (1000 + 47*64). 1038:00a9 initializes the ordinary source
  // counter at zero and advances it once per tenant-relative cell; only type
  // 45 takes the separate left-modulo-four branch. 11a0:088f bottom-aligns
  // these opaque 24-row cells in the 36-row story band.
  const IndexedDib damage(resources.find("BITMAP", 4008));
  if (damage.height != 24 ||
      damage.view.width % kOriginalCellWidth != 0) {
    throw std::runtime_error(
        "Original damaged-facility graphics have invalid dimensions");
  }
  for (std::size_t floor_index = 0; floor_index < document.floors.size();
       ++floor_index) {
    for (const auto& tenant : document.floors[floor_index].tenants) {
      if (tenant.type != 47 || tenant.left >= tenant.right) continue;
      const int destination_y =
          (119 - static_cast<int>(floor_index)) * kOriginalFloorHeight + 12 -
          view_y;
      for (std::uint32_t cell = tenant.left; cell < tenant.right; ++cell) {
        const int source_cell = static_cast<int>(cell - tenant.left);
        if ((source_cell + 1) * kOriginalCellWidth > damage.view.width) {
          throw std::runtime_error(
              "Original damaged-facility source exceeds BITMAP/4008");
        }
        const int destination_x =
            static_cast<int>(cell) * kOriginalCellWidth - view_x;
        for (int y = 0; y < damage.height; ++y) {
          const int raster_y = destination_y + y;
          if (raster_y < 0 || raster_y >= raster.height) continue;
          for (int x = 0; x < kOriginalCellWidth; ++x) {
            const int raster_x = destination_x + x;
            if (raster_x < 0 || raster_x >= raster.width) continue;
            const auto index = damage.sample_index(
                source_cell * kOriginalCellWidth + x, y);
            raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                          raster_x] = palette[index];
          }
        }
      }
    }
  }
}

void render_original_floor_boundaries(const OriginalResources& resources,
                                      const OriginalTdtDocument& document,
                                      const OriginalWorldPalette& palette,
                                      int view_x,
                                      int view_y,
                                      OriginalWorldRaster& raster) {
  // 11f8:033a assigns type 45 the four-cell BITMAP/3880 bank. 1038:00a9
  // seeds its per-view cell selector from tenant.left modulo four, advances
  // it once per world cell, masks it back to 0..3, and 1038:0a06 sends that
  // selector to 11a0:088f for an opaque 8x36 copy. These two boundary spans
  // flank the type-33 Metro tenant on floor zero after 11e8:0000 expands it.
  const IndexedDib boundary(resources.find("BITMAP", 3880));
  if (boundary.height != kOriginalFloorHeight ||
      boundary.view.width != 4 * kOriginalCellWidth) {
    throw std::runtime_error(
        "Original type-45 boundary graphics have invalid dimensions");
  }
  for (std::size_t floor_index = 0; floor_index < document.floors.size();
       ++floor_index) {
    for (const auto& tenant : document.floors[floor_index].tenants) {
      if (tenant.type != 45 || tenant.left >= tenant.right) continue;
      const int destination_y =
          (119 - static_cast<int>(floor_index)) * kOriginalFloorHeight -
          view_y;
      for (std::uint32_t cell = tenant.left; cell < tenant.right; ++cell) {
        const int source_x = static_cast<int>(cell & 3U) * kOriginalCellWidth;
        const int destination_x =
            static_cast<int>(cell) * kOriginalCellWidth - view_x;
        for (int y = 0; y < kOriginalFloorHeight; ++y) {
          const int raster_y = destination_y + y;
          if (raster_y < 0 || raster_y >= raster.height) continue;
          for (int x = 0; x < kOriginalCellWidth; ++x) {
            const int raster_x = destination_x + x;
            if (raster_x < 0 || raster_x >= raster.width) continue;
            const auto index = boundary.sample_index(source_x + x, y);
            raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                          raster_x] = palette[index];
          }
        }
      }
    }
  }
}

void draw_original_type1_atlas(const OriginalResources& resources,
                               const OriginalWorldPalette& palette,
                               int source_cell,
                               int width_cells,
                               int destination_x,
                               int destination_y,
                               OriginalWorldRaster& raster) {
  // 11f8:033a concatenates BITMAP/1064..1069 horizontally in the 36-row
  // type-1 band. 11a0:0126 addresses it in eight-pixel cells.
  draw_original_horizontal_atlas(resources, palette, 1064, 1069, source_cell,
                                 width_cells, destination_x, destination_y,
                                 raster);
}

void draw_original_type16_bank(const OriginalResources& resources,
                               const OriginalWorldPalette& palette,
                               int source_frame,
                               int width_cells,
                               int destination_x,
                               int destination_y,
                               OriginalWorldRaster& raster) {
  // The type-16 initializer slices BITMAP/2024..2029 into consecutive
  // 8x36 frames. 11a0:027c copies a run of those frames opaquely.
  draw_original_horizontal_atlas(resources, palette, 2024, 2029, source_frame,
                                 width_cells, destination_x, destination_y,
                                 raster);
}

bool original_elevator_car_on_floor(const OriginalTdtElevator& elevator,
                                    int floor) {
  for (const auto& car : elevator.car_records) {
    if (car.exact_bytes[15] != std::byte{0} &&
        static_cast<std::int8_t>(
            std::to_integer<std::uint8_t>(car.exact_bytes[0])) == floor) {
      return true;
    }
  }
  return false;
}

void render_original_elevator_floor(
    const OriginalResources& resources,
    const OriginalTdtElevator& elevator,
    const OriginalWorldPalette& palette,
    int floor,
    int destination_x,
    int destination_y,
    OriginalWorldRaster& raster) {
  const bool car_present = original_elevator_car_on_floor(elevator, floor);
  const int car_offset = car_present ? 0x58 : 0;
  const int body_x = destination_x +
                     (elevator.type == 0U ? kOriginalCellWidth : 0);

  // 10a8:0566-05cc draws the express car's two one-cell outer panels from
  // the type-1 atlas. Its four-cell type-16 body is inset between them.
  if (elevator.type == 0U && car_present) {
    draw_original_type1_atlas(resources, palette, 0x5a, 1, destination_x,
                              destination_y, raster);
    draw_original_type1_atlas(
        resources, palette, 0x5b, 1,
        destination_x + 5 * kOriginalCellWidth, destination_y, raster);
  }

  const bool serviced = floor >= 0 && floor < 120 &&
                        elevator.serviced_floors[static_cast<std::size_t>(floor)] !=
                            std::byte{0};
  if (!serviced) {
    draw_original_type16_bank(resources, palette, car_offset, 4, body_x,
                              destination_y, raster);
    return;
  }

  if (floor == 109) {
    draw_original_type16_bank(resources, palette, 0x28, 4, body_x,
                              destination_y, raster);
    return;
  }

  if (floor < 10) {
    const int first_source =
        car_offset + (floor == 0 ? 0x42 : 0x44);
    const int second_source =
        car_offset + 0x2c + ((10 - floor) % 10) * 2;
    draw_original_type16_bank(resources, palette, first_source, 2, body_x,
                              destination_y, raster);
    draw_original_type16_bank(
        resources, palette, second_source, 2,
        body_x + 2 * kOriginalCellWidth, destination_y, raster);
    return;
  }

  // 10a8:06d4-070a is a single four-cell draw. It does not execute the
  // two-cell floor-number suffix path used below floor ten and at 19+.
  if (floor < 19) {
    const int source = car_offset + 4 + (floor - 10) * 4;
    draw_original_type16_bank(resources, palette, source, 4, body_x,
                              destination_y, raster);
    return;
  }

  const int first_source =
      car_offset + 0x46 + ((floor - 19) / 10) * 2;
  const int second_source =
      car_offset + 0x2c + ((floor - 19) % 10) * 2;
  draw_original_type16_bank(resources, palette, first_source, 2, body_x,
                            destination_y, raster);
  draw_original_type16_bank(
      resources, palette, second_source, 2,
      body_x + 2 * kOriginalCellWidth, destination_y, raster);
}

void render_original_elevator_at_floor(
    const OriginalResources& resources,
    const OriginalTdtElevator& elevator,
    const OriginalWorldPalette& palette,
    int floor,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster) {
  const int destination_x =
      static_cast<int>(elevator.x) * kOriginalCellWidth - view_x;
  const int destination_y =
      (119 - floor) * kOriginalFloorHeight - view_y;
  const bool express = elevator.type == 0U;
  const int cap_width = express ? 6 : 4;
  // 10a8:0333-03dc uses distinct six-cell express caps and shared four-cell
  // standard/service caps one story outside the shaft bounds.
  if (floor == static_cast<int>(elevator.top_floor) + 1) {
    draw_original_type1_atlas(resources, palette, express ? 0x4e : 0x14,
                              cap_width, destination_x, destination_y,
                              raster);
  } else if (floor == static_cast<int>(elevator.bottom_floor) - 1) {
    draw_original_type1_atlas(resources, palette, express ? 0x54 : 0x18,
                              cap_width, destination_x, destination_y,
                              raster);
  } else if (elevator.word_3c == 0U) {
    // 10a8:07d6-0883 does not omit a hidden in-span shaft. It selects a
    // one-pixel black pen and draws only the left/right boundaries for 35
    // pixels; GDI LineTo deliberately excludes the y+35 endpoint.
    const int right_x =
        destination_x + cap_width * kOriginalCellWidth - 1;
    for (int local_y = 0; local_y < kOriginalFloorHeight - 1; ++local_y) {
      const int y = destination_y + local_y;
      if (y < 0 || y >= raster.height) continue;
      for (const int x : {destination_x, right_x}) {
        if (x < 0 || x >= raster.width) continue;
        raster.pixels[static_cast<std::size_t>(y) * raster.width + x] = 0U;
      }
    }
  } else {
    render_original_elevator_floor(resources, elevator, palette, floor,
                                   destination_x, destination_y, raster);
  }
}

void render_original_elevator_cars(const OriginalResources& resources,
                                   const OriginalTdtDocument& document,
                                   const OriginalWorldPalette& palette,
                                   int view_x,
                                   int view_y,
                                   OriginalWorldRaster& raster,
                                   const std::function<void()>&
                                       elevator_checkpoint) {
  // 1090:0cb3 calls 1090:0d15 for all eight cars in each ordinary live shaft;
  // 0d15 marks covered rows/columns in byte 7 of the Win16 presentation cache.
  // 1090:0b10 then copies every active car from the hidden type-1 staging band
  // after 10c0:007a draws Stair/Escalator graphics. The native direct layer
  // clips those same 31-pixel cars without a cache marker.
  const std::array<IndexedDib, 6> atlas = {
      IndexedDib(resources.find("BITMAP", 1064)),
      IndexedDib(resources.find("BITMAP", 1065)),
      IndexedDib(resources.find("BITMAP", 1066)),
      IndexedDib(resources.find("BITMAP", 1067)),
      IndexedDib(resources.find("BITMAP", 1068)),
      IndexedDib(resources.find("BITMAP", 1069)),
  };
  for (const auto& strip : atlas) {
    if (strip.height != kOriginalFloorHeight || strip.view.width <= 0) {
      throw std::runtime_error(
          "Original elevator-car staging atlas has invalid dimensions");
    }
  }

  for (const auto& elevator : document.elevators) {
    // 1090:0cb3 prepares late car rectangles only for word_3c == 0. Shown
    // shafts already include their car form in 10a8:0507 and never receive
    // this post-Stair/Escalator overlay.
    if (elevator.used != 0U && elevator.word_3c == 0U) {
      for (std::size_t car_index = 0U;
           car_index < elevator.car_records.size(); ++car_index) {
        const auto visual = original_elevator_car_visual(
            elevator, car_index, view_x, view_y, raster.height);
        if (!visual) continue;
        for (int y = 0; y < visual->height; ++y) {
          const int raster_y = visual->destination_y + y;
          if (raster_y < 0 || raster_y >= raster.height) continue;
          for (int x = 0; x < visual->width; ++x) {
            const int raster_x = visual->destination_x + x;
            if (raster_x < 0 || raster_x >= raster.width) continue;
            const auto index = sample_original_horizontal_atlas_index(
                atlas, visual->source_x + x, visual->source_y + y);
            raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                          raster_x] = palette[index];
          }
        }
      }
    }
    // 1090:05af-05cc calls 11e0:0e84 after every one of the 24 Elevator
    // slots; only the intervening 0b10 car compositor is conditional on used.
    if (elevator_checkpoint) elevator_checkpoint();
  }
}

std::uint16_t load_original_world_word(
    std::span<const std::byte> bytes,
    std::size_t offset,
    bool byte_swapped) {
  if (offset + 2U > bytes.size()) return 0U;
  const auto first = std::to_integer<std::uint8_t>(bytes[offset]);
  const auto second = std::to_integer<std::uint8_t>(bytes[offset + 1U]);
  return byte_swapped
      ? static_cast<std::uint16_t>((first << 8U) | second)
      : static_cast<std::uint16_t>(first | (second << 8U));
}

std::size_t original_world_header_runtime_offset(
    const OriginalTdtDocument& document,
    std::size_t version_20_offset) {
  std::size_t offset = version_20_offset -
                       (document.header.format_version >= 0x20U ? 0U : 2U);
  if (version_20_offset >= 60U &&
      document.header.format_version < 0x23U) {
    offset -= 2U;
  }
  return offset;
}

std::uint16_t load_original_world_header_word(
    const OriginalTdtDocument& document,
    std::size_t version_20_offset) {
  const auto offset =
      original_world_header_runtime_offset(document, version_20_offset);
  return load_original_world_word(document.header.exact_bytes, offset,
                                  document.header.byte_swapped);
}

std::uint32_t load_original_world_dword(
    std::span<const std::byte> bytes,
    std::size_t offset,
    bool byte_swapped) {
  if (offset + 4U > bytes.size()) {
    return std::numeric_limits<std::uint32_t>::max();
  }
  const auto first = std::to_integer<std::uint8_t>(bytes[offset]);
  const auto second = std::to_integer<std::uint8_t>(bytes[offset + 1U]);
  const auto third = std::to_integer<std::uint8_t>(bytes[offset + 2U]);
  const auto fourth = std::to_integer<std::uint8_t>(bytes[offset + 3U]);
  if (byte_swapped) {
    return (static_cast<std::uint32_t>(first) << 24U) |
           (static_cast<std::uint32_t>(second) << 16U) |
           (static_cast<std::uint32_t>(third) << 8U) |
           static_cast<std::uint32_t>(fourth);
  }
  return static_cast<std::uint32_t>(first) |
         (static_cast<std::uint32_t>(second) << 8U) |
         (static_cast<std::uint32_t>(third) << 16U) |
         (static_cast<std::uint32_t>(fourth) << 24U);
}

bool store_original_world_header_dword(OriginalTdtDocument& document,
                                       std::size_t version_20_offset,
                                       std::uint32_t value) {
  const auto offset =
      original_world_header_runtime_offset(document, version_20_offset);
  if (offset + 4U > document.header.exact_bytes.size()) return false;
  if (document.header.byte_swapped) {
    document.header.exact_bytes[offset] =
        static_cast<std::byte>(value >> 24U);
    document.header.exact_bytes[offset + 1U] =
        static_cast<std::byte>(value >> 16U);
    document.header.exact_bytes[offset + 2U] =
        static_cast<std::byte>(value >> 8U);
    document.header.exact_bytes[offset + 3U] = static_cast<std::byte>(value);
  } else {
    document.header.exact_bytes[offset] = static_cast<std::byte>(value);
    document.header.exact_bytes[offset + 1U] =
        static_cast<std::byte>(value >> 8U);
    document.header.exact_bytes[offset + 2U] =
        static_cast<std::byte>(value >> 16U);
    document.header.exact_bytes[offset + 3U] =
        static_cast<std::byte>(value >> 24U);
  }
  return true;
}

std::uint32_t load_original_world_header_dword(
    const OriginalTdtDocument& document,
    std::size_t version_20_offset) {
  const auto offset =
      original_world_header_runtime_offset(document, version_20_offset);
  return load_original_world_dword(document.header.exact_bytes, offset,
                                   document.header.byte_swapped);
}

std::int8_t original_world_signed_byte(std::byte value) {
  return std::bit_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(value));
}

bool original_facility_person_is_present(
    const OriginalTdtPersonRecord& person) noexcept {
  // Every 1028 active-family helper uses signed `cmp byte [person+5],5` / JG.
  return original_world_signed_byte(person.exact_bytes[5]) <= 5;
}

std::uint16_t next_original_world_random(
    OriginalTdtDocument& document) noexcept {
  // Microsoft C 7.0/Visual C++ 1.x rand() at 1000:3a2f.
  document.random_state = document.random_state * 0x015a4e35U + 1U;
  return static_cast<std::uint16_t>(
      (document.random_state >> 16U) & 0x7fffU);
}

std::size_t original_facility_people_owned_count(std::int8_t type) noexcept {
  // 1028:0000 draws only ordinal zero for a negative/pending facility.
  if (type < 0) return 1U;
  switch (type) {
    case 3:
      return 2U;
    case 4:
    case 5:
    case 9:
      return 3U;
    case 7:
      return 6U;
    case 6:
    case 12:
    case 40:
      return 2U;
    default:
      return 0U;
  }
}

std::size_t original_facility_position_count(
    const OriginalTdtTenant& tenant) noexcept {
  // DS:74ba is initialized by 11f8:0000. For every type admitted here its
  // value is the facility's eight-pixel span; the resource-derived entries
  // for Hotel/Condo are 4/6/10/16 respectively.
  const auto span = static_cast<std::uint16_t>(tenant.right - tenant.left);
  return span;
}

bool original_facility_is_visible(const OriginalTdtTenant& tenant,
                                  std::size_t floor,
                                  int view_x,
                                  int view_y,
                                  int width,
                                  int height) noexcept {
  if (tenant.left >= tenant.right || width <= 0 || height <= 0) return false;
  const int left = static_cast<int>(tenant.left) * kOriginalCellWidth - view_x;
  const int right = static_cast<int>(tenant.right) * kOriginalCellWidth - view_x;
  const int top =
      (119 - static_cast<int>(floor)) * kOriginalFloorHeight - view_y;
  const int bottom = top + kOriginalFloorHeight;
  return left < width && right > 0 && top < height && bottom > 0;
}

OriginalTdtTenant* original_world_person_owner(
    OriginalTdtDocument& document,
    const OriginalTdtPersonRecord& person) noexcept {
  const int floor_index = original_world_signed_byte(person.exact_bytes[0]);
  const auto key =
      std::to_integer<std::uint8_t>(person.exact_bytes[1]);
  if (floor_index < 0 ||
      floor_index >= static_cast<int>(document.floors.size()) ||
      key >= OriginalTdtFloor::kIndexCapacity) {
    return nullptr;
  }
  auto& floor = document.floors[static_cast<std::size_t>(floor_index)];
  const auto tenant_index = floor.tenant_index[key];
  if (tenant_index >= floor.tenants.size()) return nullptr;
  return &floor.tenants[tenant_index];
}

bool set_original_facility_person_graphic(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::uint8_t position,
    std::uint8_t graphic) noexcept {
  if (person_index >= document.people_count ||
      person_index >= document.people.size()) {
    return false;
  }
  auto& exact = document.people[person_index].exact_bytes;
  const bool changed = exact[7] != static_cast<std::byte>(position) ||
                       exact[8] != static_cast<std::byte>(graphic);
  exact[7] = static_cast<std::byte>(position);
  exact[8] = static_cast<std::byte>(graphic);
  return changed;
}

bool randomize_original_facility_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::uint16_t position_modulus,
    std::uint16_t graphic_modulus,
    std::uint8_t graphic_base,
    std::size_t& changed_people) noexcept {
  if (person_index >= document.people_count ||
      person_index >= document.people.size() || position_modulus == 0U ||
      graphic_modulus == 0U ||
      !original_facility_person_is_present(document.people[person_index])) {
    return false;
  }
  const auto position = static_cast<std::uint8_t>(
      next_original_world_random(document) % position_modulus);
  const auto graphic = static_cast<std::uint8_t>(
      next_original_world_random(document) % graphic_modulus + graphic_base);
  changed_people += set_original_facility_person_graphic(
      document, person_index, position, graphic) ? 1U : 0U;
  return true;
}

bool step_original_inactive_facility_person(
    OriginalTdtDocument& document,
    std::size_t first_person,
    std::size_t position_count,
    std::size_t& changed_people) noexcept {
  // 1028:0841 does not apply the active-family person-state predicate.
  if (first_person >= document.people_count ||
      first_person >= document.people.size() || position_count <= 1U) {
    return false;
  }
  const auto position = static_cast<std::uint8_t>(
      next_original_world_random(document) % (position_count - 1U));
  const auto graphic = static_cast<std::uint8_t>(
      next_original_world_random(document) % 6U + 0x39U);
  changed_people += set_original_facility_person_graphic(
      document, first_person, position, graphic) ? 1U : 0U;
  return true;
}

bool step_original_office_facility_people(
    OriginalTdtDocument& document,
    std::size_t first_person,
    std::size_t& changed_people) noexcept {
  // Exact 1028:0902 six-record order. The original resolves the owner from
  // person bytes 0/1 and reads the signed word at runtime tenant +0x0c,
  // corresponding to serialized tenant bytes 6/7.
  if (first_person >= document.people_count ||
      first_person >= document.people.size()) {
    return false;
  }
  const auto* owner = original_world_person_owner(
      document, document.people[first_person]);
  const auto owner_variant = owner
      ? std::bit_cast<std::int16_t>(load_original_world_word(
            owner->exact_bytes, 6U, document.header.byte_swapped))
      : std::int16_t{0};
  bool any = false;
  const auto randomize = [&](std::size_t ordinal, std::uint16_t position_mod,
                             std::uint16_t graphic_mod,
                             std::uint8_t graphic_base) {
    const bool active = randomize_original_facility_person(
        document, first_person + ordinal, position_mod, graphic_mod,
        graphic_base, changed_people);
    any = active || any;
    return active;
  };
  const auto fixed_position = [&](std::size_t ordinal, std::uint8_t position,
                                  std::uint16_t graphic_mod,
                                  std::uint8_t graphic_base) {
    const auto index = first_person + ordinal;
    if (index >= document.people_count || index >= document.people.size() ||
        !original_facility_person_is_present(document.people[index])) {
      return false;
    }
    const auto graphic = static_cast<std::uint8_t>(
        next_original_world_random(document) % graphic_mod + graphic_base);
    changed_people += set_original_facility_person_graphic(
        document, index, position, graphic) ? 1U : 0U;
    any = true;
    return true;
  };

  if (owner_variant < 2) {
    (void)fixed_position(0U, static_cast<std::uint8_t>(owner_variant + 1),
                         2U, 0x0eU);
    (void)fixed_position(1U, static_cast<std::uint8_t>(owner_variant + 2),
                         2U, 0x10U);
    (void)fixed_position(2U, static_cast<std::uint8_t>(owner_variant + 3),
                         2U, 0x0cU);
  } else {
    (void)fixed_position(0U, 7U, 2U, 0x0aU);
    const auto second = first_person + 1U;
    if (second < document.people_count && second < document.people.size() &&
        original_facility_person_is_present(document.people[second])) {
      const auto graphic = static_cast<std::uint8_t>(
          next_original_world_random(document) % 4U + 6U);
      const auto position = graphic >= 8U
          ? 4U
          : static_cast<std::uint8_t>(
                next_original_world_random(document) % 8U);
      changed_people += set_original_facility_person_graphic(
          document, second, position, graphic) ? 1U : 0U;
      any = true;
    }
    const auto third = first_person + 2U;
    if (third < document.people_count && third < document.people.size() &&
        original_facility_person_is_present(document.people[third])) {
      const auto graphic = static_cast<std::uint8_t>(
          next_original_world_random(document) % 4U + 2U);
      const auto position = graphic >= 4U
          ? 2U
          : static_cast<std::uint8_t>(
                next_original_world_random(document) % 8U);
      changed_people += set_original_facility_person_graphic(
          document, third, position, graphic) ? 1U : 0U;
      any = true;
    }
  }
  (void)randomize(3U, 8U, 2U, 0U);

  const auto fifth = first_person + 4U;
  if (fifth < document.people_count && fifth < document.people.size() &&
      original_facility_person_is_present(document.people[fifth])) {
    const auto graphic = static_cast<std::uint8_t>(
        next_original_world_random(document) % 4U + 0x14U);
    const auto position = static_cast<std::uint8_t>(
        next_original_world_random(document) % 8U);
    changed_people += set_original_facility_person_graphic(
        document, fifth, position, graphic) ? 1U : 0U;
    any = true;
  }

  const auto sixth = first_person + 5U;
  if (sixth < document.people_count && sixth < document.people.size() &&
      original_facility_person_is_present(document.people[sixth])) {
    const auto graphic = static_cast<std::uint8_t>(
        next_original_world_random(document) % 2U + 0x12U);
    const auto position = static_cast<std::uint8_t>(
        next_original_world_random(document) % 8U);
    changed_people += set_original_facility_person_graphic(
        document, sixth, position, graphic) ? 1U : 0U;
    any = true;
  }
  return any;
}

bool step_original_condo_facility_people(
    OriginalTdtDocument& document,
    std::size_t first_person,
    bool control_modifier,
    std::size_t& changed_people) noexcept {
  // Exact 1028:0feb three-record order; b3a0 is the weekend/calendar flag
  // and b3a1 is the integer b3de/400 day phase.
  bool any = false;
  const auto day_phase = original_day_phase(document.header.frame_time);
  const std::int32_t calendar_mod = document.header.current_day % 12;
  const std::int32_t third = calendar_mod % 3;
  const bool weekend = third >= 2;
  const auto active = [&](std::size_t ordinal) {
    const auto index = first_person + ordinal;
    return index < document.people_count && index < document.people.size() &&
           original_facility_person_is_present(document.people[index]);
  };

  if (active(0U)) {
    const auto position = static_cast<std::uint8_t>(
        next_original_world_random(document) % 15U);
    std::uint8_t graphic = 0U;
    if (weekend) {
      graphic = 0x22U;
    } else if (day_phase < 4) {
      graphic = 0x1eU;
    } else {
      graphic = static_cast<std::uint8_t>(
          next_original_world_random(document) % 2U + 0x20U);
    }
    changed_people += set_original_facility_person_graphic(
        document, first_person, position, graphic) ? 1U : 0U;
    any = true;
  }
  if (active(1U)) {
    const auto graphic = static_cast<std::uint8_t>(
        next_original_world_random(document) % 9U + 0x29U);
    const auto position = graphic == 0x31U
        ? 1U
        : static_cast<std::uint8_t>(
              next_original_world_random(document) % 15U);
    changed_people += set_original_facility_person_graphic(
        document, first_person + 1U, position, graphic) ? 1U : 0U;
    any = true;
  }
  if (active(2U)) {
    const auto position = static_cast<std::uint8_t>(
        next_original_world_random(document) % 15U);
    const auto graphic = control_modifier
        ? static_cast<std::uint8_t>(
              next_original_world_random(document) % 3U + 0x36U)
        : static_cast<std::uint8_t>(
              next_original_world_random(document) % 4U + 0x32U);
    changed_people += set_original_facility_person_graphic(
        document, first_person + 2U, position, graphic) ? 1U : 0U;
    any = true;
  }
  return any;
}

bool step_original_hotel_facility_people(
    OriginalTdtDocument& document,
    std::size_t first_person,
    std::int8_t type,
    bool control_modifier,
    std::size_t& changed_people) noexcept {
  // Exact 1028:12c5. The type-table widths are 4, 6 and 10, so every random
  // left cell uses width-1 and the two-cell figure remains inside the room.
  const std::uint16_t position_modulus =
      type == 3 ? 3U : (type == 4 ? 5U : 9U);
  bool any = false;
  any = randomize_original_facility_person(
            document, first_person, position_modulus, 3U, 0x60U,
            changed_people) ||
        any;
  const auto second = first_person + 1U;
  if (second < document.people_count && second < document.people.size() &&
      original_facility_person_is_present(document.people[second])) {
    const auto position = static_cast<std::uint8_t>(
        next_original_world_random(document) % position_modulus);
    const auto graphic = document.header.frame_time / 400U < 4U
        ? static_cast<std::uint8_t>(
              next_original_world_random(document) % 2U + 0x50U)
        : static_cast<std::uint8_t>(
              next_original_world_random(document) % 9U + 0x52U);
    changed_people += set_original_facility_person_graphic(
        document, second, position, graphic) ? 1U : 0U;
    any = true;
  }
  if (type == 3) return any;
  const auto third_person = first_person + 2U;
  if (third_person < document.people_count &&
      third_person < document.people.size() &&
      original_facility_person_is_present(document.people[third_person])) {
    const auto position = static_cast<std::uint8_t>(
        next_original_world_random(document) % position_modulus);
    const auto graphic = document.header.frame_time / 400U >= 4U &&
                                 control_modifier
        ? 0x5fU
        : static_cast<std::uint8_t>(
              next_original_world_random(document) % 4U + 0x5bU);
    changed_people += set_original_facility_person_graphic(
        document, third_person, position, graphic) ? 1U : 0U;
    any = true;
  }
  return any;
}

bool step_original_food_facility_people(
    OriginalTdtDocument& document,
    std::size_t first_person,
    std::int8_t type,
    std::size_t& changed_people) noexcept {
  bool any = false;
  if (type == 12) {
    any = randomize_original_facility_person(
              document, first_person, 3U, 2U, 0x3fU, changed_people) ||
          any;
    any = randomize_original_facility_person(
              document, first_person + 1U, 3U, 2U, 0x41U,
              changed_people) ||
          any;
  } else {
    any = randomize_original_facility_person(
              document, first_person, 3U, 3U, 0x49U, changed_people) ||
          any;
    any = randomize_original_facility_person(
              document, first_person + 1U, 3U, 4U, 0x4cU,
              changed_people) ||
          any;
  }
  return any;
}

bool step_original_cathedral_facility_people(
    OriginalTdtDocument& document,
    std::size_t first_person,
    std::size_t& changed_people,
    bool& counter_changed) noexcept {
  // Exact 1028:17f0 b406-bit-four gate and b40c phase advance.
  if ((load_original_world_header_word(document, 60U) & 4U) == 0U ||
      first_person + 1U >= document.people_count ||
      first_person + 1U >= document.people.size()) {
    return false;
  }
  const auto counter = load_original_world_header_dword(document, 66U);
  std::uint8_t first_position = 13U;
  std::uint8_t second_position = 13U;
  std::uint8_t graphic = 7U;
  if (std::bit_cast<std::int32_t>(counter) < 14) {
    first_position = static_cast<std::uint8_t>(counter);
    second_position = static_cast<std::uint8_t>(0x1bU - counter);
    changed_people += set_original_facility_person_graphic(
        document, first_person, first_position, 0U) ? 1U : 0U;
    changed_people += set_original_facility_person_graphic(
        document, first_person + 1U, second_position, 1U) ? 1U : 0U;
  } else if (std::bit_cast<std::int32_t>(counter) < 19) {
    graphic = static_cast<std::uint8_t>(counter + 0xf4U);
    changed_people += set_original_facility_person_graphic(
        document, first_person, 13U, graphic) ? 1U : 0U;
    changed_people += set_original_facility_person_graphic(
        document, first_person + 1U, 13U, graphic) ? 1U : 0U;
  } else {
    changed_people += set_original_facility_person_graphic(
        document, first_person, 13U, 7U) ? 1U : 0U;
    changed_people += set_original_facility_person_graphic(
        document, first_person + 1U, 13U, 7U) ? 1U : 0U;
  }
  counter_changed = store_original_world_header_dword(
      document, 66U, counter + 1U);
  return true;
}

bool step_original_facility_people_dispatch(
    OriginalTdtDocument& document,
    OriginalTdtTenant& tenant,
    bool people_animation_enabled,
    bool control_modifier,
    std::size_t& changed_people,
    bool& cathedral_counter_changed) noexcept {
  // 1220:6ba9 reads the serialized tenant people index at +8. The apparent
  // runtime +0x0e address includes the six-byte floor header.
  const auto first_person = load_original_world_dword(
      tenant.exact_bytes, 8U, document.header.byte_swapped);
  if (first_person >= document.people_count ||
      first_person >= document.people.size()) {
    return false;
  }
  if (tenant.type < 0) {
    if (tenant.exact_bytes[13] == std::byte{0}) return false;
    return step_original_inactive_facility_person(
        document, first_person, original_facility_position_count(tenant),
        changed_people);
  }
  switch (tenant.type) {
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 9:
    case 12:
    case 40:
      break;
    default:
      return false;
  }
  if (tenant.exact_bytes[13] == std::byte{0}) {
    if (!people_animation_enabled) return false;
    const auto key =
        std::to_integer<std::uint8_t>(tenant.exact_bytes[12]);
    const auto signed_sum = static_cast<std::int16_t>(
        static_cast<std::int16_t>(document.header.frame_time) + key);
    if (signed_sum % 16 != 0) return false;
  }
  switch (tenant.type) {
    case 3:
    case 4:
    case 5:
      return step_original_hotel_facility_people(
          document, first_person, tenant.type, control_modifier,
          changed_people);
    case 6:
    case 12:
      return step_original_food_facility_people(
          document, first_person, tenant.type, changed_people);
    case 7:
      return step_original_office_facility_people(
          document, first_person, changed_people);
    case 9:
      return step_original_condo_facility_people(
          document, first_person, control_modifier, changed_people);
    case 40:
      return step_original_cathedral_facility_people(
          document, first_person, changed_people,
          cathedral_counter_changed);
    default:
      return false;
  }
}

int original_waiting_person_width(const OriginalTdtDocument& document,
                                  std::uint32_t person_index) {
  // Exact 10a8:1a88 waiting-line cell-width selector. Most people occupy one
  // cell; the Hotel couple, Housekeeping, and generic variants 5/7 use two.
  if (person_index >= document.people_count ||
      person_index >= document.people.size()) {
    return 0;
  }
  const auto& person = document.people[person_index].exact_bytes;
  const auto type = original_world_signed_byte(person[4]);
  const auto word_2 = load_original_world_word(
      person, 2U, document.header.byte_swapped);
  switch (type) {
    case 3:
    case 4:
    case 5:
    case 7:
    case 14:
      return 1;
    case 9:
      return word_2 == 1U ? 2 : 1;
    case 15:
      return 2;
    default:
      return (word_2 & 7U) == 5U || (word_2 & 7U) == 7U ? 2 : 1;
  }
}

int original_waiting_person_graphic_base(
    const OriginalTdtDocument& document,
    std::uint32_t person_index) {
  // Exact 10a8:1913 thirteen-way person-family/variant selector. Its return
  // value is the first cell of the ten-cell waiting-person atlas family.
  if (person_index >= document.people_count ||
      person_index >= document.people.size()) {
    return -1;
  }
  const auto& person = document.people[person_index].exact_bytes;
  const auto type = original_world_signed_byte(person[4]);
  const auto word_2 = load_original_world_word(
      person, 2U, document.header.byte_swapped);
  switch (type) {
    case 3:
    case 4:
    case 5:
      return word_2 == 2U ? 40 : 0;
    case 7:
      if (word_2 == 4U) return 20;
      if (word_2 == 5U) return 40;
      return word_2 <= 1U ? 10 : 0;
    case 9:
      if (word_2 == 1U) return 70;
      if (word_2 == 2U) return 30;
      return 0;
    case 14:
      return 80;
    case 15:
      return 50;
    default:
      switch (word_2 & 7U) {
        case 1:
          return 20;
        case 3:
          return 40;
        case 5:
          return 60;
        case 7:
          return 70;
        default:
          return 0;
      }
  }
}

std::optional<std::uint8_t> original_waiting_person_palette_override(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::uint32_t person_index,
    int floor,
    std::optional<std::int16_t> isolation_metric = std::nullopt) {
  if (person_index >= document.people_count ||
      person_index >= document.people.size()) {
    return std::nullopt;
  }
  // 10a8:1737 checks the periodic VIP before the named-person table.
  if (document.post_elevator.b928 != 0U &&
      document.post_elevator.b924 ==
          static_cast<std::int32_t>(person_index)) {
    return 0x7dU;
  }
  const std::size_t linked = std::min<std::size_t>(
      document.header.person_link_count,
      document.post_elevator.dce4_person_indices.size());
  for (std::size_t index = 0U; index < linked; ++index) {
    if (document.post_elevator.dce4_person_indices[index] ==
        static_cast<std::int32_t>(person_index)) {
      return 0x2dU;
    }
  }

  const auto& person = document.people[person_index].exact_bytes;
  std::int16_t signed_metric{};
  if (isolation_metric) {
    // 10a8:12c1's DS:b3ae branch reads this signed word directly from the
    // simulated live ring; unlike the normal branch, type 15 is not forced
    // to zero here.
    signed_metric = *isolation_metric;
  } else {
    std::uint16_t metric = 0U;
    if (original_world_signed_byte(person[4]) != 15) {
      std::uint16_t elapsed = static_cast<std::uint16_t>(
          document.header.frame_time - load_original_world_word(
              person, 10U, document.header.byte_swapped));
      // 11d8:0423 discounts the extra walking time across a two/three-story
      // Lobby only on floor ten.
      if (floor == 10 && document.header.lobby_height > 1U) {
        const std::uint16_t discount =
            document.header.lobby_height == 2U ? 25U :
            document.header.lobby_height == 3U ? 50U : 0U;
        // 11d8:0423 compares the wrapping word as signed. In particular, a
        // timestamp one tick ahead produces -1 and is reduced to zero rather
        // than treated as a 65535-tick wait.
        elapsed = std::bit_cast<std::int16_t>(elapsed) >
                          static_cast<std::int16_t>(discount)
            ? static_cast<std::uint16_t>(elapsed - discount)
            : 0U;
      }
      metric = static_cast<std::uint16_t>(
          (load_original_world_word(person, 12U,
                                    document.header.byte_swapped) & 0x03ffU) +
          elapsed);
    }
    signed_metric = std::bit_cast<std::int16_t>(metric);
  }

  const std::size_t band = document.header.rating <= 2U
                               ? 0U
                               : document.header.rating == 3U ? 1U : 2U;
  const auto lower = std::bit_cast<std::int16_t>(
      part.words_00_to_40[5U + band]);
  const auto upper = std::bit_cast<std::int16_t>(
      part.words_00_to_40[8U + band]);
  if (signed_metric >= upper) return 0x1aU;
  if (signed_metric >= lower) return 0x16U;
  return std::nullopt;
}

void draw_original_waiting_person_cell(
    const std::array<IndexedDib, 2>& graphics,
    const std::array<std::uint32_t, 256>& palette,
    int source_cell,
    std::optional<std::uint8_t> palette_override,
    int world_cell_x,
    int floor,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster) {
  // Exact 10a8:1875 selector plus 11a0:0eaf indexed-person compositor:
  // transparent index zero is skipped and marker channel 0xff receives the
  // VIP/wait-band/named-person palette override before native conversion.
  if (source_cell < 0) return;
  int source_x = source_cell * kOriginalCellWidth;
  const IndexedDib* source = nullptr;
  for (const auto& graphic : graphics) {
    if (source_x < graphic.view.width) {
      source = &graphic;
      break;
    }
    source_x -= graphic.view.width;
  }
  if (!source || source_x + kOriginalCellWidth > source->view.width) {
    throw std::runtime_error("Original person graphics atlas is truncated");
  }

  const int destination_x = world_cell_x * kOriginalCellWidth - view_x;
  const int destination_y =
      original_precomputed_floor_offset(119 - floor, 1) - view_y;
  for (int y = 0; y < kOriginalFloorHeight; ++y) {
    const int raster_y = destination_y + y;
    if (raster_y < 0 || raster_y >= raster.height) continue;
    for (int x = 0; x < kOriginalCellWidth; ++x) {
      const int raster_x = destination_x + x;
      if (raster_x < 0 || raster_x >= raster.width) continue;
      auto index = source->sample_index(source_x + x, y);
      if (index == 0U) continue;
      if (index == 0xffU && palette_override) {
        index = *palette_override;
      }
      raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                    raster_x] = palette[index];
    }
  }
}

void render_original_elevator_waiting_people(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalWorldPalette& palette,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster,
    const OriginalElevatorWaitingIsolationView* waiting_isolation) {
  // Exact 10a8:088c/10a8:0fff/10a8:12c1 two-lane waiting-ring presentation,
  // including the signed metric bands, source-cell selection, and one/two-cell
  // bodies.
  const std::array<IndexedDib, 2> graphics = {
      IndexedDib(resources.find("BITMAP", 1128)),
      IndexedDib(resources.find("BITMAP", 1129)),
  };
  const auto part = original_part_table(resources.find("PART", 1000));

  struct FloorElevator {
    std::size_t index{};
    int x{};
  };
  for (int floor = 0; floor < static_cast<int>(document.floors.size());
       ++floor) {
    std::array<FloorElevator, 24> shafts{};
    std::size_t shaft_count = 0U;
    for (std::size_t index = 0U; index < document.elevators.size(); ++index) {
      const auto& elevator = document.elevators[index];
      if (elevator.used == 0U ||
          floor < static_cast<int>(elevator.bottom_floor) - 1 ||
          floor > static_cast<int>(elevator.top_floor) + 1) {
        continue;
      }
      shafts[shaft_count++] = {index, static_cast<int>(elevator.x)};
    }

    // 10a8:00a8's diminishing-gap Shell sort. Direct comparisons swap only
    // strictly descending x values, but earlier gap passes can still reverse
    // equal-x entries indirectly, so preserve this sequence exactly.
    for (int gap = static_cast<int>(shaft_count);;) {
      gap /= 2;
      if (gap <= 0) break;
      for (int end = gap; end < static_cast<int>(shaft_count); ++end) {
        for (int left = end - gap; left >= 0; left -= gap) {
          const auto right = static_cast<std::size_t>(left + gap);
          if (shafts[static_cast<std::size_t>(left)].x <= shafts[right].x) {
            break;
          }
          std::swap(shafts[static_cast<std::size_t>(left)], shafts[right]);
        }
      }
    }

    const auto& floor_state = document.floors[static_cast<std::size_t>(floor)];
    for (std::size_t sequence = 0U; sequence < shaft_count; ++sequence) {
      const auto elevator_index = shafts[sequence].index;
      const auto& elevator = document.elevators[elevator_index];
      const auto mapped = original_elevator_floor_record_index(
          elevator.type, elevator.bottom_floor, elevator.top_floor,
          static_cast<std::int16_t>(floor));
      if (mapped < 0) continue;
      const auto record = std::find_if(
          elevator.floor_records.begin(), elevator.floor_records.end(),
          [&](const OriginalTdtElevatorFloorRecord& candidate) {
            return candidate.mapped_index == mapped;
          });
      if (record == elevator.floor_records.end()) continue;

      const OriginalTdtElevatorFloorRecord* identity_record = &*record;
      const bool isolation_record =
          waiting_isolation && waiting_isolation->saved_elevator &&
          waiting_isolation->elevator_index == elevator_index;
      if (isolation_record) {
        const auto saved = std::find_if(
            waiting_isolation->saved_elevator->floor_records.begin(),
            waiting_isolation->saved_elevator->floor_records.end(),
            [&](const OriginalTdtElevatorFloorRecord& candidate) {
              return candidate.mapped_index == mapped;
            });
        if (saved ==
            waiting_isolation->saved_elevator->floor_records.end()) {
          continue;
        }
        identity_record = &*saved;
      }

      // Exact 10a8:1cbb first-lane boundary: floor left edge unless the prior
      // shaft overlaps the lane, then use the original midpoint expression.
      int first_begin = floor_state.left_edge;
      if (sequence != 0U && shafts[sequence - 1U].x >= first_begin) {
        first_begin = shafts[sequence].x -
            ((shafts[sequence].x - shafts[sequence - 1U].x - 4) >> 1);
      }
      const int first_end = shafts[sequence].x - 2;
      const int shaft_width = elevator.type == 0U ? 6 : 4;
      const int second_begin = shafts[sequence].x + shaft_width + 2;
      // Exact 10a8:1d41 second-lane boundary: floor right edge unless the next
      // shaft overlaps the lane, then use the same midpoint expression.
      int second_end = floor_state.right_edge;
      if (sequence + 1U != shaft_count &&
          shafts[sequence + 1U].x <= second_end) {
        second_end = shafts[sequence + 1U].x -
            ((shafts[sequence + 1U].x - shafts[sequence].x - 4) >> 1);
      }

      const auto draw_lane = [&](bool first_lane, int begin, int end) {
        const std::size_t count_offset = first_lane ? 0U : 2U;
        const std::size_t cursor_offset = first_lane ? 1U : 3U;
        const std::size_t table_offset = first_lane ? 4U : 164U;
        const int count = original_world_signed_byte(
            record->exact_bytes[count_offset]);
        const auto ring_cursor = std::to_integer<std::uint8_t>(
            record->exact_bytes[cursor_offset]);
        if (count <= 0 || ring_cursor >= 40U) return;

        int position = first_lane ? end - 1 : begin;
        for (int ordinal = 0; ordinal < count; ++ordinal) {
          if (first_lane ? position < begin : position >= end) break;
          const auto slot =
              (ring_cursor + static_cast<std::size_t>(ordinal)) % 40U;
          const auto person_index = load_original_world_dword(
              identity_record->exact_bytes, table_offset + slot * 4U,
              document.header.byte_swapped);
          const int width = original_waiting_person_width(
              document, person_index);
          const int graphic_base = original_waiting_person_graphic_base(
              document, person_index);
          if (width == 0 || graphic_base < 0) break;
          const auto isolation_metric = isolation_record
              ? std::optional<std::int16_t>{std::bit_cast<std::int16_t>(
                    load_original_world_word(
                        record->exact_bytes, table_offset + slot * 4U,
                        document.header.byte_swapped))}
              : std::nullopt;
          const auto override = original_waiting_person_palette_override(
              document, part, person_index, floor, isolation_metric);

          if (first_lane) {
            draw_original_waiting_person_cell(
                graphics, palette, graphic_base + 9, override, position,
                floor, view_x, view_y, raster);
            if (width == 2) {
              --position;
              if (position >= begin) {
                draw_original_waiting_person_cell(
                    graphics, palette, graphic_base + 8, override, position,
                    floor, view_x, view_y, raster);
              }
            }
            --position;
          } else {
            draw_original_waiting_person_cell(
                graphics, palette, graphic_base, override, position, floor,
                view_x, view_y, raster);
            if (width == 2) {
              ++position;
              if (position < end) {
                draw_original_waiting_person_cell(
                    graphics, palette, graphic_base + 1, override, position,
                    floor, view_x, view_y, raster);
              }
            }
            ++position;
          }
        }
      };

      draw_lane(true, first_begin, first_end);
      draw_lane(false, second_begin, second_end);
    }
  }
}

void draw_original_elevator_transfer_person(
    const OriginalTdtDocument& document,
    const std::array<IndexedDib, 2>& graphics,
    const OriginalElevatorTransferVisual& visual,
    const OriginalWorldPalette& palette,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster) {
  if (visual.elevator_index >= document.elevators.size() ||
      visual.floor < 0 ||
      visual.floor >= static_cast<std::int16_t>(document.floors.size())) {
    return;
  }
  const auto& elevator = document.elevators[visual.elevator_index];
  if (elevator.used == 0U ||
      visual.floor < static_cast<std::int16_t>(elevator.bottom_floor) ||
      visual.floor > static_cast<std::int16_t>(elevator.top_floor)) {
    return;
  }
  const int graphic_base = original_waiting_person_graphic_base(
      document, visual.person_index);
  if (graphic_base < 0) return;

  // 10a8:0de6 has four literal car-edge placements. A true direction uses
  // two adjacent atlas cells; a false direction uses one. Boarding is
  // clipped strictly inside the floor's left edge, while alighting permits
  // equality at the right edge.
  int world_x{};
  int source_cell{};
  int width_cells{};
  if (visual.boarding) {
    world_x = static_cast<int>(elevator.x) -
              (visual.direction_up ? 2 : 1);
    source_cell = graphic_base + (visual.direction_up ? 5 : 7);
    width_cells = visual.direction_up ? 2 : 1;
    if (world_x <= document.floors[static_cast<std::size_t>(visual.floor)]
                       .left_edge) {
      return;
    }
  } else {
    world_x = static_cast<int>(elevator.x) +
              (visual.direction_up ? 4 : 6);
    source_cell = graphic_base + (visual.direction_up ? 3 : 2);
    width_cells = visual.direction_up ? 2 : 1;
    if (world_x > document.floors[static_cast<std::size_t>(visual.floor)]
                      .right_edge) {
      return;
    }
  }
  for (int cell = 0; cell < width_cells; ++cell) {
    draw_original_waiting_person_cell(
        graphics, palette, source_cell + cell, std::nullopt, world_x + cell,
        visual.floor, view_x, view_y, raster);
  }
}

void render_original_elevator_layer(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    std::span<const OriginalElevatorTransferVisual> visuals,
    const OriginalWorldPalette& palette,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster) {
  std::vector<OriginalElevatorTransferVisual> ordered{};
  ordered.reserve(visuals.size());
  for (const auto& visual : visuals) {
    // 10a8:022b overwrites only the matching floor/Elevator/side slot. Do
    // this before validating the final person's graphics: a later invalid
    // cache value must not reveal the previously retained sprite.
    const auto retained = std::find_if(
        ordered.begin(), ordered.end(), [&](const auto& candidate) {
          return candidate.elevator_index == visual.elevator_index &&
                 candidate.floor == visual.floor &&
                 candidate.boarding == visual.boarding;
        });
    if (retained == ordered.end()) {
      ordered.push_back(visual);
    } else {
      *retained = visual;
    }
  }
  const std::array<IndexedDib, 2> graphics = {
      IndexedDib(resources.find("BITMAP", 1128)),
      IndexedDib(resources.find("BITMAP", 1129)),
  };
  // Exact 10a8:0000 -> 02aa order: visible rows run top-to-bottom; each row
  // gathers all used shafts from indices 0..23, sorts them by x, draws one
  // floor of that shaft, then consumes its boarding and alighting slots.
  // This interleave lets a later right-hand shaft cover an earlier transfer
  // person when adjacent Elevator footprints overlap that sprite.
  for (int floor = 119; floor >= 0; --floor) {
    std::array<std::size_t, 24> floor_elevators{};
    std::size_t count = 0U;
    for (std::size_t elevator_index = 0U;
         elevator_index < document.elevators.size(); ++elevator_index) {
      const auto& elevator = document.elevators[elevator_index];
      if (elevator.used == 0U || elevator.type > 2U ||
          elevator.top_floor < elevator.bottom_floor ||
          floor < static_cast<int>(elevator.bottom_floor) - 1 ||
          floor > static_cast<int>(elevator.top_floor) + 1) {
        continue;
      }
      floor_elevators[count++] = elevator_index;
    }
    for (int gap = static_cast<int>(count);;) {
      gap /= 2;
      if (gap <= 0) break;
      for (int end = gap; end < static_cast<int>(count); ++end) {
        for (int left = end - gap; left >= 0; left -= gap) {
          const auto right = static_cast<std::size_t>(left + gap);
          if (document.elevators[
                  floor_elevators[static_cast<std::size_t>(left)]].x <=
              document.elevators[floor_elevators[right]].x) {
            break;
          }
          std::swap(floor_elevators[static_cast<std::size_t>(left)],
                    floor_elevators[right]);
        }
      }
    }
    for (std::size_t slot = 0U; slot < count; ++slot) {
      const auto elevator_index = floor_elevators[slot];
      const auto& elevator = document.elevators[elevator_index];
      render_original_elevator_at_floor(resources, elevator, palette, floor,
                                        view_x, view_y, raster);
      if (floor < static_cast<int>(elevator.bottom_floor) ||
          floor > static_cast<int>(elevator.top_floor)) {
        continue;
      }
      for (const bool boarding : {true, false}) {
        const auto visual = std::find_if(
            ordered.begin(), ordered.end(), [&](const auto& candidate) {
              return candidate.floor == floor &&
                     candidate.elevator_index == elevator_index &&
                     candidate.boarding == boarding;
            });
        if (visual != ordered.end()) {
          draw_original_elevator_transfer_person(
              document, graphics, *visual, palette, view_x, view_y, raster);
        }
      }
    }
  }
}

void draw_original_vertical_transport_dib_frame(
    const OriginalResources& resources,
    const OriginalWorldPalette& palette,
    int first_resource,
    int last_resource,
    int source_frame,
    int source_height,
    int destination_x,
    int destination_y,
    OriginalWorldRaster& raster) {
  constexpr int kTransportWidth = 8 * kOriginalCellWidth;
  const int top_padding = kOriginalFloorHeight - source_height;
  constexpr std::size_t kConstructionFillPaletteIndex = 14U;

  int source_x = source_frame * kTransportWidth;
  int remaining = kTransportWidth;
  int output_x = destination_x;
  for (int resource_id = first_resource;
       resource_id <= last_resource && remaining > 0; ++resource_id) {
    const IndexedDib strip(resources.find("BITMAP", resource_id));
    if (strip.height != source_height || strip.view.width <= 0) {
      throw std::runtime_error(
          "Original stair/escalator graphics strip has invalid dimensions");
    }
    if (source_x >= strip.view.width) {
      source_x -= strip.view.width;
      continue;
    }
    const int chunk = std::min(remaining, strip.view.width - source_x);
    for (int y = 0; y < kOriginalFloorHeight; ++y) {
      const int raster_y = destination_y + y;
      if (raster_y < 0 || raster_y >= raster.height) {
        continue;
      }
      for (int x = 0; x < chunk; ++x) {
        const int raster_x = output_x + x;
        if (raster_x < 0 || raster_x >= raster.width) {
          continue;
        }
        const auto color = y < top_padding
            ? palette[kConstructionFillPaletteIndex]
            : palette[strip.sample_index(source_x + x, y - top_padding)];
        raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                      raster_x] = color;
      }
    }
    remaining -= chunk;
    output_x += chunk;
    source_x = 0;
  }
  if (remaining != 0) {
    throw std::runtime_error(
        "Original stair/escalator graphics atlas is truncated");
  }
}

void render_original_normal_vertical_transport(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalTdtStairRecord& transport,
    const OriginalWorldPalette& palette,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster) {
  const bool stair = (transport.shape & 1U) != 0U;
  const bool active =
      static_cast<std::uint16_t>(transport.word_6 + transport.word_8) != 0U;
  const int period = stair ? 13 : 7;
  const int frame = active
      ? static_cast<int>(document.header.frame_time % period) + 1
      : 0;
  const int destination_x =
      static_cast<int>(transport.x) * kOriginalCellWidth - view_x;
  const int lower = transport.floor;

  // 10c0:007a uses type 0x17/0x16 for Stairs and 0x1c/0x1b for
  // Escalator. The former type-0x16 bank consists of 24-row DIBs placed at
  // the bottom of the original 36-row staging band over palette color 14.
  if (stair) {
    draw_original_vertical_transport_dib_frame(
        resources, palette, 2472, 2473, frame, 36, destination_x,
        (119 - lower) * kOriginalFloorHeight - view_y, raster);
    draw_original_vertical_transport_dib_frame(
        resources, palette, 2408, 2409, frame, 24, destination_x,
        (119 - (lower + 1)) * kOriginalFloorHeight - view_y, raster);
  } else {
    draw_original_vertical_transport_dib_frame(
        resources, palette, 2792, 2792, frame, 36, destination_x,
        (119 - lower) * kOriginalFloorHeight - view_y, raster);
    draw_original_vertical_transport_dib_frame(
        resources, palette, 2728, 2728, frame, 36, destination_x,
        (119 - (lower + 1)) * kOriginalFloorHeight - view_y, raster);
  }
}

void render_original_tall_vertical_transport(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalTdtStairRecord& transport,
    const OriginalWorldPalette& palette,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster) {
  const int height = static_cast<int>(transport.shape) / 2;
  const bool stair = (transport.shape & 1U) != 0U;
  const int period = stair ? 10 : 11;
  const int family_offset = stair ? 0 : (height + 2) * 11;
  const int animation =
      static_cast<int>(document.header.frame_time % period) +
      family_offset + 1;
  const int lobby_height = document.header.lobby_height;
  if (lobby_height < 2 || lobby_height > 3) {
    return;
  }

  // 11f8:0680 selects CGPK/(4071+lobbyHeight). These resources are already
  // in 11a0's cell-major form: each source frame is eight consecutive
  // 8x36 palette-indexed tiles. 10c0:0345 advances by 11 frames per Stair
  // story or 12 per Escalator story.
  const auto graphics = resources.find("CGPK", 4071 + lobby_height);
  const int destination_x =
      static_cast<int>(transport.x) * kOriginalCellWidth - view_x;
  for (int row = 0; row < height + 2; ++row) {
    const int floor = static_cast<int>(transport.floor) + height + 1 - row;
    const int destination_y =
        (119 - floor) * kOriginalFloorHeight - view_y;
    const int frame = animation + (period + 1) * row;
    for (int cell = 0; cell < 8; ++cell) {
      draw_cgpk_tile(graphics,
                     static_cast<std::size_t>(frame * 8 + cell), palette,
                     destination_x + cell * kOriginalCellWidth,
                     destination_y, raster);
    }
  }
}

void render_original_vertical_transports(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalWorldPalette& palette,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster) {
  for (const auto& transport : document.post_elevator.stairs_bd70) {
    if (transport.used == 0U) {
      continue;
    }
    if (transport.shape / 2U == 0U) {
      render_original_normal_vertical_transport(
          resources, document, transport, palette, view_x, view_y, raster);
    } else {
      render_original_tall_vertical_transport(
          resources, document, transport, palette, view_x, view_y, raster);
    }
  }
}

std::uint32_t sample_original_floor_atlas(
    const std::array<IndexedDib, 6>& atlas,
    const OriginalWorldPalette& palette,
    int x,
    int y) {
  if (x < 0 || y < 0 || y >= kOriginalFloorHeight) {
    throw std::runtime_error("Original fire source is outside its atlas");
  }
  for (const auto& strip : atlas) {
    if (x < strip.view.width) {
      return palette[strip.sample_index(x, y)];
    }
    x -= strip.view.width;
  }
  throw std::runtime_error("Original fire source exceeds its floor atlas");
}

std::uint32_t merge_original_nonzero_channels(std::uint32_t source,
                                              std::uint32_t destination) {
  // 11a0:0a11 routes one selected 36-row cache slice into 11a0:0cd9, which
  // independently chooses the source byte when that byte is nonzero,
  // otherwise retaining the destination byte. This is not ordinary whole-
  // pixel transparency and is intentionally reproduced channel-wise.
  constexpr std::array<std::uint32_t, 4> masks = {
      0xff000000U, 0x00ff0000U, 0x0000ff00U, 0x000000ffU};
  std::uint32_t result = 0U;
  for (const auto mask : masks) {
    result |= (source & mask) != 0U ? source & mask : destination & mask;
  }
  return result;
}

std::optional<std::uint8_t> sample_original_people_atlas_index(
    const std::array<IndexedDib, 2>& atlas,
    int x,
    int y) {
  if (x < 0 || y < 0 || y >= kOriginalFloorHeight) {
    return std::nullopt;
  }
  for (const auto& strip : atlas) {
    if (x < strip.view.width) {
      return strip.sample_index(x, y);
    }
    x -= strip.view.width;
  }
  return std::nullopt;
}

std::optional<std::uint8_t> sample_original_facility_people_atlas_index(
    const std::array<IndexedDib, 7>& atlas,
    int x,
    int y) {
  if (x < 0 || y < 0 || y >= 24) return std::nullopt;
  for (const auto& strip : atlas) {
    if (x < strip.view.width) return strip.sample_index(x, y);
    x -= strip.view.width;
  }
  return std::nullopt;
}

void draw_original_facility_person(
    const std::array<IndexedDib, 7>& people_atlas,
    const IndexedDib& cathedral_atlas,
    const OriginalWorldPalette& palette,
    int graphic,
    int world_cell_x,
    int floor,
    bool cathedral,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster) {
  if (graphic < 0) return;
  const int source_x = graphic * 2 * kOriginalCellWidth;
  const int source_height = cathedral ? kOriginalFloorHeight : 24;
  const int destination_x = world_cell_x * kOriginalCellWidth - view_x;
  const int destination_y =
      (119 - floor) * kOriginalFloorHeight +
      (cathedral ? 0 : kOriginalFloorHeight - source_height) - view_y;
  for (int y = 0; y < source_height; ++y) {
    const int raster_y = destination_y + y;
    if (raster_y < 0 || raster_y >= raster.height) continue;
    for (int x = 0; x < 2 * kOriginalCellWidth; ++x) {
      const int raster_x = destination_x + x;
      if (raster_x < 0 || raster_x >= raster.width) continue;
      const auto index = cathedral
          ? (source_x + x < cathedral_atlas.view.width
                 ? std::optional<std::uint8_t>{
                       cathedral_atlas.sample_index(source_x + x, y)}
                 : std::nullopt)
          : sample_original_facility_people_atlas_index(
                people_atlas, source_x + x, y);
      if (!index) continue;
      auto& destination =
          raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                        raster_x];
      destination = merge_original_nonzero_channels(
          palette[*index], destination);
    }
  }
}

void render_original_facility_people(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalWorldPalette& palette,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster) {
  // Exact recovered 1028:0000 dispatcher plus the 1028:00b6, 1028:01ae,
  // 1028:02ab, 1028:03a3, and 1028:049b family compositors. The native pass
  // reads the same process-updated person position/graphic bytes and samples
  // the original 11a0:0afc/11a0:0c01 atlases without the WinG staging DC.
  // 11f8:033a concatenates BITMAP/1512..1518 into the type-eight two-cell
  // person atlas consumed by 11a0:0afc. Type 40 instead uses the 36-row
  // BITMAP/3560 atlas through 11a0:0c01.
  const std::array<IndexedDib, 7> people_atlas = {
      IndexedDib(resources.find("BITMAP", 1512)),
      IndexedDib(resources.find("BITMAP", 1513)),
      IndexedDib(resources.find("BITMAP", 1514)),
      IndexedDib(resources.find("BITMAP", 1515)),
      IndexedDib(resources.find("BITMAP", 1516)),
      IndexedDib(resources.find("BITMAP", 1517)),
      IndexedDib(resources.find("BITMAP", 1518)),
  };
  const IndexedDib cathedral_atlas(resources.find("BITMAP", 3560));
  const auto people_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  const bool cathedral_event =
      (load_original_world_header_word(document, 60U) & 4U) != 0U;

  for (std::size_t floor_index = 0U;
       floor_index < document.floors.size(); ++floor_index) {
    for (const auto& tenant : document.floors[floor_index].tenants) {
      if (!original_facility_is_visible(
              tenant, floor_index, view_x, view_y,
              raster.width, raster.height)) {
        continue;
      }
      const auto count = original_facility_people_owned_count(tenant.type);
      if (count == 0U || (tenant.type == 40 && !cathedral_event)) continue;
      const auto first_person = load_original_world_dword(
          tenant.exact_bytes, 8U, document.header.byte_swapped);
      if (first_person >= people_limit) continue;
      const bool inactive = tenant.type < 0;
      const bool food = tenant.type == 6 || tenant.type == 12;
      const bool cathedral = tenant.type == 40;
      for (std::size_t ordinal = 0U; ordinal < count; ++ordinal) {
        const auto person_index =
            static_cast<std::uint64_t>(first_person) + ordinal;
        if (person_index >= people_limit) break;
        const auto& person =
            document.people[static_cast<std::size_t>(person_index)];
        if (!inactive && !original_facility_person_is_present(person)) {
          continue;
        }
        const int position = original_world_signed_byte(person.exact_bytes[7]);
        const int graphic = original_world_signed_byte(person.exact_bytes[8]);
        if (position < 0) continue;
        if (food) {
          const int span = static_cast<int>(tenant.right - tenant.left);
          for (int block = 0; block < span; block += 4) {
            if (block + position + 1 >= span) continue;
            draw_original_facility_person(
                people_atlas, cathedral_atlas, palette, graphic,
                static_cast<int>(tenant.left) + block + position,
                static_cast<int>(floor_index), false, view_x, view_y, raster);
          }
        } else {
          draw_original_facility_person(
              people_atlas, cathedral_atlas, palette, graphic,
              static_cast<int>(tenant.left) + position,
              static_cast<int>(floor_index), cathedral, view_x, view_y,
              raster);
        }
      }
    }
  }
}

void render_original_security_responders(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalWorldPalette& palette,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster) {
  // 10f8:0108 draws the registered Security responders during either the
  // bomb (bit zero) or fire (bit three) event.
  if ((static_cast<std::uint8_t>(
           load_original_world_header_word(document, 60U)) &
       9U) == 0U) {
    return;
  }

  const std::array<IndexedDib, 2> atlas = {
      IndexedDib(resources.find("BITMAP", 1128)),
      IndexedDib(resources.find("BITMAP", 1129)),
  };
  const auto people_limit =
      std::min<std::size_t>(document.people_count, document.people.size());

  // DS:cf88 is ten packed words. 10f8:011b advances its byte offset by two
  // and repeats ten times, so every entry participates. The low byte is a
  // signed floor and the high byte is that floor's +0xa92 tenant key.
  for (const auto slot : document.post_elevator.cf88_words) {
    const int owner_floor = std::bit_cast<std::int8_t>(
        static_cast<std::uint8_t>(slot));
    const int owner_key = std::bit_cast<std::int8_t>(
        static_cast<std::uint8_t>(slot >> 8U));
    if (owner_floor < 0 ||
        owner_floor >= static_cast<int>(document.floors.size()) ||
        owner_key < 0) {
      continue;
    }
    const auto& floor = document.floors[static_cast<std::size_t>(owner_floor)];
    if (static_cast<std::size_t>(owner_key) >= floor.tenant_index.size()) {
      continue;
    }
    const auto tenant_index =
        floor.tenant_index[static_cast<std::size_t>(owner_key)];
    if (tenant_index >= floor.tenants.size()) {
      continue;
    }
    const auto first_person = load_original_world_dword(
        floor.tenants[tenant_index].exact_bytes, 8U,
        document.header.byte_swapped);

    // 1220:6ba9's runtime +0x0e includes the six-byte floor header, so the
    // serialized/native field is tenant byte +8. The caller visits six
    // consecutive 16-byte people records for each Security facility.
    for (std::uint32_t ordinal = 0U; ordinal < 6U; ++ordinal) {
      const std::uint64_t person_index =
          static_cast<std::uint64_t>(first_person) + ordinal;
      if (person_index >= people_limit) {
        break;
      }
      const auto& person =
          document.people[static_cast<std::size_t>(person_index)].exact_bytes;
      if (person[5] != std::byte{0} ||
          load_original_world_word(person, 10U,
                                   document.header.byte_swapped) != 0U) {
        continue;
      }

      const int floor_number = original_world_signed_byte(person[7]);
      const int world_cell_x = std::bit_cast<std::int16_t>(
          load_original_world_word(person, 14U,
                                   document.header.byte_swapped));
      if (floor_number < 0 ||
          floor_number >= static_cast<int>(document.floors.size())) {
        continue;
      }
      const int destination_x = world_cell_x * kOriginalCellWidth - view_x;
      const int destination_y =
          (119 - floor_number) * kOriginalFloorHeight - view_y;
      // 10f8:0222-0234 rejects the draw unless its full two-cell width is
      // present in the visible cell range. Vertical clipping remains inside
      // 11a0:1144's selected floor row.
      if (destination_x < 0 ||
          destination_x + 2 * kOriginalCellWidth > raster.width ||
          destination_y >= raster.height ||
          destination_y + kOriginalFloorHeight <= 0) {
        continue;
      }

      const int source_cell =
          90 + original_world_signed_byte(person[8]);
      const int source_x = source_cell * kOriginalCellWidth;
      for (int y = 0; y < kOriginalFloorHeight; ++y) {
        const int raster_y = destination_y + y;
        if (raster_y < 0 || raster_y >= raster.height) {
          continue;
        }
        for (int x = 0; x < 2 * kOriginalCellWidth; ++x) {
          const auto index =
              sample_original_people_atlas_index(atlas, source_x + x, y);
          if (!index) {
            continue;
          }
          auto& destination =
              raster.pixels[static_cast<std::size_t>(raster_y) *
                                raster.width +
                            destination_x + x];
          destination = merge_original_nonzero_channels(
              palette[*index], destination);
        }
      }
    }
  }
}

void draw_original_fire_frame(const std::array<IndexedDib, 6>& atlas,
                              const OriginalWorldPalette& palette,
                              int frame,
                              int world_cell_x,
                              int floor,
                              int view_x,
                              int view_y,
                              OriginalWorldRaster& raster) {
  constexpr int kFrameWidthCells = 12;
  const int source_x = frame * kFrameWidthCells * kOriginalCellWidth;
  const int destination_x = world_cell_x * kOriginalCellWidth - view_x;
  const int destination_y =
      (119 - floor) * kOriginalFloorHeight - view_y;
  for (int y = 0; y < kOriginalFloorHeight; ++y) {
    const int raster_y = destination_y + y;
    if (raster_y < 0 || raster_y >= raster.height) {
      continue;
    }
    for (int x = 0; x < kFrameWidthCells * kOriginalCellWidth; ++x) {
      const int raster_x = destination_x + x;
      if (raster_x < 0 || raster_x >= raster.width) {
        continue;
      }
      const auto source =
          sample_original_floor_atlas(atlas, palette, source_x + x, y);
      auto& destination =
          raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                        raster_x];
      destination = merge_original_nonzero_channels(source, destination);
    }
  }
}

std::array<IndexedDib, 6> original_fire_atlas(
    const OriginalResources& resources) {
  return {
      IndexedDib(resources.find("BITMAP", 3944)),
      IndexedDib(resources.find("BITMAP", 3945)),
      IndexedDib(resources.find("BITMAP", 3946)),
      IndexedDib(resources.find("BITMAP", 3947)),
      IndexedDib(resources.find("BITMAP", 3948)),
      IndexedDib(resources.find("BITMAP", 3949)),
  };
}

void render_original_fire_bands(const OriginalResources& resources,
                                const OriginalTdtDocument& document,
                                const OriginalWorldPalette& palette,
                                int view_x,
                                int view_y,
                                OriginalWorldRaster& raster) {
  if ((static_cast<std::uint8_t>(
           load_original_world_header_word(document, 60U)) &
       8U) == 0U) {
    return;
  }
  const int frame =
      static_cast<int>(document.header.frame_time % 4U) +
      (load_original_world_header_word(document, 72U) != 0U ? 4 : 0);
  const auto atlas = original_fire_atlas(resources);
  for (std::size_t floor = 0; floor < document.floors.size(); ++floor) {
    const auto left_edge = std::bit_cast<std::int16_t>(
        document.floors[floor].left_edge);
    const auto right_edge = std::bit_cast<std::int16_t>(
        document.floors[floor].right_edge);
    for (const auto base : {320U, 80U}) {
      const auto x = std::bit_cast<std::int16_t>(
          load_original_world_header_word(document, base + floor * 2U));
      const auto right = std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(
              static_cast<std::uint16_t>(x) + 12U));
      if (x >= 0 && left_edge <= x && right <= right_edge) {
        draw_original_fire_frame(atlas, palette, frame, x,
                                 static_cast<int>(floor), view_x, view_y,
                                 raster);
      }
    }
  }
}

void render_original_fire_crew(const OriginalResources& resources,
                               const OriginalTdtDocument& document,
                               const OriginalWorldPalette& palette,
                               int view_x,
                               int view_y,
                               OriginalWorldRaster& raster) {
  if ((static_cast<std::uint8_t>(
           load_original_world_header_word(document, 60U)) &
       8U) == 0U) {
    return;
  }
  const auto x = std::bit_cast<std::int16_t>(
      load_original_world_header_word(document, 78U));
  const auto floor = std::bit_cast<std::int16_t>(
      load_original_world_header_word(document, 76U));
  if (x <= 0 || floor < 0 ||
      static_cast<std::size_t>(floor) >= document.floors.size()) {
    return;
  }
  const auto atlas = original_fire_atlas(resources);
  draw_original_fire_frame(
      atlas, palette, 8, x, floor, view_x, view_y, raster);
}

void render_original_annual_effect(const OriginalResources& resources,
                                   const OriginalTdtDocument& document,
                                   const OriginalWorldPalette& palette,
                                   int view_x,
                                   int view_y,
                                   OriginalWorldRaster& raster) {
  const auto& state = document.post_elevator.version_18_dd6c;
  if (state.size() < 8U || state[0] == std::byte{0}) {
    return;
  }

  // 11b8:014b and 11b8:020b are the two clipped-rectangle helpers used by
  // the annual pass. 1048:00ad packs BITMAP/904 into DS:7756 at
  // (0,141)-(140,189) of the
  // shared WinG sheet. 11b8:0089 blits that exact rectangle at dd70/dd72
  // through 1208:071f with palette index zero as the transparent marker.
  // The source DIB indices are displayed through 1020:0019/098b's selected
  // WinG palette, including its time-of-day and event color transitions.
  const IndexedDib graphic(resources.find("BITMAP", 904));
  if (graphic.view.width != 140 || graphic.height != 48) {
    throw std::runtime_error(
        "Original annual-effect BITMAP/904 has invalid dimensions");
  }
  const int destination_x = static_cast<std::int16_t>(
      load_original_world_word(state, 4U, false)) - view_x;
  const int destination_y = static_cast<std::int16_t>(
      load_original_world_word(state, 6U, false)) - view_y;
  for (int y = 0; y < graphic.height; ++y) {
    const int raster_y = destination_y + y;
    if (raster_y < 0 || raster_y >= raster.height) {
      continue;
    }
    for (int x = 0; x < graphic.view.width; ++x) {
      const int raster_x = destination_x + x;
      if (raster_x < 0 || raster_x >= raster.width) {
        continue;
      }
      const auto index = graphic.sample_index(x, y);
      if (index == 0U) {
        continue;
      }
      raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                    raster_x] = palette[index];
    }
  }
}

template <std::size_t N>
void draw_original_masked_indexed_fragment(
    const std::array<std::uint8_t, N>& indices,
    int fragment_width,
    int fragment_height,
    const OriginalWorldPalette& palette,
    int destination_x,
    int destination_y,
    OriginalWorldRaster& raster) {
  if (fragment_width <= 0 || fragment_height <= 0 ||
      static_cast<std::size_t>(fragment_width) * fragment_height != N) {
    throw std::runtime_error("Original floor-edge fragment has invalid dimensions");
  }
  for (int y = 0; y < fragment_height; ++y) {
    const int raster_y = destination_y + y;
    if (raster_y < 0 || raster_y >= raster.height) continue;
    for (int x = 0; x < fragment_width; ++x) {
      const int raster_x = destination_x + x;
      if (raster_x < 0 || raster_x >= raster.width) continue;
      const auto index = indices[static_cast<std::size_t>(y) *
                                     fragment_width + x];
      // Every 11c0:0000 exterior-fragment call supplies zero as 1208:071f's
      // transparency marker. 1248:0000 therefore selects 1250:0024, whose
      // byte loop leaves destination bytes unchanged when the source equals
      // that marker. This is especially visible in BITMAP/1002: copying its
      // white palette-zero background opaquely produces the large white box
      // around the rooftop crane seen in the native port.
      if (index == 0U) continue;
      raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                    raster_x] = palette[index];
    }
  }
}

void render_original_floor_edges(const OriginalResources& resources,
                                 const OriginalTdtDocument& document,
                                 const OriginalWorldPalette& palette,
                                 int view_x,
                                 int view_y,
                                 OriginalWorldRaster& raster) {
  // Exact 11c0:0000 edge pass and recovered 11c0:0232 roof-state reset,
  // 11c0:024a top-floor scan, 11c0:02c0/11c0:0374/11c0:0428 blitters,
  // 11c0:0483/11c0:04ce/11c0:0518 source-rectangle helpers, and
  // 11c0:054e rectangle-delta helper. The Win16
  // implementation copies three rectangles from the shared sheet assembled
  // by 11f8:033a. Reconstruct those rectangles from the same DIB resources so
  // the native port does not depend on a process-local WinG staging bitmap.
  const IndexedDib floor_bank(resources.find("BITMAP", 1001));
  const IndexedDib roof(resources.find("BITMAP", 1002));
  const IndexedDib type3_bank(resources.find("BITMAP", 1193));
  const IndexedDib type4_bank(resources.find("BITMAP", 1259));
  const IndexedDib padding(resources.find("BITMAP", 5000));
  if (floor_bank.view.width != 112 || floor_bank.height != 36 ||
      roof.view.width != 36 || roof.height != 36 ||
      type3_bank.view.width != 256 || type3_bank.height != 24 ||
      type4_bank.view.width != 384 || type4_bank.height != 24 ||
      padding.view.width != 128 || padding.height != 12) {
    throw std::runtime_error("Original floor-edge source DIB has invalid dimensions");
  }

  std::array<std::uint8_t, 36U * 24U> standard_left{};
  std::array<std::uint8_t, 36U * 24U> standard_right{};
  std::array<std::uint8_t, 36U * 56U> ground_left{};
  std::array<std::uint8_t, 36U * 56U> ground_right{};
  std::array<std::uint8_t, 36U * 36U> roof_fragment{};
  const auto build_standard = [&](auto& fragment, int local_x) {
    for (int y = 0; y < 24; ++y) {
      for (int x = 0; x < 36; ++x) {
        fragment[static_cast<std::size_t>(y) * 36U + x] =
            y < 12
                ? padding.sample_index((local_x + x) % padding.view.width, y)
                : type4_bank.sample_index(local_x + x, y - 12);
      }
    }
  };
  // Sheet x=736/760 lies at local x=256/280 in BITMAP/1259. Its twelve-row
  // pad is tiled by 11f8:3ef3 from BITMAP/5000 starting at BITMAP/1259's
  // sheet origin.
  build_standard(standard_left, 256);
  build_standard(standard_right, 280);

  const auto build_ground = [&](auto& fragment, int floor_local_x,
                                int type3_local_x) {
    for (int y = 0; y < 56; ++y) {
      for (int x = 0; x < 36; ++x) {
        std::uint8_t index{};
        if (y < 36) {
          index = floor_bank.sample_index(floor_local_x + x, y);
        } else if (y < 48) {
          index = padding.sample_index(
              (type3_local_x + x) % padding.view.width, y - 36);
        } else {
          index = type3_bank.sample_index(type3_local_x + x, y - 48);
        }
        fragment[static_cast<std::size_t>(y) * 36U + x] = index;
      }
    }
  };
  // Sheet x=96/152 crosses the type-0 row, the twelve-row type-3 padding,
  // and BITMAP/1193 at local x=64/120.
  build_ground(ground_left, 0, 64);
  build_ground(ground_right, 56, 120);
  for (int y = 0; y < 36; ++y) {
    for (int x = 0; x < 36; ++x) {
      roof_fragment[static_cast<std::size_t>(y) * 36U + x] =
          roof.sample_index(x, y);
    }
  }

  for (std::size_t floor_number = 10U;
       floor_number <= 109U && floor_number < document.floors.size();
       ++floor_number) {
    const auto& floor = document.floors[floor_number];
    if (floor.tenants.empty()) continue;
    const int destination_y =
        (119 - static_cast<int>(floor_number)) * kOriginalFloorHeight - view_y;
    draw_original_masked_indexed_fragment(
        standard_left, 36, 24, palette,
        static_cast<int>(floor.left_edge) * kOriginalCellWidth - view_x - 24,
        destination_y, raster);
    draw_original_masked_indexed_fragment(
        standard_right, 36, 24, palette,
        static_cast<int>(floor.right_edge) * kOriginalCellWidth - view_x,
        destination_y, raster);
    if (floor_number == 10U) {
      draw_original_masked_indexed_fragment(
          ground_left, 36, 56, palette,
          static_cast<int>(floor.left_edge) * kOriginalCellWidth - view_x - 56,
          destination_y, raster);
      draw_original_masked_indexed_fragment(
          ground_right, 36, 56, palette,
          static_cast<int>(floor.right_edge) * kOriginalCellWidth - view_x,
          destination_y, raster);
    }
  }

  // 11c0:024a stops at the highest nonempty floor. Only a span wider than six
  // cells becomes DS:b3a4/b3a2; 11c0:0000 then draws BITMAP/1002 one story
  // above that floor, provided the selected floor is at most 109.
  for (std::size_t reverse = document.floors.size(); reverse != 0U; --reverse) {
    const std::size_t floor_number = reverse - 1U;
    const auto& floor = document.floors[floor_number];
    if (floor.tenants.empty()) continue;
    if (floor_number <= 109U && floor.right_edge - floor.left_edge > 6U) {
      draw_original_masked_indexed_fragment(
          roof_fragment, 36, 36, palette,
          static_cast<int>(floor.left_edge) * kOriginalCellWidth - view_x,
          (118 - static_cast<int>(floor_number)) * kOriginalFloorHeight - view_y,
          raster);
    }
    break;
  }
}

}  // namespace

OriginalSkyDecorationStepResult step_original_sky_decorations(
    const OriginalResources& resources,
    OriginalTdtDocument& document,
    OriginalSkyDecorationState& state,
    int view_x,
    int view_y,
    int client_width,
    int client_height) {
  OriginalSkyDecorationStepResult result{};
  // 1080:01cb rounds the client up to cells/floors and adds one unit. 083f
  // intersects that logical viewport with (0,360)-(3000,3888).
  const int visible_cells = (std::max(0, client_width) + 7) / 8 + 1;
  const int visible_floors = (std::max(0, client_height) + 35) / 36 + 1;
  const int left = std::max(view_x, 0);
  const int top = std::max(view_y, 360);
  const int right = std::min(view_x + visible_cells * 8, 3000);
  const int bottom = std::min(view_y + visible_floors * 36, 3888);
  if (right <= left || bottom <= top) return result;

  const std::array<IndexedDib, 4> graphics = {
      IndexedDib(resources.find("BITMAP", 900)),
      IndexedDib(resources.find("BITMAP", 901)),
      IndexedDib(resources.find("BITMAP", 902)),
      IndexedDib(resources.find("BITMAP", 903)),
  };
  const auto fully_visible = [&](const OriginalSkyDecorationPlacement& item) {
    return item.valid() && item.left >= left && item.top >= top &&
           item.right <= right && item.bottom <= bottom;
  };
  for (auto& placement : state.placements) {
    if (fully_visible(placement)) {
      ++result.visible;
      continue;
    }

    // 1048:0717 takes abs(rand()) before each IDIV. Microsoft rand() is
    // already nonnegative, so the native stream value can be used directly.
    const auto bitmap_index = static_cast<std::int16_t>(
        next_original_world_random(document) % graphics.size());
    const auto& graphic = graphics[static_cast<std::size_t>(bitmap_index)];
    const int available_width = right - left;
    const int available_height = bottom - top;
    OriginalSkyDecorationPlacement replacement{};
    if (available_width > graphic.view.width &&
        available_height > graphic.height) {
      replacement.bitmap_index = bitmap_index;
      replacement.left = left + static_cast<int>(
          next_original_world_random(document) %
          static_cast<std::uint16_t>(available_width - graphic.view.width));
      replacement.top = top + static_cast<int>(
          next_original_world_random(document) %
          static_cast<std::uint16_t>(available_height - graphic.height));
      replacement.right = replacement.left + graphic.view.width;
      replacement.bottom = replacement.top + graphic.height;
      ++result.visible;
    }
    if (placement != replacement) result.changed = true;
    placement = replacement;
    ++result.repositioned;
  }
  return result;
}

OriginalFacilityPeopleStepResult step_original_visible_facility_people(
    OriginalTdtDocument& document,
    int view_x,
    int view_y,
    int width,
    int height,
    bool people_animation_enabled,
    bool control_modifier) {
  OriginalFacilityPeopleStepResult result{};
  // 1038:050e walks DS:7962's screen cache from its top row downward and from
  // left to right within each row. It clears the remaining cells for a tenant
  // after the first cell, so each visible tenant is dispatched once. Preserve
  // that order because every family consumes the shared Microsoft RNG stream.
  // Native save records do not retain the cache, so recover the same identity
  // and ordering from visible tenant rectangles.
  for (std::size_t reverse_floor = document.floors.size();
       reverse_floor != 0U; --reverse_floor) {
    const std::size_t floor_index = reverse_floor - 1U;
    std::vector<OriginalTdtTenant*> visible;
    for (auto& tenant : document.floors[floor_index].tenants) {
      if (!original_facility_is_visible(
              tenant, floor_index, view_x, view_y, width, height)) {
        continue;
      }
      visible.push_back(&tenant);
    }
    std::stable_sort(
        visible.begin(), visible.end(),
        [](const OriginalTdtTenant* left, const OriginalTdtTenant* right) {
          return left->left < right->left;
        });
    for (auto* tenant : visible) {
      ++result.visible_tenants;
      if (step_original_facility_people_dispatch(
              document, *tenant, people_animation_enabled, control_modifier,
              result.changed_people, result.cathedral_counter_changed)) {
        ++result.dispatched_tenants;
      }
    }
  }
  return result;
}

OriginalWorldPalette original_world_palette(
    const OriginalResources& resources,
    const OriginalTdtDocument* document) {
  auto destination = decode_original_clut(resources.find("CLUT", 1000));
  if (!document) {
    for (std::size_t index = 188U; index < 194U; ++index) {
      store_dynamic_palette_color(destination, index, destination[index]);
    }
    return destination;
  }
  apply_original_time_palette(resources, *document, destination);
  return destination;
}

OriginalLogicalPaletteEntries original_logical_palette_entries(
    const OriginalWorldPalette& palette) noexcept {
  OriginalLogicalPaletteEntries entries{};
  for (std::size_t index = 0U; index < 255U; ++index) {
    const auto color = palette[index];
    entries[index] = {
        .red = static_cast<std::uint8_t>(color >> 16U),
        .green = static_cast<std::uint8_t>(color >> 8U),
        .blue = static_cast<std::uint8_t>(color),
        .flags = static_cast<std::uint8_t>(
            index < 188U ? 4U : (index <= 218U ? 1U : 0U)),
    };
  }
  // 1020:0f2d writes a final zero DWORD rather than consuming a CLUT record.
  entries[255] = {};
  return entries;
}

HPALETTE create_original_logical_palette(
    const OriginalWorldPalette& palette) noexcept {
  struct OriginalNativeLogPalette {
    WORD version;
    WORD count;
    std::array<PALETTEENTRY, 256> entries;
  } native_palette{0x0300U, 0x0100U, {}};

  const auto original_entries = original_logical_palette_entries(palette);
  for (std::size_t index = 0U; index < original_entries.size(); ++index) {
    native_palette.entries[index] = {
        original_entries[index].red,
        original_entries[index].green,
        original_entries[index].blue,
        original_entries[index].flags,
    };
  }
  return CreatePalette(
      reinterpret_cast<const LOGPALETTE*>(&native_palette));
}

void reset_original_palette_runtime(
    const OriginalResources& resources,
    const OriginalTdtDocument* document,
    OriginalPaletteRuntime& state,
    std::uint32_t now_tick) {
  state.colors = original_world_palette(resources, document);
  state.last_effect_tick = now_tick;
  state.effect_counter = 0U;
  state.initialized = true;
}

bool refresh_original_time_palette(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    OriginalPaletteRuntime& state) {
  if (!state.initialized) {
    reset_original_palette_runtime(resources, &document, state, 0U);
    return true;
  }
  const auto before = state.colors;
  apply_original_time_palette(resources, document, state.colors);
  return state.colors != before;
}

bool step_original_effect_palette(
    const OriginalTdtDocument& document,
    OriginalPaletteRuntime& state,
    bool effects_enabled,
    std::uint32_t now_tick) noexcept {
  // Exact 1020:053e host wrapper around the 1020:00cb effect-palette step.
  if (!state.initialized || !effects_enabled) return false;

  // 1020:00e3 subtracts the retained DWORD with wrapping arithmetic, then
  // 1000:39ea takes its signed magnitude before a signed comparison to 15.
  const std::uint32_t magnitude =
      original_tick_magnitude_delta(now_tick, state.last_effect_tick);
  if (std::bit_cast<std::int32_t>(magnitude) < 15) return false;

  apply_original_effect_colors(state.colors, state.effect_counter);
  if ((load_original_world_header_word(document, 60U) & 0x10U) != 0U &&
      std::bit_cast<std::int16_t>(document.header.frame_time) > 80 &&
      std::bit_cast<std::int16_t>(document.header.frame_time) < 1490) {
    apply_original_special_effect_colors(
        state.colors, state.effect_counter);
  }
  ++state.effect_counter;
  state.last_effect_tick = now_tick;
  return true;
}

std::uint32_t OriginalWorldRaster::at(int x, int y) const {
  if (x < 0 || y < 0 || x >= width || y >= height) {
    throw std::out_of_range("Original world raster coordinate is outside the image");
  }
  return pixels[static_cast<std::size_t>(y) * width + x];
}

std::optional<OriginalElevatorCarVisual> original_elevator_car_visual(
    const OriginalTdtElevator& elevator,
    std::size_t car_index,
    int view_x,
    int view_y,
    int client_height) noexcept {
  if (car_index >= elevator.car_records.size()) return std::nullopt;
  const auto& car = elevator.car_records[car_index].exact_bytes;
  if (car[15] == std::byte{0}) return std::nullopt;

  // 1090:227b admits only cars whose stored floor lies in DS:777c's aligned
  // top row through DS:7780's visible-row count. Motion can subsequently
  // move the 31-pixel rectangle across one of those row boundaries.
  const int current_floor = static_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(car[0]));
  const int aligned_view_y = view_y - view_y % kOriginalFloorHeight;
  const int top_floor = 119 - aligned_view_y / kOriginalFloorHeight;
  const int visible_floors =
      (std::max(0, client_height) + 35) / kOriginalFloorHeight + 1;
  const int row = top_floor - current_floor;
  if (row < 0 || row >= visible_floors) return std::nullopt;

  const int car_width = elevator.type == 0U ? 48 : 32;
  int source_bank_x = 0;
  if (elevator.type == 0U) {
    source_bank_x = 384;
  } else if (elevator.type == 2U) {
    source_bank_x = 224;
  }

  // 1090:221f selects crowd silhouettes by the car's signed occupancy:
  // exact frames for zero/one, a shared frame for two/three, then the
  // not-full/full pair selected against the signed elevator capacity.
  const int occupancy = static_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(car[3]));
  int frame = occupancy;
  if (occupancy > 1 && occupancy <= 3) {
    frame = 2;
  } else if (occupancy > 3) {
    const int capacity = static_cast<std::int8_t>(elevator.capacity);
    frame = capacity > occupancy ? 3 : 4;
  }
  if (frame < 0 || frame > 4) return std::nullopt;

  const int progress = static_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(car[1]));
  const int motion = progress * 6 * (car[4] != std::byte{0} ? 1 : -1);
  return OriginalElevatorCarVisual{
      source_bank_x + frame * car_width + 2,
      4,
      car_width - 4,
      31,
      static_cast<int>(elevator.x) * kOriginalCellWidth - view_x + 2,
      original_precomputed_floor_offset(119 - current_floor, 1) - view_y +
          motion + 5,
  };
}

OriginalWorldPoint original_initial_view(int client_width, int client_height) {
  // 1080:00d7 clamps the saved view to the scrollbar ranges, rebuilds the
  // two process-local presentation caches, and installs both scroll
  // positions. Native caches are rendered on demand, so this preserves the
  // exact clamped view calculation without a WinG rebuild side effect.
  return {
      std::max(0, (kOriginalWorldWidth - client_width) / 2),
      std::max(0, kOriginalWorldHeight - client_height -
                      kOriginalWorldBottomMargin),
  };
}

std::string original_scroll_floor_label(int view_y, int client_height) {
  // 1080:017f aligns the vertical scroll position down to a 36-pixel floor
  // band and stores its top-floor index in DS:777c. 1080:01cb computes the
  // visible-band count in DS:7780. 1080:0b26 then labels the center band.
  const int aligned_view_y =
      view_y - view_y % kOriginalFloorHeight;
  const int top_floor = 119 - aligned_view_y / kOriginalFloorHeight;
  const int visible_floors =
      (std::max(0, client_height) + 35) / kOriginalFloorHeight + 1;
  const int label_floor = top_floor - visible_floors / 2 - 9;
  if (label_floor <= 0) {
    return "B" + std::to_string(1 - label_floor);
  }
  return std::to_string(label_floor);
}

OriginalWorldPoint original_facility_focus_view(int facility_x,
                                                int facility_floor,
                                                int client_width,
                                                int client_height) noexcept {
  // 1080:01cb uses signed IDIV after adding one less than each unit size,
  // then increments the quotient. Native client dimensions are nonnegative,
  // so these expressions are the exact positive-input equivalents.
  const int visible_cells = (client_width + 7) / kOriginalCellWidth + 1;
  const int visible_floors =
      (client_height + 35) / kOriginalFloorHeight + 1;
  return {
      (facility_x - visible_cells / 2) * kOriginalCellWidth,
      (120 - facility_floor - 1 - visible_floors / 2) *
              kOriginalFloorHeight +
          12,
  };
}

namespace {

std::optional<int> original_world_map_overlay_tile(
    const OriginalTdtTenant& tenant,
    std::uint16_t mode) noexcept {
  // 11d0:0363 returns one-based strip numbers to 11d0:0145, which
  // decrements them before calling 11e0:0efb. A zero return skips painting.
  if (mode == 1U) {
    switch (std::bit_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(tenant.exact_bytes[15]))) {
      case 0:
        return 3;
      case 1:
        return 2;
      case 2:
      case 3:
        return 0;
      default:
        return std::nullopt;
    }
  }
  if (mode == 2U) {
    switch (std::bit_cast<std::int8_t>(tenant.rent_rate)) {
      case 0:
        return 3;
      case 1:
        return 2;
      case 2:
        return 1;
      case 3:
        return 0;
      default:
        return std::nullopt;
    }
  }
  if (mode == 3U &&
      (tenant.type == 3 || tenant.type == 4 || tenant.type == 5) &&
      std::bit_cast<std::int8_t>(tenant.status) >= 0x28) {
    return 3;
  }
  return std::nullopt;
}

bool original_world_map_overlay_is_full_height(std::int8_t type) noexcept {
  // Exact 11d0:04ba special-type set. Every other tenant starts twelve
  // pixels below the floor-band top and therefore covers the lower 24 rows.
  switch (type) {
    case 19:
    case 21:
    case 28:
    case 30:
    case 32:
    case 35:
      return true;
    default:
      return false;
  }
}

}  // namespace

bool original_vertical_transport_animation_active(
    const OriginalTdtDocument& document) noexcept {
  // 10c0:002e scans all 64 ten-byte records at DS:bd70. Unused records are
  // skipped; ADD sets ZF from the low 16-bit word_6+word_8 sum, so preserve
  // the otherwise-obscure wrap-to-zero case rather than widening the add.
  for (const auto& transport : document.post_elevator.stairs_bd70) {
    if (transport.used != 0U &&
        static_cast<std::uint16_t>(transport.word_6 + transport.word_8) !=
            0U) {
      return true;
    }
  }
  return false;
}

void composite_original_world_map_overlay(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalWorldPalette& palette,
    std::uint16_t mode,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster) {
  if (mode == 0U || mode > 3U || raster.width <= 0 || raster.height <= 0 ||
      raster.pixels.empty()) {
    return;
  }

  const IndexedDib strips(resources.find("BITMAP", 1003));
  if (strips.view.width != 32 || strips.height != 36) {
    throw std::runtime_error(
        "Original Map overlay BITMAP/1003 has invalid dimensions");
  }

  // 11d0:0145 visits every tenant in floor/record order. 11d0:04ba maps
  // its cell bounds into the current main-world viewport; 11e0:0efb then
  // repeats the selected eight-pixel strip without transparency.
  const std::size_t floor_count =
      std::min<std::size_t>(document.floors.size(), 120U);
  for (std::size_t floor_index = 0; floor_index < floor_count;
       ++floor_index) {
    for (const auto& tenant : document.floors[floor_index].tenants) {
      const auto tile = original_world_map_overlay_tile(tenant, mode);
      if (!tile) continue;

      const int left =
          static_cast<int>(std::bit_cast<std::int16_t>(tenant.left)) *
              kOriginalCellWidth -
          view_x;
      const int right =
          static_cast<int>(std::bit_cast<std::int16_t>(tenant.right)) *
              kOriginalCellWidth -
          view_x;
      if (right <= left) continue;

      const int floor_top =
          (119 - static_cast<int>(floor_index)) * kOriginalFloorHeight -
          view_y;
      const bool full_height =
          original_world_map_overlay_is_full_height(tenant.type);
      const int top = floor_top + (full_height ? 0 : 12);
      const int overlay_height = full_height ? 36 : 24;

      for (int local_y = 0; local_y < overlay_height; ++local_y) {
        const int destination_y = top + local_y;
        if (destination_y < 0 || destination_y >= raster.height) continue;
        for (int local_x = 0; local_x < right - left; ++local_x) {
          const int destination_x = left + local_x;
          if (destination_x < 0 || destination_x >= raster.width) continue;
          const int source_x = *tile * 8 + local_x % 8;
          const auto index = strips.sample_index(source_x, local_y);
          raster.pixels[static_cast<std::size_t>(destination_y) *
                            raster.width +
                        destination_x] = palette[index];
        }
      }
    }
  }
}

OriginalWorldRaster render_original_world(const OriginalResources& resources,
                                          const OriginalTdtDocument* document,
                                          int view_x,
                                          int view_y,
                                          int width,
                                          int height,
                                          std::span<const
                                              OriginalElevatorTransferVisual>
                                              transfer_visuals,
                                          const OriginalWorldPalette*
                                              palette_override,
                                          const OriginalSkyDecorationState*
                                              sky_decorations,
                                          std::uint16_t map_mode,
                                          const OriginalElevatorWaitingIsolationView*
                                              waiting_isolation,
                                          std::function<void()>
                                              full_frame_audio_checkpoint,
                                          bool full_frame_surface_dirty) {
  OriginalWorldRaster raster{};
  raster.width = std::max(0, width);
  raster.height = std::max(0, height);
  const std::size_t pixel_count =
      static_cast<std::size_t>(raster.width) * raster.height;
  raster.pixels.assign(pixel_count, 0x00ffffffU);
  if (raster.width == 0 || raster.height == 0) {
    return raster;
  }
  const auto derived_palette = palette_override
      ? OriginalWorldPalette{}
      : original_world_palette(resources, document);
  const auto& palette = palette_override ? *palette_override : derived_palette;
  render_original_background(resources, palette, view_x, view_y, raster);
  if (sky_decorations) {
    render_original_sky_decorations(
        resources, palette, *sky_decorations, view_x, view_y, raster);
  }
  if (document) {
    render_original_empty_floors(resources, *document, palette, view_x, view_y,
                                 raster);
    render_original_lobbies(
        resources, *document, palette, view_x, view_y, raster);
    render_original_direct_facilities(
        resources, *document, palette, view_x, view_y, raster);
    render_original_floor_boundaries(
        resources, *document, palette, view_x, view_y, raster);
    render_original_damaged_facilities(
        resources, *document, palette, view_x, view_y, raster);
    render_original_pending_facilities(
        resources, *document, palette, view_x, view_y, raster);
    // 1038:0b4a-c51 draws the translated 1028 facility-person layer over
    // each facility and below fire, Elevator and transport overlays.
    render_original_facility_people(
        resources, *document, palette, view_x, view_y, raster);
    // Direct 1090:056c checkpoint after 1038:050e.
    if (full_frame_audio_checkpoint) full_frame_audio_checkpoint();
    // 11d0:0072 orders 1038:050e's facility presentation before 11d0:0145's
    // main-world Map overlay. Later person, fire and transport presentation
    // remains above it, matching the original paint pipeline.
    composite_original_world_map_overlay(
        resources, *document, palette, map_mode, view_x, view_y, raster);
    // 1090:0571 invokes 10e8:04a0 after facilities but before Elevator and
    // Stair/Escalator layers.
    render_original_fire_bands(
        resources, *document, palette, view_x, view_y, raster);
    // 1090:0576 invokes 10f8:00c9 immediately after the fire bands. The
    // responder layer therefore remains below Elevators and transports.
    render_original_security_responders(
        resources, *document, palette, view_x, view_y, raster);
    // Direct 1090:057b checkpoint after fire bands and Security responders.
    if (full_frame_audio_checkpoint) full_frame_audio_checkpoint();
    render_original_elevator_layer(resources, *document, transfer_visuals,
                                   palette, view_x, view_y, raster);
    if (full_frame_surface_dirty && full_frame_audio_checkpoint) {
      // Direct 1090:058c after the dirty-gated Elevator/transfer layer.
      full_frame_audio_checkpoint();
    }
    render_original_elevator_waiting_people(
        resources, *document, palette, view_x, view_y, raster,
        waiting_isolation);
    if (full_frame_surface_dirty && full_frame_audio_checkpoint) {
      // Direct 1090:0596 after the dirty-gated waiting-person layer.
      full_frame_audio_checkpoint();
    }
    render_original_vertical_transports(
        resources, *document, palette, view_x, view_y, raster);
    if (full_frame_surface_dirty && full_frame_audio_checkpoint) {
      // Direct 1090:05a0 after the dirty-gated Stair/Escalator layer.
      full_frame_audio_checkpoint();
    }
    // 1090:03ab/0b10 composites live moving cars only after 10c0:007a's
    // Stair/Escalator layer, allowing the cars to overwrite intersections.
    render_original_elevator_cars(
        resources, *document, palette, view_x, view_y, raster,
        full_frame_surface_dirty ? full_frame_audio_checkpoint
                                 : std::function<void()>{});
    // 1090:05d6 invokes 10e8:0693 after those layers for the hired crew.
    render_original_fire_crew(
        resources, *document, palette, view_x, view_y, raster);
    render_original_annual_effect(
        resources, *document, palette, view_x, view_y, raster);
    // Direct 1090:05e5 after fire-crew and annual-effect presentation.
    if (full_frame_audio_checkpoint) full_frame_audio_checkpoint();
    // 11f8:0eb3 invokes 11c0:0000 after the facility, transport, fire-crew,
    // and effect layers, so outer floor caps and the roof marker are last.
    render_original_floor_edges(
        resources, *document, palette, view_x, view_y, raster);
  }
  return raster;
}

void composite_original_find_marker(
    const OriginalResources& resources,
    const OriginalWorldPalette& palette,
    int cell_x,
    int floor,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster) {
  if (cell_x < 0 || floor < 0 || floor >= 120 ||
      raster.width <= 0 || raster.height <= 0 || raster.pixels.empty()) {
    return;
  }
  const IndexedDib marker(resources.find("BITMAP", 21256));
  if (marker.view.width != 16 || marker.height != 16) {
    throw std::runtime_error(
        "Original Find BITMAP/21256 has invalid dimensions");
  }

  // 10e0:055b creates (0,0)-(16,16), offsets it by (-8,0), then sends it
  // through 1248:0000 and its 1250:0024 masked byte blitter with palette index
  // zero as the transparent marker. Its resolved cell is relative to DS:777a and floor
  // band relative to DS:777c; convert the latter's bottom-up floor coordinate
  // to the native top-down logical world used by render_original_world().
  const int destination_x = cell_x * kOriginalCellWidth - view_x - 8;
  const int destination_y =
      (119 - floor) * kOriginalFloorHeight - view_y;
  for (int y = 0; y < marker.height; ++y) {
    const int raster_y = destination_y + y;
    if (raster_y < 0 || raster_y >= raster.height) continue;
    for (int x = 0; x < marker.view.width; ++x) {
      const int raster_x = destination_x + x;
      if (raster_x < 0 || raster_x >= raster.width) continue;
      const auto index = marker.sample_index(x, y);
      if (index == 0U) continue;
      raster.pixels[static_cast<std::size_t>(raster_y) * raster.width +
                    raster_x] = palette[index];
    }
  }
}

namespace {

struct OriginalConstructionPreviewFootprint {
  int width{};
  int height{};
};

OriginalConstructionPreviewFootprint original_construction_preview_footprint(
    std::uint16_t type) noexcept {
  // 11f8:0000 initializes DS:74bc's RECT for raw types 0..48. Most widths
  // are the corresponding resource width divided by eight; the switch has
  // literal widths for several families and copies the preceding width for
  // composite lower halves. Types 25/26 deliberately retain their zero BSS
  // width. Type 18 is the one preview-only exception: 11f8:0178 writes the
  // scratch surface's full 248-pixel width instead of its 24-cell body width.
  static constexpr std::array<int, 49> kWidths = {
      1,  4,  80, 4,  6,  10, 24, 9,  2,  16, 12, 4,  16,
      26, 16, 15, 44, 2,  24, 24, 25, 25, 8,  8,  4,  0,
      0,  8,  8,  24, 24, 30, 30, 30, 7,  7,  28, 28, 28,
      28, 28, 41, 6,  4,  16, 4,  12, 31, 8,
  };
  if (type >= kWidths.size()) return {};

  int width = kWidths[type] * kOriginalCellWidth;
  int height = kOriginalFloorHeight;
  switch (type) {
    case 18:
      width = 248;
      [[fallthrough]];
    case 20:
    case 22:
    case 27:
    case 29:
    case 48:
      height = 72;
      break;
    case 31:
      height = 108;
      break;
    case 36:
      height = 180;
      break;
    default:
      break;
  }
  return {width, height};
}

}  // namespace

std::optional<OriginalConstructionPreviewRect>
original_construction_preview_rect(std::uint16_t type,
                                   int client_x,
                                   int client_y,
                                   int view_x,
                                   int view_y) noexcept {
  const auto footprint = original_construction_preview_footprint(type);
  if (footprint.width <= 0 || footprint.height <= 0) return std::nullopt;

  // 11f8:3d2d uses shared 1208:0051 to add b3f0/b3f2 to its POINT. Sibling
  // 1208:0083 (POINT subtraction) has no inbound call or relocation and thus
  // contributes no game behavior. 3da4 then subtracts half RECT.right and
  // removes the signed IDIV remainder. C++ division/remainder have the same
  // truncation-toward-zero behavior, including negative world points.
  int world_x = client_x + view_x - footprint.width / 2;
  world_x -= world_x % kOriginalCellWidth;
  int world_y = client_y + view_y;
  world_y -= world_y % kOriginalFloorHeight;
  // Literal 11f8:3df4-3e2e: Lobby type 24 is the sole no-offset case.
  // Every other type receives +12 after the signed 36-pixel snap, including
  // ordinary one-story facilities, Floor, and elevator/transport tools.
  if (type != 24U) {
    world_y += 12;
  }
  const int left = world_x - view_x;
  const int top = world_y - view_y;
  return OriginalConstructionPreviewRect{
      left, top, left + footprint.width, top + footprint.height};
}

void composite_original_construction_preview(
    std::uint16_t type,
    int client_x,
    int client_y,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster) {
  if (raster.width <= 0 || raster.height <= 0 || raster.pixels.empty()) {
    return;
  }
  const auto preview = original_construction_preview_rect(
      type, client_x, client_y, view_x, view_y);
  if (!preview) return;

  // 11f8:3cab intersects the complete shape with DS:71ca through the shared
  // 1208:0105 IntersectRect wrapper before GDI Rectangle. Consequently a
  // clipped edge receives a visible white border,
  // rather than merely clipping the original unmodified outline.
  const int left = std::max(0, preview->left);
  const int top = std::max(0, preview->top);
  const int right = std::min(raster.width, preview->right);
  const int bottom = std::min(raster.height, preview->bottom);
  if (right <= left || bottom <= top) return;

  constexpr std::uint32_t kWhitePen = 0x00ffffffU;
  const auto put = [&](int x, int y) {
    raster.pixels[static_cast<std::size_t>(y) * raster.width + x] =
        kWhitePen;
  };
  for (int x = left; x < right; ++x) {
    put(x, top);
    put(x, bottom - 1);
  }
  for (int y = top; y < bottom; ++y) {
    put(left, y);
    put(right - 1, y);
  }
}

void draw_original_world_raster(HDC destination,
                                const OriginalWorldRaster& raster,
                                int x,
                                int y) {
  // 1208:09cf copied every logical palette entry into the WinG DIB color
  // table in B,G,R,0 order before presentation. Composition has already
  // resolved those same entries into 0x00RRGGBB values; little-endian memory
  // supplies the identical B,G,R,0 byte order to SetDIBitsToDevice here.
  if (!destination || raster.width <= 0 || raster.height <= 0 ||
      raster.pixels.empty()) {
    return;
  }
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = raster.width;
  info.bmiHeader.biHeight = -raster.height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  const int result = SetDIBitsToDevice(
      destination, x, y, static_cast<DWORD>(raster.width),
      static_cast<DWORD>(raster.height), 0, 0, 0,
      static_cast<UINT>(raster.height), raster.pixels.data(), &info,
      DIB_RGB_COLORS);
  if (result == 0) {
    throw std::runtime_error("SetDIBitsToDevice failed for original world");
  }
}

}  // namespace simtower
