#include "original_command_palette.hpp"

#include "original_dib.hpp"
#include "original_tables.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <span>
#include <stdexcept>

namespace simtower {

OriginalCommandPointerPlan original_command_pointer_plan(
    OriginalCommandPointerPhase phase,
    int x,
    int y) noexcept {
  const bool close = original_palette_frame_close_rect().contains(x, y);
  if (phase == OriginalCommandPointerPhase::button_down) {
    if (close) {
      return {.close_palette = true};
    }
    if (original_command_toggle_rect().contains(x, y)) {
      return {.press_toggle = true, .sample_coarse_tick = true};
    }
    return {.activate_point = true, .sample_coarse_tick = true};
  }
  return {.restore_toggle = true, .activate_point = !close};
}

namespace {

class IndexedDib {
 public:
  explicit IndexedDib(std::span<const std::byte> resource)
      : view_(original_dib_view(resource)),
        height_(std::abs(view_.height)),
        row_bytes_((static_cast<std::size_t>(view_.width) + 3U) & ~3U) {
    if (view_.bit_count != 8U) {
      throw std::runtime_error("Original command bitmap is not 8-bit indexed");
    }
  }

  [[nodiscard]] int width() const noexcept { return view_.width; }
  [[nodiscard]] int height() const noexcept { return height_; }

  [[nodiscard]] std::uint32_t sample(int x, int y) const {
    if (x < 0 || y < 0 || x >= width() || y >= height()) {
      throw std::out_of_range("Original command bitmap sample is out of range");
    }
    const int source_y = view_.height > 0 ? height_ - 1 - y : y;
    const auto index = std::to_integer<std::uint8_t>(
        view_.pixels[static_cast<std::size_t>(source_y) * row_bytes_ +
                     static_cast<std::size_t>(x)]);
    const RGBQUAD color = view_.info->bmiColors[index];
    return (static_cast<std::uint32_t>(color.rgbRed) << 16U) |
           (static_cast<std::uint32_t>(color.rgbGreen) << 8U) |
           static_cast<std::uint32_t>(color.rgbBlue);
  }

