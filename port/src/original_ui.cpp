#include "original_ui.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace simtower {
namespace {

class Reader {
 public:
  explicit Reader(std::span<const std::byte> data) : data_(data) {}

  [[nodiscard]] std::size_t position() const { return position_; }
  [[nodiscard]] std::size_t remaining() const { return data_.size() - position_; }

  std::uint8_t u8() {
    require(1);
    return static_cast<std::uint8_t>(data_[position_++]);
  }

  std::uint16_t u16() {
    require(2);
    const auto result = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data_[position_]) |
        (static_cast<std::uint16_t>(data_[position_ + 1]) << 8U));
    position_ += 2;
    return result;
  }

  std::uint32_t u32() {
    const std::uint32_t low = u16();
    return low | (static_cast<std::uint32_t>(u16()) << 16U);
  }

  void skip(std::size_t count) {
    require(count);
    position_ += count;
  }

  std::string cstring() {
    std::string result;
    while (true) {
      const auto character = u8();
      if (character == 0) {
        return result;
      }
      result.push_back(static_cast<char>(character));
    }
  }

 private:
  void require(std::size_t count) const {
    if (count > data_.size() - position_) {
      throw std::runtime_error("Truncated original Win16 UI resource");
    }
  }

  std::span<const std::byte> data_;
  std::size_t position_ = 0;
};

std::wstring cp1252_to_wide(std::string_view value) {
  if (value.empty()) {
    return {};
  }
  const int length = MultiByteToWideChar(
      1252, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
  if (length <= 0) {
    throw std::runtime_error("Could not decode original CP1252 menu text");
  }
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(1252, 0, value.data(), static_cast<int>(value.size()),
                      result.data(), length);
  return result;
}

void append_menu_level(Reader& reader, HMENU parent) {
  while (true) {
    const std::uint16_t original_flags = reader.u16();
    const bool popup = (original_flags & 0x0010U) != 0;
    const bool last = (original_flags & 0x0080U) != 0;
    const std::uint16_t command = popup ? 0U : reader.u16();
    const std::wstring text = cp1252_to_wide(reader.cstring());

    UINT native_flags = MF_STRING;
    native_flags |= original_flags & (MF_GRAYED | MF_DISABLED | MF_CHECKED |
                                       MF_MENUBREAK | MF_MENUBARBREAK |
                                       MF_HELP | MF_RIGHTJUSTIFY);
    if (popup) {
      const HMENU child = CreatePopupMenu();
      if (!child) {
        throw std::runtime_error("CreatePopupMenu failed");
      }
      try {
        append_menu_level(reader, child);
        if (!AppendMenuW(parent, native_flags | MF_POPUP,
                         reinterpret_cast<UINT_PTR>(child), text.c_str())) {
          throw std::runtime_error("AppendMenuW failed for original popup");
        }
      } catch (...) {
        DestroyMenu(child);
        throw;
      }
    } else {
      if (command == 0U && text.empty()) {
        native_flags = MF_SEPARATOR;
      }
      if (!AppendMenuW(parent, native_flags, command, text.c_str())) {
        throw std::runtime_error("AppendMenuW failed for original command");
      }
    }

    if (last) {
      return;
    }
  }
}

}  // namespace

void fill_original_white_rect(HDC dc, const RECT& rectangle) noexcept {
  if (!dc) return;
  HBRUSH brush = CreateSolidBrush(static_cast<COLORREF>(0x00ffffffU));
  if (!brush) return;
  FillRect(dc, &rectangle, brush);
  DeleteObject(brush);
}

HMENU create_original_menu(std::span<const std::byte> resource) {
  Reader reader(resource);
  const std::uint16_t version = reader.u16();
  const std::uint16_t header_size = reader.u16();
  if (version != 0U) {
    throw std::runtime_error("Unsupported original Win16 menu version");
  }
  reader.skip(header_size);

  const HMENU menu = CreateMenu();
  if (!menu) {
    throw std::runtime_error("CreateMenu failed");
  }
  try {
    append_menu_level(reader, menu);
    return menu;
  } catch (...) {
    DestroyMenu(menu);
    throw;
  }
}

