#pragma once

#include "original_resources.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace simtower {

inline constexpr std::uint32_t kOriginalStartupSplashMinimumTicks = 180U;
inline constexpr std::uint32_t kOriginalStartupSplashNominalMinimumMs =
    kOriginalStartupSplashMinimumTicks * 16U;
inline constexpr std::uint32_t kOriginalStartupRequiredMemoryKb = 6000U;
inline constexpr std::uint32_t kOriginalStartupResidentMemoryKb = 2370U;
inline constexpr std::size_t kOriginalStartupSavePathCapacity = 0x80U;
// The supplied installation's WINDOWS/SIMTOWER.INI value. The native
// single-file build embeds this alongside the shipped Sound values so that
// removing the source tree does not silently change the original file-dialog
// starting directory.
inline constexpr std::wstring_view kOriginalShippedSaveDirectory =
    L"C:\\Maxis\\Simtower";

// SETUPSTARTUPDLGA at 1010:014c handles Paint, Init, and left-button-down.
// SETUPSTARTUPDLGB at 1010:0304 replaces the mouse route with WM_DESTROY.
[[nodiscard]] constexpr bool original_startup_splash_handles_message(
    bool modal,
    std::uint16_t message) noexcept {
  if (message == 0x000fU || message == 0x0110U) return true;
  return modal ? message == 0x0201U : message == 0x0002U;
}

inline constexpr std::uint16_t kOriginalStartupSplashModalDismissResult = 0U;

enum class OriginalStartupSplashTeardownStep : std::uint8_t {
  none,
  destroy_window,
  clear_window_handle,
  release_proc_instance,
  release_bitmap_resource_if_present,
  clear_proc_pointer,
};

// Exact 1010:00fd order. Native code has no Win16 proc instance or separately
// owned bitmap handle, but retaining those steps makes their ownership
// replacements explicit and keeps the observable DestroyWindow/handle-clear
// sequence production-consumed.
[[nodiscard]] constexpr std::array<OriginalStartupSplashTeardownStep, 5>
original_startup_splash_teardown_plan(bool window_exists) noexcept {
  using Step = OriginalStartupSplashTeardownStep;
  return window_exists
             ? std::array<Step, 5>{
                   Step::destroy_window,
                   Step::clear_window_handle,
                   Step::release_proc_instance,
                   Step::release_bitmap_resource_if_present,
                   Step::clear_proc_pointer,
               }
             : std::array<Step, 5>{};
}

enum class OriginalStartupShowAction : std::uint8_t {
  none,
  show_command,
  show_info,
  preload_command_surfaces,
  show_map,
  show_main,
  select_arrow_cursor,
  toggle_construction_mode,
  compose_and_present_command,
};

enum class OriginalStartupPrecomputeAction : std::uint8_t {
  precompute_floor_offsets,
  pack_world_and_facility_sheets,
};

// Exact 1128:0ba3 call order: build 11a0:134c's sixty floor offsets from the
// current scale dword, then run 11f8:033a's world/facility sheet pack.
[[nodiscard]] constexpr std::array<OriginalStartupPrecomputeAction, 2>
original_startup_precompute_plan() noexcept {
  return {OriginalStartupPrecomputeAction::precompute_floor_offsets,
          OriginalStartupPrecomputeAction::pack_world_and_facility_sheets};
}

// Exact visible-window/resource/cursor tail at 1128:01d9-0223. Map and Info
// are shown before 1050:03aa creates the command surface cache; Command and
// Main follow. The 1058:033c construction toggle is conditional on DS:783e
// not already being one, while 1080:05a1 always performs the final Command
// composition and synchronous presentation of DS:325a (CmdBtnWClass).
[[nodiscard]] constexpr std::array<OriginalStartupShowAction, 8>
original_startup_show_plan(bool construction_mode_enabled) noexcept {
  return {
      OriginalStartupShowAction::show_map,
      OriginalStartupShowAction::show_info,
      OriginalStartupShowAction::preload_command_surfaces,
      OriginalStartupShowAction::show_command,
      OriginalStartupShowAction::show_main,
      OriginalStartupShowAction::select_arrow_cursor,
      construction_mode_enabled
          ? OriginalStartupShowAction::none
          : OriginalStartupShowAction::toggle_construction_mode,
      OriginalStartupShowAction::compose_and_present_command,
  };
}

