#pragma once

#include "original_resources.hpp"
#include "original_tdt.hpp"
#include "original_time.hpp"

#include <windows.h>

#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace simtower {

// SETRECT at 1128:02aa creates the original logical world as 3000x4320.
inline constexpr int kOriginalWorldWidth = 3000;
inline constexpr int kOriginalWorldHeight = 4320;
inline constexpr int kOriginalWorldBottomMargin = 324;

struct OriginalPoint16 {
  std::int16_t x{};
  std::int16_t y{};

  friend bool operator==(const OriginalPoint16&, const OriginalPoint16&) =
      default;
};

// Complete 1208:0083 POINT subtraction helper. Its supplied executable has no
// inbound call or relocation, but the native port retains its exact wrapping
// 16-bit arithmetic rather than dropping a recovered game-owned function.
[[nodiscard]] constexpr OriginalPoint16 original_point_subtract(
    OriginalPoint16 point,
    std::int16_t x,
    std::int16_t y) noexcept {
  point.x = static_cast<std::int16_t>(
      static_cast<std::uint16_t>(point.x) - static_cast<std::uint16_t>(x));
  point.y = static_cast<std::int16_t>(
      static_cast<std::uint16_t>(point.y) - static_cast<std::uint16_t>(y));
  return point;
}

// 11a0:134c precomputes sixty signed floor offsets as index * scale * 36.
inline constexpr int kOriginalFloorHeight = 36;
[[nodiscard]] constexpr std::int32_t original_precomputed_floor_offset(
    std::int32_t index,
    std::int32_t scale) noexcept {
  // Both IMUL instructions retain only EAX, so overflow wraps at 32 bits.
  return std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(index) *
      static_cast<std::uint32_t>(scale) *
      static_cast<std::uint32_t>(kOriginalFloorHeight));
}
inline constexpr int kOriginalCellWidth = 8;

struct OriginalPaletteStorageContract {
  std::uint16_t global_alloc_flags{};
  std::uint32_t allocation_bytes{};
  std::size_t entry_count{};

  friend bool operator==(const OriginalPaletteStorageContract&,
                         const OriginalPaletteStorageContract&) = default;
};

// Exact 1020:0019/008f movable Win16 palette-color block ownership. Native
// fixed storage retains all 256 four-byte entries and needs no unlock thunk.
[[nodiscard]] constexpr OriginalPaletteStorageContract
original_palette_storage_contract() noexcept {
  return {0x0040U, 0x0400U, 256U};
}

// Native aggregate equivalent of 1208:002c, the shared Win16 two-word point
// assignment wrapper used by cursor, Map, Find, and information paths.
struct OriginalWorldPoint {
  int x{};
  int y{};

  friend bool operator==(const OriginalWorldPoint&,
                         const OriginalWorldPoint&) = default;
};

struct OriginalWorldRaster {
  // Native storage replacement for 1128:13dc's WinGCreateDC/SaveDC setup and
  // 1208:07d5's 8-bit WinG DIB allocation.
  // Indexed original pixels are resolved through the active logical palette
  // while composing, producing the same displayed colors in a top-down DIB.
  int width{};
  int height{};
  // Top-down 32-bit BI_RGB pixels. Each value is 0x00RRGGBB, which is laid
  // out as B,G,R,0 in little-endian DIB memory.
  std::vector<std::uint32_t> pixels{};

  [[nodiscard]] std::uint32_t at(int x, int y) const;
};

struct OriginalWingDibLayout {
  std::int16_t width{};
  std::int16_t height{};
  std::int32_t bitmap_height{};
  std::int32_t row_stride{};
  std::uint16_t planes{};
  std::uint16_t bit_count{};
  std::uint32_t compression{};
  std::uint16_t palette_entries{};
  std::uint32_t clear_raster_operation{};

  friend bool operator==(const OriginalWingDibLayout&,
                         const OriginalWingDibLayout&) = default;
};

