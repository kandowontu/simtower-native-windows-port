#pragma once

#include "original_resources.hpp"
#include "original_tdt.hpp"
#include "original_time.hpp"

#include <windows.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace simtower {

// 1128:13fc builds a 431x41 backing bitmap from BITMAP/320. 1120:0215
// presents it below the common eight-pixel palette-window frame and then
// overlays the live fields translated by this module.
inline constexpr int kOriginalInfoWidth = 431;
inline constexpr int kOriginalInfoBackingHeight = 41;
inline constexpr int kOriginalInfoClientTop = 8;

struct OriginalInfoPoint {
  int x{};
  int y{};

  friend bool operator==(const OriginalInfoPoint&,
                         const OriginalInfoPoint&) = default;
};

struct OriginalInfoClockTime {
  int hour{};
  int minute{};

  friend bool operator==(const OriginalInfoClockTime&,
                         const OriginalInfoClockTime&) = default;
};

struct OriginalInfoContent {
  std::uint16_t rating{};
  std::string balance{};
  std::string population{};
  std::string date_red{};
  std::string date_dark{};
  std::string status{};
  OriginalInfoClockTime clock{};
  OriginalInfoPoint minute_hand{};
  OriginalInfoPoint hour_hand{};
};

// Process-local state at DS:784c/77a4/77a8. The original has four entry
// points which load different STRL resources and assign one of three
// priorities. Keeping this outside the persisted TDT model is deliberate.
struct OriginalInfoStatusState {
  std::string text{};
  std::uint32_t started_tick{};
  std::int16_t priority{};
};

// Exact transient-message writers. A zero index clears the field. Income
// messages (priority zero) cannot replace command/event text, while command
// messages (priority one) cannot replace construction/gameplay notices.
[[nodiscard]] bool set_original_info_construction_status(
    const OriginalResources& resources,
    OriginalInfoStatusState& state,
    std::uint16_t string_index,
    std::uint32_t now_tick);
[[nodiscard]] bool set_original_info_notification_status(
    const OriginalResources& resources,
    OriginalInfoStatusState& state,
    std::uint16_t string_index,
    std::uint32_t now_tick);
[[nodiscard]] bool set_original_info_income_status(
    const OriginalResources& resources,
    OriginalInfoStatusState& state,
    std::uint16_t string_index,
    std::uint32_t now_tick);
[[nodiscard]] bool set_original_info_command_status(
    const OriginalResources& resources,
    OriginalInfoStatusState& state,
    std::uint16_t string_index,
    std::uint32_t now_tick);

// 1118:08f3 clears through the construction-status entry point only when the
// stored 1208:05e6 coarse tick is nonzero and its magnitude delta exceeds
// 300. Each unit is 16 milliseconds, so the nominal lifetime is 4.8 seconds.
[[nodiscard]] bool expire_original_info_status(
    OriginalInfoStatusState& state,
    std::uint32_t now_tick) noexcept;

// Exact 1118:0ce7 ordinal formatting using STRL/713 entries 11..14.
[[nodiscard]] std::string original_info_ordinal(
    const OriginalResources& resources,
    std::int32_t value);

// Exact phase switch at 1200:058d. The game calls it only for the normal
// 0..6 day phases produced while frame_time is within the simulated day.
[[nodiscard]] OriginalInfoClockTime original_info_clock_time(
    std::uint16_t frame_time) noexcept;

// Exact 1200:0037 lookup-table coordinate. index is a minute-like 0..59
// position and radius is 15 for the minute hand or 9 for the hour hand.
[[nodiscard]] OriginalInfoPoint original_info_clock_point(
    int index,
    int radius) noexcept;

// Static translation of 1118:0044/0143/026a/0368/045d/073d. The status text
// is DS:784c: normally empty, and temporarily populated by command/hover
// paths rather than by the tower filename.
[[nodiscard]] OriginalInfoContent build_original_info_content(
    const OriginalResources& resources,
    const OriginalTdtDocument* document,
    std::string_view status = {});

// Native GDI presentation of the exact BITMAP/320..323/327 resource artwork
// and the original live-text/clock overlays. The shared palette-window frame
// occupies client rows 0..7; this routine paints the 1120:0215 content path.
void draw_original_info(HDC destination,
                        const OriginalResources& resources,
                        const OriginalTdtDocument* document,
                        std::string_view status = {});

}  // namespace simtower
