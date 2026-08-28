#include "original_dib.hpp"
#include "original_dtmp.hpp"
#include "original_information.hpp"
#include "original_resources.hpp"
#include "original_tables.hpp"
#include "original_tdt.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace {

std::vector<std::byte> read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  assert(stream);
  std::vector<char> chars((std::istreambuf_iterator<char>(stream)),
                          std::istreambuf_iterator<char>());
  std::vector<std::byte> bytes(chars.size());
  for (std::size_t index = 0U; index < chars.size(); ++index) {
    bytes[index] = static_cast<std::byte>(
        static_cast<unsigned char>(chars[index]));
  }
  return bytes;
}

void store_u16(std::array<std::byte, 16>& bytes,
               std::size_t offset,
               std::uint16_t value,
               bool byte_swapped) {
  if (byte_swapped) {
    bytes[offset] = static_cast<std::byte>(value >> 8U);
    bytes[offset + 1U] = static_cast<std::byte>(value);
  } else {
    bytes[offset] = static_cast<std::byte>(value);
    bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
  }
}

void store_tenant_u16(simtower::OriginalTdtTenant& tenant,
                      std::size_t offset,
                      std::uint16_t value,
                      bool byte_swapped) {
  if (byte_swapped) {
    tenant.exact_bytes[offset] = static_cast<std::byte>(value >> 8U);
    tenant.exact_bytes[offset + 1U] = static_cast<std::byte>(value);
  } else {
    tenant.exact_bytes[offset] = static_cast<std::byte>(value);
    tenant.exact_bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
  }
}

void store_tenant_u32(simtower::OriginalTdtTenant& tenant,
                      std::size_t offset,
                      std::uint32_t value,
                      bool byte_swapped) {
  if (byte_swapped) {
    tenant.exact_bytes[offset] = static_cast<std::byte>(value >> 24U);
    tenant.exact_bytes[offset + 1U] = static_cast<std::byte>(value >> 16U);
    tenant.exact_bytes[offset + 2U] = static_cast<std::byte>(value >> 8U);
    tenant.exact_bytes[offset + 3U] = static_cast<std::byte>(value);
  } else {
    tenant.exact_bytes[offset] = static_cast<std::byte>(value);
    tenant.exact_bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
    tenant.exact_bytes[offset + 2U] = static_cast<std::byte>(value >> 16U);
    tenant.exact_bytes[offset + 3U] = static_cast<std::byte>(value >> 24U);
  }
}

void store_car_u32(simtower::OriginalTdtElevatorCarRecord& car,
                   std::size_t offset,
                   std::uint32_t value,
                   bool byte_swapped) {
  if (byte_swapped) {
    car.exact_bytes[offset] = static_cast<std::byte>(value >> 24U);
    car.exact_bytes[offset + 1U] = static_cast<std::byte>(value >> 16U);
    car.exact_bytes[offset + 2U] = static_cast<std::byte>(value >> 8U);
    car.exact_bytes[offset + 3U] = static_cast<std::byte>(value);
  } else {
    car.exact_bytes[offset] = static_cast<std::byte>(value);
    car.exact_bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
    car.exact_bytes[offset + 2U] = static_cast<std::byte>(value >> 16U);
    car.exact_bytes[offset + 3U] = static_cast<std::byte>(value >> 24U);
  }
}

void store_retail_u16(simtower::OriginalTdtRetailRecord& retail,
                      std::size_t offset,
                      std::uint16_t value,
                      bool byte_swapped) {
  if (byte_swapped) {
    retail.exact_bytes[offset] = static_cast<std::byte>(value >> 8U);
    retail.exact_bytes[offset + 1U] = static_cast<std::byte>(value);
  } else {
    retail.exact_bytes[offset] = static_cast<std::byte>(value);
    retail.exact_bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
  }
}

simtower::OriginalTdtTenant make_tenant(std::int8_t type,
                                       std::uint8_t key) {
  simtower::OriginalTdtTenant tenant{};
  tenant.type = type;
  tenant.exact_bytes[4] = static_cast<std::byte>(type);
  tenant.exact_bytes[12] = static_cast<std::byte>(key);
  tenant.preserved_07_to_0f[5] = static_cast<std::byte>(key);
  return tenant;
}

simtower::OriginalTdtDocument make_person_tower() {
  auto tower = simtower::make_original_new_tdt();
  tower.header.rating = 2U;
  tower.header.frame_time = 950U;
  tower.floors[10].tenants.push_back(make_tenant(7, 0U));
  tower.floors[10].tenant_index[0] = 0U;
  tower.people_count = 1U;
  tower.people.resize(1U);
  auto& person = tower.people[0].exact_bytes;
  person[0] = std::byte{10};
  person[1] = std::byte{0};
  store_u16(person, 2U, 1U, tower.header.byte_swapped);
  person[4] = std::byte{7};
  person[5] = std::byte{0x40};
  person[6] = std::byte{0xff};
  person[7] = std::byte{10};
  person[9] = std::byte{4};
  store_u16(person, 10U, 900U, tower.header.byte_swapped);
  store_u16(person, 12U, 25U, tower.header.byte_swapped);
  store_u16(person, 14U, 480U, tower.header.byte_swapped);
  return tower;
}

struct TestDibSurface {
  HDC dc{};
  HBITMAP bitmap{};
  HGDIOBJ previous{};
  std::uint32_t* pixels{};

  TestDibSurface(int width, int height) {
    dc = CreateCompatibleDC(nullptr);
    assert(dc);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1U;
    info.bmiHeader.biBitCount = 32U;
    info.bmiHeader.biCompression = BI_RGB;
    void* storage{};
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &storage,
                              nullptr, 0U);
    assert(bitmap && storage);
    pixels = static_cast<std::uint32_t*>(storage);
    previous = SelectObject(dc, bitmap);
  }

  ~TestDibSurface() {
    if (previous) SelectObject(dc, previous);
    if (bitmap) DeleteObject(bitmap);
    if (dc) DeleteDC(dc);
  }
};

}  // namespace