// Exact visible allocation contract of 1208:07d5. WinG receives an 8-bit,
// top-down DIB; its scanline is ((width*8+31)&~31)/8 bytes, the complete
// 256-entry logical palette follows the header, and PATBLT/BLACKNESS (0x42)
// clears the selected bitmap. Native 32-bit rasters have naturally aligned
// rows, but retain these source dimensions and orientation.
[[nodiscard]] constexpr OriginalWingDibLayout original_wing_dib_layout(
    std::int16_t width,
    std::int16_t height) noexcept {
  const auto signed_word = [](std::uint16_t value) constexpr {
    return value < 0x8000U
               ? static_cast<std::int32_t>(value)
               : static_cast<std::int32_t>(value) - 0x10000;
  };
  const std::uint16_t negative_height = static_cast<std::uint16_t>(
      0U - static_cast<std::uint16_t>(height));
  std::uint16_t row_bits = static_cast<std::uint16_t>(
      static_cast<std::uint16_t>(width) * 8U);
  row_bits = static_cast<std::uint16_t>(row_bits + 31U);
  row_bits = static_cast<std::uint16_t>(row_bits & 0xffe0U);
  return {
      width,
      height,
      signed_word(negative_height),
      signed_word(row_bits) / 8,
      1U,
      8U,
      0U,
      256U,
      0x42U,
  };
}

struct OriginalConstructionPreviewRect {
  int left{};
  int top{};
  int right{};
  int bottom{};

  [[nodiscard]] constexpr bool valid() const noexcept {
    return right > left && bottom > top;
  }

  friend bool operator==(const OriginalConstructionPreviewRect&,
                         const OriginalConstructionPreviewRect&) = default;
};

using OriginalWorldPalette = std::array<
    std::uint32_t, original_palette_storage_contract().entry_count>;

struct OriginalLogicalPaletteEntry {
  std::uint8_t red{};
  std::uint8_t green{};
  std::uint8_t blue{};
  std::uint8_t flags{};

  friend bool operator==(const OriginalLogicalPaletteEntry&,
                         const OriginalLogicalPaletteEntry&) = default;
};

using OriginalLogicalPaletteEntries =
    std::array<OriginalLogicalPaletteEntry, 256>;

// Process-local BITMAP/900..903 placements maintained by 1048:05f0/06a5/
// 0717. Each of the four slots may independently select any source bitmap;
// the rectangles use logical-world pixel coordinates and are not serialized.
struct OriginalSkyDecorationPlacement {
  std::int16_t bitmap_index{-1};
  int left{};
  int top{};
  int right{};
  int bottom{};

  [[nodiscard]] constexpr bool valid() const noexcept {
    return bitmap_index >= 0 && bitmap_index < 4 &&
           right > left && bottom > top;
  }

  friend bool operator==(const OriginalSkyDecorationPlacement&,
                         const OriginalSkyDecorationPlacement&) = default;
};

struct OriginalSkyDecorationState {
  std::array<OriginalSkyDecorationPlacement, 4> placements{};
};

struct OriginalSkyDecorationStepResult {
  std::size_t repositioned{};
  std::size_t visible{};
  bool changed{};
};

// Exact paint-time placement pass at 1048:083f/05f0/06a5/0717. The eligible
// sky band is world rectangle (0,360)-(3000,3888); a slot is randomized only
// when its complete prior rectangle is no longer inside the visible band.
[[nodiscard]] OriginalSkyDecorationStepResult
step_original_sky_decorations(
    const OriginalResources& resources,
    OriginalTdtDocument& document,
    OriginalSkyDecorationState& state,
    int view_x,
    int view_y,
    int client_width,
    int client_height);

// Exact time/event-dependent six-color palette update at 1020:098b, applied
// over the CLUT/1000 palette built by 1020:0019/0e29. Native raster
// composition consumes this table directly while the auxiliary GDI surfaces
// retain 1020:0f4f's logical-palette boundary. Entries 188..193 are
// interpolated from CLUT/1000..1003 and mirrored to 207..212 and 213..218.
[[nodiscard]] OriginalWorldPalette original_world_palette(
    const OriginalResources& resources,
    const OriginalTdtDocument* document);

// Exact PALETTEENTRY array written by 1020:0e29 before 1020:0f4f wraps it in
// a LOGPALETTE(version=0x0300,count=256). Entries 0..187 use PC_NOCOLLAPSE,
// 188..218 use PC_RESERVED, 219..254 use no flags, and entry 255 is forced to
// four zero bytes independently of the supplied RGB table.
[[nodiscard]] OriginalLogicalPaletteEntries original_logical_palette_entries(
    const OriginalWorldPalette& palette) noexcept;