enum class OriginalStartupCapabilityIssue : std::uint8_t {
  none,
  fewer_than_256_colors,
  missing_bitblt,
  missing_device_independent_bitmap,
  missing_dib_to_device,
  missing_stretchblt,
  truetype_unsupported,
  truetype_disabled,
  wave_output_unavailable,
};

struct OriginalStartupCapabilityState {
  std::uint16_t bits_per_pixel{};
  std::uint16_t raster_caps{};
  std::uint16_t rasterizer_flags{};
  bool wave_output_available{};

  friend bool operator==(const OriginalStartupCapabilityState&,
                         const OriginalStartupCapabilityState&) = default;
};

inline constexpr std::uint16_t kOriginalRasterCapBitBlt = 0x0001U;
inline constexpr std::uint16_t kOriginalRasterCapDeviceIndependentBitmap =
    0x0080U;
inline constexpr std::uint16_t kOriginalRasterCapDibToDevice = 0x0200U;
inline constexpr std::uint16_t kOriginalRasterCapStretchBlt = 0x0800U;
inline constexpr std::uint16_t kOriginalRasterizerTrueTypeAvailable = 0x0001U;
inline constexpr std::uint16_t kOriginalRasterizerTrueTypeEnabled = 0x0002U;
inline constexpr std::string_view kOriginalStartupLowColorMessage =
    "Windows is not running with a 256-color display driver.  SimTower will "
    "run, but may display distorted color and animation.  Do you still want "
    "to run SimTower?";
inline constexpr std::string_view kOriginalStartupMissingRasterMessage =
    "The display driver on this system does not appear to support the "
    "functionality that SimTower requires.  If you continue, you may "
    "experience lockups or General Protection Faults.  Do you want to "
    "continue?";
inline constexpr std::string_view kOriginalStartupTrueTypeUnsupportedMessage =
    "You cannot run SimTower without TrueType Fonts. Your Windows does not "
    "support TrueType. ";
inline constexpr std::string_view kOriginalStartupTrueTypeDisabledMessage =
    "TrueType is not available.Please change settings  to \"Use TrueType "
    "Fonts\" in the Control Panels.";
inline constexpr std::string_view kOriginalStartupNoWaveOutputMessage =
    "SimTower did not detect a sound card or proper sound drivers on this "
    "system.  Do you want to run SimTower without sound support? ";

// Exact ordered 1128:1195-12af capability gates. The four raster operations
// deliberately remain separate issues because the executable can present the
// same display-driver warning four times when each accepted check still
// leaves the next capability missing. A missing TrueType implementation
// suppresses the enabled check; the wave-output probe is always last.
[[nodiscard]] constexpr std::array<OriginalStartupCapabilityIssue, 8>
original_startup_capability_issues(
    OriginalStartupCapabilityState state) noexcept {
  std::array<OriginalStartupCapabilityIssue, 8> issues{};
  std::size_t count = 0U;
  const auto append = [&](OriginalStartupCapabilityIssue issue) {
    issues[count++] = issue;
  };
  if (state.bits_per_pixel < 8U) {
    append(OriginalStartupCapabilityIssue::fewer_than_256_colors);
  }
  if ((state.raster_caps & kOriginalRasterCapBitBlt) == 0U) {
    append(OriginalStartupCapabilityIssue::missing_bitblt);
  }
  if ((state.raster_caps &
       kOriginalRasterCapDeviceIndependentBitmap) == 0U) {
    append(
        OriginalStartupCapabilityIssue::missing_device_independent_bitmap);
  }
  if ((state.raster_caps & kOriginalRasterCapDibToDevice) == 0U) {
    append(OriginalStartupCapabilityIssue::missing_dib_to_device);
  }
  if ((state.raster_caps & kOriginalRasterCapStretchBlt) == 0U) {
    append(OriginalStartupCapabilityIssue::missing_stretchblt);
  }
  if ((state.rasterizer_flags &
       kOriginalRasterizerTrueTypeAvailable) == 0U) {
    append(OriginalStartupCapabilityIssue::truetype_unsupported);
  } else if ((state.rasterizer_flags &
              kOriginalRasterizerTrueTypeEnabled) == 0U) {
    append(OriginalStartupCapabilityIssue::truetype_disabled);
  }
  if (!state.wave_output_available) {
    append(OriginalStartupCapabilityIssue::wave_output_unavailable);
  }
  return issues;
}