HACCEL create_original_accelerators(std::span<const std::byte> resource) {
  Reader reader(resource);
  std::vector<ACCEL> entries;
  while (reader.remaining() >= 5U) {
    const std::uint8_t original_flags = reader.u8();
    ACCEL entry{};
    entry.fVirt = static_cast<BYTE>(original_flags & 0x7FU);
    entry.key = reader.u16();
    entry.cmd = reader.u16();
    entries.push_back(entry);
    if ((original_flags & 0x80U) != 0) {
      break;
    }
  }
  if (entries.empty()) {
    throw std::runtime_error("Invalid original accelerator resource");
  }
  const HACCEL table = CreateAcceleratorTableW(
      entries.data(), static_cast<int>(entries.size()));
  if (!table) {
    throw std::runtime_error("CreateAcceleratorTableW failed");
  }
  return table;
}

HICON create_original_icon(const OriginalResources& resources,
                           std::string_view group_name) {
  Reader group(resources.find("GROUP_ICON", group_name));
  if (group.u16() != 0U || group.u16() != 1U) {
    throw std::runtime_error("Invalid original group-icon header");
  }
  const std::uint16_t count = group.u16();
  if (count == 0U) {
    throw std::runtime_error("Original group icon is empty");
  }

  const int width = group.u8();
  const int height = group.u8();
  group.u8();   // color count
  group.u8();   // reserved
  group.u16();  // planes
  group.u16();  // bit depth
  const std::uint32_t declared_size = group.u32();
  const std::uint16_t resource_id = group.u16();

  auto image = resources.find("ICON", resource_id);
  if (declared_size > image.size()) {
    throw std::runtime_error("Truncated original icon image");
  }
  image = image.first(declared_size);
  HICON icon = CreateIconFromResourceEx(
      reinterpret_cast<PBYTE>(const_cast<std::byte*>(image.data())),
      static_cast<DWORD>(image.size()), TRUE, 0x00030000,
      width == 0 ? 256 : width, height == 0 ? 256 : height,
      LR_DEFAULTCOLOR);
  if (!icon) {
    throw std::runtime_error("CreateIconFromResourceEx rejected original icon");
  }
  return icon;
}

HCURSOR create_original_cursor(const OriginalResources& resources,
                               std::uint16_t group_id) {
  Reader group(resources.find("GROUP_CURSOR", group_id));
  if (group.u16() != 0U || group.u16() != 2U) {
    throw std::runtime_error("Invalid original group-cursor header");
  }
  const std::uint16_t count = group.u16();
  if (count == 0U) {
    throw std::runtime_error("Original group cursor is empty");
  }

  const std::uint16_t width = group.u16();
  const std::uint16_t mask_height = group.u16();
  group.u16();  // planes
  group.u16();  // bit depth
  const std::uint32_t declared_size = group.u32();
  const std::uint16_t resource_id = group.u16();
  auto image = resources.find("CURSOR", resource_id);
  if (declared_size > image.size()) {
    throw std::runtime_error("Truncated original cursor image");
  }
  image = image.first(declared_size);
  HCURSOR cursor = static_cast<HCURSOR>(CreateIconFromResourceEx(
      reinterpret_cast<PBYTE>(const_cast<std::byte*>(image.data())),
      static_cast<DWORD>(image.size()), FALSE, 0x00030000,
      width, mask_height / 2U, LR_DEFAULTCOLOR));
  if (!cursor) {
    throw std::runtime_error(
        "CreateIconFromResourceEx rejected original cursor");
  }
  return cursor;
}

std::optional<int> original_main_scroll_request_position(
    std::uint16_t scroll_code,
    int current_position,
    std::uint16_t message_position,
    int client_extent,
    int maximum_position) noexcept {
  int requested_position{};
  switch (scroll_code) {
    case 0U:
      requested_position = current_position - 16;
      break;
    case 1U:
      requested_position = current_position + 16;
      break;
    case 2U:
      requested_position = current_position - (client_extent - 16);
      break;
    case 3U:
      requested_position = current_position + (client_extent - 16);
      break;
    case 4U:
    case 5U:
      // 1158:01a6/023a load the Win16 lParam position directly into DX and
      // bypass the line/page clamp before calling SetScrollPos.
      return static_cast<int>(message_position);
    default:
      return std::nullopt;
  }
  return std::clamp(requested_position, 0, std::max(0, maximum_position));
}

