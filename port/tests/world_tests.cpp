#include "original_construction.hpp"
#include "original_dib.hpp"
#include "original_map.hpp"
#include "original_resources.hpp"
#include "original_tdt.hpp"
#include "original_world.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
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

std::uint8_t dib_palette_index(std::span<const std::byte> resource,
                               int x,
                               int y) {
  const auto dib = simtower::original_dib_view(resource);
  assert(dib.bit_count == 8U);
  const int height = std::abs(dib.height);
  assert(x >= 0 && x < dib.width && y >= 0 && y < height);
  const std::size_t row_bytes =
      (static_cast<std::size_t>(dib.width) + 3U) & ~3U;
  const int source_y = dib.height > 0 ? height - 1 - y : y;
  return std::to_integer<std::uint8_t>(
      dib.pixels[static_cast<std::size_t>(source_y) * row_bytes + x]);
}

std::uint32_t clut_pixel(std::span<const std::byte> clut,
                         std::uint8_t index) {
  if (index == 0xffU) {
    return 0U;
  }
  // 1020:0e29 skips CLUT source record 184 while constructing destination
  // palette entries 0..254, then explicitly clears destination entry 255.
  const std::size_t source_index =
      static_cast<std::size_t>(index) + (index >= 184U ? 1U : 0U);
  const std::size_t offset = source_index * 8U;
  assert(offset + 6U < clut.size());
  return (static_cast<std::uint32_t>(
              std::to_integer<std::uint8_t>(clut[offset + 2U]))
          << 16U) |
         (static_cast<std::uint32_t>(
              std::to_integer<std::uint8_t>(clut[offset + 4U]))
          << 8U) |
         static_cast<std::uint32_t>(
             std::to_integer<std::uint8_t>(clut[offset + 6U]));
}

std::uint32_t indexed_palette_pixel(
    std::span<const std::byte> resource,
    const simtower::OriginalWorldPalette& palette,
    int x,
    int y) {
  return palette[dib_palette_index(resource, x, y)];
}

std::uint32_t interpolate_color(std::uint32_t first,
                                std::uint32_t second,
                                std::uint16_t frame,
                                std::uint16_t first_frame,
                                std::uint16_t last_frame) {
  std::uint32_t result = 0U;
  for (const int shift : {16, 8, 0}) {
    const auto a = static_cast<std::uint8_t>(first >> shift);
    const auto b = static_cast<std::uint8_t>(second >> shift);
    const std::uint32_t duration = last_frame - first_frame;
    const std::uint8_t channel = b > a
        ? static_cast<std::uint8_t>(
              (static_cast<std::uint32_t>(frame - first_frame) * (b - a)) /
                  duration +
              a)
        : static_cast<std::uint8_t>(
              (static_cast<std::uint32_t>(last_frame - frame) * (a - b)) /
                  duration +
              b);
    result |= static_cast<std::uint32_t>(channel) << shift;
  }
  return result;
}

void assert_tile(const simtower::OriginalWorldRaster& raster,
                 int destination_x,
                 int destination_y,
                 std::span<const std::byte> cgpk,
                 std::size_t tile,
                 std::span<const std::byte> clut) {
  constexpr std::size_t kTileBytes = 8U * 36U;
  assert((tile + 1U) * kTileBytes <= cgpk.size());
  for (int y = 0; y < 36; ++y) {
    for (int x = 0; x < 8; ++x) {
      const auto palette_index = std::to_integer<std::uint8_t>(
          cgpk[tile * kTileBytes + static_cast<std::size_t>(y * 8 + x)]);
      assert(raster.at(destination_x + x, destination_y + y) ==
             clut_pixel(clut, palette_index));
    }
  }
}

struct FacilityGraphicsTestCase {
  int type;
  int width_cells;
  int first_resource;
  int last_resource;
  int frame_count;
  int graphic_height{24};
};

std::pair<int, int> facility_frame_source(
    const simtower::OriginalResources& resources,
    const FacilityGraphicsTestCase& test,
    int frame) {
  const int frame_width = test.width_cells * 8;
  for (int resource_id = test.first_resource;
       resource_id <= test.last_resource; ++resource_id) {
    const auto view = simtower::original_dib_view(
        resources.find("BITMAP", resource_id));
    assert(view.width % frame_width == 0);
    const int frames_in_resource = view.width / frame_width;
    if (frame < frames_in_resource) {
      return {resource_id, frame * frame_width};
    }
    frame -= frames_in_resource;
  }
  assert(false);
  return {-1, -1};
}

std::uint32_t horizontal_atlas_pixel(
    const simtower::OriginalResources& resources,
    const simtower::OriginalWorldPalette& palette,
    int first_resource,
    int last_resource,
    int x,
    int y) {
  assert(x >= 0);
  for (int resource_id = first_resource;
       resource_id <= last_resource; ++resource_id) {
    const auto resource = resources.find("BITMAP", resource_id);
    const auto view = simtower::original_dib_view(resource);
    if (x < view.width) {
      return indexed_palette_pixel(resource, palette, x, y);
    }
    x -= view.width;
  }
  assert(false);
  return 0;
}

std::uint8_t people_atlas_index(
    const simtower::OriginalResources& resources,
    int source_cell,
    int x,
    int y) {
  assert(source_cell >= 0 && x >= 0 && x < 8);
  int source_x = source_cell * 8 + x;
  for (const int resource_id : {1128, 1129}) {
    const auto resource = resources.find("BITMAP", resource_id);
    const auto dib = simtower::original_dib_view(resource);
    if (source_x < dib.width) {
      return dib_palette_index(resource, source_x, y);
    }
    source_x -= dib.width;
  }
  assert(false);
  return 0U;
}

std::uint8_t facility_people_atlas_index(
    const simtower::OriginalResources& resources,
    int source_x,
    int y) {
  assert(source_x >= 0 && y >= 0 && y < 24);
  for (const int resource_id : {1512, 1513, 1514, 1515, 1516, 1517, 1518}) {
    const auto resource = resources.find("BITMAP", resource_id);
    const auto dib = simtower::original_dib_view(resource);
    if (source_x < dib.width) {
      return dib_palette_index(resource, source_x, y);
    }
    source_x -= dib.width;
  }
  assert(false);
  return 0U;
}

void store_dword(std::span<std::byte> bytes,
                 std::size_t offset,
                 std::uint32_t value) {
  assert(offset + 4U <= bytes.size());
  bytes[offset] = static_cast<std::byte>(value);
  bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
  bytes[offset + 2U] = static_cast<std::byte>(value >> 16U);
  bytes[offset + 3U] = static_cast<std::byte>(value >> 24U);
}

void store_word(std::span<std::byte> bytes,
                std::size_t offset,
                std::uint16_t value) {
  assert(offset + 2U <= bytes.size());
  bytes[offset] = static_cast<std::byte>(value);
  bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

simtower::OriginalTdtElevator& test_elevator(
    simtower::OriginalTdtDocument& tower,
    std::uint16_t x) {
  const auto found = std::find_if(
      tower.elevators.begin(), tower.elevators.end(),
      [&](const simtower::OriginalTdtElevator& elevator) {
        return elevator.used != 0U && elevator.x == x;
      });
  assert(found != tower.elevators.end());
  return *found;
}

simtower::OriginalTdtElevatorFloorRecord& test_elevator_floor_record(
    simtower::OriginalTdtElevator& elevator,
    int floor) {
  const auto mapped = simtower::original_elevator_floor_record_index(
      elevator.type, elevator.bottom_floor, elevator.top_floor,
      static_cast<std::int16_t>(floor));
  assert(mapped >= 0);
  const auto found = std::find_if(
      elevator.floor_records.begin(), elevator.floor_records.end(),
      [&](const simtower::OriginalTdtElevatorFloorRecord& record) {
        return record.mapped_index == mapped;
      });
  assert(found != elevator.floor_records.end());
  return *found;
}

simtower::OriginalYenTable construction_costs() {
  simtower::OriginalYenTable costs{};
  costs[0] = 5U;
  costs[1] = 2000U;
  costs[42] = 6000U;
  costs[43] = 4000U;
  costs[3] = 200U;
  costs[7] = 400U;
  costs[0x18] = 50U;
  return costs;
}

void store_test_header_word(simtower::OriginalTdtDocument& document,
                            std::size_t offset,
                            std::uint16_t value) {
  assert(document.header.format_version >= 0x23U);
  assert(offset + 2U <= document.header.exact_bytes.size());
  document.header.exact_bytes[offset] = static_cast<std::byte>(value);
  document.header.exact_bytes[offset + 1U] =
      static_cast<std::byte>(value >> 8U);
}

simtower::OriginalTdtDocument make_facility_people_tower(
    std::int8_t type,
    std::uint16_t width,
    std::size_t people_count,
    std::int16_t variant_word = 0) {
  auto tower = simtower::make_original_new_tdt();
  auto& floor = tower.floors[11];
  floor.tenants.clear();
  floor.tenant_index.fill(0U);
  simtower::OriginalTdtTenant tenant{};
  tenant.left = 160U;
  tenant.right = static_cast<std::uint16_t>(tenant.left + width);
  tenant.type = type;
  tenant.status = 0U;
  tenant.variant = static_cast<std::uint8_t>(variant_word);
  store_word(tenant.exact_bytes, 0U, tenant.left);
  store_word(tenant.exact_bytes, 2U, tenant.right);
  tenant.exact_bytes[4] = static_cast<std::byte>(type);
  tenant.exact_bytes[5] = std::byte{0};
  store_word(tenant.exact_bytes, 6U,
             std::bit_cast<std::uint16_t>(variant_word));
  store_dword(tenant.exact_bytes, 8U, 0U);
  tenant.exact_bytes[12] = std::byte{0};
  tenant.exact_bytes[13] = std::byte{1};
  floor.tenants.push_back(tenant);
  tower.people.assign(people_count, simtower::OriginalTdtPersonRecord{});
  tower.people_count = static_cast<std::uint32_t>(people_count);
  for (auto& person : tower.people) {
    person.exact_bytes[0] = std::byte{11};
    person.exact_bytes[1] = std::byte{0};
    person.exact_bytes[5] = std::byte{0};
    person.exact_bytes[7] = std::byte{0xff};
    person.exact_bytes[8] = std::byte{0xff};
  }
  tower.random_state = 0U;
  return tower;
}

std::uint32_t merge_nonzero_channels(std::uint32_t source,
                                     std::uint32_t destination) {
  constexpr std::array<std::uint32_t, 4> masks = {
      0xff000000U, 0x00ff0000U, 0x0000ff00U, 0x000000ffU};
  std::uint32_t result = 0U;
  for (const auto mask : masks) {
    result |= (source & mask) != 0U ? source & mask : destination & mask;
  }
  return result;
}

}  // namespace

