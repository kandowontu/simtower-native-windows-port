#pragma once

#include "original_tdt.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace simtower {

struct OriginalFinanceLauncherContract {
  std::uint16_t dialog_resource_id{};
  bool main_window_owner{};
  bool preserves_dialog_result{};

  friend bool operator==(const OriginalFinanceLauncherContract&,
                         const OriginalFinanceLauncherContract&) = default;
};

// Exact 1060:0083 wrapper: DIALOG/500 is modal to Main. Its DialogBox return
// is discarded before the far function returns.
[[nodiscard]] constexpr OriginalFinanceLauncherContract
original_finance_launcher_contract() noexcept {
  return {500U, true, false};
}

enum class OriginalFinanceDialogMessageAction : std::uint8_t {
  unhandled,
  paint,
  control_color,
  key_up,
  initialize,
  left_button_down,
  left_button_up,
};

struct OriginalFinanceDialogMessagePlan {
  OriginalFinanceDialogMessageAction action{
      OriginalFinanceDialogMessageAction::unhandled};
  bool common_true_return{};

  friend bool operator==(const OriginalFinanceDialogMessagePlan&,
                         const OriginalFinanceDialogMessagePlan&) = default;
};

// Exact six-entry COUNTDLOGMAIN parallel table at 1060:00eb/0461. Paint,
// key-up, and both mouse routes all perform their work through the common
// FALSE return at 1060:0454. Initialization alone reaches the common TRUE
// return; WM_CTLCOLOR returns its brush handle directly.
[[nodiscard]] constexpr OriginalFinanceDialogMessagePlan
original_finance_dialog_message_plan(std::uint16_t message) noexcept {
  using Action = OriginalFinanceDialogMessageAction;
  switch (message) {
    case 0x000f:
      return {Action::paint, false};
    case 0x0019:
      return {Action::control_color, false};
    case 0x0101:
      return {Action::key_up, false};
    case 0x0110:
      return {Action::initialize, true};
    case 0x0201:
      return {Action::left_button_down, false};
    case 0x0202:
      return {Action::left_button_up, false};
    default:
      return {};
  }
}

// Persisted values consumed by the Finance dialog painter at 1060:0479.
// The three ten-row columns are copied from DS:b89e/b8ca/b8f6; the nine
// summary fields follow the original draw order for DTMP rectangles 6..14.
struct OriginalFinanceView {
  std::array<std::int32_t, 10> population{};
  std::array<std::int32_t, 10> income{};
  std::array<std::int32_t, 10> maintenance{};

  std::int32_t total_income{};
  std::int32_t total_maintenance{};
  std::int32_t year{};
  std::int32_t quarter{};
  std::int32_t net_revenues{};
  std::int32_t other_income{};
  std::int32_t construction_costs{};
  std::int32_t last_quarter_balance{};
  std::int32_t total_balance{};

  friend bool operator==(const OriginalFinanceView&,
                         const OriginalFinanceView&) = default;
};

struct OriginalFinanceKeyPresentation {
  bool draw_pressed{};
  bool draw_released{};
  bool close{};

  friend bool operator==(const OriginalFinanceKeyPresentation&,
                         const OriginalFinanceKeyPresentation&) = default;
};

struct OriginalFinanceValuePosition {
  int x{};
  int y{};

  friend bool operator==(const OriginalFinanceValuePosition&,
                         const OriginalFinanceValuePosition&) = default;
};

// Exact coordinate half of Finance formatter 11e0:00ca after 1208:0c89 has
// measured the signed decimal string. DTMP items 8/9 use (+60,+2), items
// 10..14 use (+64,+1), and every other item is simply right-aligned at +1.
[[nodiscard]] constexpr OriginalFinanceValuePosition
original_finance_value_position(std::size_t rectangle_index,
                                int rectangle_right,
                                int rectangle_top,
                                int text_width) noexcept {
  const bool year_or_quarter =
      rectangle_index == 8U || rectangle_index == 9U;
  const bool summary = rectangle_index >= 10U && rectangle_index <= 14U;
  return {
      rectangle_right - text_width +
          (year_or_quarter ? 60 : (summary ? 64 : 0)),
      rectangle_top + (year_or_quarter ? 2 : 1),
  };
}

// Exact COUNTDLOGMAIN WM_KEYUP branch at 1060:02a4-037e. Return (0x0d) and
// Space (0x20) synchronously present the pressed button surface, restore the
// released surface, and only then close the modal dialog.
[[nodiscard]] constexpr OriginalFinanceKeyPresentation
original_finance_key_presentation(std::uint16_t virtual_key) noexcept {
  const bool activates = virtual_key == 0x0dU || virtual_key == 0x20U;
  return {activates, activates, activates};
}

// Exact data half of 1060:0479. Signed division truncates toward zero and
// net-revenue subtraction wraps as a 32-bit x86 SUB.
[[nodiscard]] OriginalFinanceView derive_original_finance_view(
    const OriginalTdtDocument& document) noexcept;

}  // namespace simtower