int main(int argc, char** argv) {
  // Direct 1100:39df/3d5b/40d5 modal-wrapper coverage. The rename owners are
  // Main; only Movie Choice uses its caller owner and returns DialogBox result.
  using LauncherKind = simtower::OriginalInformationLauncherKind;
  using Launcher = simtower::OriginalInformationLauncherContract;
  assert(simtower::original_information_launcher_contract(
             LauncherKind::person_rename) ==
         (Launcher{730U, true, false, false}));
  assert(simtower::original_information_launcher_contract(
             LauncherKind::tenant_rename) ==
         (Launcher{732U, true, false, false}));
  assert(simtower::original_information_launcher_contract(
             LauncherKind::movie_choice) ==
         (Launcher{731U, false, true, true}));
  // Direct 1100:43ed rename-dialog gate: only an empty edit disables OK.
  static_assert(!simtower::original_rename_ok_enabled(0U));
  static_assert(simtower::original_rename_ok_enabled(1U));
  static_assert(simtower::original_rename_ok_enabled(0xfeU));

  {
    // Direct 1140:019d coverage: ratings 1/2, 3, and 4+ select consecutive
    // lower/upper threshold pairs from PART head-word bands 5..7 and 8..10.
    simtower::OriginalPartTable thresholds{};
    thresholds.words_00_to_40[5U] = 0xff9cU;  // -100
    thresholds.words_00_to_40[6U] = 0xff38U;  // -200
    thresholds.words_00_to_40[7U] = 0xfed4U;  // -300
    thresholds.words_00_to_40[8U] = 100U;
    thresholds.words_00_to_40[9U] = 200U;
    thresholds.words_00_to_40[10U] = 300U;
    auto tower = simtower::make_original_new_tdt();
    for (const auto [rating, expected] :
         std::array{
             std::pair{1U, std::pair<std::int16_t, std::int16_t>{-100, 100}},
             std::pair{2U, std::pair<std::int16_t, std::int16_t>{-100, 100}},
             std::pair{3U, std::pair<std::int16_t, std::int16_t>{-200, 200}},
             std::pair{4U, std::pair<std::int16_t, std::int16_t>{-300, 300}},
             std::pair{6U, std::pair<std::int16_t, std::int16_t>{-300, 300}},
         }) {
      tower.header.rating = static_cast<std::uint16_t>(rating);
      assert(simtower::original_information_thresholds(tower, thresholds) ==
             expected);
    }
  }

  {
    // Direct 10c0:05cd coverage: after the Elevator leg misses, a hit on a
    // used tall Stair/Escalator record selects DIALOG/761 and its exact index.
    auto tower = simtower::make_original_new_tdt();
    auto& transport = tower.post_elevator.stairs_bd70[3U];
    transport.used = 1U;
    transport.shape = 2U;
    transport.x = 100U;
    transport.floor = 10;
    const auto target = simtower::select_original_magnifier_target(
        tower, 804, 3924, 0, 0);
    assert(target.kind == simtower::OriginalMagnifierTargetKind::
                              vertical_transport_information);
    assert(target.dialog_id == 761U);
    assert(target.vertical_transport_index == 3U);
    assert(target.floor == 10);
  }

  {
    // Exact PEPLEINFODLOGFILTER 1100:020b, 1100:085b/09ad facility-control
    // boundary, ELVINFODLOGFILTER 1100:0fde, and ESCINFODLOGFILTER 1100:1316:
    // only Win16 CTLCOLOR_STATIC receives the palette-matched 0xcccccc brush.
    assert(simtower::original_facility_control_background(true) ==
           simtower::OriginalFacilityControlBackground::gray_cc);
    assert(simtower::original_facility_control_background(false) ==
           simtower::OriginalFacilityControlBackground::null_brush);
  }

  {
    // MOVIETITLEDIALOGFILTER uses one 13-pixel font for immediate painting,
    // WM_PAINT, and control colors, with palette-matched 0xcccccc statics.
    using Style = simtower::OriginalMovieChoiceDialogStyle;
    assert(simtower::original_movie_choice_dialog_style() ==
           (Style{13, true, true, 0xccU, 0xccU, 0xccU}));
  }

  {
    // Complete MOVIETITLEDIALOGFILTER 1100:4138-43da command table. Win16
    // notification codes are ignored; only IDs 1/2/3 are consumed.
    using Action = simtower::OriginalMovieChoiceDialogCommandAction;
    using Plan = simtower::OriginalMovieChoiceDialogCommandPlan;
    assert(simtower::original_movie_choice_dialog_command_plan(1U) ==
           (Plan{Action::new_release, true}));
    assert(simtower::original_movie_choice_dialog_command_plan(2U) ==
           (Plan{Action::cancel, true}));
    assert(simtower::original_movie_choice_dialog_command_plan(3U) ==
           (Plan{Action::classic, true}));
    assert(simtower::original_movie_choice_dialog_command_plan(4U) ==
           (Plan{Action::none, false}));
  }

  {
    // All four original information filters consume WM_ACTIVATE. Only a
    // nonzero activation with a different DS:31a4 modal target redirects;
    // deactivation and self-activation do not call SetActiveWindow.
    using Plan = simtower::OriginalInformationActivationPlan;
    assert(simtower::original_information_activation_plan(
               false, true, false) == (Plan{true, false}));
    assert(simtower::original_information_activation_plan(
               true, false, false) == (Plan{true, false}));
    assert(simtower::original_information_activation_plan(
               true, true, true) == (Plan{true, false}));
    assert(simtower::original_information_activation_plan(
               true, true, false) == (Plan{true, true}));
  }

  {
    // AHOTTADLOGFILTER 1068:00d0-02c1, PEPLEINFODLOGFILTER
    // 1100:0145-01ea, TENANTINFODLOGFILTER 1100:088a-09aa,
    // ELVINFODLOGFILTER 1100:0f3f-0fdb, ESCINFODLOGFILTER 1100:1277-1313,
    // and MOVIETITLEDIALOGFILTER 1100:4167-4240 return TRUE from
    // WM_INITDIALOG without explicitly focusing a control.
    using FocusPlan =
        simtower::OriginalPaintedDialogInitializationFocusPlan;
    assert(simtower::original_painted_dialog_initialization_focus_plan() ==
           (FocusPlan{false, true}));
  }

  {
    // Complete PEPLEINFODLOGFILTER 1100:0116-0395 capture lifecycle: capture
    // precedes initialization and the ID-1 close releases it before EndDialog.
    using CapturePlan = simtower::OriginalPersonInformationCapturePlan;
    assert(simtower::original_person_information_capture_plan() ==
           (CapturePlan{true, true}));
  }

  {
    // Complete ELVINFODLOGFILTER 1100:0f10-11bb and ESCINFODLOGFILTER
    // 1100:1248-14fc outer-click contract. Both consume empty as well as
    // portrait clicks and restore TOPMOST/DS:31a4; ESC alone selects the
    // palette before the common realization.
    using ClickPlan = simtower::OriginalTransportInformationClickPlan;
    using Kind = simtower::OriginalMagnifierTargetKind;
    assert(simtower::original_transport_information_click_plan(
               Kind::elevator_car_information) ==
           (ClickPlan{false, true, true, true, true}));
    assert(simtower::original_transport_information_click_plan(
               Kind::vertical_transport_information) ==
           (ClickPlan{true, true, true, true, true}));
    assert(simtower::original_transport_information_click_plan(
               Kind::facility_information) == ClickPlan{});
  }

  {
    // Complete TENANTINFODLOGFILTER 1100:085b-0e67 outer-click contract:
    // select/realize the palette and restore DS:31a4/TOPMOST even on a miss.
    using ClickPlan = simtower::OriginalFacilityInformationClickPlan;
    assert(simtower::original_facility_information_click_plan() ==
           (ClickPlan{true, true, true, true, true}));
  }

  {
    // TENANTINFODLOGFILTER 1100:0b11-0d57 consumes every WM_COMMAND. IDs 1/7
    // ignore notifications; ID 13 accepts legacy notifications zero and one
    // for rent groups 0..5 and Movie group 10.
    using Action = simtower::OriginalFacilityInformationCommandAction;
    using Plan = simtower::OriginalFacilityInformationCommandPlan;
    assert(simtower::original_facility_information_command_plan(1U, 9U, 0U) ==
           (Plan{Action::close, true}));
    assert(simtower::original_facility_information_command_plan(7U, 9U, 0U) ==
           (Plan{Action::rename, true}));
    assert(simtower::original_facility_information_command_plan(13U, 0U, 5U) ==
           (Plan{Action::change_rent, true}));
    assert(simtower::original_facility_information_command_plan(13U, 1U, 0U) ==
           (Plan{Action::change_rent, true}));
    assert(simtower::original_facility_information_command_plan(13U, 0U, 10U) ==
           (Plan{Action::choose_movie, true}));
    assert(simtower::original_facility_information_command_plan(13U, 1U, 10U) ==
           (Plan{Action::choose_movie, true}));
    assert(simtower::original_facility_information_command_plan(13U, 2U, 10U) ==
           (Plan{Action::none, true}));
    assert(simtower::original_facility_information_command_plan(99U, 0U, 0U) ==
           (Plan{Action::none, true}));
  }

  {
    // Exact 1100:4d1d -> 1208:00b5 preview scaling: 200% enlargement cap,
    // contain when only one dimension binds, and deliberate cover/crop when
    // both shrink.
    using Destination = simtower::OriginalFacilityPreviewDestination;
    simtower::OriginalFacilityPreview preview{0, 0, 32, 24};
    assert(simtower::original_facility_preview_destination(
               preview, 10, 20, 110, 120) ==
           (Destination{28, 46, 92, 94}));
    preview.width = 100;
    preview.height = 100;
    assert(simtower::original_facility_preview_destination(
               preview, 10, 20, 60, 100) ==
           (Destination{-5, 20, 75, 100}));
    assert(simtower::original_facility_preview_destination(
               preview, 10, 20, 310, 170) ==
           (Destination{85, 20, 235, 170}));
    preview.width = 100;
    preview.height = 50;
    assert(simtower::original_facility_preview_destination(
               preview, 10, 20, 210, 120) ==
           (Destination{10, 20, 210, 120}));
  }

  {
    // Direct 1100:4439 presentation coverage: fill the entire DTMP item-2
    // rectangle with exact 0xcccccc gray, center a COLORONCOLOR 200% stretch
    // of the retained world snapshot, and restore the destination DC mode.
    HDC dc = CreateCompatibleDC(nullptr);
    assert(dc);
    BITMAPINFO bitmap{};
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = 10;
    bitmap.bmiHeader.biHeight = -8;
    bitmap.bmiHeader.biPlanes = 1U;
    bitmap.bmiHeader.biBitCount = 32U;
    bitmap.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP target = CreateDIBSection(
        dc, &bitmap, DIB_RGB_COLORS, &bits, nullptr, 0U);
    assert(target && bits);
    HGDIOBJ previous_bitmap = SelectObject(dc, target);
    assert(previous_bitmap && previous_bitmap != HGDI_ERROR);
    auto* pixels = static_cast<std::uint32_t*>(bits);
    std::fill_n(pixels, 80U, 0x00ff00ffU);

    simtower::OriginalWorldRaster raster{};
    raster.width = 2;
    raster.height = 2;
    raster.pixels = {
        0x00ff0000U, 0x0000ff00U,
        0x000000ffU, 0x00ffffffU,
    };
    const simtower::OriginalFacilityPreview preview{0, 0, 2, 2};
    const RECT container{2, 2, 8, 6};
    assert(SetStretchBltMode(dc, HALFTONE) != 0);
    simtower::draw_original_facility_preview(
        dc, raster, preview, container);
    GdiFlush();
    assert(GetStretchBltMode(dc) == HALFTONE);

    const auto pixel = [&](int x, int y) {
      return pixels[static_cast<std::size_t>(y * 10 + x)] & 0x00ffffffU;
    };
    assert(pixel(1, 2) == 0x00ff00ffU);
    assert(pixel(2, 2) == 0x00ccccccU);
    assert(pixel(7, 5) == 0x00ccccccU);
    assert(pixel(8, 5) == 0x00ff00ffU);
    for (int y = 2; y < 4; ++y) {
      assert(pixel(3, y) == 0x00ff0000U);
      assert(pixel(4, y) == 0x00ff0000U);
      assert(pixel(5, y) == 0x0000ff00U);
      assert(pixel(6, y) == 0x0000ff00U);
    }
    for (int y = 4; y < 6; ++y) {
      assert(pixel(3, y) == 0x000000ffU);
      assert(pixel(4, y) == 0x000000ffU);
      assert(pixel(5, y) == 0x00ffffffU);
      assert(pixel(6, y) == 0x00ffffffU);
    }

    assert(SelectObject(dc, previous_bitmap) == target);
    assert(DeleteObject(target));
    assert(DeleteDC(dc));
  }

  assert(argc == 2);
  const auto pack = read_file(argv[1]);
  const simtower::OriginalResources resources(pack);

  {
    // Direct 1100:4514 coverage: the temporary backing expands independently
    // of 1100:4869's crop. Movie parts use 31 cells regardless of serialized
    // span; two- and five-floor families raise only their respective minima.
    using Backing = simtower::OriginalFacilityPreviewBackingCounts;
    auto backing_tower = simtower::make_original_new_tdt();
    auto movie = make_tenant(18, 0U);
    movie.left = 10U;
    movie.right = 100U;
    backing_tower.floors[10].tenants.push_back(movie);
    assert(simtower::original_facility_preview_backing_counts(
               backing_tower, 10, 0U, 10, 1) ==
           (Backing{31, 2, 62, 2}));

    auto party = make_tenant(29, 0U);
    party.left = 10U;
    party.right = 34U;
    backing_tower.floors[10].tenants[0] = party;
    assert(simtower::original_facility_preview_backing_counts(
               backing_tower, 10, 0U, 10, 1) ==
           (Backing{24, 2, 48, 2}));

    auto cathedral = make_tenant(36, 0U);
    cathedral.left = 10U;
    cathedral.right = 38U;
    backing_tower.floors[10].tenants[0] = cathedral;
    assert(simtower::original_facility_preview_backing_counts(
               backing_tower, 10, 0U, 40, 3) ==
           (Backing{40, 5, 80, 5}));

    auto malformed = make_tenant(7, 0U);
    malformed.left = 100U;
    malformed.right = 10U;
    backing_tower.floors[10].tenants[0] = malformed;
    assert(simtower::original_facility_preview_backing_counts(
               backing_tower, 10, 0U, 12, 6) ==
           (Backing{12, 6, 24, 6}));
    assert(simtower::original_facility_preview_backing_counts(
               backing_tower, -1, 0U, 12, 6) ==
           (Backing{12, 6, 24, 6}));
  }

  {
    // Exact 1100:4869 source-world RECT arithmetic has no minimum crop width.
    // Its commercial branch also always replaces the serialized span with
    // DS:74ba[source_type], including a zero table entry.
    auto zero_width_tower = simtower::make_original_new_tdt();
    simtower::OriginalPartTable zero_width_part{};
    auto zero_width_office = make_tenant(7, 0U);
    zero_width_office.left = 24U;
    zero_width_office.right = 24U;
    store_tenant_u16(zero_width_office, 0U, zero_width_office.left,
                     zero_width_tower.header.byte_swapped);
    store_tenant_u16(zero_width_office, 2U, zero_width_office.right,
                     zero_width_tower.header.byte_swapped);
    zero_width_tower.floors[10].tenants.push_back(zero_width_office);
    zero_width_tower.floors[10].tenant_index[0] = 0U;
    const auto zero_width_information =
        simtower::original_facility_information(
            resources, zero_width_tower, zero_width_part, 10, 0U);
    assert(zero_width_information.valid);
    assert((zero_width_information.preview ==
            simtower::OriginalFacilityPreview{192, 3936, 0, 24}));
    assert(!zero_width_information.preview.valid());

    auto zero_table_tower = simtower::make_original_new_tdt();
    simtower::OriginalPartTable zero_table_part{};
    auto source = make_tenant(0, 0U);
    source.left = 30U;
    source.right = 34U;
    store_tenant_u16(source, 0U, source.left,
                     zero_table_tower.header.byte_swapped);
    store_tenant_u16(source, 2U, source.right,
                     zero_table_tower.header.byte_swapped);
    zero_table_tower.floors[10].tenants.push_back(source);
    zero_table_tower.floors[10].tenant_index[0] = 0U;
    auto restaurant = make_tenant(6, 0U);
    restaurant.left = 40U;
    restaurant.right = 64U;
    store_tenant_u16(restaurant, 0U, restaurant.left,
                     zero_table_tower.header.byte_swapped);
    store_tenant_u16(restaurant, 2U, restaurant.right,
                     zero_table_tower.header.byte_swapped);
    store_tenant_u16(restaurant, 6U, 0U,
                     zero_table_tower.header.byte_swapped);
    zero_table_tower.floors[11].tenants.push_back(restaurant);
    zero_table_tower.floors[11].tenant_index[0] = 0U;
    zero_table_tower.retail[0].exact_bytes[0] = std::byte{10};
    zero_table_tower.retail[0].exact_bytes[1] = std::byte{0};
    const auto zero_table_information =
        simtower::original_facility_information(
            resources, zero_table_tower, zero_table_part, 11, 0U);
    assert(zero_table_information.valid);
    assert((zero_table_information.preview ==
            simtower::OriginalFacilityPreview{240, 3936, 0, 24}));
    assert(!zero_table_information.preview.valid());
  }

  {
    // 1100:4a17-4b0c uses the linked source tenant's type-width table entry,
    // not its serialized span, for Restaurant/Retail/Fast Food previews.
    constexpr std::array<std::pair<std::int8_t, int>, 3> cases{{
        {6, 192}, {10, 96}, {12, 128}}};
    for (const auto [type, expected_width] : cases) {
      auto commercial_tower = simtower::make_original_new_tdt();
      simtower::OriginalPartTable commercial_part{};
      auto tenant = make_tenant(type, 0U);
      tenant.left = 20U;
      tenant.right = 21U;  // Deliberately disagrees with the type-width table.
      store_tenant_u16(tenant, 0U, tenant.left,
                       commercial_tower.header.byte_swapped);
      store_tenant_u16(tenant, 2U, tenant.right,
                       commercial_tower.header.byte_swapped);
      store_tenant_u16(tenant, 6U, 0U,
                       commercial_tower.header.byte_swapped);
      commercial_tower.floors[10].tenants.push_back(tenant);
      commercial_tower.floors[10].tenant_index[0] = 0U;
      commercial_tower.retail[0].exact_bytes[0] = std::byte{10};
      commercial_tower.retail[0].exact_bytes[1] = std::byte{0};
      const auto information = simtower::original_facility_information(
          resources, commercial_tower, commercial_part, 10, 0U);
      assert(information.valid && information.preview.valid());
      assert((information.preview == simtower::OriginalFacilityPreview{
          160, 3936, expected_width, 24}));
    }
  }

  {
    // 1100:465a anchors each Metro preview to the top part by subtracting 31
    // from its part type. All three clicked parts therefore produce the same
    // vertical crop. 1100:4b82-4bf3 retains the twelve-pixel inset and stops
    // at the second boundary, producing a 60-pixel crop rather than 72.
    auto metro_tower = simtower::make_original_new_tdt();
    simtower::OriginalPartTable metro_part{};
    for (std::int16_t part_index = 0; part_index < 3; ++part_index) {
      auto tenant = make_tenant(
          static_cast<std::int8_t>(31 + part_index),
          static_cast<std::uint8_t>(part_index));
      tenant.left = 20;
      tenant.right = 50;
      store_tenant_u16(tenant, 6U, 2U,
                       metro_tower.header.byte_swapped);
      const auto floor = static_cast<std::size_t>(80 - part_index);
      metro_tower.floors[floor].tenants.push_back(tenant);
      const auto information = simtower::original_facility_information(
          resources, metro_tower, metro_part,
          static_cast<std::int16_t>(floor), 0U);
      assert(information.valid && information.preview.valid());
      assert(information.preview.view_x == 160 &&
             information.preview.view_y == 1416);
      assert(information.preview.width == 240 &&
             information.preview.height == 60);
    }

    // Direct 1108:07fe coverage: before 0x00f0 transit shows item 31; during
    // the ordinary day a variant-two record shows item 32; after phase four
    // item 30 has priority over that variant.
    const auto advisory = [&](std::uint16_t frame, std::uint16_t item) {
      metro_tower.header.frame_time = frame;
      const auto information = simtower::original_facility_information(
          resources, metro_tower, metro_part, 80, 0U);
      const auto expected = simtower::original_strl_entry(
          resources.find("STRL", 711), item);
      return std::find(information.advisory_lines.begin(),
                       information.advisory_lines.begin() +
                           information.advisory_line_count,
                       expected) != information.advisory_lines.begin() +
                                        information.advisory_line_count;
    };
    assert(advisory(0x00efU, 31U));
    assert(advisory(0x00f0U, 32U));
    assert(advisory(0x07d1U, 30U));
  }

  {
    // 1100:4bfb-4c72 applies the corresponding five-floor Cathedral crop to
    // every component: top+12 through the fifth boundary is 168 pixels.
    auto cathedral_tower = simtower::make_original_new_tdt();
    simtower::OriginalPartTable cathedral_part{};
    for (std::int16_t part_index = 0; part_index < 5; ++part_index) {
      auto tenant = make_tenant(
          static_cast<std::int8_t>(36 + part_index),
          static_cast<std::uint8_t>(part_index));
      tenant.left = 30;
      tenant.right = 58;
      const auto floor = static_cast<std::size_t>(100 - part_index);
      cathedral_tower.floors[floor].tenants.push_back(tenant);
      const auto information = simtower::original_facility_information(
          resources, cathedral_tower, cathedral_part,
          static_cast<std::int16_t>(floor), 0U);
      assert(information.valid && information.preview.valid());
      assert((information.preview == simtower::OriginalFacilityPreview{
          240, 696, 224, 168}));
    }
  }

  const auto dtmp_763 = simtower::parse_original_dtmp(
      resources.find("DTMP", 763));
  assert(dtmp_763.width_or_header == 296U);
  assert(dtmp_763.height_or_header == 199U);
  assert(dtmp_763.rectangles.size() == 12U);
  assert((dtmp_763.rectangles[1] == simtower::OriginalDtmpRect{
      12, 12, 68, 68}));
  assert((dtmp_763.rectangles[5] == simtower::OriginalDtmpRect{
      77, 82, 284, 100}));
  assert((dtmp_763.rectangles[7] == simtower::OriginalDtmpRect{
      77, 128, 284, 146}));

  const auto dtmp_764 = simtower::parse_original_dtmp(
      resources.find("DTMP", 764));
  assert(dtmp_764.width_or_header == 283U);
  assert(dtmp_764.height_or_header == 165U);
  assert(dtmp_764.rectangles.size() == 12U);
  assert(dtmp_764.rectangles[5] == simtower::OriginalDtmpRect{});
  assert(dtmp_764.rectangles[7] == simtower::OriginalDtmpRect{});

  const auto dtmp_730 = simtower::parse_original_dtmp(
      resources.find("DTMP", 730));
  assert(dtmp_730.width_or_header == 283U);
  assert(dtmp_730.height_or_header == 130U);
  assert(dtmp_730.rectangles.size() == 5U);
  assert((dtmp_730.rectangles[3] == simtower::OriginalDtmpRect{
      79, 40, 204, 69}));

  const auto dtmp_731 = simtower::parse_original_dtmp(
      resources.find("DTMP", 731));
  assert(dtmp_731.width_or_header == 280U);
  assert(dtmp_731.height_or_header == 136U);
  const auto dtmp_732 = simtower::parse_original_dtmp(
      resources.find("DTMP", 732));
  assert(dtmp_732.width_or_header == 283U);
  assert(dtmp_732.height_or_header == 130U);

  struct FacilityTemplateExpectation {
    int id;
    std::uint16_t width;
    std::uint16_t height;
    std::size_t rectangles;
  };
  constexpr std::array<FacilityTemplateExpectation, 13>
      kFacilityTemplates{{
          {748, 282, 265, 13}, {749, 304, 267, 13},
          {750, 303, 265, 13}, {751, 294, 266, 13},
          {752, 298, 265, 13}, {753, 287, 279, 13},
          {754, 283, 190, 8},  {755, 287, 189, 8},
          {756, 237, 190, 8},  {757, 280, 281, 9},
          {758, 280, 360, 14}, {759, 280, 280, 9},
          {760, 287, 279, 12},
      }};
  for (const auto& expected : kFacilityTemplates) {
    const auto dtmp = simtower::parse_original_dtmp(
        resources.find("DTMP", expected.id));
    assert(dtmp.width_or_header == expected.width);
    assert(dtmp.height_or_header == expected.height);
    assert(dtmp.rectangles.size() == expected.rectangles);
  }

  {
    // Direct 1100:1b53/1cbb coverage: Elevator and Stair/Escalator type labels
    // plus signed car occupancy /
    // capacity text accompany 1100:327f/35b7's nonnegative passenger slots,
    // preserving item-4 geometry and named/VIP portrait precedence.
    auto transport_tower = simtower::make_original_new_tdt();
    auto& elevator = transport_tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 0U;
    elevator.capacity = 3U;
    auto& car = elevator.car_records[0];
    car.exact_bytes[3] = std::byte{2};
    car.exact_bytes[184] = std::byte{10};
    car.exact_bytes[185] = std::byte{0xff};
    car.exact_bytes[186] = std::byte{11};
    store_car_u32(car, 16U, 0U, transport_tower.header.byte_swapped);
    store_car_u32(car, 20U, 1U, transport_tower.header.byte_swapped);
    store_car_u32(car, 24U, 2U, transport_tower.header.byte_swapped);
    transport_tower.people_count = 3U;
    transport_tower.people.resize(3U);
    transport_tower.people[0].exact_bytes[4] = std::byte{7};
    transport_tower.people[2].exact_bytes[4] = std::byte{15};
    transport_tower.header.person_link_count = 1U;
    transport_tower.post_elevator.dce4_person_indices[0] = 0;
    transport_tower.post_elevator.b928 = 1U;
    transport_tower.post_elevator.b924 = 2;

    simtower::OriginalMagnifierTarget target{};
    target.kind =
        simtower::OriginalMagnifierTargetKind::elevator_car_information;
    target.dialog_id = 761U;
    target.elevator_index = 0U;
    target.elevator_car_index = 0;
    auto transport = simtower::original_transport_information_text(
        resources, transport_tower, target);
    const auto elevator_dtmp = simtower::parse_original_dtmp(
        resources.find("DTMP", 761));
    const auto& elevator_row = elevator_dtmp.rectangles[3U];
    assert(transport.valid && transport.secondary == "2 / 3");
    assert(transport.person_sprites.size() == 2U);
    assert(transport.person_sprites[0].person_index == 0U &&
           transport.person_sprites[0].bitmap_id == 702U &&
           transport.person_sprites[0].frame == 1 &&
           transport.person_sprites[0].destination_x ==
               elevator_row.left + 2 &&
           transport.person_sprites[0].destination_y ==
               elevator_row.top + 5);
    assert(transport.person_sprites[1].person_index == 2U &&
           transport.person_sprites[1].bitmap_id == 703U &&
           transport.person_sprites[1].frame == 10 &&
           transport.person_sprites[1].destination_x ==
               elevator_row.left + 10);
    assert(simtower::original_information_person_sprite_hit(
               transport.person_sprites, elevator_row.left + 2,
               elevator_row.top + 5) == 0U);
    assert(simtower::original_information_person_sprite_hit(
               transport.person_sprites, elevator_row.left + 9,
               elevator_row.top + 28) == 0U);
    assert(simtower::original_information_person_sprite_hit(
               transport.person_sprites, elevator_row.left + 10,
               elevator_row.top + 5) == 2U);
    assert(!simtower::original_information_person_sprite_hit(
        transport.person_sprites, elevator_row.left + 26,
        elevator_row.top + 5));
    // 1100:5043 uses the complete DTMP item-4/item-9 panels for the hand
    // cursor, with Win16 PTINRECT right/bottom exclusion.
    assert(simtower::original_information_portrait_panel_hit(
        elevator_dtmp, elevator_row.left, elevator_row.top));
    assert(simtower::original_information_portrait_panel_hit(
        elevator_dtmp, elevator_row.right - 1,
        elevator_row.bottom - 1));
    assert(!simtower::original_information_portrait_panel_hit(
        elevator_dtmp, elevator_row.right, elevator_row.top));
    assert(!simtower::original_information_portrait_panel_hit(
        elevator_dtmp, elevator_row.left, elevator_row.bottom));
    const auto& elevator_overflow_row = elevator_dtmp.rectangles[8U];
    assert(simtower::original_information_portrait_panel_hit(
        elevator_dtmp, elevator_overflow_row.left,
        elevator_overflow_row.top));
    // 1100:4fba's ordinary-Tenant path tests item 4 only. 1100:5043 adds
    // item 9 solely for Tenant dialog groups 9..11 and transport dialogs.
    assert(!simtower::original_information_portrait_panel_hit(
        elevator_dtmp, elevator_overflow_row.left,
        elevator_overflow_row.top, false));
    assert(!simtower::original_information_portrait_panel_hit(
        elevator_dtmp, 0, 0));

    // 1100:3431 uses 1218:0771's family/state/byte-8 scan for live
    // Stair/Escalator occupants, rather than the Elevator's explicit slots.
    auto& first = transport_tower.people[0].exact_bytes;
    first[5] = std::byte{0x40};
    first[8] = std::byte{0};
    auto& second = transport_tower.people[1].exact_bytes;
    second[4] = std::byte{15};
    second[5] = std::byte{3};
    second[8] = std::byte{0};
    auto& rejected = transport_tower.people[2].exact_bytes;
    rejected[4] = std::byte{7};
    rejected[5] = std::byte{0x3f};
    rejected[8] = std::byte{0};
    auto& stair = transport_tower.post_elevator.stairs_bd70[0];
    stair.used = 1U;
    stair.shape = 0U;
    stair.word_6 = 1U;
    stair.word_8 = 1U;
    transport_tower.post_elevator.b924 = 1;
    target.kind = simtower::OriginalMagnifierTargetKind::
        vertical_transport_information;
    target.dialog_id = 762U;
    target.vertical_transport_index = 0U;
    transport = simtower::original_transport_information_text(
        resources, transport_tower, target);
    const auto stair_dtmp = simtower::parse_original_dtmp(
        resources.find("DTMP", 762));
    const auto& stair_row = stair_dtmp.rectangles[3U];
    assert(transport.valid && transport.secondary == "2");
    assert(transport.person_sprites.size() == 2U);
    assert(transport.person_sprites[0].person_index == 0U &&
           transport.person_sprites[0].bitmap_id == 702U &&
           transport.person_sprites[0].destination_x == stair_row.left + 2);
    assert(transport.person_sprites[1].person_index == 1U &&
           transport.person_sprites[1].bitmap_id == 703U &&
           transport.person_sprites[1].destination_x == stair_row.left + 10);
  }

  simtower::OriginalPartTable part{};
  part.words_00_to_40[5] = 100U;
  part.words_00_to_40[8] = 200U;
  auto tower = make_person_tower();
  // Direct 1100:21a1/2236/1f2e coverage: produce the complete person-specific
  // origin, activity, evaluation, stress, and portrait model from persisted
  // state.
  auto information = simtower::original_person_information(
      resources, tower, part, 0U);
  assert(information.valid);
  assert(information.dialog_id == 763U);
  assert(information.owner_floor == 10);
  assert(information.owner_tenant_index == 0U);
  assert(information.owner_type == 7);
  assert(information.portrait_frame == 1);
  assert(information.portrait_variant ==
         simtower::OriginalPersonPortraitVariant::normal);
  assert(information.display_name == "Salesman");
  assert(information.origin_text == "Office, Floor 1");
  assert(information.activity_text == "Lobby for sales calls");
  assert(information.evaluation.visible);
  assert(information.evaluation.value == 120);
  assert(information.evaluation.lower == 100);
  assert(information.evaluation.upper == 200);
  assert(information.evaluation.band == 1U);
  assert(information.stress.visible);
  assert(information.stress.value == 75);

  // Direct 1100:1fad and 11e0:01d8/02bd/0358 meter-helper coverage: reverse threshold-line
  // and forward fill endpoints, IDIV truncation, zero/full bounds, and the
  // three palette RGB
  // literals consumed by the production brush path.
  assert(simtower::original_information_meter_fill_right(
             2, 98, 75, 300, true) == 74);
  assert(simtower::original_information_meter_fill_right(
             2, 98, 75, 300, false) == 26);
  assert(simtower::original_information_meter_fill_right(
             2, 98, -1, 300, true) == 98);
  assert(simtower::original_information_meter_fill_right(
             2, 98, -1, 300, false) == 2);
  assert(simtower::original_information_meter_fill_right(
             2, 98, 300, 300, true) == 2);
  assert(simtower::original_information_meter_fill_right(
             2, 98, 300, 300, false) == 98);
  assert(simtower::original_information_meter_colorref(0U) == 0x00ff0000U);
  assert(simtower::original_information_meter_colorref(1U) == 0x0000ffffU);
  assert(simtower::original_information_meter_colorref(2U) == 0x000000ffU);

  {
    // Direct 1100:1dca coverage for every DS:b3a6 branch. Main-world mode
    // adds the live wait and suppresses a zero start; transport mode uses
    // only the retained low ten bits; facility mode deliberately uses zero.
    using PersonContext = simtower::OriginalPersonInformationContext;
    const auto transport_information =
        simtower::original_person_information(
            resources, tower, part, 0U, PersonContext::transport_dialog);
    assert(transport_information.stress.visible);
    assert(transport_information.stress.value == 25);

    const auto facility_information =
        simtower::original_person_information(
            resources, tower, part, 0U, PersonContext::facility_dialog);
    assert(facility_information.stress.visible);
    assert(facility_information.stress.value == 0);

    auto no_live_wait = tower;
    store_u16(no_live_wait.people[0].exact_bytes, 10U, 0U,
              no_live_wait.header.byte_swapped);
    assert(!simtower::original_person_information(
                resources, no_live_wait, part, 0U,
                PersonContext::main_world)
                .stress.visible);

    // 11d8:0423 compares the wrapping elapsed word as signed. A start one
    // tick ahead therefore discounts to zero at a two-story Lobby.
    auto wrapped_wait = tower;
    wrapped_wait.header.frame_time = 99U;
    wrapped_wait.header.lobby_height = 2U;
    auto& wrapped_person = wrapped_wait.people[0].exact_bytes;
    store_u16(wrapped_person, 10U, 100U,
              wrapped_wait.header.byte_swapped);
    store_u16(wrapped_person, 12U, 0U,
              wrapped_wait.header.byte_swapped);
    const auto wrapped_information =
        simtower::original_person_information(
            resources, wrapped_wait, part, 0U,
            PersonContext::main_world);
    assert(wrapped_information.stress.visible);
    assert(wrapped_information.stress.value == 0);

    // Other persisted Lobby heights bypass 11d8:0423's two discounts and
    // retain the signed wrapped word verbatim.
    wrapped_wait.header.lobby_height = 4U;
    const auto undiscounted_information =
        simtower::original_person_information(
            resources, wrapped_wait, part, 0U,
            PersonContext::main_world);
    assert(undiscounted_information.stress.visible);
    assert(undiscounted_information.stress.value == -1);
  }

  {
    // Direct 1100:3856 portrait-frame jump-table coverage. Type 7's final
    // comparison is signed JG, so a high-bit persisted word still selects
    // frame one after the exact word-four/word-five cases are excluded.
    auto portrait_tower = make_person_tower();
    auto& portrait = portrait_tower.people[0].exact_bytes;
    const auto frame_for = [&](std::int8_t type, std::uint16_t word) {
      portrait[4] = static_cast<std::byte>(type);
      store_u16(portrait, 2U, word, portrait_tower.header.byte_swapped);
      return simtower::original_person_information(
                 resources, portrait_tower, part, 0U)
          .portrait_frame;
    };
    assert(frame_for(3, 2U) == 4 && frame_for(5, 3U) == 0);
    assert(frame_for(7, 4U) == 2 && frame_for(7, 5U) == 4);
    assert(frame_for(7, 0x8000U) == 1 && frame_for(7, 2U) == 0);
    assert(frame_for(9, 0U) == 0 && frame_for(9, 1U) == 8);
    assert(frame_for(9, 2U) == 3 && frame_for(9, 3U) == -1);
    assert(frame_for(14, 0xffffU) == 5);
    assert(frame_for(15, 0xffffU) == 10);
    assert(frame_for(18, 1U) == 2 && frame_for(18, 3U) == 4);
    assert(frame_for(18, 5U) == 6 && frame_for(18, 7U) == 8);
    assert(frame_for(18, 0U) == 0);
  }
  assert(information.stress.band == 0U);

  // Direct 1188:04db/0541/05a7 coverage: the fixed dce4 table is searched
  // only through b402's live count, returns the matching slot, and selects
  // that slot's separately allocated sixteen-byte saved-name record.
  assert(!simtower::original_person_name_slot(tower, 0U));
  auto changed = simtower::set_original_person_name(tower, 0U, "Alice");
  assert(changed.status == simtower::OriginalPersonNameStatus::added);
  assert(changed.changed);
  assert(tower.header.person_link_count == 1U);
  assert(tower.post_elevator.dce4_person_indices[0] == 0);
  assert(simtower::original_person_name_slot(tower, 0U) == 0U);
  assert(!simtower::original_person_name_slot(tower, 1U));
  assert(simtower::original_person_saved_name(tower, 0U) == "Alice");
  information = simtower::original_person_information(
      resources, tower, part, 0U);
  assert(information.display_name == "Alice");
  assert(information.portrait_variant ==
         simtower::OriginalPersonPortraitVariant::named);

  // 1100:151b gives a saved name text precedence, but 1100:364a gives the
  // periodic VIP portrait row precedence.
  tower.post_elevator.b928 = 1U;
  tower.post_elevator.b924 = 0;
  information = simtower::original_person_information(
      resources, tower, part, 0U);
  assert(information.display_name == "Alice");
  assert(information.portrait_variant ==
         simtower::OriginalPersonPortraitVariant::vip);

  changed = simtower::set_original_person_name(tower, 0U, "Al");
  assert(changed.status == simtower::OriginalPersonNameStatus::updated);
  assert(changed.changed);
  // Existing records retain bytes after the newly written NUL, exactly as
  // LSTRCPY into the already allocated sixteen-byte block does.
  assert(tower.person_link_names[0].exact_bytes[3] == std::byte{'c'});
  assert(simtower::original_person_saved_name(tower, 0U) == "Al");
  changed = simtower::remove_original_person_name(tower, 0U);
  assert(changed.status == simtower::OriginalPersonNameStatus::removed);
  assert(changed.changed);
  assert(tower.header.person_link_count == 0U);
  assert(tower.post_elevator.dce4_person_indices[0] == -1);
  assert(simtower::remove_original_person_name(tower, 0U).status ==
         simtower::OriginalPersonNameStatus::not_named);
  assert(simtower::set_original_person_name(tower, 0U, "").status ==
         simtower::OriginalPersonNameStatus::empty);
  assert(simtower::set_original_person_name(
             tower, 0U, "1234567890123456").status ==
         simtower::OriginalPersonNameStatus::too_long);

  // All twenty slots are available even when the source save predates 0x23;
  // 1188:061c tests the live count against 20.
  tower.people_count = 21U;
  tower.people.resize(21U, tower.people[0]);
  for (std::size_t person = 0U; person < 20U; ++person) {
    assert(simtower::set_original_person_name(
               tower, person, "N").status ==
           simtower::OriginalPersonNameStatus::added);
  }
  assert(simtower::set_original_person_name(tower, 20U, "Full").status ==
         simtower::OriginalPersonNameStatus::full);

  // 1100:1716's Office model: exact title/floor suffix, status, age,
  // original rent strings, rating thresholds, and a crop of the real world.
  auto facilities = simtower::make_original_new_tdt();
  facilities.header.rating = 2U;
  auto office = make_tenant(7, 0U);
  office.left = 10U;
  office.right = 14U;
  office.status = 0U;
  office.rent_rate = 1U;
  store_tenant_u16(office, 0U, office.left,
                   facilities.header.byte_swapped);
  store_tenant_u16(office, 2U, office.right,
                   facilities.header.byte_swapped);
  office.exact_bytes[5] = std::byte{0};
  office.exact_bytes[16] = std::byte{1};
  office.exact_bytes[17] = std::byte{5};
  facilities.floors[10].tenants.push_back(office);
  facilities.floors[10].tenant_index[0] = 0U;

  auto facility = simtower::original_facility_information(
      resources, facilities, part, 10, 0U);
  assert(facility.valid);
  assert(facility.dialog_id == 748U && facility.dialog_group == 0U);
  assert(facility.display_name == "Office, Floor 1");

  {
    // Direct 1100:232e coverage. The original name painter folds the paired
    // Movie/Theater and Metro halves onto their owning type/floor, and all
    // five Cathedral slices onto type 36 at floor + type - 40 before loading
    // STRL/710 and appending 1100:27a7's floor suffix.
    struct NormalizedNameCase {
      std::int8_t type;
      std::int16_t floor;
      std::int8_t normalized_type;
      std::int16_t normalized_floor;
    };
    constexpr std::array normalization_cases{
        NormalizedNameCase{18, 50, 18, 49},
        NormalizedNameCase{19, 50, 18, 50},
        NormalizedNameCase{20, 50, 20, 49},
        NormalizedNameCase{21, 50, 20, 50},
        NormalizedNameCase{29, 50, 29, 49},
        NormalizedNameCase{30, 50, 29, 50},
        NormalizedNameCase{34, 50, 34, 49},
        NormalizedNameCase{35, 50, 34, 50},
        NormalizedNameCase{31, 50, 31, 49},
        NormalizedNameCase{32, 50, 31, 50},
        NormalizedNameCase{33, 50, 31, 51},
        NormalizedNameCase{36, 50, 36, 46},
        NormalizedNameCase{37, 50, 36, 47},
        NormalizedNameCase{38, 50, 36, 48},
        NormalizedNameCase{39, 50, 36, 49},
        NormalizedNameCase{40, 50, 36, 50},
    };
    const auto floor_suffix = [&](std::int16_t floor_number) {
      auto text = simtower::original_strl_entry(
          resources.find("STRL", 712), 1U);
      if (floor_number >= 10) {
        text += std::to_string(floor_number - 9);
      } else {
        text += simtower::original_strl_entry(
            resources.find("STRL", 712), 2U);
        text += std::to_string(10 - floor_number);
      }
      return text;
    };
    for (const auto& entry : normalization_cases) {
      auto name_tower = simtower::make_original_new_tdt();
      name_tower.header.rating = 2U;
      name_tower.floors[static_cast<std::size_t>(entry.floor)]
          .tenants.push_back(make_tenant(entry.type, 0U));
      if (entry.normalized_floor != entry.floor) {
        name_tower.floors[static_cast<std::size_t>(entry.normalized_floor)]
            .tenants.push_back(make_tenant(entry.normalized_type, 0U));
      }
      const auto normalized = simtower::original_facility_information(
          resources, name_tower, part, entry.floor, 0U);
      const auto expected = simtower::original_strl_entry(
                                resources.find("STRL", 710),
                                static_cast<std::uint16_t>(
                                    entry.normalized_type + 1)) +
                            floor_suffix(entry.normalized_floor);
      assert(normalized.valid);
      assert(normalized.display_name == expected);
    }
  }

  {
    // Direct 1100:22d5, 1100:232e, and 1100:50e4 coverage. Restaurant,
    // Retail, and Fast Food
    // dispatch linked retail byte 11 to STRL/714, /716, and /715 respectively;
    // a saved tenant name takes precedence over that subtype.
    struct ServiceNameCase {
      std::int8_t type;
      int resource_id;
      std::uint8_t subtype;
    };
    constexpr std::array service_cases{
        ServiceNameCase{6, 714, 0U},
        ServiceNameCase{10, 716, 1U},
        ServiceNameCase{12, 715, 2U},
    };
    for (const auto& entry : service_cases) {
      auto service_tower = simtower::make_original_new_tdt();
      service_tower.header.rating = 2U;
      auto service = make_tenant(entry.type, 0U);
      store_tenant_u16(service, 6U, 0U,
                       service_tower.header.byte_swapped);
      service_tower.floors[10].tenants.push_back(service);
      service_tower.floors[10].tenant_index[0] = 0U;
      service_tower.retail[0].exact_bytes[11] =
          static_cast<std::byte>(entry.subtype);
      auto service_information = simtower::original_facility_information(
          resources, service_tower, part, 10, 0U);
      assert(service_information.valid);
      assert(service_information.display_name ==
             simtower::original_strl_entry(
                 resources.find("STRL", entry.resource_id),
                 static_cast<std::uint16_t>(entry.subtype + 1U)) +
                 std::string{", Floor 1"});

      const auto named = simtower::set_original_tenant_name(
          service_tower, 10, 0U, "Named Service");
      assert(named.status == simtower::OriginalTenantNameStatus::added);
      service_information = simtower::original_facility_information(
          resources, service_tower, part, 10, 0U);
      assert(service_information.display_name == "Named Service, Floor 1");
    }
  }

  assert(facility.occupancy_text == "Occupied");
  assert(facility.age_text == "1Year2Q");
  assert(facility.rent_control_visible && facility.rent_control_enabled);
  assert(facility.selected_rent_rate == 1U);
  assert((facility.rent_choices == std::array<std::string, 4>{
      "$15000", "$10000", "$5000", "$2000"}));
  assert((facility.preview == simtower::OriginalFacilityPreview{
      80, 3936, 32, 24}));

  // Direct 1100:248d coverage: byte 17 is an unsigned quarter count. Verify
  // every division boundary, the last ordinary value, and STRL/713 item 5's
  // age-120 saturation branch through the production information builder.
  const std::array<std::pair<std::uint8_t, std::string>, 5>
      expected_age_texts{{
          {0U, "1Q"},
          {3U, "4Q"},
          {4U, "1Year1Q"},
          {119U, "29Year4Q"},
          {120U, simtower::original_strl_entry(
                     resources.find("STRL", 713), 5U)},
      }};
  for (const auto& [age, expected] : expected_age_texts) {
    facilities.floors[10].tenants[0].exact_bytes[17] =
        static_cast<std::byte>(age);
    assert(simtower::original_facility_information(
               resources, facilities, part, 10, 0U)
               .age_text == expected);
  }
  facilities.floors[10].tenants[0].exact_bytes[17] = std::byte{5};

  // Direct 1100:0644 coverage: after CB_RESETCONTENT, the six-way b3ac jump
  // table adds exactly four literal strings to combo item 13 in this order.
  // An out-of-range group adds nothing.
  const std::array<std::array<std::string, 4>, 6> expected_rent_choices{{
      {"$15000", "$10000", "$5000", "$2000"},
      {"$3000", "$2000", "$1500", "$500"},
      {"$4500", "$3000", "$2000", "$800"},
      {"$9000", "$6000", "$4000", "$1500"},
      {"$200000", "$150000", "$100000", "$40000"},
      {"$20000", "$15000", "$10000", "$4000"},
  }};
  for (std::uint8_t group = 0U; group < expected_rent_choices.size();
       ++group) {
    assert(simtower::original_rent_choices(group) ==
           expected_rent_choices[group]);
  }
  assert((simtower::original_rent_choices(6U) ==
          std::array<std::string, 4>{}));

  // Direct 1108:014b coverage. 016e addresses runtime floor_base +
  // tenant*18 + 6. The six-byte
  // floor header means this is serialized tenant word +0 (left), while +6 is
  // an unrelated type-specific link/state word. A nearby Stair therefore
  // produces no distance warning even when that link word is far away.
  auto distance_tower = simtower::make_original_new_tdt();
  distance_tower.header.rating = 2U;
  auto distance_office = make_tenant(7, 0U);
  distance_office.left = 100U;
  distance_office.right = 104U;
  distance_office.exact_bytes[14] = std::byte{1};
  store_tenant_u16(distance_office, 0U, distance_office.left,
                   distance_tower.header.byte_swapped);
  store_tenant_u16(distance_office, 2U, distance_office.right,
                   distance_tower.header.byte_swapped);
  store_tenant_u16(distance_office, 6U, 0U,
                   distance_tower.header.byte_swapped);
  distance_tower.floors[11].tenants.push_back(distance_office);
  distance_tower.floors[11].tenant_index[0] = 0U;
  auto& distance_stair = distance_tower.post_elevator.stairs_bd70[0];
  distance_stair.used = 1U;
  distance_stair.shape = 0U;
  distance_stair.x = 100U;
  distance_stair.floor = 10;
  const auto distance_information = simtower::original_facility_information(
      resources, distance_tower, part, 11, 0U);
  assert(distance_information.valid);
  assert(distance_information.advisory_line_count == 0U);

  distance_stair.x = 180U;
  auto distance_warning = simtower::original_facility_information(
      resources, distance_tower, part, 11, 0U);
  assert(distance_warning.advisory_line_count == 1U);
  assert(distance_warning.advisory_lines[0] == simtower::original_strl_entry(
      resources.find("STRL", 711), 14U));
  distance_stair.x = 225U;
  distance_warning = simtower::original_facility_information(
      resources, distance_tower, part, 11, 0U);
  assert(distance_warning.advisory_lines[0] == simtower::original_strl_entry(
      resources.find("STRL", 711), 15U));
  distance_stair.shape = 1U;
  distance_stair.x = 180U;
  distance_warning = simtower::original_facility_information(
      resources, distance_tower, part, 11, 0U);
  assert(distance_warning.advisory_lines[0] == simtower::original_strl_entry(
      resources.find("STRL", 711), 16U));
  distance_stair.x = 225U;
  distance_warning = simtower::original_facility_information(
      resources, distance_tower, part, 11, 0U);
  assert(distance_warning.advisory_lines[0] == simtower::original_strl_entry(
      resources.find("STRL", 711), 17U));

  // Direct 1108:05e3 coverage: only signed service states -1 and 3 qualify;
  // Restaurant uses the strict 1600..2200 interval and every other group-12
  // commercial type uses strict 240..2000 bounds.
  assert(!simtower::original_commercial_closed_advisory_required(
      6, 0, 1800U));
  assert(!simtower::original_commercial_closed_advisory_required(
      6, -1, 1600U));
  assert(simtower::original_commercial_closed_advisory_required(
      6, -1, 1601U));
  assert(simtower::original_commercial_closed_advisory_required(
      6, 3, 2199U));
  assert(!simtower::original_commercial_closed_advisory_required(
      6, 3, 2200U));
  assert(!simtower::original_commercial_closed_advisory_required(
      12, -1, 240U));
  assert(simtower::original_commercial_closed_advisory_required(
      12, -1, 241U));
  assert(simtower::original_commercial_closed_advisory_required(
      12, 3, 1999U));
  assert(!simtower::original_commercial_closed_advisory_required(
      12, 3, 2000U));
  assert(!simtower::original_commercial_closed_advisory_required(
      12, 3, 0x8000U));

  // SUB/CWD/XOR/SUB keeps the delta in a wrapping signed word. A serialized
  // x of 65530 is only 106 cells from 100 and -32768 remains negative after
  // absolute-value overflow, so it fails the signed 80-cell threshold.
  distance_stair.shape = 0U;
  distance_stair.x = 0xfffaU;
  distance_warning = simtower::original_facility_information(
      resources, distance_tower, part, 11, 0U);
  assert(distance_warning.advisory_lines[0] == simtower::original_strl_entry(
      resources.find("STRL", 711), 14U));
  distance_stair.x = 0x8064U;
  assert(simtower::original_facility_information(
             resources, distance_tower, part, 11, 0U)
             .advisory_line_count == 0U);

  {
    // Direct 1108:0a88 coverage. A state-two Parking tenant follows word 6
    // into the six-byte cf9c table, reads the linked dword person index, then
    // appends that person's signed floor/key owner text after STRL/711 item 34.
    auto parking_tower = simtower::make_original_new_tdt();
    parking_tower.header.rating = 2U;
    auto parking = make_tenant(11, 0U);
    parking.left = 20U;
    parking.right = 24U;
    parking.status = 2U;
    store_tenant_u16(parking, 0U, parking.left,
                     parking_tower.header.byte_swapped);
    store_tenant_u16(parking, 2U, parking.right,
                     parking_tower.header.byte_swapped);
    store_tenant_u16(parking, 6U, 0U,
                     parking_tower.header.byte_swapped);
    parking.exact_bytes[5] = std::byte{2};
    parking_tower.floors[9].tenants.push_back(parking);
    parking_tower.floors[9].tenant_index[0] = 0U;

    auto owner_office = make_tenant(7, 0U);
    owner_office.left = 40U;
    owner_office.right = 44U;
    store_tenant_u16(owner_office, 0U, owner_office.left,
                     parking_tower.header.byte_swapped);
    store_tenant_u16(owner_office, 2U, owner_office.right,
                     parking_tower.header.byte_swapped);
    parking_tower.floors[10].tenants.push_back(owner_office);
    parking_tower.floors[10].tenant_index[0] = 0U;

    parking_tower.people_count = 1U;
    parking_tower.people.resize(1U);
    parking_tower.people[0].exact_bytes[0] = std::byte{10};
    parking_tower.people[0].exact_bytes[1] = std::byte{0};
    auto parking_information = simtower::original_facility_information(
        resources, parking_tower, part, 9, 0U);
    const auto linked_text =
        simtower::original_strl_entry(resources.find("STRL", 711), 34U) +
        std::string{"Office, Floor 1"};
    assert(parking_information.advisory_line_count == 1U);
    assert(parking_information.advisory_lines[0] == linked_text);

    // Both location bytes and the tenant state compare as signed values.
    parking_tower.people[0].exact_bytes[1] = std::byte{0xff};
    assert(simtower::original_facility_information(
               resources, parking_tower, part, 9, 0U)
               .advisory_line_count == 0U);
    parking_tower.people[0].exact_bytes[1] = std::byte{0};
    parking_tower.floors[9].tenants[0].exact_bytes[5] = std::byte{0x80};
    assert(simtower::original_facility_information(
               resources, parking_tower, part, 9, 0U)
               .advisory_line_count == 0U);
  }

  // Direct 1108:0476/0893 and 1170:05f0 coverage: 1108:0000 calls its
  // advisory helpers in this precise order and caps the shared DTMP item-8
  // panel at three lines.
  facilities.header.rating = 3U;
  facilities.floors[10].tenants[0].exact_bytes[14] = std::byte{0};
  facility = simtower::original_facility_information(
      resources, facilities, part, 10, 0U);
  assert(facility.advisory_line_count == 3U);
  assert(facility.advisory_lines[0] == simtower::original_strl_entry(
      resources.find("STRL", 711), 27U));
  assert(facility.advisory_lines[1] == simtower::original_strl_entry(
      resources.find("STRL", 711), 1U));
  assert(facility.advisory_lines[2] == simtower::original_strl_entry(
      resources.find("STRL", 711), 18U));
  // 11e0:0049 and 1100:1760-176c establish the item-8 base offset; the
  // 1108:08e4 counter then advances each line by sixteen pixels.
  using AdvisoryOffset = simtower::OriginalFacilityAdvisoryTextOffset;
  assert(simtower::original_facility_advisory_text_offset(0U) ==
         (AdvisoryOffset{8, 18}));
  assert(simtower::original_facility_advisory_text_offset(1U) ==
         (AdvisoryOffset{8, 34}));
  assert(simtower::original_facility_advisory_text_offset(2U) ==
         (AdvisoryOffset{8, 50}));
  facilities.header.rating = 2U;

  // Direct 1138:0000/01b8 coverage: the Office family gate accepts the
  // normalized type-6 noise source. 1138:00a5/0128 test the current edge
  // before stepping to the
  // adjacent record. Consequently the first noisy neighbor beyond the
  // nominal 1138:0269 distance is still reported by 1108:0949.
  auto noise_tower = simtower::make_original_new_tdt();
  noise_tower.header.rating = 2U;
  auto noisy_food = make_tenant(6, 0U);
  noisy_food.left = 80U;
  noisy_food.right = 84U;
  store_tenant_u16(noisy_food, 0U, noisy_food.left,
                   noise_tower.header.byte_swapped);
  store_tenant_u16(noisy_food, 2U, noisy_food.right,
                   noise_tower.header.byte_swapped);
  auto noise_office = make_tenant(7, 1U);
  noise_office.left = 100U;
  noise_office.right = 104U;
  noise_office.exact_bytes[14] = std::byte{1};
  store_tenant_u16(noise_office, 0U, noise_office.left,
                   noise_tower.header.byte_swapped);
  store_tenant_u16(noise_office, 2U, noise_office.right,
                   noise_tower.header.byte_swapped);
  noise_tower.floors[10].tenants = {noisy_food, noise_office};
  noise_tower.floors[10].tenant_index[0] = 0U;
  noise_tower.floors[10].tenant_index[1] = 1U;
  const auto noise_information = simtower::original_facility_information(
      resources, noise_tower, part, 10, 1U);
  const auto expected_noise =
      simtower::original_strl_entry(resources.find("STRL", 710), 7U) +
      simtower::original_strl_entry(resources.find("STRL", 711), 35U) +
      simtower::original_strl_entry(resources.find("STRL", 711), 36U);
  assert(std::find(
             noise_information.advisory_lines.begin(),
             noise_information.advisory_lines.begin() +
                 noise_information.advisory_line_count,
             expected_noise) !=
         noise_information.advisory_lines.begin() +
             noise_information.advisory_line_count);

  // Direct 1100:37ef coverage: portraits are 8x24 below frame six and 16x24
  // thereafter. 1100:2852 draws the owner's live people into DTMP item 4;
  // skipped frames consume none, saved-name bitmap 702
  // is overridden by the periodic VIP bitmap 703.
  auto lineup = simtower::make_original_new_tdt();
  lineup.header.rating = 2U;
  auto lineup_office = make_tenant(7, 0U);
  lineup_office.left = 10U;
  lineup_office.right = 14U;
  lineup_office.exact_bytes[14] = std::byte{1};
  store_tenant_u16(lineup_office, 0U, lineup_office.left,
                   lineup.header.byte_swapped);
  store_tenant_u16(lineup_office, 2U, lineup_office.right,
                   lineup.header.byte_swapped);
  store_tenant_u32(lineup_office, 8U, 0U,
                   lineup.header.byte_swapped);
  lineup.floors[10].tenants.push_back(lineup_office);
  lineup.floors[10].tenant_index[0] = 0U;
  lineup.people_count = 6U;
  lineup.people.resize(6U);
  for (auto& record : lineup.people) {
    record.exact_bytes[4] = std::byte{7};
    record.exact_bytes[5] = std::byte{0};
    store_u16(record.exact_bytes, 2U, 0U,
              lineup.header.byte_swapped);
  }
  assert(simtower::set_original_person_name(
             lineup, 1U, "Named").status ==
         simtower::OriginalPersonNameStatus::added);
  assert(simtower::set_original_person_name(
             lineup, 2U, "Also named").status ==
         simtower::OriginalPersonNameStatus::added);
  lineup.post_elevator.b928 = 1U;
  lineup.post_elevator.b924 = 2;
  auto lineup_information = simtower::original_facility_information(
      resources, lineup, part, 10, 0U);
  const auto lineup_dtmp = simtower::parse_original_dtmp(
      resources.find("DTMP", 748));
  const auto& lineup_rect = lineup_dtmp.rectangles[3];
  assert(lineup_information.person_sprites.size() == 6U);
  for (std::size_t index = 0U; index < 6U; ++index) {
    const auto& sprite = lineup_information.person_sprites[index];
    assert(sprite.person_index == index);
    assert(sprite.frame == 1 && sprite.width == 8 && sprite.height == 24);
    assert(sprite.destination_x == lineup_rect.left + 7 +
                                       static_cast<int>(index) * 8);
    assert(sprite.destination_y == lineup_rect.top + 5);
  }
  assert(lineup_information.person_sprites[0].bitmap_id == 700U);
  assert(lineup_information.person_sprites[1].bitmap_id == 702U);
  assert(lineup_information.person_sprites[2].bitmap_id == 703U);

  auto hotel_lineup = lineup;
  auto& hotel = hotel_lineup.floors[10].tenants[0];
  hotel.type = 9;
  hotel.exact_bytes[4] = std::byte{9};
  hotel_lineup.people_count = 3U;
  hotel_lineup.people.resize(3U);
  for (auto& record : hotel_lineup.people) {
    record.exact_bytes[4] = std::byte{9};
    record.exact_bytes[5] = std::byte{0};
  }
  store_u16(hotel_lineup.people[0].exact_bytes, 2U, 3U,
            hotel_lineup.header.byte_swapped);
  store_u16(hotel_lineup.people[1].exact_bytes, 2U, 1U,
            hotel_lineup.header.byte_swapped);
  store_u16(hotel_lineup.people[2].exact_bytes, 2U, 2U,
            hotel_lineup.header.byte_swapped);
  hotel_lineup.header.person_link_count = 0U;
  hotel_lineup.post_elevator.b928 = 0U;
  auto hotel_information = simtower::original_facility_information(
      resources, hotel_lineup, part, 10, 0U);
  const auto hotel_dtmp = simtower::parse_original_dtmp(
      resources.find("DTMP", 752));
  const auto& hotel_rect = hotel_dtmp.rectangles[3];
  assert(hotel_information.person_sprites.size() == 2U);
  assert(hotel_information.person_sprites[0].person_index == 1U);
  assert(hotel_information.person_sprites[0].frame == 8);
  assert(hotel_information.person_sprites[0].width == 16);
  assert(hotel_information.person_sprites[0].destination_x ==
         hotel_rect.left + 7);
  assert(hotel_information.person_sprites[1].person_index == 2U);
  assert(hotel_information.person_sprites[1].frame == 3);
  assert(hotel_information.person_sprites[1].destination_x ==
         hotel_rect.left + 23);

  {
    // Direct 1100:2ec2 Movie lineup coverage. State-three occupants fill
    // DTMP item 4 and then continue at item 9; its signed dc24 byte-six gate
    // rejects both ordinary values below two and malformed high-bit values.
    auto movie_lineup = simtower::make_original_new_tdt();
    auto movie_owner = make_tenant(18, 0U);
    movie_owner.left = 30U;
    movie_owner.right = 61U;
    store_tenant_u16(movie_owner, 0U, movie_owner.left,
                     movie_lineup.header.byte_swapped);
    store_tenant_u16(movie_owner, 2U, movie_owner.right,
                     movie_lineup.header.byte_swapped);
    store_tenant_u16(movie_owner, 6U, 0U,
                     movie_lineup.header.byte_swapped);
    store_tenant_u32(movie_owner, 8U, 0U,
                     movie_lineup.header.byte_swapped);
    movie_lineup.floors[10].tenants.push_back(movie_owner);
    movie_lineup.floors[10].tenant_index[0] = 0U;
    auto& movie_link = movie_lineup.post_elevator.dc24_records[0];
    movie_link[0] = std::byte{10};
    movie_link[1] = std::byte{0xff};
    movie_link[2] = std::byte{0};
    movie_link[3] = std::byte{0xff};
    movie_link[6] = std::byte{2};
    movie_lineup.people_count = 56U;
    movie_lineup.people.resize(56U);
    for (auto& record : movie_lineup.people) {
      record.exact_bytes[4] = std::byte{18};
      record.exact_bytes[5] = std::byte{3};
      store_u16(record.exact_bytes, 2U, 0U,
                movie_lineup.header.byte_swapped);
    }

    auto movie_people = simtower::original_facility_information(
        resources, movie_lineup, part, 10, 0U);
    const auto movie_dtmp = simtower::parse_original_dtmp(
        resources.find("DTMP", 758));
    const auto& first_row = movie_dtmp.rectangles[3U];
    const auto& second_row = movie_dtmp.rectangles[8U];
    assert(movie_people.valid && movie_people.dialog_group == 10U);
    assert(!movie_people.person_sprites.empty());
    assert(movie_people.person_sprites.front().destination_x ==
           first_row.left + 2);
    assert(movie_people.person_sprites.front().destination_y ==
           first_row.top + 5);
    const auto overflow = std::find_if(
        movie_people.person_sprites.begin(), movie_people.person_sprites.end(),
        [&](const simtower::OriginalFacilityPersonSprite& sprite) {
          return sprite.destination_y == second_row.top + 5;
        });
    assert(overflow != movie_people.person_sprites.end());
    assert(overflow->destination_x == second_row.left + 2);

    movie_link[6] = std::byte{1};
    assert(simtower::original_facility_information(
               resources, movie_lineup, part, 10, 0U)
               .person_sprites.empty());
    movie_link[6] = std::byte{0x80};
    assert(simtower::original_facility_information(
               resources, movie_lineup, part, 10, 0U)
               .person_sprites.empty());
  }

  {
    // Direct 1100:307e Cathedral lineup coverage. 3124 uses signed JGE, so a
    // persisted 8000 anchor-state word must not open the multi-floor scan.
    auto cathedral_lineup = simtower::make_original_new_tdt();
    auto cathedral = make_tenant(36, 0U);
    cathedral.left = 30U;
    cathedral.right = 58U;
    store_tenant_u16(cathedral, 0U, cathedral.left,
                     cathedral_lineup.header.byte_swapped);
    store_tenant_u16(cathedral, 2U, cathedral.right,
                     cathedral_lineup.header.byte_swapped);
    store_tenant_u16(cathedral, 6U, 0x8000U,
                     cathedral_lineup.header.byte_swapped);
    store_tenant_u32(cathedral, 8U, 0U,
                     cathedral_lineup.header.byte_swapped);
    cathedral_lineup.floors[109].tenants.push_back(cathedral);
    cathedral_lineup.floors[109].tenant_index[0] = 0U;
    cathedral_lineup.header.exact_bytes[34] = std::byte{0};
    cathedral_lineup.header.exact_bytes[35] = std::byte{0};
    cathedral_lineup.people_count = 1U;
    cathedral_lineup.people.resize(1U);
    auto& person = cathedral_lineup.people[0].exact_bytes;
    person[4] = std::byte{36};
    person[5] = std::byte{3};
    store_u16(person, 2U, 0U, cathedral_lineup.header.byte_swapped);

    auto cathedral_people = simtower::original_facility_information(
        resources, cathedral_lineup, part, 109, 0U);
    assert(cathedral_people.valid && cathedral_people.dialog_group == 9U);
    assert(cathedral_people.person_sprites.empty());

    store_tenant_u16(cathedral_lineup.floors[109].tenants[0], 6U, 2U,
                     cathedral_lineup.header.byte_swapped);
    cathedral_people = simtower::original_facility_information(
        resources, cathedral_lineup, part, 109, 0U);
    assert(cathedral_people.person_sprites.size() == 1U);
    assert(cathedral_people.person_sprites[0].person_index == 0U);
  }

  // Direct 1188:050f/0575 tenant-name transaction coverage: derive the
  // floor/key link, append its 16-byte name, update in place, and
  // compact both lanes on removal.
  auto tenant_name = simtower::set_original_tenant_name(
      facilities, 10, 0U, "Acme");
  assert(tenant_name.status == simtower::OriginalTenantNameStatus::added);
  assert(tenant_name.changed && facilities.header.tenant_link_count == 1U);
  assert(simtower::original_tenant_name_slot(facilities, 10, 0U) == 0U);
  assert(simtower::original_tenant_saved_name(facilities, 10, 0U) ==
         "Acme");
  facility = simtower::original_facility_information(
      resources, facilities, part, 10, 0U);
  assert(facility.display_name == "Acme, Floor 1");

  {
    // Direct 1188:0aa0 Cathedral-name-link coverage. The clicked part's
    // byte-12 key deliberately differs from DS:b3ec: the link is
    // 109*94+b3ec, and changing only b3ec makes the prior slot unreachable.
    auto cathedral_names = simtower::make_original_new_tdt();
    cathedral_names.floors[113].tenants.push_back(make_tenant(36, 7U));
    cathedral_names.floors[113].tenant_index[7] = 0U;
    cathedral_names.header.exact_bytes[34] = std::byte{3};
    cathedral_names.header.exact_bytes[35] = std::byte{0};
    const auto named = simtower::set_original_tenant_name(
        cathedral_names, 113, 0U, "Spire");
    assert(named.status == simtower::OriginalTenantNameStatus::added);
    assert(simtower::original_tenant_name_slot(
               cathedral_names, 113, 0U) == 0U);
    assert(simtower::original_tenant_saved_name(
               cathedral_names, 113, 0U) == "Spire");
    cathedral_names.header.exact_bytes[34] = std::byte{4};
    assert(!simtower::original_tenant_name_slot(
        cathedral_names, 113, 0U));
    assert(simtower::original_tenant_saved_name(
               cathedral_names, 113, 0U).empty());
  }

  tenant_name = simtower::set_original_tenant_name(
      facilities, 10, 0U, "A");
  assert(tenant_name.status == simtower::OriginalTenantNameStatus::updated);
  assert(facilities.tenant_link_names[0].exact_bytes[2] == std::byte{'m'});
  assert(simtower::set_original_tenant_name(
             facilities, 10, 0U, "1234567890123456").status ==
         simtower::OriginalTenantNameStatus::too_long);
  tenant_name = simtower::remove_original_tenant_name(
      facilities, 10, 0U);
  assert(tenant_name.status == simtower::OriginalTenantNameStatus::removed);
  assert(facilities.header.tenant_link_count == 0U);

  assert(simtower::set_original_facility_rent_rate(
      facilities, part, 10, 0U, 3U));
  assert(facilities.floors[10].tenants[0].rent_rate == 3U);
  assert(facilities.floors[10].tenants[0].exact_bytes[16] == std::byte{3});
  assert(!simtower::set_original_facility_rent_rate(
      facilities, part, 10, 0U, 3U));

  // Direct 1100:2031 coverage: Retail uses 1100:1a5a's linked attendance
  // state and rent-adjusted PART thresholds; low/middle/high colors are
  // numbered in reverse here.
  part.words_00_to_40[25] = 25U;
  part.words_00_to_40[26] = 20U;
  part.words_00_to_40[29] = 30U;
  auto retail = make_tenant(10, 1U);
  retail.left = 20U;
  retail.right = 28U;
  retail.rent_rate = 0U;
  store_tenant_u16(retail, 0U, retail.left,
                   facilities.header.byte_swapped);
  store_tenant_u16(retail, 2U, retail.right,
                   facilities.header.byte_swapped);
  store_tenant_u16(retail, 6U, 2U,
                   facilities.header.byte_swapped);
  retail.exact_bytes[16] = std::byte{0};
  facilities.floors[10].tenants.push_back(retail);
  facilities.floors[10].tenant_index[1] = 1U;
  facilities.retail[2].exact_bytes[0] = std::byte{10};
  facilities.retail[2].exact_bytes[1] = std::byte{1};
  facilities.retail[2].exact_bytes[2] = std::byte{0};
  store_retail_u16(facilities.retail[2], 16U, 22U,
                   facilities.header.byte_swapped);
  facility = simtower::original_facility_information(
      resources, facilities, part, 10, 1U);
  assert(facility.dialog_id == 753U && facility.dialog_group == 5U);
  assert(facility.occupancy_text == "Occupied");
  assert(facility.commercial_meter.visible);
  assert(facility.commercial_meter.value == 22);
  assert(facility.commercial_meter.lower == 25);
  assert(facility.commercial_meter.upper == 30);
  assert(facility.commercial_meter.maximum == 30);
  assert(facility.commercial_meter.band == 2U);
  assert(facility.commercial_value_text == "22");
  assert((facility.preview == simtower::OriginalFacilityPreview{
      160, 3936, 96, 24}));
  {
    // Direct 1100:2715 coverage: Fast Food formats Retail byte 10 through
    // signed CBW before appending STRL/713 item seven.
    auto fast_food_tower = facilities;
    auto fast_food = make_tenant(12, 2U);
    fast_food.left = 30U;
    fast_food.right = 46U;
    store_tenant_u16(fast_food, 0U, fast_food.left,
                     fast_food_tower.header.byte_swapped);
    store_tenant_u16(fast_food, 2U, fast_food.right,
                     fast_food_tower.header.byte_swapped);
    store_tenant_u16(fast_food, 6U, 3U,
                     fast_food_tower.header.byte_swapped);
    fast_food_tower.floors[10].tenants.push_back(fast_food);
    fast_food_tower.floors[10].tenant_index[2] = 2U;
    fast_food_tower.retail[3].exact_bytes[0] = std::byte{10};
    fast_food_tower.retail[3].exact_bytes[1] = std::byte{2};
    fast_food_tower.retail[3].exact_bytes[10] = std::byte{0x80};
    const auto fast_food_information = simtower::original_facility_information(
        resources, fast_food_tower, part, 10, 2U);
    assert(fast_food_information.yesterday_profit_text ==
           "-128" + simtower::original_strl_entry(
                        resources.find("STRL", 713), 7U));
  }
  // Direct 1108:030d threshold coverage: rent tier zero adds five to raw
  // PART/1000 word 26, so metric 22 is below adjusted 25 and selects item 11.
  const auto expected_commercial_advisory =
      simtower::original_strl_entry(resources.find("STRL", 711), 11U);
  assert(std::find(
             facility.advisory_lines.begin(),
             facility.advisory_lines.begin() + facility.advisory_line_count,
             expected_commercial_advisory) !=
         facility.advisory_lines.begin() + facility.advisory_line_count);

  {
    // Direct 1108:0565/05a4 follow-up coverage. A high Restaurant metric on
    // calendar phase one appends item 22 after item 8; a low metric in a
    // version-20 game appends item 23 after item 11.
    auto followup_tower = simtower::make_original_new_tdt();
    followup_tower.header.rating = 2U;
    auto restaurant = make_tenant(6, 0U);
    restaurant.left = 20U;
    restaurant.right = 44U;
    store_tenant_u16(restaurant, 0U, restaurant.left,
                     followup_tower.header.byte_swapped);
    store_tenant_u16(restaurant, 2U, restaurant.right,
                     followup_tower.header.byte_swapped);
    store_tenant_u16(restaurant, 6U, 0U,
                     followup_tower.header.byte_swapped);
    followup_tower.floors[10].tenants.push_back(restaurant);
    followup_tower.floors[10].tenant_index[0] = 0U;

    simtower::OriginalPartTable followup_part{};
    followup_tower.header.current_day = 2;
    auto followup = simtower::original_facility_information(
        resources, followup_tower, followup_part, 10, 0U);
    assert(followup.advisory_line_count == 3U);
    assert(followup.advisory_lines[1] ==
           simtower::original_strl_entry(resources.find("STRL", 711), 8U));
    assert(followup.advisory_lines[2] ==
           simtower::original_strl_entry(resources.find("STRL", 711), 22U));

    followup_tower.header.current_day = 0;
    followup_tower.header.version_20_word = 1U;
    followup_part.words_00_to_40[20U] = 1U;
    followup = simtower::original_facility_information(
        resources, followup_tower, followup_part, 10, 0U);
    assert(followup.advisory_line_count == 3U);
    assert(followup.advisory_lines[1] ==
           simtower::original_strl_entry(resources.find("STRL", 711), 11U));
    assert(followup.advisory_lines[2] ==
           simtower::original_strl_entry(resources.find("STRL", 711), 23U));
  }

  // Direct 1218:08cd coverage through 1100:2c23: scan the global people array
  // for the Retail record's live visitors, accepting both signed direct
  // byte-6 links and service-family people whose owner links to that record.
  facilities.people_count = 2U;
  facilities.people.resize(2U);
  facilities.people[0].exact_bytes[4] = std::byte{7};
  facilities.people[0].exact_bytes[5] = std::byte{0x22};
  facilities.people[0].exact_bytes[6] = std::byte{2};
  store_u16(facilities.people[0].exact_bytes, 2U, 0U,
            facilities.header.byte_swapped);
  facilities.people[1].exact_bytes[0] = std::byte{10};
  facilities.people[1].exact_bytes[1] = std::byte{1};
  facilities.people[1].exact_bytes[4] = std::byte{10};
  facilities.people[1].exact_bytes[5] = std::byte{5};
  store_u16(facilities.people[1].exact_bytes, 2U, 0U,
            facilities.header.byte_swapped);
  facilities.retail[2].exact_bytes[9] = std::byte{2};
  facility = simtower::original_facility_information(
      resources, facilities, part, 10, 1U);
  const auto retail_dtmp = simtower::parse_original_dtmp(
      resources.find("DTMP", 753));
  const auto& retail_rect = retail_dtmp.rectangles[3];
  assert(facility.person_sprites.size() == 2U);
  assert(facility.person_sprites[0].person_index == 0U);
  assert(facility.person_sprites[0].destination_x == retail_rect.left + 2);
  assert(facility.person_sprites[1].person_index == 1U);
  assert(facility.person_sprites[1].destination_x == retail_rect.left + 10);

  // 08cd CBWs the direct byte-6 selector. Raw 82 is therefore -126 and must
  // not match requested record 130; the type-10 owner-link branch still uses
  // its full 16-bit tenant word and does match that same record.
  auto high_retail = facilities;
  store_tenant_u16(high_retail.floors[10].tenants[1], 6U, 130U,
                   high_retail.header.byte_swapped);
  high_retail.retail[130] = high_retail.retail[2];
  high_retail.retail[130].exact_bytes[9] = std::byte{1};
  high_retail.people[0].exact_bytes[6] = std::byte{0x82};
  const auto high_retail_information = simtower::original_facility_information(
      resources, high_retail, part, 10, 1U);
  assert(high_retail_information.person_sprites.size() == 1U);
  assert(high_retail_information.person_sprites[0].person_index == 1U);

  // 1100:2d3e calls the distinct 1218:0a89 Medical scan. A Retail-state
  // Office lookalike is rejected; only Office/Condo state-0x23 records with
  // the matching byte-6 dbfc link fill the requested lineup count.
  auto medical_tower = facilities;
  auto medical = make_tenant(13, 0U);
  medical.left = 40U;
  medical.right = 56U;
  store_tenant_u16(medical, 0U, medical.left,
                   medical_tower.header.byte_swapped);
  store_tenant_u16(medical, 2U, medical.right,
                   medical_tower.header.byte_swapped);
  store_tenant_u16(medical, 6U, 3U,
                   medical_tower.header.byte_swapped);
  medical_tower.floors[20].tenants.push_back(medical);
  medical_tower.floors[20].tenant_index[0] = 0U;
  medical_tower.post_elevator.dbfc_dwords[3] =
      20U | (2U << 16U);
  medical_tower.people_count = 4U;
  medical_tower.people.resize(4U);
  medical_tower.people[0].exact_bytes[4] = std::byte{7};
  medical_tower.people[0].exact_bytes[5] = std::byte{0x22};
  medical_tower.people[0].exact_bytes[6] = std::byte{3};
  medical_tower.people[1].exact_bytes[4] = std::byte{7};
  medical_tower.people[1].exact_bytes[5] = std::byte{0x23};
  medical_tower.people[1].exact_bytes[6] = std::byte{3};
  medical_tower.people[2].exact_bytes[4] = std::byte{9};
  medical_tower.people[2].exact_bytes[5] = std::byte{0x23};
  medical_tower.people[2].exact_bytes[6] = std::byte{3};
  medical_tower.people[3].exact_bytes[4] = std::byte{7};
  medical_tower.people[3].exact_bytes[5] = std::byte{0x23};
  medical_tower.people[3].exact_bytes[6] = std::byte{4};
  const auto medical_information = simtower::original_facility_information(
      resources, medical_tower, part, 20, 0U);
  assert(medical_information.person_sprites.size() == 2U);
  assert(medical_information.person_sprites[0].person_index == 1U);
  assert(medical_information.person_sprites[1].person_index == 2U);

  // Direct 1180:0de9 coverage: Movie Information and New Movie operate on
  // translated dc24/PART/TDT state and mark both source tenants dirty.
  auto movie = make_tenant(18, 2U);
  movie.left = 30U;
  movie.right = 61U;
  store_tenant_u16(movie, 0U, movie.left,
                   facilities.header.byte_swapped);
  store_tenant_u16(movie, 2U, movie.right,
                   facilities.header.byte_swapped);
  store_tenant_u16(movie, 6U, 0U,
                   facilities.header.byte_swapped);
  facilities.floors[10].tenants.push_back(movie);
  facilities.floors[10].tenants.push_back(make_tenant(19, 3U));
  facilities.floors[10].tenant_index[2] = 2U;
  facilities.floors[10].tenant_index[3] = 3U;
  auto& movie_record = facilities.post_elevator.dc24_records[0];
  movie_record[0] = std::byte{10};
  movie_record[1] = std::byte{10};
  movie_record[2] = std::byte{2};
  movie_record[3] = std::byte{2};
  movie_record[7] = std::byte{6};
  movie_record[9] = std::byte{3};
  movie_record[11] = std::byte{90};
  part.words_52_to_ac[16] = 40U;
  part.words_52_to_ac[17] = 80U;
  part.words_52_to_ac[18] = 100U;
  part.words_52_to_ac[19] = 0U;
  part.words_52_to_ac[20] = 20U;
  part.words_52_to_ac[21] = 100U;
  part.words_52_to_ac[22] = 150U;
  part.words_52_to_ac[34] = 3000U;
  part.words_52_to_ac[35] = 1500U;
  // Direct 1100:25d9 coverage: CBW makes the age signed before IDIV, the
  // quotient advances in three-quarter bands, and signed age twelve switches
  // to the localized long-running label.
  std::array<std::byte, 0x0c> movie_length{};
  movie_length[9] = std::byte{0};
  assert(simtower::original_movie_length_text(resources, movie_length) == "1Q");
  movie_length[9] = std::byte{3};
  assert(simtower::original_movie_length_text(resources, movie_length) == "2Q");
  movie_length[9] = std::byte{11};
  assert(simtower::original_movie_length_text(resources, movie_length) == "4Q");
  movie_length[9] = std::byte{12};
  assert(simtower::original_movie_length_text(resources, movie_length) ==
         simtower::original_strl_entry(resources.find("STRL", 713), 6U));
  movie_length[9] = std::byte{0xff};
  assert(simtower::original_movie_length_text(resources, movie_length) == "1Q");

  // Direct 1100:268e -> 1180:0bcb coverage: byte eleven is signed and each
  // threshold is an inclusive handoff to the next signed PART return value.
  simtower::OriginalPartTable income_part{};
  income_part.words_52_to_ac[16] = 10U;
  income_part.words_52_to_ac[17] = 20U;
  income_part.words_52_to_ac[18] = 30U;
  income_part.words_52_to_ac[19] = 0xfff9U;  // -7
  income_part.words_52_to_ac[20] = 11U;
  income_part.words_52_to_ac[21] = 22U;
  income_part.words_52_to_ac[22] = 0x8000U;  // -32768
  std::array<std::byte, 0x0c> movie_income{};
  const auto income_for = [&](std::uint8_t attendance) {
    movie_income[11] = static_cast<std::byte>(attendance);
    return simtower::original_movie_information_income(
        movie_income, income_part);
  };
  assert(income_for(0xffU) == -7);
  assert(income_for(9U) == -7);
  assert(income_for(10U) == 11);
  assert(income_for(19U) == 11);
  assert(income_for(20U) == 22);
  assert(income_for(29U) == 22);
  assert(income_for(30U) == -32768);
  assert(income_for(0x80U) == -7);

  // Direct 1108:0285 coverage: after 1108:014b's no-transport item one,
  // empty programs use item seven; each recovered capacity class selects item
  // four, five, or six, while other signed PART values append nothing.
  auto movie_advisory_tower = simtower::make_original_new_tdt();
  movie_advisory_tower.header.rating = 2U;
  auto movie_advisory_tenant = make_tenant(18, 0U);
  movie_advisory_tenant.left = 30U;
  movie_advisory_tenant.right = 61U;
  store_tenant_u16(movie_advisory_tenant, 0U, movie_advisory_tenant.left,
                   movie_advisory_tower.header.byte_swapped);
  store_tenant_u16(movie_advisory_tenant, 2U, movie_advisory_tenant.right,
                   movie_advisory_tower.header.byte_swapped);
  store_tenant_u16(movie_advisory_tenant, 6U, 0U,
                   movie_advisory_tower.header.byte_swapped);
  movie_advisory_tower.floors[10].tenants.push_back(movie_advisory_tenant);
  movie_advisory_tower.floors[10].tenant_index[0] = 0U;
  auto& movie_advisory_record =
      movie_advisory_tower.post_elevator.dc24_records[0];
  movie_advisory_record[7] = std::byte{6};
  const auto saved_movie_advisory_capacity = part.words_52_to_ac[28U];
  movie_advisory_record[9] = std::byte{0};
  auto movie_advisory = simtower::original_facility_information(
      resources, movie_advisory_tower, part, 10, 0U);
  assert(movie_advisory.advisory_line_count == 2U);
  assert(movie_advisory.advisory_lines[1] ==
         simtower::original_strl_entry(resources.find("STRL", 711), 7U));
  movie_advisory_record[9] = std::byte{3};
  for (const auto [capacity, entry] :
       std::array<std::pair<std::uint16_t, std::uint16_t>, 3>{
           std::pair{60U, 4U}, std::pair{40U, 5U}, std::pair{20U, 6U}}) {
    part.words_52_to_ac[28U] = capacity;
    movie_advisory = simtower::original_facility_information(
        resources, movie_advisory_tower, part, 10, 0U);
    assert(movie_advisory.advisory_line_count == 2U);
    assert(movie_advisory.advisory_lines[1] ==
           simtower::original_strl_entry(resources.find("STRL", 711), entry));
  }
  part.words_52_to_ac[28U] = 99U;
  assert(simtower::original_facility_information(
             resources, movie_advisory_tower, part, 10, 0U)
             .advisory_line_count == 1U);
  part.words_52_to_ac[28U] = saved_movie_advisory_capacity;

  // Direct 1108:070d coverage: a Party Hall follows tenant word six into
  // dc24 and appends item 28 only when the linked signed program state is at
  // least two.
  auto party_tower = simtower::make_original_new_tdt();
  party_tower.header.rating = 2U;
  auto party = make_tenant(29, 0U);
  party.left = 30U;
  party.right = 54U;
  store_tenant_u16(party, 0U, party.left,
                   party_tower.header.byte_swapped);
  store_tenant_u16(party, 2U, party.right,
                   party_tower.header.byte_swapped);
  store_tenant_u16(party, 6U, 0U,
                   party_tower.header.byte_swapped);
  party_tower.floors[10].tenants.push_back(party);
  party_tower.floors[10].tenant_index[0] = 0U;
  party_tower.post_elevator.dc24_records[0][6] = std::byte{2};
  auto party_information = simtower::original_facility_information(
      resources, party_tower, part, 10, 0U);
  assert(party_information.advisory_line_count == 2U);
  assert(party_information.advisory_lines[1] ==
         simtower::original_strl_entry(resources.find("STRL", 711), 28U));
  party_tower.post_elevator.dc24_records[0][6] = std::byte{1};
  assert(simtower::original_facility_information(
             resources, party_tower, part, 10, 0U)
             .advisory_line_count == 1U);

  // Direct 1108:0698/0789 coverage: late Hotel item 26 precedes its common
  // no-transport item. Security dialog group eight skips transport advice and
  // emits only state-five item 29 in the recovered helper order.
  auto state_advisory_tower = simtower::make_original_new_tdt();
  state_advisory_tower.header.rating = 2U;
  auto late_hotel = make_tenant(3, 0U);
  late_hotel.left = 20U;
  late_hotel.right = 26U;
  late_hotel.status = 0x38U;
  late_hotel.exact_bytes[5] = std::byte{0x38};
  store_tenant_u16(late_hotel, 0U, late_hotel.left,
                   state_advisory_tower.header.byte_swapped);
  store_tenant_u16(late_hotel, 2U, late_hotel.right,
                   state_advisory_tower.header.byte_swapped);
  state_advisory_tower.floors[10].tenants.push_back(late_hotel);
  state_advisory_tower.floors[10].tenant_index[0] = 0U;
  const auto hotel_advisory = simtower::original_facility_information(
      resources, state_advisory_tower, part, 10, 0U);
  assert(hotel_advisory.advisory_line_count == 2U);
  assert(hotel_advisory.advisory_lines[0] ==
         simtower::original_strl_entry(resources.find("STRL", 711), 26U));

  state_advisory_tower.floors[10].tenants.clear();
  auto security = make_tenant(20, 0U);
  security.left = 20U;
  security.right = 40U;
  security.status = 5U;
  security.exact_bytes[5] = std::byte{5};
  store_tenant_u16(security, 0U, security.left,
                   state_advisory_tower.header.byte_swapped);
  store_tenant_u16(security, 2U, security.right,
                   state_advisory_tower.header.byte_swapped);
  state_advisory_tower.floors[10].tenants.push_back(security);
  const auto security_advisory = simtower::original_facility_information(
      resources, state_advisory_tower, part, 10, 0U);
  assert(security_advisory.advisory_line_count == 1U);
  assert(security_advisory.advisory_lines[0] ==
         simtower::original_strl_entry(resources.find("STRL", 711), 29U));

  // Direct 1108:04c2 coverage: a state-one Parking record reports the exact
  // waiting-for-car advisory (STRL/711 item 20).
  state_advisory_tower.floors[10].tenants.clear();
  auto waiting_parking = make_tenant(11, 0U);
  waiting_parking.left = 20U;
  waiting_parking.right = 24U;
  waiting_parking.status = 1U;
  waiting_parking.exact_bytes[5] = std::byte{1};
  store_tenant_u16(waiting_parking, 0U, waiting_parking.left,
                   state_advisory_tower.header.byte_swapped);
  store_tenant_u16(waiting_parking, 2U, waiting_parking.right,
                   state_advisory_tower.header.byte_swapped);
  state_advisory_tower.floors[10].tenants.push_back(waiting_parking);
  state_advisory_tower.floors[10].tenant_index[0] = 0U;
  const auto parking_advisory = simtower::original_facility_information(
      resources, state_advisory_tower, part, 10, 0U);
  assert(parking_advisory.advisory_line_count == 1U);
  assert(parking_advisory.advisory_lines[0] ==
         simtower::original_strl_entry(resources.find("STRL", 711), 20U));

  // Direct 1108:0520 coverage: floor 15 lies just outside the first accepted
  // ten-floor Lobby route band, so a group-12 Restaurant appends item 21
  // after its transport and commercial advisories.
  auto route_band_tower = simtower::make_original_new_tdt();
  route_band_tower.header.rating = 2U;
  route_band_tower.header.version_20_word = 0U;
  auto route_band_restaurant = make_tenant(6, 0U);
  route_band_restaurant.left = 20U;
  route_band_restaurant.right = 44U;
  store_tenant_u16(route_band_restaurant, 0U, route_band_restaurant.left,
                   route_band_tower.header.byte_swapped);
  store_tenant_u16(route_band_restaurant, 2U, route_band_restaurant.right,
                   route_band_tower.header.byte_swapped);
  store_tenant_u16(route_band_restaurant, 6U, 0U,
                   route_band_tower.header.byte_swapped);
  route_band_tower.floors[15].tenants.push_back(route_band_restaurant);
  route_band_tower.floors[15].tenant_index[0] = 0U;
  const auto route_band_advisory = simtower::original_facility_information(
      resources, route_band_tower, part, 15, 0U);
  assert(route_band_advisory.advisory_line_count == 3U);
  assert(route_band_advisory.advisory_lines[2] ==
         simtower::original_strl_entry(resources.find("STRL", 711), 21U));

  facility = simtower::original_facility_information(
      resources, facilities, part, 10, 2U);
  assert(facility.dialog_id == 758U && facility.dialog_group == 10U);
  assert(!facility.movie_title.empty());
  assert(facility.movie_length_text == "2Q");
  assert(facility.movie_income_text == "10000");
  assert((facility.preview == simtower::OriginalFacilityPreview{
      240, 3936, 248, 60}));

  facilities.header.balance = 10000;
  facilities.header.construction_costs = 0;
  auto movie_choice = simtower::choose_original_movie(
      facilities, part, 0U, simtower::OriginalMovieChoice::new_release);
  assert(movie_choice.handled && movie_choice.changed &&
         movie_choice.affordable && movie_choice.cost == 3000);
  assert(movie_record[7] == std::byte{7} && movie_record[9] == std::byte{0});
  assert(facilities.header.balance == 7000);
  assert(facilities.header.construction_costs == -3000);
  assert(facilities.floors[10].tenants[2].exact_bytes[13] == std::byte{1});
  assert(facilities.floors[10].tenants[3].exact_bytes[13] == std::byte{1});
  facilities.header.balance = 1499;
  movie_choice = simtower::choose_original_movie(
      facilities, part, 0U, simtower::OriginalMovieChoice::classic);
  assert(movie_choice.handled && !movie_choice.changed &&
         !movie_choice.affordable && movie_choice.cost == 1500);

  // Exact activity strings from 1110:0000's state table.
  auto activity = make_person_tower();
  auto& person = activity.people[0].exact_bytes;
  person[4] = std::byte{9};
  assert(simtower::original_person_information(
             resources, activity, part, 0U).activity_text ==
         "Lobby to leave");
  person[5] = std::byte{0x41};
  person[6] = std::byte{0xff};
  assert(simtower::original_person_information(
             resources, activity, part, 0U).activity_text ==
         "Lobby to eat");
  person[4] = std::byte{7};
  person[5] = std::byte{0x45};
  assert(simtower::original_person_information(
             resources, activity, part, 0U).activity_text ==
         "Lobby to go home");

  activity.floors[12].tenants.push_back(make_tenant(6, 1U));
  activity.floors[12].tenant_index[1] = 0U;
  store_tenant_u16(activity.floors[12].tenants[0], 6U, 3U,
                   activity.header.byte_swapped);
  activity.retail[3].exact_bytes[0] = std::byte{12};
  activity.retail[3].exact_bytes[1] = std::byte{1};
  activity.retail[3].exact_bytes[11] = std::byte{1};
  person[5] = std::byte{0x41};
  person[6] = std::byte{3};
  assert(simtower::original_person_information(
             resources, activity, part, 0U).activity_text ==
         "French Restaurant, Floor 3");

  person[4] = std::byte{15};
  person[5] = std::byte{3};
  person[6] = std::byte{9};
  activity.floors[10].tenants[0].type = 15;
  activity.floors[10].tenants[0].exact_bytes[4] = std::byte{15};
  information = simtower::original_person_information(
      resources, activity, part, 0U);
  assert(information.dialog_id == 764U);
  assert(information.activity_text == ", Floor B1");
  assert(!information.stress.visible);

  // Portrait resources are three independently embedded 96x24 DIBs. This
  // memory-only test directly covers 1100:364a/37a9/37d1's precise
  // 8x24 -> 16x47 WinGStretchBlt path used by a one-cell portrait; it creates
  // no top-level window.
  for (const int id : {700, 702, 703}) {
    const auto dib = simtower::original_dib_view(
        resources.find("BITMAP", id));
    assert(dib.width == 96 && dib.height == 24 && dib.bit_count == 8U);
  }
  TestDibSurface portrait(16, 47);
  std::fill_n(portrait.pixels, 16U * 47U, 0x00abcdefU);
  simtower::draw_original_dib_region_scaled(
      portrait.dc, resources.find("BITMAP", 700),
      0, 0, 16, 47, 8, 0, 8, 24);
  assert(std::any_of(portrait.pixels, portrait.pixels + 16U * 47U,
                     [](std::uint32_t pixel) {
                       return pixel != 0x00abcdefU;
                     }));
  return 0;
}