OriginalMainScrollbarResizeState original_main_scrollbar_resize_state(
    int saved_position,
    int client_extent,
    int world_extent) noexcept {
  const int maximum = std::max(0, world_extent - client_extent);
  return {
      .minimum = 0,
      .maximum = maximum,
      .native_page_size = 0U,
      .position = std::clamp(saved_position, 0, maximum),
  };
}

OriginalMainWindowGeometry original_main_window_geometry(
    int desktop_right,
    int desktop_bottom,
    OriginalMainNonclientMetrics metrics) noexcept {
  constexpr int minimum_client_width = 100;
  constexpr int minimum_client_height = 100;
  constexpr int maximum_client_width_cap = 816;
  constexpr int maximum_client_height_cap = 576;
  constexpr int initial_x = 204;
  constexpr int initial_y = 53;

  const int maximum_client_width = std::min(
      desktop_right - metrics.vertical_scroll_width,
      maximum_client_width_cap);
  const int maximum_client_height = std::min(
      desktop_bottom - metrics.caption_height -
          metrics.horizontal_scroll_height - metrics.menu_height,
      maximum_client_height_cap);

  int initial_right = std::min(initial_x + 1000, desktop_right);
  int initial_bottom = std::min(initial_y + 1000, desktop_bottom);
  if (initial_bottom - initial_y > maximum_client_height - 1) {
    initial_bottom = initial_y + maximum_client_height - 1;
  }
  if (initial_right - initial_x > maximum_client_width - 1) {
    initial_right = initial_x + maximum_client_width - 1;
  }

  return {
      .initial_x = initial_x,
      .initial_y = initial_y,
      .initial_width = initial_right - initial_x,
      .initial_height = initial_bottom - initial_y,
      .minimum_track_width =
          2 * metrics.frame_width + minimum_client_width +
          metrics.vertical_scroll_width,
      .minimum_track_height =
          2 * metrics.frame_height + minimum_client_height +
          metrics.horizontal_scroll_height + 4 * metrics.menu_height +
          metrics.caption_height,
      .maximum_track_width =
          2 * metrics.frame_width + maximum_client_width +
          metrics.vertical_scroll_width - metrics.border_width,
      .maximum_track_height =
          2 * metrics.frame_height + maximum_client_height +
          metrics.horizontal_scroll_height + metrics.menu_height +
          metrics.caption_height - metrics.border_height,
  };
}

std::vector<OriginalAuxiliaryWindowAction>
original_auxiliary_window_actions(
    bool restore,
    bool command_visible,
    bool info_visible,
    bool map_visible) {
  std::vector<OriginalAuxiliaryWindowAction> actions(3U);
  std::size_t count{};
  const auto append = [&](OriginalAuxiliaryWindow target,
                          OriginalAuxiliaryWindowOperation operation,
                          OriginalAuxiliaryInsertAfter insert_after) {
    actions[count++] = {target, operation, insert_after};
  };
  if (restore) {
    if (command_visible) {
      append(OriginalAuxiliaryWindow::command,
             OriginalAuxiliaryWindowOperation::show,
             OriginalAuxiliaryInsertAfter::topmost);
    }
    const auto secondary_insert = command_visible
        ? OriginalAuxiliaryInsertAfter::command
        : OriginalAuxiliaryInsertAfter::top;
    if (info_visible) {
      append(OriginalAuxiliaryWindow::info,
             OriginalAuxiliaryWindowOperation::show,
             secondary_insert);
    }
    if (map_visible) {
      append(OriginalAuxiliaryWindow::map,
             OriginalAuxiliaryWindowOperation::show,
             secondary_insert);
    }
    actions.resize(count);
    return actions;
  }

  if (info_visible) {
    append(OriginalAuxiliaryWindow::info,
           OriginalAuxiliaryWindowOperation::hide,
           OriginalAuxiliaryInsertAfter::top);
  }
  if (command_visible) {
    append(OriginalAuxiliaryWindow::command,
           OriginalAuxiliaryWindowOperation::hide,
           OriginalAuxiliaryInsertAfter::top);
  }
  if (map_visible) {
    append(OriginalAuxiliaryWindow::map,
           OriginalAuxiliaryWindowOperation::hide,
           OriginalAuxiliaryInsertAfter::top);
  }
  actions.resize(count);
  return actions;
}

