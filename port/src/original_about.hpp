#pragma once

#include "original_dtmp.hpp"
#include "original_resources.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace simtower {

enum class OriginalAboutLauncherStep : std::uint8_t {
  stop_audio_channels,
  deactivate_mixer_backend,
  compact_global_heap,
  run_modal_dialog,
  activate_mixer_backend,
};

// Exact observable launcher order at 1010:049e. GLOBALCOMPACT and Win16 proc-
// instance ownership have no native equivalents; the compact step remains in
// the contract so the two direct WAVEMIXACTIVATE calls cannot be conflated
// with the stop-and-latch wrappers at 11c8:0aab/0add.
[[nodiscard]] constexpr std::array<OriginalAboutLauncherStep, 5>
original_about_launcher_plan() noexcept {
  using Step = OriginalAboutLauncherStep;
  return {
      Step::stop_audio_channels,
      Step::deactivate_mixer_backend,
      Step::compact_global_heap,
      Step::run_modal_dialog,
      Step::activate_mixer_backend,
  };
}

enum class OriginalAboutDialogMessageAction {
  unhandled,
  paint,
  control_color,
  close,
  initialize,
  timer,
};

struct OriginalAboutDialogMessagePlan {
  OriginalAboutDialogMessageAction action{
      OriginalAboutDialogMessageAction::unhandled};
  bool consume{};

  friend bool operator==(const OriginalAboutDialogMessagePlan&,
                         const OriginalAboutDialogMessagePlan&) = default;
};

// Exact seven-entry ABOUTDLGPROC parallel message table at
// 1010:0555/0973. Win16 WM_CTLCOLOR (0x0019) is translated to the split
// WM_CTLCOLOR* family by the native Win32 dialog procedure.
[[nodiscard]] constexpr OriginalAboutDialogMessagePlan
original_about_dialog_message_plan(std::uint16_t message) {
  using Action = OriginalAboutDialogMessageAction;
  switch (message) {
    case 0x000f:
      return {Action::paint, true};
    case 0x0019:
      return {Action::control_color, true};
    case 0x0100:
    case 0x0202:
    case 0x0205:
      return {Action::close, true};
    case 0x0110:
      return {Action::initialize, true};
    case 0x0113:
      return {Action::timer, false};
    default:
      return {};
  }
}

struct OriginalAboutLayout {
  int window_width{};
  int window_height{};
  OriginalDtmpRect title_bitmap{};
  OriginalDtmpRect credits_outer{};
  OriginalDtmpRect credits_inner{};
  int line_width{};
  int line_height{};
  std::uint32_t timer_interval_ms{};

  friend bool operator==(const OriginalAboutLayout&,
                         const OriginalAboutLayout&) = default;
};

struct OriginalAboutScrollLine {
  std::string text{};
  int top{};

  friend bool operator==(const OriginalAboutScrollLine&,
                         const OriginalAboutScrollLine&) = default;
};

struct OriginalAboutLineStyle {
  int width{};
  int height{};
  int font_pixels{};
  std::uint8_t background{};
  std::uint16_t draw_text_flags{};

  friend bool operator==(const OriginalAboutLineStyle&,
                         const OriginalAboutLineStyle&) = default;
};

// 1010:0a3b creates the retained 236x16 line surface; 1010:098f fills it
// with RGB(230), selects the 12-pixel font, and centers text with flags 0809.
[[nodiscard]] constexpr OriginalAboutLineStyle original_about_line_style() {
  return {236, 16, 12, 230U, 0x0809U};
}

// Exact geometry constructed by ABOUTDLGPROC at 1010:056f from BITMAP/257.
[[nodiscard]] OriginalAboutLayout derive_original_about_layout(
    const OriginalResources& resources);

// Exact CR/LF/NUL line reader at 1010:0af1 over TEXT/128.
[[nodiscard]] std::vector<std::string> original_about_credit_lines(
    const OriginalResources& resources);

// Static equivalent of the 55-ms WM_TIMER/ScrollDC path at 1010:0774/0833.
// Tops are relative to the 236x266 inner credits viewport and preserve the
// original one-pixel insertion of each sixteen-pixel line from the bottom.
[[nodiscard]] std::vector<OriginalAboutScrollLine>
original_about_visible_lines(const std::vector<std::string>& lines,
                             std::uint64_t timer_ticks,
                             int viewport_height = 266);

}  // namespace simtower