[[nodiscard]] constexpr bool original_startup_capability_requires_consent(
    OriginalStartupCapabilityIssue issue) noexcept {
  return issue == OriginalStartupCapabilityIssue::fewer_than_256_colors ||
         issue == OriginalStartupCapabilityIssue::missing_bitblt ||
         issue ==
             OriginalStartupCapabilityIssue::missing_device_independent_bitmap ||
         issue == OriginalStartupCapabilityIssue::missing_dib_to_device ||
         issue == OriginalStartupCapabilityIssue::missing_stretchblt ||
         issue == OriginalStartupCapabilityIssue::wave_output_unavailable;
}

// Exact KERNEL profile value assembled at 1128:12d0-1301 when
// [Extensions] tdt is absent: the module filename followed by " ^.tdt".
[[nodiscard]] std::string original_tdt_extension_profile_value(
    std::string_view module_filename);

// Exact four-entry NEWORLOADDLOGFILTER parallel message table at
// 1018:007c/01fd.
[[nodiscard]] constexpr bool original_new_load_dialog_handles_message(
    std::uint16_t message) noexcept {
  switch (message) {
    case 0x000f:  // WM_PAINT
    case 0x0019:  // Win16 WM_CTLCOLOR
    case 0x0110:  // WM_INITDIALOG
    case 0x0111:  // WM_COMMAND
      return true;
    default:
      return false;
  }
}

struct OriginalNewLoadDialogCommandPlan {
  bool release_dtmp_before_end{};
  bool end_dialog{};
  std::uint16_t dialog_result{};
  bool consume{true};

  friend bool operator==(const OriginalNewLoadDialogCommandPlan&,
                         const OriginalNewLoadDialogCommandPlan&) = default;
};

struct OriginalNewLoadLauncherContract {
  std::uint16_t dialog_resource_id{};
  std::int32_t original_parameter{};
  bool main_window_owner{};
  bool returns_dialog_result{};

  friend bool operator==(const OriginalNewLoadLauncherContract&,
                         const OriginalNewLoadLauncherContract&) = default;
};

enum class OriginalStartupNativeModalOwner : std::uint8_t {
  recovered_main_window,
  active_startup_splash,
};

// 1018:0000 names Main as the Win16 owner. A Win16 modal dialog also blocks
// the other windows in its task, but Win32 DialogBox disables only its direct
// owner. While the modeless title splash is alive, make that live native
// window the owner so it cannot be activated over New/Load. The recovered
// launcher contract above remains unchanged.
[[nodiscard]] constexpr OriginalStartupNativeModalOwner
original_startup_native_modal_owner(bool splash_exists) noexcept {
  return splash_exists
      ? OriginalStartupNativeModalOwner::active_startup_splash
      : OriginalStartupNativeModalOwner::recovered_main_window;
}

// Exact 1018:0000 launcher arguments. The native thunk replaces the original
// zero application parameter with a host context pointer, but the resource,
// owner, and returned DialogBox result remain unchanged.
[[nodiscard]] constexpr OriginalNewLoadLauncherContract
original_new_load_launcher_contract() noexcept {
  return {10000U, 0, true, true};
}

// NEWORLOADDLOGFILTER 1018:01bf consumes only WM_COMMAND IDs 1..3, releases
// 1070:051f's per-HWND DTMP ownership before closing the dialog with that
// control ID as the result, and never inspects the notification word. Other
// command IDs return FALSE through 1018:01ee.
[[nodiscard]] constexpr OriginalNewLoadDialogCommandPlan
original_new_load_dialog_command_plan(std::uint16_t control) noexcept {
  return control >= 1U && control <= 3U
             ? OriginalNewLoadDialogCommandPlan{true, true, control, true}
             : OriginalNewLoadDialogCommandPlan{false, false, 0U, false};
}