std::vector<OriginalAuxiliaryActivationAction>
original_auxiliary_activation_actions(
    bool active,
    bool command_visible,
    bool info_visible,
    bool map_visible) {
  std::vector<OriginalAuxiliaryActivationAction> actions;
  actions.reserve(3U);
  if (active) {
    // 1078:01fd-0251 deliberately targets the Command HWND for all three
    // visibility checks; the last enabled Command action makes it TOPMOST.
    if (map_visible) {
      actions.push_back({OriginalAuxiliaryWindow::command,
                         OriginalAuxiliaryActivationInsertAfter::top});
    }
    if (info_visible) {
      actions.push_back({OriginalAuxiliaryWindow::command,
                         OriginalAuxiliaryActivationInsertAfter::top});
    }
    if (command_visible) {
      actions.push_back({OriginalAuxiliaryWindow::command,
                         OriginalAuxiliaryActivationInsertAfter::topmost});
    }
    return actions;
  }

  if (command_visible) {
    actions.push_back({OriginalAuxiliaryWindow::command,
                       OriginalAuxiliaryActivationInsertAfter::main});
  }
  if (info_visible) {
    actions.push_back({OriginalAuxiliaryWindow::info,
                       OriginalAuxiliaryActivationInsertAfter::main});
  }
  if (map_visible) {
    actions.push_back({OriginalAuxiliaryWindow::map,
                       OriginalAuxiliaryActivationInsertAfter::main});
  }
  return actions;
}

OriginalPaletteWindowActivationPlan original_palette_window_activation_plan(
    OriginalAuxiliaryWindow window,
    bool active,
    bool modal_dialog_available,
    bool command_window_enabled,
    bool previous_shared_activation_latch) noexcept {
  OriginalPaletteWindowActivationPlan plan{};
  if (modal_dialog_available) {
    plan.insert_behind_modal = window == OriginalAuxiliaryWindow::map;
    if (active) {
      plan.activate_modal = true;
      plan.focus_modal = window == OriginalAuxiliaryWindow::command;
      return plan;
    }
    // CMDBTNWNDPROC 1050:006a jumps directly out for an inactive Command
    // while DS:31a4 exists; Info and Map continue to their local latch write.
    if (window == OriginalAuxiliaryWindow::command) return plan;
  }
  if (!modal_dialog_available &&
      window == OriginalAuxiliaryWindow::command &&
      command_window_enabled && previous_shared_activation_latch) {
    plan.promote_command_topmost = true;
    plan.command_promotion_no_activate = !active;
  }
  plan.validate_client = active && !modal_dialog_available;
  plan.write_shared_activation_latch = true;
  plan.shared_activation_latch = active;
  return plan;
}

std::vector<OriginalPaletteSurface> original_palette_repaint_order(
    bool runtime_initialized,
    bool realized_entries_changed) {
  if (!runtime_initialized || !realized_entries_changed) {
    return {};
  }
  return {
      OriginalPaletteSurface::map,
      OriginalPaletteSurface::info,
      OriginalPaletteSurface::command,
      OriginalPaletteSurface::main,
  };
}

std::vector<OriginalPaletteRepaintAction> original_palette_repaint_actions(
    bool runtime_initialized,
    bool realized_entries_changed,
    OriginalPaletteSurface source) {
  const auto order = original_palette_repaint_order(
      runtime_initialized, realized_entries_changed);
  std::vector<OriginalPaletteRepaintAction> actions;
  actions.reserve(order.size());
  for (const auto surface : order) {
    if (surface == OriginalPaletteSurface::info) {
      actions.push_back({
          .surface = surface,
          .mechanism = OriginalPaletteRepaintMechanism::update_window,
          .invalidate = true,
      });
      continue;
    }
    const bool reuse_source_dc = surface == source;
    actions.push_back({
        .surface = surface,
        .mechanism = OriginalPaletteRepaintMechanism::direct_dc,
        .invalidate = surface != OriginalPaletteSurface::command,
        .reuse_source_dc = reuse_source_dc,
        .select_and_realize = !reuse_source_dc,
        .release_dc = !reuse_source_dc,
    });
  }
  return actions;
}

}  // namespace simtower