// Exact 1020:0f4f allocation boundary after 1020:0e29. The temporary Win16
// movable block is represented by fixed native storage, but LOGPALETTE
// version 0x0300, count 0x0100, entry bytes, and CreatePalette call are
// preserved. The returned object is owned by the caller and must be deleted.
[[nodiscard]] HPALETTE create_original_logical_palette(
    const OriginalWorldPalette& palette) noexcept;

// DS:7772/77c2 and the persistent logical palette mutated by 1020:00cb.
// Value ownership also replaces 1020:008f's GlobalUnlock/GlobalFree cleanup;
// there is no palette allocation handle to release in the native port.
// This state is process-local in the original and is deliberately excluded
// from TDT serialization. The time palette pass (1020:098b) and effects pass
// are separate because disabling Effects freezes entries 194..203 while the
// time-of-day colors continue to advance.
struct OriginalPaletteRuntime {
  OriginalWorldPalette colors{};
  std::uint32_t last_effect_tick{};
  std::uint16_t effect_counter{};
  bool initialized{};
};

void reset_original_palette_runtime(
    const OriginalResources& resources,
    const OriginalTdtDocument* document,
    OriginalPaletteRuntime& state,
    std::uint32_t now_tick);

// Applies one 1020:098b pass without disturbing the effect colors at
// entries 194..203. Returns true when any RGB entry changed.
bool refresh_original_time_palette(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    OriginalPaletteRuntime& state);

// Exact 1020:00cb 15-coarse-tick gate and phase tables. 1208:05e6 supplies
// 16-ms units, so the nominal effect cadence is 240 ms. The sampled tick is
// also the value retained as DS:7772 after a successful update.
bool step_original_effect_palette(
    const OriginalTdtDocument& document,
    OriginalPaletteRuntime& state,
    bool effects_enabled,
    std::uint32_t now_tick) noexcept;

// Process-local one-frame entry/exit cache written by 10a8:022b and consumed
// by 10a8:02aa -> 0de6. The original retains one person for each transfer
// side of every visible floor/Elevator pair and clears each slot after paint;
// it is intentionally not part of the persisted TDT document.
struct OriginalElevatorTransferVisual {
  std::size_t elevator_index{};
  std::int16_t floor{};
  bool boarding{};
  bool direction_up{};
  std::uint32_t person_index{};
};

// Process-only DS:b3ae presentation input for 10a8:12c1. During Elevator
// Simulate, person identities come from the saved 0x345a shaft snapshot while
// the live waiting-ring dwords hold projected signed wait metrics.
struct OriginalElevatorWaitingIsolationView {
  std::size_t elevator_index{};
  const OriginalTdtElevator* saved_elevator{};
};

// Exact rectangle pair produced by 1090:216e/221f/227b and copied by
// 1090:0b10. Source coordinates address the type-1 BITMAP/1064..1069 staging
// band; destination coordinates are relative to the current world client.
struct OriginalElevatorCarVisual {
  int source_x{};
  int source_y{};
  int width{};
  int height{};
  int destination_x{};
  int destination_y{};

  friend bool operator==(const OriginalElevatorCarVisual&,
                         const OriginalElevatorCarVisual&) = default;
};

[[nodiscard]] std::optional<OriginalElevatorCarVisual>
original_elevator_car_visual(const OriginalTdtElevator& elevator,
                             std::size_t car_index,
                             int view_x,
                             int view_y,
                             int client_height) noexcept;

// Exact rating-to-CGPK tier selected by 11f8:06cd for all three Lobby bands:
// ratings below three use tier zero, rating three uses one, and four or above
// use two.
[[nodiscard]] int original_lobby_graphics_variant(
    std::uint16_t rating) noexcept;

// Exact presentation-only person mutation performed by 1038:0da5 through
// 1028:0570/0841/0902/0feb/12c5/1534/1692/17f0 while visible facilities are
// painted. The original advances its shared Microsoft C rand() stream here,
// so this remains a distinct paint-time pass rather than part of the normal
// simulation tick. Person bytes 7 and 8 are the left cell and graphic frame.
struct OriginalFacilityPeopleStepResult {
  std::size_t visible_tenants{};
  std::size_t dispatched_tenants{};
  std::size_t changed_people{};
  bool cathedral_counter_changed{};
};

