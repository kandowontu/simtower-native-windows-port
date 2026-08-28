#include "original_dib.hpp"
#include "original_map.hpp"
#include "original_resources.hpp"
#include "original_tdt.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <tuple>
#include <vector>

namespace {

std::vector<std::byte> read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  assert(stream);
  const std::vector<char> chars((std::istreambuf_iterator<char>(stream)),
                                std::istreambuf_iterator<char>());
  std::vector<std::byte> bytes(chars.size());
  for (std::size_t index = 0; index < chars.size(); ++index) {
    bytes[index] = static_cast<std::byte>(
        static_cast<unsigned char>(chars[index]));
  }
  return bytes;
}

std::uint8_t dib_index(std::span<const std::byte> resource, int x, int y) {
  const auto dib = simtower::original_dib_view(resource);
  assert(dib.bit_count == 8U);
  const int height = std::abs(dib.height);
  const std::size_t row_bytes =
      (static_cast<std::size_t>(dib.width) + 3U) & ~3U;
  const int source_y = dib.height > 0 ? height - 1 - y : y;
  return std::to_integer<std::uint8_t>(
      dib.pixels[static_cast<std::size_t>(source_y) * row_bytes + x]);
}

std::uint32_t clut_pixel(std::span<const std::byte> clut,
                         std::uint8_t index) {
  if (index == 255U) return 0U;
  const std::size_t source =
      static_cast<std::size_t>(index) + (index >= 184U ? 1U : 0U);
  const std::size_t offset = source * 8U;
  return (static_cast<std::uint32_t>(
              std::to_integer<std::uint8_t>(clut[offset + 2U]))
          << 16U) |
         (static_cast<std::uint32_t>(
              std::to_integer<std::uint8_t>(clut[offset + 4U]))
          << 8U) |
         static_cast<std::uint32_t>(
             std::to_integer<std::uint8_t>(clut[offset + 6U]));
}

}  // namespace

