#pragma once

#include "original_resources.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace simtower {

struct OriginalAlert {
  std::uint16_t button_mode = 0;
  std::uint16_t preserved_word = 0;
  std::string message_template;
};

struct OriginalAlertPresentation {
  std::uint16_t button_mode{};
  std::string message{};
  UINT style{};

  friend bool operator==(const OriginalAlertPresentation&,
                         const OriginalAlertPresentation&) = default;
};

// ALRT layout and consumers recovered at 1208:0133/017a/0274/0369.
[[nodiscard]] OriginalAlert parse_original_alert(
    std::span<const std::byte> resource);

[[nodiscard]] std::string format_original_alert(
    std::string_view message_template,
    const std::array<std::string_view, 4>& substitutions);

[[nodiscard]] UINT original_alert_message_box_style(
    std::uint16_t button_mode);

[[nodiscard]] int original_alert_result(std::uint16_t button_mode,
                                        int message_box_result);

// Pure resource/format/style half of 1208:0133. The native host passes this
// exact plan to MessageBox; tests can verify it without opening a window.
[[nodiscard]] OriginalAlertPresentation prepare_original_alert(
    const OriginalResources& resources,
    int resource_id,
    const std::array<std::string_view, 4>& substitutions);

int show_original_alert(HWND owner,
                        const OriginalResources& resources,
                        int resource_id,
                        const std::array<std::string_view, 4>& substitutions);

}  // namespace simtower
