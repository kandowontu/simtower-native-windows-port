#include "original_command_palette.hpp"
#include "original_dib.hpp"
#include "original_palette_frame.hpp"
#include "original_resources.hpp"
#include "original_tables.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
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

std::uint32_t dib_pixel(std::span<const std::byte> resource, int x, int y) {
  const auto dib = simtower::original_dib_view(resource);
  assert(dib.bit_count == 8U);
  const int height = std::abs(dib.height);
  assert(x >= 0 && x < dib.width && y >= 0 && y < height);
  const std::size_t row_bytes =
      (static_cast<std::size_t>(dib.width) + 3U) & ~3U;
  const int source_y = dib.height > 0 ? height - 1 - y : y;
  const auto index = std::to_integer<std::uint8_t>(
      dib.pixels[static_cast<std::size_t>(source_y) * row_bytes + x]);
  const RGBQUAD color = dib.info->bmiColors[index];
  return (static_cast<std::uint32_t>(color.rgbRed) << 16U) |
         (static_cast<std::uint32_t>(color.rgbGreen) << 8U) |
         static_cast<std::uint32_t>(color.rgbBlue);
}

void assert_region(const simtower::OriginalCommandRaster& raster,
                   int destination_x,
                   int destination_y,
                   std::span<const std::byte> source,
                   int source_x,
                   int source_y,
                   int width,
                   int height) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      assert(raster.at(destination_x + x, destination_y + y) ==
             dib_pixel(source, source_x + x, source_y + y));
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  // Direct 1050:0503 value-ownership coverage: the original frees its staging
  // surface and BITMAP/300..302 surfaces; native retains the three exact IDs.
  assert(simtower::kOriginalCommandSurfaceResourceIds ==
         (std::array<std::uint16_t, 3>{300U, 301U, 302U}));
  assert(simtower::kOriginalCommandSurfaceObjectCount == 4U);
  using simtower::OriginalPaletteFrameHit;
  // Direct 1140:00a8/010d host-plan coverage: New/Open supplies zero and both
  // rating-promotion callers supply one. Only a nonzero argument forces
  // DS:783c to edit mode two; Command presentation and 1038:0000's tile-
  // scratch rebuild are unconditional.
  assert((simtower::original_rating_command_refresh_plan(
              0U, false, false) ==
          simtower::OriginalRatingCommandRefreshPlan{
              false, true, false, true}));
  assert((simtower::original_rating_command_refresh_plan(
              1U, false, true) ==
          simtower::OriginalRatingCommandRefreshPlan{
              true, true, true, true}));
  assert((simtower::original_rating_command_refresh_plan(
              0xffffU, false, true) ==
          simtower::OriginalRatingCommandRefreshPlan{
              true, true, true, true}));
  // 11f8:3b94 leaves the pending outline untouched while DS:b3ae isolates an
  // Elevator Control simulation, but 1038:0000 still runs afterward.
  assert((simtower::original_rating_command_refresh_plan(
              1U, true, true) ==
          simtower::OriginalRatingCommandRefreshPlan{
              true, true, false, true}));
  // Direct CMDBTNWNDPROC 1050:0000 coverage: its parallel table has exactly
  // eleven messages. In particular it has no WM_CLOSE, WM_ERASEBKGND, mouse
  // move, right-button, or timer entry; those all reach DefWindowProc.
  for (const std::uint16_t message :
       {0x0001U, 0x0002U, 0x0006U, 0x000fU, 0x001cU, 0x0084U,
        0x0086U, 0x0201U, 0x0202U, 0x030fU, 0x0311U}) {
    assert(simtower::original_command_window_handles_message(message));
  }
  for (const std::uint16_t message :
       {0x0000U, 0x0010U, 0x0014U, 0x0113U, 0x0200U, 0x0204U,
        0x0205U, 0xffffU}) {
    assert(!simtower::original_command_window_handles_message(message));
  }
  for (const std::uint16_t message :
       {0x000fU, 0x0110U, 0x0200U, 0x0201U,
        0x0202U, 0x0204U, 0x0311U}) {
    assert(simtower::original_command_selector_handles_message(message));
  }
  for (const std::uint16_t message :
       {0x0000U, 0x0010U, 0x0014U, 0x0205U, 0xffffU}) {
    assert(!simtower::original_command_selector_handles_message(message));
  }
  assert((simtower::original_palette_frame_close_rect() ==
          simtower::OriginalPaletteFrameRect{4, 1, 10, 7}));
  assert(simtower::original_palette_frame_hit_test(4, 1) ==
         OriginalPaletteFrameHit::close);
  assert(simtower::original_palette_frame_hit_test(9, 6) ==
         OriginalPaletteFrameHit::close);
  assert(simtower::original_palette_frame_hit_test(10, 6) ==
         OriginalPaletteFrameHit::drag);
  assert(simtower::original_palette_frame_hit_test(5, 7) ==
         OriginalPaletteFrameHit::drag);
  assert(simtower::original_palette_frame_hit_test(5, 8) ==
         OriginalPaletteFrameHit::client);

  // 1078:00c6 is called synchronously by CMDBTNWNDPROC 1050:00f8,
  // INFOWNDPROC 1120:00a5, and MAPWNDPROC 1168:00c7. Verify its exact
  // eight-pixel active/inactive raster in a memory DC—no window is created.
  {
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = simtower::kOriginalCommandSurfaceWidth;
    info.bmiHeader.biHeight = -simtower::kOriginalPaletteFrameHeight;
    info.bmiHeader.biPlanes = 1U;
    info.bmiHeader.biBitCount = 32U;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(
        nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0U);
    HDC dc = CreateCompatibleDC(nullptr);
    assert(bitmap != nullptr && pixels != nullptr && dc != nullptr);
    HGDIOBJ previous = SelectObject(dc, bitmap);

    simtower::draw_original_palette_frame(
        dc, simtower::kOriginalCommandSurfaceWidth, false);
    assert(GetPixel(dc, 0, 0) == RGB(255, 255, 255));
    assert(GetPixel(dc, 4, 1) == RGB(0, 0, 0));
    assert(GetPixel(dc, 5, 2) == RGB(255, 255, 255));

    simtower::draw_original_palette_frame(
        dc, simtower::kOriginalCommandSurfaceWidth, true);
    assert(GetPixel(dc, 0, 0) == GetSysColor(COLOR_ACTIVECAPTION));
    assert(GetPixel(dc, 4, 1) == RGB(255, 255, 255));
    assert(GetPixel(dc, 10, 1) == GetSysColor(COLOR_ACTIVECAPTION));

    SelectObject(dc, previous);
    DeleteDC(dc);
    DeleteObject(bitmap);
  }

  assert(argc == 2);
  const auto pack = read_file(argv[1]);
  const simtower::OriginalResources resources(pack);

  // Direct 1058:0895 -> 1208:069a/1248:0000/1250:0114 coverage: opaque copies keep
  // source and destination extents equal, clip only against the destination
  // bitmap, and advance the source origin by removed left/top pixels.
  assert((simtower::original_opaque_blit_plan(
              64, 53, 10, 20, 32, 32, 4, 5) ==
          simtower::OriginalOpaqueBlitPlan{
              10, 20, 4, 5, 32, 32, true}));
  assert((simtower::original_opaque_blit_plan(
              64, 53, 10, 20, 32, 32, -5, -7) ==
          simtower::OriginalOpaqueBlitPlan{
              15, 27, 0, 0, 27, 25, true}));
  assert((simtower::original_opaque_blit_plan(
              64, 53, 10, 20, 32, 32, 50, 40) ==
          simtower::OriginalOpaqueBlitPlan{
              10, 20, 50, 40, 14, 13, true}));
  assert((simtower::original_opaque_blit_plan(
              64, 53, 10, 20, 32, 32, 64, 0) ==
          simtower::OriginalOpaqueBlitPlan{}));

  // Direct 1058:071f/0798/0828 coverage: catalog types 3+ become alternating
  // 32-pixel columns and 32-pixel rows from y=53, then the parent adds eight
  // pixels.
  assert(simtower::original_command_toggle_rect() ==
         simtower::OriginalCommandRect({20, 12, 41, 34}));
  assert(simtower::original_command_mode_rect(2) ==
         simtower::OriginalCommandRect({42, 40, 63, 61}));
  assert(simtower::original_command_facility_rect(4) ==
         simtower::OriginalCommandRect({0, 125, 32, 157}));
  assert(simtower::original_command_facility_rect(0) ==
         simtower::OriginalCommandRect({0, 61, 32, 93}));
  assert(simtower::original_command_facility_rect(1) ==
         simtower::OriginalCommandRect({32, 61, 64, 93}));
  assert(simtower::original_command_facility_rect(15) ==
         simtower::OriginalCommandRect({32, 285, 64, 317}));

  // 1050:0219 activates ordinary command points on mouse-down. Only the
  // build toggle waits for 1050:02b3's mouse-up path; this is what lets the
  // modal grouped selector track a held click before accepting its release.
  using Phase = simtower::OriginalCommandPointerPhase;
  using Plan = simtower::OriginalCommandPointerPlan;
  assert((simtower::original_command_pointer_plan(Phase::button_down, 5, 2) ==
          Plan{.close_palette = true}));
  assert((simtower::original_command_pointer_plan(Phase::button_down, 20, 12) ==
          Plan{.press_toggle = true, .sample_coarse_tick = true}));
  assert((simtower::original_command_pointer_plan(Phase::button_down, 5, 45) ==
          Plan{.activate_point = true, .sample_coarse_tick = true}));
  assert((simtower::original_command_pointer_plan(Phase::button_up, 5, 2) ==
          Plan{.restore_toggle = true}));
  assert((simtower::original_command_pointer_plan(Phase::button_up, 20, 12) ==
          Plan{.restore_toggle = true, .activate_point = true}));
  assert((simtower::original_command_pointer_plan(Phase::button_up, 5, 45) ==
          Plan{.restore_toggle = true, .activate_point = true}));

  // Direct 1050:0533 and 1058:04e0 coverage: 0517-053f samples the physical
  // primary button after preserving the swapped-button preference. Only a
  // held primary button opens DIALOG/124; only an accepted modal mutates the
  // TABL low-byte choice. Button-up re-entry and cancellation retain it.
  using SelectorPlan = simtower::OriginalCommandSelectorTransactionPlan;
  assert(simtower::original_command_primary_button_virtual_key(false) ==
         0x01U);
  assert(simtower::original_command_primary_button_virtual_key(true) ==
         0x02U);
  assert((simtower::original_command_selector_transaction_plan(false, false) ==
          SelectorPlan{false, false}));
  assert((simtower::original_command_selector_transaction_plan(false, true) ==
          SelectorPlan{false, false}));
  assert((simtower::original_command_selector_transaction_plan(true, false) ==
          SelectorPlan{true, false}));
  assert((simtower::original_command_selector_transaction_plan(true, true) ==
          SelectorPlan{true, true}));

  // Direct CMDBTNSUBWNDPROC 1050:05a7 coverage: initialization aligns and
  // clips the captured 32-pixel rows, selects the standard arrow through
  // 11e0:0d80(0), painting chooses normal/selected icon sheets, mouse motion
  // updates the one-based highlight, and a button release commits its row.
  // 0638-0671 clamps the top and then shifts the whole selector above the
  // desktop bottom.
  assert(simtower::original_command_selector_top(125, 2U, 4U, 768) == 93);
  assert(simtower::original_command_selector_top(20, 3U, 4U, 768) == 0);
  assert(simtower::original_command_selector_top(740, 1U, 4U, 768) == 640);
  assert(simtower::original_command_selector_top(740, 4U, 4U, 768) == 640);

  constexpr std::array<std::uint16_t, 5> kRatingOne = {0, 4, 7, 24, 11};
  assert(simtower::original_command_catalog(resources, 1) ==
         std::vector<std::uint16_t>(kRatingOne.begin(), kRatingOne.end()));
  // Direct 1058:03a9 coverage: the build toggle has priority even while the
  // catalog is disabled, followed by all three edit cells and then the active
  // rating's two-column facility catalog using half-open Win16 rectangles.
  assert(simtower::original_command_hit_test(resources, 1, true, 20, 12) ==
         simtower::OriginalCommandHit(
             {simtower::OriginalCommandHitKind::build_toggle, 0, 0}));
  assert(simtower::original_command_hit_test(resources, 1, false, 20, 12) ==
         simtower::OriginalCommandHit(
             {simtower::OriginalCommandHitKind::build_toggle, 0, 0}));
  assert(simtower::original_command_hit_test(resources, 1, true, 1, 41) ==
         simtower::OriginalCommandHit(
             {simtower::OriginalCommandHitKind::edit_mode, 0, 0}));
  assert(simtower::original_command_hit_test(resources, 1, true, 22, 41) ==
         simtower::OriginalCommandHit(
             {simtower::OriginalCommandHitKind::edit_mode, 1, 0}));
  assert(simtower::original_command_hit_test(resources, 1, true, 43, 41) ==
         simtower::OriginalCommandHit(
             {simtower::OriginalCommandHitKind::edit_mode, 2, 0}));
  assert(simtower::original_command_hit_test(resources, 1, true, 1, 62) ==
         simtower::OriginalCommandHit(
             {simtower::OriginalCommandHitKind::facility, 3, 24}));
  // Direct 11f8:0f63 coverage: the selected TABL/1000 catalog index resolves
  // to its raw facility-type byte before the per-family construction switch.
  assert(simtower::original_command_build_type(resources, 16) == 44);
  assert(simtower::original_command_build_type(resources, 17) == 11);
  assert(simtower::original_command_build_type(resources, 18) == 20);
  // TABL/1000 icons 4/6/5 dispatch through 11f8:09f7/09fd/0a03 to the
  // standard, express, and service elevator variants respectively.
  assert(simtower::original_command_build_type(resources, 4) == 1);
  assert(simtower::original_command_build_type(resources, 6) == 42);
  assert(simtower::original_command_build_type(resources, 5) == 43);

  // Direct 1140:02ee/0341/04c5 coverage: resolve a rating's command table,
  // scan the tab group, and decode its selected TABM/catalog lane.
  auto rating_three = simtower::original_command_rating_state(resources, 3);
  const auto parking_group =
      simtower::original_command_group(resources, rating_three, 6);
  assert(parking_group);
  assert(parking_group->tabm_number == 9);
  assert(parking_group->selection_index == 1);
  constexpr std::array<std::uint16_t, 3> kParkingIcons = {16, 17, 18};
  assert(parking_group->catalog_icons ==
         std::vector<std::uint16_t>(kParkingIcons.begin(), kParkingIcons.end()));
  assert(simtower::original_command_catalog(resources, rating_three)[6] == 16);

  // Direct 1050:0978 and 1140:0394 selector-composition coverage. WM_INITDIALOG passes the
  // stored one-based index (1), so group 9 has no selected sheet initially;
  // WM_MOUSEMOVE over Parking passes icon+1 (18), and the build gate replaces
  // every row with the disabled BITMAP/302 sheet.
  const auto initial_selector = simtower::render_original_command_selector(
      resources, *parking_group, parking_group->selection_index);
  assert(initial_selector.width == 32 && initial_selector.height == 96);
  assert_region(initial_selector, 0, 0, resources.find("BITMAP", 300),
                0, 64, 32, 32);
  const auto parking_hover = simtower::render_original_command_selector(
      resources, *parking_group, 18);
  assert_region(parking_hover, 0, 32, resources.find("BITMAP", 301),
                32, 64, 32, 32);
  const auto disabled_selector = simtower::render_original_command_selector(
      resources, *parking_group, 18, false);
  assert_region(disabled_selector, 0, 32, resources.find("BITMAP", 302),
                32, 64, 32, 32);
  simtower::original_command_select_group_choice(
      resources, rating_three, 6, 2);
  assert(simtower::original_command_catalog(resources, rating_three)[6] == 17);
  assert(simtower::original_command_hit_test(
             resources, rating_three, true, 1, 158) ==
         simtower::OriginalCommandHit(
             {simtower::OriginalCommandHitKind::facility, 9, 11}));
  assert(simtower::original_command_hit_test(resources, 1, false, 1, 62).kind ==
         simtower::OriginalCommandHitKind::none);
  assert(simtower::original_command_hit_test(resources, 1, false, 41, 12).kind ==
         simtower::OriginalCommandHitKind::none);

  // Direct 1058:0773/0803 and 1080:0884 coverage: Rating one uses an outer
  // height of 157, hence a 155-pixel client with
  // the original one-pixel WS_BORDER. Mode 3 is the first catalog entry.
  const auto raster = simtower::render_original_command_palette(
      resources, 1, true, 3, 63, 155);
  assert_region(raster, 0, 0, resources.find("BITMAP", 602), 0, 0, 64, 32);
  assert_region(raster, 0, 32, resources.find("BITMAP", 604), 0, 0, 64, 21);
  assert_region(raster, 0, 53, resources.find("BITMAP", 301), 0, 0, 32, 32);
  assert_region(raster, 32, 53, resources.find("BITMAP", 300), 128, 0, 32, 32);
  assert_region(raster, 0, 85, resources.find("BITMAP", 300), 224, 0, 32, 32);
  assert_region(raster, 32, 85, resources.find("BITMAP", 300), 0, 96, 32, 32);
  assert_region(raster, 0, 117, resources.find("BITMAP", 300), 96, 32, 32, 32);
  // The previous right-hand icon's final scanline overwrites y=116 because
  // 1080:05a1 fills the odd slot before composing catalog icons.
  for (int y = 117; y < 147; ++y) {
    for (int x = 32; x < 63; ++x) {
      assert(raster.at(x, y) == 0x00d9d9d9U);
    }
  }

  const auto selected_edit = simtower::render_original_command_palette(
      resources, 1, true, 2, 63, 155);
  assert_region(selected_edit, 42, 32, resources.find("BITMAP", 605),
                42, 0, 21, 21);

  const auto disabled = simtower::render_original_command_palette(
      resources, 1, false, 3, 63, 155);
  assert_region(disabled, 0, 0, resources.find("BITMAP", 600), 0, 0, 64, 32);
  assert_region(disabled, 0, 32, resources.find("BITMAP", 606), 0, 0, 64, 21);
  assert_region(disabled, 0, 53, resources.find("BITMAP", 302), 0, 0, 32, 32);

  // 1050:0219 -> 1080:07a6(1) presents the original pressed header before
  // 1050:02b3 -> 1080:07a6(0) restores it on WM_LBUTTONUP.
  const auto enabled_pressed = simtower::render_original_command_palette(
      resources, 1, true, 3, 63, 155, true);
  assert_region(enabled_pressed, 0, 0, resources.find("BITMAP", 603),
                0, 0, 64, 32);
  const auto disabled_pressed = simtower::render_original_command_palette(
      resources, 1, false, 3, 63, 155, true);
  assert_region(disabled_pressed, 0, 0, resources.find("BITMAP", 601),
                0, 0, 64, 32);

  return 0;
}
