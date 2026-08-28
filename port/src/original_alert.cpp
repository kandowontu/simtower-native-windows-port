#include "original_alert.hpp"

#include <stdexcept>

namespace simtower {
namespace {

std::uint16_t little_u16(std::span<const std::byte> data, std::size_t offset) {
  if (offset > data.size() || data.size() - offset < 2U) {
    throw std::runtime_error("Truncated original ALRT word");
  }
  return static_cast<std::uint16_t>(
      static_cast<std::uint16_t>(data[offset]) |
      (static_cast<std::uint16_t>(data[offset + 1U]) << 8U));
}

}  // namespace

OriginalAlert parse_original_alert(std::span<const std::byte> resource) {
  OriginalAlert result;
  result.button_mode = little_u16(resource, 0);
  result.preserved_word = little_u16(resource, 2);
  std::size_t cursor = 4;
  while (cursor < resource.size() && resource[cursor] != std::byte{0}) {
    result.message_template.push_back(static_cast<char>(resource[cursor++]));
  }
  if (cursor >= resource.size()) {
    throw std::runtime_error("Unterminated original ALRT message");
  }
  return result;
}

std::string format_original_alert(
    std::string_view message_template,
    const std::array<std::string_view, 4>& substitutions) {
  // Exact native string realization of 1208:0274. The Win16 routine walks
  // the loaded ALRT template, expands ^0..^3 through LSTRCPY, and discards an
  // unsupported escape together with its selector byte.
  std::string result;
  for (std::size_t cursor = 0; cursor < message_template.size(); ++cursor) {
    const char character = message_template[cursor];
    if (character != '^') {
      result.push_back(character);
      continue;
    }

    ++cursor;
    if (cursor >= message_template.size()) {
      break;
    }
    const unsigned index =
        static_cast<unsigned char>(message_template[cursor]) -
        static_cast<unsigned char>('0');
    if (index <= 3U) {
      result.append(substitutions[index]);
    }
    // 1208:030f discards both bytes for an unsupported escape.
  }
  return result;
}

UINT original_alert_message_box_style(std::uint16_t button_mode) {
  // 1208:0385-038f: ALRT[0] | MB_ICONEXCLAMATION | MB_SYSTEMMODAL.
  return static_cast<UINT>(button_mode) | MB_ICONEXCLAMATION | MB_SYSTEMMODAL;
}

int original_alert_result(std::uint16_t button_mode,
                          int message_box_result) {
  // Exact 1208:0369 return normalization after USER!MESSAGEBOX. Modes 0/2
  // always return one; modes 1/4 normalize the affirmative result; mode 3
  // preserves the original yes/no/cancel values as one/two/three.
  switch (button_mode) {
    case 1:
      return message_box_result == IDOK ? 1 : 2;
    case 3:
      if (message_box_result == IDYES) {
        return 1;
      }
      if (message_box_result == IDNO) {
        return 2;
      }
      return 3;
    case 4:
      return message_box_result == IDYES ? 1 : 2;
    case 0:
    case 2:
    default:
      return 1;
  }
}

OriginalAlertPresentation prepare_original_alert(
    const OriginalResources& resources,
    int resource_id,
    const std::array<std::string_view, 4>& substitutions) {
  const auto alert = parse_original_alert(resources.find("ALRT", resource_id));
  return {
      alert.button_mode,
      format_original_alert(alert.message_template, substitutions),
      original_alert_message_box_style(alert.button_mode),
  };
}

int show_original_alert(
    HWND owner,
    const OriginalResources& resources,
    int resource_id,
    const std::array<std::string_view, 4>& substitutions) {
  // Native owner/resource boundary for the shared 11e0:0c9e wrapper and its
  // 1208:0133/017a/0274/0369 resource/format/result helpers. The ALRT
  // bytes are immutable spans in the self-contained resource pack, so the
  // obsolete Win16 lock-count cleanup at 1208:0cb5 is unnecessary here.
  const auto presentation =
      prepare_original_alert(resources, resource_id, substitutions);
  const int native_result = MessageBoxA(
      owner, presentation.message.c_str(), "SimTower", presentation.style);
  return original_alert_result(presentation.button_mode, native_result);
}

}  // namespace simtower
