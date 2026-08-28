#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace simtower {

// Exact five-entry AHOTTADLOGFILTER parallel message table at
// 1068:00b6/0421.
[[nodiscard]] constexpr bool original_event_dialog_handles_message(
    std::uint16_t message) noexcept {
  switch (message) {
    case 0x000f:  // WM_PAINT
    case 0x0019:  // Win16 WM_CTLCOLOR
    case 0x0110:  // WM_INITDIALOG
    case 0x0111:  // WM_COMMAND
    case 0x0113:  // WM_TIMER
      return true;
    default:
      return false;
  }
}

struct OriginalDialogValue {
  enum class Kind { none, text, ordinal };
  Kind kind = Kind::none;
  std::string text;
  std::uint16_t ordinal = 0;
};

struct OriginalDialogItem {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t width = 0;
  std::int16_t height = 0;
  std::uint16_t id = 0;
  std::uint32_t style = 0;
  OriginalDialogValue window_class;
  OriginalDialogValue text;
};

struct OriginalDialog {
  std::uint32_t style = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t width = 0;
  std::int16_t height = 0;
  OriginalDialogValue menu;
  OriginalDialogValue window_class;
  OriginalDialogValue caption;
  std::uint16_t font_point_size = 0;
  std::string font_face;
  std::vector<OriginalDialogItem> items;
};

struct OriginalDialogScreenRect {
  std::int32_t left = 0;
  std::int32_t top = 0;
  std::int32_t right = 0;
  std::int32_t bottom = 0;
};

struct OriginalDialogScreenPosition {
  std::int32_t left = 0;
  std::int32_t top = 0;

  bool operator==(const OriginalDialogScreenPosition&) const = default;
};

struct OriginalDialogItemTextContract {
  std::uint16_t get_buffer_characters{};
  bool set_forwards_text{};

  friend bool operator==(const OriginalDialogItemTextContract&,
                         const OriginalDialogItemTextContract&) = default;
};

// Exact shared wrappers at 11e0:0000/0026. The getter always passes 0xfe as
// its buffer character count; the setter forwards the caller's text without
// imposing a separate length limit.
[[nodiscard]] constexpr OriginalDialogItemTextContract
original_dialog_item_text_contract() noexcept {
  return {0x00feU, true};
}

enum class OriginalDialogRectScreenCorner : std::uint8_t {
  upper_left,
  lower_right,
};

// Exact 1070:06cd popup confinement order. The Win16 helper converts the
// client rectangle's upper-left point first, then its lower-right point,
// before passing the resulting screen rectangle to ClipCursor.
[[nodiscard]] constexpr std::array<OriginalDialogRectScreenCorner, 2>
original_dialog_rect_screen_conversion_order() noexcept {
  return {OriginalDialogRectScreenCorner::upper_left,
          OriginalDialogRectScreenCorner::lower_right};
}

// Exact 11e0:0c10 desktop-center arithmetic after the window rectangle is
// normalized to the origin. Win16 uses signed 16-bit subtraction followed by
// division-by-two truncation and passes SetWindowPos flags 0x41.
[[nodiscard]] OriginalDialogScreenPosition original_dialog_center_position(
    OriginalDialogScreenRect desktop,
    OriginalDialogScreenRect window) noexcept;

[[nodiscard]] OriginalDialog parse_original_dialog(
    std::span<const std::byte> resource);

// Exact mutable-template patch at 1008:0000. The shared launcher enables
// DS_SETFONT and replaces the template's point-size word while preserving
// the original face name and every other template field.
void apply_original_dialog_font_point_size(
    OriginalDialog& dialog,
    std::uint16_t point_size) noexcept;

// Exact 1068:0439 all-control pass. Every caret consumes itself and the next
// character; a ^0 pair emits the raw signed `%ld` argument, while any other
// pair emits nothing.
[[nodiscard]] std::string format_original_dialog_caret_arguments(
    std::string text,
    std::int32_t argument);

// Exact final numeric substitution produced by AHOTTADLOGFILTER's
// 1068:0439 and 1068:0175-0256 initialization passes on dialog item 3.
// After the all-caret pass, the first "#0" receives the wrapping 32-bit
// absolute value; remaining zero suffix characters are preserved (for
// example "$#000" -> "$200000").
[[nodiscard]] std::string format_original_dialog_argument(
    std::string text,
    std::int32_t argument);

enum class OriginalEventDialogAction {
  ignore,
  close_decline,
  close_accept,
  warn_insufficient_funds_then_close_decline,
};

// Exact AHOTTADLOGFILTER command/timer decision at 1068:0380-0406. Button 1
// declines. Button 2 (and the 37.856-second timer) accepts unless a negative
// argument would make the signed, wrapping balance negative. A manual
// unaffordable acceptance shows ALRT/1004 before declining; timer expiry
// suppresses that warning.
[[nodiscard]] OriginalEventDialogAction original_event_dialog_action(
    std::uint16_t command,
    std::int32_t argument,
    std::int32_t balance,
    bool timer_fired) noexcept;

// Exact shared dialog placement at 11e0:0b52. A zero requested-left centers
// horizontally; Elevator Control passes 8. The vertical policy is not ordinary
// centering: it keeps at least 43 pixels above ordinary dialogs, uses an
// 80-pixel fallback for nearly full-height dialogs, then bottom-clamps.
[[nodiscard]] OriginalDialogScreenPosition original_dialog_screen_position(
    OriginalDialogScreenRect desktop,
    OriginalDialogScreenRect window,
    std::int32_t requested_left) noexcept;

// Converts the Win16 ANSI template to a Win32 Unicode DLGTEMPLATE while
// preserving styles, coordinates, identifiers, classes, text, and font.
[[nodiscard]] std::vector<std::byte> build_native_dialog_template(
    const OriginalDialog& dialog);

// Exact facility-type switch at 1100:03ac. The returned resource is
// DIALOG/(0x02ec + b3ac). Types handled by the separate Lobby/status path,
// and an inactive negative tenant, return no facility dialog.
[[nodiscard]] std::optional<std::uint16_t>
original_facility_information_dialog_id(std::int8_t type) noexcept;

// Exact Magnifying Glass resource selection at 1100:0e86 and 1100:11da.
// Elevator type zero uses DIALOG/761 while every other elevator type uses
// DIALOG/762; Stair/Escalator information always uses DIALOG/761.
[[nodiscard]] std::uint16_t original_elevator_information_dialog_id(
    std::uint8_t type) noexcept;
[[nodiscard]] constexpr std::uint16_t
original_vertical_transport_information_dialog_id() noexcept {
  return 0x02f9U;
}

}  // namespace simtower
