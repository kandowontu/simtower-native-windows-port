#include "original_startup.hpp"

#include "original_dib.hpp"

#include <algorithm>
#include <string>

namespace simtower {

std::string original_tdt_extension_profile_value(
    std::string_view module_filename) {
  return std::string(module_filename) + " ^.tdt";
}

std::string normalize_native_startup_command_line(
    std::string_view command_line) {
  if (command_line.size() >= 2U && command_line.front() == '"' &&
      command_line.back() == '"') {
    return std::string(command_line.substr(1U, command_line.size() - 2U));
  }
  return std::string(command_line);
}

std::optional<OriginalStartupCommandTarget>
original_startup_command_target(std::string_view module_directory,
                                std::string_view command_line) {
  if (command_line.empty()) return std::nullopt;
  const auto separator = command_line.find_last_of('\\');
  if (separator != std::string_view::npos) {
    return OriginalStartupCommandTarget{
        std::string(command_line),
        std::string(command_line.substr(separator + 1U))};
  }
  return OriginalStartupCommandTarget{
      std::string(module_directory) + std::string(command_line),
      std::string(command_line)};
}

OriginalStartupWindowSize original_startup_window_size(
    int desktop_right,
    int desktop_bottom,
    bool modal,
    int border_width,
    int border_height) noexcept {
  // 1010:014c SETUPSTARTUPDLGA, WM_INITDIALOG 0172-01d7.
  if (modal) {
    return {desktop_right + border_width * 2,
            desktop_bottom + border_height * 2};
  }
  // 1010:0304 SETUPSTARTUPDLGB, WM_INITDIALOG 0329-0370.
  return {desktop_right, desktop_bottom};
}

OriginalStartupBitmapPlacement original_startup_bitmap_placement(
    const OriginalResources& resources,
    int bitmap_id,
    int client_width,
    int client_height) {
  const auto dib = original_dib_view(resources.find("BITMAP", bitmap_id));
  const int width = dib.width;
  const int height = dib.height < 0 ? -dib.height : dib.height;
  // Both exported filters use signed division truncated toward zero and clamp
  // only negative positions, at 1010:0282-02ae and 1010:03f7-0423.
  return {std::max(0, (client_width - width) / 2),
          std::max(0, (client_height - height) / 2),
          width, height};
}

std::string original_startup_low_memory_message(
    std::uint32_t free_space_bytes) {
  // DS:0086 is a 300-byte copied prefix and DS:01b2 is the 143-byte suffix
  // format. 1128:1392 appends the formatted suffix at the prefix NUL.
  return "Windows doesn't have enough memory to run SimTower. SimTower for "
         "Windows needs at least " +
         std::to_string(kOriginalStartupRequiredMemoryKb) +
         "K of virtual memory to run.  SimTower found " +
         std::to_string(original_startup_memory_kb(free_space_bytes)) +
         "K bytes of virtual memory free. Please see the README file on how "
         "to correct this situation.";
}

}  // namespace simtower