struct OriginalNewLoadDialogStyle {
  int paint_font_pixels{};
  int control_font_pixels{};
  bool clear_class_cursor{};
  bool show_during_initialization{};
  bool realize_logical_palette{};

  friend bool operator==(const OriginalNewLoadDialogStyle&,
                         const OriginalNewLoadDialogStyle&) = default;
};

// Exact NEWORLOADDLOGFILTER 1018:0096-013d/0145-01b8 presentation contract.
// The dialog clears GCL_HCURSOR before selecting Arrow, shows itself during
// initialization, uses an 11-pixel paint font, selects a 13-pixel font for
// every control-color request, and realizes the active logical palette for
// both immediate and WM_PAINT presentations.
[[nodiscard]] constexpr OriginalNewLoadDialogStyle
original_new_load_dialog_style() noexcept {
  return {11, 13, true, true, true};
}

struct OriginalStartupWindowSize {
  int width{};
  int height{};

  friend bool operator==(const OriginalStartupWindowSize&,
                         const OriginalStartupWindowSize&) = default;
};

struct OriginalStartupBitmapPlacement {
  int left{};
  int top{};
  int width{};
  int height{};

  friend bool operator==(const OriginalStartupBitmapPlacement&,
                         const OriginalStartupBitmapPlacement&) = default;
};

struct OriginalStartupCommandTarget {
  std::string path{};
  std::string file_title{};

  friend bool operator==(const OriginalStartupCommandTarget&,
                         const OriginalStartupCommandTarget&) = default;
};

// Win16 receives a raw tail whose file path is not surrounded by quotes. A
// modern Windows file association quotes paths containing spaces before they
// reach WinMain. Remove only that host-added matching pair, then pass the
// result through the exact recovered target logic below.
[[nodiscard]] std::string normalize_native_startup_command_line(
    std::string_view command_line);

// Exact 1258:0029-0070 plus 1128:00e5-017e command-line file target. WinMain
// copies the raw nonempty ANSI tail, while the startup routine recognizes only
// a backslash as a supplied directory. A bare name is prefixed by the module
// directory (which 1258 retains with its trailing backslash).
[[nodiscard]] std::optional<OriginalStartupCommandTarget>
original_startup_command_target(std::string_view module_directory,
                                std::string_view command_line);

// SETUPSTARTUPDLGA at 1010:014c adds two border metrics to the desktop
// rectangle; modeless SETUPSTARTUPDLGB at 1010:0304 uses it verbatim.
[[nodiscard]] OriginalStartupWindowSize original_startup_window_size(
    int desktop_right,
    int desktop_bottom,
    bool modal,
    int border_width,
    int border_height) noexcept;

// Exact SETUPSTARTUPDLGA/SETUPSTARTUPDLGB WM_PAINT centering and negative
// clamp for the active BITMAP resource.
[[nodiscard]] OriginalStartupBitmapPlacement
original_startup_bitmap_placement(const OriginalResources& resources,
                                  int bitmap_id,
                                  int client_width,
                                  int client_height);

// Exact 1128:1318 startup-memory calculation. Win16 GETFREESPACE returns
// bytes; the original shifts right by ten and adds the resident 0x942K
// already occupied by SimTower before comparing unsigned against 0x1770K.
[[nodiscard]] constexpr std::uint32_t original_startup_memory_kb(
    std::uint32_t free_space_bytes) noexcept {
  return (free_space_bytes >> 10U) + kOriginalStartupResidentMemoryKb;
}

[[nodiscard]] constexpr bool original_startup_memory_sufficient(
    std::uint32_t free_space_bytes) noexcept {
  return original_startup_memory_kb(free_space_bytes) >=
         kOriginalStartupRequiredMemoryKb;
}

// Exact two copied data-segment strings and `%ld` substitution used by the
// 1128:1318 MessageBox error path.
[[nodiscard]] std::string original_startup_low_memory_message(
    std::uint32_t free_space_bytes);

}  // namespace simtower