[[nodiscard]] OriginalFacilityPeopleStepResult
step_original_visible_facility_people(
    OriginalTdtDocument& document,
    int view_x,
    int view_y,
    int width,
    int height,
    bool people_animation_enabled,
    bool control_modifier);

// 1128:08d6 centers the initial view horizontally and places its bottom
// kOriginalWorldBottomMargin pixels above the logical world's bottom.
[[nodiscard]] OriginalWorldPoint original_initial_view(int client_width,
                                                        int client_height);

// Exact text written over the main window's vertical-scrollbar up-arrow by
// 1080:0b26. The label is the floor at the center of the visible floor bands;
// basements use B1, B2, ... and above-ground floors are unadorned decimals.
[[nodiscard]] std::string original_scroll_floor_label(int view_y,
                                                       int client_height);

// Exact 1080:0000 facility-focus transform. 1080:01cb first converts the
// client rectangle to the visible cell/floor counts held at DS:777e/7780;
// the focus routine then centers the requested grid coordinate and floor in
// those logical units before writing the pixel scroll position.
[[nodiscard]] OriginalWorldPoint original_facility_focus_view(
    int facility_x,
    int facility_floor,
    int client_width,
    int client_height) noexcept;

// Exact main-world Map overlay at 11d0:0145. 11d0:0363 selects one of the
// four eight-pixel strips in BITMAP/1003, 11d0:04ba supplies each tenant's
// destination rectangle, and 11e0:0efb tiles the strip horizontally.
void composite_original_world_map_overlay(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalWorldPalette& palette,
    std::uint16_t mode,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster);

// Exact 10c0:002e Stair/Escalator animation-dirty scan. A used transport
// keeps the shared 1090:03ab repaint flag raised while the wrapping 16-bit
// sum of its two direction counters is nonzero.
[[nodiscard]] bool original_vertical_transport_animation_active(
    const OriginalTdtDocument& document) noexcept;

// Static translation of the recovered 1048:03a3 background pass and the
// type-0x18 branch of 1038:050e. It consumes the original BITMAP, CLUT, and
// CGPK bytes directly; no redrawn or inferred artwork is involved.
[[nodiscard]] OriginalWorldRaster render_original_world(
    const OriginalResources& resources,
    const OriginalTdtDocument* document,
    int view_x,
    int view_y,
    int width,
    int height,
    std::span<const OriginalElevatorTransferVisual> transfer_visuals = {},
    const OriginalWorldPalette* palette_override = nullptr,
    const OriginalSkyDecorationState* sky_decorations = nullptr,
    std::uint16_t map_mode = 0U,
    const OriginalElevatorWaitingIsolationView* waiting_isolation = nullptr,
    std::function<void()> full_frame_audio_checkpoint = {},
    bool full_frame_surface_dirty = false);

// Exact transient Find target at 10e0:055b: BITMAP/21256 is centered on the
// resolved cell, anchored to the resolved floor band, clipped to the current
// view, and copied with palette index zero transparent.
void composite_original_find_marker(
    const OriginalResources& resources,
    const OriginalWorldPalette& palette,
    int cell_x,
    int floor,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster);

// Exact construction-pointer rectangle at 11f8:0000/3da4. The selected
// footprint is centered horizontally on the client pointer, snapped with
// signed IDIV remainder semantics to the 8x36 world grid, and multi-floor
// shapes begin twelve pixels below their snapped floor boundary.
[[nodiscard]] std::optional<OriginalConstructionPreviewRect>
original_construction_preview_rect(std::uint16_t type,
                                   int client_x,
                                   int client_y,
                                   int view_x,
                                   int view_y) noexcept;

// Exact visible result of 11f8:3c13. Its scratch WinG save/restore machinery
// is unnecessary with the native on-demand raster, but the pre-draw client
// intersection and WHITE_PEN/NULL_BRUSH Rectangle pixels are preserved.
void composite_original_construction_preview(
    std::uint16_t type,
    int client_x,
    int client_y,
    int view_x,
    int view_y,
    OriginalWorldRaster& raster);

void draw_original_world_raster(HDC destination,
                                const OriginalWorldRaster& raster,
                                int x = 0,
                                int y = 0);

}  // namespace simtower
