#pragma once

#include "original_dtmp.hpp"
#include "original_resources.hpp"

#include <windows.h>

namespace simtower {

struct OriginalDtmpWindowSize {
  int width = 0;
  int height = 0;
  friend bool operator==(const OriginalDtmpWindowSize&,
                         const OriginalDtmpWindowSize&) = default;
};

struct OriginalDtmpConfigurationPlan {
  bool resize_before_dc{};
  bool realize_palette{};
  bool set_update_current_position{};
  bool resize_after_dc{};

  friend bool operator==(const OriginalDtmpConfigurationPlan&,
                         const OriginalDtmpConfigurationPlan&) = default;
};

// Native translations of the DTMP dialog helpers at 1070:0005/0231.
[[nodiscard]] OriginalDtmpWindowSize original_dtmp_window_size(
    const OriginalDtmp& dtmp,
    const OriginalResources& resources);

// Exact signed-resource split at 1070:0046-0065/00e6-01ed. A bitmap-backed
// DTMP sizes the outer window before acquiring its DC and does not realize the
// logical palette there. A bitmap-less DTMP realizes the palette, adds
// TA_UPDATECP, releases the DC, and only then applies a nonzero header size.
[[nodiscard]] constexpr OriginalDtmpConfigurationPlan
original_dtmp_configuration_plan(const OriginalDtmp& dtmp) noexcept {
  if (dtmp.bitmap_resource_id >= 0) {
    return {true, false, true, false};
  }
  return {false, true, true, dtmp.width_or_header != 0U};
}

void configure_original_dtmp_window(HWND dialog,
                                    const OriginalDtmp& dtmp,
                                    const OriginalResources& resources,
                                    HPALETTE logical_palette);

void render_original_dtmp(HWND dialog,
                          HDC destination,
                          const OriginalDtmp& dtmp,
                          const OriginalResources& resources);

// Exact native GDI translation of the dialog-frame path at 1068:0567 and
// its two recovered bevel routines at 11e0:06d9/0950.
void paint_original_dialog_chrome(HWND dialog,
                                  HDC destination,
                                  const OriginalDtmp& dtmp);

}  // namespace simtower