int main(int argc, char** argv) {
  // Direct MAPWNDPROC 1168:0000 coverage: all thirteen parallel-table keys,
  // including WM_COMMAND's explicit DefWindowProc target, and no invented
  // WM_CLOSE/WM_CAPTURECHANGED entries.
  for (const std::uint16_t message :
       {0x0001U, 0x0002U, 0x0006U, 0x000fU, 0x001cU, 0x0084U,
        0x0086U, 0x0111U, 0x0200U, 0x0201U, 0x0202U, 0x030fU,
        0x0311U}) {
    assert(simtower::original_map_window_handles_message(message));
  }
  for (const std::uint16_t message :
       {0x0000U, 0x0010U, 0x0014U, 0x0113U, 0x0204U, 0x0215U,
        0xffffU}) {
    assert(!simtower::original_map_window_handles_message(message));
  }

  // Direct 1058:085c coverage inside 1058:01d6: only modes 0..2 are live at
  // rating one. The fourth painted toolbar cell is inert until rating two,
  // while every 50x18 rectangle remains half-open.
  assert(simtower::original_map_toolbar_mode_at(0, 0, 1U) == 0U);
  assert(simtower::original_map_toolbar_mode_at(149, 17, 1U) == 2U);
  assert(!simtower::original_map_toolbar_mode_at(150, 0, 1U));
  assert(simtower::original_map_toolbar_mode_at(150, 0, 2U) == 3U);
  assert(simtower::original_map_toolbar_mode_at(199, 17, 6U) == 3U);
  assert(!simtower::original_map_toolbar_mode_at(200, 0, 6U));
  assert(!simtower::original_map_toolbar_mode_at(0, 18, 6U));

  // MAPWNDPROC 1168:013a consumes every move but enters 1058:0284 only for a
  // held captured drag. 0156 alone clears the state; Win32's host-only
  // WM_CAPTURECHANGED must pass through without normalizing it.
  using PointerMessage = simtower::OriginalMapPointerMessage;
  using PointerPlan = simtower::OriginalMapPointerMessagePlan;
  assert((simtower::original_map_pointer_message_plan(
              PointerMessage::button_down, false, true) ==
          PointerPlan{true, false, true, false, false, false}));
  assert((simtower::original_map_pointer_message_plan(
              PointerMessage::mouse_move, true, true) ==
          PointerPlan{true, true, false, false, false, false}));
  assert((simtower::original_map_pointer_message_plan(
              PointerMessage::mouse_move, true, false) ==
          PointerPlan{true, false, false, false, false, false}));
  assert((simtower::original_map_pointer_message_plan(
              PointerMessage::mouse_move, false, true) ==
          PointerPlan{true, false, false, false, false, false}));
  assert((simtower::original_map_pointer_message_plan(
              PointerMessage::button_up, true, false) ==
          PointerPlan{true, false, false, true, true, true}));
  assert((simtower::original_map_pointer_message_plan(
              PointerMessage::capture_changed, true, true) ==
          PointerPlan{}));

  assert(argc == 2);
  const auto bytes = read_file(argv[1]);
  const simtower::OriginalResources resources(bytes);
  const auto clut = resources.find("CLUT", 1000);
  const auto background = resources.find("BITMAP", 352);

  // Direct 1160:0000/11d0:0254/1168:02be backing-surface coverage:
  // initialization and 0254 create the 200x306 cyclic composition and its
  // 310/311/312 toolbar source rows; the painter presents it at client y=8.
  // Its interleaved WAVMIX checkpoints surround this pure renderer in host.
  auto tower = simtower::make_original_new_tdt();
  tower.header.frame_time = 16U * 3U;
  auto& lobby_floor = tower.floors[10];
  lobby_floor.left_edge = 10U;
  lobby_floor.right_edge = 50U;
  simtower::OriginalTdtTenant lobby_record{};
  lobby_record.left = 10U;
  lobby_record.right = 50U;
  lobby_record.type = 0x18;
  lobby_record.preserved_07_to_0f[8] = std::byte{0xff};
  lobby_record.rent_rate = 4U;
  lobby_floor.tenants.push_back(lobby_record);
  auto raster = simtower::render_original_map(resources, &tower, 0U);
  assert(raster.width == 200 && raster.height == 306);

  // 1080:093a selects BITMAP/311 for the active button and BITMAP/310 for
  // the other three, preserving their original palette indices exactly.
  assert(raster.at(0, 0) ==
         clut_pixel(clut, dib_index(resources.find("BITMAP", 311), 0, 0)));
  assert(raster.at(50, 0) ==
         clut_pixel(clut, dib_index(resources.find("BITMAP", 310), 50, 0)));

  // At b3de=48 the upper 264 background rows are cyclically shifted by 30;
  // the bottom 24 rows are copied without a horizontal shift.
  assert(raster.at(0, 18) ==
         clut_pixel(clut, dib_index(background, 30, 0)));
  assert(raster.at(199, 18) ==
         clut_pixel(clut, dib_index(background, 29, 0)));
  assert(raster.at(0, 282) ==
         clut_pixel(clut, dib_index(background, 0, 264)));

  // 1090:046f-047c invokes 1080:09c3 only on exact sixteen-tick boundaries,
  // including the signed-clock wrap region used by the background shifter.
  assert(simtower::original_map_animation_refresh_due(0U));
  assert(!simtower::original_map_animation_refresh_due(15U));
  assert(simtower::original_map_animation_refresh_due(16U));
  assert(!simtower::original_map_animation_refresh_due(17U));
  assert(simtower::original_map_animation_refresh_due(0xfff0U));
  assert(!simtower::original_map_animation_refresh_due(0xffffU));

  // The initial lobby's occupied-floor band is the exact 0xCCCCCC brush.
  const int lobby_top = 18 + (109 * 36) / 15;
  const int lobby_left =
      (static_cast<int>(tower.floors[10].left_edge) * 8) / 15;
  assert(raster.at(lobby_left, lobby_top) == 0x00ccccccU);

  // 1160:0420 maps Elevator types 0/1/2 to blue/black/red and LineTo omits
  // its final endpoint.
  auto& elevator = tower.elevators[0];
  elevator.used = 1U;
  elevator.type = 0U;
  elevator.x = 30U;
  elevator.top_floor = 10;
  elevator.bottom_floor = 9;
  raster = simtower::render_original_map(resources, &tower, 0U);
  const int elevator_x = (30 * 8) / 15;
  const int elevator_top = 18 + ((120 - 10 - 2) * 36) / 15;
  const int elevator_bottom = 18 + ((120 - 9) * 36) / 15;
  assert(raster.at(elevator_x, elevator_top) == 0x000000ffU);
  assert(raster.at(elevator_x, elevator_bottom - 1) == 0x000000ffU);
  assert(raster.at(elevator_x, elevator_bottom) != 0x000000ffU);

  // 11d0:0363 mode one maps exact tenant state 0 to palette red E6,0,0.
  auto& lobby = lobby_floor.tenants[0];
  lobby.preserved_07_to_0f[8] = std::byte{0};
  raster = simtower::render_original_map(resources, &tower, 1U);
  assert(raster.at(lobby_left, lobby_top) == 0x00e60000U);
  const auto legend = resources.find("BITMAP", 313);
  assert(raster.at(0, 18) ==
         clut_pixel(clut, dib_index(legend, 0, 0)));

  // Direct 1160:01dc coverage: 059f-060a uses BITMAP/(312+mode)'s DIB width
  // to right-align the legend at x=(200-width), fixes its top at y=18, and
  // leaves the cyclic BITMAP/352 background immediately outside that RECT.
  // The resource shapes are part of the original geometry, not host scaling.
  for (const auto [mode, bitmap_id, expected_width] :
       {std::tuple{1U, 313, 200}, std::tuple{2U, 314, 200},
        std::tuple{3U, 315, 81}}) {
    const auto legend_resource = resources.find("BITMAP", bitmap_id);
    const auto legend_view = simtower::original_dib_view(legend_resource);
    assert(legend_view.width == expected_width);
    assert(std::abs(legend_view.height) == 20);
    const int destination_x = 200 - expected_width;
    const auto legend_raster =
        simtower::render_original_map(resources, nullptr, mode);
    assert(legend_raster.at(destination_x, 18) ==
           clut_pixel(clut, dib_index(legend_resource, 0, 0)));
    assert(legend_raster.at(199, 37) ==
           clut_pixel(clut,
                      dib_index(legend_resource, expected_width - 1, 19)));
    assert(legend_raster.at(destination_x, 38) ==
           clut_pixel(clut, dib_index(background, destination_x, 20)));
    if (destination_x != 0) {
      assert(legend_raster.at(destination_x - 1, 18) ==
             clut_pixel(clut,
                        dib_index(background, destination_x - 1, 0)));
    }
  }

  // Disabled toolbar state comes entirely from BITMAP/312.
  raster = simtower::render_original_map(resources, &tower, 2U, true);
  assert(raster.at(150, 0) ==
         clut_pixel(clut, dib_index(resources.find("BITMAP", 312), 150, 0)));
  assert(raster.at(100, 0) ==
         clut_pixel(clut, dib_index(resources.find("BITMAP", 311), 100, 0)));

  // 1080:0209 fits the 3000x4320 world into the original 200x288 Map
  // content rectangle exactly. Wider/taller sources constrain only their
  // dominant axis; smaller sources are not enlarged and retain signed-trunc
  // centering inside an offset container.
  assert((simtower::original_aspect_fit_rect(
              {0, 18, 200, 306}, {0, 0, 3000, 4320}) ==
          simtower::OriginalMapRect{0, 18, 200, 306}));
  assert((simtower::original_aspect_fit_rect(
              {0, 0, 100, 100}, {0, 0, 200, 100}) ==
          simtower::OriginalMapRect{0, 25, 100, 75}));
  assert((simtower::original_aspect_fit_rect(
              {0, 0, 100, 100}, {0, 0, 100, 200}) ==
          simtower::OriginalMapRect{25, 0, 75, 100}));
  assert((simtower::original_aspect_fit_rect(
              {10, 20, 210, 220}, {5, 7, 105, 57}) ==
          simtower::OriginalMapRect{60, 95, 160, 145}));

  // Direct 1080:038e/04b0 coverage: scale all four world-client coordinates
  // with signed 32-bit IDIV by 4320 after multiplying by 288, then offset the
  // vertical pair by the 18-pixel toolbar. Negative inputs prove truncation
  // toward zero rather than floor division.
  assert((simtower::original_map_view_rect(150, 300, 640, 480) ==
          simtower::OriginalMapRect{10, 38, 52, 70}));
  assert((simtower::original_map_view_rect(-1, -16, 1, 16) ==
          simtower::OriginalMapRect{0, 17, 0, 18}));
  assert((simtower::original_map_centered_view(
              100, 150, 150, 300, 640, 480) ==
          simtower::OriginalWorldPoint{1185, 1740}));
  // 1058:06df compares the newly centered focus rectangle with DS:7796 and
  // returns without 1080:0440 when they are equal. Preserve a non-map-aligned
  // raw view rather than rounding it merely because its center was clicked.
  assert((simtower::original_map_centered_view(
              31, 54, 151, 300, 640, 480) ==
          simtower::OriginalWorldPoint{151, 300}));
  // The same no-op boundary applies throughout the clamped edge band.
  assert((simtower::original_map_centered_view(
              199, 305, 2360, 3840, 640, 480) ==
          simtower::OriginalWorldPoint{2360, 3840}));
  // Direct 1058:064e/11e0:0ab2 coverage: the complete focus rectangle is
  // shifted back inside the map rather than clipping one edge independently.
  assert((simtower::original_map_centered_view(
              0, 18, 150, 300, 640, 480) ==
          simtower::OriginalWorldPoint{0, 0}));
  assert((simtower::original_map_centered_view(
              199, 305, 150, 300, 640, 480) ==
          simtower::OriginalWorldPoint{2370, 3840}));

  // 1080:0054 moves exactly the active view axis by the pointer's distance
  // beyond the client. The right and bottom comparisons are strict, so both
  // rectangle boundaries remain visible without scrolling.
  const simtower::OriginalMapRect client{10, 20, 210, 220};
  assert((simtower::original_keep_pointer_visible(
              {300, 400}, client, {10, 20}, true) ==
          simtower::OriginalWorldPoint{300, 400}));
  assert((simtower::original_keep_pointer_visible(
              {300, 400}, client, {210, 220}, false) ==
          simtower::OriginalWorldPoint{300, 400}));
  assert((simtower::original_keep_pointer_visible(
              {300, 400}, client, {-5, 999}, true) ==
          simtower::OriginalWorldPoint{285, 400}));
  assert((simtower::original_keep_pointer_visible(
              {300, 400}, client, {225, -999}, true) ==
          simtower::OriginalWorldPoint{315, 400}));
  assert((simtower::original_keep_pointer_visible(
              {300, 400}, client, {999, 5}, false) ==
          simtower::OriginalWorldPoint{300, 385}));
  assert((simtower::original_keep_pointer_visible(
              {300, 400}, client, {-999, 235}, false) ==
          simtower::OriginalWorldPoint{300, 415}));

  // 1160:050d expands the mapped annual-effect point to a 3x3 red square.
  auto& effect = tower.post_elevator.version_18_dd6c;
  effect.assign(8U, std::byte{0});
  effect[0] = std::byte{1};
  effect[4] = std::byte{150};
  effect[6] = std::byte{44};
  effect[7] = std::byte{1};  // y=300
  raster = simtower::render_original_map(resources, &tower, 0U);
  assert(raster.at(9, 37) == 0x00ff0000U);
  assert(raster.at(11, 39) == 0x00ff0000U);

  return 0;
}