 private:
  OriginalDibView view_{};
  int height_{};
  std::size_t row_bytes_{};
};

void copy_pixels(const IndexedDib& source,
                 int source_x,
                 int source_y,
                 int width,
                 int height,
                 int destination_x,
                 int destination_y,
                 OriginalCommandRaster& destination) {
  // Native clipped equivalent of 1058:0895 -> 1208:069a -> 1248:0000 ->
  // 1250:0114. The -1 transparency marker selects the equal-size opaque byte
  // blitter.
  const auto plan = original_opaque_blit_plan(
      destination.width, destination.height, source_x, source_y,
      width, height, destination_x, destination_y);
  if (!plan.valid) return;
  for (int y = 0; y < plan.height; ++y) {
    const int output_y = plan.destination_y + y;
    for (int x = 0; x < plan.width; ++x) {
      const int output_x = plan.destination_x + x;
      destination.pixels[static_cast<std::size_t>(output_y) *
                             destination.width + output_x] =
          source.sample(plan.source_x + x, plan.source_y + y);
    }
  }
}

void fill_pixels(OriginalCommandRaster& raster,
                 int left,
                 int top,
                 int width,
                 int height,
                 std::uint32_t color) {
  const int first_x = std::max(0, left);
  const int first_y = std::max(0, top);
  const int last_x = std::min(raster.width, left + width);
  const int last_y = std::min(raster.height, top + height);
  for (int y = first_y; y < last_y; ++y) {
    std::fill_n(raster.pixels.begin() +
                    static_cast<std::size_t>(y) * raster.width + first_x,
                std::max(0, last_x - first_x), color);
  }
}

}  // namespace

std::uint32_t OriginalCommandRaster::at(int x, int y) const {
  if (x < 0 || y < 0 || x >= width || y >= height) {
    throw std::out_of_range("Original command raster coordinate is outside the image");
  }
  return pixels[static_cast<std::size_t>(y) * width + x];
}

int original_command_selector_top(int anchor_top,
                                  std::uint16_t one_based_choice,
                                  std::size_t icon_count,
                                  int desktop_bottom) noexcept {
  // 1050:0638-0671 performs these adjustments sequentially: align the
  // selected 32-pixel row, clamp a negative top to zero, then move the whole
  // popup upward if its bottom would extend past the desktop.
  int top = anchor_top -
            (static_cast<int>(one_based_choice) - 1) * 32;
  if (top < 0) top = 0;
  const int height = static_cast<int>(icon_count) * 32;
  if (height + top > desktop_bottom) {
    top -= height + top - desktop_bottom;
  }
  return top;
}

std::vector<std::uint16_t> original_command_catalog(
    const OriginalResources& resources,
    std::uint16_t rating) {
  return original_command_catalog(
      resources, original_command_rating_state(resources, rating));
}

OriginalCommandRatingState original_command_rating_state(
    const OriginalResources& resources,
    std::uint16_t rating) {
  const std::uint16_t bounded_rating = std::clamp<std::uint16_t>(rating, 1U, 6U);
  return {bounded_rating,
          original_word_table(resources.find("TABL", 1000 + bounded_rating))};
}

std::optional<OriginalCommandGroup> original_command_group(
    const OriginalResources& resources,
    const OriginalCommandRatingState& state,
    std::size_t catalog_index) {
  if (catalog_index >= state.encoded_entries.size()) {
    throw std::out_of_range("Original command catalog index is out of range");
  }
  // Exact 1140:02ee/0341/04c5 resource transaction: the high byte names the
  // TABM, the low byte is its one-based choice, and the prior TABM handle is
  // replaced by a value view of TABM/(1000+number).
  const std::uint16_t encoded = state.encoded_entries[catalog_index];
  const std::uint16_t tabm_number = encoded >> 8U;
  if (tabm_number == 0U) {
    return std::nullopt;
  }
  OriginalCommandGroup group{};
  group.tabm_number = tabm_number;
  group.selection_index = encoded & 0xFFU;
  group.catalog_icons = original_word_table(
      resources.find("TABM", static_cast<int>(1000U + tabm_number)));
  if (group.selection_index == 0U ||
      group.selection_index > group.catalog_icons.size()) {
    throw std::out_of_range("Original command TABM choice is out of range");
  }
  return group;
}

std::vector<std::uint16_t> original_command_catalog(
    const OriginalResources& resources,
    const OriginalCommandRatingState& state) {
  std::vector<std::uint16_t> result;
  result.reserve(state.encoded_entries.size());
  for (std::size_t index = 0; index < state.encoded_entries.size(); ++index) {
    const auto group = original_command_group(resources, state, index);
    if (group) {
      result.push_back(group->catalog_icons[group->selection_index - 1U]);
    } else {
      result.push_back(state.encoded_entries[index]);
    }
  }
  return result;
}

void original_command_select_group_choice(
    const OriginalResources& resources,
    OriginalCommandRatingState& state,
    std::size_t catalog_index,
    std::uint16_t one_based_choice) {
  const auto group = original_command_group(resources, state, catalog_index);
  if (!group) {
    throw std::invalid_argument("Original command entry has no TABM choices");
  }
  if (one_based_choice == 0U ||
      one_based_choice > group->catalog_icons.size()) {
    throw std::out_of_range("Original command TABM choice is out of range");
  }
  state.encoded_entries[catalog_index] = static_cast<std::uint16_t>(
      (group->tabm_number << 8U) | one_based_choice);
}

std::uint16_t original_command_build_type(
    const OriginalResources& resources,
    std::uint16_t catalog_icon) {
  const auto mapping = resources.find("TABL", 1000);
  if (catalog_icon >= 45U || catalog_icon >= mapping.size()) {
    throw std::out_of_range("Original command icon mapping is out of range");
  }
  return std::to_integer<std::uint8_t>(mapping[catalog_icon]);
}

OriginalCommandHit original_command_hit_test(
    const OriginalResources& resources,
    std::uint16_t rating,
    bool build_enabled,
    int x,
    int y) {
  return original_command_hit_test(
      resources, original_command_rating_state(resources, rating),
      build_enabled, x, y);
}

OriginalCommandHit original_command_hit_test(
    const OriginalResources& resources,
    const OriginalCommandRatingState& state,
    bool build_enabled,
    int x,
    int y) {
  if (original_command_toggle_rect().contains(x, y)) {
    return {OriginalCommandHitKind::build_toggle, 0U, 0U};
  }
  if (!build_enabled) {
    return {};
  }
  for (std::uint16_t mode = 0; mode < 3U; ++mode) {
    if (original_command_mode_rect(mode).contains(x, y)) {
      return {OriginalCommandHitKind::edit_mode, mode, 0U};
    }
  }
  const auto catalog = original_command_catalog(resources, state);
  for (std::size_t index = 0; index < catalog.size(); ++index) {
    if (original_command_facility_rect(static_cast<std::uint16_t>(index))
            .contains(x, y)) {
      return {OriginalCommandHitKind::facility,
              static_cast<std::uint16_t>(index + 3U),
              original_command_build_type(resources, catalog[index])};
    }
  }
  return {};
}

OriginalCommandRaster render_original_command_palette(
    const OriginalResources& resources,
    std::uint16_t rating,
    bool build_enabled,
    std::uint16_t selected_mode,
    int client_width,
    int client_height,
    bool build_toggle_pressed) {
  return render_original_command_palette(
      resources, original_command_rating_state(resources, rating),
      build_enabled, selected_mode, client_width, client_height,
      build_toggle_pressed);
}

OriginalCommandRaster render_original_command_palette(
    const OriginalResources& resources,
    const OriginalCommandRatingState& state,
    bool build_enabled,
    std::uint16_t selected_mode,
    int client_width,
    int client_height,
    bool build_toggle_pressed) {
  OriginalCommandRaster raster{};
  raster.pixels.assign(
      static_cast<std::size_t>(raster.width) * raster.height, 0U);

  // 1080:07a6 selects the complete 64x32 header on button transitions:
  // 600/601 are disabled normal/pressed, 602/603 enabled normal/pressed.
  const int header_id = (build_enabled ? 602 : 600) +
                        (build_toggle_pressed ? 1 : 0);
  const IndexedDib header(resources.find("BITMAP", header_id));
  const IndexedDib modes(
      resources.find("BITMAP", build_enabled ? 604 : 606));
  // 1058:0803 constructs the 64x32 header source rectangle and 1058:0773
  // constructs the 64x21 mode-strip rectangle immediately below it.
  copy_pixels(header, 0, 0, 64, 32, 0, 0, raster);
  copy_pixels(modes, 0, 0, 64, 21, 0, 32, raster);

  const auto catalog = original_command_catalog(resources, state);
  if ((catalog.size() & 1U) != 0U) {
    // 1080:067c-06cb uses the 16-bit Mac-style RGB value 0xd998 in all
    // channels. The original logical palette maps it to index 2, #d9d9d9.
    fill_pixels(raster, client_width - 31, client_height - 39, 31, 31,
                0x00d9d9d9U);
  }

  const IndexedDib normal_icons(resources.find("BITMAP", 300));
  const IndexedDib selected_icons(resources.find("BITMAP", 301));
  const IndexedDib disabled_icons(resources.find("BITMAP", 302));
  // 1080:0884 chooses the normal/selected/disabled icon sheet, converts the
  // catalog index into a 32-pixel eight-column cell, and copies that RECT.
  for (std::size_t index = 0; index < catalog.size(); ++index) {
    const std::uint16_t absolute_mode =
        static_cast<std::uint16_t>(index + 3U);
    const IndexedDib& sheet =
        !build_enabled ? disabled_icons
                       : (selected_mode == absolute_mode ? selected_icons
                                                         : normal_icons);
    const int type = catalog[index];
    const int source_x = (type % 8) * 32;
    const int source_y = (type / 8) * 32;
    const int destination_x = static_cast<int>(index & 1U) * 32;
    const int destination_y = 53 + static_cast<int>(index / 2U) * 32;
    copy_pixels(sheet, source_x, source_y, 32, 32,
                destination_x, destination_y, raster);
  }

  if (build_enabled && selected_mode < 3U) {
    const IndexedDib selected_modes(resources.find("BITMAP", 605));
    const int x = static_cast<int>(selected_mode) * 21;
    copy_pixels(selected_modes, x, 0, 21, 21, x, 32, raster);
  }
  return raster;
}

OriginalCommandRaster render_original_command_selector(
    const OriginalResources& resources,
    const OriginalCommandGroup& group,
    std::uint16_t selected_icon_plus_one,
    bool build_enabled) {
  OriginalCommandRaster raster{};
  raster.width = 32;
  raster.height = static_cast<int>(group.catalog_icons.size()) * 32;
  raster.pixels.assign(
      static_cast<std::size_t>(raster.width) * raster.height, 0U);

  const IndexedDib normal_icons(resources.find("BITMAP", 300));
  const IndexedDib selected_icons(resources.find("BITMAP", 301));
  const IndexedDib disabled_icons(resources.find("BITMAP", 302));
  for (std::size_t index = 0; index < group.catalog_icons.size(); ++index) {
    const std::uint16_t icon = group.catalog_icons[index];
    const IndexedDib& sheet =
        !build_enabled ? disabled_icons
                       : (icon + 1U == selected_icon_plus_one
                              ? selected_icons
                              : normal_icons);
    copy_pixels(sheet, (icon % 8U) * 32, (icon / 8U) * 32, 32, 32,
                0, static_cast<int>(index) * 32, raster);
  }
  return raster;
}

void draw_original_command_raster(HDC destination,
                                  const OriginalCommandRaster& raster,
                                  int x,
                                  int y) {
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
    throw std::runtime_error("SetDIBitsToDevice failed for original command palette");
  }
}

}  // namespace simtower