int main(int argc, char** argv) {
  assert(argc == 2);
  const auto pack = read_file(argv[1]);
  const simtower::OriginalResources resources(pack);
  const auto base_palette = simtower::original_world_palette(resources, nullptr);
  const auto default_document = simtower::make_original_new_tdt();
  const auto default_palette =
      simtower::original_world_palette(resources, &default_document);

  // 1020:0e29 emits three exact flag bands, then forces entry 255 to a zero
  // DWORD instead of consuming the caller's last color. Before every WinG
  // presentation, 1208:09cf reads all 256 logical entries and stores each as
  // B,G,R,0. Native's 0x00RRGGBB DIB words have exactly that little-endian
  // byte layout, so exercise the complete range and the transport ordering.
  auto logical_source = base_palette;
  logical_source[42] = 0x00123456U;
  logical_source[255] = 0x00112233U;
  const auto logical_entries =
      simtower::original_logical_palette_entries(logical_source);
  assert(logical_entries[0].flags == 4U);
  assert(logical_entries[187].flags == 4U);
  assert(logical_entries[188].flags == 1U);
  assert(logical_entries[218].flags == 1U);
  assert(logical_entries[219].flags == 0U);
  assert(logical_entries[254].flags == 0U);
  for (std::size_t index = 0U; index < 255U; ++index) {
    assert(logical_entries[index].red ==
           static_cast<std::uint8_t>(logical_source[index] >> 16U));
    assert(logical_entries[index].green ==
           static_cast<std::uint8_t>(logical_source[index] >> 8U));
    assert(logical_entries[index].blue ==
           static_cast<std::uint8_t>(logical_source[index]));
  }
  const auto dib_bytes =
      std::bit_cast<std::array<std::uint8_t, 4>>(logical_source[42]);
  assert((dib_bytes ==
          std::array<std::uint8_t, 4>{0x56U, 0x34U, 0x12U, 0x00U}));
  assert((logical_entries[255] ==
          simtower::OriginalLogicalPaletteEntry{}));
  // Direct 1020:0f4f coverage: production and this test pass the recovered
  // 0x0300/256 LOGPALETTE with all 1,024 entry bytes through CreatePalette.
  // GetPaletteEntries proves that the resulting GDI object retained every
  // RGB/flag byte, including the explicit zero entry 255.
  HPALETTE logical_palette =
      simtower::create_original_logical_palette(logical_source);
  assert(logical_palette != nullptr);
  std::array<PALETTEENTRY, 256> native_entries{};
  assert(GetPaletteEntries(logical_palette, 0U,
                           static_cast<UINT>(native_entries.size()),
                           native_entries.data()) == native_entries.size());
  for (std::size_t index = 0U; index < native_entries.size(); ++index) {
    assert(native_entries[index].peRed == logical_entries[index].red);
    assert(native_entries[index].peGreen == logical_entries[index].green);
    assert(native_entries[index].peBlue == logical_entries[index].blue);
    assert(native_entries[index].peFlags == logical_entries[index].flags);
  }
  assert(DeleteObject(logical_palette));

  // Direct 1208:07d5 coverage: the shared WinG allocator negates the signed
  // height for top-down storage, rounds 8-bit rows to DWORD boundaries,
  // installs one plane and 256 colors, then clears with BLACKNESS.
  assert((simtower::original_wing_dib_layout(32, 3240) ==
          simtower::OriginalWingDibLayout{
              32, 3240, -3240, 32, 1U, 8U, 0U, 256U, 0x42U}));
  assert((simtower::original_wing_dib_layout(33, 17) ==
          simtower::OriginalWingDibLayout{
              33, 17, -17, 36, 1U, 8U, 0U, 256U, 0x42U}));
  assert((simtower::original_wing_dib_layout(431, 41) ==
          simtower::OriginalWingDibLayout{
              431, 41, -41, 432, 1U, 8U, 0U, 256U, 0x42U}));
  assert((simtower::original_wing_dib_layout(816, 576) ==
          simtower::OriginalWingDibLayout{
              816, 576, -576, 816, 1U, 8U, 0U, 256U, 0x42U}));

  // Direct 1208:002c semantic coverage: native point publication preserves
  // the original ordered x/y pair, including signed coordinates.
  const simtower::OriginalWorldPoint signed_point{-12, 32767};
  assert(signed_point.x == -12 && signed_point.y == 32767);
  assert(simtower::kOriginalWorldWidth == 3000);
  assert(simtower::kOriginalWorldHeight == 4320);
  assert(simtower::original_initial_view(816, 576) ==
         simtower::OriginalWorldPoint({1092, 3420}));
  // 1080:01cb -> 1080:0000 uses visible cell/floor counts, rather than a
  // pixel half-width, when centering an event on a facility coordinate.
  assert(simtower::original_facility_focus_view(148, 17, 816, 576) ==
         simtower::OriginalWorldPoint({776, 3396}));
  assert(simtower::original_facility_focus_view(148, 17, 815, 575) ==
         simtower::OriginalWorldPoint({776, 3396}));

  // 1080:0b26 labels the floor band at the viewport center over the vertical
  // scrollbar's up-arrow. 1080:017f first aligns the view to a 36-pixel band.
  assert(simtower::original_scroll_floor_label(3420, 576) == "7");
  assert(simtower::original_scroll_floor_label(3421, 576) == "7");
  assert(simtower::original_scroll_floor_label(3672, 576) == "B1");
  assert(simtower::original_scroll_floor_label(3708, 576) == "B2");
  assert(simtower::original_scroll_floor_label(0, 0) == "110");

  // Direct 1208:071f/1248:0000/1250:0024 coverage through 10e0:055b: load the original
  // 16x16 BITMAP/21256 Find target, center it eight pixels left of the
  // resolved cell coordinate, clip both edges, and copy every nonzero palette
  // index while leaving marker-zero destination pixels untouched.
  {
    const auto marker = resources.find("BITMAP", 21256);
    const auto marker_view = simtower::original_dib_view(marker);
    assert(marker_view.width == 16 && std::abs(marker_view.height) == 16 &&
           marker_view.bit_count == 8U);
    const int view_x = 144;
    const int view_y = 680;
    const int destination_x = 8;
    const int destination_y = 4;
    const auto baseline = simtower::render_original_world(
        resources, nullptr, view_x, view_y, 32, 24);
    auto rendered = baseline;
    simtower::composite_original_find_marker(
        resources, base_palette, 20, 100, view_x, view_y, rendered);
    std::size_t opaque_pixels{};
    std::size_t transparent_pixels{};
    for (int y = 0; y < 16; ++y) {
      for (int x = 0; x < 16; ++x) {
        const auto index = dib_palette_index(marker, x, y);
        const auto expected = index == 0U
            ? baseline.at(destination_x + x, destination_y + y)
            : base_palette[index];
        assert(rendered.at(destination_x + x, destination_y + y) ==
               expected);
        index == 0U ? ++transparent_pixels : ++opaque_pixels;
      }
    }
    assert(opaque_pixels != 0U && transparent_pixels != 0U);

    auto clipped = baseline;
    simtower::composite_original_find_marker(
        resources, base_palette, 18, 100, view_x, view_y, clipped);
    for (int y = 0; y < 16; ++y) {
      for (int x = 8; x < 16; ++x) {
        const auto index = dib_palette_index(marker, x, y);
        const auto expected = index == 0U
            ? baseline.at(x - 8, destination_y + y)
            : base_palette[index];
        assert(clipped.at(x - 8, destination_y + y) == expected);
      }
    }
  }

  // Direct 1090:056c/057b/058c/0596/05a0/05c3/05e5 checkpoint coverage.
  // Clean frames retain only the three unconditional renderer checkpoints;
  // a dirty frame adds three transport checkpoints and one after each of the
  // exact 24 Elevator slots.
  {
    auto tower = simtower::make_original_new_tdt();
    std::size_t clean_checkpoints{};
    (void)simtower::render_original_world(
        resources, &tower, 0, 0, 32, 36, {}, nullptr, nullptr, 0U, nullptr,
        [&] { ++clean_checkpoints; }, false);
    assert(clean_checkpoints == 3U);
    std::size_t dirty_checkpoints{};
    (void)simtower::render_original_world(
        resources, &tower, 0, 0, 32, 36, {}, nullptr, nullptr, 0U, nullptr,
        [&] { ++dirty_checkpoints; }, true);
    assert(dirty_checkpoints == 30U);
  }

  // 1038:0da5 -> 1028:0570 gates active facility-person updates by
  // (b3de + tenant key) % 16 unless tenant byte 13 is set. 1028:0902 then
  // mutates all six Office presentation records in its exact RNG order.
  {
    auto tower = make_facility_people_tower(7, 9U, 6U, 0);
    auto& tenant = tower.floors[11].tenants[0];
    tenant.exact_bytes[13] = std::byte{0};
    tower.header.frame_time = 1U;
    auto step = simtower::step_original_visible_facility_people(
        tower, 160 * 8, (119 - 11) * 36, 72, 36, true, false);
    assert(step.visible_tenants == 1U && step.dispatched_tenants == 0U &&
           step.changed_people == 0U && tower.random_state == 0U);

    tower.header.frame_time = 0U;
    step = simtower::step_original_visible_facility_people(
        tower, 160 * 8, (119 - 11) * 36, 72, 36, false, false);
    assert(step.dispatched_tenants == 0U && tower.random_state == 0U);

    tenant.exact_bytes[13] = std::byte{1};
    std::uint32_t expected_state = 0U;
    const auto next_random = [&]() {
      expected_state = expected_state * 0x015a4e35U + 1U;
      return static_cast<std::uint16_t>(
          (expected_state >> 16U) & 0x7fffU);
    };
    std::array<std::uint8_t, 6> positions{};
    std::array<std::uint8_t, 6> graphics{};
    positions[0] = 1U;
    graphics[0] = static_cast<std::uint8_t>(next_random() % 2U + 0x0eU);
    positions[1] = 2U;
    graphics[1] = static_cast<std::uint8_t>(next_random() % 2U + 0x10U);
    positions[2] = 3U;
    graphics[2] = static_cast<std::uint8_t>(next_random() % 2U + 0x0cU);
    positions[3] = static_cast<std::uint8_t>(next_random() % 8U);
    graphics[3] = static_cast<std::uint8_t>(next_random() % 2U);
    graphics[4] = static_cast<std::uint8_t>(next_random() % 4U + 0x14U);
    positions[4] = static_cast<std::uint8_t>(next_random() % 8U);
    graphics[5] = static_cast<std::uint8_t>(next_random() % 2U + 0x12U);
    positions[5] = static_cast<std::uint8_t>(next_random() % 8U);

    step = simtower::step_original_visible_facility_people(
        tower, 160 * 8, (119 - 11) * 36, 72, 36, false, false);
    assert(step.dispatched_tenants == 1U && step.changed_people == 6U &&
           tower.random_state == expected_state);
    for (std::size_t index = 0U; index < 6U; ++index) {
      assert(tower.people[index].exact_bytes[7] ==
             static_cast<std::byte>(positions[index]));
      assert(tower.people[index].exact_bytes[8] ==
             static_cast<std::byte>(graphics[index]));
    }
  }

  // Direct 1028:1534 and 1028:1692 coverage. Both two-person food-service
  // helpers test signed state <= 5 independently and consume position RNG
  // before graphic RNG, but use distinct frame moduli and bases.
  {
    auto tower = make_facility_people_tower(12, 4U, 2U);
    tower.people[0].exact_bytes[5] = std::byte{0x80};
    tower.people[1].exact_bytes[5] = std::byte{5};
    std::uint32_t expected_state = 0U;
    const auto next_random = [&]() {
      expected_state = expected_state * 0x015a4e35U + 1U;
      return static_cast<std::uint16_t>(
          (expected_state >> 16U) & 0x7fffU);
    };
    const auto first_position = next_random() % 3U;
    const auto first_graphic = next_random() % 2U + 0x3fU;
    const auto second_position = next_random() % 3U;
    const auto second_graphic = next_random() % 2U + 0x41U;
    const auto step = simtower::step_original_visible_facility_people(
        tower, 160 * 8, (119 - 11) * 36, 32, 36, false, false);
    assert(step.dispatched_tenants == 1U && step.changed_people == 2U &&
           tower.random_state == expected_state);
    assert(tower.people[0].exact_bytes[7] ==
               static_cast<std::byte>(first_position) &&
           tower.people[0].exact_bytes[8] ==
               static_cast<std::byte>(first_graphic));
    assert(tower.people[1].exact_bytes[7] ==
               static_cast<std::byte>(second_position) &&
           tower.people[1].exact_bytes[8] ==
               static_cast<std::byte>(second_graphic));
  }

  {
    auto tower = make_facility_people_tower(6, 4U, 2U);
    std::uint32_t expected_state = 0U;
    const auto next_random = [&]() {
      expected_state = expected_state * 0x015a4e35U + 1U;
      return static_cast<std::uint16_t>(
          (expected_state >> 16U) & 0x7fffU);
    };
    const auto first_position = next_random() % 3U;
    const auto first_graphic = next_random() % 3U + 0x49U;
    const auto second_position = next_random() % 3U;
    const auto second_graphic = next_random() % 4U + 0x4cU;
    const auto step = simtower::step_original_visible_facility_people(
        tower, 160 * 8, (119 - 11) * 36, 32, 36, false, false);
    assert(step.dispatched_tenants == 1U && step.changed_people == 2U &&
           tower.random_state == expected_state);
    assert(tower.people[0].exact_bytes[7] ==
               static_cast<std::byte>(first_position) &&
           tower.people[0].exact_bytes[8] ==
               static_cast<std::byte>(first_graphic));
    assert(tower.people[1].exact_bytes[7] ==
               static_cast<std::byte>(second_position) &&
           tower.people[1].exact_bytes[8] ==
               static_cast<std::byte>(second_graphic));
  }

  // Direct 1038:050e cache-order coverage. The original scans the top visible
  // floor first and consumes all of that tenant's RNG calls before descending
  // to the next screen row; saved floor order runs in the opposite direction.
  {
    auto tower = make_facility_people_tower(7, 9U, 12U, 0);
    auto upper = tower.floors[11].tenants[0];
    store_dword(upper.exact_bytes, 8U, 6U);
    auto& upper_floor = tower.floors[12];
    upper_floor.tenants.clear();
    upper_floor.tenant_index.fill(0U);
    upper_floor.tenants.push_back(upper);
    for (std::size_t index = 6U; index < 12U; ++index) {
      tower.people[index].exact_bytes[0] = std::byte{12};
    }

    std::uint32_t expected_state = 0U;
    const auto next_random = [&]() {
      expected_state = expected_state * 0x015a4e35U + 1U;
      return static_cast<std::uint16_t>(
          (expected_state >> 16U) & 0x7fffU);
    };
    const auto expected_office = [&]() {
      std::array<std::uint8_t, 6> positions{1U, 2U, 3U};
      std::array<std::uint8_t, 6> graphics{};
      graphics[0] = static_cast<std::uint8_t>(next_random() % 2U + 0x0eU);
      graphics[1] = static_cast<std::uint8_t>(next_random() % 2U + 0x10U);
      graphics[2] = static_cast<std::uint8_t>(next_random() % 2U + 0x0cU);
      positions[3] = static_cast<std::uint8_t>(next_random() % 8U);
      graphics[3] = static_cast<std::uint8_t>(next_random() % 2U);
      graphics[4] = static_cast<std::uint8_t>(next_random() % 4U + 0x14U);
      positions[4] = static_cast<std::uint8_t>(next_random() % 8U);
      graphics[5] = static_cast<std::uint8_t>(next_random() % 2U + 0x12U);
      positions[5] = static_cast<std::uint8_t>(next_random() % 8U);
      return std::pair{positions, graphics};
    };
    const auto [upper_positions, upper_graphics] = expected_office();
    const auto [lower_positions, lower_graphics] = expected_office();

    const auto step = simtower::step_original_visible_facility_people(
        tower, 160 * 8, (119 - 12) * 36, 72, 72, true, false);
    assert(step.visible_tenants == 2U && step.dispatched_tenants == 2U &&
           step.changed_people == 12U && tower.random_state == expected_state);
    for (std::size_t ordinal = 0U; ordinal < 6U; ++ordinal) {
      assert(tower.people[6U + ordinal].exact_bytes[7] ==
             static_cast<std::byte>(upper_positions[ordinal]));
      assert(tower.people[6U + ordinal].exact_bytes[8] ==
             static_cast<std::byte>(upper_graphics[ordinal]));
      assert(tower.people[ordinal].exact_bytes[7] ==
             static_cast<std::byte>(lower_positions[ordinal]));
      assert(tower.people[ordinal].exact_bytes[8] ==
             static_cast<std::byte>(lower_graphics[ordinal]));
    }
  }

  // 1028:12c5's late-day Control variant fixes the third Hotel graphic at
  // 0x5f and therefore consumes no sixth random value.
  {
    auto tower = make_facility_people_tower(5, 10U, 3U);
    tower.header.frame_time = 1600U;
    std::uint32_t expected_state = 0U;
    const auto next_random = [&]() {
      expected_state = expected_state * 0x015a4e35U + 1U;
      return static_cast<std::uint16_t>(
          (expected_state >> 16U) & 0x7fffU);
    };
    const auto first_position = next_random() % 9U;
    const auto first_graphic = next_random() % 3U + 0x60U;
    const auto second_position = next_random() % 9U;
    const auto second_graphic = next_random() % 9U + 0x52U;
    const auto third_position = next_random() % 9U;
    const auto step = simtower::step_original_visible_facility_people(
        tower, 160 * 8, (119 - 11) * 36, 80, 36, true, true);
    assert(step.dispatched_tenants == 1U && step.changed_people == 3U &&
           tower.random_state == expected_state);
    assert(tower.people[0].exact_bytes[7] ==
               static_cast<std::byte>(first_position) &&
           tower.people[0].exact_bytes[8] ==
               static_cast<std::byte>(first_graphic));
    assert(tower.people[1].exact_bytes[7] ==
               static_cast<std::byte>(second_position) &&
           tower.people[1].exact_bytes[8] ==
               static_cast<std::byte>(second_graphic));
    assert(tower.people[2].exact_bytes[7] ==
               static_cast<std::byte>(third_position) &&
           tower.people[2].exact_bytes[8] == std::byte{0x5f});
  }

  // 1028:0feb uses b3a0's weekend phase for the first Condo resident and
  // DS:3218's retained Control bit for the third resident.
  {
    auto tower = make_facility_people_tower(9, 16U, 3U);
    tower.header.current_day = 2;
    std::uint32_t expected_state = 0U;
    const auto next_random = [&]() {
      expected_state = expected_state * 0x015a4e35U + 1U;
      return static_cast<std::uint16_t>(
          (expected_state >> 16U) & 0x7fffU);
    };
    const auto first_position = next_random() % 15U;
    const auto second_graphic = next_random() % 9U + 0x29U;
    const auto second_position = second_graphic == 0x31U
        ? 1U
        : static_cast<std::uint16_t>(next_random() % 15U);
    const auto third_position = next_random() % 15U;
    const auto third_graphic = next_random() % 3U + 0x36U;
    const auto step = simtower::step_original_visible_facility_people(
        tower, 160 * 8, (119 - 11) * 36, 128, 36, true, true);
    assert(step.dispatched_tenants == 1U && step.changed_people == 3U &&
           tower.random_state == expected_state);
    assert(tower.people[0].exact_bytes[7] ==
               static_cast<std::byte>(first_position) &&
           tower.people[0].exact_bytes[8] == std::byte{0x22});
    assert(tower.people[1].exact_bytes[7] ==
               static_cast<std::byte>(second_position) &&
           tower.people[1].exact_bytes[8] ==
               static_cast<std::byte>(second_graphic));
    assert(tower.people[2].exact_bytes[7] ==
               static_cast<std::byte>(third_position) &&
           tower.people[2].exact_bytes[8] ==
               static_cast<std::byte>(third_graphic));
  }

  // 1028:0841 updates ordinal zero of a negative facility even when that
  // person's signed state exceeds five.
  {
    auto tower = make_facility_people_tower(-3, 4U, 2U);
    tower.people[0].exact_bytes[5] = std::byte{0x7f};
    std::uint32_t expected_state = 0U;
    const auto next_random = [&]() {
      expected_state = expected_state * 0x015a4e35U + 1U;
      return static_cast<std::uint16_t>(
          (expected_state >> 16U) & 0x7fffU);
    };
    const auto position = next_random() % 3U;
    const auto graphic = next_random() % 6U + 0x39U;
    const auto step = simtower::step_original_visible_facility_people(
        tower, 160 * 8, (119 - 11) * 36, 32, 36, false, false);
    assert(step.dispatched_tenants == 1U && step.changed_people == 1U &&
           tower.random_state == expected_state);
    assert(tower.people[0].exact_bytes[7] ==
               static_cast<std::byte>(position) &&
           tower.people[0].exact_bytes[8] ==
               static_cast<std::byte>(graphic));
    assert(tower.people[1].exact_bytes[7] == std::byte{0xff});
  }

  // 1028:17f0 consumes b406 bit four and b40c. Phase 14 places both
  // Cathedral records at cell 13 with graphic two, then advances b40c.
  {
    auto tower = make_facility_people_tower(40, 28U, 2U);
    store_test_header_word(tower, 60U, 4U);
    store_dword(tower.header.exact_bytes, 66U, 14U);
    const auto step = simtower::step_original_visible_facility_people(
        tower, 160 * 8, (119 - 11) * 36, 224, 36, false, false);
    assert(step.dispatched_tenants == 1U && step.changed_people == 2U &&
           step.cathedral_counter_changed && tower.random_state == 0U);
    for (const auto& person : tower.people) {
      assert(person.exact_bytes[7] == std::byte{13} &&
             person.exact_bytes[8] == std::byte{2});
    }
    assert(std::to_integer<std::uint8_t>(tower.header.exact_bytes[66]) ==
           15U);
  }

  // 1028:00b6 and 11a0:0afc draw each Office figure as two adjacent
  // eight-pixel cells from the concatenated BITMAP/1512..1518 atlas.
  {
    auto tower = make_facility_people_tower(7, 9U, 6U);
    for (std::size_t index = 1U; index < tower.people.size(); ++index) {
      tower.people[index].exact_bytes[5] = std::byte{6};
    }
    tower.people[0].exact_bytes[7] = std::byte{0};
    tower.people[0].exact_bytes[8] = std::byte{0};
    const auto palette = simtower::original_world_palette(resources, &tower);
    const auto raster = simtower::render_original_world(
        resources, &tower, 160 * 8, (119 - 11) * 36, 72, 36);
    const auto office = resources.find("BITMAP", 1448);
    for (int y = 0; y < 24; ++y) {
      for (int x = 0; x < 16; ++x) {
        const auto background = indexed_palette_pixel(
            office, palette, x, y);
        const auto source =
            palette[facility_people_atlas_index(resources, x, y)];
        assert(raster.at(x, 12 + y) ==
               merge_nonzero_channels(source, background));
      }
    }
    for (int y = 0; y < 24; ++y) {
      assert(raster.at(16, 12 + y) ==
             indexed_palette_pixel(office, palette, 16, y));
    }
  }

  // Direct 1028:01ae/02ab/03a3/049b and 11a0:0c01 compositor coverage. The Hotel and Condo
  // families draw one two-cell 24-row atlas figure, food-service repeats the
  // same figure once per four-cell block, and Cathedral switches to the
  // dedicated 36-row BITMAP/3560 atlas only while b406 bit two is active.
  const auto assert_standard_facility_person =
      [&](std::int8_t type, std::uint16_t width,
          std::size_t people_count, bool repeats) {
        auto tower = make_facility_people_tower(type, width, people_count);
        const auto palette = simtower::original_world_palette(resources, &tower);
        const auto baseline = simtower::render_original_world(
            resources, &tower, 160 * 8, (119 - 11) * 36,
            static_cast<int>(width) * 8, 36);
        tower.people[0].exact_bytes[7] = std::byte{0};
        tower.people[0].exact_bytes[8] = std::byte{0};
        const auto rendered = simtower::render_original_world(
            resources, &tower, 160 * 8, (119 - 11) * 36,
            static_cast<int>(width) * 8, 36);
        const std::array origins = {0, repeats ? 32 : 0};
        bool changed = false;
        for (std::size_t copy = 0; copy < (repeats ? 2U : 1U); ++copy) {
          for (int y = 0; y < 24; ++y) {
            for (int x = 0; x < 16; ++x) {
              const int destination_x = origins[copy] + x;
              const auto source =
                  palette[facility_people_atlas_index(resources, x, y)];
              const auto expected = merge_nonzero_channels(
                  source, baseline.at(destination_x, 12 + y));
              assert(rendered.at(destination_x, 12 + y) == expected);
              changed = changed || expected != baseline.at(destination_x, 12 + y);
            }
          }
        }
        assert(changed);
        if (repeats) {
          for (int y = 0; y < 36; ++y) {
            for (int x = 16; x < 32; ++x) {
              assert(rendered.at(x, y) == baseline.at(x, y));
            }
          }
        }
      };
  assert_standard_facility_person(3, 4U, 2U, false);  // 1028:01ae Hotel
  assert_standard_facility_person(9, 16U, 3U, false); // 1028:02ab Condo
  assert_standard_facility_person(6, 8U, 2U, true);   // 1028:03a3 food

  {
    auto tower = make_facility_people_tower(40, 28U, 2U);
    store_test_header_word(tower, 60U, 4U);
    const auto palette = simtower::original_world_palette(resources, &tower);
    const auto baseline = simtower::render_original_world(
        resources, &tower, 160 * 8, (119 - 11) * 36, 224, 36);
    tower.people[0].exact_bytes[7] = std::byte{0};
    tower.people[0].exact_bytes[8] = std::byte{2};
    const auto rendered = simtower::render_original_world(
        resources, &tower, 160 * 8, (119 - 11) * 36, 224, 36);
    const auto cathedral_atlas = resources.find("BITMAP", 3560);
    bool changed = false;
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 16; ++x) {
        const auto source =
            palette[dib_palette_index(cathedral_atlas, 32 + x, y)];
        const auto expected =
            merge_nonzero_channels(source, baseline.at(x, y));
        assert(rendered.at(x, y) == expected);
        changed = changed || expected != baseline.at(x, y);
      }
    }
    assert(changed);
  }

  {
    // Direct 1048:0000 coverage: the tiled BITMAP/857 background samples the
    // original world-x phase instead of restarting at the left clip edge.
    const auto raster = simtower::render_original_world(
        resources, nullptr, 40, 0, 32, 8);
    for (int y = 0; y < raster.height; ++y) {
      for (int x = 0; x < raster.width; ++x) {
        assert(raster.at(x, y) == indexed_palette_pixel(
            resources.find("BITMAP", 857), base_palette, (40 + x) % 32, y));
      }
    }
  }

  // 1048:03a3 restarts the same 3240-pixel strip at world y=1080.
  {
    const auto raster = simtower::render_original_world(
        resources, nullptr, 0, 1080, 32, 8);
    for (int y = 0; y < raster.height; ++y) {
      for (int x = 0; x < raster.width; ++x) {
        assert(raster.at(x, y) == indexed_palette_pixel(
            resources.find("BITMAP", 857), base_palette, x, y));
      }
    }
  }

  // The repeated city strip is opaque at 3905..3959; y=3960 remains the
  // white tail of the initialized sky surface.
  {
    const auto skyline = simtower::render_original_world(
        resources, nullptr, 95, 3905, 4, 55);
    for (int y = 0; y < 55; ++y) {
      for (int x = 0; x < 4; ++x) {
        assert(skyline.at(x, y) == indexed_palette_pixel(
            resources.find("BITMAP", 905), base_palette,
            (95 + x) % 96, y));
      }
    }
    const auto below = simtower::render_original_world(
        resources, nullptr, 0, 3960, 8, 8);
    for (const auto pixel : below.pixels) {
      assert(pixel == 0x00ffffffU);
    }
  }

  // Direct 1048:083f/00ad/05f0/06a5/0717 coverage: place four independently selected
  // BITMAP/900..903 sky decorations inside the visible portion of world
  // rectangle (0,360)-(3000,3888). Placement consumes the same Microsoft
  // rand() stream as the later facility-person paint pass.
  {
    const std::array<std::pair<int, int>, 4> dimensions = {
        std::pair{96, 41}, std::pair{192, 19},
        std::pair{292, 38}, std::pair{216, 43}};
    for (std::size_t index = 0U; index < dimensions.size(); ++index) {
      const auto view = simtower::original_dib_view(
          resources.find("BITMAP", 900 + static_cast<int>(index)));
      assert(view.width == dimensions[index].first &&
             std::abs(view.height) == dimensions[index].second &&
             view.bit_count == 8U);
    }

    auto tower = simtower::make_original_new_tdt();
    tower.random_state = 1U;
    simtower::OriginalSkyDecorationState state{};
    auto step = simtower::step_original_sky_decorations(
        resources, tower, state, 100, 360, 800, 540);
    assert(step.changed && step.repositioned == 4U && step.visible == 4U);
    assert(state.placements[0] ==
           simtower::OriginalSkyDecorationPlacement({2, 230, 582, 522, 620}));
    assert(state.placements[1] ==
           simtower::OriginalSkyDecorationPlacement({2, 404, 483, 696, 521}));
    assert(state.placements[2] ==
           simtower::OriginalSkyDecorationPlacement({3, 595, 389, 811, 432}));
    assert(state.placements[3] ==
           simtower::OriginalSkyDecorationPlacement({2, 332, 392, 624, 430}));
    assert(tower.random_state == 3'101'602'181U);

    const auto retained_random = tower.random_state;
    step = simtower::step_original_sky_decorations(
        resources, tower, state, 100, 360, 800, 540);
    assert(!step.changed && step.repositioned == 0U && step.visible == 4U);
    assert(tower.random_state == retained_random);

    const auto baseline = simtower::render_original_world(
        resources, nullptr, 100, 360, 800, 540);
    const auto decorated = simtower::render_original_world(
        resources, nullptr, 100, 360, 800, 540,
        std::span<const simtower::OriginalElevatorTransferVisual>{},
        &base_palette, &state);
    const auto graphic = resources.find("BITMAP", 902);
    bool checked_opaque_pixel = false;
    for (int y = 0; y < 38 && !checked_opaque_pixel; ++y) {
      for (int x = 0; x < 292; ++x) {
        if (dib_palette_index(graphic, x, y) == 0U) continue;
        const int raster_x = state.placements[0].left - 100 + x;
        const int raster_y = state.placements[0].top - 360 + y;
        assert(decorated.at(raster_x, raster_y) ==
               indexed_palette_pixel(graphic, base_palette, x, y));
        assert(decorated.at(raster_x, raster_y) !=
               baseline.at(raster_x, raster_y));
        checked_opaque_pixel = true;
        break;
      }
    }
    assert(checked_opaque_pixel);

    // Scrolling right clips only slot zero, so precisely that slot consumes
    // three more random values and is replaced inside the new visible band.
    step = simtower::step_original_sky_decorations(
        resources, tower, state, 300, 360, 800, 540);
    assert(step.changed && step.repositioned == 1U && step.visible == 4U);
    assert(state.placements[0] ==
           simtower::OriginalSkyDecorationPlacement({3, 683, 730, 899, 773}));
    assert(tower.random_state == 3'359'393'392U);

    // A band no larger than any source consumes only the four selection
    // random values and leaves the corresponding rectangles empty.
    auto small_tower = simtower::make_original_new_tdt();
    small_tower.random_state = 1U;
    simtower::OriginalSkyDecorationState small{};
    step = simtower::step_original_sky_decorations(
        resources, small_tower, small, 0, 360, 0, 0);
    assert(step.changed == false && step.repositioned == 4U &&
           step.visible == 0U);
    assert(small_tower.random_state == 71'484'141U);
    assert(std::all_of(
        small.placements.begin(), small.placements.end(),
        [](const auto& placement) { return !placement.valid(); }));
  }

  const auto view = simtower::original_initial_view(816, 576);
  const auto clut = resources.find("CLUT", 1000);
  const auto clut1 = resources.find("CLUT", 1001);
  const auto clut2 = resources.find("CLUT", 1002);
  const auto clut3 = resources.find("CLUT", 1003);
  {
    // Direct 1020:0853/098b/08b4 coverage: update only entries 188..193 and mirror those
    // six colors into both 207..212 and 213..218 after every update.
    auto tower = simtower::make_original_new_tdt();
    // 1128:00c4 -> 10d0:086c/0ac2 performs this fresh-tower palette update
    // before the New/Open dialog while the active-document latch remains
    // clear. It is observably not the base 1020:0019 CLUT used before the
    // bootstrap: the initial frame 0x09e5 selects CLUT/1002 here.
    assert(tower.header.frame_time == 0x09e5U);
    const auto startup_bootstrap_palette =
        simtower::original_world_palette(resources, &tower);
    assert(startup_bootstrap_palette != base_palette);
    assert(startup_bootstrap_palette[188] == clut_pixel(clut2, 188U));
    const auto check = [&](std::uint16_t frame, std::uint32_t expected) {
      tower.header.frame_time = frame;
      const auto palette = simtower::original_world_palette(resources, &tower);
      assert(palette[188] == expected);
      assert(palette[207] == expected && palette[213] == expected);
    };
    check(0U, clut_pixel(clut, 188U));
    check(1600U, clut_pixel(clut, 188U));
    check(1650U, interpolate_color(clut_pixel(clut, 188U),
                                   clut_pixel(clut1, 188U),
                                   1650U, 1600U, 1700U));
    check(1700U, clut_pixel(clut1, 188U));
    check(1750U, interpolate_color(clut_pixel(clut1, 188U),
                                   clut_pixel(clut2, 188U),
                                   1750U, 1700U, 1800U));
    check(1800U, clut_pixel(clut2, 188U));
    check(2200U, clut_pixel(clut2, 188U));
    check(2533U, clut_pixel(clut2, 188U));
    check(2566U, clut_pixel(clut1, 188U));
    check(2600U, clut_pixel(clut, 188U));

    assert(tower.header.exact_bytes.size() > 61U);
    tower.header.exact_bytes[60] = std::byte{0x10};
    tower.header.exact_bytes[61] = std::byte{0};
    check(0U, clut_pixel(clut, 188U));
    check(40U, interpolate_color(clut_pixel(clut, 188U),
                                 clut_pixel(clut3, 188U), 40U, 0U, 80U));
    check(80U, clut_pixel(clut3, 188U));
    check(100U, clut_pixel(clut3, 188U));
    check(1500U, clut_pixel(clut3, 188U));
    check(1550U, interpolate_color(clut_pixel(clut3, 188U),
                                   clut_pixel(clut, 188U),
                                   1550U, 1500U, 1600U));
    check(1600U, clut_pixel(clut, 188U));
  }
  {
    // The palette is not merely exposed as data: both 1048's world sheet
    // and 1160's map sheet resolve their indexed source pixels through the
    // same active WinG palette selected by 1020:098b.
    auto tower = simtower::make_original_new_tdt();
    tower.header.frame_time = 1800U;
    const auto active_palette =
        simtower::original_world_palette(resources, &tower);
    const auto is_dynamic_index = [](std::uint8_t index) {
      return (index >= 188U && index <= 193U) ||
             (index >= 207U && index <= 218U);
    };

    const auto sky = resources.find("BITMAP", 857);
    const auto sky_view = simtower::original_dib_view(sky);
    bool checked_world = false;
    for (int y = 0; y < std::abs(sky_view.height) && !checked_world; ++y) {
      for (int x = 0; x < sky_view.width; ++x) {
        const auto index = dib_palette_index(sky, x, y);
        if (!is_dynamic_index(index) ||
            active_palette[index] == base_palette[index]) {
          continue;
        }
        const auto raster = simtower::render_original_world(
            resources, &tower, x, y, 1, 1);
        assert(raster.at(0, 0) == active_palette[index]);
        checked_world = true;
        break;
      }
    }
    assert(checked_world);

    const auto map_source = resources.find("BITMAP", 352);
    const auto map_view = simtower::original_dib_view(map_source);
    const int phase = ((static_cast<std::int16_t>(1800) >> 4) % 20) * 10;
    bool checked_map = false;
    for (int y = 0; y < std::abs(map_view.height) && !checked_map; ++y) {
      for (int source_x = 0; source_x < map_view.width; ++source_x) {
        const auto index = dib_palette_index(map_source, source_x, y);
        if (!is_dynamic_index(index) ||
            active_palette[index] == base_palette[index]) {
          continue;
        }
        const int destination_x =
            y < simtower::kOriginalMapContentHeight - 24
                ? (source_x - phase + simtower::kOriginalMapWidth) %
                      simtower::kOriginalMapWidth
                : source_x;
        const auto raster =
            simtower::render_original_map(resources, &tower, 0U);
        assert(raster.at(destination_x,
                         y + simtower::kOriginalMapToolbarHeight) ==
               active_palette[index]);
        checked_map = true;
        break;
      }
    }
    assert(checked_map);
  }
  {
    // 1020:00cb retains its own tick/counter state and changes ten palette
    // entries independently of 098b. Check the exact 15-coarse-tick boundary
    // (nominally 240 ms) and all three phase families from the first two
    // successful updates.
    auto tower = simtower::make_original_new_tdt();
    simtower::OriginalPaletteRuntime runtime{};
    simtower::reset_original_palette_runtime(
        resources, &tower, runtime, 1000U);
    assert(runtime.initialized && runtime.effect_counter == 0U &&
           runtime.last_effect_tick == 1000U);
    assert(!simtower::step_original_effect_palette(
        tower, runtime, true, 1014U));
    assert(simtower::step_original_effect_palette(
        tower, runtime, true, 1015U));
    assert(runtime.effect_counter == 1U &&
           runtime.last_effect_tick == 1015U);
    assert(runtime.colors[194] == 0x00f3ffffU);
    assert(runtime.colors[195] == 0x009eb8b8U);
    assert(runtime.colors[196] == 0x00f3ffffU);
    assert(runtime.colors[197] == 0x00cfc29cU);
    assert(runtime.colors[198] == 0x00cc786bU);
    assert(runtime.colors[199] == 0x00b5b582U);
    assert(runtime.colors[200] == 0x0002029cU);
    assert(runtime.colors[201] == 0x00828282U);
    assert(runtime.colors[202] == 0x004f4f4fU);
    assert(runtime.colors[203] == 0x00828282U);

    // Disabling Effects returns before both the timestamp and phase update.
    assert(!simtower::step_original_effect_palette(
        tower, runtime, false, 2000U));
    assert(runtime.effect_counter == 1U &&
           runtime.last_effect_tick == 1015U);
    assert(simtower::step_original_effect_palette(
        tower, runtime, true, 2000U));
    assert(runtime.effect_counter == 2U);
    assert(runtime.colors[194] == 0x00f3ffffU);
    assert(runtime.colors[195] == 0x00f3ffffU);
    assert(runtime.colors[196] == 0x009eb8b8U);
    assert(runtime.colors[197] == 0x00cc786bU);
    assert(runtime.colors[198] == 0x00cfc29cU);
    assert(runtime.colors[199] == 0x0002029cU);
    assert(runtime.colors[200] == 0x00b5b582U);

    // 098b changes only its own entries: an intervening time transition must
    // preserve the ten effect colors just written by 00cb.
    const auto effect_snapshot = runtime.colors;
    tower.header.frame_time = 1700U;
    assert(simtower::refresh_original_time_palette(
        resources, tower, runtime));
    for (std::size_t index = 194U; index <= 203U; ++index) {
      assert(runtime.colors[index] == effect_snapshot[index]);
    }

    // Exact 1020:053e: during the strict 80<clock<1490 special window, the
    // routine alternates two six-color bands over aliases 207..218. Its local
    // loop sends one six-entry AnimatePalette span at 207 and one at 213;
    // phase two is even, so the first span receives the active colors.
    store_test_header_word(tower, 60U, 0x10U);
    tower.header.frame_time = 100U;
    (void)simtower::refresh_original_time_palette(resources, tower, runtime);
    assert(simtower::step_original_effect_palette(
        tower, runtime, true, 2015U));
    constexpr std::array<std::uint32_t, 6> active_special = {
        0x00ceefffU, 0x00a5e7ffU, 0x008cd6ffU,
        0x0063ceffU, 0x0042c6ffU, 0x004ab4ffU};
    constexpr std::array<std::uint32_t, 6> inactive_special = {
        0x004c5466U, 0x0057667fU, 0x00576e8cU,
        0x00616699U, 0x00595e7fU, 0x004a4c66U};
    assert(std::equal(active_special.begin(), active_special.end(),
                      runtime.colors.begin() + 207));
    assert(std::equal(inactive_special.begin(), inactive_special.end(),
                      runtime.colors.begin() + 213));

    // The next odd phase reverses which of the two spans is active.
    assert(simtower::step_original_effect_palette(
        tower, runtime, true, 2030U));
    assert(std::equal(inactive_special.begin(), inactive_special.end(),
                      runtime.colors.begin() + 207));
    assert(std::equal(active_special.begin(), active_special.end(),
                      runtime.colors.begin() + 213));

    // The endpoint is excluded. 098b restores the aliases and 00cb leaves
    // them alone even though its ordinary ten-color update still advances.
    tower.header.frame_time = 1490U;
    (void)simtower::refresh_original_time_palette(resources, tower, runtime);
    const auto endpoint_alias = runtime.colors[207];
    assert(simtower::step_original_effect_palette(
        tower, runtime, true, 2045U));
    assert(runtime.colors[207] == endpoint_alias);

    // Signed IDIV is observable after the 16-bit counter wraps: -1 is not
    // divisible by three, whereas treating 0xffff as unsigned would be.
    runtime.effect_counter = 0xffffU;
    assert(simtower::step_original_effect_palette(
        tower, runtime, true, 2060U));
    assert(runtime.colors[195] == 0x00f3ffffU);
    assert(runtime.effect_counter == 0U);

    // Like the original shared magnitude helper, a backwards tick sample is
    // compared by its signed absolute distance.
    runtime.last_effect_tick = 1000U;
    assert(simtower::step_original_effect_palette(
        tower, runtime, true, 900U));
  }
  // Direct 11f8:06cd coverage: each of its three resource reloads uses the
  // same signed-compare-equivalent rating tier.
  assert(simtower::original_lobby_graphics_variant(0U) == 0);
  assert(simtower::original_lobby_graphics_variant(2U) == 0);
  assert(simtower::original_lobby_graphics_variant(3U) == 1);
  assert(simtower::original_lobby_graphics_variant(4U) == 2);
  assert(simtower::original_lobby_graphics_variant(0xffffU) == 2);

  {
    auto tower = simtower::make_original_new_tdt();
    const auto result = simtower::build_original_initial_lobby(
        tower, 160, 192, 1, construction_costs());
    assert(result.succeeded());
    const auto raster = simtower::render_original_world(
        resources, &tower, view.x, view.y, 816, 576);
    // 11c0:02c0 overlays the first twelve pixels with the exterior cap.
    assert_tile(raster, 162 * 8 - view.x, (119 - 10) * 36 - view.y,
                resources.find("CGPK", 2536), 2, clut);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    const auto result = simtower::build_original_initial_lobby(
        tower, 160, 192, 3, construction_costs());
    assert(result.succeeded());
    const auto raster = simtower::render_original_world(
        resources, &tower, view.x, view.y, 816, 576);
    const int x = 162 * 8 - view.x;
    assert_tile(raster, x, (119 - 10) * 36 - view.y,
                resources.find("CGPK", 2536), 84, clut);
    assert_tile(raster, x, (119 - 11) * 36 - view.y,
                resources.find("CGPK", 2600), 43, clut);
    assert_tile(raster, x, (119 - 12) * 36 - view.y,
                resources.find("CGPK", 2664), 2, clut);
  }

  // The left-edge phase table at 1038:00a9 selects tile 34 when left%8=1.
  {
    auto tower = simtower::make_original_new_tdt();
    const auto result = simtower::build_original_initial_lobby(
        tower, 145, 150, 1, construction_costs());
    assert(result.succeeded());
    const auto raster = simtower::render_original_world(
        resources, &tower, view.x, view.y, 816, 576);
    const auto lobby = resources.find("CGPK", 2536);
    constexpr std::size_t tile = 34U;
    for (int y = 24; y < 36; ++y) {
      for (int x = 0; x < 8; ++x) {
        const auto index = std::to_integer<std::uint8_t>(
            lobby[tile * 8U * 36U + static_cast<std::size_t>(y * 8 + x)]);
        assert(raster.at(145 * 8 - view.x + x,
                         (119 - 10) * 36 - view.y + y) ==
               clut_pixel(clut, index));
      }
    }
  }

  // Direct 1128:13dc, 11c0:0000/0232/024a/0374/0428/0483/04ce/0518/054e,
  // and 11f8:3ef3 coverage for the final
  // exterior-edge scan,
  // highest-floor roof decision, and exact source/destination blits. It
  // composes its source
  // rectangles from the exact 11f8:033a WinG sheet rather than from invented
  // artwork: type-4/5000 standard caps, the type-0/type-3 foundation ends,
  // and BITMAP/1002's roof marker.
  {
    auto tower = simtower::make_original_new_tdt();
    simtower::OriginalTdtTenant boundary{};
    boundary.left = 100U;
    boundary.right = 130U;
    boundary.type = 45;
    boundary.exact_bytes[4] = std::byte{45};
    auto& floor = tower.floors[20];
    floor.left_edge = boundary.left;
    floor.right_edge = boundary.right;
    floor.tenants.push_back(boundary);
    const int floor_y = (119 - 20) * 36;

    auto left_baseline_tower = tower;
    left_baseline_tower.floors[20].left_edge = 60U;
    const auto left_baseline = simtower::render_original_world(
        resources, &left_baseline_tower, 100 * 8 - 24, floor_y, 36, 24);
    const auto left = simtower::render_original_world(
        resources, &tower, 100 * 8 - 24, floor_y, 36, 24);
    for (int y = 0; y < 24; ++y) {
      for (int x = 0; x < 36; ++x) {
        const auto source = y < 12
            ? resources.find("BITMAP", 5000)
            : resources.find("BITMAP", 1259);
        const int source_x = y < 12 ? x : 256 + x;
        const int source_y = y < 12 ? y : y - 12;
        const auto index = dib_palette_index(source, source_x, source_y);
        const auto expected = index == 0U
            ? left_baseline.at(x, y)
            : default_palette[index];
        assert(left.at(x, y) == expected);
      }
    }

    auto right_baseline_tower = tower;
    right_baseline_tower.floors[20].right_edge = 170U;
    const auto right_baseline = simtower::render_original_world(
        resources, &right_baseline_tower, 130 * 8, floor_y, 36, 24);
    const auto right = simtower::render_original_world(
        resources, &tower, 130 * 8, floor_y, 36, 24);
    for (int y = 0; y < 24; ++y) {
      for (int x = 0; x < 36; ++x) {
        const auto source = y < 12
            ? resources.find("BITMAP", 5000)
            : resources.find("BITMAP", 1259);
        const int source_x = y < 12 ? 24 + x : 280 + x;
        const int source_y = y < 12 ? y : y - 12;
        const auto index = dib_palette_index(source, source_x, source_y);
        const auto expected = index == 0U
            ? right_baseline.at(x, y)
            : default_palette[index];
        assert(right.at(x, y) == expected);
      }
    }

    const int roof_y = (118 - 20) * 36;
    const auto roof_baseline = simtower::render_original_world(
        resources, nullptr, 100 * 8, roof_y, 36, 36, {}, &default_palette);
    const auto roof = simtower::render_original_world(
        resources, &tower, 100 * 8, roof_y, 36, 36);
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 36; ++x) {
        const auto source = resources.find("BITMAP", 1002);
        const auto index = dib_palette_index(source, x, y);
        assert(roof.at(x, y) ==
               (index == 0U ? roof_baseline.at(x, y)
                            : default_palette[index]));
      }
    }

    // The highest occupied floor owns the 024a decision even when its span is
    // too narrow. It suppresses a lower floor's roof marker instead of making
    // the scan continue downward.
    auto narrow = boundary;
    narrow.left = 200U;
    narrow.right = 206U;
    auto& upper = tower.floors[21];
    upper.left_edge = narrow.left;
    upper.right_edge = narrow.right;
    upper.tenants.push_back(narrow);
    const auto suppressed = simtower::render_original_world(
        resources, &tower, 100 * 8, roof_y, 36, 36, {}, &default_palette);
    const auto background = simtower::render_original_world(
        resources, nullptr, 100 * 8, roof_y, 36, 36, {}, &default_palette);
    assert(suppressed.pixels == background.pixels);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    simtower::OriginalTdtTenant boundary{};
    boundary.left = 100U;
    boundary.right = 130U;
    boundary.type = 45;
    boundary.exact_bytes[4] = std::byte{45};
    auto& floor = tower.floors[10];
    floor.left_edge = boundary.left;
    floor.right_edge = boundary.right;
    floor.tenants.push_back(boundary);
    const int floor_y = (119 - 10) * 36;
    auto ground_baseline_tower = tower;
    ground_baseline_tower.floors[10].left_edge = 50U;
    const auto ground_baseline = simtower::render_original_world(
        resources, &ground_baseline_tower, 100 * 8 - 56, floor_y, 36, 56);
    const auto ground = simtower::render_original_world(
        resources, &tower, 100 * 8 - 56, floor_y, 36, 56);
    for (int y = 0; y < 56; ++y) {
      for (int x = 0; x < 36; ++x) {
        std::span<const std::byte> source{};
        int source_x{};
        int source_y{};
        if (y < 36) {
          source = resources.find("BITMAP", 1001);
          source_x = x;
          source_y = y;
        } else if (y < 48) {
          source = resources.find("BITMAP", 5000);
          source_x = 64 + x;
          source_y = y - 36;
        } else {
          source = resources.find("BITMAP", 1193);
          source_x = 64 + x;
          source_y = y - 48;
        }
        const auto index = dib_palette_index(source, source_x, source_y);
        auto expected = ground_baseline.at(x, y);
        // 11c0 draws the ordinary 24-row cap first, then the ground-only
        // 56-row cap. A transparent ground pixel in their four-pixel overlap
        // therefore preserves the already-masked ordinary cap.
        if (x >= 32 && y < 24) {
          const int ordinary_x = x - 32;
          const auto ordinary_source = y < 12
              ? resources.find("BITMAP", 5000)
              : resources.find("BITMAP", 1259);
          const int ordinary_source_x = y < 12
              ? ordinary_x
              : 256 + ordinary_x;
          const int ordinary_source_y = y < 12 ? y : y - 12;
          const auto ordinary_index = dib_palette_index(
              ordinary_source, ordinary_source_x, ordinary_source_y);
          if (ordinary_index != 0U) expected = default_palette[ordinary_index];
        }
        if (index != 0U) expected = default_palette[index];
        assert(ground.at(x, y) == expected);
      }
    }
  }

  // Type 45 is not an empty Metro sentinel in the renderer. 11f8:033a gives
  // it BITMAP/3880, 1038:00a9 phases its four cells by absolute world cell,
  // and 1038:0a06 -> 11a0:088f copies the complete 8x36 cell opaquely.
  // Exercise an unaligned viewport across both phase wrap points on floor
  // zero, where 11e8:0000 creates these spans around the Metro tenant.
  {
    auto tower = simtower::make_original_new_tdt();
    auto& floor = tower.floors[0];
    simtower::OriginalTdtTenant boundary{};
    boundary.left = 101U;
    boundary.right = 111U;
    boundary.type = 45;
    boundary.exact_bytes[4] = std::byte{45};
    floor.left_edge = boundary.left;
    floor.right_edge = boundary.right;
    floor.tenants = {boundary};
    const int view_x = static_cast<int>(boundary.left) * 8 + 3;
    const int world_y = (119 - 0) * 36;
    const int width = 73;
    const auto raster = simtower::render_original_world(
        resources, &tower, view_x, world_y, width, 36, {}, &default_palette);
    const auto graphic = resources.find("BITMAP", 3880);
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < width; ++x) {
        const int world_pixel_x = view_x + x;
        const int cell = world_pixel_x / 8;
        const int source_x = (cell & 3) * 8 + world_pixel_x % 8;
        assert(raster.at(x, y) == indexed_palette_pixel(
            graphic, default_palette, source_x, y));
      }
    }
  }

  // Type zero does not use the facility-relative frame selector. 11f8:033a's
  // 1000 + type*64 formula assigns it BITMAP/1000..1003; 3944..3949 belongs
  // to type 46 (fire). 11a0:0000 repeats the fixed tenant-status source cell,
  // which is two for a fresh or bulldozed Floor. Check every copied pixel and
  // again with a three-pixel horizontal scroll remainder.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 220, 1, construction_costs())
               .succeeded());
    assert(simtower::build_original_floor(
               tower, 11, 120, 200, construction_costs())
               .succeeded());
    assert(tower.floors[11].tenants.size() == 1U);
    assert(tower.floors[11].tenants[0].status == 2U);
    const int world_y = (119 - 11) * 36;

    for (const int remainder : {0, 3}) {
      const int floor_view_x = 120 * 8 + remainder;
      const int width = 77 * 8 - remainder;
      const auto raster = simtower::render_original_world(
          resources, &tower, floor_view_x, world_y, width, 36);
      const int view_left_cell = floor_view_x / 8;
      for (int y = 0; y < 36; ++y) {
        for (int x = 0; x < width; ++x) {
          const int world_pixel_x = floor_view_x + x;
          if (y < 24 && world_pixel_x < 120 * 8 + 12) {
            // 11c0:02c0's masked left cap is deliberately above this strip.
            continue;
          }
          const int source_x = 2 * 8 + world_pixel_x % 8;
          assert(raster.at(x, y) == horizontal_atlas_pixel(
              resources, default_palette, 1000, 1003, source_x, y));
        }
      }
    }
  }

  // Type 47 is not an empty hole: 1038:09e3 sends its per-cell phase to the
  // BITMAP/4008 damage bank. Verify the 24-row rubble strip is bottom-aligned
  // and advances from tenant-relative cell zero across the former facility
  // span. 1038:00a9's left-modulo-four seed is exclusive to type 45.
  {
    auto tower = simtower::make_original_new_tdt();
    auto& floor = tower.floors[11];
    simtower::OriginalTdtTenant damaged{};
    damaged.left = 121U;
    damaged.right = 130U;
    damaged.type = 47;
    damaged.exact_bytes[4] = std::byte{47};
    floor.left_edge = damaged.left;
    floor.right_edge = damaged.right;
    floor.tenants = {damaged};
    const int world_x = static_cast<int>(damaged.left) * 8;
    const int world_y = (119 - 11) * 36;
    const auto baseline = simtower::render_original_world(
        resources, nullptr, world_x, world_y, 72, 36, {}, &default_palette);
    const auto raster = simtower::render_original_world(
        resources, &tower, world_x, world_y, 72, 36, {}, &default_palette);
    const auto damage = resources.find("BITMAP", 4008);
    for (int y = 0; y < 36; ++y) {
      for (int x = 12; x < 72; ++x) {
        if (y < 12) {
          assert(raster.at(x, y) == baseline.at(x, y));
        } else {
          const int source_x = x;
          assert(raster.at(x, y) == indexed_palette_pixel(
              damage, default_palette, source_x, y - 12));
        }
      }
    }
  }

  // 1038:0716 treats Office status as signed. A positive type-7 record with
  // status 0xff therefore remains in the first branch: SAR gives -1 and
  // variant five selects frame nine. An unsigned comparison would incorrectly
  // seek nonexistent frame 41 and leave this whole facility blank.
  {
    auto tower = simtower::make_original_new_tdt();
    tower.people_count = 0U;
    auto& floor = tower.floors[0];
    simtower::OriginalTdtTenant office{};
    office.left = 160U;
    office.right = 169U;
    office.type = 7;
    office.variant = 5U;
    office.status = 0xffU;
    office.exact_bytes[4] = std::byte{7};
    floor.left_edge = office.left;
    floor.right_edge = office.right;
    floor.tenants = {office};
    const int world_y = (119 - 0) * 36;
    const auto raster = simtower::render_original_world(
        resources, &tower, 160 * 8, world_y, 72, 36, {}, &default_palette);
    for (int y = 0; y < 24; ++y) {
      for (int x = 0; x < 72; ++x) {
        assert(raster.at(x, 12 + y) == indexed_palette_pixel(
            resources.find("BITMAP", 1450), default_palette, 72 + x, y));
      }
    }

    // The variant operand is the full signed word at tenant +0x0c, not just
    // its low byte. High byte one makes the selected frame unavailable, so
    // the original leaves the background untouched instead of drawing the
    // low-byte frame ten.
    floor.tenants[0].status = 0U;
    floor.tenants[0].preserved_07_to_0f[0] = std::byte{1};
    const auto baseline = simtower::render_original_world(
        resources, nullptr, 160 * 8, world_y, 72, 36, {}, &default_palette);
    const auto high_word = simtower::render_original_world(
        resources, &tower, 160 * 8, world_y, 72, 36, {}, &default_palette);
    assert(high_word.pixels == baseline.pixels);
  }

  // Direct 10f0:01f9 visible-facility traversal. 11f8:033a packs
  // BITMAP/1448..1451 into the type-7 bank. 1038:0716
  // selects frames 0..11 from variant/status and frames 12..13 from the
  // occupied status alone; 11a0:060c places the 24-row image beneath the
  // twelve untouched rows at the top of a 36-pixel floor.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 160, 192, 1, construction_costs())
               .succeeded());
    assert(simtower::build_original_office(
               tower, 11, 160, 0, construction_costs())
               .succeeded());
    auto& office = tower.floors[11].tenants[0];
    office.type = 7;
    office.exact_bytes[4] = std::byte{7};
    // This block isolates the underlying facility bank. The translated
    // 1028 person overlay has its own complete pixel assertion above.
    tower.people_count = 0U;
    const int office_world_y = (119 - 11) * 36;

    for (int frame = 0; frame < 14; ++frame) {
      if (frame < 12) {
        office.variant = static_cast<std::uint8_t>(frame / 2);
        office.status = static_cast<std::uint8_t>((frame % 2) * 8);
      } else {
        office.variant = 5;
        office.status = static_cast<std::uint8_t>(16 + (frame - 12) * 8);
      }
      const auto raster = simtower::render_original_world(
          resources, &tower, 160 * 8, office_world_y, 72, 36);
      const int resource_id = 1448 + frame / 4;
      const int source_x = (frame % 4) * 72;
      for (int y = 0; y < 24; ++y) {
        for (int x = 0; x < 72; ++x) {
          if (y < 12 && x < 12) continue;
          assert(raster.at(x, 12 + y) == indexed_palette_pixel(
              resources.find("BITMAP", resource_id), default_palette,
              source_x + x, y));
        }
      }
    }
  }

  // Direct 1038:0def and 11a0:047c/060c coverage: 11f8:033a concatenates each
  // type's
  // consecutive BITMAP resources and 11f8:3fb1 slices that row into width*8
  // by 24 frames. These are all 1038:06a8 ordinary facility-bank cases plus
  // type-0x2c, including linked retail and DS:dbfc selectors. Every opaque
  // cache-copy pixel and packed source frame is checked across boundaries.
  {
    const std::array<FacilityGraphicsTestCase, 30> tests = {{
        {3, 4, 1192, 1195, 18},
        {4, 6, 1256, 1263, 36},
        {5, 10, 1320, 1323, 18},
        {6, 24, 1384, 1393, 20},
        {8, 2, 1512, 1518, 99},
        {9, 16, 1576, 1590, 15},
        {10, 12, 1640, 1652, 35},
        {11, 4, 1704, 1705, 15},
        {12, 16, 1768, 1777, 20},
        {13, 26, 1832, 1834, 3},
        {14, 16, 1896, 1896, 1},
        {15, 15, 1960, 1960, 1},
        {17, 2, 2088, 2088, 8, 36},
        {18, 24, 2152, 2152, 4},
        {19, 24, 2216, 2216, 4, 36},
        {20, 25, 2280, 2285, 6},
        {21, 25, 2344, 2350, 7, 36},
        {29, 24, 2856, 2856, 3},
        {30, 24, 2920, 2920, 3, 36},
        {31, 30, 2984, 2985, 3},
        {32, 30, 3048, 3049, 3, 36},
        {33, 30, 3112, 3113, 3, 36},
        {34, 7, 3176, 3177, 18},
        {35, 7, 3240, 3241, 18, 36},
        {36, 28, 3304, 3305, 3, 36},
        {37, 28, 3368, 3369, 3, 36},
        {38, 28, 3432, 3433, 3, 36},
        {39, 28, 3496, 3497, 3, 36},
        {40, 28, 3560, 3562, 4, 36},
        {0x2c, 16, 3816, 3818, 3},
    }};

    for (const auto& test : tests) {
      auto tower = simtower::make_original_new_tdt();
      // Isolate the direct facility banks from the separately asserted 1028
      // person layer; fresh documents retain a reusable people pool.
      tower.people_count = 0U;
      auto& floor = tower.floors[11];
      floor.tenants.clear();
      simtower::OriginalTdtTenant tenant{};
      tenant.left = 160;
      tenant.right = static_cast<std::uint16_t>(160 + test.width_cells);
      tenant.type = static_cast<std::int8_t>(test.type);
      floor.tenants.push_back(tenant);
      floor.left_edge = tenant.left;
      floor.right_edge = tenant.right;

      for (int frame = 0; frame < test.frame_count; ++frame) {
        auto& current = floor.tenants[0];
        if (test.type == 3 || test.type == 4 || test.type == 5) {
          current.variant = static_cast<std::uint8_t>(frame / 9);
          current.status = static_cast<std::uint8_t>((frame % 9) * 8);
        } else if (test.type == 9) {
          current.variant = static_cast<std::uint8_t>(frame / 5);
          current.status = static_cast<std::uint8_t>((frame % 5) * 8);
        } else if (test.type == 6 || test.type == 12) {
          current.variant = 0;
          tower.retail[0].exact_bytes[2] =
              static_cast<std::byte>(frame % 4);
          tower.retail[0].exact_bytes[11] =
              static_cast<std::byte>(frame / 4);
        } else if (test.type == 10) {
          current.variant = 0;
          if (frame == 0x21) {
            tower.retail[0].exact_bytes[2] = std::byte{0xff};
            tower.retail[0].exact_bytes[11] = std::byte{0};
          } else if (frame == 0x22) {
            tower.retail[0].exact_bytes[2] = std::byte{3};
            tower.retail[0].exact_bytes[11] = std::byte{0};
          } else {
            tower.retail[0].exact_bytes[2] =
                static_cast<std::byte>(frame % 3);
            tower.retail[0].exact_bytes[11] =
                static_cast<std::byte>(frame / 3);
          }
        } else if (test.type == 13) {
          current.variant = 0;
          tower.post_elevator.dbfc_dwords[0] =
              frame == 0 ? 0x00010000U : 0U;
          tower.header.frame_time =
              frame == 1 ? 0U : static_cast<std::uint16_t>(4U * 0x190U);
        } else if (test.type == 18 || test.type == 19) {
          current.variant = 0;
          tower.post_elevator.dc24_records[0][6] =
              static_cast<std::byte>(frame);
        } else if (test.type == 29 || test.type == 30) {
          current.variant = 0;
          // 1038:08a3 ignores tenant status and clamps linked dc24 state
          // three (and larger signed states) to frame two.
          current.status = 0xffU;
          tower.post_elevator.dc24_records[0][6] =
              static_cast<std::byte>(frame == 2 ? 3 : frame);
        } else if (test.type == 31 || test.type == 32 || test.type == 33 ||
                   (test.type >= 36 && test.type <= 40)) {
          current.variant = static_cast<std::uint8_t>(frame);
        } else if (test.type == 34 || test.type == 35) {
          current.variant = 0;
          tower.post_elevator.dc24_records[0][6] =
              static_cast<std::byte>(frame < 3 ? frame : 3);
          tower.post_elevator.dc24_records[0][7] =
              static_cast<std::byte>(frame < 3 ? 0 : frame - 3);
        } else {
          current.variant = 0;
          current.status = static_cast<std::uint8_t>(
              test.type == 14 ? 7 : frame);
        }

        const int world_y = (119 - 11) * 36;
        const int width = test.width_cells * 8;
        const auto raster = simtower::render_original_world(
            resources, &tower, 160 * 8, world_y, width, 36);
        const auto frame_palette =
            simtower::original_world_palette(resources, &tower);
        const auto [resource_id, source_x] =
            facility_frame_source(resources, test, frame);
        for (int y = 0; y < test.graphic_height; ++y) {
          for (int x = 0; x < width; ++x) {
            if (36 - test.graphic_height + y < 24 && x < 12) continue;
            assert(raster.at(x, 36 - test.graphic_height + y) ==
                   indexed_palette_pixel(
                       resources.find("BITMAP", resource_id), frame_palette,
                       source_x + x, y));
          }
        }
      }
    }
  }

  // The exceptional 200x60 BITMAP/2280 contains both Recycling Center halves
  // status zero. 11f8:033a extracts its top 24 rows for type 20; its bottom
  // 36 rows are byte-for-byte the type-21 BITMAP/2344 frame rendered below.
  {
    const auto combined = resources.find("BITMAP", 2280);
    const auto lower = resources.find("BITMAP", 2344);
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 200; ++x) {
        assert(dib_pixel(combined, x, 24 + y) == dib_pixel(lower, x, y));
      }
    }
  }

  // Direct 11a0:088f coverage: negative facility records are the original
  // 11f0 deferred state. 1038:09c0 selects bank 1 of type 0x29, padded with
  // twelve rows of staging CLUT color 14, while 1038:00a9 supplies the
  // tenant-relative cell number without an absolute-world phase. Check the
  // non-aligned room's opaque 8x36 copies.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, construction_costs())
               .succeeded());
    assert(simtower::build_original_hotel_room(
               tower, 3, 11, 121, 0, construction_costs())
               .succeeded());
    assert(tower.floors[11].tenants.size() == 1U);
    assert(tower.floors[11].tenants[0].type == -3);
    // Isolate the type-0x29 pending-construction strip from 1028:0000's
    // independently tested negative-facility person overlay.
    tower.people_count = 0U;

    const int world_y = (119 - 11) * 36;
    const auto raster = simtower::render_original_world(
        resources, &tower, 121 * 8, world_y, 4 * 8, 36);
    const auto construction = resources.find("BITMAP", 3625);
    for (int cell = 0; cell < 4; ++cell) {
      const int source_x = cell * 8;
        for (int y = 0; y < 36; ++y) {
        for (int x = 0; x < 8; ++x) {
          if (y < 24 && cell * 8 + x < 12) continue;
          const auto expected =
              y < 12 ? clut_pixel(clut, 14)
                     : indexed_palette_pixel(
                           construction, default_palette,
                           source_x + x, y - 12);
          assert(raster.at(cell * 8 + x, y) == expected);
        }
      }
    }
  }

  // 10a8:02aa routes a standard elevator through the separate type-1 cap
  // atlas and type-16 shaft bank. Direct 11a0:0126 and 11a0:027c coverage
  // checks both opaque 8x36-cell copiers pixel by pixel. A new car is active on its
  // only serviced floor, selecting the +0x58 car forms in 10a8:0507. Check
  // all pixels in both caps, the base shaft run, and its centered overlay.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, construction_costs())
               .succeeded());
    const simtower::OriginalPartTable part{};
    assert(simtower::build_original_standard_elevator(
               tower, 10, 120, construction_costs(), part)
               .succeeded());
    auto& car = tower.elevators[0].car_records[0].exact_bytes;
    car[0] = std::byte{10};
    car[1] = std::byte{0};
    car[3] = std::byte{0};
    car[4] = std::byte{1};
    car[15] = std::byte{1};

    const int elevator_x = 120 * 8;
    const int top_world_y = (119 - 11) * 36;
    const auto raster = simtower::render_original_world(
        resources, &tower, elevator_x, top_world_y, 32, 108);
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 32; ++x) {
        assert(raster.at(x, y) == horizontal_atlas_pixel(
            resources, default_palette, 1064, 1069, 0x14 * 8 + x, y));
        auto body_expected = horizontal_atlas_pixel(
            resources, default_palette, 2024, 2029, 0x5c * 8 + x, y);
        assert(raster.at(x, 36 + y) == body_expected);
        assert(raster.at(x, 72 + y) == horizontal_atlas_pixel(
            resources, default_palette, 1064, 1069, 0x18 * 8 + x, y));
      }
    }

    // Direct 1090:216e coverage: exact source rectangle, occupancy frame,
    // shaft-relative destination, and later signed six-pixel motion helper.
    assert((simtower::original_elevator_car_visual(
                tower.elevators[0], 0U, elevator_x, top_world_y, 108) ==
            simtower::OriginalElevatorCarVisual{2, 4, 28, 31, 2, 41}));

    // 1090:227b moves in six-pixel increments. Direction zero subtracts the
    // progress; nonzero adds it. 1090:221f maps occupancy three to frame two
    // and a capacity-sized occupancy to the full-car frame four.
    car[1] = std::byte{2};
    car[3] = std::byte{3};
    car[4] = std::byte{0};
    assert((simtower::original_elevator_car_visual(
                tower.elevators[0], 0U, elevator_x, top_world_y, 108) ==
            simtower::OriginalElevatorCarVisual{66, 4, 28, 31, 2, 29}));
    const auto shown_moving = simtower::render_original_world(
        resources, &tower, elevator_x, top_world_y, 32, 108);
    // A nonzero SHOW word keeps the car baked into 10a8:0507's shaft form;
    // 1090:0cb3 skips its late moving-car preparation entirely.
    assert(shown_moving.pixels == raster.pixels);

    // With SHOW zero, 10a8:07d6 retains both caps but replaces every in-span
    // floor with two black 35-pixel boundary lines. Disable the car first so
    // that this exact LineTo endpoint behavior can be checked independently.
    tower.elevators[0].used = 0U;
    const auto without_elevator = simtower::render_original_world(
        resources, &tower, elevator_x, top_world_y, 32, 108);
    tower.elevators[0].used = 1U;
    tower.elevators[0].word_3c = 0U;
    car[15] = std::byte{0};
    const auto hidden_outline = simtower::render_original_world(
        resources, &tower, elevator_x, top_world_y, 32, 108);
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 32; ++x) {
        assert(hidden_outline.at(x, y) == raster.at(x, y));
        assert(hidden_outline.at(x, 72 + y) == raster.at(x, 72 + y));
        const auto expected =
            (x == 0 || x == 31) && y < 35
                ? 0U
                : without_elevator.at(x, 36 + y);
        assert(hidden_outline.at(x, 36 + y) == expected);
      }
    }

    // Direct 1090:0d15/0b10 coverage: 0d15 derives and clips each active
    // car's coverage rectangle, while 0b10 copies the separate late car layer
    // opaquely only for that hidden state, after waiting people and transports.
    car[15] = std::byte{1};
    const auto moving = simtower::render_original_world(
        resources, &tower, elevator_x, top_world_y, 32, 108);
    for (int y = 0; y < 31; ++y) {
      for (int x = 0; x < 28; ++x) {
        assert(moving.at(2 + x, 29 + y) == horizontal_atlas_pixel(
            resources, default_palette, 1064, 1069, 66 + x, 4 + y));
      }
    }

    car[3] = static_cast<std::byte>(tower.elevators[0].capacity);
    car[4] = std::byte{1};
    assert((simtower::original_elevator_car_visual(
                tower.elevators[0], 0U, elevator_x, top_world_y, 108) ==
            simtower::OriginalElevatorCarVisual{130, 4, 28, 31, 2, 53}));
    car[0] = std::byte{20};
    assert(!simtower::original_elevator_car_visual(
        tower.elevators[0], 0U, elevator_x, top_world_y, 108));
  }

  // Type 2 uses the same four-cell 10a8 cap/body branch as type 1. Raw
  // command 43 constructs it, and the only active car selects +0x58.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, construction_costs())
               .succeeded());
    const simtower::OriginalPartTable part{};
    assert(simtower::build_original_elevator(
               tower, 43U, 10, 120, construction_costs(), part)
               .succeeded());
    auto& car = tower.elevators[0].car_records[0].exact_bytes;
    car[0] = std::byte{10};
    car[1] = std::byte{0};
    car[3] = std::byte{0};
    car[4] = std::byte{1};
    car[15] = std::byte{1};

    const int elevator_x = 120 * 8;
    const int top_world_y = (119 - 11) * 36;
    const auto raster = simtower::render_original_world(
        resources, &tower, elevator_x, top_world_y, 32, 108);
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 32; ++x) {
        assert(raster.at(x, y) == horizontal_atlas_pixel(
            resources, default_palette, 1064, 1069, 0x14 * 8 + x, y));
        auto body_expected = horizontal_atlas_pixel(
            resources, default_palette, 2024, 2029, 0x5c * 8 + x, y);
        assert(raster.at(x, 36 + y) == body_expected);
        assert(raster.at(x, 72 + y) == horizontal_atlas_pixel(
            resources, default_palette, 1064, 1069, 0x18 * 8 + x, y));
      }
    }
  }

  // Type 0 is six cells wide. 10a8 draws six-cell caps, one-cell outer car
  // panels from the type-1 atlas, and the common four-cell body inset by one
  // cell. Raw command 42 constructs this exact layout.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, construction_costs())
               .succeeded());
    const simtower::OriginalPartTable part{};
    assert(simtower::build_original_elevator(
               tower, 42U, 10, 120, construction_costs(), part)
               .succeeded());
    auto& car = tower.elevators[0].car_records[0].exact_bytes;
    car[0] = std::byte{10};
    car[1] = std::byte{0};
    car[3] = std::byte{0};
    car[4] = std::byte{1};
    car[15] = std::byte{1};

    const int elevator_x = 120 * 8;
    const int top_world_y = (119 - 11) * 36;
    const auto raster = simtower::render_original_world(
        resources, &tower, elevator_x, top_world_y, 48, 108);
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 48; ++x) {
        assert(raster.at(x, y) == horizontal_atlas_pixel(
            resources, default_palette, 1064, 1069, 0x4e * 8 + x, y));
        std::uint32_t body_expected{};
        if (x < 8) {
          body_expected = horizontal_atlas_pixel(
              resources, default_palette, 1064, 1069, 0x5a * 8 + x, y);
        } else if (x < 40) {
          body_expected = horizontal_atlas_pixel(
              resources, default_palette, 2024, 2029,
              0x5c * 8 + (x - 8), y);
        } else {
          body_expected = horizontal_atlas_pixel(
              resources, default_palette, 1064, 1069,
              0x5b * 8 + (x - 40), y);
        }
        assert(raster.at(x, 36 + y) == body_expected);
        assert(raster.at(x, 72 + y) == horizontal_atlas_pixel(
            resources, default_palette, 1064, 1069, 0x54 * 8 + x, y));
      }
    }
  }

  // 10c0:007a draws each normal Stair from the original type-0x17 and
  // type-0x16 banks after elevators. Type 0x16 is two 24-row DIBs padded at
  // the top with CLUT color 14. Check both the static frame and animation
  // frame 13, which crosses into the second source resource.
  {
    auto tower = simtower::make_original_new_tdt();
    auto& stair = tower.post_elevator.stairs_bd70[0];
    stair.used = 1U;
    stair.shape = 1U;
    stair.x = 124U;
    stair.floor = 10;
    const int top_world_y = (119 - 11) * 36;

    for (const int frame : {0, 13}) {
      stair.word_6 = static_cast<std::uint16_t>(frame == 0 ? 0 : 1);
      stair.word_8 = 0U;
      tower.header.frame_time =
          static_cast<std::uint16_t>(frame == 0 ? 0 : frame - 1);
      const auto raster = simtower::render_original_world(
          resources, &tower, 124 * 8, top_world_y, 64, 72);
      for (int y = 0; y < 36; ++y) {
        for (int x = 0; x < 64; ++x) {
          const auto upper_expected = y < 12
              ? clut_pixel(clut, 14)
              : horizontal_atlas_pixel(resources, base_palette, 2408, 2409,
                                        frame * 64 + x, y - 12);
          assert(raster.at(x, y) == upper_expected);
          assert(raster.at(x, 36 + y) == horizontal_atlas_pixel(
              resources, base_palette, 2472, 2473, frame * 64 + x, y));
        }
      }
    }
  }

  // Shape 0 is Escalator. Its two 512x36 banks contain eight complete
  // eight-cell frames; active word_6/word_8 state selects frame_time%7+1.
  {
    auto tower = simtower::make_original_new_tdt();
    auto& escalator = tower.post_elevator.stairs_bd70[0];
    escalator.used = 1U;
    escalator.shape = 0U;
    escalator.x = 124U;
    escalator.floor = 10;
    escalator.word_6 = 1U;
    tower.header.frame_time = 6U;
    const int top_world_y = (119 - 11) * 36;
    const auto raster = simtower::render_original_world(
        resources, &tower, 124 * 8, top_world_y, 64, 72);
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 64; ++x) {
        assert(raster.at(x, y) == horizontal_atlas_pixel(
            resources, base_palette, 2728, 2728, 7 * 64 + x, y));
        assert(raster.at(x, 36 + y) == horizontal_atlas_pixel(
            resources, base_palette, 2792, 2792, 7 * 64 + x, y));
      }
    }
  }

  // 10c0:0345 renders lobby-spanning shapes from the cell-major original
  // CGPK bank loaded by 11f8:0680. For a three-story Lobby, shape 5 uses
  // frames 10/21/32/43 and shape 4 uses 55/67/79/91 at these frame times.
  {
    auto tower = simtower::make_original_new_tdt();
    tower.header.lobby_height = 3U;
    auto& transport = tower.post_elevator.stairs_bd70[0];
    transport.used = 1U;
    transport.x = 124U;
    transport.floor = 10;
    const int top_world_y = (119 - 13) * 36;
    const auto graphics = resources.find("CGPK", 4074);

    transport.shape = 5U;
    tower.header.frame_time = 9U;
    auto raster = simtower::render_original_world(
        resources, &tower, 124 * 8, top_world_y, 64, 144);
    for (int row = 0; row < 4; ++row) {
      const int frame = 10 + row * 11;
      for (int cell = 0; cell < 8; ++cell) {
        assert_tile(raster, cell * 8, row * 36, graphics,
                    static_cast<std::size_t>(frame * 8 + cell), clut);
      }
    }

    transport.shape = 4U;
    tower.header.frame_time = 10U;
    raster = simtower::render_original_world(
        resources, &tower, 124 * 8, top_world_y, 64, 144);
    for (int row = 0; row < 4; ++row) {
      const int frame = 55 + row * 12;
      for (int cell = 0; cell < 8; ++cell) {
        assert_tile(raster, cell * 8, row * 36, graphics,
                    static_cast<std::size_t>(frame * 8 + cell), clut);
      }
    }
  }

  // Direct 10a8:088c/1875/12c1 coverage paints the second Elevator waiting
  // ring after the
  // shaft layer. Type 3 uses graphic family zero and its right-facing cell
  // zero selected by 10a8:1913. Palette index zero is transparent; every
  // other source byte is copied through the shared CLUT. This directly covers
  // 11a0:0eaf's packed compositor: zero preserves destination, ordinary bytes
  // replace it, and marker 0xff uses the supplied override.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, construction_costs())
               .succeeded());
    const simtower::OriginalPartTable part{};
    assert(simtower::build_original_standard_elevator(
               tower, 10, 120, construction_costs(), part)
               .succeeded());
    tower.people.resize(1U);
    tower.people_count = 1U;
    tower.people[0].exact_bytes[4] = std::byte{3};
    store_word(tower.people[0].exact_bytes, 10U,
               tower.header.frame_time);
    auto& record = test_elevator_floor_record(test_elevator(tower, 120U), 10);
    record.exact_bytes[3] = std::byte{0};
    store_dword(record.exact_bytes, 164U, 0U);

    constexpr int kPersonX = 126;
    constexpr int kWorldY = (119 - 10) * 36;
    const auto without_person = simtower::render_original_world(
        resources, &tower, kPersonX * 8, kWorldY, 8, 36);
    record.exact_bytes[2] = std::byte{1};
    const auto with_person = simtower::render_original_world(
        resources, &tower, kPersonX * 8, kWorldY, 8, 36);
    bool saw_transparent = false;
    bool saw_opaque = false;
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 8; ++x) {
        const auto index = people_atlas_index(resources, 0, x, y);
        const auto expected = index == 0U
            ? without_person.at(x, y)
            : clut_pixel(clut, index);
        assert(with_person.at(x, y) == expected);
        saw_transparent |= index == 0U;
        saw_opaque |= index != 0U;
      }
    }
    assert(saw_transparent && saw_opaque);
  }

  // Direct 10a8:12c1/1737 signed-metric and palette-band coverage. At a
  // two-story Lobby, a start
  // timestamp one tick ahead wraps to -1 and 11d8:0423 discounts it to zero.
  // The retained low-ten-bit metric 100 therefore crosses PART/1000's first
  // wait band and replaces every person-atlas 0xff marker with CLUT index 22.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, construction_costs())
               .succeeded());
    const simtower::OriginalPartTable part{};
    assert(simtower::build_original_standard_elevator(
               tower, 10, 120, construction_costs(), part)
               .succeeded());
    tower.header.lobby_height = 2U;
    tower.header.frame_time = 0U;
    tower.header.rating = 1U;
    tower.people.resize(1U);
    tower.people_count = 1U;
    auto& person = tower.people[0].exact_bytes;
    person[4] = std::byte{3};
    store_word(person, 10U, 1U);
    store_word(person, 12U, 100U);
    auto& record = test_elevator_floor_record(test_elevator(tower, 120U), 10);
    record.exact_bytes[3] = std::byte{0};
    store_dword(record.exact_bytes, 164U, 0U);

    constexpr int kPersonX = 126;
    constexpr int kWorldY = (119 - 10) * 36;
    const auto without_person = simtower::render_original_world(
        resources, &tower, kPersonX * 8, kWorldY, 8, 36);
    record.exact_bytes[2] = std::byte{1};
    const auto with_person = simtower::render_original_world(
        resources, &tower, kPersonX * 8, kWorldY, 8, 36);
    bool saw_wait_substitution = false;
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 8; ++x) {
        const auto index = people_atlas_index(resources, 0, x, y);
        const auto expected = index == 0U
            ? without_person.at(x, y)
            : clut_pixel(clut, index == 0xffU ? 0x16U : index);
        assert(with_person.at(x, y) == expected);
        saw_wait_substitution |= index == 0xffU;
      }
    }
    assert(saw_wait_substitution);
  }

  // Direct 10a8:1cbb/1d41 adjacent-shaft lane-boundary coverage. With
  // standard shafts at cells 120 and 140, the shared midpoint expression
  // splits the gap at cell 132: the left shaft's second queue occupies
  // 128..131, and the right shaft's first queue occupies 132..137.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 220, 1, construction_costs())
               .succeeded());
    const simtower::OriginalPartTable part{};
    assert(simtower::build_original_standard_elevator(
               tower, 10, 120, construction_costs(), part)
               .succeeded());
    assert(simtower::build_original_standard_elevator(
               tower, 10, 140, construction_costs(), part)
               .succeeded());
    tower.people.assign(20U, simtower::OriginalTdtPersonRecord{});
    tower.people_count = 20U;
    for (auto& person : tower.people) {
      person.exact_bytes[4] = std::byte{3};
    }
    auto& left_record =
        test_elevator_floor_record(test_elevator(tower, 120U), 10);
    auto& right_record =
        test_elevator_floor_record(test_elevator(tower, 140U), 10);
    for (std::size_t slot = 0; slot < 10U; ++slot) {
      store_dword(left_record.exact_bytes, 164U + slot * 4U,
                  static_cast<std::uint32_t>(slot));
      store_dword(right_record.exact_bytes, 4U + slot * 4U,
                  static_cast<std::uint32_t>(10U + slot));
    }
    constexpr int kViewCell = 100;
    constexpr int kWorldY = (119 - 10) * 36;
    const auto baseline = simtower::render_original_world(
        resources, &tower, kViewCell * 8, kWorldY, 80 * 8, 36);
    const auto cell_changed = [&](const simtower::OriginalWorldRaster& raster,
                                  int world_cell) {
      const int left = (world_cell - kViewCell) * 8;
      for (int y = 0; y < 36; ++y) {
        for (int x = 0; x < 8; ++x) {
          if (raster.at(left + x, y) != baseline.at(left + x, y)) {
            return true;
          }
        }
      }
      return false;
    };

    left_record.exact_bytes[2] = std::byte{10};
    const auto left_second = simtower::render_original_world(
        resources, &tower, kViewCell * 8, kWorldY, 80 * 8, 36);
    for (int cell = 128; cell <= 131; ++cell) {
      assert(cell_changed(left_second, cell));
    }
    assert(!cell_changed(left_second, 132));

    left_record.exact_bytes[2] = std::byte{0};
    right_record.exact_bytes[0] = std::byte{10};
    const auto right_first = simtower::render_original_world(
        resources, &tower, kViewCell * 8, kWorldY, 80 * 8, 36);
    assert(!cell_changed(right_first, 131));
    for (int cell = 132; cell <= 137; ++cell) {
      assert(cell_changed(right_first, cell));
    }
    assert(!cell_changed(right_first, 138));
  }

  // Direct 11a0:134c coverage: native consumes the same wrapped signed
  // index*scale*36 offsets instead of retaining the disposable 60-dword
  // process table.
  static_assert(simtower::original_precomputed_floor_offset(0, 1) == 0);
  static_assert(simtower::original_precomputed_floor_offset(59, 1) == 2124);
  static_assert(simtower::original_precomputed_floor_offset(59, -1) == -2124);
  static_assert(simtower::original_precomputed_floor_offset(
                    59, std::numeric_limits<std::int32_t>::max()) == -2124);

  // Direct 1208:0083 coverage: subtract both signed POINT words with Win16
  // wraparound, including the otherwise-overflowing minimum-minus-one case.
  static_assert(simtower::original_point_subtract({12, -9}, 5, -4) ==
                simtower::OriginalPoint16{7, -5});
  static_assert(simtower::original_point_subtract({-32768, 32767}, 1, -1) ==
                simtower::OriginalPoint16{32767, -32768});

  // Direct 10f0:0121/01f9 and 10a8:12c1 DS:b3ae presentation coverage: the
  // saved
  // Elevator snapshot supplies the person dword while the live simulated
  // ring slot supplies the signed wait metric. This reproduces the preview
  // state after 10f0 restores only the original count/cursor bytes.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, construction_costs())
               .succeeded());
    const simtower::OriginalPartTable part{};
    assert(simtower::build_original_standard_elevator(
               tower, 10, 120, construction_costs(), part)
               .succeeded());
    tower.header.rating = 1U;
    tower.people.resize(1U);
    tower.people_count = 1U;
    tower.people[0].exact_bytes[4] = std::byte{3};
    auto& record = test_elevator_floor_record(test_elevator(tower, 120U), 10);
    record.exact_bytes[3] = std::byte{0};
    store_dword(record.exact_bytes, 164U, 0U);
    const auto saved_elevator = tower.elevators[0];
    const simtower::OriginalElevatorWaitingIsolationView isolation{
        0U, &saved_elevator};
    // The live slot no longer contains a person index; 100 is its projected
    // metric and selects the lower PART/1000 band.
    store_dword(record.exact_bytes, 164U, 100U);

    constexpr int kPersonX = 126;
    constexpr int kWorldY = (119 - 10) * 36;
    const auto without_person = simtower::render_original_world(
        resources, &tower, kPersonX * 8, kWorldY, 8, 36);
    record.exact_bytes[2] = std::byte{1};
    const auto with_person = simtower::render_original_world(
        resources, &tower, kPersonX * 8, kWorldY, 8, 36,
        std::span<const simtower::OriginalElevatorTransferVisual>{},
        nullptr, nullptr, 0U, &isolation);
    bool saw_wait_substitution = false;
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 8; ++x) {
        const auto index = people_atlas_index(resources, 0, x, y);
        const auto expected = index == 0U
            ? without_person.at(x, y)
            : clut_pixel(clut, index == 0xffU ? 0x16U : index);
        assert(with_person.at(x, y) == expected);
        saw_wait_substitution |= index == 0xffU;
      }
    }
    assert(saw_wait_substitution);
  }

  // Direct 10a8:1a88 width-selector coverage through its waiting-ring
  // consumer. The first ring walks right-to-left; a type-15 person is two
  // cells wide, so 10a8:0fff emits family cells 58 and 59 in screen order.
  // Cursor 39 also verifies the exact forty-entry ring wrap boundary.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, construction_costs())
               .succeeded());
    const simtower::OriginalPartTable part{};
    assert(simtower::build_original_standard_elevator(
               tower, 10, 120, construction_costs(), part)
               .succeeded());
    tower.people.resize(1U);
    tower.people_count = 1U;
    tower.people[0].exact_bytes[4] = std::byte{15};
    auto& record = test_elevator_floor_record(test_elevator(tower, 120U), 10);
    record.exact_bytes[1] = std::byte{39};
    store_dword(record.exact_bytes, 4U + 39U * 4U, 0U);

    constexpr int kPersonX = 116;
    constexpr int kWorldY = (119 - 10) * 36;
    const auto without_person = simtower::render_original_world(
        resources, &tower, kPersonX * 8, kWorldY, 16, 36);
    record.exact_bytes[0] = std::byte{1};
    const auto with_person = simtower::render_original_world(
        resources, &tower, kPersonX * 8, kWorldY, 16, 36);
    for (int cell = 0; cell < 2; ++cell) {
      for (int y = 0; y < 36; ++y) {
        for (int x = 0; x < 8; ++x) {
          const auto index = people_atlas_index(
              resources, 58 + cell, x, y);
          const int destination_x = cell * 8 + x;
          const auto expected = index == 0U
              ? without_person.at(destination_x, y)
              : clut_pixel(clut, index);
          assert(with_person.at(destination_x, y) == expected);
        }
      }
    }
  }

  // Direct 10a8:1737 priority coverage. Type 14 selects family 80, putting
  // left-facing cell 89 in BITMAP/1129 rather than BITMAP/1128. The periodic
  // VIP branch precedes named/metric
  // classification and replaces every source 0xff byte with CLUT index 125.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, construction_costs())
               .succeeded());
    const simtower::OriginalPartTable part{};
    assert(simtower::build_original_standard_elevator(
               tower, 10, 120, construction_costs(), part)
               .succeeded());
    tower.people.resize(1U);
    tower.people_count = 1U;
    tower.people[0].exact_bytes[4] = std::byte{14};
    tower.post_elevator.b928 = 1U;
    tower.post_elevator.b924 = 0;
    auto& record = test_elevator_floor_record(test_elevator(tower, 120U), 10);
    record.exact_bytes[1] = std::byte{0};
    store_dword(record.exact_bytes, 4U, 0U);

    constexpr int kPersonX = 117;
    constexpr int kWorldY = (119 - 10) * 36;
    const auto without_person = simtower::render_original_world(
        resources, &tower, kPersonX * 8, kWorldY, 8, 36);
    record.exact_bytes[0] = std::byte{1};
    const auto with_person = simtower::render_original_world(
        resources, &tower, kPersonX * 8, kWorldY, 8, 36);
    bool saw_vip_substitution = false;
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 8; ++x) {
        const auto index = people_atlas_index(resources, 89, x, y);
        const auto expected = index == 0U
            ? without_person.at(x, y)
            : clut_pixel(clut, index == 0xffU ? 0x7dU : index);
        assert(with_person.at(x, y) == expected);
        saw_vip_substitution |= index == 0xffU;
      }
    }
    assert(saw_vip_substitution);
  }

  // 10a8:022b's process-only car-transfer caches are painted by 02aa ->
  // 10a8:0de6 between the Elevator body and waiting queues. The four exact cases
  // are boarding/up = family+5..6 at x-2, boarding/down = family+7 at x-1,
  // alighting/up = family+3..4 at x+4, and alighting/down = family+2 at x+6.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, construction_costs())
               .succeeded());
    const simtower::OriginalPartTable part{};
    assert(simtower::build_original_standard_elevator(
               tower, 10, 120, construction_costs(), part)
               .succeeded());
    tower.people.resize(1U);
    tower.people_count = 1U;
    tower.people[0].exact_bytes[4] = std::byte{3};
    auto& elevator = test_elevator(tower, 120U);
    const int world_y = (119 - 10) * 36;

    struct TransferCase {
      bool boarding;
      bool direction_up;
      int world_x;
      int source;
      int width;
    };
    const std::array<TransferCase, 4> cases = {{
        {true, true, 118, 5, 2},
        {true, false, 119, 7, 1},
        {false, true, 124, 3, 2},
        {false, false, 126, 2, 1},
    }};
    for (const auto& test : cases) {
      const auto baseline = simtower::render_original_world(
          resources, &tower, test.world_x * 8, world_y,
          test.width * 8, 36);
      const std::array visuals = {
          simtower::OriginalElevatorTransferVisual{
              static_cast<std::size_t>(&elevator - tower.elevators.data()),
              10, test.boarding, test.direction_up, 0U},
      };
      const auto rendered = simtower::render_original_world(
          resources, &tower, test.world_x * 8, world_y,
          test.width * 8, 36, visuals);
      bool saw_opaque = false;
      for (int cell = 0; cell < test.width; ++cell) {
        for (int y = 0; y < 36; ++y) {
          for (int x = 0; x < 8; ++x) {
            const auto index = people_atlas_index(
                resources, test.source + cell, x, y);
            const int destination_x = cell * 8 + x;
            const auto expected = index == 0U
                ? baseline.at(destination_x, y)
                : clut_pixel(clut, index);
            assert(rendered.at(destination_x, y) == expected);
            saw_opaque |= index != 0U;
          }
        }
      }
      assert(saw_opaque);
    }

    // The transfer predicate uses a strict left and inclusive right boundary.
    // The exact-left draw remains hidden by 11c0's nonzero cap pixels. At the
    // inclusive right edge, however, the accepted person remains visible
    // through the cap's palette-zero holes; moving the cap outward exposes a
    // different portion of that same accepted draw.
    tower.floors[10].left_edge = 118U;
    std::array boundary_visual = {
        simtower::OriginalElevatorTransferVisual{0U, 10, true, true, 0U},
    };
    auto baseline = simtower::render_original_world(
        resources, &tower, 118 * 8, world_y, 16, 36);
    auto clipped = simtower::render_original_world(
        resources, &tower, 118 * 8, world_y, 16, 36, boundary_visual);
    assert(clipped.pixels == baseline.pixels);

    tower.floors[10].left_edge = 100U;
    tower.floors[10].right_edge = 126U;
    boundary_visual[0] = {0U, 10, false, false, 0U};
    baseline = simtower::render_original_world(
        resources, &tower, 126 * 8, world_y, 8, 36);
    const auto inclusive = simtower::render_original_world(
        resources, &tower, 126 * 8, world_y, 8, 36, boundary_visual);
    assert(inclusive.pixels != baseline.pixels);
    tower.floors[10].right_edge = 127U;
    baseline = simtower::render_original_world(
        resources, &tower, 126 * 8, world_y, 8, 36);
    const auto exposed = simtower::render_original_world(
        resources, &tower, 126 * 8, world_y, 8, 36, boundary_visual);
    assert(exposed.pixels != baseline.pixels);
    assert(exposed.pixels != inclusive.pixels);

    // 10a8:022b overwrites the same floor/Elevator/side cache slot instead
    // of layering both transient people. The last family must therefore be
    // identical to rendering that family alone, including transparent pixels.
    tower.people.resize(2U);
    tower.people_count = 2U;
    tower.people[1].exact_bytes[4] = std::byte{14};
    const std::array overwritten = {
        simtower::OriginalElevatorTransferVisual{
            0U, 10, true, true, 0U},
        simtower::OriginalElevatorTransferVisual{
            0U, 10, true, true, 1U},
    };
    const std::array retained = {
        simtower::OriginalElevatorTransferVisual{
            0U, 10, true, true, 1U},
    };
    const auto overwritten_render = simtower::render_original_world(
        resources, &tower, 116 * 8, world_y, 32, 36, overwritten);
    const auto retained_render = simtower::render_original_world(
        resources, &tower, 116 * 8, world_y, 32, 36, retained);
    assert(overwritten_render.pixels == retained_render.pixels);

    // 10a8:0000 sorts each row by shaft x rather than Elevator-table index,
    // and 02aa interleaves each shaft floor with that shaft's transfer slots.
    // Put the right shaft in the lower table index: it must still paint after
    // the left shaft and completely cover the left shaft's two-cell exit.
    tower.elevators[1] = tower.elevators[0];
    tower.elevators[1].x = 120U;
    tower.elevators[0].x = 124U;
    const std::array adjacent_visual = {
        simtower::OriginalElevatorTransferVisual{
            1U, 10, false, true, 0U},
    };
    const auto adjacent_baseline = simtower::render_original_world(
        resources, &tower, 120 * 8, world_y, 64, 36);
    const auto adjacent_transfer = simtower::render_original_world(
        resources, &tower, 120 * 8, world_y, 64, 36, adjacent_visual);
    assert(adjacent_transfer.pixels == adjacent_baseline.pixels);
  }

  // 10f8:00c9 walks all ten packed cf88 Security registrations, resolves
  // each high-byte tenant key through the floor's +0xa92 index table, and
  // draws each active zero-delay responder as two cells from people-atlas
  // family 90+state. 11a0:1144 merges nonzero channels independently.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 220, 1, construction_costs())
               .succeeded());
    assert(simtower::build_original_floor(
               tower, 11, 100, 220, construction_costs())
               .succeeded());
    auto& owner = tower.floors[11].tenants[0];
    // 1220:6ba9's runtime +0x0e includes the six-byte floor header; the
    // serialized people-run index is tenant byte +8.
    store_dword(owner.exact_bytes, 8U, 0U);
    tower.floors[11].tenant_index[0] = 0U;
    tower.people.resize(6U);
    tower.people_count = 6U;
    for (auto& person : tower.people) {
      person.exact_bytes[5] = std::byte{1};
    }
    auto& responder = tower.people[5];
    responder.exact_bytes[5] = std::byte{0};
    responder.exact_bytes[7] = std::byte{11};
    responder.exact_bytes[8] = std::byte{8};
    store_word(responder.exact_bytes, 10U, 0U);
    store_word(responder.exact_bytes, 14U, 120U);

    tower.post_elevator.cf88_words.fill(0xffffU);
    // The last slot proves that 10f8's ten iterations are not five packed
    // pairs; floor 11/key zero is the native word 000b.
    tower.post_elevator.cf88_words[9] = 0x000bU;
    store_test_header_word(tower, 60U, 0U);
    constexpr int kWorldX = 120 * 8;
    constexpr int kWorldY = (119 - 11) * 36;
    const auto baseline = simtower::render_original_world(
        resources, &tower, kWorldX, kWorldY, 16, 36);

    store_test_header_word(tower, 60U, 1U);
    const auto rendered = simtower::render_original_world(
        resources, &tower, kWorldX, kWorldY, 16, 36);
    bool saw_changed_channel = false;
    for (int cell = 0; cell < 2; ++cell) {
      for (int y = 0; y < 36; ++y) {
        for (int x = 0; x < 8; ++x) {
          const int destination_x = cell * 8 + x;
          const auto index = people_atlas_index(
              resources, 98 + cell, x, y);
          const auto expected = merge_nonzero_channels(
              clut_pixel(clut, index),
              baseline.at(destination_x, y));
          assert(rendered.at(destination_x, y) == expected);
          saw_changed_channel |= expected != baseline.at(destination_x, y);
        }
      }
    }
    assert(saw_changed_channel);

    // The original rejects a responder unless both cells fit horizontally.
    const auto clipped_baseline = simtower::render_original_world(
        resources, &tower, kWorldX, kWorldY, 15, 36);
    responder.exact_bytes[5] = std::byte{1};
    const auto inactive = simtower::render_original_world(
        resources, &tower, kWorldX, kWorldY, 15, 36);
    assert(clipped_baseline.pixels == inactive.pixels);

    // Nonzero person word +10 suppresses an otherwise active responder.
    responder.exact_bytes[5] = std::byte{0};
    store_word(responder.exact_bytes, 10U, 1U);
    const auto delayed = simtower::render_original_world(
        resources, &tower, kWorldX, kWorldY, 16, 36);
    assert(delayed.pixels == baseline.pixels);
  }

  // 1048:00ad packs BITMAP/904 as the 140x48 annual moving-effect sprite;
  // 11b8:0089 draws it at dd70/dd72 through CLUT/1000 with index zero
  // transparent. The native direct renderer does not need the original's
  // adjacent saved-background rectangle, but its resulting pixels are exact.
  {
    auto tower = simtower::make_original_new_tdt();
    auto& state = tower.post_elevator.version_18_dd6c;
    assert(state.size() >= 8U);
    const int world_x = 2860;
    const int world_y = 2196;
    state[0] = std::byte{0};
    const auto baseline = simtower::render_original_world(
        resources, &tower, world_x, world_y, 140, 48);

    state[0] = std::byte{1};
    state[4] = std::byte{0x2c};
    state[5] = std::byte{0x0b};
    state[6] = std::byte{0x94};
    state[7] = std::byte{0x08};
    const auto rendered = simtower::render_original_world(
        resources, &tower, world_x, world_y, 140, 48);
    const auto graphic = resources.find("BITMAP", 904);
    bool saw_transparent = false;
    bool saw_opaque = false;
    for (int y = 0; y < 48; ++y) {
      for (int x = 0; x < 140; ++x) {
        const auto index = dib_palette_index(graphic, x, y);
        const auto expected = index == 0U
            ? baseline.at(x, y)
            : clut_pixel(clut, index);
        assert(rendered.at(x, y) == expected);
        saw_transparent |= index == 0U;
        saw_opaque |= index != 0U;
      }
    }
    assert(saw_transparent && saw_opaque);

    // Direct 11b8:014b/020b coverage: the same source-relative selection is
    // retained when the left and top of the destination are clipped by the
    // visible viewport.
    state[4] = std::byte{93};
    state[5] = std::byte{0};
    state[6] = std::byte{93};
    state[7] = std::byte{0};
    state[0] = std::byte{0};
    const auto clipped_baseline = simtower::render_original_world(
        resources, &tower, 100, 100, 20, 10);
    state[0] = std::byte{1};
    const auto clipped = simtower::render_original_world(
        resources, &tower, 100, 100, 20, 10);
    for (int y = 0; y < 10; ++y) {
      for (int x = 0; x < 20; ++x) {
        const auto index = dib_palette_index(graphic, x + 7, y + 7);
        const auto expected = index == 0U
            ? clipped_baseline.at(x, y)
            : clut_pixel(clut, index);
        assert(clipped.at(x, y) == expected);
      }
    }
  }

  // Direct 10e8:04a0/10e8:0693 coverage uses the nine 12-cell frames at the
  // BITMAP/3944..3949 type-46 atlas. Direct 11a0:0a11/0cd9 coverage verifies
  // the selected 36-row slice and its independent nonzero-channel overlay:
  // fire is below Elevator/transport layers, while crew frame eight is above.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 220, 1, construction_costs())
               .succeeded());
    assert(simtower::build_original_floor(
               tower, 11, 100, 220, construction_costs())
               .succeeded());
    const int world_x = 120 * 8;
    const int world_y = (119 - 11) * 36;
    store_test_header_word(tower, 60U, 0U);
    store_test_header_word(tower, 72U, 0U);
    store_test_header_word(tower, 76U, 11U);
    store_test_header_word(tower, 78U, 0U);
    for (std::size_t floor = 0; floor < tower.floors.size(); ++floor) {
      store_test_header_word(tower, 80U + floor * 2U, 0xffffU);
      store_test_header_word(tower, 320U + floor * 2U, 0xffffU);
    }
    tower.header.frame_time = 6U;
    const auto baseline = simtower::render_original_world(
        resources, &tower, world_x, world_y, 96, 36);

    store_test_header_word(tower, 60U, 8U);
    store_test_header_word(tower, 320U + 11U * 2U, 120U);
    const auto fire = simtower::render_original_world(
        resources, &tower, world_x, world_y, 96, 36);
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 96; ++x) {
        const auto source = horizontal_atlas_pixel(
            resources, base_palette, 3944, 3949, 2 * 96 + x, y);
        assert(fire.at(x, y) ==
               merge_nonzero_channels(source, baseline.at(x, y)));
      }
    }

    // A nonzero b412 selects the parallel frames 4..7.
    store_test_header_word(tower, 72U, 1U);
    const auto secom_fire = simtower::render_original_world(
        resources, &tower, world_x, world_y, 96, 36);
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 96; ++x) {
        const auto source = horizontal_atlas_pixel(
            resources, base_palette, 3944, 3949, 6 * 96 + x, y);
        assert(secom_fire.at(x, y) ==
               merge_nonzero_channels(source, baseline.at(x, y)));
      }
    }

    // With no fire bands, b418>0 draws the fixed crew frame eight.
    store_test_header_word(tower, 320U + 11U * 2U, 0xffffU);
    store_test_header_word(tower, 78U, 120U);
    const auto crew = simtower::render_original_world(
        resources, &tower, world_x, world_y, 96, 36);
    for (int y = 0; y < 36; ++y) {
      for (int x = 0; x < 96; ++x) {
        const auto source = horizontal_atlas_pixel(
            resources, base_palette, 3944, 3949, 8 * 96 + x, y);
        assert(crew.at(x, y) ==
               merge_nonzero_channels(source, baseline.at(x, y)));
      }
    }
  }

  // Direct 11d0:0072/0145/0363/04ba and 11e0:0efb coverage: the parent paints
  // facilities before the selected Map overlay from the four exact 8x36
  // strips in BITMAP/1003. Verify
  // selection, ordinary/special geometry, repetition, and clipped phase
  // without opening either native or emulated UI.
  {
    auto tower = simtower::make_original_new_tdt();
    constexpr std::size_t kFloor = 10U;
    auto& tenants = tower.floors[kFloor].tenants;
    tenants.clear();
    simtower::OriginalTdtTenant tenant{};
    tenant.left = 10U;
    tenant.right = 14U;
    tenant.type = 24;
    tenant.status = 0U;
    tenant.rent_rate = 2U;
    tenant.exact_bytes[15] = std::byte{0};
    tenants.push_back(tenant);

    const auto palette = simtower::original_world_palette(resources, &tower);
    const auto strips = resources.find("BITMAP", 1003);
    constexpr std::uint32_t kSentinel = 0x00123456U;
    constexpr int kFloorTop = (119 - static_cast<int>(kFloor)) * 36;
    simtower::OriginalWorldRaster overlay{32, 36,
        std::vector<std::uint32_t>(32U * 36U, kSentinel)};

    simtower::composite_original_world_map_overlay(
        resources, tower, palette, 0U, 80, kFloorTop, overlay);
    assert(std::all_of(overlay.pixels.begin(), overlay.pixels.end(),
                       [](std::uint32_t pixel) {
                         return pixel == kSentinel;
                       }));

    simtower::composite_original_world_map_overlay(
        resources, tower, palette, 1U, 80, kFloorTop, overlay);
    assert(overlay.at(0, 11) == kSentinel);
    assert(overlay.at(0, 12) ==
           indexed_palette_pixel(strips, palette, 24, 0));
    assert(overlay.at(8, 12) ==
           indexed_palette_pixel(strips, palette, 24, 0));
    assert(overlay.at(31, 35) ==
           indexed_palette_pixel(strips, palette, 31, 23));

    std::fill(overlay.pixels.begin(), overlay.pixels.end(), kSentinel);
    tenants.front().exact_bytes[15] = std::byte{1};
    simtower::composite_original_world_map_overlay(
        resources, tower, palette, 1U, 80, kFloorTop, overlay);
    assert(overlay.at(0, 12) ==
           indexed_palette_pixel(strips, palette, 16, 0));

    std::fill(overlay.pixels.begin(), overlay.pixels.end(), kSentinel);
    tenants.front().exact_bytes[15] = std::byte{2};
    simtower::composite_original_world_map_overlay(
        resources, tower, palette, 1U, 80, kFloorTop, overlay);
    assert(overlay.at(0, 12) ==
           indexed_palette_pixel(strips, palette, 0, 0));

    std::fill(overlay.pixels.begin(), overlay.pixels.end(), kSentinel);
    simtower::composite_original_world_map_overlay(
        resources, tower, palette, 2U, 80, kFloorTop, overlay);
    assert(overlay.at(0, 12) ==
           indexed_palette_pixel(strips, palette, 8, 0));

    std::fill(overlay.pixels.begin(), overlay.pixels.end(), kSentinel);
    tenants.front().type = 3;
    tenants.front().status = 0x28U;
    simtower::composite_original_world_map_overlay(
        resources, tower, palette, 3U, 80, kFloorTop, overlay);
    assert(overlay.at(0, 11) == kSentinel);
    assert(overlay.at(0, 12) ==
           indexed_palette_pixel(strips, palette, 24, 0));
    std::fill(overlay.pixels.begin(), overlay.pixels.end(), kSentinel);
    tenants.front().status = 0x27U;
    simtower::composite_original_world_map_overlay(
        resources, tower, palette, 3U, 80, kFloorTop, overlay);
    assert(std::all_of(overlay.pixels.begin(), overlay.pixels.end(),
                       [](std::uint32_t pixel) {
                         return pixel == kSentinel;
                       }));

    std::fill(overlay.pixels.begin(), overlay.pixels.end(), kSentinel);
    tenants.front().type = 19;
    tenants.front().exact_bytes[15] = std::byte{0};
    simtower::composite_original_world_map_overlay(
        resources, tower, palette, 1U, 80, kFloorTop, overlay);
    assert(overlay.at(0, 0) ==
           indexed_palette_pixel(strips, palette, 24, 0));
    assert(overlay.at(31, 35) ==
           indexed_palette_pixel(strips, palette, 31, 35));

    std::fill(overlay.pixels.begin(), overlay.pixels.end(), kSentinel);
    tenants.front().type = 24;
    simtower::composite_original_world_map_overlay(
        resources, tower, palette, 1U, 84, kFloorTop + 16, overlay);
    assert(overlay.at(0, 0) ==
           indexed_palette_pixel(strips, palette, 28, 4));
  }

  // 10c0:002e raises 1090:03ab's shared repaint flag for used animated
  // Stairs/Escalators. Its ADD is 16-bit, so ffff+1 deliberately tests zero.
  {
    simtower::OriginalTdtDocument tower{};
    auto& first = tower.post_elevator.stairs_bd70[0];
    first.word_6 = 1U;
    assert(!simtower::original_vertical_transport_animation_active(tower));

    first.used = 1U;
    first.word_6 = 0U;
    assert(!simtower::original_vertical_transport_animation_active(tower));

    first.word_6 = 1U;
    assert(simtower::original_vertical_transport_animation_active(tower));

    first.word_6 = 0xffffU;
    first.word_8 = 1U;
    assert(!simtower::original_vertical_transport_animation_active(tower));

    first.word_6 = 0U;
    first.word_8 = 1U;
    assert(simtower::original_vertical_transport_animation_active(tower));

    first.used = 0U;
    auto& last = tower.post_elevator.stairs_bd70.back();
    last.used = 1U;
    last.word_8 = 2U;
    assert(simtower::original_vertical_transport_animation_active(tower));
  }

  // Direct 1208:0051 coverage inside 11f8:0000/3da4 defines the edit-surface
  // construction footprint and its signed 8x36 snap. Verify ordinary tools,
  // both single-cell/Lobby special
  // widths, every selectable multi-floor family, the type-18 scratch-width
  // quirk, and negative-coordinate IDIV semantics without opening a UI.
  {
    using Rect = simtower::OriginalConstructionPreviewRect;
    assert(simtower::original_construction_preview_rect(
               7U, 100, 50, 8, 36) ==
           Rect({64, 48, 136, 84}));
    assert(simtower::original_construction_preview_rect(
               0U, 15, 15, 0, 0) ==
           Rect({8, 12, 16, 48}));
    assert(simtower::original_construction_preview_rect(
               24U, 20, 15, 0, 0) ==
           Rect({0, 0, 32, 36}));
    assert(simtower::original_construction_preview_rect(
               18U, 200, 50, 0, 0) ==
           Rect({72, 48, 320, 120}));
    assert(simtower::original_construction_preview_rect(
               20U, 200, 50, 0, 0) ==
           Rect({96, 48, 296, 120}));
    assert(simtower::original_construction_preview_rect(
               22U, 200, 50, 0, 0) ==
           Rect({168, 48, 232, 120}));
    assert(simtower::original_construction_preview_rect(
               27U, 200, 50, 0, 0) ==
           Rect({168, 48, 232, 120}));
    assert(simtower::original_construction_preview_rect(
               29U, 200, 50, 0, 0) ==
           Rect({104, 48, 296, 120}));
    assert(simtower::original_construction_preview_rect(
               31U, 200, 50, 0, 0) ==
           Rect({80, 48, 320, 156}));
    assert(simtower::original_construction_preview_rect(
               36U, 200, 50, 0, 0) ==
           Rect({88, 48, 312, 228}));
    assert(simtower::original_construction_preview_rect(
               48U, 200, 50, 0, 0) ==
           Rect({168, 48, 232, 120}));
    assert(simtower::original_construction_preview_rect(
               20U, 92, -48, 0, 0) ==
           Rect({-8, -24, 192, 48}));
    assert(!simtower::original_construction_preview_rect(
        25U, 0, 0, 0, 0));
    assert(!simtower::original_construction_preview_rect(
        49U, 0, 0, 0, 0));

    // 1090:03ab compares these derived DS:77ac rectangles after every
    // eligible idle callback. A fixed client point is insufficient: changing
    // either the selected footprint or an off-grid viewport changes the
    // rectangle and must trigger the preview-only presentation.
    assert(simtower::original_construction_preview_rect(
               18U, 200, 50, 0, 0) !=
           simtower::original_construction_preview_rect(
               20U, 200, 50, 0, 0));
    assert(simtower::original_construction_preview_rect(
               20U, 200, 50, 0, 0) !=
           simtower::original_construction_preview_rect(
               20U, 200, 50, 0, 1));
  }

  // 11f8:3c13 intersects the preview with the client before calling GDI
  // Rectangle with WHITE_PEN and NULL_BRUSH. Verify hollow pixels and the
  // otherwise-surprising white border introduced at every clipped edge.
  {
    constexpr std::uint32_t kSentinel = 0x00123456U;
    constexpr std::uint32_t kWhite = 0x00ffffffU;
    simtower::OriginalWorldRaster raster{
        160, 100, std::vector<std::uint32_t>(160U * 100U, kSentinel)};
    simtower::composite_original_construction_preview(
        7U, 100, 50, 8, 36, raster);
    for (int x = 64; x < 136; ++x) {
      assert(raster.at(x, 48) == kWhite);
      assert(raster.at(x, 83) == kWhite);
    }
    for (int y = 48; y < 84; ++y) {
      assert(raster.at(64, y) == kWhite);
      assert(raster.at(135, y) == kWhite);
    }
    assert(raster.at(65, 49) == kSentinel);
    assert(raster.at(63, 48) == kSentinel);
    assert(raster.at(136, 83) == kSentinel);

    simtower::OriginalWorldRaster clipped{
        40, 30, std::vector<std::uint32_t>(40U * 30U, kSentinel)};
    simtower::composite_original_construction_preview(
        20U, 92, -48, 0, 0, clipped);
    for (int x = 0; x < 40; ++x) {
      assert(clipped.at(x, 0) == kWhite);
      assert(clipped.at(x, 29) == kWhite);
    }
    for (int y = 0; y < 30; ++y) {
      assert(clipped.at(0, y) == kWhite);
      assert(clipped.at(39, y) == kWhite);
    }
    assert(clipped.at(1, 1) == kSentinel);
  }

  return 0;
}
