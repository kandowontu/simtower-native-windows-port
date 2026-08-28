#include "original_about.hpp"
#include "original_alert.hpp"
#include "original_audio.hpp"
#include "original_command_palette.hpp"
#include "original_construction.hpp"
#include "original_dib.hpp"
#include "original_dialog.hpp"
#include "original_dtmp.hpp"
#include "original_dtmp_runtime.hpp"
#include "original_elevator_control.hpp"
#include "original_finance.hpp"
#include "original_find.hpp"
#include "original_font.hpp"
#include "original_info.hpp"
#include "original_information.hpp"
#include "original_map.hpp"
#include "original_palette_frame.hpp"
#include "original_people.hpp"
#include "original_resources.hpp"
#include "original_simulation.hpp"
#include "original_startup.hpp"
#include "original_tables.hpp"
#include "original_tdt_file.hpp"
#include "original_time.hpp"
#include "original_ui.hpp"
#include "original_world.hpp"

#include <windows.h>
#include <commdlg.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr wchar_t kMainClass[] = L"Tower_MainWClass";
constexpr wchar_t kMapClass[] = L"Tower_MapWClass";
constexpr wchar_t kInfoClass[] = L"Tower_InfoWClass";
constexpr wchar_t kCommandClass[] = L"CmdBtnWClass";
constexpr UINT kOriginalAudioCallbackMessage = 0x03BD;
constexpr DWORD kMainStyle = 0x00FF0000UL;
constexpr DWORD kPaletteWindowStyle = 0x80800000UL;

void configure_original_main_host_chrome(HWND window) {
  // The recovered Win16 window owns a classic non-client frame.  On current
  // Windows versions the same style bits are otherwise rendered as a modern
  // DWM caption and themed scrollbars, which is immediately unlike the
  // supplied executable.  Ask USER/DWM for the unthemed legacy path while
  // preserving the original style, hit testing, menu, and scrollbar message
  // contracts.  Resolve both optional APIs dynamically so the self-contained
  // executable keeps its existing import boundary and remains usable where
  // either component is unavailable.
  if (HMODULE theme = LoadLibraryW(L"uxtheme.dll")) {
    using SetWindowThemeFn = HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR);
    if (const auto set_window_theme = reinterpret_cast<SetWindowThemeFn>(
            GetProcAddress(theme, "SetWindowTheme"))) {
      (void)set_window_theme(window, L"", L"");
    }
    FreeLibrary(theme);
  }

  if (HMODULE dwm = LoadLibraryW(L"dwmapi.dll")) {
    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(
        HWND, DWORD, LPCVOID, DWORD);
    if (const auto set_attribute =
            reinterpret_cast<DwmSetWindowAttributeFn>(
                GetProcAddress(dwm, "DwmSetWindowAttribute"))) {
      constexpr DWORD kDwmwaNcRenderingPolicy = 2U;
      constexpr DWORD kDwmNcRenderingDisabled = 1U;
      (void)set_attribute(window, kDwmwaNcRenderingPolicy,
                          &kDwmNcRenderingDisabled,
                          sizeof(kDwmNcRenderingDisabled));
    }
    FreeLibrary(dwm);
  }
}

struct OriginalMainCaptionLayout {
  RECT system_button{};
  RECT title{};
  RECT minimize_button{};
  RECT maximize_button{};
  int caption_top{};
  int caption_bottom{};
};

OriginalMainCaptionLayout original_main_caption_layout(HWND window) {
  RECT bounds{};
  GetWindowRect(window, &bounds);
  const int width =
      std::max(1, static_cast<int>(bounds.right - bounds.left));
  const int frame_x = std::max(1, GetSystemMetrics(SM_CXFRAME));
  const int frame_y = std::max(1, GetSystemMetrics(SM_CYFRAME));
  const int caption_height = std::max(1, GetSystemMetrics(SM_CYCAPTION));
  const int button_width = caption_height;
  const int right = std::max(frame_x, width - frame_x);

  OriginalMainCaptionLayout layout{};
  layout.caption_top = frame_y;
  layout.caption_bottom = frame_y + caption_height;
  layout.system_button = {
      frame_x, layout.caption_top, frame_x + button_width,
      layout.caption_bottom};
  layout.maximize_button = {
      right - button_width, layout.caption_top, right,
      layout.caption_bottom};
  layout.minimize_button = {
      right - 2 * button_width, layout.caption_top,
      right - button_width, layout.caption_bottom};
  layout.title = {
      layout.system_button.right, layout.caption_top,
      layout.minimize_button.left, layout.caption_bottom};
  return layout;
}

void draw_original_main_caption_button(HDC dc, const RECT& rectangle,
                                       bool system_button,
                                       bool minimize_button) {
  const HBRUSH face = CreateSolidBrush(RGB(192, 192, 192));
  FillRect(dc, &rectangle, face);
  DeleteObject(face);

  const HPEN white = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
  const HPEN gray = CreatePen(PS_SOLID, 1, RGB(128, 128, 128));
  const HPEN black = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
  HGDIOBJ previous_pen = SelectObject(dc, white);
  MoveToEx(dc, rectangle.left, rectangle.bottom - 1, nullptr);
  LineTo(dc, rectangle.left, rectangle.top);
  LineTo(dc, rectangle.right - 1, rectangle.top);
  SelectObject(dc, black);
  LineTo(dc, rectangle.right - 1, rectangle.bottom - 1);
  LineTo(dc, rectangle.left, rectangle.bottom - 1);
  SelectObject(dc, gray);
  MoveToEx(dc, rectangle.left + 1, rectangle.bottom - 2, nullptr);
  LineTo(dc, rectangle.right - 2, rectangle.bottom - 2);
  LineTo(dc, rectangle.right - 2, rectangle.top + 1);

  SelectObject(dc, black);
  const int center_x = (rectangle.left + rectangle.right) / 2;
  const int center_y = (rectangle.top + rectangle.bottom) / 2;
  if (system_button) {
    const int half = std::max(
        2, static_cast<int>((rectangle.right - rectangle.left) / 5));
    MoveToEx(dc, center_x - half, center_y + half, nullptr);
    LineTo(dc, center_x + half + 1, center_y + half);
  } else {
    const int half = std::max(
        2, static_cast<int>((rectangle.right - rectangle.left) / 5));
    POINT triangle[3]{};
    if (minimize_button) {
      triangle[0] = {center_x - half, center_y - 1};
      triangle[1] = {center_x + half, center_y - 1};
      triangle[2] = {center_x, center_y + half};
    } else {
      triangle[0] = {center_x - half, center_y + 1};
      triangle[1] = {center_x + half, center_y + 1};
      triangle[2] = {center_x, center_y - half};
    }
    HGDIOBJ previous_brush = SelectObject(dc, GetStockObject(BLACK_BRUSH));
    Polygon(dc, triangle, 3);
    SelectObject(dc, previous_brush);
  }

  SelectObject(dc, previous_pen);
  DeleteObject(black);
  DeleteObject(gray);
  DeleteObject(white);
}

void paint_original_main_caption(HWND window, bool active) {
  HDC dc = GetWindowDC(window);
  if (!dc) return;
  const auto layout = original_main_caption_layout(window);

  RECT caption{
      layout.system_button.left, layout.caption_top,
      layout.maximize_button.right, layout.caption_bottom};
  const HBRUSH background =
      CreateSolidBrush(active ? RGB(0, 0, 128) : RGB(192, 192, 192));
  FillRect(dc, &caption, background);
  DeleteObject(background);

  draw_original_main_caption_button(dc, layout.system_button, true, false);
  draw_original_main_caption_button(dc, layout.minimize_button, false, true);
  draw_original_main_caption_button(dc, layout.maximize_button, false, false);

  wchar_t title[256]{};
  GetWindowTextW(window, title, static_cast<int>(std::size(title)));
  const int previous_background = SetBkMode(dc, TRANSPARENT);
  const COLORREF previous_color =
      SetTextColor(dc, active ? RGB(255, 255, 255) : RGB(0, 0, 0));
  HGDIOBJ previous_font = nullptr;
  if (const HFONT font = simtower::original_cached_font(13)) {
    previous_font = SelectObject(dc, font);
  }
  RECT title_bounds = layout.title;
  DrawTextW(dc, title, -1, &title_bounds,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
                DT_NOPREFIX);
  if (previous_font) SelectObject(dc, previous_font);
  SetTextColor(dc, previous_color);
  SetBkMode(dc, previous_background);
  ReleaseDC(window, dc);
}

std::optional<LRESULT> original_main_caption_hit_test(HWND window,
                                                      LPARAM position) {
  RECT bounds{};
  GetWindowRect(window, &bounds);
  const int x = static_cast<std::int16_t>(LOWORD(position)) - bounds.left;
  const int y = static_cast<std::int16_t>(HIWORD(position)) - bounds.top;
  const auto layout = original_main_caption_layout(window);
  if (y < layout.caption_top || y >= layout.caption_bottom) {
    return std::nullopt;
  }
  const auto contains = [x, y](const RECT& rectangle) {
    return x >= rectangle.left && x < rectangle.right &&
           y >= rectangle.top && y < rectangle.bottom;
  };
  if (contains(layout.system_button)) return HTSYSMENU;
  if (contains(layout.minimize_button)) return HTMINBUTTON;
  if (contains(layout.maximize_button)) return HTMAXBUTTON;
  if (contains(layout.title)) return HTCAPTION;
  return std::nullopt;
}

HINSTANCE g_instance = nullptr;
simtower::OriginalResources g_resources;
HICON g_application_icon = nullptr;
HACCEL g_accelerators = nullptr;
HPALETTE g_logical_palette = nullptr;
std::array<HCURSOR, simtower::kOriginalCustomCursorResourceIds.size()>
    g_original_cursors{};
HWND g_main_window = nullptr;
HWND g_map_window = nullptr;
HWND g_info_window = nullptr;
HWND g_command_window = nullptr;
HWND g_elevator_control_window = nullptr;
HWND g_startup_splash_window = nullptr;
bool g_main_message_runtime_initialized = false;
bool g_original_window_activation_latch = false;  // DS:31a6, Main/ELVDLOGMAIN
bool g_main_paint_active = false;          // DS:0244
bool g_original_main_caption_active = false;
bool g_main_pointer_interaction_armed = false;  // DS:0242
std::optional<simtower::OriginalMainSurfacePass>
    g_main_pending_surface_pass{};
std::uint32_t g_original_modal_dialog_depth = 0U;  // DS:24b8 lock
HWND g_original_active_modal_window = nullptr;     // DS:31a4
simtower::OriginalDtmp g_startup_dtmp;
simtower::OriginalYenTable g_construction_costs{};
simtower::OriginalYenTable g_rent_income{};
simtower::OriginalYenTable g_maintenance_costs{};
simtower::OriginalPartTable g_part{};
std::unique_ptr<simtower::OriginalAudioRuntime> g_audio;

void original_runtime_audio_pump(std::uint32_t milliseconds) {
  if (g_audio) g_audio->pump(milliseconds);
}

void original_runtime_audio_pump() {
  original_runtime_audio_pump(GetTickCount());
}

void run_original_runtime_audio_checkpoints(std::uint8_t count) {
  for (std::uint8_t index = 0U; index < count; ++index) {
    original_runtime_audio_pump();
  }
}

std::uint32_t original_runtime_coarse_tick(std::uint32_t milliseconds) {
  // Native boundary for 1208:05e6: every coarse-clock read first enters
  // 11e0:0e84's 48-ms audio pump, even when no simulation tick is due.
  original_runtime_audio_pump(milliseconds);
  return simtower::original_coarse_tick(milliseconds);
}

std::uint32_t original_runtime_coarse_tick() {
  return original_runtime_coarse_tick(GetTickCount());
}

std::optional<simtower::OriginalTdtDocument> g_tower_document;
std::filesystem::path g_tower_path;
std::wstring g_tower_file_title = L"untitled";
std::wstring g_original_save_directory;  // 1128:04f7, DS:3120[0x80]
bool g_original_beep_only = false;        // 1128:046c, DS:de28
bool g_tower_dirty = false;
bool g_people_animation_enabled = true;   // 1128:0545, DS:de30
bool g_effects_animation_enabled = true;  // 1128:054b, DS:de32
bool g_fast_mode_enabled = true;          // 1128:0551, DS:de34
std::uint32_t g_native_fast_frame_tick{}; // native Win16 host-cadence adapter
bool g_toolbar_visible = true;            // DS:31a8 after window creation
bool g_info_visible = true;               // DS:31aa after window creation
bool g_map_visible = true;                // DS:31ac after window creation
std::uint32_t g_original_last_auxiliary_pointer_tick = 0U;  // DS:31b0/31b2
int g_original_system_font_ascent = 0;     // 1128:1184, DS:31b6
bool g_build_mode_enabled = true;         // DS:783e
bool g_command_toggle_pressed = false;    // 1080:07a6 BITMAP/601 or 603 frame
bool g_main_closing = false;              // DS:31c6
bool g_original_map_pointer_down = false;  // DS:0248
// Native mirror of DS:7796, the last focus rectangle XOR-presented on Map.
std::optional<simtower::OriginalMapRect> g_original_map_focus_rectangle{};
bool g_world_control_modifier = false;    // DS:3218, last mouse wParam & 8
bool g_world_shift_modifier = false;      // DS:321a, last mouse wParam & 4
std::uint16_t g_command_mode = 3U;        // DS:783c
std::uint16_t g_map_mode = 0U;            // DS:7840
bool g_map_drag_active = false;            // DS:3216 for MAPWNDPROC
std::uint16_t g_selected_build_type = 24U;  // mandatory initial Lobby

struct OriginalMainPointerState {
  bool inside{};
  int client_x{};
  int client_y{};
};

OriginalMainPointerState g_main_pointer{};
// Native mirror of DS:77ac, the last construction-outline rectangle derived
// by 11f8:3c13. 1090:03ab compares this rectangle—not the raw mouse point—to
// decide whether its preview-only pass has any pixels to present.
std::optional<simtower::OriginalConstructionPreviewRect>
    g_original_main_preview_rect{};
// Native mirror of DS:025c, the scratch-backing latch shared by
// 11f8:3b94/3c13. It matters even when the final outline pixels are derived
// directly into the native retained RGB surface.
simtower::OriginalConstructionPreviewScratchState
    g_original_main_preview_scratch{};
std::array<simtower::OriginalCommandRatingState, 6> g_command_ratings{};
// Native resource-handle equivalent of DS:7960. 10d0:086c resets the
// document rating before Open I/O but does not replace this table until the
// successful 10d0:0ac2 -> 1140:010d reconstruction boundary.
std::uint16_t g_active_command_rating = 1U;
std::uint16_t g_office_variant = 0U;      // DS:7954, cycles modulo six
// Build type is the raw original type byte (currently through 0x2c). Keep
// selectors addressable by that byte even for one-variant composite types.
std::array<std::uint16_t, 256> g_facility_variants{};
simtower::OriginalSimulationState g_simulation_state{};
simtower::OriginalPaletteRuntime g_palette_runtime{};
simtower::OriginalSkyDecorationState g_sky_decorations{};
simtower::OriginalInfoStatusState g_info_status{};
simtower::OriginalFindMarkerState g_find_marker{};  // DS:77b4..77c0
std::vector<simtower::OriginalElevatorTransferVisual>
    g_elevator_transfer_visuals{};

struct OriginalMainBackingSurface {
  simtower::OriginalWorldRaster raster{};
  simtower::OriginalWorldPoint view{};
  int client_width{};
  int client_height{};
  bool valid{};
  bool dirty{true};
};

// Native equivalent of the persistent DS:3264 WinG bitmap. Ordinary
// MAINWNDPROC WM_PAINT presents this cache without replaying 1080/1090 work.
OriginalMainBackingSurface g_original_main_backing{};

struct OriginalElevatorControlDialogContext {
  simtower::OriginalDtmp dtmp{};
  simtower::OriginalElevatorControlState state{};
  std::int16_t initial_pointer_x{};
  std::int16_t initial_pointer_y{};
  std::int16_t requested_left{8};
  int pressed_item{-1};  // DS:31b4, FFFF while no control is pressed.
};

std::unique_ptr<OriginalElevatorControlDialogContext>
    g_elevator_control_context{};

struct OriginalStartupSplashContext {
  int bitmap_id{128};
  bool modal{};
};

OriginalStartupSplashContext g_startup_splash{};

simtower::OriginalWorldPoint current_original_view(HWND window);
std::optional<simtower::OriginalConstructionPreviewRect>
current_original_main_preview_rect();
void set_original_view(HWND window, int x, int y);
void select_original_map_mode(std::uint16_t mode);
void apply_original_construction_toggle();
void apply_original_auxiliary_window_activation_state(bool active);
int run_original_event_dialog(
    const simtower::OriginalEventDialogRequest& request);
int run_original_transport_information_dialog(
    const simtower::OriginalMagnifierTarget& target);
int run_original_person_information_dialog(HWND owner,
                                           std::size_t person_index,
                                           bool* changed = nullptr,
                                           simtower::OriginalPersonInformationContext context =
                                               simtower::OriginalPersonInformationContext::main_world);
int run_original_facility_information_dialog(
    HWND owner,
    std::int16_t floor,
    std::size_t tenant_index,
    bool* changed = nullptr);
void open_original_elevator_control(std::size_t elevator_index,
                                    std::int16_t pointer_x,
                                    std::int16_t pointer_y);
void consume_original_person_family_dispatches(
    const std::vector<simtower::OriginalPersonFamilyDispatchResult>&
        dispatches);
void consume_original_person_family_dispatch(
    const simtower::OriginalPersonFamilyDispatchResult& dispatch);
void consume_original_person_host_requests(
    const std::vector<simtower::OriginalPersonHostRequest>& requests);
bool advance_original_main_surface_state(
    simtower::OriginalMainSurfacePass pass);
void rebuild_original_main_backing(
    simtower::OriginalMainSurfacePass pass,
    bool advance_state = true,
    bool full_frame_surface_dirty = false);
void present_original_main_backing_direct();
void present_original_info_surface_direct();
void request_original_main_surface_pass(
    simtower::OriginalMainSurfacePass pass,
    bool synchronous);

void invalidate_original_main_surface() {
  g_original_main_backing.dirty = true;
  if (g_main_window) InvalidateRect(g_main_window, nullptr, FALSE);
}

void invalidate_original_surface(HWND window) {
  if (window == g_main_window) {
    invalidate_original_main_surface();
  } else if (window) {
    InvalidateRect(window, nullptr, FALSE);
  }
}

constexpr wchar_t kOriginalModalThunkProperty[] =
    L"SimTower.OriginalModalThunk";

struct OriginalModalDialogThunkContext {
  DLGPROC procedure{};
  LPARAM parameter{};
};

INT_PTR CALLBACK original_modal_dialog_thunk(HWND dialog,
                                             UINT message,
                                             WPARAM wparam,
                                             LPARAM lparam) {
  OriginalModalDialogThunkContext* context{};
  if (message == WM_INITDIALOG) {
    context = reinterpret_cast<OriginalModalDialogThunkContext*>(lparam);
    SetPropW(dialog, kOriginalModalThunkProperty,
             reinterpret_cast<HANDLE>(context));
    // The Win16 dialog filters publish their HWND to DS:31a4 during their
    // initialization branch. Do so before forwarding WM_INITDIALOG so any
    // synchronous palette activation observes the exact modal target.
    g_original_active_modal_window = dialog;
  } else {
    context = reinterpret_cast<OriginalModalDialogThunkContext*>(
        GetPropW(dialog, kOriginalModalThunkProperty));
  }
  if (!context || !context->procedure) return FALSE;
  const LPARAM forwarded_lparam =
      message == WM_INITDIALOG ? context->parameter : lparam;
  const INT_PTR result =
      context->procedure(dialog, message, wparam, forwarded_lparam);
  if (message == WM_NCDESTROY) {
    RemovePropW(dialog, kOriginalModalThunkProperty);
  }
  return result;
}

INT_PTR run_original_modal_dialog(HINSTANCE instance,
                                  const DLGTEMPLATE* dialog_template,
                                  HWND owner,
                                  DLGPROC dialog_proc,
                                  LPARAM parameter) {
  // MAINWNDPROC consults DS:24b8 before hit testing, scrolling, pointer input,
  // commands, and system keys while the original modal manager is active.
  // A depth counter preserves nested Person/Facility/Rename dialog behavior.
  ++g_original_modal_dialog_depth;
  const HWND previous_modal = g_original_active_modal_window;
  OriginalModalDialogThunkContext context{dialog_proc, parameter};
  const INT_PTR result = ::DialogBoxIndirectParamW(
      instance, dialog_template, owner, original_modal_dialog_thunk,
      reinterpret_cast<LPARAM>(&context));
  g_original_active_modal_window = previous_modal;
  --g_original_modal_dialog_depth;
  return result;
}

[[nodiscard]] bool original_main_modal_input_locked() noexcept {
  return g_original_modal_dialog_depth != 0U;
}

void destroy_original_cursors() noexcept {
  for (auto& cursor : g_original_cursors) {
    if (cursor) {
      DestroyCursor(cursor);
      cursor = nullptr;
    }
  }
}

void destroy_original_logical_palette() noexcept {
  if (g_logical_palette) {
    DeleteObject(g_logical_palette);
    g_logical_palette = nullptr;
  }
}

void destroy_original_auxiliary_windows() noexcept {
  // 1258:0095/016e -> 10b8:0000: Map, Info, Command, clearing the matching
  // persisted visibility word after each synchronous DestroyWindow call.
  for (const auto target : simtower::kOriginalAuxiliaryShutdownOrder) {
    HWND* handle = nullptr;
    bool* visible = nullptr;
    switch (target) {
      case simtower::OriginalAuxiliaryWindow::map:
        handle = &g_map_window;
        visible = &g_map_visible;
        break;
      case simtower::OriginalAuxiliaryWindow::info:
        handle = &g_info_window;
        visible = &g_info_visible;
        break;
      case simtower::OriginalAuxiliaryWindow::command:
        handle = &g_command_window;
        visible = &g_toolbar_visible;
        break;
    }
    if (*handle) {
      DestroyWindow(*handle);
      *handle = nullptr;
    }
    *visible = false;
  }
}

void destroy_original_process_resources() noexcept {
  for (const auto action : simtower::original_process_teardown_plan()) {
    switch (action) {
      case simtower::OriginalProcessTeardownAction::
          release_command_and_world_surfaces:
        // Original cached GDI surfaces are immutable embedded DIB views in
        // the native port and therefore carry no live host handle.
        break;
      case simtower::OriginalProcessTeardownAction::release_font_bank:
        simtower::destroy_original_font_cache();
        break;
      case simtower::OriginalProcessTeardownAction::release_cursor_bank:
        destroy_original_cursors();
        break;
      case simtower::OriginalProcessTeardownAction::shutdown_audio:
        if (g_audio) g_audio->shutdown();
        break;
      case simtower::OriginalProcessTeardownAction::release_palette:
        destroy_original_logical_palette();
        break;
      case simtower::OriginalProcessTeardownAction::
          release_runtime_owned_storage:
        g_tower_document.reset();
        break;
    }
  }

  // The accelerator, converted icon, and C++ audio object are native adapter
  // ownership added outside the recovered teardown sequence.
  if (g_accelerators) {
    DestroyAcceleratorTable(g_accelerators);
    g_accelerators = nullptr;
  }
  if (g_application_icon) {
    DestroyIcon(g_application_icon);
    g_application_icon = nullptr;
  }
  g_audio.reset();
}

void create_original_logical_palette() {
  g_logical_palette = simtower::create_original_logical_palette(
      g_palette_runtime.colors);
  if (!g_logical_palette) {
    throw std::runtime_error("Could not create original logical palette");
  }
}

void synchronize_original_logical_palette() {
  if (!g_logical_palette || !g_palette_runtime.initialized) return;
  constexpr std::size_t first = 188U;
  constexpr std::size_t count = 31U;
  const auto original_entries =
      simtower::original_logical_palette_entries(g_palette_runtime.colors);
  std::array<PALETTEENTRY, count> native_entries{};
  for (std::size_t index = 0U; index < count; ++index) {
    const auto& entry = original_entries[first + index];
    native_entries[index] = {
        entry.red, entry.green, entry.blue, entry.flags};
  }
  if (!AnimatePalette(g_logical_palette, static_cast<UINT>(first),
                      static_cast<UINT>(count), native_entries.data())) {
    throw std::runtime_error("Could not animate original logical palette");
  }
}

bool step_native_effect_palette(const simtower::OriginalTdtDocument& document,
                                bool effects_enabled,
                                std::uint32_t now_ms) {
  const bool changed = simtower::step_original_effect_palette(
      document, g_palette_runtime, effects_enabled,
      original_runtime_coarse_tick(now_ms));
  if (changed) synchronize_original_logical_palette();
  return changed;
}

bool refresh_native_time_palette(
    const simtower::OriginalTdtDocument& document) {
  const bool changed = simtower::refresh_original_time_palette(
      g_resources, document, g_palette_runtime);
  if (changed) synchronize_original_logical_palette();
  return changed;
}

HCURSOR resolve_original_cursor(std::uint16_t selector) noexcept {
  switch (selector) {
    case 0U:
      return LoadCursorW(nullptr, IDC_ARROW);
    case 2U:
      return LoadCursorW(nullptr, IDC_CROSS);
    case 4U:
      return LoadCursorW(nullptr, IDC_WAIT);
    case 1000U:
      return LoadCursorW(nullptr, IDC_SIZENS);
    default:
      if (selector >= 1002U && selector <= 1005U) {
        const auto cursor = g_original_cursors[selector - 1002U];
        return cursor ? cursor : LoadCursorW(nullptr, IDC_ARROW);
      }
      return LoadCursorW(nullptr, IDC_ARROW);
  }
}

bool original_screen_point_in_window(HWND window, POINT point) noexcept {
  RECT bounds{};
  return window && GetWindowRect(window, &bounds) && PtInRect(&bounds, point);
}

void update_original_main_cursor_from_screen_point(POINT screen_point) {
  const bool point_in_map =
      original_screen_point_in_window(g_map_window, screen_point);
  const bool point_in_info =
      original_screen_point_in_window(g_info_window, screen_point);
  const bool point_in_command =
      original_screen_point_in_window(g_command_window, screen_point);

  POINT client_point = screen_point;
  RECT client_bounds{};
  const bool converted = g_main_window &&
                         ScreenToClient(g_main_window, &client_point) &&
                         GetClientRect(g_main_window, &client_bounds);
  const bool point_in_main =
      converted && PtInRect(&client_bounds, client_point);
  const auto plan = simtower::original_main_cursor_point_plan(
      point_in_map, g_map_visible,
      point_in_info, g_info_visible,
      point_in_command, g_toolbar_visible,
      point_in_main, g_main_window && IsIconic(g_main_window),
      g_build_mode_enabled, g_command_mode,
      g_main_window && GetCapture() == g_main_window);
  if (plan.set_cursor) {
    SetCursor(resolve_original_cursor(plan.selector));
  }
}

void invalidate_original_info_status() {
  // Exact 1118:0000 boundary: invalidate the complete Info client and force
  // the pending repaint synchronously when finance, load/save, or status text
  // changes. Construction debit routines 1178:01db/027c/0697 reach the same
  // complete 1118:0143 painter through a window DC immediately after mutation.
  if (g_info_window) {
    InvalidateRect(g_info_window, nullptr, FALSE);
    UpdateWindow(g_info_window);
  }
}

void show_original_construction_status(std::uint16_t code) {
  if (simtower::set_original_info_construction_status(
          g_resources, g_info_status, code,
          original_runtime_coarse_tick())) {
    invalidate_original_info_status();
  }
}

void show_original_notification_status(std::uint16_t code) {
  if (simtower::set_original_info_notification_status(
          g_resources, g_info_status, code,
          original_runtime_coarse_tick())) {
    invalidate_original_info_status();
  }
}

void show_original_income_status(std::uint16_t code) {
  if (simtower::set_original_info_income_status(
          g_resources, g_info_status, code,
          original_runtime_coarse_tick())) {
    invalidate_original_info_status();
  }
}

void show_original_command_status(std::uint16_t code) {
  if (simtower::set_original_info_command_status(
          g_resources, g_info_status, code,
          original_runtime_coarse_tick())) {
    invalidate_original_info_status();
  }
}

struct OriginalLobbyDragState {
  bool active{};
  std::int16_t floor{};
  std::int32_t anchor_left{};
  std::int32_t anchor_right{};
};

OriginalLobbyDragState g_lobby_drag{};

struct OriginalFloorDragState {
  bool active{};
  std::int16_t floor{};
  std::int32_t anchor_left{};
  std::int32_t anchor_right{};
};

OriginalFloorDragState g_floor_drag{};

enum class OriginalParkingDragKind : std::uint8_t {
  none,
  parking,
  ramp,
};

struct OriginalParkingDragState {
  OriginalParkingDragKind kind{OriginalParkingDragKind::none};
  std::int16_t press_floor{};
  std::int32_t snapped_left{};
  std::int32_t snapped_right{};
  simtower::OriginalParkingDragRunState parking{};
  simtower::OriginalParkingRampDragRunState ramp{};
};

OriginalParkingDragState g_parking_drag{};

// Native mirrors of 11f8's shared 24cc successful-step counter and 24ce
// down-time balance snapshot. They span Floor, Lobby, Parking, and Ramp and
// are consumed only by the common button-up completion boundary.
struct OriginalConstructionDragCompletionState {
  bool active{};
  std::uint16_t type{};
  std::uint16_t successful_steps{};
  std::int32_t balance_at_press{};
  bool priority_sound_latch_armed{};  // DS:24ca
};

OriginalConstructionDragCompletionState g_construction_drag_completion{};
LPARAM g_retained_construction_press_position{};
bool g_retained_construction_press_position_valid{};
std::optional<simtower::OriginalLobbyPlacement>
    g_retained_construction_placement{};

enum class OriginalElevatorShaftDragDirection : std::uint8_t {
  none,
  upper,
  lower,
};

struct OriginalElevatorShaftDragState {
  bool capture_active{};  // DS:02a6, set by every 10a0:0544 finger press
  OriginalElevatorShaftDragDirection direction{
      OriginalElevatorShaftDragDirection::none};
  std::size_t elevator_index{};
};

OriginalElevatorShaftDragState g_elevator_shaft_drag{};

simtower::OriginalCommandRatingState& active_original_command_rating(
    std::uint16_t rating) {
  const std::uint16_t bounded = std::clamp<std::uint16_t>(rating, 1U, 6U);
  auto& state = g_command_ratings[bounded - 1U];
  if (state.encoded_entries.empty()) {
    state = simtower::original_command_rating_state(g_resources, bounded);
  }
  return state;
}

void resize_original_command_window(std::uint16_t rating) {
  if (!g_command_window) {
    return;
  }
  const auto count = simtower::original_command_catalog(
                         g_resources, active_original_command_rating(rating))
                         .size();
  RECT outer{};
  GetWindowRect(g_command_window, &outer);
  // 1140:010d passes the result of 1140:03f8 directly to MoveWindow as the
  // outer height, preserving the existing screen position and width.
  MoveWindow(g_command_window, outer.left, outer.top,
             outer.right - outer.left,
             simtower::original_rating_window_height(
                 static_cast<std::uint16_t>(count)),
             TRUE);
}

void reset_original_command_state(bool startup_suppressed = false) {
  // 10d0:086c initializes these before both the New and Open reconstruction
  // chains. The selected build shape is the mandatory first Lobby while mode
  // index 3 is what the original command surface highlights. At
  // 10d0:08cc-08e1, DS:31c4 keeps construction disabled only for the
  // pre-dialog bootstrap invoked by 1128:00c4.
  g_build_mode_enabled = !startup_suppressed;
  g_command_toggle_pressed = false;
  g_command_mode = 3U;
  g_map_mode = 0U;
  g_map_drag_active = false;
  g_selected_build_type = 24U;
  g_office_variant = 0U;
  g_facility_variants.fill(0U);
  g_lobby_drag = {};
  g_floor_drag = {};
  g_parking_drag = {};
  g_construction_drag_completion = {};
  g_retained_construction_press_position_valid = false;
  g_retained_construction_placement.reset();
  g_original_main_preview_scratch = {};
  g_info_status = {};
  simtower::reset_original_find_marker(g_find_marker);
  // 10d0:086c changes the document/edit globals only. Rating resource
  // replacement, Command geometry/presentation, preview restoration, and the
  // derived Map focus adjustment occur later in 10d0:0ac2. In particular, Open
  // must not repaint a fresh rating-one command palette before disk transfer
  // has succeeded.
}

void refresh_original_rating_command(std::uint16_t rating,
                                     std::uint16_t argument,
                                     bool command_composition_allowed = true) {
  const bool elevator_isolation_active =
      g_elevator_control_context &&
      g_elevator_control_context->state.isolation_active;
  const auto plan = simtower::original_rating_command_refresh_plan(
      argument, elevator_isolation_active,
      g_original_main_preview_rect.has_value());

  // Native keeps immutable value views for 1140:010d's rating TABL and
  // 11f8:06cd's rating-dependent facility sheets, so selecting the cached
  // rating state replaces both Win16 resource-handle transactions.
  auto& rating_state = active_original_command_rating(rating);
  g_active_command_rating = rating_state.rating;
  resize_original_command_window(g_active_command_rating);
  if (plan.force_command_mode_two) g_command_mode = 2U;
  if (plan.present_command_synchronously && command_composition_allowed &&
      g_command_window) {
    InvalidateRect(g_command_window, nullptr, FALSE);
    UpdateWindow(g_command_window);
  }
  if (plan.restore_preview_scratch) {
    g_original_main_preview_rect.reset();
  }
  if (plan.rebuild_visible_tile_scratch) {
    // 1038:0000 rebuilds a transient indexed tile cache. Native derives that
    // cache from the document on demand, so invalidate only its retained RGB
    // transport; do not invent a synchronous Main presentation here.
    g_original_main_backing.dirty = true;
  }
}

void draw_original_map_focus_rectangle(
    HDC dc, const simtower::OriginalMapRect& focus) {
  if (!dc) return;
  RECT native_focus{focus.left,
                    focus.top + simtower::kOriginalMapClientTop,
                    focus.right,
                    focus.bottom + simtower::kOriginalMapClientTop};
  DrawFocusRect(dc, &native_focus);
}

void adjust_original_derived_map_focus_synchronously() {
  // 10d0:0ac2 ends its reconstruction with 1080:055d's direct-DC old/new
  // XOR focus transaction. It does not enter MAPWNDPROC or redraw the Map
  // backing, toolbar, frame, or audio-pump boundaries.
  if (!g_map_window || !g_main_window) return;
  HDC dc = GetDC(g_map_window);
  if (!dc) return;
  std::optional<simtower::OriginalMapRect> recomputed{};
  for (const auto step : simtower::original_derived_map_focus_order()) {
    switch (step) {
      case simtower::OriginalDerivedMapFocusStep::erase_previous_focus:
        if (g_original_map_focus_rectangle) {
          draw_original_map_focus_rectangle(
              dc, *g_original_map_focus_rectangle);
        }
        break;
      case simtower::OriginalDerivedMapFocusStep::recompute_focus: {
        RECT main_client{};
        GetClientRect(g_main_window, &main_client);
        const auto view = current_original_view(g_main_window);
        recomputed = simtower::original_map_view_rect(
            view.x, view.y, main_client.right - main_client.left,
            main_client.bottom - main_client.top);
        break;
      }
      case simtower::OriginalDerivedMapFocusStep::draw_recomputed_focus:
        if (recomputed) {
          draw_original_map_focus_rectangle(dc, *recomputed);
          g_original_map_focus_rectangle = *recomputed;
        }
        break;
    }
  }
  ReleaseDC(g_map_window, dc);
}

void reset_original_simulation_state() {
  g_elevator_transfer_visuals.clear();
  g_original_main_preview_rect.reset();
  g_original_main_backing = {};
  g_original_main_backing.dirty = true;
  g_main_pending_surface_pass.reset();
  if (!g_tower_document) {
    g_simulation_state = {};
    g_palette_runtime = {};
    return;
  }
  // Complete exact 10b0:0000 New/Open reset chain: pending construction,
  // tenant records, parking/commercial tables, Elevator/Stair transients, then
  // people records. This intentionally precedes the process-local clock state.
  simtower::reset_original_loaded_simulation_state(*g_tower_document);
  const auto now = GetTickCount();
  g_simulation_state.frame_time = g_tower_document->header.frame_time;
  g_simulation_state.current_day = g_tower_document->header.current_day;
  g_simulation_state.day_phase =
      simtower::original_day_phase(g_simulation_state.frame_time);
  g_simulation_state.calendar_phase =
      simtower::original_calendar_phase(g_simulation_state.current_day);
  g_simulation_state.last_tick = original_runtime_coarse_tick(now);
  // Make the first Fast Mode frame immediately eligible. Subsequent frames
  // retain the observed Win16 full-frame host cadence instead of running at
  // the speed of the native PeekMessage busy loop.
  g_native_fast_frame_tick =
      now - simtower::kNativeFastModeFramePeriodMs;
  // 10d0:086c clears DS:77c2 while the initialization chain retains the
  // current tick in DS:7772. 1020:098b then selects the document-time colors.
  simtower::reset_original_palette_runtime(
      g_resources, &*g_tower_document, g_palette_runtime,
      original_runtime_coarse_tick(now));
  synchronize_original_logical_palette();
}

void play_original_event_sounds(
    std::span<const simtower::OriginalEventSoundRequest> requests) {
  if (!g_audio) {
    return;
  }
  for (const auto& request : requests) {
    if (request.reserved_if_idle) {
      (void)g_audio->play_reserved_if_idle(
          request.resource, request.repeat, GetTickCount());
    } else {
      (void)g_audio->play_resource(request.resource, request.repeat,
                                   request.priority, GetTickCount());
    }
  }
}

bool apply_original_event_damage(
    std::span<const simtower::OriginalFacilityDamageRequest> requests) {
  if (!g_tower_document || requests.empty()) {
    return false;
  }
  const auto result = simtower::apply_original_facility_damage_sequence(
      *g_tower_document, g_rent_income, requests);
  play_original_event_sounds(result.sound_requests);
  for (const auto code : result.notification_codes) {
    show_original_notification_status(code);
  }
  return result.changed != 0U;
}

void focus_original_event_coordinate(std::int16_t floor, std::int16_t x) {
  RECT client{};
  GetClientRect(g_main_window, &client);
  const auto view = simtower::original_facility_focus_view(
      x, floor, client.right - client.left, client.bottom - client.top);
  set_original_view(g_main_window, view.x, view.y);
}

void set_original_fire_menu_enabled(bool enabled) {
  if (!g_main_window) {
    return;
  }
  if (HMENU menu = GetMenu(g_main_window)) {
    // 0x9c48 is the literal command ID. Win16 and Win32 both define zero as
    // MF_ENABLED and one as MF_GRAYED.
    EnableMenuItem(menu, 0x9c48U,
                   MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_GRAYED));
  }
}

bool consume_original_bomb_action(
    const simtower::OriginalEventActionResult& action) {
  if (!g_tower_document) {
    return false;
  }
  bool changed = action.changed;
  const auto host_plan = simtower::original_bomb_event_host_plan(action);
  for (const auto operation : host_plan.sequence()) {
    switch (operation) {
      case simtower::OriginalEventHostOperation::play_sounds:
        play_original_event_sounds(action.sound_requests);
        break;
      case simtower::OriginalEventHostOperation::apply_damage:
        changed = apply_original_event_damage(action.damage_requests) ||
                  changed;
        break;
      case simtower::OriginalEventHostOperation::dispatch_security:
        simtower::dispatch_original_security_response(
            *g_tower_document, action.security_dispatch_flags);
        changed = true;
        break;
      case simtower::OriginalEventHostOperation::focus_coordinate:
        focus_original_event_coordinate(action.focus_floor, action.focus_x);
        break;
      case simtower::OriginalEventHostOperation::show_dialog:
        (void)run_original_event_dialog(action.dialog);
        break;
      case simtower::OriginalEventHostOperation::complete_deferred_action:
        simtower::complete_original_event_action(
            *g_tower_document, action.deferred_completion);
        changed = true;
        break;
      case simtower::OriginalEventHostOperation::update_fire_menu:
        break;
    }
  }
  return changed;
}

bool consume_original_fire_action(
    const simtower::OriginalEventActionResult& action) {
  if (!g_tower_document) {
    return false;
  }
  bool changed = action.changed;
  const auto host_plan = simtower::original_fire_event_host_plan(action);
  for (const auto operation : host_plan.sequence()) {
    switch (operation) {
      case simtower::OriginalEventHostOperation::apply_damage:
        changed = apply_original_event_damage(action.damage_requests) ||
                  changed;
        break;
      case simtower::OriginalEventHostOperation::play_sounds:
        play_original_event_sounds(action.sound_requests);
        break;
      case simtower::OriginalEventHostOperation::update_fire_menu:
        set_original_fire_menu_enabled(*action.fire_menu_enabled);
        break;
      case simtower::OriginalEventHostOperation::show_dialog:
        (void)run_original_event_dialog(action.dialog);
        break;
      case simtower::OriginalEventHostOperation::complete_deferred_action:
        simtower::complete_original_event_action(
            *g_tower_document, action.deferred_completion);
        changed = true;
        break;
      case simtower::OriginalEventHostOperation::dispatch_security:
      case simtower::OriginalEventHostOperation::focus_coordinate:
        break;
    }
  }
  return changed;
}

bool consume_original_fire_crew_resolution(
    const simtower::OriginalFireCrewResolution& resolution) {
  if (!g_tower_document || !resolution.handled) {
    return false;
  }
  if (resolution.followup_dialog.valid()) {
    (void)run_original_event_dialog(resolution.followup_dialog);
  }
  if (resolution.security_dispatch_pending) {
    simtower::dispatch_original_security_response(
        *g_tower_document, resolution.security_dispatch_flags);
  }
  if (resolution.focus_requested) {
    focus_original_event_coordinate(resolution.focus_floor,
                                    resolution.focus_x);
  }
  if (resolution.fire_menu_enabled) {
    set_original_fire_menu_enabled(*resolution.fire_menu_enabled);
  }
  return resolution.hired || resolution.security_dispatch_pending;
}

bool run_original_bomb_offer_boundary() {
  if (!g_tower_document) return false;
  const auto offer = simtower::prepare_original_bomb_event(
      *g_tower_document, g_part);
  if (!offer.offered) return false;
  const auto choice = static_cast<std::uint16_t>(
      run_original_event_dialog(offer.dialog));
  const auto resolution = simtower::resolve_original_bomb_event(
      *g_tower_document, g_part, choice);
  if (resolution.direct_wave_resource != 0 && g_audio) {
    (void)g_audio->play_resource(
        resolution.direct_wave_resource, 0, 4, GetTickCount());
  }
  if (resolution.paid) {
    // 10c8:014f-016a plays WAVE/10015 before 1178:07e8 performs its direct
    // Info balance paint. Paying never raises the active Bomb/world flag.
    invalidate_original_info_status();
  }
  if (resolution.followup_dialog.valid()) {
    (void)run_original_event_dialog(resolution.followup_dialog);
  }
  if (resolution.started) {
    simtower::commit_original_bomb_event(*g_tower_document);
  }
  return resolution.started;
}

bool run_original_fire_offer_boundary() {
  if (!g_tower_document) return false;
  const auto fire = simtower::prepare_original_fire_event(
      *g_tower_document, g_part, g_simulation_state.day_phase);
  if (!fire.offered) return false;
  (void)run_original_event_dialog(fire.dialog);
  simtower::commit_original_fire_event(*g_tower_document, g_part);
  focus_original_event_coordinate(fire.floor, fire.x);
  return true;
}

void run_original_idle_simulation() {
  const auto finish_original_idle_audio_pass = [] {
    // 1258:02d9 always ends through 11c8:0135(0). The zero force word makes
    // both 11c8:02c0 channel calls deliberate no-ops; preserve that boundary
    // without turning it into native completion cleanup or forced stopping.
    if (g_audio) g_audio->stop_all(false);
  };

  // 1258:0195-01c3 reconciles WAVMIX activation on every empty-queue pass,
  // not only on WM_SIZE. In particular, losing main-window activation must
  // stop all live channels even when the window was not minimized.
  if (g_audio) {
    switch (simtower::original_idle_audio_transition(
        g_original_window_activation_latch,
        g_main_window && IsIconic(g_main_window), g_audio->active())) {
      case simtower::OriginalIdleAudioTransition::activate:
        g_audio->activate();
        break;
      case simtower::OriginalIdleAudioTransition::deactivate:
        g_audio->deactivate();
        break;
      case simtower::OriginalIdleAudioTransition::none:
        break;
    }
  }

  // 1258:01c3-023a continuously repairs the original TOPMOST Command/elevator
  // ordering. Its -1 insert-after value is Win16 TOPMOST; the elevator branch
  // deliberately omits NOACTIVATE while main is active.
  const HWND active_window = GetActiveWindow();
  const auto order = simtower::original_idle_window_order_plan(
      g_original_window_activation_latch,
      g_elevator_control_window != nullptr,
      active_window == g_command_window, g_toolbar_visible,
      active_window == g_elevator_control_window);
  if (order.promote_command_topmost && g_command_window) {
    SetWindowPos(g_command_window, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
  }
  switch (order.elevator) {
    case simtower::OriginalIdleElevatorWindowOrder::
        promote_topmost_and_activate:
      SetWindowPos(g_elevator_control_window, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOSIZE | SWP_NOMOVE);
      break;
    case simtower::OriginalIdleElevatorWindowOrder::insert_behind_main:
      SetWindowPos(g_elevator_control_window, g_main_window, 0, 0, 0, 0,
                   SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
      break;
    case simtower::OriginalIdleElevatorWindowOrder::none:
      break;
  }

  // 1258:023a invokes 1020:00cb on every empty-queue pass, before both the
  // Find-marker timeout and 1200:0196's six-coarse-tick (~96-ms) simulation
  // gate. The
  // original indexed palette changes in place; native direct rasters need an
  // explicit repaint when this independent pass advances the palette.
  if (g_tower_document &&
      step_native_effect_palette(
          *g_tower_document, g_effects_animation_enabled, GetTickCount())) {
    invalidate_original_main_surface();
    InvalidateRect(g_map_window, nullptr, FALSE);
  }

  // 1258:0186 invokes 10e0:051d on every empty-queue pass, independently of
  // whether the simulation clock advances. An active Map overlay exits via
  // 11d0:0000; otherwise 1058:033c performs the ordinary construction toggle.
  auto expired_marker = g_find_marker;
  if (simtower::expire_original_find_marker(
          expired_marker,
          original_runtime_coarse_tick())) {
    g_find_marker = expired_marker;
    apply_original_construction_toggle();
  }
  if (!g_tower_document) {
    finish_original_idle_audio_pass();
    return;
  }
  const bool elevator_finger_capture_active =
      g_elevator_shaft_drag.capture_active;
  const auto idle_gate = simtower::original_idle_world_pass_plan(
      g_build_mode_enabled, elevator_finger_capture_active, false);
  if (!idle_gate.run_scheduler) {
    finish_original_idle_audio_pass();
    return;
  }

  if (idle_gate.sample_cursor) {
    POINT cursor{};
    if (GetCursorPos(&cursor) && ScreenToClient(g_main_window, &cursor)) {
      // 1258:0256 passes the converted point even when it lies outside the
      // client. 11f8:3c13 performs the later client intersection itself.
      g_main_pointer.inside = true;
      g_main_pointer.client_x = cursor.x;
      g_main_pointer.client_y = cursor.y;
    }
  }
  // 1090:03ab saves DS:77ac before either its preview-only derivation or its
  // complete frame. A stationary point can still produce a different shape
  // after tool, mode, or viewport state changes, so preserve the rectangle.
  const auto previous_preview_rect = g_original_main_preview_rect;
  simtower::OriginalSimulationStep step{};
  const auto scheduler_now_ms = GetTickCount();
  if (!g_fast_mode_enabled || simtower::native_fast_mode_frame_due(
                                  g_native_fast_frame_tick,
                                  scheduler_now_ms)) {
    step = simtower::step_original_simulation(
        g_simulation_state, *g_tower_document,
        original_runtime_coarse_tick(scheduler_now_ms),
        g_fast_mode_enabled);
    if (g_fast_mode_enabled && step.advanced) {
      g_native_fast_frame_tick = scheduler_now_ms;
    }
  }
  const auto world_pass = simtower::original_idle_world_pass_plan(
      g_build_mode_enabled, elevator_finger_capture_active,
      step.advanced);
  if (!step.advanced) {
    if (world_pass.run_preview_only_pass) {
      // 1090:03bf-0427 always restores/derives DS:77ac, then EQUALRECT alone
      // decides whether 1158:0ae5 presents the changed rectangle. Keep this
      // repaint out of the full-frame RNG/person-animation path.
      const bool isolation_active =
          g_elevator_control_context &&
          g_elevator_control_context->state.isolation_active;
      const auto restore =
          simtower::original_construction_preview_restore_plan(
              g_original_main_preview_scratch, isolation_active,
              previous_preview_rect.has_value());
      run_original_runtime_audio_checkpoints(restore.audio_checkpoints);
      if (restore.clear_previous_rectangle) {
        g_original_main_preview_rect.reset();
      }
      const auto current_preview_rect =
          current_original_main_preview_rect();
      const auto draw = simtower::original_construction_preview_draw_plan(
          g_original_main_preview_scratch, g_build_mode_enabled,
          g_command_mode);
      const auto preview_presentation =
          simtower::original_preview_only_presentation_plan(
              previous_preview_rect == current_preview_rect);
      if (preview_presentation.present_main_direct) {
        // 1090:03fb-0427 owns a Main DC and calls 1158:0ae5 immediately;
        // this is not a queued WM_PAINT. Rebuild the native RGB transport
        // cache without presentation-state advancement, then synchronously
        // present the changed result before the idle pass returns.
        g_main_pending_surface_pass.reset();
        rebuild_original_main_backing(
            simtower::OriginalMainSurfacePass::preview_repaint, false);
      } else {
        g_original_main_preview_rect = current_preview_rect;
      }
      // 11f8:3c99/3cd2/3d19 surround the scratch capture and outline draw;
      // all three still run when EQUALRECT later suppresses presentation.
      run_original_runtime_audio_checkpoints(draw.audio_checkpoints);
      if (preview_presentation.present_main_direct) {
        present_original_main_backing_direct();
      }
    }
    finish_original_idle_audio_pass();
    return;
  }
  g_main_pending_surface_pass.reset();
  if (step.day_changed) {
    // 1200:04d4-04fe synchronously repaints 1118:045d immediately after the
    // day/calendar update, before the ordinary 1090 frame pass.
    InvalidateRect(g_info_window, nullptr, FALSE);
    UpdateWindow(g_info_window);
  }

  // 1258:026e calls the scheduler first, including each translated far call,
  // then 1090:03ab(1); 1090:042f is the construction-queue pass.
  bool world_changed = false;
  for (const auto& call : step.calls) {
    if (!simtower::original_simulation_call_supported(call)) {
      throw std::runtime_error(
          "Recovered scheduler emitted an unsupported native call");
    }
    if (call.selector == 0x11c8U && call.offset == 0x0167U &&
        call.arguments.size() == 3U) {
      if (g_audio) {
        (void)g_audio->play_resource(
            call.arguments[0], call.arguments[1],
            static_cast<std::uint16_t>(call.arguments[2]), GetTickCount());
      }
    } else if (call.selector == 0x11c8U && call.offset == 0x03abU) {
      RECT client{};
      GetClientRect(g_main_window, &client);
      const auto view = current_original_view(g_main_window);
      const auto resource = simtower::select_original_ambient_sound(
          *g_tower_document, g_audio && g_audio->sound_enabled(), view.x,
          view.y, client.right - client.left, client.bottom - client.top);
      if (resource && g_audio) {
        (void)g_audio->play_resource(*resource, 0, 1, GetTickCount());
      }
    } else if (call.selector == 0x1228U && call.offset == 0x0968U) {
      (void)simtower::advance_original_facilities_for_day(
          *g_tower_document, g_simulation_state.calendar_phase);
      world_changed = true;
    } else if (call.selector == 0x1228U && call.offset == 0x0b59U) {
      (void)simtower::advance_original_facilities_for_evening(
          *g_tower_document);
      world_changed = true;
    } else if (call.selector == 0x1228U && call.offset == 0x086bU) {
      (void)simtower::prepare_original_facilities_for_night(
          *g_tower_document);
      world_changed = true;
    } else if (call.selector == 0x1198U && call.offset == 0x01abU) {
      simtower::refresh_original_parking_for_day(*g_tower_document);
    } else if (call.selector == 0x1170U && call.offset == 0x011fU) {
      simtower::refresh_original_medical_for_day(*g_tower_document);
      world_changed = true;
    } else if (call.selector == 0x1020U && call.offset == 0x0dcbU) {
      world_changed = simtower::raise_original_periodic_b406_flag(
                          *g_tower_document) ||
                      world_changed;
    } else if (call.selector == 0x1020U && call.offset == 0x0e0bU) {
      (void)simtower::clear_original_periodic_b406_flag(*g_tower_document);
      // 1020:0e1f sets the original window-dirty word unconditionally.
      world_changed = true;
    } else if (call.selector == 0x1240U && call.offset == 0x01deU) {
      (void)simtower::reset_original_periodic_b924_state(*g_tower_document);
    } else if (call.selector == 0x1060U && call.offset == 0x003aU) {
      simtower::reset_original_quarter_finance(*g_tower_document);
    } else if (call.selector == 0x1178U && call.offset == 0x0b44U) {
      simtower::charge_original_three_day_maintenance(
          *g_tower_document, g_maintenance_costs, g_part);
      world_changed = true;
    } else if (call.selector == 0x1180U && call.offset == 0x05afU) {
      simtower::reset_original_entertainment_for_day(
          *g_tower_document, g_part);
      world_changed = true;
    } else if (call.selector == 0x1180U && call.offset == 0x06a8U &&
               call.arguments.size() == 2U) {
      simtower::begin_original_entertainment_arrivals(
          *g_tower_document,
          static_cast<std::uint16_t>(call.arguments[0]),
          static_cast<std::uint16_t>(call.arguments[1]));
      world_changed = true;
    } else if (call.selector == 0x1180U && call.offset == 0x0826U &&
               call.arguments.size() == 2U) {
      simtower::advance_original_entertainment_show(
          *g_tower_document,
          static_cast<std::uint16_t>(call.arguments[0]),
          static_cast<std::uint16_t>(call.arguments[1]));
      world_changed = true;
    } else if (call.selector == 0x1180U && call.offset == 0x090aU &&
               call.arguments.size() == 2U) {
      const auto income = simtower::finish_original_entertainment_phase(
          *g_tower_document, g_part,
          static_cast<std::uint16_t>(call.arguments[0]),
          static_cast<std::uint16_t>(call.arguments[1]));
      for (const auto code : income.codes) {
        show_original_income_status(code);
      }
      world_changed = true;
    } else if (call.selector == 0x11a8U && call.offset == 0x0184U) {
      simtower::reset_original_commercial_for_day(
          *g_tower_document, g_part);
      world_changed = true;
    } else if (call.selector == 0x11a8U && call.offset == 0x0250U) {
      simtower::reset_original_restaurants_for_evening(
          *g_tower_document, g_part);
      world_changed = true;
    } else if (call.selector == 0x11a8U && call.offset == 0x0554U) {
      const auto income = simtower::close_original_nonrestaurant_commercial(
          *g_tower_document, g_part);
      for (const auto code : income.codes) {
        show_original_income_status(code);
      }
      world_changed = true;
    } else if (call.selector == 0x11a8U && call.offset == 0x0603U) {
      const auto income = simtower::close_original_restaurants_for_night(
          *g_tower_document, g_part);
      for (const auto code : income.codes) {
        show_original_income_status(code);
      }
      world_changed = true;
    } else if (call.selector == 0x1130U && call.offset == 0x0000U) {
      simtower::advance_original_tenants_at_midnight(
          *g_tower_document, g_part, g_rent_income);
      world_changed = true;
    } else if (call.selector == 0x1130U && call.offset == 0x0109U) {
      simtower::advance_original_hotels_for_evening(
          *g_tower_document, g_part);
      world_changed = true;
    } else if (call.selector == 0x1130U && call.offset == 0x01e2U) {
      world_changed =
          simtower::repair_original_hotel_pair_states(
              *g_tower_document, g_simulation_state.day_phase) != 0U ||
          world_changed;
    } else if (call.selector == 0x1188U && call.offset == 0x0977U) {
      (void)simtower::remove_original_nightly_person_links(*g_tower_document);
    } else if (call.selector == 0x1188U && call.offset == 0x0a20U) {
      (void)simtower::remove_original_hotel_person_links(*g_tower_document);
    } else if (call.selector == 0x1088U && call.offset == 0x00deU) {
      const auto result =
          simtower::reset_original_recycling_for_day(*g_tower_document);
      if (result.play_transition_sound && g_audio) {
        (void)g_audio->play_resource(0x08e8, 0, 1, GetTickCount());
      }
      if (result.notification_code != 0U) {
        show_original_notification_status(result.notification_code);
      }
      world_changed = world_changed || result.touched != 0U;
    } else if (call.selector == 0x1088U && call.offset == 0x01d1U) {
      world_changed =
          simtower::finish_original_recycling_day_start(*g_tower_document) !=
              0U ||
          world_changed;
    } else if (call.selector == 0x1088U && call.offset == 0x0000U &&
               call.arguments.size() == 1U) {
      const auto result = simtower::advance_original_recycling_phase(
          *g_tower_document,
          static_cast<std::uint8_t>(call.arguments[0]));
      if (result.notification_code != 0U) {
        show_original_notification_status(result.notification_code);
      }
      world_changed = world_changed || result.touched != 0U;
    } else if (call.selector == 0x11e8U && call.offset == 0x0273U) {
      const auto result =
          simtower::pulse_original_metro_effects(*g_tower_document);
      if (result.play_transition_sound && g_audio) {
        (void)g_audio->play_resource(0x271a, 0, 1, GetTickCount());
      }
      world_changed = world_changed || result.touched != 0U;
    } else if (call.selector == 0x11b8U && call.offset == 0x0028U) {
      const auto result =
          simtower::start_original_annual_effect(*g_tower_document);
      if (result.notification_code != 0U) {
        show_original_notification_status(result.notification_code);
      }
      world_changed = world_changed || result.started;
    } else if (call.selector == 0x1040U && call.offset == 0x0000U) {
      (void)simtower::reset_original_cathedral_for_day(*g_tower_document);
      world_changed = true;
    } else if (call.selector == 0x1040U && call.offset == 0x0179U) {
      (void)simtower::close_original_cathedral_for_day(*g_tower_document);
      world_changed = true;
    } else if (call.selector == 0x1220U && call.offset == 0x0000U) {
      (void)simtower::reset_original_people_for_day(*g_tower_document);
    } else if (call.selector == 0x1220U && call.offset == 0x1059U) {
      (void)simtower::sweep_original_people_transit(
          *g_tower_document, g_simulation_state.frame_time);
      world_changed = true;
    } else if (call.selector == 0x10c8U && call.offset == 0x006eU) {
      world_changed = run_original_bomb_offer_boundary() || world_changed;
    } else if (call.selector == 0x10e8U && call.offset == 0x0029U) {
      world_changed = run_original_fire_offer_boundary() || world_changed;
    } else {
      throw std::runtime_error(
          "Recovered scheduler call has no native host dispatch");
    }
  }
  // 1200:0529 samples 1208:05e6 only after all scheduled far calls return.
  // In particular, a midnight/event modal postpones the next 96-ms gate from
  // its close time; committing the entry sample would advance immediately
  // after a long modal. This tail precedes the separate 1090:03ab full frame.
  simtower::finish_original_simulation_step(
      g_simulation_state, original_runtime_coarse_tick());
  // Direct 1090:042a checkpoint before the full-frame construction queue.
  original_runtime_audio_pump();
  // Exact leading order inside 1090:03ab(1): 11f0:0211 construction queue,
  // then rating, Bomb/Fire, Security, normal people, Elevator cars, annual
  // effects, and ambient audio in their recovered host order.
  const auto pending =
      simtower::step_original_pending_construction(*g_tower_document);
  world_changed = world_changed ||
                   pending ==
                       simtower::OriginalPendingStepStatus::activated;
  // Direct 1090:0434 checkpoint between construction activation and rating.
  original_runtime_audio_pump();

  // Exact 1090:0439 position: after the pending-construction pass and before
  // Bomb/Fire dispatch, 1140:002d evaluates one population-rating step and
  // 1148:007e enforces its persisted prerequisites. The modal is a host-only
  // boundary; all rating/VIP/latch mutation has already occurred headlessly.
  const auto rating = simtower::step_original_rating_progress(
      *g_tower_document, g_part, g_simulation_state.calendar_phase,
      g_simulation_state.day_phase);
  if (rating.notification_code) {
    show_original_notification_status(*rating.notification_code);
  }
  if (rating.promoted) {
    // 1140:00a8 is the rating-six form of this same promotion boundary: set
    // the final rating, present its modal, select the rating command layout,
    // resize Command, and repaint Info. Native state mutation occurs in the
    // headless rating step immediately above; these calls provide its UI half.
    g_tower_dirty = true;
    InvalidateRect(g_info_window, nullptr, FALSE);
    UpdateWindow(g_info_window);
    if (rating.dialog.valid()) {
      (void)run_original_event_dialog(rating.dialog);
    }
    // Both 1140:002d and the rating-six 1140:00a8 pass one to 1140:010d.
    // That argument resets DS:783c to edit mode two before synchronously
    // presenting the newly selected Command surface; it does not preserve the
    // active construction tool and does not present or invalidate Map here.
    refresh_original_rating_command(
        g_tower_document->header.rating, 1U);
    world_changed = true;
  }
  // Direct 1090:043e checkpoint after rating (including its modal boundary)
  // and before Bomb/Fire progression.
  original_runtime_audio_pump();

  const auto bomb =
      simtower::advance_original_bomb_event(*g_tower_document, g_part);
  world_changed = consume_original_bomb_action(bomb) || world_changed;

  if (simtower::original_fire_event_active(*g_tower_document)) {
    if (g_tower_document->header.frame_time == 2000U) {
      world_changed = consume_original_fire_action(
                          simtower::finish_original_fire_event(
                              *g_tower_document)) ||
                      world_changed;
    } else {
      if (simtower::original_fire_crew_offer_due(*g_tower_document, g_part)) {
        const auto choice = static_cast<std::uint16_t>(
            run_original_event_dialog(
                simtower::original_fire_crew_offer(g_part)));
        const auto resolution = simtower::resolve_original_fire_crew_offer(
            *g_tower_document, g_part, choice, true);
        world_changed =
            consume_original_fire_crew_resolution(resolution) || world_changed;
      }
      world_changed = consume_original_fire_action(
                          simtower::advance_original_fire_event(
                              *g_tower_document, g_part)) ||
                      world_changed;
    }
  }

  // First 1020:00cb call at 1090:0448, after Bomb/Fire and before Security.
  // The original calls the same 15-coarse-tick (~240-ms)-gated routine four
  // times per frame;
  // long frames can therefore advance it again between expensive passes.
  bool native_palette_recolor_required = step_native_effect_palette(
      *g_tower_document, g_effects_animation_enabled, GetTickCount());
  // Direct 1090:044d checkpoint after the first in-frame effects pass.
  original_runtime_audio_pump();

  if (simtower::original_emergency_people_pass_active(
          *g_tower_document)) {
    // 1090:0452-045e selects the type-14 emergency tenant/person scan only
    // while Bomb bit zero or Fire bit three is active. The translated family
    // returns 1080:0000 requests explicitly so this host boundary remains the
    // only view mutation.
    const auto security =
        simtower::step_original_security_people(*g_tower_document, g_part);
    for (const auto& effect : security.effects) {
      focus_original_event_coordinate(effect.floor, effect.x);
    }
    world_changed = security.changed != 0U || world_changed;
  } else {
    // With both event bits clear, 1090:0460 runs the normal one-sixteenth
    // person pass instead of 0f85. The translated Hotel, commercial,
    // Housekeeping, Metro, Cathedral, and Movie/Party wrappers share one
    // ordered scan so route, finance, service, and queue mutations retain
    // original person-index order. Cross-family Elevator timeouts execute the
    // translated 1637/1b41/1aed ring path in that same pass.
    const auto people =
        simtower::step_original_translated_people(
            *g_tower_document, g_part, g_rent_income);
    consume_original_person_host_requests(people.host_requests);
    if (people.cathedral_ceremony) {
      const auto& ceremony = *people.cathedral_ceremony;
      focus_original_event_coordinate(ceremony.effect_floor,
                                      ceremony.effect_x);
      if (g_audio) {
        if (ceremony.stop_both_audio_channels) {
          g_audio->stop_all(true);
        }
        if (ceremony.wave_resource != 0) {
          (void)g_audio->play_resource(
              ceremony.wave_resource, ceremony.wave_repeat,
              ceremony.wave_priority, GetTickCount());
        }
      }
    }
    world_changed = people.changed != 0U || world_changed;
  }

  // Second 1020:00cb call at 1090:0465, after the normal people pass.
  native_palette_recolor_required =
      step_native_effect_palette(
          *g_tower_document, g_effects_animation_enabled, GetTickCount()) ||
      native_palette_recolor_required;
  // Direct 1090:046a checkpoint after the selected people pass and second
  // effects pass.
  original_runtime_audio_pump();

  // 1090:046f-047c calls 1080:09c3 every sixteen clock ticks. That routine
  // invalidates and synchronously repaints the Map even when no simulation
  // mutation raised the shared world-dirty flag, advancing BITMAP/352's
  // horizontally cycled background on its original cadence.
  if (simtower::original_map_animation_refresh_due(
          g_tower_document->header.frame_time)) {
    InvalidateRect(g_map_window, nullptr, FALSE);
    UpdateWindow(g_map_window);
  }
  // Direct 1090:0481 checkpoint after the independent Map cadence branch.
  original_runtime_audio_pump();
  // 1090:04a7 enters 11f8:3b94 after saving the old DS:77ac rectangle. Its
  // two nested checkpoints occur only when a nonempty outline is actually
  // restored outside Elevator-Control isolation.
  const bool elevator_isolation_active =
      g_elevator_control_context &&
      g_elevator_control_context->state.isolation_active;
  const auto preview_restore =
      simtower::original_construction_preview_restore_plan(
          g_original_main_preview_scratch, elevator_isolation_active,
          previous_preview_rect.has_value());
  run_original_runtime_audio_checkpoints(
      preview_restore.audio_checkpoints);
  if (preview_restore.clear_previous_rectangle) {
    g_original_main_preview_rect.reset();
  }
  // Direct 1090:04b1 checkpoint after the backing/annual scratch restore and
  // before the per-Elevator state loops.
  original_runtime_audio_pump();

  // Exact 1090:04c0-0542 order: each used Elevator advances all eight cars,
  // runs all eight 1210:07a6/0351 passenger pairs, and enters 11e0:0e84
  // before the next Elevator. 10a8's process-local cache owns two entries per
  // visible floor/Elevator pair and consumes them in one top-to-bottom,
  // left-to-right paint.
  const auto elevators = simtower::step_original_elevator_frame(
      *g_tower_document, g_part, g_rent_income,
      g_elevator_control_context &&
          g_elevator_control_context->state.isolation_active,
      simtower::OriginalElevatorFrameHostHooks{
          [] {
            if (g_audio) {
              (void)g_audio->play_resource(
                  0x1772U, 0U, 0U, GetTickCount());
            }
          },
          [](const simtower::OriginalPersonFamilyDispatchResult& dispatch) {
            consume_original_person_family_dispatch(dispatch);
          },
          [] { original_runtime_audio_pump(); }});
  g_elevator_transfer_visuals.clear();
  for (const auto& event : elevators.transfer_visuals) {
    g_elevator_transfer_visuals.push_back(
        simtower::OriginalElevatorTransferVisual{
            event.elevator_index, event.floor, event.boarding,
            event.direction_up,
            static_cast<std::uint32_t>(event.person_index)});
  }
  world_changed = elevators.changed || world_changed;
  // Exact 1090:0551 position: 10c0:002e raises the shared repaint flag when
  // any used Stair/Escalator has a nonzero wrapping word_6+word_8 passenger
  // count. This keeps its frame_time-selected BITMAP animation advancing on
  // frames where no unrelated simulation mutation dirties the world.
  world_changed =
      simtower::original_vertical_transport_animation_active(
          *g_tower_document) ||
      world_changed;
  // Direct 1090:0556 checkpoint after the Stair/Escalator dirty scan.
  original_runtime_audio_pump();
  // 1090:0567 advances the visible 1038:050e facility-person presentation
  // after Elevator/Stair work and before the later annual/palette tail. It
  // draws into DS:3264 directly; an unrelated WM_PAINT never repeats it.
  world_changed = advance_original_main_surface_state(
                      simtower::OriginalMainSurfacePass::simulation_frame) ||
                  world_changed;
  // 1090:05db invokes 11b8:0060 later in the same frame, including the frame
  // that starts the annual effect.
  world_changed =
      simtower::advance_original_annual_effect(*g_tower_document) ||
      world_changed;

  // 1090:05ee redraws the construction outline after every world layer and
  // before either 1020:098b or the third 1020:00cb palette call. Preserve the
  // pre-preview dirty word for 0580's conditional renderer family; EQUALRECT
  // contributes only to the later 061f presentation test.
  const auto current_preview_rect = current_original_main_preview_rect();
  const bool preview_changed =
      previous_preview_rect != current_preview_rect;
  const bool renderer_surface_dirty = world_changed;
  rebuild_original_main_backing(
      simtower::OriginalMainSurfacePass::simulation_frame, false,
      renderer_surface_dirty);
  const auto preview_draw =
      simtower::original_construction_preview_draw_plan(
          g_original_main_preview_scratch, g_build_mode_enabled,
          g_command_mode);
  run_original_runtime_audio_checkpoints(preview_draw.audio_checkpoints);
  world_changed = preview_changed || world_changed;

  // 1090:060f/0615 applies the time palette and then makes the third
  // independently gated 1020:00cb effect-animation call. Both mutate one
  // persistent logical palette, including the Effects-menu freeze behavior.
  const bool palette_changed =
      refresh_native_time_palette(*g_tower_document);
  const bool effect_palette_changed = step_native_effect_palette(
      *g_tower_document, g_effects_animation_enabled, GetTickCount());
  native_palette_recolor_required =
      palette_changed || effect_palette_changed ||
      native_palette_recolor_required;
  if (native_palette_recolor_required) {
    // Win16 recolors the already-retained indexed bitmap in place. Rebuild
    // native's RGB transport without replaying any simulation/checkpoint work.
    rebuild_original_main_backing(
        simtower::OriginalMainSurfacePass::palette_repaint, false);
  }
  // Direct 1090:061a checkpoint after time-palette realization and the third
  // in-frame effects pass.
  original_runtime_audio_pump();

  // Event completion writes the shared b3de clock directly. Keep the native
  // scheduler mirror synchronized so the following tick cannot overwrite it.
  g_simulation_state.frame_time = g_tower_document->header.frame_time;
  g_simulation_state.current_day = g_tower_document->header.current_day;
  g_simulation_state.day_phase =
      simtower::original_day_phase(g_simulation_state.frame_time);
  g_simulation_state.calendar_phase =
      simtower::original_calendar_phase(g_simulation_state.current_day);
  g_original_main_preview_rect = current_preview_rect;
  auto presentation =
      simtower::original_full_frame_auxiliary_presentation_plan(world_changed);
  if (native_palette_recolor_required) {
    // AnimatePalette recolors every visible indexed surface without touching
    // DS:31cc. Native's true-color transport needs an explicit equivalent,
    // but must not feed that adapter-only need back into 0580's dirty gate.
    presentation.present_main_direct = true;
    presentation.invalidate_command = true;
    presentation.invalidate_map = true;
    InvalidateRect(g_map_window, nullptr, FALSE);
  }
  for (const auto tail_step : simtower::original_full_frame_tail_order()) {
    switch (tail_step) {
      case simtower::OriginalFullFrameTailStep::present_main_if_dirty:
        if (presentation.present_main_direct) {
          // 1090:062c-0648 obtains the Main DC and calls 1158:0a3c directly.
          // Keep this out of WM_PAINT so exposure handling stays independent.
          present_original_main_backing_direct();
        }
        break;
      case simtower::OriginalFullFrameTailStep::
          invalidate_command_if_dirty:
        if (presentation.invalidate_command) {
          // 1090:064d-065b invalidates only DS:325a Command after Main.
          InvalidateRect(g_command_window, nullptr, FALSE);
        }
        break;
      case simtower::OriginalFullFrameTailStep::present_info_direct:
        if (presentation.present_info_direct) {
          // 1090:0661-06cd always owns an Info DC and paints 1118:073d before
          // its conditional fields, without queueing a whole-window paint.
          present_original_info_surface_direct();
        }
        break;
      case simtower::OriginalFullFrameTailStep::expire_info_status:
        // 1118:08f3 runs only after the unconditional Info pass, so the text
        // remains visible in this frame and clears for the next presentation.
        if (simtower::expire_original_info_status(
                g_info_status, original_runtime_coarse_tick())) {
          invalidate_original_info_status();
        }
        break;
      case simtower::OriginalFullFrameTailStep::step_final_effect_palette: {
        // Win16 AnimatePalette recolors retained indexed pixels immediately.
        // Native RGB must rematerialize/present only when this fourth call at
        // 1090:06dc actually advances the effects palette.
        const bool changed = step_native_effect_palette(
            *g_tower_document, g_effects_animation_enabled, GetTickCount());
        if (changed) {
          rebuild_original_main_backing(
              simtower::OriginalMainSurfacePass::palette_repaint, false);
          present_original_main_backing_direct();
          InvalidateRect(g_map_window, nullptr, FALSE);
        }
        break;
      }
      case simtower::OriginalFullFrameTailStep::final_audio_checkpoint:
        // 1090:06e1 restores the WinG backing selection before 06ed's final
        // direct 11e0:0e84 checkpoint.
        original_runtime_audio_pump();
        break;
    }
  }

  if (simtower::original_idle_elevator_refresh_required(
          true, g_elevator_control_window != nullptr,
          g_elevator_control_context != nullptr,
          g_elevator_control_context != nullptr)) {
    // 1258:0285-02c9 performs this synchronously after 1090:03ab(1), even
    // when no elevator mutation raised the world's dirty word. Native's full
    // direct-DIB paint replaces 1098:1a5b's backing-bitmap refresh plus the
    // following UpdateWindow.
    InvalidateRect(g_elevator_control_window, nullptr, FALSE);
    UpdateWindow(g_elevator_control_window);
  }
  finish_original_idle_audio_pass();
}

constexpr wchar_t kTdtFilter[] =
    L"SimTower Data (*.TDT)\0*.TDT\0all files (*.*)\0*.*\0\0";
constexpr wchar_t kOpenTdtTitle[] = L"Open SimTower data file";
constexpr wchar_t kSaveTdtTitle[] = L"Save SimTower data file";
constexpr wchar_t kTdtDefaultExtension[] = L"TDT";
constexpr char kInvalidFilename[] = "That is not a valid filename.";
constexpr char kSaveDialogTitle[] = "SimTower for Windows";
constexpr char kReplaceExisting[] =
    "This file name already exists. Do you want to replace it?";

constexpr char kWaveMixFailureMessage[] =
    "SimTower did not detect a sound card or proper sound drivers on this "
    "system.  Do you want to run ";

simtower::OriginalWorldPoint current_original_view(HWND window) {
  SCROLLINFO horizontal{sizeof(horizontal), SIF_POS};
  SCROLLINFO vertical{sizeof(vertical), SIF_POS};
  GetScrollInfo(window, SB_HORZ, &horizontal);
  GetScrollInfo(window, SB_VERT, &vertical);
  return {horizontal.nPos, vertical.nPos};
}

std::optional<simtower::OriginalConstructionPreviewRect>
current_original_main_preview_rect() {
  if (!g_main_window || !g_main_pointer.inside ||
      !g_build_mode_enabled || g_command_mode < 3U) {
    return std::nullopt;
  }
  const auto view = current_original_view(g_main_window);
  return simtower::original_construction_preview_rect(
      g_selected_build_type, g_main_pointer.client_x,
      g_main_pointer.client_y, view.x, view.y);
}

void present_original_document_transition_synchronously() {
  // 10d0:001d and the success tail of 10d0:062a both execute 0ac2 followed
  // by 1080:0a02 and 1118:0000. Keep the two Map presentations distinct:
  // 0ac2 adjusts the restored focus first, then 0a02 performs the separate
  // full Map repaint between Main and Command.
  for (const auto step :
       simtower::original_document_transition_refresh_order()) {
    switch (step) {
      case simtower::OriginalDocumentTransitionRefreshStep::
          derived_map_focus_adjustment:
        adjust_original_derived_map_focus_synchronously();
        break;
      case simtower::OriginalDocumentTransitionRefreshStep::fire_menu_update:
        set_original_fire_menu_enabled(
            g_tower_document &&
            simtower::original_fire_crew_menu_enabled_after_rebuild(
                *g_tower_document));
        break;
      case simtower::OriginalDocumentTransitionRefreshStep::
          main_rebuild_with_sky:
        request_original_main_surface_pass(
            simtower::OriginalMainSurfacePass::rebuild_with_sky, true);
        break;
      case simtower::OriginalDocumentTransitionRefreshStep::map_repaint:
        if (g_map_window) {
          InvalidateRect(g_map_window, nullptr, FALSE);
          UpdateWindow(g_map_window);
        }
        break;
      case simtower::OriginalDocumentTransitionRefreshStep::command_repaint:
        if (g_command_window) {
          InvalidateRect(g_command_window, nullptr, FALSE);
          UpdateWindow(g_command_window);
        }
        break;
      case simtower::OriginalDocumentTransitionRefreshStep::info_repaint:
        invalidate_original_info_status();
        break;
    }
  }
}

void set_original_view(HWND window, int x, int y) {
  SCROLLINFO info{sizeof(info), SIF_POS};
  info.nPos = x;
  SetScrollInfo(window, SB_HORZ, &info, TRUE);
  info.nPos = y;
  SetScrollInfo(window, SB_VERT, &info, TRUE);
  if (window == g_main_window) {
    // 1080:0000/0054 complete camera publication through 1080:0a02, then
    // run 1080:055d's separate old/new XOR focus adjustment.
    for (const auto step : simtower::original_camera_refresh_order()) {
      switch (step) {
        case simtower::OriginalCameraRefreshStep::main_rebuild_with_sky:
          request_original_main_surface_pass(
              simtower::OriginalMainSurfacePass::rebuild_with_sky, true);
          break;
        case simtower::OriginalCameraRefreshStep::map_repaint:
          if (g_map_window) {
            InvalidateRect(g_map_window, nullptr, FALSE);
            UpdateWindow(g_map_window);
          }
          break;
        case simtower::OriginalCameraRefreshStep::command_repaint:
          if (g_command_window) {
            InvalidateRect(g_command_window, nullptr, FALSE);
            UpdateWindow(g_command_window);
          }
          break;
        case simtower::OriginalCameraRefreshStep::map_focus_adjustment:
          adjust_original_derived_map_focus_synchronously();
          break;
      }
    }
  } else {
    invalidate_original_surface(window);
  }
}

void restore_original_derived_view_position(HWND window, int x, int y) {
  // Position-only half of 1080:00d7. SetScrollInfo performs the Win16
  // SetScrollPos range clamp; 10d0:0ac2 then adjusts the retained Map focus
  // through 1080:055d, and its New/Open caller invokes 1080:0a02 afterward.
  SCROLLINFO info{sizeof(info), SIF_POS};
  info.nPos = x;
  SetScrollInfo(window, SB_HORZ, &info, TRUE);
  info.nPos = y;
  SetScrollInfo(window, SB_VERT, &info, TRUE);
  g_original_main_backing.dirty = true;
}

void keep_original_pointer_visible(HWND window,
                                   int client_x,
                                   int client_y,
                                   bool horizontal_axis) {
  RECT client{};
  if (!GetClientRect(window, &client)) return;
  const auto view = current_original_view(window);
  const auto adjusted = simtower::original_keep_pointer_visible(
      view,
      {client.left, client.top, client.right, client.bottom},
      {client_x, client_y}, horizontal_axis);
  if (adjusted == view) return;

  // 1080:0054 commits through 1080:00d7 (the scrollbar clamps the raw
  // position) and immediately executes 1080:055d's focus transaction.
  set_original_view(window, adjusted.x, adjusted.y);
}

void play_original_construction_success_audio(std::uint16_t type) {
  const auto plan = simtower::original_construction_success_audio_plan(
      type, g_original_beep_only);
  // 11f8:0e21-0e67 clears the status first, then tests DS:de28 for the exact
  // BeepOnly value one before applying the five-type WAVE/7000 exclusion.
  if (plan.beep) {
    MessageBeep(static_cast<UINT>(-1));
  }
  if (plan.play_general_wave && g_audio) {
    (void)g_audio->play_resource(7000, 0, 4);
  }
}

void apply_original_construction_mutation_presentation(
    HWND window,
    bool document_changed,
    bool balance_changed) {
  const auto plan =
      simtower::original_world_mutation_presentation_plan(
          document_changed, balance_changed);
  if (plan.mark_document_dirty) {
    g_tower_dirty = true;
  }
  if (plan.invalidate_main_surface) {
    // 1038:002f marks the affected cells in the Win16 tile caches. Native's
    // retained RGB transport derives those cells from the document on demand.
    invalidate_original_surface(window);
  }
  if (plan.repaint_info_balance_synchronously) {
    invalidate_original_info_status();
  }
  if (plan.invalidate_map_surface && g_map_window) {
    InvalidateRect(g_map_window, nullptr, FALSE);
  }
}

bool apply_original_lobby_result(
    HWND window,
    const simtower::OriginalConstructionResult& result,
    std::int32_t old_balance,
    std::uint16_t old_lobby_height,
    bool continuous_drag = false) {
  const bool priority_sound_latch_armed =
      continuous_drag && g_construction_drag_completion.active &&
      g_construction_drag_completion.priority_sound_latch_armed;
  const auto helper_completion =
      simtower::original_floor_lobby_helper_completion_plan(
          result.succeeded(), result.construction_sound_requested,
          priority_sound_latch_armed);
  if (helper_completion.sound ==
          simtower::OriginalCapturedHelperSound::priority_five &&
      g_audio) {
    (void)g_audio->play_resource(7001, 0, 5);
  } else if (helper_completion.sound ==
                 simtower::OriginalCapturedHelperSound::reserved_if_idle &&
             g_audio) {
    (void)g_audio->play_reserved_if_idle(7001, 0, GetTickCount());
  }
  if (helper_completion.clear_priority_sound_latch) {
    g_construction_drag_completion.priority_sound_latch_armed = false;
  }
  if (!result.succeeded()) {
    if (result.construction_status_code != 0U) {
      show_original_construction_status(result.construction_status_code);
    }
    // 11f8:0ec0 uses WAVE/7002 for a rejected single-click command. The
    // continuous Floor/Lobby/Parking/Ramp branches return through 11f8:0e09 and
    // defer one WAVE/7002 to button-up only if 24cc has no accepted step.
    if (!continuous_drag && g_audio) {
      (void)g_audio->play_resource(7002, 0, 4);
    }
    const bool changed = result.document_changed ||
        g_tower_document &&
        (g_tower_document->header.balance != old_balance ||
         g_tower_document->header.lobby_height != old_lobby_height);
    if (changed) {
      const bool balance_changed =
          g_tower_document && g_tower_document->header.balance != old_balance;
      apply_original_construction_mutation_presentation(
          window, true, balance_changed);
    }
    return changed;
  }
  if (continuous_drag && g_construction_drag_completion.active &&
      helper_completion.increment_successful_step) {
    ++g_construction_drag_completion.successful_steps;
  }
  if (!continuous_drag) {
    // 11f8:0e21 clears STRL/1003, 0e29 applies BeepOnly, and 0e58 plays the
    // type-filtered command-success sound.
    show_original_construction_status(0U);
    play_original_construction_success_audio(g_selected_build_type);
  }
  // 1148:0163 is downstream of 07d8's common success boundary. Captured
  // tools return at 0e02 after each accepted step and reach that boundary
  // only once, on their successful button-up.
  const auto rating_completion =
      !continuous_drag && g_tower_document
          ? simtower::complete_original_rating_construction(
                *g_tower_document, g_part, g_selected_build_type)
          : simtower::OriginalRatingConstructionResult{};
  if (rating_completion.dialog.valid()) {
    (void)run_original_event_dialog(rating_completion.dialog);
  }
  if (!continuous_drag && g_tower_document &&
      simtower::original_construction_rebuilds_routes(
          g_selected_build_type)) {
    // Literal 0ed8 table: 1148:0163 precedes 11b0:049f then 11b0:00f2.
    simtower::rebuild_original_transport_route_graphs(*g_tower_document);
  }
  const bool changed = result.document_changed || rating_completion.changed ||
      g_tower_document &&
      (g_tower_document->header.balance != old_balance ||
       g_tower_document->header.lobby_height != old_lobby_height);
  if (changed) {
    const bool balance_changed =
        g_tower_document && g_tower_document->header.balance != old_balance;
    apply_original_construction_mutation_presentation(
        window, true, balance_changed);
  }
  return changed;
}

void mark_original_world_interaction_changed(HWND window) {
  const auto plan = simtower::original_world_mutation_presentation_plan(
      true, false);
  if (plan.mark_document_dirty) g_tower_dirty = true;
  if (plan.invalidate_main_surface) invalidate_original_surface(window);
  if (plan.repaint_info_balance_synchronously) {
    invalidate_original_info_status();
  }
  if (plan.invalidate_map_surface && g_map_window) {
    InvalidateRect(g_map_window, nullptr, FALSE);
  }
}

void run_original_facility_bulldozer(HWND window, LPARAM position) {
  if (!g_tower_document) return;
  const auto old_balance = g_tower_document->header.balance;
  const auto view = current_original_view(window);
  const int client_x = static_cast<std::int16_t>(LOWORD(position));
  const int client_y = static_cast<std::int16_t>(HIWORD(position));

  // Exact 1058:0084/0097/00aa precedence: an Elevator or vertical transport
  // owns the click before 11f8:0793 is allowed to inspect floor facilities.
  // Elevator demolition owns the click before vertical/facility handling.
  const auto elevator_hit = simtower::original_elevator_hit_from_client(
      *g_tower_document, client_x, client_y, view.x, view.y);
  if (elevator_hit.hit) {
    const auto& elevator =
        g_tower_document->elevators[elevator_hit.elevator_index];
    const auto action = simtower::original_elevator_bulldozer_action(
        true, elevator_hit.car_index, elevator.cars, elevator_hit.floor,
        elevator.bottom_floor, elevator.top_floor, elevator.word_3c);
    if (action == simtower::OriginalElevatorBulldozerAction::consume) {
      return;
    }
    if (action == simtower::OriginalElevatorBulldozerAction::remove_car ||
        action == simtower::OriginalElevatorBulldozerAction::remove_shaft) {
      simtower::OriginalElevatorDemolitionResult demolition{};
      if (action == simtower::OriginalElevatorBulldozerAction::remove_car) {
        demolition = simtower::remove_original_elevator_car(
            *g_tower_document, elevator_hit.elevator_index,
            static_cast<std::size_t>(elevator_hit.car_index), g_part,
            g_rent_income);
      } else {
        // A cap click, a click outside the stored span, or a click on the
        // last car enters 0179's serviced-floor connectivity confirmation.
        if (simtower::
                original_elevator_shaft_demolition_requires_confirmation(
                    *g_tower_document, elevator_hit.elevator_index)) {
          const auto text = simtower::original_strl_entry(
              g_resources.find("STRL", 1005), 5U);
          if (simtower::show_original_alert(
                  window, g_resources, 1005, {text, {}, {}, {}}) != 1) {
            return;
          }
        }
        demolition = simtower::remove_original_elevator_shaft(
            *g_tower_document, elevator_hit.elevator_index, g_part,
            g_rent_income);
      }
      if (demolition.removed) {
        consume_original_person_family_dispatches(
            demolition.family_dispatches);
        if (g_audio) {
          (void)g_audio->play_resource(7003, 0, 4);
        }
        mark_original_world_interaction_changed(window);
        if (g_tower_document->header.balance != old_balance) {
          invalidate_original_info_status();
        }
      }
      return;
    }
    // The pass-through result is the literal zero returned by 0201 when an
    // in-span shaft-body hit has word_3c == 0. Preserve 1058's next two legs.
  }
  const auto vertical_hit =
      simtower::original_vertical_transport_hit_from_client(
          *g_tower_document, client_x, client_y, view.x, view.y);
  if (vertical_hit.hit) {
    const auto demolition = simtower::remove_original_vertical_transport(
        *g_tower_document, vertical_hit.transport_index, g_part,
        g_rent_income);
    if (demolition.removed) {
      consume_original_person_family_dispatches(
          demolition.family_dispatches);
      if (g_audio) {
        (void)g_audio->play_resource(7003, 0, 4);
      }
      mark_original_world_interaction_changed(window);
      if (g_tower_document->header.balance != old_balance) {
        invalidate_original_info_status();
      }
    }
    return;
  }
  const auto click = simtower::apply_original_facility_click_damage(
      *g_tower_document, g_rent_income, client_x, client_y, view.x, view.y);
  if (!click.hit.hit) return;
  const auto& result = click.damage;
  play_original_event_sounds(result.sound_requests);
  if (result.alert_code != 0) {
    show_original_construction_status(
        static_cast<std::uint16_t>(result.alert_code));
  }
  for (const auto code : result.notification_codes) {
    show_original_notification_status(code);
  }
  if (result.changed) {
    mark_original_world_interaction_changed(window);
    if (g_tower_document->header.balance != old_balance) {
      // Exact 1178:0697 direct demolition/structure debit repaint.
      invalidate_original_info_status();
    }
  }
}

void run_original_magnifying_glass(HWND window, LPARAM position) {
  if (!g_tower_document) return;
  const auto view = current_original_view(window);
  const auto target = simtower::select_original_magnifier_target(
      *g_tower_document,
      static_cast<std::int16_t>(LOWORD(position)),
      static_cast<std::int16_t>(HIWORD(position)), view.x, view.y);
  switch (target.kind) {
    case simtower::OriginalMagnifierTargetKind::elevator_car_information:
    case simtower::OriginalMagnifierTargetKind::vertical_transport_information:
      (void)run_original_transport_information_dialog(target);
      break;
    case simtower::OriginalMagnifierTargetKind::elevator_control:
      open_original_elevator_control(
          target.elevator_index,
          static_cast<std::int16_t>(LOWORD(position)),
          static_cast<std::int16_t>(HIWORD(position)));
      break;
    case simtower::OriginalMagnifierTargetKind::facility_information:
      (void)run_original_facility_information_dialog(
          window, target.floor, target.tenant_index);
      break;
    case simtower::OriginalMagnifierTargetKind::none:
      break;
    case simtower::OriginalMagnifierTargetKind::waiting_person_information:
      (void)run_original_person_information_dialog(
          window, target.person_index);
      break;
  }
}

bool apply_original_shift_replacement(
    HWND window,
    WPARAM keys,
    std::uint16_t selected_type,
    const simtower::OriginalLobbyPlacement& placement) {
  if (!g_tower_document || (keys & MK_SHIFT) == 0U) return true;
  const auto old_balance = g_tower_document->header.balance;
  const auto result = simtower::apply_original_replacement_demolition(
      *g_tower_document, g_rent_income, selected_type, placement.floor,
      static_cast<std::int16_t>(placement.left),
      static_cast<std::int16_t>(placement.right));
  play_original_event_sounds(result.sound_requests);
  for (const auto code : result.alert_codes) {
    if (code != 0) {
      show_original_construction_status(static_cast<std::uint16_t>(code));
    }
  }
  for (const auto code : result.notification_codes) {
    show_original_notification_status(code);
  }
  if (result.changed != 0U) {
    mark_original_world_interaction_changed(window);
    if (g_tower_document->header.balance != old_balance) {
      invalidate_original_info_status();
    }
  }
  return result.completed;
}

void consume_original_person_family_dispatches(
    const std::vector<simtower::OriginalPersonFamilyDispatchResult>&
        dispatches) {
  // 0883's process-only callback work is played in the same dispatch order as
  // the original. Persisted mutation remains entirely in the translation
  // layer; this host boundary performs only Info, modal, focus, and audio work.
  for (const auto& dispatch : dispatches) {
    consume_original_person_family_dispatch(dispatch);
  }
}

void consume_original_person_family_dispatch(
    const simtower::OriginalPersonFamilyDispatchResult& dispatch) {
  consume_original_person_host_requests(dispatch.host_requests);
  if (dispatch.security_effect.valid()) {
    focus_original_event_coordinate(dispatch.security_effect.floor,
                                    dispatch.security_effect.x);
  }
  if (dispatch.cathedral_arrival &&
      dispatch.cathedral_arrival->status ==
          simtower::OriginalCathedralArrivalStatus::ceremony_started) {
    const auto& ceremony = *dispatch.cathedral_arrival;
    focus_original_event_coordinate(ceremony.effect_floor,
                                    ceremony.effect_x);
    if (g_audio) {
      if (ceremony.stop_both_audio_channels) g_audio->stop_all(true);
      if (ceremony.wave_resource != 0) {
        (void)g_audio->play_resource(
            ceremony.wave_resource, ceremony.wave_repeat,
            ceremony.wave_priority, GetTickCount());
      }
    }
  }
}

void consume_original_person_host_requests(
    const std::vector<simtower::OriginalPersonHostRequest>& requests) {
  for (const auto& request : requests) {
    switch (request.kind) {
      case simtower::OriginalPersonHostRequestKind::income_status:
        show_original_income_status(request.code);
        break;
      case simtower::OriginalPersonHostRequestKind::notification_status:
        show_original_notification_status(request.code);
        break;
      case simtower::OriginalPersonHostRequestKind::hotel_dialog:
        (void)run_original_event_dialog(
            {request.code, request.argument, 10000});
        break;
    }
  }
}

void run_original_elevator_finger(HWND window, LPARAM position) {
  if (!g_tower_document) return;
  const auto view = current_original_view(window);
  const int client_x = static_cast<std::int16_t>(LOWORD(position));
  const int client_y = static_cast<std::int16_t>(HIWORD(position));
  const auto hit = simtower::original_elevator_hit_from_client(
      *g_tower_document, client_x, client_y, view.x, view.y);
  if (!hit.hit) return;

  if (simtower::add_original_elevator_service_floor(
          *g_tower_document, hit.elevator_index, hit.floor)) {
    // 1038:0000 only rebuilds the original's transient per-view tile cache.
    // The native renderer derives those frames directly, so invalidation is
    // its equivalent after the persisted graph mutation.
    mark_original_world_interaction_changed(window);
    return;
  }

  const auto& elevator =
      g_tower_document->elevators[hit.elevator_index];
  if (hit.floor < 0 || hit.floor >= 120 ||
      elevator.serviced_floors[static_cast<std::size_t>(hit.floor)] ==
          std::byte{0}) {
    return;
  }

  const auto warning = simtower::original_elevator_service_floor_warning_code(
      *g_tower_document, hit.elevator_index, hit.floor);
  if (warning != 0U) {
    const auto text = simtower::original_strl_entry(
        g_resources.find("STRL", 1005), warning);
    if (simtower::show_original_alert(
            window, g_resources, 1005, {text, {}, {}, {}}) == 2) {
      return;
    }
  }
  const auto removal = simtower::remove_original_elevator_service_floor(
      *g_tower_document, hit.elevator_index, hit.floor, g_part,
      g_rent_income);
  if (removal.cleanup.status ==
      simtower::OriginalElevatorFloorPeopleCleanupStatus::cleaned) {
    consume_original_person_family_dispatches(removal.family_dispatches);
    mark_original_world_interaction_changed(window);
  }
}

void begin_original_elevator_finger(HWND window, LPARAM position) {
  if (!g_tower_document) return;
  const auto view = current_original_view(window);
  const int client_x = static_cast<std::int16_t>(LOWORD(position));
  const int client_y = static_cast<std::int16_t>(HIWORD(position));
  const auto hit = simtower::original_elevator_hit_from_client(
      *g_tower_document, client_x, client_y, view.x, view.y);
  const auto* elevator = hit.hit
      ? &g_tower_document->elevators[hit.elevator_index]
      : nullptr;
  const auto path = simtower::original_elevator_finger_press_path(
      hit.hit, elevator && elevator->word_3c != 0U, hit.floor,
      elevator ? elevator->bottom_floor : 0,
      elevator ? elevator->top_floor : 0);
  if (path == simtower::OriginalElevatorFingerPressPath::service_floor) {
    // 1058:00d9 first gives an in-span, initialized shaft to 10a0:0000.
    // That path consumes the click even when a service-floor mutation is
    // rejected, so it never enters 10a0:0544's capture transaction.
    run_original_elevator_finger(window, position);
    return;
  }

  // 10a0:0562-059f captures every otherwise-unhandled Finger press and sets
  // DS:02a6 before scanning the shaft caps. Even a press in empty space keeps
  // the idle scheduler suppressed until the matching button-up.
  g_elevator_shaft_drag.capture_active = true;
  SetCapture(window);
  if (path ==
      simtower::OriginalElevatorFingerPressPath::capture_upper_cap) {
    // 10a0:0544 stores direction one for the upper cap, captures the mouse,
    // and feeds every subsequent point through 07b7 -> 0819.
    g_elevator_shaft_drag = {
        true, OriginalElevatorShaftDragDirection::upper, hit.elevator_index};
    SetCursor(resolve_original_cursor(1000U));
    return;
  }
  if (path ==
      simtower::OriginalElevatorFingerPressPath::capture_lower_cap) {
    g_elevator_shaft_drag = {
        true, OriginalElevatorShaftDragDirection::lower, hit.elevator_index};
    SetCursor(resolve_original_cursor(1000U));
    return;
  }
}

void update_original_elevator_finger(HWND window, LPARAM position) {
  if (!g_tower_document ||
      g_elevator_shaft_drag.direction ==
          OriginalElevatorShaftDragDirection::none) {
    return;
  }
  const auto index = g_elevator_shaft_drag.elevator_index;
  if (index >= g_tower_document->elevators.size()) return;

  const auto view = current_original_view(window);
  const int client_x = static_cast<std::int16_t>(LOWORD(position));
  const int client_y = static_cast<std::int16_t>(HIWORD(position));
  // Exact 10a0:07b7 captured-shaft pointer transform: add the live view,
  // convert world y through the 120-story/36-pixel grid, then offset one floor
  // inward before dispatching the 0819/0b87 upper/lower transaction.
  const auto pointer = simtower::original_floor_placement_from_client(
      client_x, client_y, view.x, view.y);
  const auto target = static_cast<std::int16_t>(
      pointer.floor +
      (g_elevator_shaft_drag.direction ==
               OriginalElevatorShaftDragDirection::upper
           ? -1
           : 1));
  const auto old_top = g_tower_document->elevators[index].top_floor;
  const auto old_bottom = g_tower_document->elevators[index].bottom_floor;
  const auto old_balance = g_tower_document->header.balance;
  bool succeeded = false;
  if (g_elevator_shaft_drag.direction ==
      OriginalElevatorShaftDragDirection::upper) {
    if (target > old_top) {
      succeeded = simtower::extend_original_elevator_shaft(
          *g_tower_document, index, target, g_construction_costs).succeeded();
    } else {
      const auto shrink = simtower::shrink_original_elevator_shaft(
          *g_tower_document, index,
          simtower::OriginalElevatorShaftEnd::upper, target, g_part,
          g_rent_income);
      succeeded = shrink.succeeded();
      if (succeeded) {
        consume_original_person_family_dispatches(shrink.family_dispatches);
      }
    }
  } else {
    if (target < old_bottom) {
      succeeded = simtower::extend_original_elevator_shaft(
          *g_tower_document, index, target, g_construction_costs).succeeded();
    } else {
      const auto shrink = simtower::shrink_original_elevator_shaft(
          *g_tower_document, index,
          simtower::OriginalElevatorShaftEnd::lower, target, g_part,
          g_rent_income);
      succeeded = shrink.succeeded();
      if (succeeded) {
        consume_original_person_family_dispatches(shrink.family_dispatches);
      }
    }
  }
  if (succeeded &&
      (g_tower_document->elevators[index].top_floor != old_top ||
       g_tower_document->elevators[index].bottom_floor != old_bottom)) {
    mark_original_world_interaction_changed(window);
    if (g_tower_document->header.balance != old_balance) {
      invalidate_original_info_status();
    }
    keep_original_pointer_visible(window, client_x, client_y, true);
  }
}

void continue_original_elevator_finger(HWND window, LPARAM position) {
  if (!g_tower_document) return;
  if (g_elevator_shaft_drag.direction !=
      OriginalElevatorShaftDragDirection::none) {
    update_original_elevator_finger(window, position);
    return;
  }

  // 10a0:0667-079a scans for a cap on every armed left-button move while
  // DS:0080 has no direction. This can begin a shaft drag after the initial
  // press occurred in empty space or was consumed by 10a0:0000 in-span.
  const auto view = current_original_view(window);
  const auto hit = simtower::original_elevator_hit_from_client(
      *g_tower_document,
      static_cast<std::int16_t>(LOWORD(position)),
      static_cast<std::int16_t>(HIWORD(position)), view.x, view.y);
  if (!hit.hit) return;
  const auto& elevator = g_tower_document->elevators[hit.elevator_index];
  const auto path = simtower::original_elevator_finger_press_path(
      true, elevator.word_3c != 0U, hit.floor,
      elevator.bottom_floor, elevator.top_floor);
  if (path ==
      simtower::OriginalElevatorFingerPressPath::capture_upper_cap) {
    g_elevator_shaft_drag.direction =
        OriginalElevatorShaftDragDirection::upper;
  } else if (path ==
             simtower::OriginalElevatorFingerPressPath::capture_lower_cap) {
    g_elevator_shaft_drag.direction =
        OriginalElevatorShaftDragDirection::lower;
  } else {
    return;
  }
  g_elevator_shaft_drag.elevator_index = hit.elevator_index;
  SetCursor(resolve_original_cursor(1000U));
  update_original_elevator_finger(window, position);
}

void double_click_original_elevator_finger(HWND window, LPARAM position) {
  // 10a0:05bf-062c opens Elevator Control for any shaft hit. Both branches
  // clear direction, restore the Finger cursor, and release capture, but the
  // miss branch deliberately leaves DS:02a6 armed until the following up.
  std::optional<std::size_t> hit_index{};
  if (g_tower_document) {
    const auto view = current_original_view(window);
    const auto hit = simtower::original_elevator_hit_from_client(
        *g_tower_document,
        static_cast<std::int16_t>(LOWORD(position)),
        static_cast<std::int16_t>(HIWORD(position)), view.x, view.y);
    if (hit.hit) {
      SetCursor(resolve_original_cursor(0U));
      hit_index = hit.elevator_index;
    }
  }
  const auto plan = simtower::original_elevator_finger_double_click_plan(
      hit_index.has_value());
  if (plan.open_control) {
    open_original_elevator_control(
        *hit_index,
        static_cast<std::int16_t>(LOWORD(position)),
        static_cast<std::int16_t>(HIWORD(position)));
  }
  if (plan.clear_capture_latch) {
    g_elevator_shaft_drag = {};
  } else {
    g_elevator_shaft_drag.direction =
        OriginalElevatorShaftDragDirection::none;
  }
  SetCursor(resolve_original_cursor(1004U));
  ReleaseCapture();
}

void update_original_parking_drag(HWND window,
                                  LPARAM position,
                                  bool refresh_horizontal_snap = true);

void begin_original_lobby_drag(HWND window, WPARAM keys, LPARAM position,
                               bool setup_press = true) {
  if (!g_tower_document) return;
  // Exact 1058:0000 edit-mode dispatch. Mode zero is the Bulldozer, one is
  // the elevator Finger tool, and two is the Magnifying Glass. Only modes
  // three and above enter 11f8:07d8 construction.
  if (g_command_mode == 0U) {
    if (g_build_mode_enabled) run_original_facility_bulldozer(window, position);
    return;
  }
  if (g_command_mode == 1U) {
    if (g_build_mode_enabled) begin_original_elevator_finger(window, position);
    return;
  }
  if (g_command_mode == 2U) {
    run_original_magnifying_glass(window, position);
    return;
  }
  if (g_command_mode < 3U || !g_build_mode_enabled) return;
  const auto view = current_original_view(window);
  const int client_x = static_cast<std::int16_t>(LOWORD(position));
  const int client_y = static_cast<std::int16_t>(HIWORD(position));
  std::optional<simtower::OriginalLobbyPlacement> bonus_placement;
  if (g_selected_build_type == 0U) {
    bonus_placement = simtower::original_floor_placement_from_client(
        client_x, client_y, view.x, view.y);
  } else if (g_selected_build_type == 7U) {
    bonus_placement = simtower::original_office_placement_from_client(
        client_x, client_y, view.x, view.y);
  } else if (g_selected_build_type == 24U) {
    bonus_placement = simtower::original_lobby_placement_from_client(
        client_x, client_y, view.x, view.y);
  } else if (simtower::original_facility_width_cells(
                 g_selected_build_type) != 0U) {
    bonus_placement = simtower::original_facility_placement_from_client(
        g_selected_build_type, client_x, client_y, view.x, view.y);
  }

  // 11f8:090e-0950 resets 24cc and arms every continuous tool before the
  // hidden 0955 initial-balance branch and before Shift replacement. Keep the
  // capture alive even when either later prepass returns early.
  if (setup_press) {
    g_retained_construction_press_position = position;
    g_retained_construction_press_position_valid = true;
    g_retained_construction_placement = bonus_placement;
    g_construction_drag_completion = {};
    if (simtower::original_continuous_construction_type(
            g_selected_build_type) && bonus_placement) {
      g_construction_drag_completion = {
          true, g_selected_build_type, 0U,
          g_tower_document->header.balance, true};
      const auto& placement = *bonus_placement;
      if (g_selected_build_type == 0U) {
        g_floor_drag = {
            true, placement.floor, placement.left, placement.right};
      } else if (g_selected_build_type == 24U) {
        g_lobby_drag = {
            true, placement.floor, placement.left, placement.right};
      }
      SetCapture(window);
    }
  }
  if (bonus_placement && simtower::apply_original_initial_balance_bonus(
                             *g_tower_document, bonus_placement->floor,
                             bonus_placement->left)) {
    // 11f8:0980 calls 1178:076f directly. This changes only finance state and
    // immediately paints Info; it does not dirty Main or Map pixels.
    g_tower_dirty = true;
    invalidate_original_info_status();
    return;
  }
  if (g_selected_build_type == 0U) {
    const auto placement = simtower::original_floor_placement_from_client(
        client_x, client_y, view.x, view.y);
    const auto old_balance = g_tower_document->header.balance;
    const auto old_height = g_tower_document->header.lobby_height;
    const auto result = simtower::build_original_floor(
        *g_tower_document, placement.floor,
        static_cast<std::uint16_t>(placement.left),
        static_cast<std::uint16_t>(placement.right), g_construction_costs);
    (void)apply_original_lobby_result(window, result, old_balance, old_height,
                                      true);
    if (result.succeeded()) {
      keep_original_pointer_visible(window, client_x, client_y, true);
    }
    return;
  }
  if (g_selected_build_type == 22U || g_selected_build_type == 27U) {
    const auto type = static_cast<std::uint8_t>(g_selected_build_type);
    const auto placement = simtower::original_facility_placement_from_client(
        type, client_x, client_y, view.x, view.y);
    const auto old_balance = g_tower_document->header.balance;
    const auto old_height = g_tower_document->header.lobby_height;
    simtower::OriginalConstructionResult result{
        simtower::OriginalConstructionStatus::invalid_span, 0, 20U};
    result = simtower::build_original_vertical_transport(
        *g_tower_document, type, placement.floor,
        static_cast<std::uint16_t>(placement.left), g_construction_costs);
    (void)apply_original_lobby_result(window, result, old_balance, old_height);
    return;
  }
  if (g_selected_build_type == 11U || g_selected_build_type == 0x2cU) {
    const auto type = g_selected_build_type;
    const auto placement = simtower::original_facility_placement_from_client(
        type, client_x, client_y, view.x, view.y);
    if (setup_press && !apply_original_shift_replacement(
            window, keys, type, placement)) {
      return;
    }

    // Unlike Floor/Lobby, 0946 captures Parking/Ramp without setting DS:3216.
    // Their 240d/25a2 helpers initialize only when actually reached. The floor
    // remains 08ce's down-time snap, while a later move refreshes c6/c8 first.
    if (g_parking_drag.kind == OriginalParkingDragKind::none) {
      g_parking_drag.kind = type == 11U ? OriginalParkingDragKind::parking
                                        : OriginalParkingDragKind::ramp;
      g_parking_drag.press_floor =
          g_retained_construction_placement
              ? g_retained_construction_placement->floor
              : placement.floor;
      g_parking_drag.snapped_left = placement.left;
      g_parking_drag.snapped_right = placement.right;
    }
    update_original_parking_drag(window, position, true);
    return;
  }
  if ((g_selected_build_type >= 3U && g_selected_build_type <= 5U) ||
      g_selected_build_type == 6U || g_selected_build_type == 9U ||
      g_selected_build_type == 10U || g_selected_build_type == 12U ||
      g_selected_build_type == 13U || g_selected_build_type == 14U ||
      g_selected_build_type == 15U || g_selected_build_type == 17U ||
      g_selected_build_type == 18U || g_selected_build_type == 20U ||
      g_selected_build_type == 29U || g_selected_build_type == 31U ||
      g_selected_build_type == 36U) {
    const auto type = static_cast<std::uint8_t>(g_selected_build_type);
    const auto placement = simtower::original_facility_placement_from_client(
        type, client_x, client_y, view.x, view.y);
    const auto old_balance = g_tower_document->header.balance;
    const auto old_height = g_tower_document->header.lobby_height;
    simtower::OriginalConstructionResult result{
        simtower::OriginalConstructionStatus::invalid_span, 0, 20U};
    // 11f8:09c1-09e3 runs Shift replacement before the type jump table. The
    // constructor itself owns span validation and any preceding type limit.
    if (setup_press &&
        !apply_original_shift_replacement(window, keys, type, placement)) {
      return;
    }
    if (type == 9U) {
      result = simtower::build_original_condo(
          *g_tower_document, placement.floor,
          static_cast<std::uint16_t>(placement.left),
          static_cast<std::uint8_t>(g_facility_variants[type]),
          g_construction_costs);
    } else if (type == 10U) {
      result = simtower::build_original_retail_shop(
          *g_tower_document, placement.floor,
          static_cast<std::uint16_t>(placement.left), g_construction_costs);
    } else if (type == 6U) {
      result = simtower::build_original_restaurant(
          *g_tower_document, placement.floor,
          static_cast<std::uint16_t>(placement.left), g_construction_costs);
    } else if (type == 12U) {
      result = simtower::build_original_fast_food(
          *g_tower_document, placement.floor,
          static_cast<std::uint16_t>(placement.left), g_construction_costs);
    } else if (type == 14U) {
      result = simtower::build_original_security(
          *g_tower_document, placement.floor,
          static_cast<std::uint16_t>(placement.left), g_construction_costs);
    } else if (type == 15U) {
      result = simtower::build_original_housekeeping(
          *g_tower_document, placement.floor,
          static_cast<std::uint16_t>(placement.left), g_construction_costs);
    } else if (type == 17U) {
      result = simtower::build_original_secom_center(
          *g_tower_document, placement.floor,
          static_cast<std::uint16_t>(placement.left), g_construction_costs);
    } else if (type == 18U) {
      result = simtower::build_original_movie_theater(
          *g_tower_document, placement.floor,
          static_cast<std::uint16_t>(placement.left), g_construction_costs);
    } else if (type == 20U) {
      result = simtower::build_original_recycling_center(
          *g_tower_document, placement.floor,
          static_cast<std::uint16_t>(placement.left), g_construction_costs);
    } else if (type == 29U) {
      result = simtower::build_original_party_hall(
          *g_tower_document, placement.floor,
          static_cast<std::uint16_t>(placement.left), g_construction_costs);
    } else if (type == 31U) {
      result = simtower::build_original_metro_station(
          *g_tower_document, placement.floor,
          static_cast<std::uint16_t>(placement.left), g_construction_costs);
    } else if (type == 36U) {
      result = simtower::build_original_cathedral(
          *g_tower_document, placement.floor,
          static_cast<std::uint16_t>(placement.left), g_construction_costs);
    } else if (type == 13U) {
      result = simtower::build_original_medical_center(
          *g_tower_document, placement.floor,
          static_cast<std::uint16_t>(placement.left),
          static_cast<std::uint8_t>(g_facility_variants[type]),
          g_construction_costs);
    } else {
      result = simtower::build_original_hotel_room(
          *g_tower_document, type, placement.floor,
          static_cast<std::uint16_t>(placement.left),
          static_cast<std::uint8_t>(g_facility_variants[type]),
          g_construction_costs);
    }
    if (type == 13U) {
      // 11f8:0c46-0c7f checks the ten-center sentinel before entering the
      // constructor/variant path; all attempted constructors advance 795c.
      if (result.construction_status_code != 30U) {
        g_facility_variants[type] = static_cast<std::uint16_t>(
            (g_facility_variants[type] + 1U) % 3U);
      }
    } else if (type != 6U && type != 10U && type != 12U && type != 14U &&
               type != 15U && type != 17U && type != 18U && type != 20U &&
               type != 29U && type != 31U && type != 36U) {
      // 11f8:0b21/0b57/0b8d/0bf9 advances these selectors even after a
      // rejected attempt. Retail has no appearance selector to advance.
      const std::uint16_t cycle = type == 4U ? 4U : (type == 9U ? 3U : 2U);
      g_facility_variants[type] = static_cast<std::uint16_t>(
          (g_facility_variants[type] + 1U) % cycle);
    }
    (void)apply_original_lobby_result(window, result, old_balance, old_height);
    return;
  }
  if (g_selected_build_type == 7U) {
    const auto placement = simtower::original_office_placement_from_client(
        client_x, client_y, view.x, view.y);
    const auto old_balance = g_tower_document->header.balance;
    const auto old_height = g_tower_document->header.lobby_height;
    simtower::OriginalConstructionResult result{
        simtower::OriginalConstructionStatus::invalid_span, 0, 20U};
    if (setup_press &&
        !apply_original_shift_replacement(window, keys, 7U, placement)) {
      return;
    }
    result = simtower::build_original_office(
        *g_tower_document, placement.floor,
        static_cast<std::uint16_t>(placement.left),
        static_cast<std::uint8_t>(g_office_variant),
        g_construction_costs);
    // 11f8:0bc3-0bd0 advances DS:7954 after every type-7 attempt, including
    // rejected placement, and keeps the command palette on Office.
    g_office_variant = static_cast<std::uint16_t>((g_office_variant + 1U) % 6U);
    (void)apply_original_lobby_result(window, result, old_balance, old_height);
    return;
  }
  if (g_selected_build_type == 1U || g_selected_build_type == 42U ||
      g_selected_build_type == 43U) {
    const auto placement = simtower::original_facility_placement_from_client(
        g_selected_build_type, client_x, client_y, view.x, view.y);
    const auto old_balance = g_tower_document->header.balance;
    const auto old_height = g_tower_document->header.lobby_height;
    const auto result = simtower::build_original_elevator(
        *g_tower_document, g_selected_build_type, placement.floor,
        static_cast<std::uint16_t>(placement.left),
        g_construction_costs, g_part);
    if (apply_original_lobby_result(window, result, old_balance, old_height) &&
        result.new_elevator_shaft_created) {
      // 11f8:140d is reached only after allocating a new shaft. The existing
      // add-car branch jumps from 122f straight to 1446 and leaves the active
      // construction command untouched.
      g_command_mode = 1U;
      InvalidateRect(g_command_window, nullptr, FALSE);
      // 1080:05a1 composes and presents Command before 0fea returns.
      UpdateWindow(g_command_window);
    }
    return;
  }
  const auto placement = simtower::original_lobby_placement_from_client(
      client_x, client_y, view.x, view.y);
  if (g_selected_build_type != 24U) {
    return;
  }
  const auto old_balance = g_tower_document->header.balance;
  const auto old_height = g_tower_document->header.lobby_height;
  simtower::OriginalConstructionResult result{};
  if (placement.floor == 10 && old_height == 0U) {
    // 11f8:098f-09b3: Control selects a two-story lobby; Control+Shift
    // selects three. Shift alone leaves the default one-story selection.
    std::uint16_t height = 1U;
    if ((keys & MK_CONTROL) != 0U) {
      height = (keys & MK_SHIFT) != 0U ? 3U : 2U;
    }
    result = simtower::build_original_initial_lobby(
        *g_tower_document, static_cast<std::uint16_t>(placement.left),
        static_cast<std::uint16_t>(placement.right), height,
        g_construction_costs);
  } else if (placement.floor == 10) {
    result = simtower::extend_original_lobby(
        *g_tower_document, static_cast<std::uint16_t>(placement.left),
        static_cast<std::uint16_t>(placement.right), g_construction_costs);
  } else {
    result = simtower::build_original_sky_lobby(
        *g_tower_document, placement.floor,
        static_cast<std::uint16_t>(placement.left),
        static_cast<std::uint16_t>(placement.right), g_construction_costs);
  }
  (void)apply_original_lobby_result(window, result, old_balance, old_height,
                                    true);
  if (result.succeeded()) {
    keep_original_pointer_visible(window, client_x, client_y, true);
  }
}

void update_original_lobby_drag(HWND window, LPARAM position) {
  if (!g_lobby_drag.active || !g_tower_document) {
    return;
  }
  const auto view = current_original_view(window);
  const int client_x = static_cast<std::int16_t>(LOWORD(position));
  const int client_y = static_cast<std::int16_t>(HIWORD(position));
  const auto placement = simtower::original_lobby_placement_from_client(
      client_x, client_y, view.x, view.y);
  const std::int32_t left = std::min(g_lobby_drag.anchor_left, placement.left);
  const std::int32_t right =
      std::max(g_lobby_drag.anchor_right, placement.right);
  const auto old_balance = g_tower_document->header.balance;
  const auto old_height = g_tower_document->header.lobby_height;
  const auto result = g_lobby_drag.floor == 10
      ? simtower::extend_original_lobby(
            *g_tower_document, static_cast<std::uint16_t>(left),
            static_cast<std::uint16_t>(right), g_construction_costs)
      : simtower::build_original_sky_lobby(
            *g_tower_document, g_lobby_drag.floor,
            static_cast<std::uint16_t>(left),
            static_cast<std::uint16_t>(right), g_construction_costs);
  (void)apply_original_lobby_result(window, result, old_balance, old_height,
                                    true);
  if (result.succeeded()) {
    keep_original_pointer_visible(window, client_x, client_y, true);
  }
}

void update_original_floor_drag(HWND window, LPARAM position) {
  if (!g_floor_drag.active || !g_tower_document) {
    return;
  }
  const auto view = current_original_view(window);
  const int client_x = static_cast<std::int16_t>(LOWORD(position));
  const int client_y = static_cast<std::int16_t>(HIWORD(position));
  const auto placement = simtower::original_floor_placement_from_client(
      client_x, client_y, view.x, view.y);
  const std::int32_t left =
      std::min(g_floor_drag.anchor_left, placement.left);
  const std::int32_t right =
      std::max(g_floor_drag.anchor_right, placement.right);
  const auto old_balance = g_tower_document->header.balance;
  const auto old_height = g_tower_document->header.lobby_height;
  const auto result = simtower::build_original_floor(
      *g_tower_document, g_floor_drag.floor,
      static_cast<std::uint16_t>(left), static_cast<std::uint16_t>(right),
      g_construction_costs);
  (void)apply_original_lobby_result(window, result, old_balance, old_height,
                                    true);
  if (result.succeeded()) {
    keep_original_pointer_visible(window, client_x, client_y, true);
  }
}

void update_original_parking_drag(HWND window,
                                  LPARAM position,
                                  bool refresh_horizontal_snap) {
  if (g_parking_drag.kind == OriginalParkingDragKind::none ||
      !g_tower_document) {
    return;
  }
  const auto view = current_original_view(window);
  const int client_x = static_cast<std::int16_t>(LOWORD(position));
  const int client_y = static_cast<std::int16_t>(HIWORD(position));
  const auto type = g_parking_drag.kind == OriginalParkingDragKind::parking
      ? 11U
      : 44U;
  const auto placement = simtower::original_facility_placement_from_client(
      type, client_x, client_y, view.x, view.y);
  if (refresh_horizontal_snap) {
    // 07d8 refreshes c6/c8 for move but not for double-click. Ramp passes
    // these live horizontal words to every 17fd attempt in the helper call.
    g_parking_drag.snapped_left = placement.left;
    g_parking_drag.snapped_right = placement.right;
  }

  std::vector<std::int32_t> parking_units;
  std::vector<simtower::OriginalParkingRampDragAttempt> ramp_attempts;
  simtower::OriginalParkingDragRunState parking_next{};
  simtower::OriginalParkingRampDragRunState ramp_next{};
  if (g_parking_drag.kind == OriginalParkingDragKind::parking) {
    auto plan = simtower::original_parking_drag_run_plan(
        g_parking_drag.parking,
        g_parking_drag.snapped_left,
        g_parking_drag.snapped_right,
        placement.left,
        placement.right);
    parking_units = std::move(plan.unit_lefts);
    parking_next = plan.next_state;
  } else {
    auto plan = simtower::original_parking_ramp_drag_run_plan(
        g_parking_drag.ramp,
        g_parking_drag.press_floor,
        placement.floor,
        g_parking_drag.snapped_left);
    ramp_attempts = std::move(plan.attempts);
    ramp_next = plan.next_state;
  }

  const auto old_balance = g_tower_document->header.balance;
  bool attempted = false;
  bool final_attempt_succeeded = false;
  bool any_succeeded = false;
  const auto consume_result = [&](
      const simtower::OriginalConstructionResult& result) {
    attempted = true;
    final_attempt_succeeded = result.succeeded();
    any_succeeded = any_succeeded || final_attempt_succeeded;
    if (!result.succeeded() && result.construction_status_code != 0U) {
      show_original_construction_status(result.construction_status_code);
    }
  };
  for (const auto left : parking_units) {
    consume_result(simtower::build_original_parking(
        *g_tower_document, g_parking_drag.press_floor,
        static_cast<std::uint16_t>(left), g_construction_costs));
  }
  for (const auto& attempt : ramp_attempts) {
    consume_result(simtower::build_original_parking_ramp(
        *g_tower_document, attempt.floor,
        static_cast<std::uint16_t>(attempt.left),
        g_construction_costs));
  }

  if (any_succeeded) {
    apply_original_construction_mutation_presentation(
        window, true,
        g_tower_document->header.balance != old_balance);
  }

  const auto completion = simtower::original_captured_helper_completion_plan(
      attempted, final_attempt_succeeded,
      g_construction_drag_completion.priority_sound_latch_armed);
  if (completion.increment_successful_step &&
      g_construction_drag_completion.active) {
    ++g_construction_drag_completion.successful_steps;
  }
  if (completion.sound ==
          simtower::OriginalCapturedHelperSound::priority_five &&
      g_audio) {
    (void)g_audio->play_resource(7001, 0, 5);
  } else if (completion.sound ==
                 simtower::OriginalCapturedHelperSound::reserved_if_idle &&
             g_audio) {
    (void)g_audio->play_reserved_if_idle(7001, 0, GetTickCount());
  }
  if (completion.clear_priority_sound_latch) {
    g_construction_drag_completion.priority_sound_latch_armed = false;
  }
  if (completion.returned_success) {
    // 240d passes one for horizontal auto-scroll; 25a2 passes zero for the
    // vertical axis. Both calls precede the helper's retained-pointer update.
    keep_original_pointer_visible(
        window, client_x, client_y,
        g_parking_drag.kind == OriginalParkingDragKind::parking);
  }

  // 2570/26a6 re-snap LPARAM after possible auto-scroll, so the next helper
  // call consumes the post-scroll world coordinate rather than the one above.
  const auto post_view = current_original_view(window);
  const auto post_placement =
      simtower::original_facility_placement_from_client(
          type, client_x, client_y, post_view.x, post_view.y);
  if (g_parking_drag.kind == OriginalParkingDragKind::parking) {
    parking_next.retained_left = post_placement.left;
    parking_next.retained_right = post_placement.right;
    g_parking_drag.parking = parking_next;
  } else {
    ramp_next.retained_floor = post_placement.floor;
    ramp_next.retained_upper_exclusive = static_cast<std::int16_t>(
        post_placement.floor + 1);
    g_parking_drag.ramp = ramp_next;
  }
}

void finish_original_construction_drag(HWND window) {
  if (!g_tower_document) return;
  const auto type = g_selected_build_type;
  const auto plan = simtower::original_construction_release_plan(
      type, g_construction_drag_completion.successful_steps,
      g_construction_drag_completion.balance_at_press,
      g_tower_document->header.balance);
  if (!plan.handled) return;

  // Floor/Lobby's balance-change WAVE/7000 precedes RELEASECAPTURE at 0839;
  // Parking/Ramp deliberately never play this or the general success wave.
  if (plan.play_drag_success_wave && g_audio) {
    (void)g_audio->play_resource(7000, 0, 4);
  }

  g_lobby_drag = {};
  g_floor_drag = {};
  g_parking_drag = {};
  g_construction_drag_completion = {};
  ReleaseCapture();

  if (!plan.complete_success_tail) {
    if (plan.play_failure_wave && g_audio) {
      (void)g_audio->play_resource(7002, 0, 4);
    }
    return;
  }

  // 0e21's common success tail is reached once per captured command, never
  // once per accepted drag step. It clears the status, runs 1148:0163, then
  // executes the literal route-rebuild table before repaint/dirty handling.
  show_original_construction_status(0U);
  play_original_construction_success_audio(type);
  const auto old_balance = g_tower_document->header.balance;
  const auto rating_completion =
      simtower::complete_original_rating_construction(
          *g_tower_document, g_part, type);
  if (rating_completion.dialog.valid()) {
    (void)run_original_event_dialog(rating_completion.dialog);
  }
  if (simtower::original_construction_rebuilds_routes(type)) {
    simtower::rebuild_original_transport_route_graphs(*g_tower_document);
  }
  apply_original_construction_mutation_presentation(
      window, true, g_tower_document->header.balance != old_balance);
}

void end_original_lobby_drag() {
  const bool elevator_finger_capture_active =
      g_elevator_shaft_drag.capture_active;
  if (g_lobby_drag.active || g_floor_drag.active ||
      g_parking_drag.kind != OriginalParkingDragKind::none ||
      g_construction_drag_completion.active ||
      elevator_finger_capture_active) {
    g_lobby_drag = {};
    g_floor_drag = {};
    g_parking_drag = {};
    g_elevator_shaft_drag = {};
    g_construction_drag_completion = {};
    ReleaseCapture();
    // 10a0:05a2 restores the Finger cursor immediately after releasing its
    // capture, independently of whether a cap drag became active.
    if (elevator_finger_capture_active) {
      SetCursor(resolve_original_cursor(1004U));
    }
  }
}

void forward_original_armed_double_click(HWND window,
                                         WPARAM keys,
                                         LPARAM position) {
  // MAINWNDPROC forwards WM_LBUTTONDBLCLK without arming DS:0242; 1058:003a
  // then makes it effective only if a prior down is still armed. The ordinary
  // Windows sequence clears that latch on the preceding button-up, but retain
  // the exact behavior for reentrant/synthetic sequences.
  if (!g_main_pointer_interaction_armed || !g_tower_document) return;
  if (g_command_mode == 0U || g_command_mode == 2U) return;
  if (g_command_mode == 1U) {
    const auto view = current_original_view(window);
    const auto hit = simtower::original_elevator_hit_from_client(
        *g_tower_document,
        static_cast<std::int16_t>(LOWORD(position)),
        static_cast<std::int16_t>(HIWORD(position)), view.x, view.y);
    if (hit.hit) {
      open_original_elevator_control(
          hit.elevator_index,
          static_cast<std::int16_t>(LOWORD(position)),
          static_cast<std::int16_t>(HIWORD(position)));
    }
    return;
  }
  if (!g_build_mode_enabled) return;
  if (g_parking_drag.kind != OriginalParkingDragKind::none) {
    update_original_parking_drag(window, position, false);
  } else if (g_floor_drag.active) {
    update_original_floor_drag(window, position);
  } else if (g_lobby_drag.active) {
    update_original_lobby_drag(window, position);
  } else if (g_construction_drag_completion.active &&
             (g_selected_build_type == 11U ||
              g_selected_build_type == 44U) &&
             g_retained_construction_placement) {
    // 0x0203 does not refresh c4/c6/c8. A helper skipped by the down-time
    // bonus/Shift path therefore initializes from those retained snaps, while
    // its tail still publishes the double-click LPARAM for the following call.
    g_parking_drag.kind = g_selected_build_type == 11U
        ? OriginalParkingDragKind::parking
        : OriginalParkingDragKind::ramp;
    g_parking_drag.press_floor = g_retained_construction_placement->floor;
    g_parking_drag.snapped_left = g_retained_construction_placement->left;
    g_parking_drag.snapped_right = g_retained_construction_placement->right;
    update_original_parking_drag(window, position, false);
  } else if (g_retained_construction_press_position_valid) {
    // 0x0203 bypasses 08ce's placement recomputation, 090e's counter reset,
    // 0946's capture, and 09c1's Shift replacement. Re-run the type attempt
    // at the coordinates retained by its already-armed down only.
    begin_original_lobby_drag(
        window, keys, g_retained_construction_press_position, false);
  }
}

void publish_original_world_input_modifiers(WPARAM keys) {
  const auto modifiers = simtower::original_world_input_modifiers(
      static_cast<std::uint16_t>(keys));
  g_world_control_modifier = modifiers.control;
  g_world_shift_modifier = modifiers.shift;
}

void dispatch_original_world_input(
    HWND window,
    simtower::OriginalWorldInputMessage message,
    WPARAM keys,
    LPARAM position) {
  // 1058 publishes modifiers even when one of the following gates suppresses
  // every tool action.
  publish_original_world_input_modifiers(keys);
  const bool isolation_active =
      g_elevator_control_context &&
      g_elevator_control_context->state.isolation_active;
  const bool emergency_active =
      g_tower_document &&
      simtower::original_emergency_people_pass_active(*g_tower_document);
  const auto plan = simtower::original_world_input_plan(
      message, g_main_pointer_interaction_armed, isolation_active,
      emergency_active, g_command_mode, g_build_mode_enabled,
      (keys & MK_LBUTTON) != 0U);

  switch (plan.action) {
    case simtower::OriginalWorldInputAction::none:
      break;
    case simtower::OriginalWorldInputAction::emergency_feedback:
      if (g_audio) (void)g_audio->play_resource(7002, 0, 4);
      break;
    case simtower::OriginalWorldInputAction::bulldozer:
      run_original_facility_bulldozer(window, position);
      break;
    case simtower::OriginalWorldInputAction::elevator_finger:
      switch (message) {
        case simtower::OriginalWorldInputMessage::button_down:
          begin_original_elevator_finger(window, position);
          break;
        case simtower::OriginalWorldInputMessage::mouse_move:
          continue_original_elevator_finger(window, position);
          break;
        case simtower::OriginalWorldInputMessage::button_up:
          end_original_lobby_drag();
          break;
        case simtower::OriginalWorldInputMessage::double_click:
          double_click_original_elevator_finger(window, position);
          break;
      }
      break;
    case simtower::OriginalWorldInputAction::magnifier:
      run_original_magnifying_glass(window, position);
      break;
    case simtower::OriginalWorldInputAction::construction:
      switch (message) {
        case simtower::OriginalWorldInputMessage::button_down:
          begin_original_lobby_drag(window, keys, position);
          break;
        case simtower::OriginalWorldInputMessage::mouse_move:
          if ((keys & MK_LBUTTON) != 0U) {
            if (g_parking_drag.kind != OriginalParkingDragKind::none) {
              update_original_parking_drag(window, position);
            } else if (g_floor_drag.active) {
              update_original_floor_drag(window, position);
            } else if (g_lobby_drag.active) {
              update_original_lobby_drag(window, position);
            } else if (g_construction_drag_completion.active &&
                       (g_selected_build_type == 11U ||
                        g_selected_build_type == 44U)) {
              begin_original_lobby_drag(window, keys, position, false);
            }
          }
          break;
        case simtower::OriginalWorldInputMessage::button_up:
          finish_original_construction_drag(window);
          break;
        case simtower::OriginalWorldInputMessage::double_click:
          forward_original_armed_double_click(window, keys, position);
          break;
      }
      break;
  }

  // Mode two performs this tail after its down-only hit chain and after every
  // other routed mouse phase. 1100:0000 may set DS:77c0 while the Person
  // Information modal runs, so inspect the live post-action latch here.
  // 1058:033c owns the complete Find/build toggle.
  if (plan.check_find_exit_latch && g_find_marker.phase != 0U) {
    apply_original_construction_toggle();
  }
}

void remove_original_recent_file_items(HWND window) {
  // 10d0:0046-0075 and 10d0:0240-026f remove positions 6 and 5 from the
  // File submenu when the optional recent-file pair is present.
  HMENU file = GetSubMenu(GetMenu(window), 1);
  if (file && GetMenuItemCount(file) >= 6) {
    DeleteMenu(file, 6, MF_BYPOSITION);
    DeleteMenu(file, 5, MF_BYPOSITION);
  }
}

void update_original_tower_title(HWND window) {
  const auto title =
      simtower::original_tower_window_title(g_tower_file_title);
  SetWindowTextW(window, title.c_str());
}

void show_original_file_error(std::uint16_t string_index) {
  const std::string message = simtower::original_strl_entry(
      g_resources.find("STRL", 1002), string_index);
  const std::array<std::string_view, 4> substitutions = {
      message, std::string_view{}, std::string_view{}, std::string_view{}};
  (void)simtower::show_original_alert(g_main_window, g_resources, 1000,
                                      substitutions);
}

std::uint16_t original_error_string_for_status(
    simtower::OriginalTdtStatus status) {
  // 10d0:06b3-06c9 selects only the explicit version alerts; every other
  // parser failure uses STRL/1002 entry 2.
  if (status == simtower::OriginalTdtStatus::version_too_new) {
    return 4;
  }
  if (status == simtower::OriginalTdtStatus::version_too_old) {
    return 5;
  }
  return 2;
}

void begin_original_new_tower(HWND window) {
  remove_original_recent_file_items(window);
  // 10d0:001d calls the exact constructor chain at 10d0:086c, followed by
  // the derived rebuild at 10d0:0ac2, before resetting the file identity.
  const auto preserved_view = current_original_view(window);
  auto document = simtower::make_original_new_tdt();
  // 10d0:086c deliberately leaves DS:b3f0/b3f2 untouched, so New retains
  // the prior scroll position instead of using the zero words in a freshly
  // serialized header.
  document.header.view_x = static_cast<std::uint16_t>(preserved_view.x);
  document.header.view_y = static_cast<std::uint16_t>(preserved_view.y);
  simtower::carry_original_process_random_state(
      g_tower_document ? &*g_tower_document : nullptr, document);
  g_tower_document = std::move(document);
  reset_original_simulation_state();
  reset_original_command_state();
  refresh_original_rating_command(g_tower_document->header.rating, 0U);
  restore_original_derived_view_position(
      window, g_tower_document->header.view_x,
      g_tower_document->header.view_y);
  present_original_document_transition_synchronously();
  g_lobby_drag = {};
  const auto view = current_original_view(window);
  g_tower_document->header.view_x = static_cast<std::uint16_t>(view.x);
  g_tower_document->header.view_y = static_cast<std::uint16_t>(view.y);
  g_tower_path.clear();
  g_tower_file_title = L"untitled";
  g_tower_dirty = false;
  update_original_tower_title(window);
}

// Exact native disk-transaction boundary for 10d0:0777: optional replacement
// confirmation, create/write/close through 10d0:0b3a's translated TDT codec,
// failure cleanup, and document-identity/dirty-state commit after success.
bool save_original_tower_to(HWND window, const std::filesystem::path& path,
                            bool ask_before_replace) {
  if (!g_tower_document) {
    return false;
  }
  if (ask_before_replace && std::filesystem::exists(path)) {
    MessageBeep(static_cast<UINT>(-1));
    if (MessageBoxA(window, kReplaceExisting, "Save SimTower data file",
                    MB_YESNO | MB_ICONEXCLAMATION) == IDNO) {
      return false;
    }
  }
  try {
    const auto view = current_original_view(window);
    g_tower_document->header.view_x = static_cast<std::uint16_t>(view.x);
    g_tower_document->header.view_y = static_cast<std::uint16_t>(view.y);
    simtower::save_original_tdt_file(path, *g_tower_document);
  } catch (const simtower::OriginalTdtFileError& error) {
    // 10d0:07e0/080e maps create and transfer failures to STRL/1002 entry 1.
    // Only a post-create transfer failure enters 1000:2140 and deletes the
    // partial target; an initial LCREAT failure has no owned file to remove.
    if (simtower::original_failed_save_deletes_target(error.operation())) {
      std::error_code ignored{};
      (void)std::filesystem::remove(path, ignored);
    }
    show_original_file_error(1);
    return false;
  } catch (const simtower::OriginalTdtError&) {
    // Native serialization precedes ofstream creation, while original 0777
    // creates first and then lets 0b3a report the transfer error. Preserve the
    // externally visible deletion performed by its 080e failure branch.
    if (simtower::original_failed_save_deletes_target(
            simtower::OriginalTdtFileOperation::write)) {
      std::error_code ignored{};
      (void)std::filesystem::remove(path, ignored);
    }
    show_original_file_error(1);
    return false;
  }
  g_tower_path = path;
  g_tower_file_title = path.filename().wstring();
  g_tower_dirty = false;
  update_original_tower_title(window);
  return true;
}

constexpr wchar_t kOriginalCommonDialogProcProperty[] =
    L"SimTowerOriginalCommonDialogProc";
constexpr wchar_t kOriginalCommonDialogActiveProperty[] =
    L"SimTowerOriginalCommonDialogActive";

void paint_original_common_dialog_caption(HWND window, bool active) {
  HDC dc = GetWindowDC(window);
  if (!dc) return;
  RECT bounds{};
  GetWindowRect(window, &bounds);
  const int width =
      std::max(1, static_cast<int>(bounds.right - bounds.left));
  const int frame_x = std::max(1, GetSystemMetrics(SM_CXDLGFRAME));
  const int frame_y = std::max(1, GetSystemMetrics(SM_CYDLGFRAME));
  const int caption_height = std::max(1, GetSystemMetrics(SM_CYCAPTION));
  RECT caption{frame_x, frame_y, width - frame_x,
               frame_y + caption_height};
  const HBRUSH background =
      CreateSolidBrush(active ? RGB(0, 0, 128) : RGB(192, 192, 192));
  FillRect(dc, &caption, background);
  DeleteObject(background);

  RECT system_button{caption.left, caption.top,
                     caption.left + caption_height, caption.bottom};
  draw_original_main_caption_button(dc, system_button, true, false);

  wchar_t title[256]{};
  GetWindowTextW(window, title, static_cast<int>(std::size(title)));
  RECT title_bounds{system_button.right, caption.top, caption.right,
                    caption.bottom};
  const int previous_background = SetBkMode(dc, TRANSPARENT);
  const COLORREF previous_color =
      SetTextColor(dc, active ? RGB(255, 255, 255) : RGB(0, 0, 0));
  HGDIOBJ previous_font = nullptr;
  if (const HFONT font = simtower::original_cached_font(13)) {
    previous_font = SelectObject(dc, font);
  }
  DrawTextW(dc, title, -1, &title_bounds,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
                DT_NOPREFIX);
  if (previous_font) SelectObject(dc, previous_font);
  SetTextColor(dc, previous_color);
  SetBkMode(dc, previous_background);
  ReleaseDC(window, dc);
}

LRESULT CALLBACK original_common_dialog_subclass(HWND window, UINT message,
                                                 WPARAM wparam,
                                                 LPARAM lparam) {
  const auto previous = reinterpret_cast<WNDPROC>(
      GetPropW(window, kOriginalCommonDialogProcProperty));
  if (!previous) return DefWindowProcW(window, message, wparam, lparam);
  if (message == WM_NCHITTEST) {
    RECT bounds{};
    GetWindowRect(window, &bounds);
    const int x = static_cast<std::int16_t>(LOWORD(lparam)) - bounds.left;
    const int y = static_cast<std::int16_t>(HIWORD(lparam)) - bounds.top;
    const int frame_x = std::max(1, GetSystemMetrics(SM_CXDLGFRAME));
    const int frame_y = std::max(1, GetSystemMetrics(SM_CYDLGFRAME));
    const int caption_height = std::max(1, GetSystemMetrics(SM_CYCAPTION));
    if (y >= frame_y && y < frame_y + caption_height) {
      return x >= frame_x && x < frame_x + caption_height
          ? HTSYSMENU
          : HTCAPTION;
    }
  } else if (message == WM_NCPAINT) {
    const LRESULT result =
        CallWindowProcW(previous, window, message, wparam, lparam);
    paint_original_common_dialog_caption(
        window,
        GetPropW(window, kOriginalCommonDialogActiveProperty) != nullptr);
    return result;
  } else if (message == WM_NCACTIVATE) {
    const LRESULT result =
        CallWindowProcW(previous, window, message, wparam, lparam);
    if (wparam != FALSE) {
      SetPropW(window, kOriginalCommonDialogActiveProperty,
               reinterpret_cast<HANDLE>(1));
    } else {
      RemovePropW(window, kOriginalCommonDialogActiveProperty);
    }
    paint_original_common_dialog_caption(window, wparam != FALSE);
    return result;
  } else if (message == WM_SETTEXT) {
    const LRESULT result =
        CallWindowProcW(previous, window, message, wparam, lparam);
    paint_original_common_dialog_caption(
        window,
        GetPropW(window, kOriginalCommonDialogActiveProperty) != nullptr);
    return result;
  } else if (message == WM_NCDESTROY) {
    const LRESULT result =
        CallWindowProcW(previous, window, message, wparam, lparam);
    RemovePropW(window, kOriginalCommonDialogActiveProperty);
    RemovePropW(window, kOriginalCommonDialogProcProperty);
    return result;
  }
  return CallWindowProcW(previous, window, message, wparam, lparam);
}

void subclass_original_common_dialog(HWND dialog) {
  if (GetPropW(dialog, kOriginalCommonDialogProcProperty)) return;
  const auto previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
      dialog, GWLP_WNDPROC,
      reinterpret_cast<LONG_PTR>(original_common_dialog_subclass)));
  if (!previous) return;
  SetPropW(dialog, kOriginalCommonDialogProcProperty,
           reinterpret_cast<HANDLE>(previous));
  SetPropW(dialog, kOriginalCommonDialogActiveProperty,
           reinterpret_cast<HANDLE>(1));
}

struct OriginalCommonDialogChildLayout {
  HWND window{};
  RECT rectangle{};
};

BOOL CALLBACK collect_original_common_dialog_child(HWND child,
                                                   LPARAM parameter) {
  auto* children = reinterpret_cast<
      std::vector<OriginalCommonDialogChildLayout>*>(parameter);
  RECT rectangle{};
  GetWindowRect(child, &rectangle);
  children->push_back({child, rectangle});
  return TRUE;
}

void adapt_original_common_dialog(HWND dialog) {
  RECT outer{};
  GetWindowRect(dialog, &outer);
  RECT old_client{};
  GetClientRect(dialog, &old_client);
  const int old_client_width =
      std::max(1, static_cast<int>(old_client.right - old_client.left));

  std::vector<OriginalCommonDialogChildLayout> children{};
  EnumChildWindows(dialog, collect_original_common_dialog_child,
                   reinterpret_cast<LPARAM>(&children));
  for (auto& child : children) {
    MapWindowPoints(HWND_DESKTOP, dialog,
                    reinterpret_cast<POINT*>(&child.rectangle), 2);
  }

  // The supplied Win16 common dialog is 470 pixels wide. COMDLG32's legacy
  // host on current Windows retains the same vertical layout but contracts it
  // to 408 logical pixels, so restore the original outer width and scale only
  // the horizontal child geometry.
  constexpr int kOriginalCommonDialogOuterWidth = 470;
  const int old_outer_width = outer.right - outer.left;
  const int outer_height = outer.bottom - outer.top;
  const int target_width =
      std::max(old_outer_width, kOriginalCommonDialogOuterWidth);
  const int target_left =
      outer.left - (target_width - old_outer_width) / 2;
  MoveWindow(dialog, target_left, outer.top, target_width, outer_height, FALSE);

  RECT new_client{};
  GetClientRect(dialog, &new_client);
  const int new_client_width =
      std::max(1, static_cast<int>(new_client.right - new_client.left));
  for (const auto& child : children) {
    const int left = MulDiv(child.rectangle.left, new_client_width,
                            old_client_width);
    const int right = MulDiv(child.rectangle.right, new_client_width,
                             old_client_width);
    MoveWindow(child.window, left, child.rectangle.top,
               std::max(1, right - left),
               child.rectangle.bottom - child.rectangle.top, FALSE);

    wchar_t text[128]{};
    GetWindowTextW(child.window, text, static_cast<int>(std::size(text)));
    if (lstrcmpiW(text, L"File &name:") == 0) {
      SetWindowTextW(child.window, L"File &Name:");
    } else if (lstrcmpiW(text, L"&Folders:") == 0) {
      SetWindowTextW(child.window, L"&Directories:");
    } else if (lstrcmpiW(text, L"List files of &type:") == 0) {
      SetWindowTextW(child.window, L"List Files of &Type:");
    } else if (lstrcmpiW(text, L"&Read only") == 0) {
      SetWindowTextW(child.window, L"&Read Only");
    } else if (lstrcmpiW(text, L"Net&work...") == 0) {
      ShowWindow(child.window, SW_HIDE);
    }
    if (const HFONT font = simtower::original_cached_font(13)) {
      SendMessageW(child.window, WM_SETFONT,
                   reinterpret_cast<WPARAM>(font), FALSE);
    }
  }
  InvalidateRect(dialog, nullptr, TRUE);
}

UINT_PTR CALLBACK original_common_dialog_host_hook(HWND dialog, UINT message,
                                                   WPARAM, LPARAM) {
  // A hook without OFN_EXPLORER selects COMDLG32's legacy dialog host.  The
  // original recovered flags remain zero; this native-only adapter prevents
  // current Windows from substituting the unrelated Explorer file picker for
  // the compact Win16 common dialog.  Returning zero leaves every message and
  // all file-selection behavior to COMDLG32.
  if (message == WM_INITDIALOG) {
    configure_original_main_host_chrome(dialog);
    subclass_original_common_dialog(dialog);
    adapt_original_common_dialog(dialog);
    // COMDLG32 assigns several localized labels after it has called the hook's
    // WM_INITDIALOG branch. Reapply the appearance adapter through the same
    // dialog queue once that initialization has completed.
    PostMessageW(dialog, WM_APP + 0x53U, 0U, 0);
  } else if (message == WM_APP + 0x53U) {
    adapt_original_common_dialog(dialog);
  }
  return 0U;
}

void apply_original_common_dialog_host(OPENFILENAMEW& dialog) {
  dialog.Flags |= OFN_ENABLEHOOK;
  dialog.lpfnHook = original_common_dialog_host_hook;
}

bool save_original_tower_as(HWND window) {
  for (;;) {
    // 10d0:0401/0409 demotes the enabled palettes and restores Arrow before
    // every common-dialog attempt, including retries after an invalid name.
    apply_original_auxiliary_window_activation_state(false);
    SetCursor(resolve_original_cursor(0U));

    constexpr auto file_dialog =
        simtower::original_tdt_file_dialog_profile();
    std::array<wchar_t, file_dialog.maximum_file_characters> selected{};
    std::array<wchar_t,
               file_dialog.maximum_file_title_characters + 1U> file_title{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = kTdtFilter;
    dialog.nFilterIndex = file_dialog.filter_index;
    dialog.lpstrFile = selected.data();
    dialog.nMaxFile = static_cast<DWORD>(
        file_dialog.maximum_file_characters);
    dialog.lpstrFileTitle = file_title.data();
    dialog.nMaxFileTitle = static_cast<DWORD>(
        file_dialog.maximum_file_title_characters);
    // 10d0:0462-046b points both Open and Save As at DS:3120, populated from
    // [Paths] Save during 1128:02aa. The pointer remains non-null even when
    // that fixed buffer contains the empty string.
    dialog.lpstrInitialDir = g_original_save_directory.c_str();
    dialog.lpstrTitle = kSaveTdtTitle;
    dialog.Flags = file_dialog.flags;
    apply_original_common_dialog_host(dialog);
    dialog.lpstrDefExt = kTdtDefaultExtension;
    // 10d0:049c intentionally calls GETOPENFILENAME for the Save As sheet.
    if (!GetOpenFileNameW(&dialog)) {
      apply_original_auxiliary_window_activation_state(true);
      return false;
    }

    auto normalized = simtower::original_tdt_normalized_path(selected.data());
    if (!simtower::original_tdt_basename_is_valid(normalized)) {
      MessageBoxA(nullptr, kInvalidFilename, kSaveDialogTitle,
                  MB_OK | MB_ICONEXCLAMATION);
      continue;
    }
    const bool saved = save_original_tower_to(
        window, normalized,
        simtower::original_tdt_save_overwrite_prompt(true));
    apply_original_auxiliary_window_activation_state(true);
    return saved;
  }
}

bool save_original_tower(HWND window) {
  if (g_tower_path.empty() || g_tower_file_title == L"untitled") {
    return save_original_tower_as(window);
  }
  // 10d0:0305 clears the overwrite-prompt flag for a normal Save.
  return save_original_tower_to(
      window, g_tower_path,
      simtower::original_tdt_save_overwrite_prompt(false));
}

bool confirm_original_tower_transition(HWND window,
                                       std::uint16_t action_index) {
  if (!simtower::original_tower_transition_requires_confirmation(
          g_tower_document ? &*g_tower_document : nullptr)) {
    return true;
  }
  if (g_audio) {
    g_audio->stop_all(true);
  }
  const std::string action = simtower::original_strl_entry(
      g_resources.find("STRL", 1001), action_index);
  const std::string tower_name(g_tower_file_title.begin(),
                               g_tower_file_title.end());
  const std::array<std::string_view, 4> substitutions = {
      tower_name, action, std::string_view{}, std::string_view{}};
  const int result = simtower::show_original_alert(
      window, g_resources, 1001, substitutions);
  if (result == 1) {
    return save_original_tower(window);
  }
  return result == 2;
}

// Complete 10d0:0225/062a accepted-path transaction shared by the Open sheet
// and 1128:00e5's startup command-line path. The loader resets the working
// banks before touching disk, maps every failure to its original alert, then
// 0225 falls back to a fresh tower rather than restoring the previous one.
bool load_original_tower_from_path(HWND window,
                                   const std::filesystem::path& selected,
                                   std::wstring file_title) {
  remove_original_recent_file_items(window);
  auto empty_document = simtower::make_original_new_tdt();
  simtower::carry_original_process_random_state(
      g_tower_document ? &*g_tower_document : nullptr, empty_document);
  g_tower_document = std::move(empty_document);
  reset_original_simulation_state();
  reset_original_command_state();
  g_lobby_drag = {};
  try {
    auto document = simtower::load_original_tdt_file(selected);
    simtower::carry_original_process_random_state(
        g_tower_document ? &*g_tower_document : nullptr, document);
    g_tower_document = std::move(document);
    reset_original_simulation_state();
    reset_original_command_state();
    refresh_original_rating_command(g_tower_document->header.rating, 0U);
    restore_original_derived_view_position(
        window, g_tower_document->header.view_x,
        g_tower_document->header.view_y);
    present_original_document_transition_synchronously();
    g_lobby_drag = {};
    g_tower_path = selected;
    g_tower_file_title = !file_title.empty()
                             ? std::move(file_title)
                             : g_tower_path.filename().wstring();
    g_tower_dirty = false;
    update_original_tower_title(window);
    const auto clamped_view = current_original_view(window);
    g_tower_document->header.view_x =
        static_cast<std::uint16_t>(clamped_view.x);
    g_tower_document->header.view_y =
        static_cast<std::uint16_t>(clamped_view.y);
    return true;
  } catch (const simtower::OriginalTdtError& error) {
    show_original_file_error(original_error_string_for_status(error.status()));
  } catch (const simtower::OriginalTdtFileError&) {
    // 10d0:0664 uses STRL/1002 entry 2 for an open failure.
    show_original_file_error(2);
  }
  begin_original_new_tower(window);
  return false;
}

// Exact native file-command boundary for 10d0:0122. The Win32 common dialog
// preserves the original 128/15-character buffers, flags, filter, caption and
// extension; an accepted path enters 10d0:0225 above.
bool open_original_tower(HWND window) {
  if (!confirm_original_tower_transition(window, 2)) {
    return false;
  }
  // 10d0:0143/014b demotes the enabled palettes and restores Arrow before
  // entering COMMDLG; 0205 restores their application-active ordering on
  // cancellation, load failure/fallback, and success alike.
  apply_original_auxiliary_window_activation_state(false);
  SetCursor(resolve_original_cursor(0U));
  constexpr auto file_dialog =
      simtower::original_tdt_file_dialog_profile();
  std::array<wchar_t, file_dialog.maximum_file_characters> selected{};
  std::array<wchar_t,
             file_dialog.maximum_file_title_characters + 1U> file_title{};
  OPENFILENAMEW dialog{};
  dialog.lStructSize = sizeof(dialog);
  dialog.hwndOwner = window;
  dialog.lpstrFilter = kTdtFilter;
  dialog.nFilterIndex = file_dialog.filter_index;
  dialog.lpstrFile = selected.data();
  dialog.nMaxFile = static_cast<DWORD>(
      file_dialog.maximum_file_characters);
  dialog.lpstrFileTitle = file_title.data();
  dialog.nMaxFileTitle = static_cast<DWORD>(
      file_dialog.maximum_file_title_characters);
  dialog.lpstrInitialDir = g_original_save_directory.c_str();
  dialog.lpstrTitle = kOpenTdtTitle;
  dialog.Flags = file_dialog.flags;
  apply_original_common_dialog_host(dialog);
  dialog.lpstrDefExt = kTdtDefaultExtension;
  if (!GetOpenFileNameW(&dialog)) {
    apply_original_auxiliary_window_activation_state(true);
    return false;
  }
  const bool loaded = load_original_tower_from_path(
      window, selected.data(),
      file_title[0] != L'\0' ? std::wstring(file_title.data())
                              : std::wstring{});
  apply_original_auxiliary_window_activation_state(true);
  return loaded;
}

bool initialize_original_startup_memory() {
  // 1128:1318 compacts the Win16 global heap, reads GETFREESPACE in bytes,
  // adds SimTower's resident 0x942K, and requires an unsigned total of
  // 0x1770K. Modern Windows manages global-heap compaction automatically;
  // available page-file commitment is the corresponding native boundary.
  MEMORYSTATUSEX memory{};
  memory.dwLength = sizeof(memory);
  if (!GlobalMemoryStatusEx(&memory)) {
    return true;
  }
  const auto free_space_bytes = static_cast<std::uint32_t>(
      std::min<std::uint64_t>(
          memory.ullAvailPageFile,
          std::numeric_limits<std::uint32_t>::max()));
  if (simtower::original_startup_memory_sufficient(free_space_bytes)) {
    return true;
  }
  const auto message =
      simtower::original_startup_low_memory_message(free_space_bytes);
  MessageBoxA(nullptr, message.c_str(), "Error", MB_OK | MB_ICONEXCLAMATION);
  return false;
}

bool confirm_original_startup_capability(
    simtower::OriginalStartupCapabilityIssue issue,
    bool& sound_available) {
  using Issue = simtower::OriginalStartupCapabilityIssue;
  switch (issue) {
    case Issue::none:
      return true;
    case Issue::fewer_than_256_colors:
      return MessageBoxA(nullptr,
                         simtower::kOriginalStartupLowColorMessage.data(),
                         "SimTower",
                         MB_YESNO | MB_ICONEXCLAMATION) != IDNO;
    case Issue::missing_bitblt:
    case Issue::missing_device_independent_bitmap:
    case Issue::missing_dib_to_device:
    case Issue::missing_stretchblt:
      return MessageBoxA(nullptr,
                         simtower::kOriginalStartupMissingRasterMessage.data(),
                         "SimTower", MB_YESNO | MB_ICONEXCLAMATION) != IDNO;
    case Issue::truetype_unsupported:
      MessageBoxA(
          nullptr,
          simtower::kOriginalStartupTrueTypeUnsupportedMessage.data(),
          "SimTower", MB_OK);
      return false;
    case Issue::truetype_disabled:
      MessageBoxA(nullptr,
                  simtower::kOriginalStartupTrueTypeDisabledMessage.data(),
                  "SimTower", MB_OK);
      return false;
    case Issue::wave_output_unavailable:
      if (MessageBoxA(nullptr,
                      simtower::kOriginalStartupNoWaveOutputMessage.data(),
                      "SimTower",
                      MB_YESNO | MB_ICONEXCLAMATION | MB_SYSTEMMODAL) == IDNO) {
        return false;
      }
      sound_available = false;
      return true;
  }
  return false;
}

bool apply_original_startup_capability_issues(
    const std::array<simtower::OriginalStartupCapabilityIssue, 8>& issues,
    bool& sound_available) {
  for (const auto issue : issues) {
    if (issue == simtower::OriginalStartupCapabilityIssue::none) break;
    if (!confirm_original_startup_capability(issue, sound_available)) {
      return false;
    }
  }
  return true;
}

void register_original_tdt_profile_if_missing() {
  // 1128:12af-1301 reads the legacy WIN.INI [Extensions] tdt value. Only an
  // absent value is written, as "<module filename> ^.tdt"; the return value
  // is ignored. Preserve the original externally visible shell association.
  std::array<char, 80> existing{};
  if (GetProfileStringA("Extensions", "tdt", nullptr, existing.data(),
                        static_cast<DWORD>(existing.size())) != 0U) {
    return;
  }
  std::array<char, 32768> module{};
  const DWORD length = GetModuleFileNameA(
      g_instance, module.data(), static_cast<DWORD>(module.size()));
  if (length == 0U || length >= module.size()) return;
  const auto value = simtower::original_tdt_extension_profile_value(
      std::string_view(module.data(), length));
  WriteProfileStringA("Extensions", "tdt", value.c_str());
}

bool initialize_original_startup_capabilities(bool& sound_available) {
  // Complete 1128:1139 preflight order: capture the system-font ascent and
  // screen raster facts, perform 1318's memory gate, ask about each missing
  // display operation independently, require TrueType availability/enabled
  // state, probe wave device zero, and finally publish the .tdt association.
  TEXTMETRICA metrics{};
  std::uint16_t bits_per_pixel = 0U;
  std::uint16_t raster_caps = 0U;
  if (const HDC screen = GetDC(nullptr)) {
    GetTextMetricsA(screen, &metrics);
    raster_caps =
        static_cast<std::uint16_t>(GetDeviceCaps(screen, RASTERCAPS));
    bits_per_pixel =
        static_cast<std::uint16_t>(GetDeviceCaps(screen, BITSPIXEL));
    ReleaseDC(nullptr, screen);
  }
  g_original_system_font_ascent = metrics.tmAscent;

  if (!initialize_original_startup_memory()) return false;

  constexpr std::uint16_t kRequiredRasterCaps =
      simtower::kOriginalRasterCapBitBlt |
      simtower::kOriginalRasterCapDeviceIndependentBitmap |
      simtower::kOriginalRasterCapDibToDevice |
      simtower::kOriginalRasterCapStretchBlt;
  if (!apply_original_startup_capability_issues(
          simtower::original_startup_capability_issues(
              {bits_per_pixel, raster_caps,
               simtower::kOriginalRasterizerTrueTypeAvailable |
                   simtower::kOriginalRasterizerTrueTypeEnabled,
               true}),
          sound_available)) {
    return false;
  }

  RASTERIZER_STATUS rasterizer{};
  GetRasterizerCaps(&rasterizer, sizeof(rasterizer));
  if (!apply_original_startup_capability_issues(
          simtower::original_startup_capability_issues(
              {8U, kRequiredRasterCaps,
               static_cast<std::uint16_t>(rasterizer.wFlags), true}),
          sound_available)) {
    return false;
  }

  WAVEOUTCAPSW wave_capabilities{};
  const bool wave_output_available =
      waveOutGetDevCapsW(0, &wave_capabilities, sizeof(wave_capabilities)) ==
      MMSYSERR_NOERROR;
  if (!apply_original_startup_capability_issues(
          simtower::original_startup_capability_issues(
              {8U, kRequiredRasterCaps,
               simtower::kOriginalRasterizerTrueTypeAvailable |
                   simtower::kOriginalRasterizerTrueTypeEnabled,
               wave_output_available}),
          sound_available)) {
    return false;
  }

  register_original_tdt_profile_if_missing();
  return true;
}

bool initialize_original_audio() {
  // Native backend for the following 11c8:006b WAVMIX initialization. The
  // device-zero capability gate has already run in 1128:1275-12af above; an
  // accepted no-wave fallback reaches this routine with sound disabled.
  if (!g_audio->initialize()) {
    const int choice = MessageBoxA(nullptr, kWaveMixFailureMessage, "SimTower",
                                   MB_YESNO | MB_ICONEXCLAMATION |
                                       MB_SYSTEMMODAL);
    g_audio->set_sound_enabled(false);
    return choice == IDYES;
  }
  return true;
}

bool original_regular_file(const std::filesystem::path& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES &&
         (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0U;
}

std::optional<std::filesystem::path> find_original_startup_profile() {
  // 1128:03ad-044e probes %WINDIR%\SIMTOWER.INI first, then the installed
  // program directory's lowercase simtower.ini spelling.
  std::array<wchar_t, 32768> buffer{};
  const UINT windows_length =
      GetWindowsDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
  if (windows_length != 0U && windows_length < buffer.size()) {
    const auto path = std::filesystem::path(buffer.data()) / L"SIMTOWER.INI";
    if (original_regular_file(path)) {
      return path;
    }
  }

  const DWORD module_length = GetModuleFileNameW(
      nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (module_length != 0U && module_length < buffer.size()) {
    const auto path =
        std::filesystem::path(buffer.data()).parent_path() / L"simtower.ini";
    if (original_regular_file(path)) {
      return path;
    }
  }
  return std::nullopt;
}

simtower::OriginalSoundProfileValues read_original_sound_profile(
    bool wavemix_available,
    const std::optional<std::filesystem::path>& path) {
  if (!path) {
    // The supplied installation's SIMTOWER.INI is part of the ripped corpus.
    // Embed its five values so the single-file release reproduces the shipped
    // configuration without requiring a sidecar file. A real original INI at
    // either recovered search location still takes precedence.
    return {true, 0U, 1U, 1U, 1U, 1U};
  }

  simtower::OriginalSoundProfileValues values{};
  values.profile_available = true;
  values.beep_only =
      GetPrivateProfileIntW(L"Sound", L"BeepOnly", 0, path->c_str());
  if (values.beep_only == 1U || !wavemix_available) {
    // The original jumps over all four remaining reads in both cases.
    return values;
  }
  values.all_sounds =
      GetPrivateProfileIntW(L"Sound", L"AllSounds", 1, path->c_str());
  values.elevator =
      GetPrivateProfileIntW(L"Sound", L"Elevator", 1, path->c_str());
  values.events =
      GetPrivateProfileIntW(L"Sound", L"Events", 1, path->c_str());
  values.background =
      GetPrivateProfileIntW(L"Sound", L"Background", 1, path->c_str());
  return values;
}

void apply_original_startup_profile() {
  if (!g_audio) {
    return;
  }
  const auto path = find_original_startup_profile();
  const bool wavemix_available = g_audio->sound_enabled();
  const auto state = simtower::original_sound_profile_state(
      wavemix_available,
      read_original_sound_profile(wavemix_available, path));
  g_original_beep_only = state.beep_only;
  for (std::size_t category = 0; category < state.category_enabled.size();
       ++category) {
    g_audio->set_category_enabled(category, state.category_enabled[category]);
  }
  g_audio->set_sound_enabled(state.sound_enabled);

  // 1128:04f7-0515 reads Paths/Save after every profile-backed sound branch,
  // including BeepOnly and unavailable WAVMIX. 0517-0542 instead clears the
  // 128-byte buffer when neither INI candidate exists. For the self-contained
  // build, absence means use the value ripped from the supplied installation,
  // exactly as the five embedded Sound defaults above do.
  if (!path) {
    g_original_save_directory = simtower::kOriginalShippedSaveDirectory;
    return;
  }
  std::array<wchar_t, simtower::kOriginalStartupSavePathCapacity> save_path{};
  GetPrivateProfileStringW(L"Paths", L"Save", L"", save_path.data(),
                           static_cast<DWORD>(save_path.size()),
                           path->c_str());
  g_original_save_directory = save_path.data();
}

void apply_original_audio_menu_state(HMENU menu) {
  if (!menu || !g_audio) {
    return;
  }
  constexpr std::array<UINT, 3> commands{40011U, 40012U, 40013U};
  for (std::size_t category = 0; category < commands.size(); ++category) {
    CheckMenuItem(menu, commands[category],
                  MF_BYCOMMAND |
                      (g_audio->category_enabled(category) ? MF_CHECKED
                                                           : MF_UNCHECKED));
    EnableMenuItem(menu, commands[category],
                   MF_BYCOMMAND | (g_audio->sound_enabled() ? MF_ENABLED
                                                            : MF_GRAYED));
  }
}

void toggle_original_audio_category(HWND window, UINT command,
                                    std::size_t category) {
  if (!g_audio) {
    return;
  }
  const bool was_enabled = g_audio->category_enabled(category);
  // 1158:07c1/07fd/0839 flush every channel before disabling any category.
  if (was_enabled) {
    g_audio->stop_all(true);
  }
  g_audio->set_category_enabled(category, !was_enabled);
  CheckMenuItem(GetMenu(window), command,
                MF_BYCOMMAND | (!was_enabled ? MF_CHECKED : MF_UNCHECKED));
}

void toggle_original_menu_flag(HWND window, UINT command, bool& enabled) {
  enabled = !enabled;
  CheckMenuItem(GetMenu(window), command,
                MF_BYCOMMAND | (enabled ? MF_CHECKED : MF_UNCHECKED));
}

void toggle_original_auxiliary_window(HWND owner, UINT command, HWND target,
                                      bool& visible, HWND insert_after) {
  const auto plan = simtower::original_auxiliary_visibility_plan(
      simtower::OriginalAuxiliaryVisibilityTrigger::menu_command, visible);
  visible = plan.visible;
  if (plan.operation == simtower::OriginalAuxiliaryWindowOperation::show) {
    // 1158:08b5-09a2 uses the literal flag word 0x43: preserve z-order
    // target selection while showing without moving or sizing.
    SetWindowPos(target, insert_after, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  } else {
    ShowWindow(target, SW_HIDE);
  }
  if (plan.update_menu_check) {
    CheckMenuItem(GetMenu(owner), command,
                  MF_BYCOMMAND | (visible ? MF_CHECKED : MF_UNCHECKED));
  }
}

void apply_original_auxiliary_window_size_state(bool restore) {
  const auto actions = simtower::original_auxiliary_window_actions(
      restore, g_toolbar_visible, g_info_visible, g_map_visible);
  for (const auto& action : actions) {
    HWND target{};
    switch (action.target) {
      case simtower::OriginalAuxiliaryWindow::command:
        target = g_command_window;
        break;
      case simtower::OriginalAuxiliaryWindow::info:
        target = g_info_window;
        break;
      case simtower::OriginalAuxiliaryWindow::map:
        target = g_map_window;
        break;
    }
    if (!target) continue;
    if (action.operation ==
        simtower::OriginalAuxiliaryWindowOperation::hide) {
      ShowWindow(target, SW_HIDE);
      continue;
    }

    HWND insert_after = HWND_TOP;
    if (action.insert_after ==
        simtower::OriginalAuxiliaryInsertAfter::topmost) {
      insert_after = HWND_TOPMOST;
    } else if (action.insert_after ==
               simtower::OriginalAuxiliaryInsertAfter::command) {
      insert_after = g_command_window;
    }
    // 1078:0000 passes Win16 flag word 0x53: NOSIZE, NOMOVE, NOACTIVATE,
    // SHOWWINDOW. This preserves each palette's position and dimensions.
    SetWindowPos(target, insert_after, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE |
                     SWP_SHOWWINDOW);
  }
}

void apply_original_auxiliary_window_activation_state(bool active) {
  const auto actions = simtower::original_auxiliary_activation_actions(
      active, g_toolbar_visible, g_info_visible, g_map_visible);
  for (const auto& action : actions) {
    HWND target{};
    switch (action.target) {
      case simtower::OriginalAuxiliaryWindow::command:
        target = g_command_window;
        break;
      case simtower::OriginalAuxiliaryWindow::info:
        target = g_info_window;
        break;
      case simtower::OriginalAuxiliaryWindow::map:
        target = g_map_window;
        break;
    }
    if (!target) continue;

    HWND insert_after = HWND_TOP;
    if (action.insert_after ==
        simtower::OriginalAuxiliaryActivationInsertAfter::topmost) {
      insert_after = HWND_TOPMOST;
    } else if (action.insert_after ==
               simtower::OriginalAuxiliaryActivationInsertAfter::main) {
      insert_after = g_main_window;
    }
    // Literal 1078:0212/0230/024c/027c flag word 0x13.
    SetWindowPos(target, insert_after, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
  }
}

void apply_original_palette_app_activation(bool active) {
  const auto plan = simtower::original_palette_app_activation_plan(
      active, g_main_window != nullptr);
  if (plan.promote_main) {
    // Literal Win16 flag word 0x53 at 1050:004d, 1120:004a, and 1168:004d:
    // NOSIZE, NOMOVE, NOACTIVATE, SHOWWINDOW with insert-after HWND_TOP.
    SetWindowPos(g_main_window, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE |
                     SWP_SHOWWINDOW);
  }
  apply_original_auxiliary_window_activation_state(plan.forwarded_active);
}

void center_native_dialog(HWND dialog) {
  // Exact native boundary for 11e0:0c10: center against the desktop and call
  // SetWindowPos(HWND_TOP, ..., flags 0x41).
  RECT desktop{};
  RECT window{};
  GetWindowRect(GetDesktopWindow(), &desktop);
  GetWindowRect(dialog, &window);
  const auto position = simtower::original_dialog_center_position(
      {desktop.left, desktop.top, desktop.right, desktop.bottom},
      {window.left, window.top, window.right, window.bottom});
  SetWindowPos(dialog, HWND_TOP, position.left, position.top,
               0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
}

void position_original_dialog(HWND dialog, int requested_left = 0) {
  RECT desktop{};
  RECT window{};
  GetWindowRect(GetDesktopWindow(), &desktop);
  GetWindowRect(dialog, &window);
  const auto position = simtower::original_dialog_screen_position(
      {desktop.left, desktop.top, desktop.right, desktop.bottom},
      {window.left, window.top, window.right, window.bottom},
      requested_left);
  // 11e0:0b52 passes insert-after -1 and flags 0x41: TOPMOST, NOSIZE,
  // SHOWWINDOW. This placement is shared by the eleven original call sites.
  SetWindowPos(dialog, HWND_TOPMOST, position.left, position.top,
               0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
}

void paint_original_startup_splash(HWND dialog,
                                   const OriginalStartupSplashContext& context) {
  PAINTSTRUCT paint{};
  HDC dc = BeginPaint(dialog, &paint);
  if (!dc) return;
  if (g_logical_palette) {
    // Both SETUPSTARTUPDLGA 1010:0220-0231 and SETUPSTARTUPDLGB
    // 1010:0395-03a6 select and realize the shared palette before black fill.
    SelectPalette(dc, g_logical_palette, FALSE);
    RealizePalette(dc);
  }
  RECT client{};
  GetClientRect(dialog, &client);
  HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
  if (black) {
    FillRect(dc, &client, black);
    DeleteObject(black);
  }
  const auto placement = simtower::original_startup_bitmap_placement(
      g_resources, context.bitmap_id,
      client.right - client.left, client.bottom - client.top);
  simtower::draw_original_dib(
      dc, g_resources.find("BITMAP", context.bitmap_id),
      placement.left, placement.top);
  EndPaint(dialog, &paint);
}

// Native filters for SETUPSTARTUPDLGA/SETUPSTARTUPDLGB at
// 1010:014c/0304. The modeless B path is the one invoked by 1128:0042/00a2;
// the modal A mouse-dismissal branch is preserved for the exported boundary.
INT_PTR CALLBACK startup_splash_dialog_proc(HWND dialog, UINT message,
                                             WPARAM, LPARAM lparam) {
  if (message == WM_INITDIALOG) {
    auto* context = reinterpret_cast<OriginalStartupSplashContext*>(lparam);
    SetWindowLongPtrW(dialog, DWLP_USER,
                      reinterpret_cast<LONG_PTR>(context));
    RECT desktop{};
    GetWindowRect(GetDesktopWindow(), &desktop);
    const auto size = simtower::original_startup_window_size(
        desktop.right, desktop.bottom, context->modal,
        GetSystemMetrics(SM_CXBORDER), GetSystemMetrics(SM_CYBORDER));
    MoveWindow(dialog, 0, 0, size.width, size.height, TRUE);
    return TRUE;
  }
  auto* context = reinterpret_cast<OriginalStartupSplashContext*>(
      GetWindowLongPtrW(dialog, DWLP_USER));
  switch (message) {
    case WM_PAINT:
      if (context) paint_original_startup_splash(dialog, *context);
      return TRUE;
    case WM_LBUTTONDOWN:
      if (context && context->modal) {
        EndDialog(dialog,
                  simtower::kOriginalStartupSplashModalDismissResult);
        return TRUE;
      }
      break;
    case WM_DESTROY:
      // SETUPSTARTUPDLGB 1010:0469 frees its current DIB resource and
      // returns TRUE. Embedded resource views need no native release.
      if (context && !context->modal) return TRUE;
      break;
    case WM_NCDESTROY:
      SetWindowLongPtrW(dialog, DWLP_USER, 0);
      return TRUE;
  }
  return FALSE;
}

bool create_original_startup_splash(int bitmap_id) {
  // 1010:0018 creates named DIALOG/TOWER_TITLE modelessly and passes the
  // requested bitmap resource ID as its initialization value.
  g_startup_splash = {bitmap_id, false};
  const auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", "TOWER_TITLE"));
  const auto native = simtower::build_native_dialog_template(original);
  g_startup_splash_window = CreateDialogIndirectParamW(
      g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
      nullptr, startup_splash_dialog_proc,
      reinterpret_cast<LPARAM>(&g_startup_splash));
  if (!g_startup_splash_window) return false;
  SetCursor(LoadCursorW(nullptr, IDC_WAIT));
  SetClassLongPtrW(g_startup_splash_window, GCLP_HCURSOR, 0);
  ShowWindow(g_startup_splash_window, SW_SHOW);
  UpdateWindow(g_startup_splash_window);
  return true;
}

void update_original_startup_splash(int bitmap_id) {
  if (!g_startup_splash_window) return;
  // 1010:0018's existing-window branch reloads the DIB, invalidates the whole
  // splash with erase enabled, and repaints synchronously.
  g_startup_splash.bitmap_id = bitmap_id;
  InvalidateRect(g_startup_splash_window, nullptr, TRUE);
  UpdateWindow(g_startup_splash_window);
  // 1128:00af-00c3 repeats the class-cursor clear and Wait selection after
  // BITMAP/256 has been synchronously presented. This keeps mouse movement
  // during the following bootstrap/New-or-Load interval from restoring the
  // dialog class cursor.
  SetClassLongPtrW(g_startup_splash_window, GCLP_HCURSOR, 0);
  SetCursor(LoadCursorW(nullptr, IDC_WAIT));
}

void destroy_original_startup_splash() {
  // 1010:00fd owns the modeless splash destruction/resource-release boundary.
  for (const auto step : simtower::original_startup_splash_teardown_plan(
           g_startup_splash_window != nullptr)) {
    using Step = simtower::OriginalStartupSplashTeardownStep;
    switch (step) {
      case Step::destroy_window:
        DestroyWindow(g_startup_splash_window);
        break;
      case Step::clear_window_handle:
        g_startup_splash_window = nullptr;
        break;
      case Step::release_proc_instance:
        // Native dialog procedures are ordinary borrowed function pointers.
        break;
      case Step::release_bitmap_resource_if_present:
        // Embedded resource views are borrowed; clear the retained view state.
        g_startup_splash = {};
        break;
      case Step::clear_proc_pointer:
      case Step::none:
        break;
    }
  }
}

HFONT make_original_dialog_font(int pixel_height);

struct OriginalStartupDialogContext {
  HFONT paint_font{};
  HFONT control_font{};
};

void paint_original_startup_dialog(HWND dialog,
                                   HDC dc,
                                   const OriginalStartupDialogContext& context,
                                   bool render_dtmp) {
  const auto style = simtower::original_new_load_dialog_style();
  if (style.realize_logical_palette && g_logical_palette) {
    SelectPalette(dc, g_logical_palette, FALSE);
    RealizePalette(dc);
  }
  const auto previous_alignment = SetTextAlign(dc, TA_UPDATECP);
  const auto previous_background = SetBkMode(dc, TRANSPARENT);
  HGDIOBJ previous_font =
      context.paint_font ? SelectObject(dc, context.paint_font) : nullptr;
  simtower::paint_original_dialog_chrome(dialog, dc, g_startup_dtmp);
  if (render_dtmp) {
    simtower::render_original_dtmp(dialog, dc, g_startup_dtmp, g_resources);
  }
  if (context.paint_font) SelectObject(dc, previous_font);
  SetBkMode(dc, previous_background);
  SetTextAlign(dc, previous_alignment);
}

// Native message boundary for exported NEWORLOADDLOGFILTER at 1018:0067.
INT_PTR CALLBACK startup_dialog_proc(HWND dialog, UINT message,
                                     WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_INITDIALOG: {
      auto* context = reinterpret_cast<OriginalStartupDialogContext*>(lparam);
      SetWindowLongPtrW(dialog, DWLP_USER,
                        reinterpret_cast<LONG_PTR>(context));
      const auto style = simtower::original_new_load_dialog_style();
      context->paint_font =
          make_original_dialog_font(style.paint_font_pixels);
      context->control_font =
          make_original_dialog_font(style.control_font_pixels);
      // NEWORLOADDLOGFILTER 1018:0096-0142.
      simtower::configure_original_dtmp_window(dialog, g_startup_dtmp,
                                               g_resources,
                                               g_logical_palette);
      if (style.clear_class_cursor) {
        SetClassLongPtrW(dialog, GCLP_HCURSOR, 0);
      }
      SetCursor(LoadCursorW(nullptr, IDC_ARROW));
      center_native_dialog(dialog);
      if (style.show_during_initialization) ShowWindow(dialog, SW_SHOW);
      HDC dc = GetDC(dialog);
      if (dc) {
        paint_original_startup_dialog(dialog, dc, *context, true);
        ReleaseDC(dialog, dc);
      }
      SetFocus(GetDlgItem(dialog, 1));
      return TRUE;
    }
    case WM_PAINT: {
      // NEWORLOADDLOGFILTER 1018:015e-01bd.
      auto* context = reinterpret_cast<OriginalStartupDialogContext*>(
          GetWindowLongPtrW(dialog, DWLP_USER));
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(dialog, &paint);
      if (dc && context) {
        paint_original_startup_dialog(dialog, dc, *context, false);
      }
      EndPaint(dialog, &paint);
      return TRUE;
    }
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSCROLLBAR:
    case WM_CTLCOLORSTATIC: {
      auto* context = reinterpret_cast<OriginalStartupDialogContext*>(
          GetWindowLongPtrW(dialog, DWLP_USER));
      if (context && context->control_font) {
        SelectObject(reinterpret_cast<HDC>(wparam), context->control_font);
      }
      return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
    }
    case WM_COMMAND:
      {
        const auto plan = simtower::original_new_load_dialog_command_plan(
            static_cast<std::uint16_t>(LOWORD(wparam)));
        if (plan.release_dtmp_before_end) {
          // Native value ownership replaces 1070:051f's HWND-slot lookup,
          // FreeResource, slot clear, and live-count decrement.
          g_startup_dtmp = {};
        }
        if (plan.end_dialog) {
          EndDialog(dialog, plan.dialog_result);
        }
        return plan.consume ? TRUE : FALSE;
      }
  }
  return FALSE;
}

int run_original_startup_dialog() {
  // Exact native launcher for 1018:0000: load DIALOG/10000 and run the
  // NEWORLOADDLOGFILTER modal transaction owned by 1018:0067.
  const auto launcher = simtower::original_new_load_launcher_contract();
  const auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", launcher.dialog_resource_id));
  const auto native = simtower::build_native_dialog_template(original);
  g_startup_dtmp = simtower::parse_original_dtmp(
      g_resources.find("DTMP", launcher.dialog_resource_id));
  OriginalStartupDialogContext context{};
  // Win16's modal manager blocked every window in the task. Win32 DialogBox
  // disables only its direct owner, so retaining hidden Main as the native
  // owner leaves the separate modeless splash enabled and able to cover this
  // chooser. Own New/Load by the live splash until startup tears it down.
  const auto owner_plan = simtower::original_startup_native_modal_owner(
      g_startup_splash_window != nullptr);
  const HWND native_owner =
      owner_plan ==
              simtower::OriginalStartupNativeModalOwner::active_startup_splash
          ? g_startup_splash_window
          : (launcher.main_window_owner ? g_main_window : nullptr);
  return static_cast<int>(run_original_modal_dialog(
      g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
      native_owner, startup_dialog_proc,
      reinterpret_cast<LPARAM>(&context)));
}

struct OriginalEventDialogContext {
  simtower::OriginalDtmp dtmp{};
  struct TextOverlay {
    std::size_t rectangle_index{};
    std::string text{};
  };
  std::array<TextOverlay, 2> text_overlays{};
  std::size_t text_overlay_count{};
  std::vector<simtower::OriginalFacilityPersonSprite> person_sprites{};
  std::optional<simtower::OriginalMagnifierTarget> transport_target{};
  std::int32_t argument{};
  bool use_ahotta_filter{};
  bool timer_fired{};
  HBRUSH static_background{};
  HFONT control_font{};
};

void release_native_dialog_brush(HBRUSH& brush) {
  // The recovered filters share the process-global DS:31ae gray brush and do
  // not own a WM_DESTROY/WM_NCDESTROY branch. Native contexts use distinct
  // brushes, so release that host-only ownership after DialogBox has returned
  // instead of adding a synthetic message to an original filter table.
  if (!brush) return;
  DeleteObject(brush);
  brush = nullptr;
}

std::wstring original_cp1252_to_wide(std::string_view text) {
  if (text.empty()) return {};
  const int length = MultiByteToWideChar(
      1252, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (length <= 0) return {};
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  MultiByteToWideChar(1252, 0, text.data(), static_cast<int>(text.size()),
                      result.data(), length);
  return result;
}

std::string original_wide_to_cp1252(std::wstring_view text) {
  if (text.empty()) return {};
  const int length = WideCharToMultiByte(
      1252, 0, text.data(), static_cast<int>(text.size()),
      nullptr, 0, nullptr, nullptr);
  if (length <= 0) return {};
  std::string result(static_cast<std::size_t>(length), '\0');
  WideCharToMultiByte(1252, 0, text.data(), static_cast<int>(text.size()),
                      result.data(), length, nullptr, nullptr);
  return result;
}

void paint_original_information_text(HDC dc,
                                     const OriginalEventDialogContext& context) {
  if (context.text_overlay_count == 0U) return;
  HFONT temporary_font = nullptr;
  HFONT font = context.control_font;
  if (!font) {
    temporary_font = CreateFontW(
        -13, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    font = temporary_font;
  }
  HGDIOBJ old_font = font ? SelectObject(dc, font) : nullptr;
  const auto old_align = SetTextAlign(dc, TA_UPDATECP);
  const auto old_background = SetBkMode(dc, TRANSPARENT);
  for (std::size_t index = 0U; index < context.text_overlay_count; ++index) {
    const auto& overlay = context.text_overlays[index];
    if (overlay.rectangle_index >= context.dtmp.rectangles.size()) continue;
    const auto& rectangle = context.dtmp.rectangles[overlay.rectangle_index];
    const auto wide = original_cp1252_to_wide(overlay.text);
    const auto origin = simtower::original_relative_gdi_position(
        static_cast<std::int16_t>(rectangle.left),
        static_cast<std::int16_t>(rectangle.top), 8, 1);
    MoveToEx(dc, origin.x, origin.y, nullptr);
    TextOutW(dc, 0, 0, wide.data(), static_cast<int>(wide.size()));
  }
  SetBkMode(dc, old_background);
  SetTextAlign(dc, old_align);
  if (font) {
    SelectObject(dc, old_font);
  }
  // This fallback belongs to 1060's independent information-overlay path,
  // not the borrowed 1208 font bank used by the dialog filters below.
  if (temporary_font) DeleteObject(temporary_font);
}

void paint_original_information_sprites(
    HDC dc, const OriginalEventDialogContext& context) {
  for (const auto& sprite : context.person_sprites) {
    simtower::draw_original_dib_region_scaled(
        dc, g_resources.find("BITMAP", sprite.bitmap_id),
        sprite.destination_x, sprite.destination_y,
        sprite.width, sprite.height,
        sprite.frame * 8, 0, sprite.width, 24);
  }
}

void close_original_event_dialog(
    HWND dialog,
    OriginalEventDialogContext& context,
    std::uint16_t command) {
  const auto action = simtower::original_event_dialog_action(
      command, context.argument,
      g_tower_document ? g_tower_document->header.balance : 0,
      context.timer_fired);
  switch (action) {
    case simtower::OriginalEventDialogAction::ignore:
      return;
    case simtower::OriginalEventDialogAction::
        warn_insufficient_funds_then_close_decline:
      (void)simtower::show_original_alert(
          dialog, g_resources, 1004,
          std::array<std::string_view, 4>{});
      [[fallthrough]];
    case simtower::OriginalEventDialogAction::close_decline:
      EndDialog(dialog, 1);
      return;
    case simtower::OriginalEventDialogAction::close_accept:
      EndDialog(dialog, 2);
      return;
  }
}

// Native translation of exported AHOTTADLOGFILTER at 1068:00a1. The same
// painter is also reused for the simpler transport-information boundaries;
// those contexts deliberately leave use_ahotta_filter false.
INT_PTR CALLBACK event_dialog_proc(HWND dialog, UINT message,
                                   WPARAM wparam, LPARAM lparam) {
  if (message == WM_INITDIALOG) {
    auto* context = reinterpret_cast<OriginalEventDialogContext*>(lparam);
    SetWindowLongPtrW(dialog, DWLP_USER,
                      reinterpret_cast<LONG_PTR>(context));
    simtower::configure_original_dtmp_window(dialog, context->dtmp,
                                             g_resources,
                                             g_logical_palette);
    // 1068:00fb-010e clears Win16 GCL_HCURSOR, then 11e0:0d80 selector zero
    // selects the stock arrow as the current cursor.
    SetClassLongPtrW(dialog, GCLP_HCURSOR, 0);
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    if (context->use_ahotta_filter) {
      context->control_font = make_original_dialog_font(14);
      SetTimer(dialog, 10U, 0x93e0U, nullptr);
    } else if (context->transport_target) {
      // ELVINFODLOGFILTER 1100:0faf and ESCINFODLOGFILTER 1100:12e7
      // select the shared 13-pixel Arial font for their painted text and
      // static controls.
      context->control_font = make_original_dialog_font(13);
    }
    position_original_dialog(dialog);
    HDC dc = GetDC(dialog);
    if (dc) {
      if (context->transport_target && g_logical_palette) {
        // 1100:0f85-0f97 / 12bd-12cf realize the active CLUT before the
        // first resource-backed information presentation.
        SelectPalette(dc, g_logical_palette, FALSE);
        RealizePalette(dc);
      }
      SetTextAlign(dc, TA_UPDATECP);
      SetBkMode(dc, TRANSPARENT);
      simtower::paint_original_dialog_chrome(dialog, dc, context->dtmp);
      simtower::render_original_dtmp(dialog, dc, context->dtmp, g_resources);
      paint_original_information_text(dc, *context);
      paint_original_information_sprites(dc, *context);
      ReleaseDC(dialog, dc);
    }
    const auto focus_plan =
        simtower::original_painted_dialog_initialization_focus_plan();
    if (focus_plan.set_explicit_focus) {
      SetFocus(GetDlgItem(dialog, 1));
    }
    return focus_plan.consume ? TRUE : FALSE;
  }

  auto* context = reinterpret_cast<OriginalEventDialogContext*>(
      GetWindowLongPtrW(dialog, DWLP_USER));
  switch (message) {
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(dialog, &paint);
      if (dc && context) {
        if (context->transport_target && g_logical_palette) {
          // Exact paint-time palette realization at 1100:10aa-10bc and
          // 1100:1483-1495.
          SelectPalette(dc, g_logical_palette, FALSE);
          RealizePalette(dc);
        }
        SetTextAlign(dc, TA_UPDATECP);
        SetBkMode(dc, TRANSPARENT);
        simtower::paint_original_dialog_chrome(dialog, dc, context->dtmp);
        paint_original_information_text(dc, *context);
        paint_original_information_sprites(dc, *context);
      }
      EndPaint(dialog, &paint);
      return TRUE;
    }
    case WM_ACTIVATE:
      if (context && context->transport_target) {
        const auto plan = simtower::original_information_activation_plan(
            LOWORD(wparam) != WA_INACTIVE,
            g_original_active_modal_window != nullptr,
            g_original_active_modal_window == dialog);
        if (plan.activate_nested_modal) {
          // 1100:1053-106e / 138b-13a6 keep a nested information dialog as
          // the active modal target instead of allowing its parent to steal
          // activation.
          SetActiveWindow(g_original_active_modal_window);
        }
        return plan.consume ? TRUE : FALSE;
      }
      return FALSE;
    case WM_CTLCOLORSTATIC:
      if (context &&
          (context->use_ahotta_filter || context->transport_target)) {
        const auto background =
            simtower::original_facility_control_background(true);
        HDC dc = reinterpret_cast<HDC>(wparam);
        if (context->control_font) SelectObject(dc, context->control_font);
        SetBkMode(dc, TRANSPARENT);
        if (g_logical_palette) {
          SelectPalette(dc, g_logical_palette, FALSE);
          RealizePalette(dc);
        }
        if (background ==
                simtower::OriginalFacilityControlBackground::gray_cc &&
            !context->static_background) {
          COLORREF color = RGB(0xcc, 0xcc, 0xcc);
          if (g_logical_palette) {
            color = PALETTEINDEX(GetNearestPaletteIndex(g_logical_palette,
                                                       color));
          }
          context->static_background = CreateSolidBrush(color);
        }
        return reinterpret_cast<INT_PTR>(context->static_background);
      }
      return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSCROLLBAR:
      // Win16 WM_CTLCOLOR subtype six is static; every other subtype returns
      // stock object 5 (NULL_BRUSH) without changing the background mode.
      return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
    case WM_LBUTTONDOWN:
      if (context && context->transport_target) {
        const auto click_plan =
            simtower::original_transport_information_click_plan(
                context->transport_target->kind);
        if (!click_plan.consume) break;

        HDC dc = GetDC(dialog);
        if (dc && g_logical_palette) {
          if (click_plan.select_palette) {
            SelectPalette(dc, g_logical_palette, FALSE);
          }
          if (click_plan.realize_palette) RealizePalette(dc);
        }

        std::optional<std::size_t> person{};
        if (g_tower_document) {
          person = simtower::original_information_person_sprite_hit(
              context->person_sprites,
              static_cast<std::int16_t>(LOWORD(lparam)),
              static_cast<std::int16_t>(HIWORD(lparam)));
        }
        if (person) {
          bool changed = false;
          (void)run_original_person_information_dialog(
              dialog, *person, &changed,
              simtower::OriginalPersonInformationContext::transport_dialog);
          const auto refreshed =
              simtower::original_transport_information_text(
                  g_resources, *g_tower_document,
                  *context->transport_target);
          context->text_overlays[0].text = refreshed.primary;
          context->text_overlays[1].text = refreshed.secondary;
          context->person_sprites = refreshed.person_sprites;
          InvalidateRect(dialog, nullptr, FALSE);
          UpdateWindow(dialog);
        }
        if (dc) ReleaseDC(dialog, dc);

        // Both original filters perform these restorations after every click,
        // including empty panel space (1100:118e-11a8 / 145a-14e9).
        if (click_plan.restore_topmost) {
          SetWindowPos(dialog, HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
        }
        if (click_plan.restore_modal_target) {
          g_original_active_modal_window = dialog;
        }
        return TRUE;
      }
      break;
    case WM_MOUSEMOVE:
      if (context && context->transport_target) {
        // Exact 1100:5043 information-panel cursor selection.
        const bool portrait_panel =
            simtower::original_information_portrait_panel_hit(
                context->dtmp,
                static_cast<std::int16_t>(LOWORD(lparam)),
                static_cast<std::int16_t>(HIWORD(lparam)));
        SetCursor(resolve_original_cursor(portrait_panel ? 1003U : 0U));
        return TRUE;
      }
      break;
    case WM_TIMER:
      if (context && context->use_ahotta_filter) {
        // 1068:0380 kills the identifier supplied by WM_TIMER rather than
        // assuming the ID used when the timer was created.
        KillTimer(dialog, static_cast<UINT_PTR>(wparam));
        context->timer_fired = true;
        close_original_event_dialog(dialog, *context, 2U);
        return TRUE;
      }
      break;
    case WM_COMMAND:
      if (context && context->use_ahotta_filter) {
        const auto command = static_cast<std::uint16_t>(LOWORD(wparam));
        if (command == 1U || command == 2U) {
          close_original_event_dialog(dialog, *context, command);
          return TRUE;
        }
      } else {
        // ELVINFODLOGFILTER 1100:1113 and ESCINFODLOGFILTER 1100:13d5
        // recognize only the IDOK command and always return dialog result 1.
        if (LOWORD(wparam) == 1U) {
          g_original_active_modal_window = nullptr;
          EndDialog(dialog, 1);
          return TRUE;
        }
      }
      break;
  }
  return FALSE;
}

int run_original_event_dialog(
    const simtower::OriginalEventDialogRequest& request) {
  if (!request.valid()) {
    return 0;
  }

  // 1068:0000 forcibly stops both mixer channels, then submits the dialog's
  // WAVE resource with category zero and priority four before opening it.
  if (g_audio) {
    g_audio->stop_all(true);
    if (request.wave_resource != 0) {
      (void)g_audio->play_resource(request.wave_resource, 0, 4,
                                   GetTickCount());
    }
  }

  auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", request.dialog_id));
  // Exact 1068:0000 -> 1008:0085 event-dialog launcher path: after loading
  // the resource template, the original forces its font point size to 8.
  simtower::apply_original_dialog_font_point_size(original, 8U);
  // 1068:0439 scans every original control before AHOTTADLOGFILTER's item-3
  // fallback at 1068:0175. Apply the exact destructive caret-pair pass to all
  // text controls, then apply the item-3 #0 fallback.
  for (auto& item : original.items) {
    if (item.text.kind != simtower::OriginalDialogValue::Kind::text) continue;
    item.text.text = simtower::format_original_dialog_caret_arguments(
        std::move(item.text.text), request.argument);
  }
  const auto item_three = std::ranges::find_if(
      original.items, [](const auto& item) { return item.id == 3U; });
  if (item_three != original.items.end() &&
      item_three->text.kind == simtower::OriginalDialogValue::Kind::text) {
    item_three->text.text = simtower::format_original_dialog_argument(
        std::move(item_three->text.text), request.argument);
  }
  const auto native = simtower::build_native_dialog_template(original);
  OriginalEventDialogContext context{
      simtower::parse_original_dtmp(
          g_resources.find("DTMP", request.dialog_id))};
  context.argument = request.argument;
  context.use_ahotta_filter = true;
  const int result = static_cast<int>(run_original_modal_dialog(
      g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
      g_main_window, event_dialog_proc,
      reinterpret_cast<LPARAM>(&context)));
  release_native_dialog_brush(context.static_background);
  return result;
}

struct OriginalFinanceDialogContext {
  simtower::OriginalDtmp dtmp{};
  simtower::OriginalFinanceView view{};
  bool pressed{};
};

HFONT make_original_dialog_font(int pixel_height) {
  // Borrow the process-global 1208:0a8d/0ba7 bank. The original clamps below
  // nine, reuses matching heights, and stops selecting once ten slots exist.
  return simtower::original_cached_font(
      static_cast<std::int16_t>(pixel_height));
}

const simtower::OriginalDtmpRect* original_elevator_control_rectangle(
    const OriginalElevatorControlDialogContext& context,
    std::size_t one_based_index) {
  if (one_based_index == 0U ||
      one_based_index > context.dtmp.rectangles.size()) {
    return nullptr;
  }
  return &context.dtmp.rectangles[one_based_index - 1U];
}

RECT original_native_rectangle(const simtower::OriginalDtmpRect& source) {
  return {static_cast<LONG>(source.left), static_cast<LONG>(source.top),
          static_cast<LONG>(source.right), static_cast<LONG>(source.bottom)};
}

bool original_point_in_rectangle(const simtower::OriginalDtmpRect& rectangle,
                                 int x,
                                 int y) {
  return rectangle.left != 0xffffU && rectangle.top != 0xffffU &&
         x >= rectangle.left && x < rectangle.right &&
         y >= rectangle.top && y < rectangle.bottom;
}

void size_original_dialog_client(HWND dialog,
                                 int client_width,
                                 int client_height,
                                 int x,
                                 int y) {
  RECT outer{0, 0, client_width, client_height};
  const auto style = static_cast<DWORD>(GetWindowLongPtrW(dialog, GWL_STYLE));
  const auto extended =
      static_cast<DWORD>(GetWindowLongPtrW(dialog, GWL_EXSTYLE));
  AdjustWindowRectEx(&outer, style, FALSE, extended);
  SetWindowPos(dialog, nullptr, x, y, outer.right - outer.left,
               outer.bottom - outer.top,
               SWP_NOACTIVATE | SWP_NOZORDER);
}

void draw_original_elevator_control_region(
    HDC dc,
    int bitmap_id,
    const simtower::OriginalDtmpRect& rectangle) {
  simtower::draw_original_dib_region(
      dc, g_resources.find("BITMAP", bitmap_id), rectangle.left,
      rectangle.top, rectangle.left, rectangle.top,
      static_cast<int>(rectangle.right) - rectangle.left,
      static_cast<int>(rectangle.bottom) - rectangle.top);
}

void paint_original_elevator_control_grid(
    HDC dc,
    const OriginalElevatorControlDialogContext& context,
    const simtower::OriginalTdtDocument& document) {
  // Exact 1098:16a4 grid-frame and line-count geometry.
  if (!context.state.valid ||
      context.state.elevator_index >= document.elevators.size()) {
    return;
  }
  const auto& elevator =
      document.elevators[context.state.elevator_index];
  const auto* grid_source = original_elevator_control_rectangle(context, 6U);
  if (!grid_source) return;
  RECT grid = original_native_rectangle(*grid_source);
  HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  HBRUSH white = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
  FrameRect(dc, &grid, black);

  HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
  HGDIOBJ previous_pen = pen ? SelectObject(dc, pen) : nullptr;
  int active_columns = 0;
  for (const auto& car : elevator.car_records) {
    if (car.exact_bytes[15] != std::byte{0}) ++active_columns;
  }
  for (int line_index = 1; line_index <= active_columns + 1;
       ++line_index) {
    const int x = grid.left + line_index *
        simtower::kOriginalElevatorControlCellSize;
    MoveToEx(dc, x, grid.top, nullptr);
    LineTo(dc, x, grid.bottom - 1);
  }
  for (int line_index = 1;
       line_index < simtower::kOriginalElevatorControlVisibleFloors;
       ++line_index) {
    const int y = grid.top + line_index *
        simtower::kOriginalElevatorControlCellSize;
    MoveToEx(dc, grid.left, y, nullptr);
    LineTo(dc, grid.right - 1, y);
  }

  const auto old_alignment = SetTextAlign(dc, TA_LEFT | TA_TOP);
  const auto old_background_mode = SetBkMode(dc, TRANSPARENT);
  const auto old_text_color = GetTextColor(dc);
  const auto old_background_color = GetBkColor(dc);
  HFONT floor_font = make_original_dialog_font(13);
  HFONT small_floor_font = make_original_dialog_font(9);
  HGDIOBJ old_font = floor_font ? SelectObject(dc, floor_font) : nullptr;

  // Exact 1098:17c7/1895 fifteen-row label cells: above-shaft gray, served
  // floors black/white, unserved floors crossed, and three-digit font inset.
  for (std::int16_t row = 0;
       row < simtower::kOriginalElevatorControlVisibleFloors; ++row) {
    const auto cell_source =
        simtower::original_elevator_control_cell_rect(-1, row);
    RECT cell = original_native_rectangle(cell_source);
    const auto floor_plan =
        simtower::original_elevator_control_floor_cell_plan(
            elevator, context.state, row);
    const auto floor = floor_plan.floor;
    if (floor_plan.above_top) {
      HBRUSH gray = CreateSolidBrush(RGB(136, 136, 136));
      if (gray) {
        FillRect(dc, &cell, gray);
        DeleteObject(gray);
      }
      continue;
    }

    const bool serviced = floor_plan.serviced;
    FillRect(dc, &cell, serviced ? black : white);
    SetTextColor(dc, serviced ? RGB(255, 255, 255) : RGB(0, 0, 0));
    SetBkColor(dc, serviced ? RGB(0, 0, 0) : RGB(255, 255, 255));

    const auto label = original_cp1252_to_wide(floor_plan.label);
    if (floor_plan.small_font && small_floor_font) {
      SelectObject(dc, small_floor_font);
    } else if (floor_font) {
      SelectObject(dc, floor_font);
    }
    SIZE extent{};
    GetTextExtentPoint32W(dc, label.data(), static_cast<int>(label.size()),
                          &extent);
    const int x = cell.left +
                  ((cell.right - cell.left - extent.cx) / 2) +
                  floor_plan.horizontal_inset;
    const int y = cell.top + floor_plan.vertical_inset;
    TextOutW(dc, x, y, label.data(), static_cast<int>(label.size()));

    if (!serviced) {
      // Exact 1098:226e BLACK_PEN diagonals, ending at right-1/bottom-1.
      MoveToEx(dc, cell.left, cell.top, nullptr);
      LineTo(dc, cell.right - 1, cell.bottom - 1);
      MoveToEx(dc, cell.right - 1, cell.top, nullptr);
      LineTo(dc, cell.left, cell.bottom - 1);
    }
  }

  if (old_font) SelectObject(dc, old_font);
  SetTextColor(dc, old_text_color);
  SetBkColor(dc, old_background_color);
  SetBkMode(dc, old_background_mode);
  SetTextAlign(dc, old_alignment);

  std::int16_t visual_column = 0;
  for (std::size_t car_index = 0U;
       car_index < elevator.car_records.size(); ++car_index) {
    // Exact 1098:1f45/1f9d walk all eight raw car records, compact the active
    // columns, and call 1e33 for selected/unselected current-floor refreshes.
    // Painting every visible row here performs both traversals in one pass.
    if (elevator.car_records[car_index].exact_bytes[15] == std::byte{0}) {
      continue;
    }
    for (std::int16_t row = 0;
         row < simtower::kOriginalElevatorControlVisibleFloors; ++row) {
      const auto floor = simtower::original_elevator_control_visible_floor(
          elevator, context.state, row);
      const auto bitmap = simtower::original_elevator_control_car_bitmap(
          document, context.state.elevator_index, car_index, floor);
      if (bitmap == 0U) continue;
      const auto cell = simtower::original_elevator_control_cell_rect(
          visual_column, row);
      // 11e0:0430 clips the original 16x16 DIB to this 12x12 grid cell.
      simtower::draw_original_dib_region(
          dc, g_resources.find("BITMAP", bitmap), cell.left, cell.top,
          0, 0, cell.right - cell.left, cell.bottom - cell.top);
    }

    // 1098:1e33 frames the car's current floor, not its configured home.
    if (const auto frame =
            simtower::original_elevator_control_current_car_frame(
                document, context.state, car_index, visual_column)) {
      RECT cell = original_native_rectangle(*frame);
      FrameRect(dc, &cell, black);
    }
    ++visual_column;
  }

  if (pen) {
    SelectObject(dc, previous_pen);
    DeleteObject(pen);
  }
}

void paint_original_elevator_control(
    HWND dialog,
    HDC dc,
    const OriginalElevatorControlDialogContext& context) {
  // Native direct-DIB translation of the complete 1098:0068 painter and
  // 1098:12e9 full refresh orchestration. The original staged BITMAP/400/401
  // and small overlays through temporary Win16 DCs; rectangle selection,
  // invalidated regions, layer order, text, and grid are kept by one paint.
  simtower::draw_original_dib(
      dc, g_resources.find("BITMAP", 400), 0, 0);
  if (!g_tower_document || !context.state.valid ||
      context.state.elevator_index >= g_tower_document->elevators.size()) {
    return;
  }
  const auto& elevator =
      g_tower_document->elevators[context.state.elevator_index];

  if (context.state.isolation_active) {
    if (const auto* rectangle =
            original_elevator_control_rectangle(context, 2U)) {
      draw_original_elevator_control_region(dc, 410, *rectangle);
    }
  }
  // Exact native presentation boundary for 1098:13e4: after a schedule-bank
  // change, the Win16 build erased/invalidated these two DTMP button regions.
  // The direct-DIB painter redraws the selected BITMAP/401 region on demand.
  if (const auto* rectangle = original_elevator_control_rectangle(
          context, 12U + context.state.schedule_bank)) {
    draw_original_elevator_control_region(dc, 401, *rectangle);
  }
  if (context.pressed_item == 1) {
    if (const auto* rectangle =
            original_elevator_control_rectangle(context, 1U)) {
      draw_original_elevator_control_region(dc, 401, *rectangle);
    }
  } else if (context.pressed_item == 2) {
    if (const auto* rectangle =
            original_elevator_control_rectangle(context, 2U)) {
      draw_original_elevator_control_region(
          dc, context.state.isolation_active ? 411 : 401, *rectangle);
    }
  }

  // Exact 1098:1502 six-phase schedule buttons and 1098:1498 selected-phase
  // double frame.
  for (std::uint8_t phase = 0U; phase < 6U; ++phase) {
    if (const auto* rectangle =
            original_elevator_control_rectangle(context, 23U + phase)) {
      const auto mode = std::min<std::uint8_t>(
          2U, std::to_integer<std::uint8_t>(elevator.schedule[
                  simtower::original_elevator_control_schedule_index(
                      28U, context.state.schedule_bank, phase)]));
      simtower::draw_original_dib(
          dc, g_resources.find("BITMAP", 402U + mode),
          rectangle->left, rectangle->top);
    }
  }
  if (const auto* selected = original_elevator_control_rectangle(
          context, 23U + context.state.day_phase)) {
    RECT frame = original_native_rectangle(*selected);
    HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    FrameRect(dc, &frame, black);
    InflateRect(&frame, -1, -1);
    FrameRect(dc, &frame, black);
  }
  if (const auto* rectangle =
          original_elevator_control_rectangle(context, 5U)) {
    simtower::draw_original_dib(
        dc,
        g_resources.find(
            "BITMAP",
            simtower::original_elevator_control_show_bitmap(elevator)),
        rectangle->left, rectangle->top);
  }

  const auto old_alignment = SetTextAlign(dc, TA_LEFT | TA_TOP);
  const auto old_background_mode = SetBkMode(dc, TRANSPARENT);
  const auto old_text = SetTextColor(dc, RGB(0, 0, 0));
  const auto old_background = SetBkColor(dc, RGB(255, 255, 255));
  HFONT font = make_original_dialog_font(16);
  HGDIOBJ old_font = font ? SelectObject(dc, font) : nullptr;
  // Exact 1098:27bd/2893 Elevator-control DTMP 41 waiting count and DTMP 42
  // departure-delay text fields.
  const std::array values{
      std::pair<std::size_t, std::string>{
          41U, simtower::original_elevator_control_waiting_text(
                   elevator, context.state)},
      std::pair<std::size_t, std::string>{
          42U, simtower::original_elevator_control_departure_text(
                   elevator, context.state)},
  };
  for (const auto& [index, value] : values) {
    const auto* rectangle =
        original_elevator_control_rectangle(context, index);
    if (!rectangle) continue;
    const auto wide = original_cp1252_to_wide(value);
    TextOutW(dc, static_cast<int>(rectangle->left) + 2,
             static_cast<int>(rectangle->top) + 3,
             wide.data(), static_cast<int>(wide.size()));
  }
  if (font) {
    SelectObject(dc, old_font);
  }
  SetTextColor(dc, old_text);
  SetBkColor(dc, old_background);
  SetBkMode(dc, old_background_mode);
  SetTextAlign(dc, old_alignment);

  paint_original_elevator_control_grid(dc, context, *g_tower_document);
  (void)dialog;
}

int original_elevator_control_hit_item(
    const OriginalElevatorControlDialogContext& context,
    LPARAM position) {
  const int x = static_cast<std::int16_t>(LOWORD(position));
  const int y = static_cast<std::int16_t>(HIWORD(position));
  for (std::size_t item = 1U; item <= 28U; ++item) {
    const auto* rectangle =
        original_elevator_control_rectangle(context, item);
    if (rectangle && original_point_in_rectangle(*rectangle, x, y)) {
      return static_cast<int>(item);
    }
  }
  return -1;
}

void refresh_original_elevator_control_scrollbar(
    HWND dialog,
    const OriginalElevatorControlDialogContext& context) {
  HWND scrollbar = GetDlgItem(dialog, 7);
  if (!scrollbar) return;
  const bool enabled =
      simtower::original_elevator_control_has_scrollbar(context.state);
  EnableWindow(scrollbar, enabled);
  if (enabled) {
    SetScrollRange(scrollbar, SB_CTL, context.state.scroll_min,
                   context.state.scroll_max, FALSE);
    SetScrollPos(scrollbar, SB_CTL, context.state.scroll_position, TRUE);
  }
}

void mark_original_elevator_control_changed(HWND dialog,
                                            bool world_changed) {
  g_tower_dirty = true;
  InvalidateRect(dialog, nullptr, FALSE);
  if (world_changed) {
    invalidate_original_main_surface();
    InvalidateRect(g_info_window, nullptr, FALSE);
    InvalidateRect(g_map_window, nullptr, FALSE);
  }
}

struct OriginalElevatorPopupContext {
  std::uint8_t original_mode{};
  std::int16_t hover_mode{-1};
  std::size_t phase_item{};
  POINT phase_screen{};
};

void paint_original_elevator_popup(HDC dc,
                                   const OriginalElevatorPopupContext& context) {
  simtower::draw_original_dib(
      dc, g_resources.find("BITMAP", 405), 0, 0);
  if (context.hover_mode < 0) return;
  const auto highlight = simtower::original_elevator_control_popup_highlight(
      static_cast<std::uint8_t>(context.hover_mode),
      simtower::kOriginalElevatorControlPopupWidth,
      simtower::kOriginalElevatorControlPopupHeight);
  if (!highlight) return;
  const int top = static_cast<int>(highlight->top);
  simtower::draw_original_dib_region(
      dc, g_resources.find("BITMAP", 406), 0, top, 0, top,
      static_cast<int>(highlight->right - highlight->left),
      static_cast<int>(highlight->bottom - highlight->top));
}

// Native message boundary for exported ELVPOPUP at 1098:22f8.
INT_PTR CALLBACK elevator_popup_dialog_proc(HWND dialog, UINT message,
                                             WPARAM, LPARAM lparam) {
  if (message == WM_INITDIALOG) {
    auto* context =
        reinterpret_cast<OriginalElevatorPopupContext*>(lparam);
    SetWindowLongPtrW(dialog, DWLP_USER,
                      reinterpret_cast<LONG_PTR>(context));
    const int row_height =
        simtower::kOriginalElevatorControlPopupHeight / 3;
    size_original_dialog_client(
        dialog, simtower::kOriginalElevatorControlPopupWidth,
        simtower::kOriginalElevatorControlPopupHeight,
        context->phase_screen.x,
        context->phase_screen.y - context->original_mode * row_height);
    SetCapture(dialog);
    RECT client{};
    GetClientRect(dialog, &client);
    POINT upper_left{client.left, client.top};
    POINT lower_right{client.right, client.bottom};
    // 1070:06cd converts both corners of its popup client RECT to screen
    // coordinates before confining the pointer to the resulting rectangle.
    for (const auto corner :
         simtower::original_dialog_rect_screen_conversion_order()) {
      POINT* const point =
          corner == simtower::OriginalDialogRectScreenCorner::upper_left
              ? &upper_left
              : &lower_right;
      ClientToScreen(dialog, point);
    }
    RECT clip{upper_left.x, upper_left.y, lower_right.x, lower_right.y};
    ClipCursor(&clip);
    return TRUE;
  }

  auto* context = reinterpret_cast<OriginalElevatorPopupContext*>(
      GetWindowLongPtrW(dialog, DWLP_USER));
  switch (message) {
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(dialog, &paint);
      if (dc && context) paint_original_elevator_popup(dc, *context);
      EndPaint(dialog, &paint);
      return TRUE;
    }
    case WM_MOUSEMOVE:
      if (context) {
        const int x = static_cast<std::int16_t>(LOWORD(lparam));
        const int y = static_cast<std::int16_t>(HIWORD(lparam));
        const auto selection =
            simtower::original_elevator_control_popup_selection(
                x, y, simtower::kOriginalElevatorControlPopupWidth,
                simtower::kOriginalElevatorControlPopupHeight);
        // 1098:2675 shares its out-of-bounds leg with 26b2's commit/close
        // path. ClipCursor normally makes this rare, but synthetic motion and
        // capture transitions retain the original behavior.
        if (!selection) {
          ClipCursor(nullptr);
          ReleaseCapture();
          EndDialog(dialog, context->original_mode);
          return TRUE;
        }
        if (static_cast<std::int16_t>(*selection) != context->hover_mode) {
          context->hover_mode = static_cast<std::int16_t>(*selection);
          InvalidateRect(dialog, nullptr, FALSE);
          UpdateWindow(dialog);
        }
      }
      return TRUE;
    case WM_LBUTTONUP:
      if (context) {
        const int x = static_cast<std::int16_t>(LOWORD(lparam));
        const int y = static_cast<std::int16_t>(HIWORD(lparam));
        const auto selection =
            simtower::original_elevator_control_popup_selection(
                x, y, simtower::kOriginalElevatorControlPopupWidth,
                simtower::kOriginalElevatorControlPopupHeight);
        ClipCursor(nullptr);
        ReleaseCapture();
        EndDialog(dialog, selection.value_or(context->original_mode));
      }
      return TRUE;
    case WM_RBUTTONUP:
      ClipCursor(nullptr);
      ReleaseCapture();
      EndDialog(dialog, context ? context->original_mode : 0);
      return TRUE;
  }
  return FALSE;
}

std::uint8_t run_original_elevator_popup(
    HWND owner,
    const OriginalElevatorControlDialogContext& context,
    std::size_t phase_item) {
  if (!g_tower_document ||
      context.state.elevator_index >= g_tower_document->elevators.size()) {
    return 0U;
  }
  const auto* rectangle =
      original_elevator_control_rectangle(context, phase_item);
  if (!rectangle) return 0U;
  POINT phase_screen{static_cast<LONG>(rectangle->left),
                     static_cast<LONG>(rectangle->top)};
  ClientToScreen(owner, &phase_screen);
  const auto& elevator =
      g_tower_document->elevators[context.state.elevator_index];
  auto popup_state = context.state;
  popup_state.day_phase = static_cast<std::uint8_t>(phase_item - 23U);
  const auto mode = std::min<std::uint8_t>(
      2U, simtower::original_elevator_control_floor_mode(
              elevator, popup_state));
  // ELVPOPUP initializes DS:1fb2 to -1 and does not composite a highlight
  // until WM_MOUSEMOVE selects a band.
  OriginalElevatorPopupContext popup{mode, -1, phase_item, phase_screen};
  const auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", 124));
  const auto native = simtower::build_native_dialog_template(original);
  const int result = static_cast<int>(run_original_modal_dialog(
      g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
      owner, elevator_popup_dialog_proc,
      reinterpret_cast<LPARAM>(&popup)));
  return result >= 0 && result <= 2
             ? static_cast<std::uint8_t>(result)
             : mode;
}

void run_original_elevator_control_grid_action(
    HWND dialog,
    OriginalElevatorControlDialogContext& context,
    LPARAM position) {
  // Exact native boundary for 1098:1ff5: scan the 15 visible rows and eight
  // cars, apply the service-floor/car-home action, and repaint the affected
  // cell with the original scrolling coordinate transform.
  if (!g_tower_document) return;
  const int x = static_cast<std::int16_t>(LOWORD(position));
  const int y = static_cast<std::int16_t>(HIWORD(position));
  const auto hit = simtower::original_elevator_control_grid_hit(
      *g_tower_document, context.state, x, y);
  if (!hit.hit()) return;
  if (context.state.isolation_active) {
    const auto text = simtower::original_strl_entry(
        g_resources.find("STRL", 1004), 1U);
    (void)simtower::show_original_alert(
        dialog, g_resources, 1000, {text, {}, {}, {}});
    return;
  }

  bool changed = false;
  if (hit.kind ==
      simtower::OriginalElevatorControlGridKind::service_floor) {
    const auto elevator_index = context.state.elevator_index;
    const auto floor = hit.floor;
    const auto& elevator = g_tower_document->elevators[elevator_index];
    const bool serviced =
        floor >= 0 && floor < 120 &&
        elevator.serviced_floors[static_cast<std::size_t>(floor)] !=
            std::byte{0};
    if (!serviced) {
      changed = simtower::original_elevator_control_add_service_floor(
          *g_tower_document, elevator_index, floor);
    } else {
      const auto warning =
          simtower::original_elevator_service_floor_warning_code(
              *g_tower_document, elevator_index, floor);
      if (warning != 0U) {
        const auto text = simtower::original_strl_entry(
            g_resources.find("STRL", 1005), warning);
        if (simtower::show_original_alert(
                dialog, g_resources, 1005, {text, {}, {}, {}}) == 2) {
          return;
        }
      }
      const auto removal =
          simtower::original_elevator_control_remove_service_floor(
              *g_tower_document, elevator_index, floor, g_part,
              g_rent_income);
      changed = removal.cleanup.status ==
          simtower::OriginalElevatorFloorPeopleCleanupStatus::cleaned;
      if (changed) {
        consume_original_person_family_dispatches(
            removal.family_dispatches);
      }
    }
  } else if (hit.kind ==
             simtower::OriginalElevatorControlGridKind::car) {
    changed = simtower::original_elevator_control_set_car_home(
        *g_tower_document, context.state.elevator_index,
        static_cast<std::size_t>(hit.car_index), hit.floor);
  }
  if (changed) mark_original_elevator_control_changed(dialog, true);
}

void resume_original_elevator_control(
    HWND dialog,
    OriginalElevatorControlDialogContext& context) {
  if (!g_tower_document || !context.state.isolation_active) return;
  if (simtower::resume_original_elevator_control_isolation(
          *g_tower_document, context.state, g_build_mode_enabled)) {
    g_elevator_transfer_visuals.clear();
    InvalidateRect(dialog, nullptr, FALSE);
    invalidate_original_main_surface();
    InvalidateRect(g_map_window, nullptr, FALSE);
    InvalidateRect(g_info_window, nullptr, FALSE);
    InvalidateRect(g_command_window, nullptr, FALSE);
  }
}

void toggle_original_elevator_control_simulation(
    HWND dialog,
    OriginalElevatorControlDialogContext& context) {
  if (!g_tower_document) return;
  if (context.state.isolation_active) {
    resume_original_elevator_control(dialog, context);
    return;
  }
  if (!simtower::begin_original_elevator_control_isolation(
          *g_tower_document, context.state, g_build_mode_enabled)) {
    return;
  }
  const auto preview = simtower::prepare_original_elevator_control_preview(
      *g_tower_document, context.state, g_part, g_rent_income);
  if (!preview.prepared) {
    const auto text = simtower::original_strl_entry(
        g_resources.find("STRL", 1004), 2U);
    (void)simtower::show_original_alert(
        dialog, g_resources, 1000, {text, {}, {}, {}});
    resume_original_elevator_control(dialog, context);
    return;
  }
  for (std::size_t index = 0U;
       index < preview.movement_sound_requests; ++index) {
    if (g_audio) {
      (void)g_audio->play_resource(0x1772U, 0U, 0U, GetTickCount());
    }
  }
  // 10f0:0090-0097 calls 1080:0a1e(0) after masking the isolated Elevator.
  request_original_main_surface_pass(
      simtower::OriginalMainSurfacePass::rebuild_without_sky, true);
  InvalidateRect(dialog, nullptr, FALSE);
  InvalidateRect(g_command_window, nullptr, FALSE);
}

void close_original_elevator_control(
    HWND dialog,
    OriginalElevatorControlDialogContext& context) {
  const auto plan = simtower::original_elevator_control_close_plan();
  if (plan.resume_isolation) {
    resume_original_elevator_control(dialog, context);
  }
  if (plan.clear_published_window_before_destroy &&
      dialog == g_elevator_control_window) {
    g_elevator_control_window = nullptr;
  }
  for (const auto window : plan.enable_order) {
    HWND target = nullptr;
    switch (window) {
      case simtower::OriginalElevatorControlCloseWindow::command:
        target = g_command_window;
        break;
      case simtower::OriginalElevatorControlCloseWindow::map:
        target = g_map_window;
        break;
      case simtower::OriginalElevatorControlCloseWindow::info:
        target = g_info_window;
        break;
      case simtower::OriginalElevatorControlCloseWindow::main:
        target = g_main_window;
        break;
    }
    EnableWindow(target, TRUE);
  }
  if (plan.explicitly_activate_main && g_main_window) {
    SetActiveWindow(g_main_window);
  }
  DestroyWindow(dialog);
}

// Native message boundary for exported ELVDLOGMAIN at 1098:0628.
INT_PTR CALLBACK elevator_control_dialog_proc(HWND dialog, UINT message,
                                               WPARAM wparam,
                                               LPARAM lparam) {
  if (message == WM_INITDIALOG) {
    auto* context = reinterpret_cast<OriginalElevatorControlDialogContext*>(
        lparam);
    SetWindowLongPtrW(dialog, DWLP_USER,
                      reinterpret_cast<LONG_PTR>(context));
    EnableWindow(g_main_window, FALSE);
    EnableWindow(g_map_window, FALSE);
    EnableWindow(g_info_window, FALSE);
    EnableWindow(g_command_window, FALSE);
    RECT current{};
    GetWindowRect(dialog, &current);
    size_original_dialog_client(
        dialog, simtower::kOriginalElevatorControlClientWidth,
        simtower::kOriginalElevatorControlClientHeight,
        current.left, current.top);
    position_original_dialog(dialog, context->requested_left);
    simtower::render_original_dtmp(
        dialog, nullptr, context->dtmp, g_resources);
    refresh_original_elevator_control_scrollbar(dialog, *context);
    if (g_tower_document) {
      const auto title = original_cp1252_to_wide(
          simtower::original_elevator_control_title(
              g_resources, *g_tower_document,
              context->state.elevator_index));
      SetWindowTextW(dialog, title.c_str());
    }
    SetClassLongPtrW(dialog, GCLP_HCURSOR,
                     reinterpret_cast<LONG_PTR>(
                         LoadCursorW(nullptr, IDC_ARROW)));
    SetFocus(dialog);
    return TRUE;
  }

  auto* context = reinterpret_cast<OriginalElevatorControlDialogContext*>(
      GetWindowLongPtrW(dialog, DWLP_USER));
  switch (message) {
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(dialog, &paint);
      if (dc && context) {
        paint_original_elevator_control(dialog, dc, *context);
      }
      EndPaint(dialog, &paint);
      return TRUE;
    }
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORSCROLLBAR: {
      SetBkMode(reinterpret_cast<HDC>(wparam), TRANSPARENT);
      const auto brush = simtower::original_elevator_control_stock_brush(
          message == WM_CTLCOLORSCROLLBAR);
      return reinterpret_cast<INT_PTR>(
          GetStockObject(static_cast<int>(brush)));
    }
    case WM_ACTIVATE: {
      const bool active = LOWORD(wparam) != WA_INACTIVE;
      const HWND first_child = active ? nullptr : GetTopWindow(dialog);
      const auto plan = simtower::original_elevator_control_activation_plan(
          active, first_child != nullptr);
      // 1098:08c9 writes the same DS:31a6 word used by MAINWNDPROC and the
      // 1258 idle audio/z-order gates. Keeping it set while Elevator Control
      // owns focus prevents spurious game-audio deactivation.
      g_original_window_activation_latch = plan.shared_activation_latch;
      HWND insert_after = g_main_window;
      if (plan.insert_after ==
          simtower::OriginalElevatorControlActivationInsertAfter::topmost) {
        insert_after = HWND_TOPMOST;
      } else if (
          plan.insert_after ==
          simtower::OriginalElevatorControlActivationInsertAfter::
              first_child) {
        insert_after = first_child;
      }
      UINT flags = SWP_NOSIZE | SWP_NOMOVE;
      if (plan.no_activate) flags |= SWP_NOACTIVATE;
      SetWindowPos(dialog, insert_after, 0, 0, 0, 0, flags);
      return TRUE;
    }
    case WM_VSCROLL:
      if (context) {
        std::optional<simtower::OriginalElevatorControlScrollCommand>
            command;
        switch (LOWORD(wparam)) {
          case SB_LINEUP:
            command = simtower::OriginalElevatorControlScrollCommand::line_up;
            break;
          case SB_LINEDOWN:
            command = simtower::OriginalElevatorControlScrollCommand::line_down;
            break;
          case SB_PAGEUP:
            command = simtower::OriginalElevatorControlScrollCommand::page_up;
            break;
          case SB_PAGEDOWN:
            command = simtower::OriginalElevatorControlScrollCommand::page_down;
            break;
          case SB_THUMBPOSITION:
            command =
                simtower::OriginalElevatorControlScrollCommand::thumb_position;
            break;
          case SB_THUMBTRACK:
            command =
                simtower::OriginalElevatorControlScrollCommand::thumb_track;
            break;
        }
        if (command && simtower::original_elevator_control_scroll(
                           context->state, *command,
                           static_cast<std::int16_t>(HIWORD(wparam)))) {
          SetScrollPos(GetDlgItem(dialog, 7), SB_CTL,
                       context->state.scroll_position, TRUE);
          InvalidateRect(dialog, nullptr, FALSE);
          UpdateWindow(dialog);
        }
      }
      return TRUE;
    case WM_MOUSEMOVE:
      if (context && context->pressed_item >= 1 &&
          context->pressed_item <= 2) {
        InvalidateRect(dialog, nullptr, FALSE);
        UpdateWindow(dialog);
      }
      return TRUE;
    case WM_LBUTTONDOWN:
      if (context && g_tower_document) {
        const int item = original_elevator_control_hit_item(*context, lparam);
        context->pressed_item = item;
        bool changed = false;
        if (item == 8) {
          changed = simtower::original_elevator_control_adjust_waiting(
              *g_tower_document, context->state, 1);
        } else if (item == 9) {
          changed = simtower::original_elevator_control_adjust_waiting(
              *g_tower_document, context->state, -1);
        } else if (item == 10) {
          changed = simtower::original_elevator_control_adjust_departure(
              *g_tower_document, context->state, 1);
        } else if (item == 11) {
          changed = simtower::original_elevator_control_adjust_departure(
              *g_tower_document, context->state, -1);
        } else if (item >= 23 && item <= 28) {
          const auto mode = run_original_elevator_popup(
              dialog, *context, static_cast<std::size_t>(item));
          changed = simtower::original_elevator_control_select_phase(
                        context->state,
                        static_cast<std::uint8_t>(item - 23)) ||
                    changed;
          changed = simtower::original_elevator_control_set_floor_mode(
                        *g_tower_document, context->state, mode) ||
                    changed;
        }
        if (changed) {
          mark_original_elevator_control_changed(dialog, false);
        } else {
          InvalidateRect(dialog, nullptr, FALSE);
        }
        UpdateWindow(dialog);
      }
      return TRUE;
    case WM_LBUTTONUP:
      if (context && g_tower_document) {
        const int item = original_elevator_control_hit_item(*context, lparam);
        const int pressed = context->pressed_item;
        context->pressed_item = -1;
        InvalidateRect(dialog, nullptr, FALSE);
        UpdateWindow(dialog);
        if (item != pressed) return TRUE;
        if (item == 1) {
          close_original_elevator_control(dialog, *context);
          return TRUE;
        }
        if (item == 2) {
          toggle_original_elevator_control_simulation(dialog, *context);
          return TRUE;
        }
        if (item == 5 &&
            simtower::original_elevator_control_toggle_show(
                *g_tower_document, context->state.elevator_index)) {
          mark_original_elevator_control_changed(dialog, true);
          return TRUE;
        }
        if (item == 6) {
          run_original_elevator_control_grid_action(
              dialog, *context, lparam);
          return TRUE;
        }
        if ((item == 12 || item == 13) &&
            simtower::original_elevator_control_select_bank(
                context->state,
                static_cast<std::uint8_t>(item - 12))) {
          // 1098:2215 invalidates all fifteen schedule rows, then redraws the
          // grid and phase bands. A complete native invalidation reaches the
          // same 1098:16a4/1a5b paint paths in the following UpdateWindow.
          InvalidateRect(dialog, nullptr, FALSE);
          UpdateWindow(dialog);
          return TRUE;
        }
      }
      return TRUE;
  }
  return FALSE;
}

void open_original_elevator_control(std::size_t elevator_index,
                                    std::int16_t pointer_x,
                                    std::int16_t pointer_y) {
  // Exact 1098:0000 captures the elevator index, instantiates ELVDLOGMAIN,
  // and creates modeless DIALOG/400. The native template is translated from
  // that same embedded dialog resource before CreateDialogIndirectParamW.
  if (!g_tower_document || g_elevator_control_window) return;
  const auto launcher = simtower::original_elevator_control_launch_contract(
      elevator_index, pointer_x, pointer_y);
  auto context = std::make_unique<OriginalElevatorControlDialogContext>();
  context->dtmp = simtower::parse_original_dtmp(
      g_resources.find("DTMP", launcher.dialog_resource_id));
  context->state = simtower::make_original_elevator_control_state(
      *g_tower_document, launcher.elevator_index,
      g_simulation_state.calendar_phase,
      g_simulation_state.day_phase);
  if (!context->state.valid) return;
  context->initial_pointer_x = launcher.initial_pointer_x;
  context->initial_pointer_y = launcher.initial_pointer_y;
  context->requested_left = launcher.requested_left;
  const auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", launcher.dialog_resource_id));
  const auto native = simtower::build_native_dialog_template(original);
  HWND dialog = CreateDialogIndirectParamW(
      g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
      launcher.ownerless ? nullptr : g_main_window,
      elevator_control_dialog_proc,
      reinterpret_cast<LPARAM>(context.get()));
  if (!dialog) return;
  g_elevator_control_context = std::move(context);
  g_elevator_control_window = dialog;
}

struct OriginalPersonDialogContext {
  simtower::OriginalDtmp dtmp{};
  std::size_t person_index{};
  simtower::OriginalPersonInformationContext source_context{
      simtower::OriginalPersonInformationContext::main_world};
  simtower::OriginalPersonInformation information{};
  bool changed{};
  HBRUSH control_background{};
  HFONT control_font{};
};

struct OriginalPersonRenameDialogContext {
  simtower::OriginalDtmp dtmp{};
  std::size_t person_index{};
  bool changed{};
  HBRUSH control_background{};
};

const simtower::OriginalDtmpRect* original_person_rectangle(
    const OriginalPersonDialogContext& context,
    std::size_t one_based_index) {
  if (one_based_index == 0U ||
      one_based_index > context.dtmp.rectangles.size()) {
    return nullptr;
  }
  return &context.dtmp.rectangles[one_based_index - 1U];
}

void paint_original_person_text(HDC dc,
                                const simtower::OriginalDtmpRect* rectangle,
                                int offset_x,
                                int offset_y,
                                std::string_view text) {
  if (!rectangle || rectangle->left == 0 || rectangle->bottom == 0 ||
      text.empty()) {
    return;
  }
  const auto wide = original_cp1252_to_wide(text);
  MoveToEx(dc, static_cast<int>(rectangle->left) + offset_x,
           static_cast<int>(rectangle->top) + offset_y, nullptr);
  TextOutW(dc, 0, 0, wide.data(), static_cast<int>(wide.size()));
}

RECT original_person_meter_inner(
    const simtower::OriginalDtmpRect& rectangle) {
  RECT result{rectangle.left, rectangle.top,
              rectangle.right, rectangle.bottom};
  InflateRect(&result, -2, -2);
  return result;
}

void paint_original_person_meter(
    HDC dc,
    const simtower::OriginalDtmpRect* source,
    const simtower::OriginalInformationMeter& meter,
    bool reverse,
    bool thresholds) {
  // Native presentation of 1100:1fad and shared 11e0:02bd: deflate by two,
  // retain signed/truncating reverse-threshold geometry, classify the value,
  // paint against maximum 300, and draw both vertical threshold ticks.
  if (!source || source->left == 0 || source->bottom == 0 ||
      !meter.visible) {
    return;
  }
  RECT fill = original_person_meter_inner(*source);
  if (IsRectEmpty(&fill)) return;
  fill.right = simtower::original_information_meter_fill_right(
      fill.left, fill.right, meter.value, meter.maximum, reverse);
  if (fill.right > fill.left) {
    const COLORREF color = static_cast<COLORREF>(
        simtower::original_information_meter_colorref(meter.band));
    // 11e0:025f-0299 and 03d2-040c do not pass a raw RGB color to the
    // brush. They resolve it against DS:795e first and pass PALETTEINDEX,
    // preserving the selected logical palette on indexed displays.
    const COLORREF brush_color = g_logical_palette
        ? PALETTEINDEX(GetNearestPaletteIndex(g_logical_palette, color))
        : color;
    HBRUSH brush = CreateSolidBrush(brush_color);
    if (brush) {
      FillRect(dc, &fill, brush);
      DeleteObject(brush);
    }
  }
  if (!thresholds) return;
  const RECT inner = original_person_meter_inner(*source);
  HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
  HGDIOBJ old_pen = pen ? SelectObject(dc, pen) : nullptr;
  for (const auto threshold : {meter.lower, meter.upper}) {
    const int x = simtower::original_information_meter_fill_right(
        inner.left, inner.right, threshold, meter.maximum, true);
    MoveToEx(dc, x, inner.top, nullptr);
    LineTo(dc, x, inner.bottom - 1);
  }
  if (pen) {
    SelectObject(dc, old_pen);
    DeleteObject(pen);
  }
}

void paint_original_person_dialog(HWND dialog,
                                  HDC dc,
                                  const OriginalPersonDialogContext& context) {
  simtower::paint_original_dialog_chrome(dialog, dc, context.dtmp);
  if (!context.information.valid) return;

  const auto* portrait = original_person_rectangle(context, 2U);
  if (portrait && portrait->left != 0 && portrait->bottom != 0 &&
      context.information.portrait_frame >= 0) {
    int bitmap_id = 700;
    if (context.information.portrait_variant ==
        simtower::OriginalPersonPortraitVariant::named) {
      bitmap_id = 702;
    } else if (context.information.portrait_variant ==
               simtower::OriginalPersonPortraitVariant::vip) {
      bitmap_id = 703;
    }
    const int source_width = context.information.portrait_frame >= 6 ? 16 : 8;
    int destination_x = static_cast<int>(portrait->left) + 20;
    if (source_width == 16) destination_x -= 8;
    simtower::draw_original_dib_region_scaled(
        dc, g_resources.find("BITMAP", bitmap_id),
        destination_x, static_cast<int>(portrait->top) + 2,
        source_width * 2, 47,
        context.information.portrait_frame * 8, 0,
        source_width, 24);
  }

  const auto old_align = SetTextAlign(dc, TA_UPDATECP);
  const auto old_background = SetBkMode(dc, TRANSPARENT);
  HFONT temporary_font = nullptr;
  HFONT font = context.control_font;
  if (!font) {
    temporary_font = make_original_dialog_font(13);
    font = temporary_font;
  }
  HGDIOBJ old_font = font ? SelectObject(dc, font) : nullptr;
  paint_original_person_text(
      dc, original_person_rectangle(context, 3U), 4, 4,
      context.information.display_name);
  paint_original_person_text(
      dc, original_person_rectangle(context, 5U), 4, 3,
      context.information.origin_text);
  paint_original_person_text(
      dc, original_person_rectangle(context, 7U), 4, 3,
      context.information.activity_text);
  if (font) {
    SelectObject(dc, old_font);
  }
  SetBkMode(dc, old_background);
  SetTextAlign(dc, old_align);

  paint_original_person_meter(
      dc, original_person_rectangle(context, 6U),
      context.information.evaluation, true, true);
  paint_original_person_meter(
      dc, original_person_rectangle(context, 8U),
      context.information.stress, false, false);
}

void enable_original_person_rename_button(HWND dialog) {
  // Exact 1100:43ed edit-item-4 nonempty gate for the dialog's OK button.
  EnableWindow(GetDlgItem(dialog, 1),
               simtower::original_rename_ok_enabled(
                   static_cast<std::size_t>(
                       GetWindowTextLengthW(GetDlgItem(dialog, 4)))));
}

void paint_original_rename_dialog(HWND dialog,
                                  HDC dc,
                                  const simtower::OriginalDtmp& dtmp,
                                  bool render_dtmp) {
  // NAMEPEPLEDIALOGFILTER 1100:3b3e-3b8a/3c10-3c5b and
  // NAMETENANTDIALOGFILTER 1100:3eb5-3f01/3f87-3fd2 use this same
  // palette-realized, transparent, TA_UPDATECP presentation boundary.
  if (g_logical_palette) {
    SelectPalette(dc, g_logical_palette, FALSE);
    RealizePalette(dc);
  }
  const UINT previous_alignment = SetTextAlign(dc, TA_UPDATECP);
  const int previous_background = SetBkMode(dc, TRANSPARENT);
  if (render_dtmp) {
    simtower::render_original_dtmp(dialog, dc, dtmp, g_resources);
  } else {
    simtower::paint_original_dialog_chrome(dialog, dc, dtmp);
  }
  SetBkMode(dc, previous_background);
  SetTextAlign(dc, previous_alignment);
}

BOOL original_set_dialog_item_text(HWND dialog,
                                   int item,
                                   const wchar_t* text) {
  constexpr auto contract = simtower::original_dialog_item_text_contract();
  return contract.set_forwards_text
             ? SetDlgItemTextW(dialog, item, text)
             : FALSE;
}

std::wstring original_get_dialog_item_text(HWND dialog, int item) {
  constexpr auto contract = simtower::original_dialog_item_text_contract();
  std::array<wchar_t, contract.get_buffer_characters> text{};
  const UINT length = GetDlgItemTextW(
      dialog, item, text.data(), static_cast<int>(text.size()));
  return std::wstring(text.data(), static_cast<std::size_t>(length));
}

// Native message boundary for NAMEPEPLEDIALOGFILTER at 1100:3a39.
INT_PTR CALLBACK person_rename_dialog_proc(HWND dialog, UINT message,
                                           WPARAM wparam, LPARAM lparam) {
  // The original filter's shared 11e0:0026/0000 wrappers are respectively
  // SetDlgItemText and GetDlgItemText capped at 0xfe bytes. Wide native text
  // is converted through the same CP1252 persistence boundary below.
  if (message == WM_INITDIALOG) {
    auto* context = reinterpret_cast<OriginalPersonRenameDialogContext*>(
        lparam);
    SetWindowLongPtrW(dialog, DWLP_USER,
                      reinterpret_cast<LONG_PTR>(context));
    simtower::configure_original_dtmp_window(dialog, context->dtmp,
                                             g_resources,
                                             g_logical_palette);
    position_original_dialog(dialog);
    // NAMEPEPLEDIALOGFILTER captures input, clears Win16 GCL_HCURSOR, and
    // selects the system arrow during its 1100:3a69 initialization path.
    SetCapture(dialog);
    SetClassLongPtrW(dialog, GCLP_HCURSOR, 0);
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    COLORREF background = RGB(0xcc, 0xcc, 0xcc);
    if (g_logical_palette) {
      background = PALETTEINDEX(
          GetNearestPaletteIndex(g_logical_palette, background));
    }
    context->control_background = CreateSolidBrush(background);
    const auto name = original_cp1252_to_wide(
        simtower::original_person_saved_name(
            *g_tower_document, context->person_index));
    original_set_dialog_item_text(dialog, 4, name.c_str());
    EnableWindow(GetDlgItem(dialog, 3), !name.empty());
    enable_original_person_rename_button(dialog);
    if (HDC dc = GetDC(dialog)) {
      paint_original_rename_dialog(dialog, dc, context->dtmp, true);
      ReleaseDC(dialog, dc);
    }
    const auto focus = simtower::original_rename_dialog_focus_plan(
        simtower::OriginalRenameDialogFocusPhase::initialize);
    if (focus.focus_edit) SetFocus(GetDlgItem(dialog, 4));
    if (focus.select_all) SendDlgItemMessageW(dialog, 4, EM_SETSEL, 0U, -1);
    return focus.handled_result ? TRUE : FALSE;
  }

  auto* context = reinterpret_cast<OriginalPersonRenameDialogContext*>(
      GetWindowLongPtrW(dialog, DWLP_USER));
  switch (message) {
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(dialog, &paint);
      if (dc && context) {
        paint_original_rename_dialog(dialog, dc, context->dtmp, false);
      }
      EndPaint(dialog, &paint);
      // 1100:3c60-3c7a re-enables and focuses edit item 4 after every paint.
      EnableWindow(GetDlgItem(dialog, 4), TRUE);
      const auto focus = simtower::original_rename_dialog_focus_plan(
          simtower::OriginalRenameDialogFocusPhase::after_paint);
      if (focus.focus_edit) SetFocus(GetDlgItem(dialog, 4));
      if (focus.select_all) {
        SendDlgItemMessageW(dialog, 4, EM_SETSEL, 0U, -1);
      }
      return focus.handled_result ? TRUE : FALSE;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
      HDC dc = reinterpret_cast<HDC>(wparam);
      SetBkMode(dc, TRANSPARENT);
      if (g_logical_palette) {
        SelectPalette(dc, g_logical_palette, FALSE);
        RealizePalette(dc);
      }
      return reinterpret_cast<INT_PTR>(
          context && context->control_background
              ? context->control_background
              : GetStockObject(NULL_BRUSH));
    }
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
      return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
    case WM_COMMAND: {
      if (!context) break;
      using CommandAction = simtower::OriginalRenameDialogCommandAction;
      const auto command = simtower::original_rename_dialog_command_plan(
          static_cast<std::uint16_t>(LOWORD(wparam)));
      const auto close_dialog = [&]() {
        // NAMEPEPLEDIALOGFILTER 1100:3ce8-3d0f shares this exact tail across
        // Save, Cancel, and Remove.
        // 1070:051f releases the HWND-owned DTMP before the brush/capture and
        // EndDialog operations.
        context->dtmp = {};
        if (context->control_background) {
          DeleteObject(context->control_background);
          context->control_background = nullptr;
        }
        g_original_active_modal_window = nullptr;
        if (GetCapture() == dialog) ReleaseCapture();
        EndDialog(dialog, command.close_result);
      };
      switch (command.action) {
        case CommandAction::refresh_edit_gate:
          enable_original_person_rename_button(dialog);
          return TRUE;
        case CommandAction::save:
          if (!g_tower_document) return command.consume ? TRUE : FALSE;
          {
            const auto wide = original_get_dialog_item_text(dialog, 4);
            if (wide.empty()) return TRUE;
            const auto name = original_wide_to_cp1252(wide);
            const auto result = simtower::set_original_person_name(
                *g_tower_document, context->person_index, name);
            if (result.status ==
                simtower::OriginalPersonNameStatus::too_long) {
              const auto text = simtower::original_strl_entry(
                  g_resources.find("STRL", 1005), 4U);
              (void)simtower::show_original_alert(
                  dialog, g_resources, 1000, {text, {}, {}, {}});
              return TRUE;
            }
            if (result.status == simtower::OriginalPersonNameStatus::full) {
              const auto text = simtower::original_strl_entry(
                  g_resources.find("STRL", 1005), 6U);
              (void)simtower::show_original_alert(
                  dialog, g_resources, 1000, {text, {}, {}, {}});
            }
            context->changed = context->changed || result.changed;
          }
          close_dialog();
          return TRUE;
        case CommandAction::cancel:
          close_dialog();
          return TRUE;
        case CommandAction::remove:
          if (g_tower_document) {
            const auto result = simtower::remove_original_person_name(
                *g_tower_document, context->person_index);
            context->changed = context->changed || result.changed;
          }
          close_dialog();
          return TRUE;
        case CommandAction::none:
          return command.consume ? TRUE : FALSE;
      }
      break;
    }
  }
  return FALSE;
}

bool run_original_person_rename_dialog(std::size_t person_index) {
  // Native DIALOG/730 launcher corresponding to 1100:39df.
  const auto launcher = simtower::original_information_launcher_contract(
      simtower::OriginalInformationLauncherKind::person_rename);
  const auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", launcher.dialog_resource_id));
  const auto native = simtower::build_native_dialog_template(original);
  OriginalPersonRenameDialogContext context{
      simtower::parse_original_dtmp(
          g_resources.find("DTMP", launcher.dialog_resource_id)),
      person_index};
  (void)run_original_modal_dialog(
      g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
      launcher.main_window_owner ? g_main_window : nullptr,
      person_rename_dialog_proc,
      reinterpret_cast<LPARAM>(&context));
  // Recognized commands already mirror 1100:3ce8-3d0f and clear this handle;
  // retain only a host-side fallback for a failed/externally ended modal loop.
  release_native_dialog_brush(context.control_background);
  return context.changed;
}

// Native message boundary for PEPLEINFODLOGFILTER at 1100:0116.
INT_PTR CALLBACK person_information_dialog_proc(HWND dialog, UINT message,
                                                WPARAM wparam,
                                                LPARAM lparam) {
  if (message == WM_INITDIALOG) {
    auto* context = reinterpret_cast<OriginalPersonDialogContext*>(lparam);
    SetWindowLongPtrW(dialog, DWLP_USER,
                      reinterpret_cast<LONG_PTR>(context));
    const auto capture_plan =
        simtower::original_person_information_capture_plan();
    if (capture_plan.capture_on_initialization) SetCapture(dialog);
    simtower::configure_original_dtmp_window(dialog, context->dtmp,
                                             g_resources,
                                             g_logical_palette);
    simtower::render_original_dtmp(dialog, nullptr, context->dtmp,
                                   g_resources);
    // PEPLEINFODLOGFILTER 1100:0168-017b clears Win16 GCL_HCURSOR and then
    // selects the stock arrow as the current pointer.
    SetClassLongPtrW(dialog, GCLP_HCURSOR, 0);
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    context->control_font = make_original_dialog_font(13);
    position_original_dialog(dialog);
    if (HDC dc = GetDC(dialog)) {
      // 1100:0189-01e5 performs a palette-realized immediate presentation
      // with the same 13-pixel font before the modal loop begins.
      if (g_logical_palette) {
        SelectPalette(dc, g_logical_palette, FALSE);
        RealizePalette(dc);
      }
      paint_original_person_dialog(dialog, dc, *context);
      ReleaseDC(dialog, dc);
    }
    const auto focus_plan =
        simtower::original_painted_dialog_initialization_focus_plan();
    if (focus_plan.set_explicit_focus) {
      SetFocus(GetDlgItem(dialog, 1));
    }
    return focus_plan.consume ? TRUE : FALSE;
  }

  auto* context = reinterpret_cast<OriginalPersonDialogContext*>(
      GetWindowLongPtrW(dialog, DWLP_USER));
  switch (message) {
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(dialog, &paint);
      if (dc && context) {
        if (g_logical_palette) {
          // Exact paint-time SelectPalette/RealizePalette at 1100:028d-029e.
          SelectPalette(dc, g_logical_palette, FALSE);
          RealizePalette(dc);
        }
        paint_original_person_dialog(dialog, dc, *context);
      }
      EndPaint(dialog, &paint);
      return TRUE;
    }
    case WM_ACTIVATE:
      if (const auto plan = simtower::original_information_activation_plan(
              LOWORD(wparam) != WA_INACTIVE,
              g_original_active_modal_window != nullptr,
              g_original_active_modal_window == dialog);
          plan.activate_nested_modal) {
        // 1100:01ed-0208 redirects activation to a currently nested modal.
        SetActiveWindow(g_original_active_modal_window);
      }
      return TRUE;
    case WM_CTLCOLORSTATIC: {
      HDC dc = reinterpret_cast<HDC>(wparam);
      if (context && context->control_font) {
        SelectObject(dc, context->control_font);
      }
      if (g_logical_palette) {
        SelectPalette(dc, g_logical_palette, FALSE);
        RealizePalette(dc);
      }
      if (context && !context->control_background &&
          simtower::original_facility_control_background(true) ==
              simtower::OriginalFacilityControlBackground::gray_cc) {
        COLORREF color = RGB(0xcc, 0xcc, 0xcc);
        if (g_logical_palette) {
          color = PALETTEINDEX(
              GetNearestPaletteIndex(g_logical_palette, color));
        }
        context->control_background = CreateSolidBrush(color);
      }
      // PEPLEINFODLOGFILTER 1100:0225-026a changes the static background mode
      // only after palette selection/realization and brush preparation.
      SetBkMode(dc, TRANSPARENT);
      return reinterpret_cast<INT_PTR>(
          context && context->control_background
              ? context->control_background
              : GetStockObject(NULL_BRUSH));
    }
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG: {
      HDC dc = reinterpret_cast<HDC>(wparam);
      if (context && context->control_font) {
        SelectObject(dc, context->control_font);
      }
      return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
    }
    case WM_COMMAND:
      if (!context) break;
      if (LOWORD(wparam) == 1U) {
        g_original_active_modal_window = nullptr;
        if (simtower::original_person_information_capture_plan()
                .release_before_close &&
            GetCapture() == dialog) {
          ReleaseCapture();
        }
        EndDialog(dialog, 1);
        return TRUE;
      }
      if (LOWORD(wparam) == 4U && g_tower_document) {
        // 1100:032e clears DS:31a4 while Rename Person owns the modal slot,
        // then 0344 republishes the Person panel after the child returns.
        g_original_active_modal_window = nullptr;
        const bool renamed = run_original_person_rename_dialog(
            context->person_index);
        g_original_active_modal_window = dialog;
        // 1100:0344-0359 always restores the Person panel as a visible
        // topmost window after the nested Rename dialog returns.
        SetWindowPos(dialog, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
        if (renamed) {
          context->changed = true;
          g_tower_dirty = true;
          update_original_tower_title(g_main_window);
        }
        context->information = simtower::original_person_information(
            g_resources, *g_tower_document, g_part,
            context->person_index, context->source_context);
        InvalidateRect(dialog, nullptr, FALSE);
        UpdateWindow(dialog);
        return TRUE;
      }
      break;
  }
  return FALSE;
}

int run_original_person_information_dialog(HWND owner,
                                           std::size_t person_index,
                                           bool* changed,
                                           simtower::OriginalPersonInformationContext source_context) {
  if (!g_tower_document) return 0;
  auto information = simtower::original_person_information(
      g_resources, *g_tower_document, g_part, person_index, source_context);
  if (!information.valid) return 0;
  const auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", information.dialog_id));
  const auto native = simtower::build_native_dialog_template(original);
  OriginalPersonDialogContext context{
      simtower::parse_original_dtmp(
          g_resources.find("DTMP", information.dialog_id)),
      person_index, source_context, std::move(information)};
  const int result = static_cast<int>(run_original_modal_dialog(
      g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
      owner, person_information_dialog_proc,
      reinterpret_cast<LPARAM>(&context)));
  release_native_dialog_brush(context.control_background);
  // 1100:00f3-0107 rechecks 10e0:0cc9 after DialogBox returns. Only an active
  // Find marker for this same person sets DS:77c0; 1058:019f consumes it after
  // the main-world magnifier dispatch returns.
  if (simtower::original_person_information_sets_find_exit_latch(
          g_find_marker, person_index)) {
    g_find_marker.phase = 1U;
  }
  if (changed) *changed = context.changed;
  return result;
}

struct OriginalFacilityDialogContext {
  simtower::OriginalDtmp dtmp{};
  std::int16_t floor{-1};
  std::size_t tenant_index{};
  simtower::OriginalFacilityInformation information{};
  simtower::OriginalWorldRaster preview_raster{};
  bool changed{};
  HBRUSH static_background{};
  HFONT control_font{};
};

struct OriginalTenantRenameDialogContext {
  simtower::OriginalDtmp dtmp{};
  std::int16_t floor{-1};
  std::size_t tenant_index{};
  bool changed{};
  HBRUSH control_background{};
};

struct OriginalMovieChoiceDialogContext {
  simtower::OriginalDtmp dtmp{};
  std::size_t linked_record_index{};
  bool changed{};
  HBRUSH static_background{};
  HFONT control_font{};
};

const simtower::OriginalDtmpRect* original_facility_rectangle(
    const OriginalFacilityDialogContext& context,
    std::size_t one_based_index) {
  if (one_based_index == 0U ||
      one_based_index > context.dtmp.rectangles.size()) {
    return nullptr;
  }
  return &context.dtmp.rectangles[one_based_index - 1U];
}

void draw_original_facility_preview(
    HDC dc,
    const OriginalFacilityDialogContext& context) {
  // Complete native boundary for 1100:4439: derive the 4869 world crop, use
  // DTMP item 2 as the container, fill it with 0xcccccc, then apply 4d1d's
  // scale-and-center geometry. Direct world rasterization replaces the
  // original temporary WinG surface and WinGStretchBlt presentation step.
  const auto* source = original_facility_rectangle(context, 2U);
  if (!source || !context.information.preview.valid()) {
    return;
  }
  const RECT container{source->left, source->top, source->right,
                       source->bottom};
  simtower::draw_original_facility_preview(
      dc, context.preview_raster, context.information.preview, container);
}

void paint_original_facility_dialog(
    HWND dialog,
    HDC dc,
    const OriginalFacilityDialogContext& context) {
  simtower::paint_original_dialog_chrome(dialog, dc, context.dtmp);
  if (!context.information.valid) return;
  draw_original_facility_preview(dc, context);

  paint_original_person_meter(
      dc, original_facility_rectangle(context, 6U),
      context.information.evaluation, true, true);
  paint_original_person_meter(
      dc, original_facility_rectangle(context, 6U),
      context.information.commercial_meter, false, false);

  for (const auto& sprite : context.information.person_sprites) {
    simtower::draw_original_dib_region_scaled(
        dc, g_resources.find("BITMAP", sprite.bitmap_id),
        sprite.destination_x, sprite.destination_y,
        sprite.width, sprite.height,
        sprite.frame * 8, 0, sprite.width, 24);
  }

  const auto old_align = SetTextAlign(dc, TA_UPDATECP);
  const auto old_background = SetBkMode(dc, TRANSPARENT);
  HFONT temporary_font = nullptr;
  HFONT font = context.control_font;
  if (!font) {
    temporary_font = make_original_dialog_font(13);
    font = temporary_font;
  }
  HGDIOBJ old_font = font ? SelectObject(dc, font) : nullptr;
  paint_original_person_text(
      dc, original_facility_rectangle(context, 3U), 4, 4,
      context.information.display_name);
  paint_original_person_text(
      dc, original_facility_rectangle(context, 5U), 4, 3,
      context.information.occupancy_text);
  paint_original_person_text(
      dc, original_facility_rectangle(context, 9U), 4, 3,
      context.information.age_text);
  paint_original_person_text(
      dc, original_facility_rectangle(context, 5U), 4, 3,
      context.information.movie_length_text);
  paint_original_person_text(
      dc, original_facility_rectangle(context, 6U), 4, 3,
      context.information.movie_title);
  paint_original_person_text(
      dc, original_facility_rectangle(context, 10U), 4, 3,
      context.information.movie_income_text);
  paint_original_person_text(
      dc, original_facility_rectangle(context, 6U), 4, 3,
      context.information.commercial_value_text);
  paint_original_person_text(
      dc, original_facility_rectangle(context, 9U), 4, 3,
      context.information.yesterday_profit_text);
  // Exact 1108:08e4 presentation boundary: STRL/711 advisory text is emitted
  // at the 11e0:0049/1100:1760-176c item-8 base point plus DS:b3a7 * 16,
  // with the shared three-line counter already enforced by
  // build_original_facility_advisories().
  for (std::size_t line = 0U;
       line < context.information.advisory_line_count;
       ++line) {
    const auto offset =
        simtower::original_facility_advisory_text_offset(line);
    paint_original_person_text(
        dc, original_facility_rectangle(context, 8U), offset.x, offset.y,
        context.information.advisory_lines[line]);
  }
  if (font) {
    SelectObject(dc, old_font);
  }
  SetBkMode(dc, old_background);
  SetTextAlign(dc, old_align);
}

void enable_original_tenant_rename_button(HWND dialog) {
  // NAMETENANTDIALOGFILTER shares 1100:43ed's edit-item-4 OK-button gate.
  EnableWindow(GetDlgItem(dialog, 1),
               GetWindowTextLengthW(GetDlgItem(dialog, 4)) != 0);
}

// Native message boundary for NAMETENANTDIALOGFILTER at 1100:3dc4.
INT_PTR CALLBACK tenant_rename_dialog_proc(HWND dialog, UINT message,
                                           WPARAM wparam, LPARAM lparam) {
  // This filter shares 11e0:0026 SetDlgItemText and 11e0:0000 GetDlgItemText
  // (0xfe-byte maximum) with the person/movie name filters.
  if (message == WM_INITDIALOG) {
    auto* context = reinterpret_cast<OriginalTenantRenameDialogContext*>(
        lparam);
    SetWindowLongPtrW(dialog, DWLP_USER,
                      reinterpret_cast<LONG_PTR>(context));
    simtower::configure_original_dtmp_window(
        dialog, context->dtmp, g_resources, g_logical_palette);
    position_original_dialog(dialog);
    SetClassLongPtrW(dialog, GCLP_HCURSOR, 0);
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    COLORREF background = RGB(0xcc, 0xcc, 0xcc);
    if (g_logical_palette) {
      background = PALETTEINDEX(
          GetNearestPaletteIndex(g_logical_palette, background));
    }
    context->control_background = CreateSolidBrush(background);
    const auto name = original_cp1252_to_wide(
        simtower::original_tenant_saved_name(
            *g_tower_document, context->floor, context->tenant_index));
    original_set_dialog_item_text(dialog, 4, name.c_str());
    EnableWindow(GetDlgItem(dialog, 3), !name.empty());
    enable_original_tenant_rename_button(dialog);
    if (HDC dc = GetDC(dialog)) {
      paint_original_rename_dialog(dialog, dc, context->dtmp, true);
      ReleaseDC(dialog, dc);
    }
    const auto focus = simtower::original_rename_dialog_focus_plan(
        simtower::OriginalRenameDialogFocusPhase::initialize);
    if (focus.focus_edit) SetFocus(GetDlgItem(dialog, 4));
    if (focus.select_all) SendDlgItemMessageW(dialog, 4, EM_SETSEL, 0U, -1);
    return focus.handled_result ? TRUE : FALSE;
  }

  auto* context = reinterpret_cast<OriginalTenantRenameDialogContext*>(
      GetWindowLongPtrW(dialog, DWLP_USER));
  switch (message) {
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(dialog, &paint);
      if (dc && context) {
        paint_original_rename_dialog(dialog, dc, context->dtmp, false);
      }
      EndPaint(dialog, &paint);
      // NAMETENANTDIALOGFILTER 1100:3fd7-3ff1 restores edit item 4 after
      // every paint just like the Person rename dialog.
      EnableWindow(GetDlgItem(dialog, 4), TRUE);
      const auto focus = simtower::original_rename_dialog_focus_plan(
          simtower::OriginalRenameDialogFocusPhase::after_paint);
      if (focus.focus_edit) SetFocus(GetDlgItem(dialog, 4));
      if (focus.select_all) {
        SendDlgItemMessageW(dialog, 4, EM_SETSEL, 0U, -1);
      }
      return focus.handled_result ? TRUE : FALSE;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
      HDC dc = reinterpret_cast<HDC>(wparam);
      SetBkMode(dc, TRANSPARENT);
      if (g_logical_palette) {
        SelectPalette(dc, g_logical_palette, FALSE);
        RealizePalette(dc);
      }
      return reinterpret_cast<INT_PTR>(
          context && context->control_background
              ? context->control_background
              : GetStockObject(NULL_BRUSH));
    }
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
      return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
    case WM_COMMAND: {
      if (!context) break;
      using CommandAction = simtower::OriginalRenameDialogCommandAction;
      const auto command = simtower::original_rename_dialog_command_plan(
          static_cast<std::uint16_t>(LOWORD(wparam)));
      const auto close_dialog = [&]() {
        // NAMETENANTDIALOGFILTER 1100:4069-4082 shares EndDialog(1) across
        // Save, Cancel, and Remove and clears the active modal slot first.
        context->dtmp = {};
        g_original_active_modal_window = nullptr;
        EndDialog(dialog, command.close_result);
      };
      switch (command.action) {
        case CommandAction::refresh_edit_gate:
          enable_original_tenant_rename_button(dialog);
          return TRUE;
        case CommandAction::save:
          if (!g_tower_document) return command.consume ? TRUE : FALSE;
          {
            const auto wide = original_get_dialog_item_text(dialog, 4);
            if (wide.empty()) return TRUE;
            const auto name = original_wide_to_cp1252(wide);
            const auto result = simtower::set_original_tenant_name(
                *g_tower_document, context->floor, context->tenant_index,
                name);
            if (result.status ==
                simtower::OriginalTenantNameStatus::too_long) {
              const auto text = simtower::original_strl_entry(
                  g_resources.find("STRL", 1005), 4U);
              (void)simtower::show_original_alert(
                  dialog, g_resources, 1000, {text, {}, {}, {}});
              return TRUE;
            }
            if (result.status == simtower::OriginalTenantNameStatus::full) {
              const auto text = simtower::original_strl_entry(
                  g_resources.find("STRL", 1005), 7U);
              (void)simtower::show_original_alert(
                  dialog, g_resources, 1000, {text, {}, {}, {}});
            }
            context->changed = context->changed || result.changed;
          }
          close_dialog();
          return TRUE;
        case CommandAction::cancel:
          close_dialog();
          return TRUE;
        case CommandAction::remove:
          if (g_tower_document) {
            const auto result = simtower::remove_original_tenant_name(
                *g_tower_document, context->floor, context->tenant_index);
            context->changed = context->changed || result.changed;
          }
          close_dialog();
          return TRUE;
        case CommandAction::none:
          return command.consume ? TRUE : FALSE;
      }
      break;
    }
  }
  return FALSE;
}

bool run_original_tenant_rename_dialog(std::int16_t floor,
                                       std::size_t tenant_index) {
  // Native DIALOG/732 launcher corresponding to 1100:3d5b.
  if (!g_tower_document) return false;
  const auto launcher = simtower::original_information_launcher_contract(
      simtower::OriginalInformationLauncherKind::tenant_rename);
  const auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", launcher.dialog_resource_id));
  const auto native = simtower::build_native_dialog_template(original);
  OriginalTenantRenameDialogContext context{
      simtower::parse_original_dtmp(
          g_resources.find("DTMP", launcher.dialog_resource_id)),
      floor, tenant_index};
  (void)run_original_modal_dialog(
      g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
      launcher.main_window_owner ? g_main_window : nullptr,
      tenant_rename_dialog_proc,
      reinterpret_cast<LPARAM>(&context));
  release_native_dialog_brush(context.control_background);
  return context.changed;
}

void enable_original_movie_choice_buttons(HWND dialog) {
  if (!g_tower_document) return;
  const auto new_cost = std::bit_cast<std::int16_t>(
      g_part.words_52_to_ac[34]);
  const auto classic_cost = std::bit_cast<std::int16_t>(
      g_part.words_52_to_ac[35]);
  EnableWindow(GetDlgItem(dialog, 1),
               g_tower_document->header.balance >= new_cost);
  EnableWindow(GetDlgItem(dialog, 3),
               g_tower_document->header.balance >= classic_cost);
}

void paint_original_movie_choice_dialog(
    HWND dialog,
    HDC dc,
    const OriginalMovieChoiceDialogContext& context,
    bool render_dtmp) {
  const auto style = simtower::original_movie_choice_dialog_style();
  if (style.realize_logical_palette && g_logical_palette) {
    SelectPalette(dc, g_logical_palette, FALSE);
    RealizePalette(dc);
  }
  const UINT previous_alignment = SetTextAlign(dc, TA_UPDATECP);
  const int previous_background = SetBkMode(dc, TRANSPARENT);
  HGDIOBJ previous_font =
      context.control_font ? SelectObject(dc, context.control_font) : nullptr;
  if (render_dtmp) {
    simtower::render_original_dtmp(dialog, dc, context.dtmp, g_resources);
  } else {
    simtower::paint_original_dialog_chrome(dialog, dc, context.dtmp);
  }
  if (context.control_font) SelectObject(dc, previous_font);
  SetBkMode(dc, previous_background);
  SetTextAlign(dc, previous_alignment);
}

// Native message boundary for MOVIETITLEDIALOGFILTER at 1100:4138.
INT_PTR CALLBACK movie_choice_dialog_proc(HWND dialog, UINT message,
                                          WPARAM wparam, LPARAM lparam) {
  if (message == WM_INITDIALOG) {
    auto* context = reinterpret_cast<OriginalMovieChoiceDialogContext*>(
        lparam);
    SetWindowLongPtrW(dialog, DWLP_USER,
                      reinterpret_cast<LONG_PTR>(context));
    simtower::configure_original_dtmp_window(
        dialog, context->dtmp, g_resources, g_logical_palette);
    position_original_dialog(dialog);
    const auto style = simtower::original_movie_choice_dialog_style();
    if (style.clear_class_cursor) {
      SetClassLongPtrW(dialog, GCLP_HCURSOR, 0);
    }
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    enable_original_movie_choice_buttons(dialog);
    context->control_font = make_original_dialog_font(style.font_pixels);
    COLORREF background = RGB(style.static_red, style.static_green,
                              style.static_blue);
    if (g_logical_palette) {
      background = PALETTEINDEX(
          GetNearestPaletteIndex(g_logical_palette, background));
    }
    context->static_background = CreateSolidBrush(background);
    HDC dc = GetDC(dialog);
    if (dc) {
      paint_original_movie_choice_dialog(dialog, dc, *context, true);
      ReleaseDC(dialog, dc);
    }
    const auto focus_plan =
        simtower::original_painted_dialog_initialization_focus_plan();
    if (focus_plan.set_explicit_focus) {
      SetFocus(GetDlgItem(dialog, 2));
    }
    return focus_plan.consume ? TRUE : FALSE;
  }

  auto* context = reinterpret_cast<OriginalMovieChoiceDialogContext*>(
      GetWindowLongPtrW(dialog, DWLP_USER));
  switch (message) {
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(dialog, &paint);
      if (dc && context) {
        paint_original_movie_choice_dialog(dialog, dc, *context, false);
      }
      EndPaint(dialog, &paint);
      return TRUE;
    }
    case WM_CTLCOLORSTATIC: {
      HDC dc = reinterpret_cast<HDC>(wparam);
      if (context && context->control_font) {
        SelectObject(dc, context->control_font);
      }
      SetBkMode(dc, TRANSPARENT);
      if (g_logical_palette) {
        SelectPalette(dc, g_logical_palette, FALSE);
        RealizePalette(dc);
      }
      return reinterpret_cast<INT_PTR>(
          context && context->static_background
              ? context->static_background
              : GetStockObject(NULL_BRUSH));
    }
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
      if (context && context->control_font) {
        SelectObject(reinterpret_cast<HDC>(wparam), context->control_font);
      }
      return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
    case WM_COMMAND:
      if (context) {
        using CommandAction =
            simtower::OriginalMovieChoiceDialogCommandAction;
        const auto command =
            simtower::original_movie_choice_dialog_command_plan(
                static_cast<std::uint16_t>(LOWORD(wparam)));
        switch (command.action) {
          case CommandAction::cancel:
            context->dtmp = {};
            EndDialog(dialog, 0);
            return TRUE;
          case CommandAction::new_release:
          case CommandAction::classic:
            if (g_tower_document) {
              const auto choice =
                  command.action == CommandAction::new_release
                      ? simtower::OriginalMovieChoice::new_release
                      : simtower::OriginalMovieChoice::classic;
              const auto result = simtower::choose_original_movie(
                  *g_tower_document, g_part,
                  context->linked_record_index, choice);
              context->changed = context->changed || result.changed;
            }
            // 1100:43b4-43c2 always closes recognized New/Classic commands
            // with result one; affordability is enforced by disabled buttons.
            context->dtmp = {};
            EndDialog(dialog, 1);
            return TRUE;
          case CommandAction::none:
            return command.consume ? TRUE : FALSE;
        }
      }
      break;
  }
  return FALSE;
}

bool run_original_movie_choice_dialog(HWND owner,
                                      std::size_t linked_record_index) {
  // Native DIALOG/731 launcher corresponding to 1100:40d5.
  const auto launcher = simtower::original_information_launcher_contract(
      simtower::OriginalInformationLauncherKind::movie_choice);
  const auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", launcher.dialog_resource_id));
  const auto native = simtower::build_native_dialog_template(original);
  OriginalMovieChoiceDialogContext context{
      simtower::parse_original_dtmp(
          g_resources.find("DTMP", launcher.dialog_resource_id)),
      linked_record_index};
  const auto dialog_result = run_original_modal_dialog(
      g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
      launcher.caller_supplied_owner ? owner : g_main_window,
      movie_choice_dialog_proc,
      reinterpret_cast<LPARAM>(&context));
  release_native_dialog_brush(context.static_background);
  return dialog_result != 0;
}

void refresh_original_facility_dialog(
    HWND dialog,
    OriginalFacilityDialogContext& context) {
  if (!g_tower_document) return;
  context.information = simtower::original_facility_information(
      g_resources, *g_tower_document, g_part,
      context.floor, context.tenant_index);
  if (context.information.rent_control_visible) {
    SendDlgItemMessageW(
        dialog, 13, CB_SETCURSEL,
        context.information.selected_rent_rate, 0);
    EnableWindow(GetDlgItem(dialog, 13),
                 context.information.rent_control_enabled);
  }
  InvalidateRect(dialog, nullptr, FALSE);
  UpdateWindow(dialog);
}

// Native message boundary for TENANTINFODLOGFILTER at 1100:085b.
INT_PTR CALLBACK facility_information_dialog_proc(HWND dialog, UINT message,
                                                  WPARAM wparam,
                                                  LPARAM lparam) {
  if (message == WM_INITDIALOG) {
    auto* context = reinterpret_cast<OriginalFacilityDialogContext*>(lparam);
    SetWindowLongPtrW(dialog, DWLP_USER,
                      reinterpret_cast<LONG_PTR>(context));
    simtower::configure_original_dtmp_window(
        dialog, context->dtmp, g_resources, g_logical_palette);
    simtower::render_original_dtmp(
        dialog, nullptr, context->dtmp, g_resources);
    if (context->information.rent_control_visible) {
      for (const auto& choice : context->information.rent_choices) {
        const auto wide = original_cp1252_to_wide(choice);
        SendDlgItemMessageW(
            dialog, 13, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(wide.c_str()));
      }
      SendDlgItemMessageW(
          dialog, 13, CB_SETCURSEL,
          context->information.selected_rent_rate, 0);
      EnableWindow(GetDlgItem(dialog, 13),
                   context->information.rent_control_enabled);
    }
    // TENANTINFODLOGFILTER 1100:08bd-08d0 clears Win16 GCL_HCURSOR and
    // selects the stock arrow as the current pointer.
    SetClassLongPtrW(dialog, GCLP_HCURSOR, 0);
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    context->control_font = make_original_dialog_font(13);
    COLORREF background = RGB(0xcc, 0xcc, 0xcc);
    if (g_logical_palette) {
      background = PALETTEINDEX(
          GetNearestPaletteIndex(g_logical_palette, background));
    }
    context->static_background = CreateSolidBrush(background);
    position_original_dialog(dialog);
    if (HDC dc = GetDC(dialog)) {
      // 1100:08d1-092d performs the first palette-realized 13-pixel-font
      // presentation synchronously during initialization.
      if (g_logical_palette) {
        SelectPalette(dc, g_logical_palette, FALSE);
        RealizePalette(dc);
      }
      paint_original_facility_dialog(dialog, dc, *context);
      ReleaseDC(dialog, dc);
    }
    const auto focus_plan =
        simtower::original_painted_dialog_initialization_focus_plan();
    if (focus_plan.set_explicit_focus) {
      SetFocus(GetDlgItem(dialog, 1));
    }
    return focus_plan.consume ? TRUE : FALSE;
  }

  auto* context = reinterpret_cast<OriginalFacilityDialogContext*>(
      GetWindowLongPtrW(dialog, DWLP_USER));
  switch (message) {
    case WM_ACTIVATE:
      // Exact 1100:0a22-0a3d nested-dialog activation guard.
      if (const auto plan = simtower::original_information_activation_plan(
              LOWORD(wparam) != WA_INACTIVE,
              g_original_active_modal_window != nullptr,
              g_original_active_modal_window == dialog);
          plan.activate_nested_modal) {
        SetActiveWindow(g_original_active_modal_window);
      }
      return TRUE;
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(dialog, &paint);
      if (dc && context) {
        if (g_logical_palette) {
          // Exact paint-time SelectPalette/RealizePalette at 1100:0aa2-0ab3.
          SelectPalette(dc, g_logical_palette, FALSE);
          RealizePalette(dc);
        }
        paint_original_facility_dialog(dialog, dc, *context);
      }
      EndPaint(dialog, &paint);
      return TRUE;
    }
    case WM_LBUTTONDOWN:
      if (context) {
        const auto click_plan =
            simtower::original_facility_information_click_plan();
        HDC dc = GetDC(dialog);
        if (dc && g_logical_palette) {
          if (click_plan.select_palette) {
            SelectPalette(dc, g_logical_palette, FALSE);
          }
          if (click_plan.realize_palette) RealizePalette(dc);
        }

        std::optional<std::size_t> person{};
        if (g_tower_document) {
          person = simtower::original_information_person_sprite_hit(
              context->information.person_sprites,
              static_cast<std::int16_t>(LOWORD(lparam)),
              static_cast<std::int16_t>(HIWORD(lparam)));
        }
        if (person) {
          bool changed = false;
          (void)run_original_person_information_dialog(
              dialog, *person, &changed,
              simtower::OriginalPersonInformationContext::facility_dialog);
          context->changed = context->changed || changed;
          refresh_original_facility_dialog(dialog, *context);
        }
        if (dc) ReleaseDC(dialog, dc);

        // 1100:0e3a-0e50 performs both restorations for every click, not only
        // after a portrait hit.
        if (click_plan.restore_modal_target) {
          g_original_active_modal_window = dialog;
        }
        if (click_plan.restore_topmost) {
          SetWindowPos(dialog, HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
        }
        return click_plan.consume ? TRUE : FALSE;
      }
      break;
    case WM_MOUSEMOVE:
      if (context) {
        // TENANTINFODLOGFILTER uses 1100:5043 (items 4/9) only for dialog
        // groups 9..11; every other group uses 1100:4fba (item 4 only).
        const bool include_item_9 =
            context->information.dialog_group >= 9U &&
            context->information.dialog_group <= 11U;
        const bool portrait_panel =
            simtower::original_information_portrait_panel_hit(
                context->dtmp,
                static_cast<std::int16_t>(LOWORD(lparam)),
                static_cast<std::int16_t>(HIWORD(lparam)), include_item_9);
        SetCursor(resolve_original_cursor(portrait_panel ? 1003U : 0U));
        return TRUE;
      }
      break;
    case WM_CTLCOLORSTATIC: {
      HDC dc = reinterpret_cast<HDC>(wparam);
      if (context && context->control_font) {
        SelectObject(dc, context->control_font);
      }
      SetBkMode(dc, TRANSPARENT);
      if (g_logical_palette) {
        SelectPalette(dc, g_logical_palette, FALSE);
        RealizePalette(dc);
      }
      if (simtower::original_facility_control_background(true) ==
              simtower::OriginalFacilityControlBackground::gray_cc &&
          context && context->static_background) {
        return reinterpret_cast<INT_PTR>(context->static_background);
      }
      return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
    }
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
      if (context && context->control_font) {
        SelectObject(reinterpret_cast<HDC>(wparam), context->control_font);
      }
      return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
    case WM_COMMAND:
      if (context) {
        using CommandAction =
            simtower::OriginalFacilityInformationCommandAction;
        const auto command =
            simtower::original_facility_information_command_plan(
                static_cast<std::uint16_t>(LOWORD(wparam)),
                static_cast<std::uint16_t>(HIWORD(wparam)),
                context->information.dialog_group);
        switch (command.action) {
          case CommandAction::close:
            g_original_active_modal_window = nullptr;
            EndDialog(dialog, 1);
            return TRUE;
          case CommandAction::rename:
            if (g_tower_document) {
              // TENANTINFODLOGFILTER 1100:0b4e clears DS:31a4 for the nested
              // Rename Tenant modal, then republishes itself at 0b62.
              g_original_active_modal_window = nullptr;
              const bool renamed = run_original_tenant_rename_dialog(
                  context->floor, context->tenant_index);
              // 1100:0b62-0b78 restores the Facility panel after Rename,
              // including a cancelled rename.
              g_original_active_modal_window = dialog;
              SetWindowPos(dialog, HWND_TOPMOST, 0, 0, 0, 0,
                           SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
              if (renamed) {
                context->changed = true;
                g_tower_dirty = true;
                update_original_tower_title(g_main_window);
              }
              refresh_original_facility_dialog(dialog, *context);
            }
            return command.consume ? TRUE : FALSE;
          case CommandAction::change_rent:
            if (g_tower_document) {
              const auto selection = SendDlgItemMessageW(
                  dialog, 13, CB_GETCURSEL, 0, 0);
              if (selection >= 0 && selection <= 3 &&
                  simtower::set_original_facility_rent_rate(
                      *g_tower_document, g_part,
                      context->floor, context->tenant_index,
                      static_cast<std::uint8_t>(selection))) {
                context->changed = true;
                g_tower_dirty = true;
                update_original_tower_title(g_main_window);
                refresh_original_facility_dialog(dialog, *context);
              }
            }
            return command.consume ? TRUE : FALSE;
          case CommandAction::choose_movie:
            if (g_tower_document &&
                context->information.linked_record_index !=
                    simtower::OriginalMagnifierTarget::kNoIndex &&
                run_original_movie_choice_dialog(
                    dialog, context->information.linked_record_index)) {
              context->changed = true;
              g_tower_dirty = true;
              update_original_tower_title(g_main_window);
              refresh_original_facility_dialog(dialog, *context);
            }
            return command.consume ? TRUE : FALSE;
          case CommandAction::none:
            return command.consume ? TRUE : FALSE;
        }
      }
      break;
  }
  return FALSE;
}

int run_original_facility_information_dialog(
    HWND owner,
    std::int16_t floor,
    std::size_t tenant_index,
    bool* changed) {
  if (!g_tower_document) return 0;
  auto information = simtower::original_facility_information(
      g_resources, *g_tower_document, g_part, floor, tenant_index);
  if (!information.valid) return 0;
  const auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", information.dialog_id));
  const auto native = simtower::build_native_dialog_template(original);
  OriginalFacilityDialogContext context{
      simtower::parse_original_dtmp(
          g_resources.find("DTMP", information.dialog_id)),
      floor, tenant_index, std::move(information)};
  const auto& preview = context.information.preview;
  if (preview.valid()) {
    RECT main_client{};
    if (g_main_window) GetClientRect(g_main_window, &main_client);
    const auto visible_cells = static_cast<std::int16_t>(
        (std::max(0L, main_client.right) + 7L) / 8L + 1L);
    const auto visible_floors = static_cast<std::int16_t>(
        (std::max(0L, main_client.bottom - main_client.top) + 35L) / 36L +
        1L);
    const auto backing =
        simtower::original_facility_preview_backing_counts(
            *g_tower_document, floor, tenant_index,
            visible_cells, visible_floors);
    context.preview_raster = simtower::render_original_world(
        g_resources, &*g_tower_document,
        preview.view_x, preview.view_y,
        std::max(preview.width,
                 static_cast<int>(backing.visible_cells) * 8),
        std::max(preview.height,
                 static_cast<int>(backing.visible_floors) * 36),
        std::span<const simtower::OriginalElevatorTransferVisual>{},
        g_palette_runtime.initialized ? &g_palette_runtime.colors : nullptr);
  }
  if (const auto resource = simtower::select_original_facility_sound(
          *g_tower_document, g_audio && g_audio->sound_enabled(),
          floor, tenant_index);
      resource && g_audio) {
    (void)g_audio->play_resource(*resource, 0U, 1U, GetTickCount());
  }
  const int result = static_cast<int>(run_original_modal_dialog(
      g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
      owner, facility_information_dialog_proc,
      reinterpret_cast<LPARAM>(&context)));
  release_native_dialog_brush(context.static_background);
  if (changed) *changed = context.changed;
  return result;
}

const simtower::OriginalDtmpRect& original_finance_rectangle(
    const OriginalFinanceDialogContext& context,
    std::size_t one_based_index) {
  if (one_based_index == 0U ||
      one_based_index > context.dtmp.rectangles.size()) {
    throw std::runtime_error("Original Finance DTMP rectangle is missing");
  }
  return context.dtmp.rectangles[one_based_index - 1U];
}

void paint_original_finance_value(
    HDC dc,
    const OriginalFinanceDialogContext& context,
    std::size_t rectangle_index,
    std::int32_t value) {
  // Exact native text/placement equivalent of Finance helper 11e0:00ca:
  // 1208:0c89 measures the formatted string and 1208:07a5 emits it at the
  // current drawing position.
  const auto text = std::to_wstring(value);
  SIZE extent{};
  GetTextExtentPoint32W(dc, text.data(), static_cast<int>(text.size()),
                        &extent);
  const auto& rectangle =
      original_finance_rectangle(context, rectangle_index);
  const auto position = simtower::original_finance_value_position(
      rectangle_index, rectangle.right, rectangle.top, extent.cx);
  MoveToEx(dc, position.x, position.y, nullptr);
  TextOutW(dc, 0, 0, text.data(), static_cast<int>(text.size()));
}

void paint_original_finance_dialog(
    HDC dc,
    const OriginalFinanceDialogContext& context) {
  // 1060:0105/0479 builds the default surface from BITMAP/500. BITMAP/501
  // differs in the DTMP/500 button rectangle and supplies its pressed state.
  simtower::draw_original_dib(dc, g_resources.find("BITMAP", 500), 0, 0);
  if (context.pressed) {
    const auto& button = original_finance_rectangle(context, 1U);
    simtower::draw_original_dib_region(
        dc, g_resources.find("BITMAP", 501), button.left, button.top,
        button.left, button.top, button.right - button.left,
        button.bottom - button.top);
  }

  const int old_background = SetBkMode(dc, TRANSPARENT);
  const UINT old_alignment = SetTextAlign(dc, TA_UPDATECP);
  HFONT font14 = make_original_dialog_font(14);
  HFONT font12 = make_original_dialog_font(12);
  HGDIOBJ old_font = font14 ? SelectObject(dc, font14) : nullptr;

  paint_original_finance_value(dc, context, 6U, context.view.total_income);
  paint_original_finance_value(dc, context, 7U,
                               context.view.total_maintenance);
  if (font12) SelectObject(dc, font12);
  paint_original_finance_value(dc, context, 8U, context.view.year);
  paint_original_finance_value(dc, context, 9U, context.view.quarter);
  paint_original_finance_value(dc, context, 10U,
                               context.view.net_revenues);
  paint_original_finance_value(dc, context, 11U,
                               context.view.other_income);
  paint_original_finance_value(dc, context, 12U,
                               context.view.construction_costs);
  paint_original_finance_value(dc, context, 13U,
                               context.view.last_quarter_balance);
  paint_original_finance_value(dc, context, 14U,
                               context.view.total_balance);

  SetTextAlign(dc, TA_UPDATECP | TA_RIGHT | TA_BASELINE);
  for (std::size_t row = 0U; row < 10U; ++row) {
    const std::array values{
        context.view.population[row],
        context.view.income[row],
        context.view.maintenance[row],
    };
    for (std::size_t column = 0U; column < values.size(); ++column) {
      const auto& rectangle =
          original_finance_rectangle(context, 3U + column);
      const auto text = std::to_wstring(values[column]);
      MoveToEx(dc, static_cast<int>(rectangle.right) - 1,
               static_cast<int>(rectangle.top) +
                   static_cast<int>(row + 1U) * 13 - 2,
               nullptr);
      TextOutW(dc, 0, 0, text.data(), static_cast<int>(text.size()));
    }
  }

  if (old_font) SelectObject(dc, old_font);
  SetTextAlign(dc, old_alignment);
  SetBkMode(dc, old_background);
}

bool original_finance_button_hit(
    const OriginalFinanceDialogContext& context,
    LPARAM position) {
  const auto& button = original_finance_rectangle(context, 1U);
  const int x = static_cast<std::int16_t>(LOWORD(position));
  const int y = static_cast<std::int16_t>(HIWORD(position));
  return x >= button.left && x < button.right &&
         y >= button.top && y < button.bottom;
}

// Native message boundary for COUNTDLOGMAIN at 1060:00d3.
INT_PTR CALLBACK finance_dialog_proc(HWND dialog, UINT message,
                                     WPARAM wparam, LPARAM lparam) {
  if (message == WM_INITDIALOG) {
    auto* context = reinterpret_cast<OriginalFinanceDialogContext*>(lparam);
    SetWindowLongPtrW(dialog, DWLP_USER,
                      reinterpret_cast<LONG_PTR>(context));
    simtower::configure_original_dtmp_window(dialog, context->dtmp,
                                             g_resources,
                                             g_logical_palette);
    position_original_dialog(dialog);
    // 1060:0278 follows the custom surface setup with 11e0:0d80(0).
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    SetFocus(dialog);
    return TRUE;
  }

  auto* context = reinterpret_cast<OriginalFinanceDialogContext*>(
      GetWindowLongPtrW(dialog, DWLP_USER));
  switch (message) {
    // Win16 WM_CTLCOLOR at 1060:028e ignores the control subtype and returns
    // NULL_BRUSH.  Win32 splits that message by control class.
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSCROLLBAR:
    case WM_CTLCOLORSTATIC:
      return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(dialog, &paint);
      if (dc && context) paint_original_finance_dialog(dc, *context);
      EndPaint(dialog, &paint);
      return FALSE;
    }
    case WM_KEYUP:
      if (context) {
        const auto presentation =
            simtower::original_finance_key_presentation(
                static_cast<std::uint16_t>(wparam));
        const auto& button = original_finance_rectangle(*context, 1U);
        const RECT rectangle{button.left, button.top,
                             button.right, button.bottom};
        if (presentation.draw_pressed) {
          context->pressed = true;
          InvalidateRect(dialog, &rectangle, FALSE);
          UpdateWindow(dialog);
        }
        if (presentation.draw_released) {
          context->pressed = false;
          InvalidateRect(dialog, &rectangle, FALSE);
          UpdateWindow(dialog);
        }
        if (!presentation.close) break;
        EndDialog(dialog, 0);
        // COUNTDLOGMAIN clears DS:31a4 after EndDialog and then returns
        // FALSE through 1060:0454.
        g_original_active_modal_window = nullptr;
        return FALSE;
      }
      break;
    case WM_LBUTTONDOWN:
      if (context && original_finance_button_hit(*context, lparam)) {
        context->pressed = true;
        const auto& button = original_finance_rectangle(*context, 1U);
        const RECT rectangle{button.left, button.top,
                             button.right, button.bottom};
        InvalidateRect(dialog, &rectangle, FALSE);
        UpdateWindow(dialog);
      }
      return FALSE;
    case WM_LBUTTONUP:
      if (context) {
        const bool close = original_finance_button_hit(*context, lparam);
        context->pressed = false;
        const auto& button = original_finance_rectangle(*context, 1U);
        const RECT rectangle{button.left, button.top,
                             button.right, button.bottom};
        InvalidateRect(dialog, &rectangle, FALSE);
        UpdateWindow(dialog);
        if (close) {
          EndDialog(dialog, 0);
          g_original_active_modal_window = nullptr;
        }
      }
      return FALSE;
  }
  return FALSE;
}

void run_original_finance_dialog() {
  if (!g_tower_document) return;
  const auto launcher = simtower::original_finance_launcher_contract();
  const auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", launcher.dialog_resource_id));
  const auto native = simtower::build_native_dialog_template(original);
  OriginalFinanceDialogContext context{
      simtower::parse_original_dtmp(
          g_resources.find("DTMP", launcher.dialog_resource_id)),
      simtower::derive_original_finance_view(*g_tower_document)};
  (void)run_original_modal_dialog(
      g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
      launcher.main_window_owner ? g_main_window : nullptr,
      finance_dialog_proc,
      reinterpret_cast<LPARAM>(&context));
}

bool run_original_help_contents() {
  // 1158:091a calls WinHelp with CS:06ac ("SIMTOWER.HLP") and command 3.
  // Keep the original help compiler payload inside the PE, materializing it
  // only when F1/Help Contents is requested so no installed game files are
  // required at runtime.
  const HRSRC resource = FindResourceW(
      g_instance, MAKEINTRESOURCEW(102), MAKEINTRESOURCEW(10));
  if (!resource) return false;
  const HGLOBAL loaded = LoadResource(g_instance, resource);
  const auto* bytes = static_cast<const char*>(LockResource(loaded));
  const DWORD size = SizeofResource(g_instance, resource);
  if (!bytes || size == 0U) return false;

  std::error_code error;
  auto directory = std::filesystem::temp_directory_path(error);
  if (error) return false;
  directory /= L"SimTower Native Help";
  std::filesystem::create_directories(directory, error);
  if (error) return false;
  const auto help_path = directory / L"SIMTOWER.HLP";

  bool write = true;
  const auto existing_size = std::filesystem::file_size(help_path, error);
  if (!error && existing_size == size) write = false;
  error.clear();
  if (write) {
    std::ofstream stream(help_path, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    stream.write(bytes, static_cast<std::streamsize>(size));
    if (!stream) return false;
  }
  return WinHelpW(g_main_window, help_path.c_str(), HELP_CONTENTS, 0U) != FALSE;
}

struct OriginalFindDialogContext {
  simtower::OriginalFindMode mode{simtower::OriginalFindMode::tenant};
  simtower::OriginalDtmp dtmp{};
  std::vector<simtower::OriginalFindEntry> entries{};
  simtower::OriginalFindResolution resolution{};
  bool changed{};
  HFONT control_font{};
};

int original_find_selected_index(HWND dialog) {
  // 10d8:0487 sends the Win16 list-box selection query (message 0x420) to
  // Find item 5. LB_GETCURSEL is its native Win32 semantic equivalent.
  constexpr auto query = simtower::original_find_selection_query();
  const auto selected = SendDlgItemMessageW(
      dialog, query.control_id, LB_GETCURSEL, query.wparam,
      static_cast<LPARAM>(query.lparam));
  return selected == LB_ERR ? -1 : static_cast<int>(selected);
}

void enable_original_find_actions(HWND dialog, bool enabled) {
  EnableWindow(GetDlgItem(dialog, 3), enabled);
  EnableWindow(GetDlgItem(dialog, 4), enabled);
}

void populate_original_find_list(HWND dialog,
                                 const OriginalFindDialogContext& context) {
  SendDlgItemMessageW(dialog, 5, LB_RESETCONTENT, 0U, 0);
  for (const auto& entry : context.entries) {
    const auto text = original_cp1252_to_wide(entry.name);
    SendDlgItemMessageW(dialog, 5, LB_ADDSTRING, 0U,
                        reinterpret_cast<LPARAM>(text.c_str()));
  }
}

void paint_original_find_dialog(HWND dialog,
                                HDC dc,
                                const OriginalFindDialogContext& context) {
  const auto plan = simtower::original_find_presentation_plan(
      simtower::OriginalFindPresentationPhase::paint);
  if (plan.realize_palette && g_logical_palette) {
    SelectPalette(dc, g_logical_palette, FALSE);
    RealizePalette(dc);
  }
  if (plan.draw_generic_chrome) {
    simtower::paint_original_dialog_chrome(dialog, dc, context.dtmp);
  }
  if (plan.render_positive_dtmp) {
    // 10d8:0346 passes a positive DTMP ID to 1070:0231, which redraws the
    // bitmap and also replays child MoveWindow/ShowWindow operations.
    simtower::render_original_dtmp(
        dialog, dc, context.dtmp, g_resources);
  }
}

bool resolve_original_find_selection(HWND dialog,
                                     OriginalFindDialogContext& context) {
  if (!g_tower_document) return false;
  const int selected = original_find_selected_index(dialog);
  if (selected < 0 ||
      static_cast<std::size_t>(selected) >= context.entries.size()) {
    return false;
  }

  RECT client{};
  GetClientRect(g_main_window, &client);
  const auto& entry = context.entries[static_cast<std::size_t>(selected)];
  // FINDDIALOGFILTER clears a previous DS:77b4 target before resolving the
  // new list selection.
  simtower::reset_original_find_marker(g_find_marker);
  if (context.mode == simtower::OriginalFindMode::person) {
    context.resolution = simtower::resolve_original_find_person(
        *g_tower_document, entry.link, client.right - client.left,
        client.bottom - client.top);
  } else {
    context.resolution = simtower::resolve_original_find_tenant(
        *g_tower_document, static_cast<std::uint16_t>(entry.link),
        client.right - client.left, client.bottom - client.top);
  }
  if (context.mode == simtower::OriginalFindMode::person &&
      context.resolution.alerts()) {
    // Exact 10e0:0669/06cd named-person fallbacks. They select ALRT/1002
    // when the person is outside the tower, or ALRT/1003 with the recovered
    // lobby floor text when no more precise tenant/transit position exists.
    SetCursor(resolve_original_cursor(0U));
    if (context.resolution.kind ==
        simtower::OriginalFindResolutionKind::not_in_tower_alert) {
      (void)simtower::show_original_alert(
          dialog, g_resources, 1002, {entry.name, {}, {}, {}});
    } else {
      const auto floor_text = simtower::original_find_floor_text(
          context.resolution.floor);
      (void)simtower::show_original_alert(
          dialog, g_resources, 1003,
          {entry.name, floor_text, {}, {}});
    }
    // Both original alert helpers call 10e0:04cf(0), leaving Find open and
    // the target marker reset.
    context.resolution = {};
    invalidate_original_main_surface();
    return false;
  }
  if (!context.resolution.focused()) {
    // 10e0:0cc9 keeps the modal open for targets which no longer resolve.
    // Do not invent a viewport jump.
    return false;
  }
  simtower::start_original_find_marker(
      g_find_marker, context.resolution,
      context.mode == simtower::OriginalFindMode::person
          ? entry.link
          : 0xffffffffU,
      original_runtime_coarse_tick());
  g_original_active_modal_window = nullptr;
  EndDialog(dialog, 1);
  return true;
}

// Native message boundary for FINDDIALOGFILTER at 10d8:006f.
INT_PTR CALLBACK find_dialog_proc(HWND dialog, UINT message,
                                  WPARAM wparam, LPARAM lparam) {
  if (message == WM_INITDIALOG) {
    auto* context = reinterpret_cast<OriginalFindDialogContext*>(lparam);
    SetWindowLongPtrW(dialog, DWLP_USER,
                      reinterpret_cast<LONG_PTR>(context));
    simtower::configure_original_dtmp_window(dialog, context->dtmp,
                                             g_resources,
                                             g_logical_palette);
    SetClassLongPtrW(dialog, GCLP_HCURSOR, 0);
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    context->control_font = make_original_dialog_font(13);
    position_original_dialog(dialog);
    populate_original_find_list(dialog, *context);
    enable_original_find_actions(dialog, false);
    HDC dc = GetDC(dialog);
    if (dc) {
      const auto plan = simtower::original_find_presentation_plan(
          simtower::OriginalFindPresentationPhase::initialization);
      if (plan.realize_palette && g_logical_palette) {
        SelectPalette(dc, g_logical_palette, FALSE);
        RealizePalette(dc);
      }
      SetTextAlign(dc, TA_UPDATECP);
      SetBkMode(dc, TRANSPARENT);
      HFONT initial_font = make_original_dialog_font(plan.font_pixels);
      HGDIOBJ old_font = initial_font ? SelectObject(dc, initial_font)
                                     : nullptr;
      if (plan.draw_generic_chrome) {
        simtower::paint_original_dialog_chrome(dialog, dc, context->dtmp);
      }
      if (plan.render_positive_dtmp) {
        simtower::render_original_dtmp(
            dialog, dc, context->dtmp, g_resources);
      }
      if (initial_font) {
        SelectObject(dc, old_font);
      }
      ReleaseDC(dialog, dc);
    }
    const auto focus = simtower::original_find_initialization_focus_plan();
    if (focus.focus_list) SetFocus(GetDlgItem(dialog, 5));
    return focus.handled_result ? TRUE : FALSE;
  }

  auto* context = reinterpret_cast<OriginalFindDialogContext*>(
      GetWindowLongPtrW(dialog, DWLP_USER));
  switch (message) {
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(dialog, &paint);
      if (dc && context) paint_original_find_dialog(dialog, dc, *context);
      EndPaint(dialog, &paint);
      return TRUE;
    }
    case WM_CTLCOLORLISTBOX:
      if (context && context->control_font) {
        SelectObject(reinterpret_cast<HDC>(wparam), context->control_font);
      }
      return reinterpret_cast<INT_PTR>(GetStockObject(WHITE_BRUSH));
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
      if (context && context->control_font) {
        SelectObject(reinterpret_cast<HDC>(wparam), context->control_font);
      }
      return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
    case WM_COMMAND: {
      if (!context) break;
      using CommandAction = simtower::OriginalFindDialogCommandAction;
      const auto command = simtower::original_find_dialog_command_plan(
          static_cast<std::uint16_t>(LOWORD(wparam)),
          static_cast<std::uint16_t>(HIWORD(wparam)));
      switch (command.action) {
        case CommandAction::close:
          // 10d8:018c calls 10e0:04cf(0) before cleanup/EndDialog(1).
          simtower::reset_original_find_marker(g_find_marker);
          invalidate_original_main_surface();
          g_original_active_modal_window = nullptr;
          EndDialog(dialog, 1);
          return TRUE;
        case CommandAction::remove: {
          if (!g_tower_document) return command.consume ? TRUE : FALSE;
          const int selected = original_find_selected_index(dialog);
          if (selected >= 0 &&
              simtower::remove_original_find_entry(
                  *g_tower_document, context->mode,
                  static_cast<std::size_t>(selected))) {
            SendDlgItemMessageW(dialog, 5, LB_DELETESTRING,
                                static_cast<WPARAM>(selected), 0);
            context->entries.erase(
                context->entries.begin() +
                static_cast<std::ptrdiff_t>(selected));
            context->changed = true;
            g_tower_dirty = true;
            update_original_tower_title(g_main_window);
          }
          return command.consume ? TRUE : FALSE;
        }
        case CommandAction::resolve:
          (void)resolve_original_find_selection(dialog, *context);
          return command.consume ? TRUE : FALSE;
        case CommandAction::enable_actions:
          enable_original_find_actions(dialog, true);
          return command.consume ? TRUE : FALSE;
        case CommandAction::disable_actions:
          enable_original_find_actions(dialog, false);
          return command.consume ? TRUE : FALSE;
        case CommandAction::none:
          return command.consume ? TRUE : FALSE;
      }
      break;
    }
  }
  return FALSE;
}

void run_original_find_dialog(simtower::OriginalFindMode mode) {
  if (!g_tower_document) return;
  const auto launcher = simtower::original_find_launcher_contract(mode);
  const auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", launcher.dialog_resource_id));
  const auto native = simtower::build_native_dialog_template(original);
  OriginalFindDialogContext context{
      mode,
      simtower::parse_original_dtmp(
          g_resources.find("DTMP", launcher.dialog_resource_id)),
      simtower::original_find_entries(*g_tower_document, mode)};
  (void)run_original_modal_dialog(
      g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
      launcher.main_window_owner ? g_main_window : nullptr,
      find_dialog_proc, reinterpret_cast<LPARAM>(&context));
  if (context.resolution.focused()) {
    // 10e0:0cea exits construction into edit mode two, synchronously presents
    // Command then Map, restores pending preview scratch through 11f8:3b94,
    // and only then applies 1080:0000's camera transform/full refresh.
    end_original_lobby_drag();
    g_build_mode_enabled = false;
    g_command_mode = 2U;
    for (const auto step : simtower::original_find_focus_refresh_order()) {
      switch (step) {
        case simtower::OriginalFindFocusRefreshStep::command_repaint:
          if (g_command_window) {
            InvalidateRect(g_command_window, nullptr, FALSE);
            UpdateWindow(g_command_window);
          }
          break;
        case simtower::OriginalFindFocusRefreshStep::map_repaint:
          if (g_map_window) {
            InvalidateRect(g_map_window, nullptr, FALSE);
            UpdateWindow(g_map_window);
          }
          break;
        case simtower::OriginalFindFocusRefreshStep::restore_preview_scratch:
          if ((!g_elevator_control_context ||
               !g_elevator_control_context->state.isolation_active) &&
              g_original_main_preview_rect) {
            g_original_main_preview_rect.reset();
            g_original_main_backing.dirty = true;
          }
          break;
        case simtower::OriginalFindFocusRefreshStep::camera_transform:
          set_original_view(g_main_window, context.resolution.view.x,
                            context.resolution.view.y);
          break;
      }
    }
  }
}

struct OriginalAboutDialogContext {
  simtower::OriginalAboutLayout layout{};
  std::vector<std::string> lines{};
  simtower::OriginalDtmp chrome{};
  std::uint64_t timer_ticks{};
  bool timer_started{};
  HBRUSH control_background{};
};

void paint_original_about_dialog(HWND dialog,
                                 HDC dc,
                                 const OriginalAboutDialogContext& context) {
  // 1010:056f lays down the custom dialog and credits bevel before drawing
  // BITMAP/257 at (+10,+10).
  simtower::paint_original_dialog_chrome(dialog, dc, context.chrome);
  simtower::draw_original_dib(
      dc, g_resources.find("BITMAP", 257),
      context.layout.title_bitmap.left, context.layout.title_bitmap.top);

  // 1010:0a3b staged TEXT/128 into a 236x16 bitmap and 1010:098f filled it
  // with RGB(230) before centered DrawText flags 0809. Drawing the same line
  // directly into this clipped native surface eliminates only that staging DC.
  const RECT clip{
      context.layout.credits_inner.left,
      context.layout.credits_inner.top,
      context.layout.credits_inner.right,
      context.layout.credits_inner.bottom};
  const auto line_style = simtower::original_about_line_style();
  HBRUSH background = CreateSolidBrush(RGB(
      line_style.background, line_style.background, line_style.background));
  if (background) {
    FillRect(dc, &clip, background);
    DeleteObject(background);
  }

  const int saved = SaveDC(dc);
  IntersectClipRect(dc, clip.left, clip.top, clip.right, clip.bottom);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(0, 0, 0));
  HFONT font = make_original_dialog_font(line_style.font_pixels);
  HGDIOBJ previous_font = font ? SelectObject(dc, font) : nullptr;
  for (const auto& line : simtower::original_about_visible_lines(
           context.lines, context.timer_ticks,
           clip.bottom - clip.top)) {
    RECT rectangle{
        clip.left,
        clip.top + line.top,
        clip.right,
        clip.top + line.top + context.layout.line_height};
    const auto text = original_cp1252_to_wide(line.text);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rectangle,
              line_style.draw_text_flags);
  }
  if (font) {
    SelectObject(dc, previous_font);
  }
  if (saved != 0) RestoreDC(dc, saved);
}

void close_original_about_dialog(HWND dialog,
                                 OriginalAboutDialogContext* context) {
  KillTimer(dialog, 9U);
  if (context && context->control_background) {
    DeleteObject(context->control_background);
    context->control_background = nullptr;
  }
  EndDialog(dialog, 1);
}

// Native message boundary for ABOUTDLGPROC at 1010:053f.
INT_PTR CALLBACK about_dialog_proc(HWND dialog, UINT message,
                                   WPARAM wparam, LPARAM lparam) {
  if (message == WM_INITDIALOG) {
    auto* context = reinterpret_cast<OriginalAboutDialogContext*>(lparam);
    SetWindowLongPtrW(dialog, DWLP_USER,
                      reinterpret_cast<LONG_PTR>(context));
    RECT current{};
    GetWindowRect(dialog, &current);
    MoveWindow(dialog, current.left, current.top,
               context->layout.window_width,
               context->layout.window_height, FALSE);
    center_native_dialog(dialog);
    // 1010:057b writes zero to Win16 GCL_HCURSOR (-12), then 11e0:0d80(0)
    // explicitly selects IDC_ARROW for the current pointer.
    SetClassLongPtrW(dialog, GCLP_HCURSOR, 0);
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    const UINT gray_index = g_logical_palette
                                ? GetNearestPaletteIndex(
                                      g_logical_palette, RGB(230, 230, 230))
                                : 0U;
    context->control_background =
        CreateSolidBrush(g_logical_palette ? PALETTEINDEX(gray_index)
                                           : RGB(230, 230, 230));
    return TRUE;
  }

  auto* context = reinterpret_cast<OriginalAboutDialogContext*>(
      GetWindowLongPtrW(dialog, DWLP_USER));
  switch (message) {
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(dialog, &paint);
      if (dc && context) paint_original_about_dialog(dialog, dc, *context);
      EndPaint(dialog, &paint);
      if (context && !context->timer_started) {
        context->timer_started = true;
        if (SetTimer(dialog, 9U, context->layout.timer_interval_ms,
                     nullptr) == 0U) {
          MessageBeep(static_cast<UINT>(-1));
          MessageBoxA(dialog, "SetTimer failed", "SimTower - About",
                      MB_OK | MB_ICONEXCLAMATION);
        }
      }
      return TRUE;
    }
    // Win16 WM_CTLCOLOR at 1010:0758 ignores the control subtype, realizes
    // the shared palette in the supplied DC, and returns the 0xe6e6e6 brush.
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSCROLLBAR:
    case WM_CTLCOLORSTATIC: {
      HDC dc = reinterpret_cast<HDC>(wparam);
      if (g_logical_palette) {
        SelectPalette(dc, g_logical_palette, FALSE);
        RealizePalette(dc);
      }
      return reinterpret_cast<INT_PTR>(
          context && context->control_background
              ? context->control_background
              : GetStockObject(LTGRAY_BRUSH));
    }
    case WM_TIMER:
      if (context) {
        ++context->timer_ticks;
        const RECT credits{
            context->layout.credits_inner.left,
            context->layout.credits_inner.top,
            context->layout.credits_inner.right,
            context->layout.credits_inner.bottom};
        InvalidateRect(dialog, &credits, FALSE);
        PostMessageW(dialog, WM_PAINT, 0U, 0L);
      }
      // ABOUTDLGPROC 1010:0774 reaches the common FALSE return after posting
      // WM_PAINT; the timer identifier is deliberately not inspected.
      return FALSE;
    case WM_KEYDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
      close_original_about_dialog(dialog, context);
      return TRUE;
  }
  return FALSE;
}

int run_original_about_dialog() {
  const auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", "TOWER_TITLE"));
  const auto native = simtower::build_native_dialog_template(original);
  OriginalAboutDialogContext context{
      simtower::derive_original_about_layout(g_resources),
      simtower::original_about_credit_lines(g_resources)};
  context.chrome.rectangles.push_back(context.layout.credits_outer);
  int result = 0;
  for (const auto step : simtower::original_about_launcher_plan()) {
    using Step = simtower::OriginalAboutLauncherStep;
    switch (step) {
      case Step::stop_audio_channels:
        if (g_audio) g_audio->stop_all(true);
        break;
      case Step::deactivate_mixer_backend:
        if (g_audio) g_audio->deactivate_mixer_backend();
        break;
      case Step::compact_global_heap:
        // Win32 has no useful equivalent for Win16 GLOBALCOMPACT(0x1e8480).
        break;
      case Step::run_modal_dialog:
        result = static_cast<int>(run_original_modal_dialog(
            g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
            g_main_window, about_dialog_proc,
            reinterpret_cast<LPARAM>(&context)));
        break;
      case Step::activate_mixer_backend:
        if (g_audio) g_audio->activate_mixer_backend();
        break;
    }
  }
  return result;
}

int run_original_transport_information_dialog(
    const simtower::OriginalMagnifierTarget& target) {
  // Shared native boundaries for ELVINFODLOGFILTER at 1100:0f10 and
  // ESCINFODLOGFILTER at 1100:1248.
  if (!g_tower_document) return 0;
  const auto text = simtower::original_transport_information_text(
      g_resources, *g_tower_document, target);
  if (!text.valid || target.dialog_id == 0U) return 0;

  const auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", target.dialog_id));
  const auto native = simtower::build_native_dialog_template(original);
  OriginalEventDialogContext context{};
  context.dtmp = simtower::parse_original_dtmp(
      g_resources.find("DTMP", target.dialog_id));
  // 1100:1b53/1cbb call 11e0:008d for DTMP rectangles 3 and 6, then
  // 1208:0cf5 adds the literal (+8,+1) text origin used above.
  context.text_overlays[0] = {2U, text.primary};
  context.text_overlays[1] = {5U, text.secondary};
  context.text_overlay_count = context.text_overlays.size();
  context.person_sprites = text.person_sprites;
  context.transport_target = target;
  const int result = static_cast<int>(run_original_modal_dialog(
      g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
      g_main_window, event_dialog_proc,
      reinterpret_cast<LPARAM>(&context)));
  release_native_dialog_brush(context.static_background);
  return result;
}

const simtower::OriginalTdtDocument* original_presentation_document(
    std::optional<simtower::OriginalTdtDocument>& storage) {
  if (!g_tower_document) return nullptr;
  if (!g_elevator_control_context ||
      !g_elevator_control_context->state.isolation_active) {
    return &*g_tower_document;
  }
  // 10f0 masks the shafts for dispatch, while 10f0:0121/01f9's b3ae-aware
  // presentation path continues drawing every visible floor/tenant through
  // 11a0:0000.
  // Reconstitute only the saved `used` bytes in a paint-only copy so the
  // native direct renderer preserves that surface during Simulate.
  storage = *g_tower_document;
  for (std::size_t index = 0U; index < storage->elevators.size(); ++index) {
    storage->elevators[index].used =
        g_elevator_control_context->state.saved_elevator_used[index];
  }
  return &*storage;
}

void present_original_info_surface_direct() {
  if (!g_info_window) return;
  if (HDC dc = GetDC(g_info_window)) {
    if (g_logical_palette) {
      SelectPalette(dc, g_logical_palette, FALSE);
      RealizePalette(dc);
    }
    // 1090:0683 enters 11e0:0e84 after palette realization and before the
    // unconditional 1118:073d clock/calendar painter.
    original_runtime_audio_pump();
    std::optional<simtower::OriginalTdtDocument> presentation_storage{};
    const auto* presentation_document =
        original_presentation_document(presentation_storage);
    // 1118:073d is unconditional; 1118:0143/0368/026a are latch-selected.
    // Redrawing the same retained BITMAP/320 content and all current fields is
    // the native field-latch equivalent and does not repaint the outer frame.
    simtower::draw_original_info(
        dc, g_resources, presentation_document, g_info_status.text);
    // 1090:0691 is the second direct checkpoint, before the three conditional
    // field-latch painters. Native's retained painter has already emitted the
    // current field values, but the Hotel latch/audio mutation still occurs
    // at this exact point.
    original_runtime_audio_pump();
    if (g_tower_document) {
      const auto checkout_presentation =
          simtower::consume_original_hotel_checkout_presentation(
              *g_tower_document);
      if (checkout_presentation.play_cash_sound && g_audio) {
        (void)g_audio->play_resource(10013U, 2U, 3U, GetTickCount());
      }
    }
    ReleaseDC(g_info_window, dc);
    // Direct 1090:06d2 checkpoint after the Info DC is released.
    original_runtime_audio_pump();
  }
}

void draw_original_command_contents(HDC dc, const RECT& client) {
  const auto raster = simtower::render_original_command_palette(
      g_resources, active_original_command_rating(g_active_command_rating),
      g_build_mode_enabled, g_command_mode,
      client.right - client.left, client.bottom - client.top,
      g_command_toggle_pressed);
  simtower::draw_original_command_raster(
      dc, raster, 0, simtower::kOriginalCommandPaintOffsetY);
}

[[nodiscard]] bool original_main_backing_matches_window() {
  if (!g_main_window || !g_original_main_backing.valid) return false;
  RECT client{};
  if (!GetClientRect(g_main_window, &client)) return false;
  const auto view = current_original_view(g_main_window);
  return g_original_main_backing.view == view &&
         g_original_main_backing.client_width == client.right - client.left &&
         g_original_main_backing.client_height == client.bottom - client.top;
}

bool advance_original_main_surface_state(
    simtower::OriginalMainSurfacePass pass) {
  if (!g_main_window || !g_tower_document) return false;
  const auto plan = simtower::original_main_surface_pass_plan(pass);
  if (!plan.advance_sky_decorations &&
      !plan.advance_visible_facility_people) {
    return false;
  }
  RECT client{};
  if (!GetClientRect(g_main_window, &client)) return false;
  const auto view = current_original_view(g_main_window);
  const int width = client.right - client.left;
  const int height = client.bottom - client.top;
  if (plan.advance_sky_decorations) {
    // 1080:0a1e calls 1048:03a3 only when its argument is nonzero. It precedes
    // 1038:050e and therefore consumes the shared Microsoft RNG first.
    (void)simtower::step_original_sky_decorations(
        g_resources, *g_tower_document, g_sky_decorations,
        view.x, view.y, width, height);
  }
  if (plan.advance_visible_facility_people) {
    const auto people = simtower::step_original_visible_facility_people(
        *g_tower_document, view.x, view.y, width, height,
        g_people_animation_enabled, g_world_control_modifier);
    // Every dispatched 1038:050e facility branch reaches its DS:31cc write,
    // even if the selected person bytes happen to retain their prior values.
    // This includes the negative pending-construction presentation path.
    return people.dispatched_tenants != 0U;
  }
  return false;
}

void rebuild_original_main_backing(
    simtower::OriginalMainSurfacePass pass,
    bool advance_state,
    bool full_frame_surface_dirty) {
  // 11f8:3b94 copies the original dirty tile scratch into DS:3264 before its
  // dynamic layers. Native derives the same retained RGB backing from current
  // document state because it has no separate indexed tile-scratch surface.
  if (!g_main_window) {
    g_original_main_backing.valid = false;
    return;
  }
  const auto plan = simtower::original_main_surface_pass_plan(pass);
  if (advance_state) (void)advance_original_main_surface_state(pass);

  if (plan.clear_elevator_transfer_visuals_before_rebuild) {
    // Every 1080:0a1e rebuild context first calls 10a8:0000, which resets all
    // two-sided transfer slots while rebuilding the per-floor x order. Native
    // derives that order directly during rendering, but must preserve the
    // original reset-before-draw position.
    g_elevator_transfer_visuals.clear();
  }

  RECT client{};
  if (!GetClientRect(g_main_window, &client)) {
    g_original_main_backing.valid = false;
    return;
  }
  const auto view = current_original_view(g_main_window);
  const int width = client.right - client.left;
  const int height = client.bottom - client.top;
  if (width <= 0 || height <= 0) {
    g_original_main_backing.valid = false;
    return;
  }

  std::optional<simtower::OriginalTdtDocument> presentation_storage{};
  const auto* presentation_document =
      original_presentation_document(presentation_storage);
  std::optional<simtower::OriginalElevatorWaitingIsolationView>
      waiting_isolation{};
  if (g_elevator_control_context &&
      g_elevator_control_context->state.isolation_active &&
      g_elevator_control_context->state.saved_elevator_record) {
    waiting_isolation = simtower::OriginalElevatorWaitingIsolationView{
        g_elevator_control_context->state.elevator_index,
        &*g_elevator_control_context->state.saved_elevator_record};
  }
  auto raster = simtower::render_original_world(
      g_resources, presentation_document,
      view.x, view.y, width, height, g_elevator_transfer_visuals,
      g_palette_runtime.initialized ? &g_palette_runtime.colors : nullptr,
      &g_sky_decorations, g_map_mode,
      waiting_isolation ? &*waiting_isolation : nullptr,
      pass == simtower::OriginalMainSurfacePass::simulation_frame
          ? std::function<void()>{[] { original_runtime_audio_pump(); }}
          : std::function<void()>{},
      full_frame_surface_dirty);
  if (g_find_marker.active()) {
    const auto derived_palette = g_palette_runtime.initialized
        ? simtower::OriginalWorldPalette{}
        : simtower::original_world_palette(
              g_resources, presentation_document);
    const auto& palette = g_palette_runtime.initialized
        ? g_palette_runtime.colors
        : derived_palette;
    simtower::composite_original_find_marker(
        g_resources, palette, g_find_marker.cell_x,
        g_find_marker.floor, view.x, view.y, raster);
  }
  const auto preview_rect = current_original_main_preview_rect();
  if (plan.update_construction_preview_rect) {
    g_original_main_preview_rect = preview_rect;
  }
  if (preview_rect) {
    // 1090:03df/05ee changes only the scratch construction rectangle. Native
    // retains its final clipped WHITE_PEN/NULL_BRUSH pixels in this cache.
    simtower::composite_original_construction_preview(
        g_selected_build_type, g_main_pointer.client_x,
        g_main_pointer.client_y, view.x, view.y, raster);
  }
  g_original_main_backing.raster = std::move(raster);
  g_original_main_backing.view = view;
  g_original_main_backing.client_width = width;
  g_original_main_backing.client_height = height;
  g_original_main_backing.valid = true;
  g_original_main_backing.dirty = false;

  if (plan.consume_elevator_transfer_visuals) {
    g_elevator_transfer_visuals.clear();
  }
}

void present_original_main_backing_direct() {
  if (!g_main_window) return;
  if (g_original_main_backing.dirty ||
      !original_main_backing_matches_window()) {
    // A native model mutation can invalidate the RGB transport cache without
    // authorizing either original presentation-state step. Rebuild pixels
    // from current state, then perform only 1158:0a3c's presentation role.
    rebuild_original_main_backing(
        simtower::OriginalMainSurfacePass::window_paint, false);
  }
  if (!g_original_main_backing.valid) return;
  if (HDC dc = GetDC(g_main_window)) {
    if (g_logical_palette) {
      SelectPalette(dc, g_logical_palette, FALSE);
      RealizePalette(dc);
    }
    simtower::draw_original_world_raster(
        dc, g_original_main_backing.raster);
    ReleaseDC(g_main_window, dc);
  }
}

void request_original_main_surface_pass(
    simtower::OriginalMainSurfacePass pass,
    bool synchronous) {
  g_main_pending_surface_pass = pass;
  g_original_main_backing.dirty = true;
  if (!g_main_window) return;
  invalidate_original_main_surface();
  if (synchronous) UpdateWindow(g_main_window);
}

void paint_known_original_surface(
    HWND window,
    HDC dc,
    simtower::OriginalMainSurfacePass pass =
        simtower::OriginalMainSurfacePass::window_paint) {
  RECT client{};
  GetClientRect(window, &client);
  if (window == g_main_window) {
    // 1158:0ae5's changed-rectangle blit and 1158:0ba8's client-band blit are
    // transport variants over the same DS:3264 pixels. Clipping the retained
    // raster through the supplied paint DC replaces both obsolete WinG paths.
    const auto plan = simtower::original_main_surface_pass_plan(pass);
    if (plan.rebuild_native_backing || g_original_main_backing.dirty ||
        !original_main_backing_matches_window()) {
      rebuild_original_main_backing(pass);
    }
    if (g_original_main_backing.valid) {
      simtower::draw_original_world_raster(
          dc, g_original_main_backing.raster);
    }
    return;
  }

  std::optional<simtower::OriginalTdtDocument> presentation_storage{};
  const auto* presentation_document =
      original_presentation_document(presentation_storage);

  simtower::fill_original_white_rect(dc, client);

  if (window == g_command_window) {
    simtower::draw_original_palette_frame(
        dc, simtower::kOriginalCommandSurfaceWidth,
        GetActiveWindow() == window);
  } else if (window == g_info_window) {
    simtower::draw_original_palette_frame(
        dc, simtower::kOriginalInfoWidth, GetActiveWindow() == window);
  } else if (window == g_map_window) {
    simtower::draw_original_palette_frame(
        dc, simtower::kOriginalMapWidth, GetActiveWindow() == window);
  }

  if (window == g_command_window) {
    draw_original_command_contents(dc, client);
  } else if (window == g_info_window) {
    simtower::draw_original_info(
        dc, g_resources,
        presentation_document,
        g_info_status.text);
  } else if (window == g_map_window) {
    // 1168:0326 XOR-erases the retained DS:7796 focus after the frame and
    // before the Map backing work. The later background blit overwrites that
    // intermediate result; 03bf then XOR-draws the same retained rectangle.
    if (!g_original_map_focus_rectangle && g_main_window) {
      RECT main_client{};
      GetClientRect(g_main_window, &main_client);
      const auto view = current_original_view(g_main_window);
      g_original_map_focus_rectangle = simtower::original_map_view_rect(
          view.x, view.y, main_client.right - main_client.left,
          main_client.bottom - main_client.top);
    }
    if (g_original_map_focus_rectangle) {
      draw_original_map_focus_rectangle(
          dc, *g_original_map_focus_rectangle);
    }
    // 1168:032f follows the first focus/frame pass and precedes the four
    // toolbar cells plus 1160:01dc backing-surface composition.
    original_runtime_audio_pump();
    const bool disabled =
        g_tower_document && g_tower_document->header.rating == 1U;
    const auto raster = simtower::render_original_map(
        g_resources, presentation_document,
        g_map_mode, disabled,
        g_palette_runtime.initialized ? &g_palette_runtime.colors : nullptr);
    // 1168:0388 services WAVMIX immediately before the backing blit.
    original_runtime_audio_pump();
    simtower::draw_original_world_raster(
        dc, raster, 0, simtower::kOriginalMapClientTop);

    // 1168:03bf repeats 1058:094c with the same retained rectangle. A camera
    // caller subsequently executes 1080:055d to exchange it for the newly
    // derived scroll rectangle; an ordinary Map exposure does not.
    if (g_original_map_focus_rectangle) {
      draw_original_map_focus_rectangle(
          dc, *g_original_map_focus_rectangle);
    }
    // 1168:03c8 is the painter's final operation after the focus rectangle.
    original_runtime_audio_pump();
  }
}

LRESULT paint_window(
    HWND window,
    simtower::OriginalMainSurfacePass pass =
        simtower::OriginalMainSurfacePass::window_paint) {
  if (window == g_main_window &&
      simtower::original_main_surface_pass_plan(pass)
          .draw_scroll_floor_label) {
    // 1080:0b26 draws the viewport-center floor label directly into the
    // vertical scrollbar's up-arrow before 1080:0a1e paints the client. The
    // independent 1158:00da exposure path does not call this helper.
    RECT client{};
    RECT outer{};
    if (GetClientRect(window, &client) && GetWindowRect(window, &outer)) {
      const auto view = current_original_view(window);
      const auto label = simtower::original_scroll_floor_label(
          view.y, client.bottom - client.top);
      const std::wstring wide(label.begin(), label.end());
      RECT indicator{2, 0, 20, 15};
      OffsetRect(&indicator,
                 outer.right - outer.left - 16 -
                     2 * GetSystemMetrics(SM_CXFRAME),
                 GetSystemMetrics(SM_CYCAPTION) +
                     GetSystemMetrics(SM_CYFRAME));
      if (HDC window_dc = GetWindowDC(window)) {
        if (HBRUSH brush = CreateSolidBrush(GetSysColor(COLOR_MENU))) {
          FillRect(window_dc, &indicator, brush);
          DeleteObject(brush);
        }
        const HGDIOBJ old_font =
            SelectObject(window_dc, GetStockObject(SYSTEM_FONT));
        const COLORREF old_background =
            SetBkColor(window_dc, GetSysColor(COLOR_MENU));
        DrawTextW(window_dc, wide.c_str(), static_cast<int>(wide.size()),
                  &indicator, DT_CENTER);
        SelectObject(window_dc, old_font);
        SetBkColor(window_dc, old_background);
        ReleaseDC(window, window_dc);
      }
    }
  }
  PAINTSTRUCT paint{};
  HDC dc = BeginPaint(window, &paint);
  if (dc) {
    // MAINWNDPROC 1158:00da->0a3c, CMDBTNWNDPROC 1050:010d, INFOWNDPROC
    // 1120:0215, and MAPWNDPROC 1168:02be all select and realize the one
    // logical palette in their paint DC before presenting indexed content.
    if (g_logical_palette) {
      SelectPalette(dc, g_logical_palette, FALSE);
      RealizePalette(dc);
    }
    paint_known_original_surface(window, dc, pass);
  }
  EndPaint(window, &paint);
  return 0;
}

void install_original_scrollbar_state(
    HWND window,
    int bar,
    const simtower::OriginalMainScrollbarResizeState& state,
    BOOL redraw) {
  SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
  info.nMin = state.minimum;
  info.nMax = state.maximum;
  info.nPage = state.native_page_size;
  info.nPos = state.position;
  SetScrollInfo(window, bar, &info, redraw);
}

void redraw_original_scrollbar_position(HWND window, int bar, int position) {
  SCROLLINFO info{sizeof(info), SIF_POS};
  info.nPos = position;
  SetScrollInfo(window, bar, &info, TRUE);
}

simtower::OriginalMainWindowGeometry current_original_main_window_geometry() {
  RECT desktop{};
  GetWindowRect(GetDesktopWindow(), &desktop);
  return simtower::original_main_window_geometry(
      desktop.right, desktop.bottom,
      {
          .frame_width = GetSystemMetrics(SM_CXFRAME),
          .frame_height = GetSystemMetrics(SM_CYFRAME),
          .vertical_scroll_width = GetSystemMetrics(SM_CXVSCROLL),
          .horizontal_scroll_height = GetSystemMetrics(SM_CYHSCROLL),
          .menu_height = GetSystemMetrics(SM_CYMENU),
          .caption_height = GetSystemMetrics(SM_CYCAPTION),
          .border_width = GetSystemMetrics(SM_CXBORDER),
          .border_height = GetSystemMetrics(SM_CYBORDER),
      });
}

void scroll_original(HWND window, int bar, WPARAM wparam, int world_extent) {
  SCROLLINFO info{sizeof(info), SIF_ALL};
  if (!GetScrollInfo(window, bar, &info)) {
    return;
  }
  RECT client{};
  GetClientRect(window, &client);
  const int client_extent =
      bar == SB_HORZ ? client.right - client.left : client.bottom - client.top;
  // Win16 carries the thumb position in LOWORD(lParam); Win32 carries the
  // equivalent 16-bit field in HIWORD(wParam). Reading nTrackPos here loses
  // the exact SB_THUMBPOSITION value delivered on button release.
  const auto position = simtower::original_main_scroll_request_position(
      LOWORD(wparam), info.nPos, static_cast<std::uint16_t>(HIWORD(wparam)),
      client_extent, world_extent - client_extent);
  if (!position) {
    return;
  }
  info.fMask = SIF_POS;
  info.nPos = *position;
  SetScrollInfo(window, bar, &info, TRUE);
  // 1058:05f8 immediately performs Main's complete 1080:0a1e(1) rebuild and
  // then only 1080:055d's direct Map-focus XOR exchange. A full MAPWNDPROC
  // repaint here would add backing, frame, and audio-pump work absent from the
  // recovered scrollbar path.
  for (const auto step : simtower::original_scrollbar_refresh_order()) {
    switch (step) {
      case simtower::OriginalScrollbarRefreshStep::main_rebuild_with_sky:
        request_original_main_surface_pass(
            simtower::OriginalMainSurfacePass::rebuild_with_sky, true);
        break;
      case simtower::OriginalScrollbarRefreshStep::map_focus_adjustment:
        if (window == g_main_window) {
          adjust_original_derived_map_focus_synchronously();
        }
        break;
    }
  }
}

void select_original_map_mode(std::uint16_t mode) {
  const std::uint16_t maximum =
      g_tower_document && g_tower_document->header.rating == 1U ? 2U : 3U;
  if (mode > maximum) return;

  // 11d0:0000 couples every map overlay selection to edit mode two. Mode
  // zero restores construction; modes one through three disable it.
  g_map_mode = mode;
  g_build_mode_enabled = mode == 0U;
  g_command_mode = 2U;
  if (mode == 1U && g_tower_document) {
    // 11d0:0031-0039 invokes the complete 1130:00b5 all-tenant
    // satisfaction refresh before invalidating the Map and world surfaces.
    simtower::refresh_original_map_tenant_satisfaction(
        *g_tower_document, g_part);
  }
  if (mode == 0U) {
    // 11d0:001d clears the transient Find target while restoring
    // construction mode.
    simtower::reset_original_find_marker(g_find_marker);
  }
  InvalidateRect(g_map_window, nullptr, FALSE);
  InvalidateRect(g_command_window, nullptr, FALSE);
  // 11d0:0066 supplies zero: the overlay redraw includes facility people but
  // deliberately leaves the 1048:03a3 sky-decoration state untouched.
  request_original_main_surface_pass(
      simtower::OriginalMainSurfacePass::rebuild_without_sky, true);
}

void apply_original_construction_toggle() {
  const auto plan = simtower::original_construction_toggle_plan(
      g_build_mode_enabled, g_map_mode);
  if (plan.path ==
      simtower::OriginalConstructionTogglePath::exit_map_overlay) {
    // 1058:0352 clears DS:7840 first and delegates the complete repaint/state
    // transaction to 11d0:0000, bypassing the ordinary WAVMIX branch.
    select_original_map_mode(0U);
    return;
  }

  g_build_mode_enabled = plan.build_mode_after;
  if (plan.force_command_mode_two) g_command_mode = 2U;
  if (plan.reset_find_marker) {
    simtower::reset_original_find_marker(g_find_marker);
  }
  if (g_audio) {
    if (plan.audio == simtower::OriginalIdleAudioTransition::activate) {
      g_audio->activate();
    } else if (plan.audio ==
               simtower::OriginalIdleAudioTransition::deactivate) {
      g_audio->deactivate();
    }
  }
  if (plan.refresh_command_synchronously && g_command_window) {
    InvalidateRect(g_command_window, nullptr, FALSE);
    UpdateWindow(g_command_window);
  }
  if (plan.restore_preview_scratch) {
    // 11f8:3b94 clears DS:77ac after restoring the retained old outline.
    g_original_main_preview_rect.reset();
  }
  if (plan.present_main_synchronously) {
    request_original_main_surface_pass(
        simtower::OriginalMainSurfacePass::preview_repaint, true);
  }
}

void update_original_map_drag(LPARAM position) {
  // Exact 1058:0284 Map content drag: remove the eight-pixel client frame,
  // map the clicked overview point to the centered world viewport, and publish
  // both scroll axes synchronously through the native view boundary.
  if (!g_main_window) return;
  const int map_x = static_cast<std::int16_t>(LOWORD(position));
  const int map_y = static_cast<std::int16_t>(HIWORD(position)) -
                    simtower::kOriginalMapClientTop;
  if (map_x < 0 || map_x >= simtower::kOriginalMapWidth ||
      map_y < simtower::kOriginalMapToolbarHeight ||
      map_y >= simtower::kOriginalMapBackingHeight) {
    return;
  }
  RECT main_client{};
  GetClientRect(g_main_window, &main_client);
  const auto view = current_original_view(g_main_window);
  const auto centered = simtower::original_map_centered_view(
      map_x, map_y, view.x, view.y,
      main_client.right - main_client.left,
      main_client.bottom - main_client.top);
  // 1058:06df returns false when EQUALRECT says the focus rectangle is
  // unchanged. 0284 then skips both scroll publication and its two XOR focus
  // draws, so do not invalidate either native surface on that no-op path.
  if (centered == view) return;

  // 1058:02d7-032e pumps, XOR-erases the old Map focus, commits/clamps both
  // scroll positions, XOR-draws the new focus, pumps, presents Main through
  // 1080:0a1e(1), releases the Map DC, and pumps once more in that order.
  original_runtime_audio_pump();
  HDC map_dc = g_map_window ? GetDC(g_map_window) : nullptr;
  const auto focus_for_view = [&](simtower::OriginalWorldPoint focus_view) {
    return simtower::original_map_view_rect(
        focus_view.x, focus_view.y,
        main_client.right - main_client.left,
        main_client.bottom - main_client.top);
  };
  const auto draw_focus = [&](const simtower::OriginalMapRect& focus) {
    if (!map_dc) return;
    draw_original_map_focus_rectangle(map_dc, focus);
  };
  const auto previous_focus = g_original_map_focus_rectangle.value_or(
      focus_for_view(view));
  draw_focus(previous_focus);

  SCROLLINFO info{sizeof(info), SIF_POS};
  info.nPos = centered.x;
  SetScrollInfo(g_main_window, SB_HORZ, &info, TRUE);
  info.nPos = centered.y;
  SetScrollInfo(g_main_window, SB_VERT, &info, TRUE);
  const auto updated_focus =
      focus_for_view(current_original_view(g_main_window));
  draw_focus(updated_focus);
  g_original_map_focus_rectangle = updated_focus;

  original_runtime_audio_pump();
  request_original_main_surface_pass(
      simtower::OriginalMainSurfacePass::rebuild_with_sky, true);
  if (map_dc) ReleaseDC(g_map_window, map_dc);
  original_runtime_audio_pump();
}

HWND original_palette_surface_window(
    simtower::OriginalPaletteSurface surface) noexcept {
  switch (surface) {
    case simtower::OriginalPaletteSurface::map:
      return g_map_window;
    case simtower::OriginalPaletteSurface::info:
      return g_info_window;
    case simtower::OriginalPaletteSurface::command:
      return g_command_window;
    case simtower::OriginalPaletteSurface::main:
      return g_main_window;
  }
  return nullptr;
}

std::optional<simtower::OriginalPaletteSurface>
original_palette_surface_from_window(HWND window) noexcept {
  if (window == g_map_window) return simtower::OriginalPaletteSurface::map;
  if (window == g_info_window) return simtower::OriginalPaletteSurface::info;
  if (window == g_command_window) {
    return simtower::OriginalPaletteSurface::command;
  }
  if (window == g_main_window) return simtower::OriginalPaletteSurface::main;
  return std::nullopt;
}

void paint_original_palette_surface_direct(
    simtower::OriginalPaletteSurface surface,
    HWND window,
    HDC dc) {
  if (surface == simtower::OriginalPaletteSurface::command) {
    RECT client{};
    GetClientRect(window, &client);
    // 1158:0d0e-0d56 blits only the Command backing content at y=8; the
    // nonclient-like eight-pixel frame is deliberately not repainted here.
    draw_original_command_contents(dc, client);
    return;
  }
  if (surface == simtower::OriginalPaletteSurface::map) {
    // Direct 1158:0c29 -> 1168:02be entry retains the painter's leading pump.
    original_runtime_audio_pump();
  }
  if (g_logical_palette) {
    // Both direct targets enter their ordinary presentation helper, which
    // repeats SelectPalette/RealizePalette even when 0c29 supplied the DC.
    SelectPalette(dc, g_logical_palette, FALSE);
    RealizePalette(dc);
  }
  // Map enters 1168:02be and Main enters 1158:0a3c directly. The palette pass
  // renders current colors without replaying ordinary WM_PAINT's RNG/person
  // presentation step or consuming the one-frame Elevator transfer cache.
  paint_known_original_surface(
      window, dc, simtower::OriginalMainSurfacePass::palette_repaint);
}

void repaint_original_palette_surfaces(HWND source,
                                       HDC source_dc,
                                       bool realized_entries_changed) {
  const auto source_surface = original_palette_surface_from_window(source);
  if (!source_surface) return;
  for (const auto& action : simtower::original_palette_repaint_actions(
           g_main_message_runtime_initialized, realized_entries_changed,
           *source_surface)) {
    const HWND target = original_palette_surface_window(action.surface);
    if (!target) continue;

    if (action.mechanism ==
        simtower::OriginalPaletteRepaintMechanism::update_window) {
      InvalidateRect(target, nullptr, FALSE);
      UpdateWindow(target);
      continue;
    }

    HDC target_dc = action.reuse_source_dc ? source_dc : GetDC(target);
    if (!target_dc) continue;
    if (action.select_and_realize) {
      SelectPalette(target_dc, g_logical_palette, FALSE);
      RealizePalette(target_dc);
    }
    if (action.invalidate) {
      RECT client{};
      GetClientRect(target, &client);
      InvalidateRect(target, &client, FALSE);
    }
    paint_original_palette_surface_direct(
        action.surface, target, target_dc);
    if (action.release_dc) {
      ReleaseDC(target, target_dc);
    }
  }
}

bool realize_original_palette_message(HWND source) {
  if (!source || !g_logical_palette) return false;
  const HDC dc = GetDC(source);
  if (!dc) return false;
  SelectPalette(dc, g_logical_palette, FALSE);
  const bool changed = RealizePalette(dc) != 0U;
  if (changed) {
    repaint_original_palette_surfaces(source, dc, true);
  }
  ReleaseDC(source, dc);
  return changed;
}

bool realize_original_command_selector_palette(HWND dialog,
                                               bool update_colors_when_changed) {
  if (!dialog || !g_logical_palette) return false;
  const HDC dc = GetDC(dialog);
  if (!dc) return false;
  SelectPalette(dc, g_logical_palette, FALSE);
  const bool changed = RealizePalette(dc) != 0U;
  if (changed && update_colors_when_changed) {
    // Exact GDI!UPDATECOLORS call at 1050:0937, using the still-live DC that
    // realized the palette rather than scheduling a replacement WM_PAINT.
    UpdateColors(dc);
  }
  ReleaseDC(dialog, dc);
  return changed;
}

void apply_original_palette_window_activation(
    HWND window,
    simtower::OriginalAuxiliaryWindow kind,
    bool active) {
  const auto plan = simtower::original_palette_window_activation_plan(
      kind, active, g_original_active_modal_window != nullptr,
      g_toolbar_visible, g_original_window_activation_latch);
  if (plan.insert_behind_modal) {
    // MAPWNDPROC 1168:0071 uses literal flags 0x13 and performs this before
    // forwarding a nonzero activation to the active DS:31a4 modal.
    SetWindowPos(window, g_original_active_modal_window, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
  }
  if (plan.promote_command_topmost) {
    UINT flags = SWP_NOSIZE | SWP_NOMOVE;
    if (plan.command_promotion_no_activate) flags |= SWP_NOACTIVATE;
    SetWindowPos(g_command_window, HWND_TOPMOST, 0, 0, 0, 0, flags);
  }
  if (plan.activate_modal) {
    SetActiveWindow(g_original_active_modal_window);
  }
  if (plan.focus_modal) {
    SetFocus(g_original_active_modal_window);
  }
  if (plan.validate_client) {
    // CMDBTNWNDPROC 1050:008d, INFOWNDPROC 1120:007d, and MAPWNDPROC
    // 1168:009f replace RECT.bottom with eight and validate only the palette
    // title strip before writing DS:31a6; content updates remain pending.
    RECT client{};
    if (GetClientRect(window, &client)) {
      client = simtower::original_palette_activation_validation_rect(client);
      ValidateRect(window, &client);
    }
  }
  if (plan.write_shared_activation_latch) {
    g_original_window_activation_latch = plan.shared_activation_latch;
  }
}

int original_auxiliary_surface_width(
    simtower::OriginalAuxiliaryWindow kind) noexcept {
  if (kind == simtower::OriginalAuxiliaryWindow::command) {
    return simtower::kOriginalCommandSurfaceWidth;
  }
  if (kind == simtower::OriginalAuxiliaryWindow::info) {
    return simtower::kOriginalInfoWidth;
  }
  return simtower::kOriginalMapWidth;
}

void paint_original_auxiliary_activation_frame(
    HWND window,
    simtower::OriginalAuxiliaryWindow kind,
    bool active) {
  // Exact immediate 1078:00c6 boundary used by 1050:00f8, 1120:00a5, and
  // 1168:00c7. WM_NCACTIVATE repaints only the eight-pixel client title strip;
  // it does not invalidate or rebuild the rest of the palette surface.
  if (HDC dc = GetDC(window)) {
    if (g_logical_palette) {
      SelectPalette(dc, g_logical_palette, FALSE);
      RealizePalette(dc);
    }
    simtower::draw_original_palette_frame(
        dc, original_auxiliary_surface_width(kind), active);
    ReleaseDC(window, dc);
  }
}

LRESULT paint_original_auxiliary_window(
    HWND window,
    simtower::OriginalAuxiliaryWindow kind) {
  const bool visible = kind == simtower::OriginalAuxiliaryWindow::command
      ? g_toolbar_visible
      : kind == simtower::OriginalAuxiliaryWindow::info
          ? g_info_visible
          : g_map_visible;
  const auto plan = simtower::original_auxiliary_paint_plan(
      kind, visible, g_main_message_runtime_initialized, g_main_closing);
  if (plan.invalidate_entire_client) {
    InvalidateRect(window, nullptr, FALSE);
  }

  PAINTSTRUCT paint{};
  HDC dc = BeginPaint(window, &paint);
  if (dc && plan.realize_palette) {
    if (kind == simtower::OriginalAuxiliaryWindow::map) {
      // 1168:02cd is before SelectPalette; Info's 1120:026f checkpoint is
      // instead between SelectPalette and RealizePalette.
      original_runtime_audio_pump();
    }
    if (g_logical_palette) SelectPalette(dc, g_logical_palette, FALSE);
    if (kind == simtower::OriginalAuxiliaryWindow::info) {
      original_runtime_audio_pump();
    }
    if (g_logical_palette) RealizePalette(dc);
  }
  if (dc && plan.draw_content) {
    paint_known_original_surface(window, dc);
  }
  EndPaint(window, &paint);
  return 0;
}

// Shared native transport for the distinct INFOWNDPROC 1120:0000 and
// MAPWNDPROC 1168:0000 tables. The wrappers below preserve procedure identity
// even during synchronous creation messages, before their global HWNDs exist.
LRESULT auxiliary_window_proc_for_kind(
    HWND window,
    UINT message,
    WPARAM wparam,
    LPARAM lparam,
    simtower::OriginalAuxiliaryWindow kind) {
  switch (message) {
    case WM_CREATE:
      // MAPWNDPROC's first 1168:028a entry returns directly at 027b. Info has
      // no corresponding entry and therefore continues to DefWindowProc.
      if (kind == simtower::OriginalAuxiliaryWindow::map) return 0;
      break;
    case WM_PAINT:
      return paint_original_auxiliary_window(window, kind);
    case WM_DESTROY:
      // INFOWNDPROC 1120:0167 and MAPWNDPROC 1168:0203 both post WM_QUIT.
      PostQuitMessage(0);
      return 0;
    case WM_QUERYNEWPALETTE:
    case WM_PALETTECHANGED: {
      const auto plan = simtower::original_auxiliary_palette_message_plan(
          message == WM_QUERYNEWPALETTE
              ? simtower::OriginalPaletteMessageKind::query_new_palette
              : simtower::OriginalPaletteMessageKind::palette_changed,
          message == WM_PALETTECHANGED &&
              reinterpret_cast<HWND>(wparam) == window,
          g_main_message_runtime_initialized, g_main_closing);
      if (plan.realize) {
        (void)realize_original_palette_message(window);
      }
      return static_cast<LRESULT>(plan.result);
    }
    case WM_NCHITTEST: {
      POINT point{static_cast<std::int16_t>(LOWORD(lparam)),
                  static_cast<std::int16_t>(HIWORD(lparam))};
      ScreenToClient(window, &point);
      const auto hit =
          simtower::original_palette_frame_hit_test(point.x, point.y);
      if (hit == simtower::OriginalPaletteFrameHit::close) return HTCLIENT;
      if (hit == simtower::OriginalPaletteFrameHit::drag) return HTCAPTION;
      break;
    }
    case WM_NCACTIVATE:
      paint_original_auxiliary_activation_frame(window, kind,
                                                wparam != FALSE);
      return TRUE;
    case WM_ACTIVATE:
      apply_original_palette_window_activation(
          window, kind,
          LOWORD(wparam) != WA_INACTIVE);
      return 0;
    case WM_ACTIVATEAPP:
      apply_original_palette_app_activation(wparam != 0U);
      return 0;
    case WM_LBUTTONDOWN:
      if (simtower::original_palette_frame_hit_test(
              static_cast<std::int16_t>(LOWORD(lparam)),
              static_cast<std::int16_t>(HIWORD(lparam))) ==
          simtower::OriginalPaletteFrameHit::close) {
        bool* visible = kind == simtower::OriginalAuxiliaryWindow::info
            ? &g_info_visible
            : &g_map_visible;
        const auto plan = simtower::original_auxiliary_visibility_plan(
            simtower::OriginalAuxiliaryVisibilityTrigger::close_box,
            *visible);
        *visible = plan.visible;
        if (plan.operation ==
            simtower::OriginalAuxiliaryWindowOperation::hide) {
          ShowWindow(window, SW_HIDE);
        }
        return 0;
      }
      if (kind == simtower::OriginalAuxiliaryWindow::map) {
        const auto pointer_plan = simtower::original_map_pointer_message_plan(
            simtower::OriginalMapPointerMessage::button_down,
            g_map_drag_active, true);
        if (pointer_plan.set_pointer_down) {
          g_original_map_pointer_down = true;
        }
        // Exact 1058:01d6 mouse-down split: 1058:085c's four 50x18 toolbar
        // rectangles select a mode; the content area captures and calls 0284.
        const int x = static_cast<std::int16_t>(LOWORD(lparam));
        const int map_y = static_cast<std::int16_t>(HIWORD(lparam)) -
                          simtower::kOriginalMapClientTop;
        const std::uint16_t rating = g_tower_document
            ? g_tower_document->header.rating
            : 1U;
        if (const auto mode = simtower::original_map_toolbar_mode_at(
                x, map_y, rating)) {
          select_original_map_mode(*mode);
        } else if (x >= 0 && x < simtower::kOriginalMapWidth &&
                   map_y >= simtower::kOriginalMapToolbarHeight &&
                   map_y < simtower::kOriginalMapBackingHeight) {
          g_map_drag_active = true;
          SetCapture(window);
          update_original_map_drag(lparam);
        }
        return 0;
      }
      break;
    case WM_MOUSEMOVE:
      if (kind == simtower::OriginalAuxiliaryWindow::map) {
        const auto plan = simtower::original_map_pointer_message_plan(
            simtower::OriginalMapPointerMessage::mouse_move,
            g_map_drag_active, (wparam & MK_LBUTTON) != 0U);
        if (plan.update_drag) update_original_map_drag(lparam);
        if (plan.consume_message) return 0;
      }
      break;
    case WM_LBUTTONUP:
      if (kind == simtower::OriginalAuxiliaryWindow::map) {
        // MAPWNDPROC 1168:0156 clears both pointer/drag words and releases
        // capture on every Map button-up, not only after a content drag.
        const auto plan = simtower::original_map_pointer_message_plan(
            simtower::OriginalMapPointerMessage::button_up,
            g_map_drag_active, false);
        if (plan.clear_pointer_down) g_original_map_pointer_down = false;
        if (plan.clear_drag) g_map_drag_active = false;
        if (plan.release_capture) ReleaseCapture();
        if (plan.consume_message) return 0;
      }
      break;
    default:
      break;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK info_window_proc(HWND window, UINT message,
                                  WPARAM wparam, LPARAM lparam) {
  return auxiliary_window_proc_for_kind(
      window, message, wparam, lparam,
      simtower::OriginalAuxiliaryWindow::info);
}

LRESULT CALLBACK map_window_proc(HWND window, UINT message,
                                 WPARAM wparam, LPARAM lparam) {
  return auxiliary_window_proc_for_kind(
      window, message, wparam, lparam,
      simtower::OriginalAuxiliaryWindow::map);
}

struct OriginalCommandSelectorContext {
  simtower::OriginalCommandGroup group{};
  std::uint16_t choice{};
  std::uint16_t highlight{};
  int anchor_left{};
  int anchor_top{};
};

OriginalCommandSelectorContext* command_selector_context(HWND dialog) {
  return reinterpret_cast<OriginalCommandSelectorContext*>(
      GetWindowLongPtrW(dialog, GWLP_USERDATA));
}

void close_original_command_selector(HWND dialog, INT_PTR result) {
  ClipCursor(nullptr);
  if (GetCapture() == dialog) {
    ReleaseCapture();
  }
  EndDialog(dialog, result);
}

bool original_selector_point(HWND dialog, LPARAM lparam, int& row) {
  const int x = static_cast<std::int16_t>(LOWORD(lparam));
  const int y = static_cast<std::int16_t>(HIWORD(lparam));
  RECT client{};
  GetClientRect(dialog, &client);
  POINT point{x, y};
  if (!PtInRect(&client, point)) {
    return false;
  }
  row = y / 32;
  return true;
}

// Native modal equivalent of CMDBTNSUBWNDPROC at 1050:05a7.
INT_PTR CALLBACK command_selector_dialog_proc(HWND dialog,
                                               UINT message,
                                               WPARAM wparam,
                                               LPARAM lparam) {
  if (message == WM_INITDIALOG) {
    auto* context = reinterpret_cast<OriginalCommandSelectorContext*>(lparam);
    SetWindowLongPtrW(dialog, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(context));
    // 1050:05d6 uses Win16 GCL_HCURSOR (-12), not GCL_HBRBACKGROUND, and
    // immediately follows it with 11e0:0d80(0)'s standard arrow cursor.
    SetClassLongPtrW(dialog, GCLP_HCURSOR, 0);
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));

    const int height = static_cast<int>(context->group.catalog_icons.size()) * 32;
    RECT desktop{};
    GetWindowRect(GetDesktopWindow(), &desktop);
    const int top = simtower::original_command_selector_top(
        context->anchor_top, context->choice,
        context->group.catalog_icons.size(), desktop.bottom);
    MoveWindow(dialog, context->anchor_left, top, 32, height, FALSE);
    SetCapture(dialog);
    RECT clip{};
    GetWindowRect(dialog, &clip);
    ClipCursor(&clip);
    InvalidateRect(dialog, nullptr, FALSE);
    return TRUE;
  }

  auto* context = command_selector_context(dialog);
  if (!context) {
    return FALSE;
  }
  switch (message) {
    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(dialog, &paint);
      if (dc) {
        const auto raster = simtower::render_original_command_selector(
            g_resources, context->group, context->highlight,
            g_build_mode_enabled);
        simtower::draw_original_command_raster(dc, raster, 0, 0);
      }
      EndPaint(dialog, &paint);
      return TRUE;
    }
    case WM_MOUSEMOVE: {
      int row = 0;
      if (!original_selector_point(dialog, lparam, row)) {
        close_original_command_selector(dialog, 0);
        // 1050:07b9 closes through the common FALSE return, unlike every
        // in-bounds mouse route.
        return FALSE;
      }
      const std::uint16_t highlight = static_cast<std::uint16_t>(
          context->group.catalog_icons[static_cast<std::size_t>(row)] + 1U);
      if (highlight != context->highlight) {
        context->highlight = highlight;
        InvalidateRect(dialog, nullptr, FALSE);
        UpdateWindow(dialog);
      }
      return TRUE;
    }
    case WM_LBUTTONDOWN: {
      int row = 0;
      if (!original_selector_point(dialog, lparam, row)) {
        close_original_command_selector(dialog, 0);
        return TRUE;
      }
      context->choice = static_cast<std::uint16_t>(row + 1);
      // 1050:080c-0836 commits on button-down but, unlike the release route,
      // does not explicitly unclip the pointer or release capture first.
      EndDialog(dialog, 1);
      return TRUE;
    }
    case WM_LBUTTONUP: {
      int row = 0;
      if (!original_selector_point(dialog, lparam, row)) {
        close_original_command_selector(dialog, 0);
        return TRUE;
      }
      context->choice = static_cast<std::uint16_t>(row + 1);
      close_original_command_selector(dialog, 1);
      return TRUE;
    }
    case WM_RBUTTONDOWN:
      close_original_command_selector(dialog, 0);
      return TRUE;
    case WM_PALETTECHANGED: {
      const auto plan =
          simtower::original_command_selector_palette_changed_plan(
              reinterpret_cast<HWND>(wparam) == dialog);
      if (plan.realize) {
        (void)realize_original_command_selector_palette(
            dialog, plan.update_colors_when_changed);
      }
      return static_cast<INT_PTR>(plan.result);
    }
  }
  return FALSE;
}

void run_original_command_selector(
    simtower::OriginalCommandRatingState& state,
    std::size_t catalog_index) {
  // Native transaction for 1050:0533 and 1058:04e0: launch the modal command
  // selector, resolve TABM group/current choice,
  // present CMDBTNSUBWNDPROC only while selecting, persist an accepted choice,
  // then let the caller resolve TABL/1000's construction type.
  const auto group = simtower::original_command_group(
      g_resources, state, catalog_index);
  if (!group) {
    return;
  }

  // 1058:0517-053f opens the captured selector only while the physical
  // primary button is down. This matters when CMDBTNWNDPROC's button-up path
  // re-enters a grouped command after a cancelled drag: the original keeps
  // the current choice instead of opening a second modal popup. Query the
  // equivalent native setting without temporarily changing the user's global
  // SwapMouseButton preference as the Win16 implementation did.
  const bool buttons_swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;
  const int primary_button = static_cast<int>(
      simtower::original_command_primary_button_virtual_key(buttons_swapped));
  const bool primary_button_down = GetAsyncKeyState(primary_button) < 0;
  if (!simtower::original_command_selector_transaction_plan(
           primary_button_down, false)
           .show_modal) {
    return;
  }

  const auto cell = simtower::original_command_facility_rect(
      static_cast<std::uint16_t>(catalog_index));
  RECT command_outer{};
  GetWindowRect(g_command_window, &command_outer);
  OriginalCommandSelectorContext context{
      *group, group->selection_index, group->selection_index,
      command_outer.left + cell.left, command_outer.top + cell.top};

  const auto original = simtower::parse_original_dialog(
      g_resources.find("DIALOG", 124));
  const auto native = simtower::build_native_dialog_template(original);
  const INT_PTR result = run_original_modal_dialog(
      g_instance, reinterpret_cast<const DLGTEMPLATE*>(native.data()),
      g_command_window, command_selector_dialog_proc,
      reinterpret_cast<LPARAM>(&context));
  if (simtower::original_command_selector_transaction_plan(
          primary_button_down, result != 0)
          .write_choice) {
    simtower::original_command_select_group_choice(
        g_resources, state, catalog_index, context.choice);
  }
}

void activate_original_command_point(HWND window, LPARAM position) {
  // 1058:03a9 ignores command activation while 10f0's isolated Elevator
  // simulation owns the mutable world, although the pressed frame above may
  // still be presented by CMDBTNWNDPROC.
  if (g_elevator_control_context &&
      g_elevator_control_context->state.isolation_active) {
    return;
  }
  const int x = static_cast<std::int16_t>(LOWORD(position));
  const int y = static_cast<std::int16_t>(HIWORD(position));
  auto& rating_state =
      active_original_command_rating(g_active_command_rating);
  const auto hit = simtower::original_command_hit_test(
      g_resources, rating_state, g_build_mode_enabled, x, y);
  switch (hit.kind) {
    case simtower::OriginalCommandHitKind::build_toggle:
      apply_original_construction_toggle();
      return;
    case simtower::OriginalCommandHitKind::edit_mode:
      g_command_mode = hit.mode_index;
      // 1058:0439 calls 1118:0ad5(0) when an edit tool is chosen.
      show_original_command_status(0U);
      break;
    case simtower::OriginalCommandHitKind::facility:
      g_command_mode = hit.mode_index;
      run_original_command_selector(
          rating_state, static_cast<std::size_t>(hit.mode_index - 3U));
      g_selected_build_type = simtower::original_command_build_type(
          g_resources,
          simtower::original_command_catalog(g_resources, rating_state)
              [hit.mode_index - 3U]);
      // 11f8:0f63 loads TABL/1000's raw type byte and calls
      // 1118:0ad5(type+1), selecting the matching STRL/1009 cost line.
      show_original_command_status(
          static_cast<std::uint16_t>(g_selected_build_type + 1U));
      break;
    case simtower::OriginalCommandHitKind::none:
      return;
  }
  InvalidateRect(window, nullptr, FALSE);
  invalidate_original_main_surface();
  InvalidateRect(g_map_window, nullptr, FALSE);
}

// Native message boundary for CMDBTNWNDPROC at 1050:0000.
LRESULT CALLBACK command_window_proc(HWND window, UINT message,
                                     WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_CREATE:
      // The first entry in 1050:037e dispatches directly to 1050:036f.
      return 0;
    case WM_PAINT:
      return paint_original_auxiliary_window(
          window, simtower::OriginalAuxiliaryWindow::command);
    case WM_DESTROY:
      // CMDBTNWNDPROC 1050:01b0 deletes its four WinG/GDI cache objects and
      // posts WM_QUIT. Native surfaces are value-owned, leaving only the quit.
      PostQuitMessage(0);
      return 0;
    case WM_NCHITTEST: {
      POINT point{static_cast<std::int16_t>(LOWORD(lparam)),
                  static_cast<std::int16_t>(HIWORD(lparam))};
      ScreenToClient(window, &point);
      const auto hit =
          simtower::original_palette_frame_hit_test(point.x, point.y);
      if (hit == simtower::OriginalPaletteFrameHit::close) return HTCLIENT;
      if (hit == simtower::OriginalPaletteFrameHit::drag) return HTCAPTION;
      break;
    }
    case WM_NCACTIVATE:
      paint_original_auxiliary_activation_frame(
          window, simtower::OriginalAuxiliaryWindow::command,
          wparam != FALSE);
      return TRUE;
    case WM_ACTIVATE:
      apply_original_palette_window_activation(
          window, simtower::OriginalAuxiliaryWindow::command,
          LOWORD(wparam) != WA_INACTIVE);
      return 0;
    case WM_ACTIVATEAPP:
      apply_original_palette_app_activation(wparam != 0U);
      return 0;
    case WM_LBUTTONDOWN: {
      const int frame_x = static_cast<std::int16_t>(LOWORD(lparam));
      const int frame_y = static_cast<std::int16_t>(HIWORD(lparam));
      const auto plan = simtower::original_command_pointer_plan(
          simtower::OriginalCommandPointerPhase::button_down,
          frame_x, frame_y);
      if (plan.close_palette) {
        const auto visibility = simtower::original_auxiliary_visibility_plan(
            simtower::OriginalAuxiliaryVisibilityTrigger::close_box,
            g_toolbar_visible);
        g_toolbar_visible = visibility.visible;
        if (visibility.operation ==
            simtower::OriginalAuxiliaryWindowOperation::hide) {
          ShowWindow(window, SW_HIDE);
        }
        return 0;
      }
      // 1050:0219 delays only the build toggle. Every edit/facility point
      // enters 1058:03a9 on mouse-down, allowing 1050:05a7's grouped selector
      // to capture the still-held button and consume its eventual release.
      if (plan.press_toggle) {
        g_command_toggle_pressed = true;
        InvalidateRect(window, nullptr, FALSE);
        UpdateWindow(window);
      }
      if (plan.activate_point) {
        activate_original_command_point(window, lparam);
      }
      // 1050:02a4-02ad enters 1208:05e6 after every non-close button-down,
      // even when the grouped selector held the call until button release.
      // Preserve both DS:31b0/31b2 and 11e0:0e84's pump side effect.
      if (plan.sample_coarse_tick) {
        g_original_last_auxiliary_pointer_tick = original_runtime_coarse_tick();
      }
      return 0;
    }
    case WM_LBUTTONUP: {
      const int frame_x = static_cast<std::int16_t>(LOWORD(lparam));
      const int frame_y = static_cast<std::int16_t>(HIWORD(lparam));
      const auto plan = simtower::original_command_pointer_plan(
          simtower::OriginalCommandPointerPhase::button_up,
          frame_x, frame_y);
      // 1050:02b3 restores 1080:07a6's normal header unconditionally before
      // applying the release point outside the close box.
      if (plan.restore_toggle) {
        g_command_toggle_pressed = false;
        InvalidateRect(window, nullptr, FALSE);
        UpdateWindow(window);
      }
      if (plan.activate_point) {
        activate_original_command_point(window, lparam);
      }
      return 0;
    }
    case WM_QUERYNEWPALETTE:
    case WM_PALETTECHANGED: {
      const auto plan = simtower::original_auxiliary_palette_message_plan(
          message == WM_QUERYNEWPALETTE
              ? simtower::OriginalPaletteMessageKind::query_new_palette
              : simtower::OriginalPaletteMessageKind::palette_changed,
          message == WM_PALETTECHANGED &&
              reinterpret_cast<HWND>(wparam) == window,
          g_main_message_runtime_initialized, g_main_closing);
      if (plan.realize) {
        (void)realize_original_palette_message(window);
      }
      return static_cast<LRESULT>(plan.result);
    }
    default:
      return DefWindowProcW(window, message, wparam, lparam);
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT apply_original_main_shutdown_message(
    HWND window,
    simtower::OriginalMainShutdownMessage message) {
  auto plan = simtower::original_main_shutdown_plan(
      message, g_main_closing, false);
  if (plan.request_confirmation) {
    plan = simtower::original_main_shutdown_plan(
        message, g_main_closing,
        confirm_original_tower_transition(window, 4));
  }
  if (plan.set_closing_latch) g_main_closing = true;
  if (plan.stop_audio_channels && g_audio) g_audio->stop_all(true);
  if (plan.destroy_main_window) DestroyWindow(window);
  if (plan.clear_main_window_handle && g_main_window == window) {
    // 1158:04cc clears DS:3258 only after synchronous DestroyWindow returns.
    g_main_window = nullptr;
  }
  if (plan.post_quit_message) PostQuitMessage(0);
  return static_cast<LRESULT>(plan.result);
}

// Native message boundary for MAINWNDPROC at 1158:0000.
LRESULT CALLBACK main_window_proc(HWND window, UINT message,
                                  WPARAM wparam, LPARAM lparam) {
  // Message set and dispatch order are recovered from the 22-entry parallel
  // lookup table at 1158:0597/05c3.
  switch (message) {
    case WM_CREATE: {
      configure_original_main_host_chrome(window);
      HMENU menu = GetMenu(window);
      HMENU options = GetSubMenu(menu, 1);
      if (options) {
        // 1158:006d-0091 addresses Fast Mode by position in Options.
        CheckMenuItem(options, 2U,
                      MF_BYPOSITION |
                          (g_fast_mode_enabled ? MF_CHECKED : MF_UNCHECKED));
      }
      HMENU help = GetSubMenu(menu, 3);
      if (help) {
        DeleteMenu(help, 9000, MF_BYCOMMAND);
        DeleteMenu(help, 9001, MF_BYCOMMAND);
        DeleteMenu(help, 9002, MF_BYCOMMAND);
        DeleteMenu(help, 2, MF_BYPOSITION);
      }
      return 0;
    }
    case WM_DESTROY:
      return apply_original_main_shutdown_message(
          window, simtower::OriginalMainShutdownMessage::destroy);
    case WM_SIZE:
      // 1158:041c invalidates first. Restore/maximize then rebuilds both
      // scroll ranges, runs 1078:0000(1), and activates WAVMIX; minimize runs
      // 1078:0000(0) before deactivating WAVMIX and does not resize the view.
      switch (simtower::original_main_size_disposition(
          g_main_message_runtime_initialized,
          static_cast<std::uint16_t>(wparam))) {
        case simtower::OriginalMainSizeDisposition::ignore:
          return 0;
        case simtower::OriginalMainSizeDisposition::invalidate_only:
          g_original_main_backing.dirty = true;
          InvalidateRect(window, nullptr, FALSE);
          return 0;
        case simtower::OriginalMainSizeDisposition::restore_or_maximize:
          break;
        case simtower::OriginalMainSizeDisposition::minimize:
          InvalidateRect(window, nullptr, FALSE);
          apply_original_auxiliary_window_size_state(false);
          if (g_audio) g_audio->deactivate();
          return 0;
      }
      g_original_main_backing.dirty = true;
      InvalidateRect(window, nullptr, FALSE);
      {
        RECT client{};
        GetClientRect(window, &client);
        const auto saved = current_original_view(window);
        const auto vertical = simtower::original_main_scrollbar_resize_state(
            saved.y, client.bottom - client.top,
            simtower::kOriginalWorldHeight);
        const auto horizontal =
            simtower::original_main_scrollbar_resize_state(
                saved.x, client.right - client.left,
                simtower::kOriginalWorldWidth);
        // 1158:05ef installs vertical then horizontal ranges/positions without
        // repainting. 1080:00d7 subsequently repaints horizontal then vertical
        // positions after clamping the saved view to the new ranges.
        install_original_scrollbar_state(window, SB_VERT, vertical, FALSE);
        install_original_scrollbar_state(window, SB_HORZ, horizontal, FALSE);
        redraw_original_scrollbar_position(window, SB_HORZ,
                                           horizontal.position);
        redraw_original_scrollbar_position(window, SB_VERT,
                                           vertical.position);
      }
      InvalidateRect(g_map_window, nullptr, FALSE);
      // 1158:05ef completes resize handling through the synchronous
      // 1080:0a1e(1) boundary after rebuilding both ranges.
      request_original_main_surface_pass(
          simtower::OriginalMainSurfacePass::rebuild_with_sky, true);
      apply_original_auxiliary_window_size_state(true);
      if (g_audio) g_audio->activate();
      return 0;
    case WM_ACTIVATE:
      // 1158:012c keeps the custom palette-window frames synchronized with
      // main-window activation and reasserts the main window when restored.
      // Preserve its raw nonzero/zero state for 1258:0195's idle audio and
      // auxiliary-window reconciliation.
      g_original_window_activation_latch =
          LOWORD(wparam) != WA_INACTIVE;
      g_original_main_caption_active = g_original_window_activation_latch;
      if (LOWORD(wparam) != WA_INACTIVE && !IsIconic(window)) {
        // 1158:0148 calls 1078:0000(1) on every non-iconic activation, not
        // only on WM_SIZE restore. Re-show and restore the original palette
        // z-order before reasserting the main window.
        apply_original_auxiliary_window_size_state(true);
        SetActiveWindow(window);
      }
      paint_original_main_caption(window, g_original_main_caption_active);
      return 0;
    case WM_ACTIVATEAPP:
      // 1158:0118 -> 1078:01e8 demotes enabled palettes behind the main
      // window on deactivation and restores the Command promotion on return.
      apply_original_auxiliary_window_activation_state(wparam != 0U);
      return 0;
    case WM_NCHITTEST:
      // 1158:0050 returns HTERROR while DS:24b8's modal-input lock is set;
      // otherwise the message is left to DefWindowProc.
      if (original_main_modal_input_locked()) return HTERROR;
      // Host-only replacement for modern three-button caption hit testing.
      // Win16 exposes the system menu at left and only the original down/up
      // minimize/maximize buttons at right.
      if (const auto hit = original_main_caption_hit_test(window, lparam)) {
        return *hit;
      }
      return DefWindowProcW(window, message, wparam, lparam);
    case WM_NCPAINT: {
      // The original delegates this message to Win16 DefWindowProc.  Current
      // USER/DWM cannot reproduce that frame, so retain its layout and native
      // mechanics but overlay the recovered centered, two-button caption.
      const LRESULT result = DefWindowProcW(window, message, wparam, lparam);
      paint_original_main_caption(window, g_original_main_caption_active);
      return result;
    }
    case WM_NCACTIVATE: {
      const LRESULT result = DefWindowProcW(window, message, wparam, lparam);
      g_original_main_caption_active = wparam != FALSE;
      paint_original_main_caption(window, g_original_main_caption_active);
      return result;
    }
    case WM_PAINT: {
      // 1158:00da publishes DS:0244 before BeginPaint and clears it only after
      // EndPaint, suppressing reentrant left-button dispatch during rendering.
      g_main_paint_active = true;
      const auto pass = g_main_pending_surface_pass.value_or(
          simtower::OriginalMainSurfacePass::window_paint);
      g_main_pending_surface_pass.reset();
      LRESULT result{};
      if (simtower::original_main_wm_paint_presents_backing(
              IsIconic(window))) {
        result = paint_window(window, pass);
      } else {
        // 1158:00da still validates the update region while iconic, but skips
        // the 1158:0a3c backing blit entirely.
        PAINTSTRUCT paint{};
        BeginPaint(window, &paint);
        EndPaint(window, &paint);
      }
      g_main_paint_active = false;
      return result;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
      const auto phase = message == WM_LBUTTONDOWN
          ? simtower::OriginalMainPointerMessagePhase::button_down
          : simtower::OriginalMainPointerMessagePhase::double_click;
      const auto plan = simtower::original_main_pointer_message_plan(
          phase, original_main_modal_input_locked(), g_main_paint_active,
          (wparam & MK_LBUTTON) != 0U,
          g_main_pointer_interaction_armed);
      if (!plan.forward_to_world_dispatch) return 0;
      if (plan.arm_interaction) g_main_pointer_interaction_armed = true;
      dispatch_original_world_input(
          window,
          phase == simtower::OriginalMainPointerMessagePhase::button_down
              ? simtower::OriginalWorldInputMessage::button_down
              : simtower::OriginalWorldInputMessage::double_click,
          wparam, lparam);
      return 0;
    }
    case WM_MOUSEMOVE: {
      // 1158:0314 obtains the live screen point and runs the complete
      // 1258:0505 cursor resolver before DS:24b8 can suppress world input.
      const auto move_plan = simtower::original_main_mouse_move_plan(
          original_main_modal_input_locked());
      if (move_plan.refresh_cursor) {
        POINT screen_point{};
        if (GetCursorPos(&screen_point)) {
          update_original_main_cursor_from_screen_point(screen_point);
        }
      }
      if (!move_plan.forward_to_world_dispatch) return 0;
      dispatch_original_world_input(
          window, simtower::OriginalWorldInputMessage::mouse_move,
          wparam, lparam);
      return 0;
    }
    case WM_LBUTTONUP: {
      const auto plan = simtower::original_main_pointer_message_plan(
          simtower::OriginalMainPointerMessagePhase::button_up,
          original_main_modal_input_locked(), g_main_paint_active,
          (wparam & MK_LBUTTON) != 0U,
          g_main_pointer_interaction_armed);
      if (!plan.forward_to_world_dispatch) return 0;
      dispatch_original_world_input(
          window, simtower::OriginalWorldInputMessage::button_up,
          wparam, lparam);
      if (plan.clear_interaction) {
        g_main_pointer_interaction_armed = false;
      }
      return 0;
    }
    case WM_CLOSE:
      return apply_original_main_shutdown_message(
          window, simtower::OriginalMainShutdownMessage::close);
    case WM_QUERYENDSESSION:
      return apply_original_main_shutdown_message(
          window, simtower::OriginalMainShutdownMessage::query_end_session);
    case WM_GETMINMAXINFO: {
      auto* limits = reinterpret_cast<MINMAXINFO*>(lparam);
      // 1128:02aa derives desktop-sensitive maxima before 1158:0334-0415
      // adds the exact Win16 nonclient metrics to all four tracking limits.
      const auto geometry = current_original_main_window_geometry();
      limits->ptMinTrackSize.x = geometry.minimum_track_width;
      limits->ptMinTrackSize.y = geometry.minimum_track_height;
      limits->ptMaxTrackSize.x = geometry.maximum_track_width;
      limits->ptMaxTrackSize.y = geometry.maximum_track_height;
      limits->ptMaxSize = limits->ptMaxTrackSize;
      return 0;
    }
    case WM_COMMAND:
      // MAINWNDPROC 1158:046f consumes all application commands while the
      // process-wide modal-manager latch at DS:24b8 is active.
      if (original_main_modal_input_locked()) return 0;
      // Exact 1158:06b9 27-entry application-command dispatcher. The native
      // switch retains every public and hidden command target from that table.
      switch (LOWORD(wparam)) {
        case 3000U:
        case 3001U:
        case 3002U:
          (void)run_original_event_dialog(
              {static_cast<std::uint16_t>(LOWORD(wparam)), 0, 10000});
          break;
        case 40001U:
          // 1158:071a uses 10d0:0604's confirmation/latch/audio transaction,
          // then destroys Main without WM_CLOSE's DS:3258-clear tail.
          (void)apply_original_main_shutdown_message(
              window,
              simtower::OriginalMainShutdownMessage::exit_command);
          break;
        case 40002U:
          (void)save_original_tower_as(window);
          break;
        case 40003U:
          (void)save_original_tower(window);
          break;
        case 40004U:
          (void)open_original_tower(window);
          break;
        case 40005U:
          if (confirm_original_tower_transition(window, 1)) {
            begin_original_new_tower(window);
          }
          break;
        case 40007U:
          // 1158:0759 toggles Fast Mode at DS:de34.
          toggle_original_menu_flag(window, 40007U, g_fast_mode_enabled);
          break;
        case 40008U:
          // Exact 10e8:01e2 reconsideration path made available only after a
          // scheduled fire-crew offer was declined.
          if (g_tower_document &&
              simtower::original_fire_crew_menu_offer_available(
                  *g_tower_document)) {
            const auto choice = static_cast<std::uint16_t>(
                run_original_event_dialog(
                    simtower::original_fire_crew_offer(g_part)));
            const auto resolution = simtower::resolve_original_fire_crew_offer(
                *g_tower_document, g_part, choice, false);
            if (consume_original_fire_crew_resolution(resolution)) {
              invalidate_original_main_surface();
              InvalidateRect(g_info_window, nullptr, FALSE);
            }
          }
          break;
        case 40009U:
          // 1158:0723 toggles People animation at DS:de30.
          toggle_original_menu_flag(window, 40009U,
                                    g_people_animation_enabled);
          break;
        case 40010U:
          // 1158:073e toggles Effects animation at DS:de32.
          toggle_original_menu_flag(window, 40010U,
                                    g_effects_animation_enabled);
          break;
        case 40011U:
          toggle_original_audio_category(window, 40011U, 0);
          break;
        case 40012U:
          // The original command table associates 40012 with DS:de2c
          // (Events), despite the MENU resource labelling it Background.
          toggle_original_audio_category(window, 40012U, 1);
          break;
        case 40013U:
          // Likewise 40013 controls DS:de2e (Background) in the binary.
          toggle_original_audio_category(window, 40013U, 2);
          break;
        case 40014U:
          // The command palette is the one auxiliary window promoted with
          // HWND_TOPMOST by the original dispatcher.
          toggle_original_auxiliary_window(window, 40014U, g_command_window,
                                           g_toolbar_visible, HWND_TOPMOST);
          break;
        case 40015U:
          toggle_original_auxiliary_window(window, 40015U, g_info_window,
                                           g_info_visible, HWND_TOP);
          break;
        case 40016U:
          toggle_original_auxiliary_window(window, 40016U, g_map_window,
                                           g_map_visible, HWND_TOP);
          break;
        case 40017U:
          // 1158:08fb -> 1060:0083 opens DIALOG/500's Finance modal.
          run_original_finance_dialog();
          break;
        case 40018U:
          // 1158:0912 -> 1010:049e opens the animated resource-backed About.
          (void)run_original_about_dialog();
          break;
        case 40019U:
          // 1158:0903 -> 10d8:0000(1): Find a saved named person.
          run_original_find_dialog(simtower::OriginalFindMode::person);
          break;
        case 40020U:
          // 1158:0907 -> 10d8:0000(0): Find a saved named tenant.
          run_original_find_dialog(simtower::OriginalFindMode::tenant);
          break;
        case 40021U:
          // 1158:091a: the menu item and F1 accelerator both request the
          // supplied SIMTOWER.HLP contents through WinHelp command 3.
          (void)run_original_help_contents();
          break;
        case 9000U:
          // Hidden release-menu entry retained by the 27-way table.
          if (run_original_bomb_offer_boundary()) {
            mark_original_world_interaction_changed(window);
          }
          break;
        case 9001U: {
          // Hidden Treasure command calls 1148:020f directly, bypassing the
          // normal construction/span predicate.
          if (!g_tower_document) break;
          const auto treasure = simtower::award_original_rating_treasure(
              *g_tower_document, g_part);
          if (treasure.dialog.valid()) {
            (void)run_original_event_dialog(treasure.dialog);
          }
          if (treasure.changed) {
            g_tower_dirty = true;
            // 1148:025d-026c presents DIALOG/3040 first, then 1178:076f
            // credits and directly repaints Info. The b922 latch itself has
            // no Main/Map presentation side effect.
            if (treasure.treasure_awarded) {
              invalidate_original_info_status();
            }
          }
          break;
        }
        case 9002U:
          // Hidden Fire command is the same 10e8:0029 boundary used by the
          // scheduler.
          if (run_original_fire_offer_boundary()) {
            mark_original_world_interaction_changed(window);
          }
          break;
        case 9003U:
          // 1158:0946 acquires the Main DC and calls 1158:0ba8 directly. Keep
          // this out of WM_PAINT and preserve the retained backing pixels.
          present_original_main_backing_direct();
          break;
        default: {
          const auto command = LOWORD(wparam);
          // 1158:0992 sends any non-table DIALOG id in 3000..4001 through
          // 1068:0000 with argument zero and WAVE/10000.
          if (simtower::original_main_command_route(command) ==
              simtower::OriginalMainCommandRoute::
                  generic_dialog_then_def_window_proc) {
            (void)run_original_event_dialog({command, 0, 10000});
          }
          // 1158:09b1 calls DefWindowProc for every ID outside the literal
          // command table, including IDs in the generic dialog range.
          return DefWindowProcW(window, message, wparam, lparam);
        }
      }
      return 0;
    case WM_HSCROLL:
      if (original_main_modal_input_locked()) return 0;
      // Both original scroll branches invalidate before decoding the request,
      // so even an unsupported code retains this exposure side effect.
      InvalidateRect(window, nullptr, FALSE);
      scroll_original(window, SB_HORZ, wparam,
                      simtower::kOriginalWorldWidth);
      return 0;
    case WM_VSCROLL:
      if (original_main_modal_input_locked()) return 0;
      InvalidateRect(window, nullptr, FALSE);
      scroll_original(window, SB_VERT, wparam,
                      simtower::kOriginalWorldHeight);
      return 0;
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
      // 1158:0492 preserves USER's default system-key handling normally, but
      // consumes it while any original modal dialog owns the interaction lock.
      if (original_main_modal_input_locked()) return 0;
      return DefWindowProcW(window, message, wparam, lparam);
    case WM_QUERYNEWPALETTE: {
      const auto plan = simtower::original_main_palette_message_plan(
          simtower::OriginalPaletteMessageKind::query_new_palette, false);
      if (plan.realize) {
        (void)realize_original_palette_message(window);
      }
      return static_cast<LRESULT>(plan.result);
    }
    case WM_PALETTECHANGED: {
      const auto plan = simtower::original_main_palette_message_plan(
          simtower::OriginalPaletteMessageKind::palette_changed,
          reinterpret_cast<HWND>(wparam) == window);
      if (plan.realize) {
        (void)realize_original_palette_message(window);
      }
      return static_cast<LRESULT>(plan.result);
    }
    case kOriginalAudioCallbackMessage:
      // MAINWNDPROC's 22-entry table routes 0x03BD to 11c8:09d2, the
      // WAVMIX callback. The native waveOut backend does not use this Win16
      // callback message. Simulation is driven from the empty-message-queue
      // path at 1258:0186 -> 1200:0196, never from a fabricated timer.
      // 11e0:0e84's 48-ms empty-queue pump is therefore represented by this
      // consumed callback boundary: waveOut completes asynchronously and has
      // no WAVMIXPUMP state requiring a second native scheduler.
      return 0;
    default:
      return DefWindowProcW(window, message, wparam, lparam);
  }
}

void register_original_classes() {
  // Complete native counterpart of 1258:0345: the main, Map, Info, and
  // command classes retain their original procedures, extra bytes, icon/
  // cursor choices, white stock background, and main-class CS_DBLCLKS flag.
  for (const auto& spec : simtower::original_window_class_specs()) {
    WNDCLASSW window_class{};
    window_class.style = spec.style;
    window_class.cbWndExtra = spec.window_extra_bytes;
    window_class.hInstance = g_instance;
    window_class.hbrBackground =
        static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    window_class.lpszClassName = spec.class_name.data();
    switch (spec.procedure) {
      case simtower::OriginalWindowClassProcedure::main:
        window_class.lpfnWndProc = main_window_proc;
        break;
      case simtower::OriginalWindowClassProcedure::map:
        window_class.lpfnWndProc = map_window_proc;
        break;
      case simtower::OriginalWindowClassProcedure::info:
        window_class.lpfnWndProc = info_window_proc;
        break;
      case simtower::OriginalWindowClassProcedure::command:
        window_class.lpfnWndProc = command_window_proc;
        break;
    }
    if (spec.icon ==
        simtower::OriginalWindowClassIcon::application_resource) {
      window_class.hIcon = g_application_icon;
    } else if (spec.icon ==
               simtower::OriginalWindowClassIcon::system_application) {
      window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    if (spec.cursor == simtower::OriginalWindowClassCursor::arrow) {
      window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    }
    if (spec.menu == simtower::OriginalWindowClassMenu::tower_menu) {
      window_class.lpszMenuName = L"TOWER_MENU";
    } else if (spec.menu ==
               simtower::OriginalWindowClassMenu::empty_string) {
      // 1258:04b4 points at the empty byte immediately before CmdBtnWClass.
      window_class.lpszMenuName = L"";
    }
    if (!RegisterClassW(&window_class)) {
      throw std::runtime_error("Could not register original window class");
    }
  }
}

void initialize_original_window_dc(HWND window) {
  // Recovered four-window DC initialization repeated at 1128:069a-06e4,
  // 077f-07c9, 0864-08ae, and 09a0-09ea. These calls occur while the windows
  // are still hidden and before their first ordinary paint.
  const HDC dc = GetDC(window);
  if (!dc) return;
  if (g_logical_palette) {
    SelectPalette(dc, g_logical_palette, FALSE);
    RealizePalette(dc);
  }
  if (const HFONT font = simtower::original_cached_font(9)) {
    SelectObject(dc, font);
  }
  SetTextAlign(dc, TA_UPDATECP);
  SetBkMode(dc, TRANSPARENT);
  ReleaseDC(window, dc);
}

HWND create_palette_window(
    const wchar_t* class_name,
    const simtower::OriginalStartupAuxiliaryWindowSpec& spec) {
  HWND window = CreateWindowExW(
      0, class_name, L"", kPaletteWindowStyle, spec.x, spec.y,
      spec.create_outer_width, spec.create_outer_height,
      nullptr, nullptr, g_instance, nullptr);
  if (!window) {
    throw std::runtime_error("Could not create original auxiliary window");
  }
  const HWND insert_after =
      spec.insert_after ==
              simtower::OriginalStartupAuxiliaryInsertAfter::topmost
          ? HWND_TOPMOST
          : HWND_TOP;
  SetWindowPos(window, insert_after, 0, 0,
               spec.set_position_width, spec.set_position_height,
               spec.set_position_flags);
  SetWindowLongPtrW(window, GWLP_ID, static_cast<LONG_PTR>(spec.window_id));
  initialize_original_window_dc(window);
  return window;
}

void create_original_windows(HMENU menu) {
  // Win16 USER uses Pascal argument order. Preserve 1128:05eb's complete
  // Command/Info/Map sequence, including the Command-only raw outer resize,
  // initial z bands, IDs, and hidden-window DC setup.
  const auto auxiliary = simtower::original_startup_auxiliary_window_specs(
      GetSystemMetrics(SM_CXBORDER), GetSystemMetrics(SM_CYBORDER));
  g_command_window = create_palette_window(kCommandClass, auxiliary[0]);
  g_info_window = create_palette_window(kInfoClass, auxiliary[1]);
  g_map_window = create_palette_window(kMapClass, auxiliary[2]);

  const auto geometry = current_original_main_window_geometry();
  g_main_window = CreateWindowExW(
      0, kMainClass, L"SimTower", kMainStyle,
      geometry.initial_x, geometry.initial_y,
      std::max(1, geometry.initial_width),
      std::max(1, geometry.initial_height),
      nullptr, menu, g_instance, nullptr);
  if (!g_main_window) {
    throw std::runtime_error("Could not create Tower_MainWClass");
  }
  initialize_original_window_dc(g_main_window);
  RECT client{};
  GetClientRect(g_main_window, &client);
  const auto initial = simtower::original_initial_view(
      client.right - client.left, client.bottom - client.top);
  // 1128:08d6 initializes the Win16 vertical range/position first, followed
  // by horizontal, while 1158:041c's DS:02a4 gate suppresses creation-time
  // WM_SIZE side effects. nPage=0 retains the original fixed-size thumb.
  install_original_scrollbar_state(
      g_main_window, SB_VERT,
      simtower::original_main_scrollbar_resize_state(
          initial.y, client.bottom - client.top,
          simtower::kOriginalWorldHeight),
      FALSE);
  install_original_scrollbar_state(
      g_main_window, SB_HORZ,
      simtower::original_main_scrollbar_resize_state(
          initial.x, client.right - client.left,
          simtower::kOriginalWorldWidth),
      FALSE);
  // 1128:0ae7-0b95 applies the loaded Sound profile only after Main exists
  // and both initial scrollbars are installed; this does not belong to
  // MAINWNDPROC's much smaller WM_CREATE branch.
  apply_original_audio_menu_state(GetMenu(g_main_window));
}

void preload_original_command_surfaces() {
  // 1050:03aa creates the original cached BITMAP/300..302 command surfaces
  // between showing Info and showing Command. Embedded DIB views are immutable,
  // but acquire them at the same host boundary instead of validating them
  // before any window exists.
  for (const auto bitmap : simtower::kOriginalCommandSurfaceResourceIds) {
    (void)simtower::original_dib_view(g_resources.find("BITMAP", bitmap));
  }
}

void show_original_windows() {
  for (const auto action :
       simtower::original_startup_show_plan(g_build_mode_enabled)) {
    using Action = simtower::OriginalStartupShowAction;
    switch (action) {
      case Action::show_command:
        // 1128:01f4-01fe: DS:325a / CmdBtnWClass.
        ShowWindow(g_command_window, SW_SHOW);
        break;
      case Action::show_info:
        // 1128:01e4-01ee: DS:325c / Tower_InfoWClass.
        ShowWindow(g_info_window, SW_SHOW);
        break;
      case Action::preload_command_surfaces:
        // 1128:01ef-01f3 -> 1050:03aa.
        preload_original_command_surfaces();
        break;
      case Action::show_map:
        // 1128:01d9-01e3: DS:325e / Tower_MapWClass.
        ShowWindow(g_map_window, SW_SHOW);
        break;
      case Action::show_main:
        // 1128:01ff-0209: DS:3258 / Tower_MainWClass.
        ShowWindow(g_main_window, SW_SHOW);
        break;
      case Action::select_arrow_cursor:
        // 1128:020a-0211 -> 11e0:0d80(0).
        SetCursor(resolve_original_cursor(0U));
        break;
      case Action::toggle_construction_mode:
        // 1128:0212-021d -> 1058:033c when DS:783e != 1.
        apply_original_construction_toggle();
        break;
      case Action::compose_and_present_command:
        // 1128:021e-0222 -> 1080:05a1 composes the Command backing, then
        // 1080:0784-0798 invalidates
        // and synchronously presents DS:325a (CmdBtnWClass). It does not force
        // a Main presentation at this boundary.
        InvalidateRect(g_command_window, nullptr, FALSE);
        UpdateWindow(g_command_window);
        break;
      case Action::none:
        break;
    }
  }
}

void validate_original_startup_data() {
  // 1128:0ba3 invokes 11a0:134c's floor-offset precompute and 11f8:033a's
  // world/facility sheet pack. Native transforms and embedded resource views
  // perform the same work directly, so validation touches their inputs here.
  for (const auto action : simtower::original_startup_precompute_plan()) {
    switch (action) {
      case simtower::OriginalStartupPrecomputeAction::precompute_floor_offsets:
        for (std::int32_t floor = 0; floor < 60; ++floor) {
          (void)simtower::original_precomputed_floor_offset(floor, 1);
        }
        break;
      case simtower::OriginalStartupPrecomputeAction::
          pack_world_and_facility_sheets:
        // Native rendering samples the immutable embedded DIBs directly; the
        // validation pass below opens those same sheet-pack inputs in this
        // recovered second phase.
        break;
    }
  }
  // Exact 1178:000c loads construction, rent-income, and maintenance tables
  // from YEN/1000, YEN/1001, and YEN/1002 respectively.
  g_part = simtower::original_part_table(g_resources.find("PART", 1000));
  g_construction_costs =
      simtower::original_yen_table(g_resources.find("YEN", 1000));
  g_rent_income =
      simtower::original_yen_table(g_resources.find("YEN", 1001));
  g_maintenance_costs =
      simtower::original_yen_table(g_resources.find("YEN", 1002));
  // 1140:0005 initializes rating one and loads TABL/1001; preload all six
  // immutable native rating states so promotion only changes the active view.
  for (int rating = 1001; rating <= 1006; ++rating) {
    (void)simtower::original_word_table(g_resources.find("TABL", rating));
    g_command_ratings[static_cast<std::size_t>(rating - 1001)] =
        simtower::original_command_rating_state(
            g_resources, static_cast<std::uint16_t>(rating - 1000));
  }
  for (int bitmap = 600; bitmap <= 606; ++bitmap) {
    (void)simtower::original_dib_view(g_resources.find("BITMAP", bitmap));
  }
  (void)simtower::original_dib_view(g_resources.find("BITMAP", 320));
  (void)simtower::original_dib_view(g_resources.find("BITMAP", 352));
  for (int bitmap = 850; bitmap <= 857; ++bitmap) {
    (void)simtower::original_dib_view(g_resources.find("BITMAP", bitmap));
  }
  (void)simtower::original_dib_view(g_resources.find("BITMAP", 905));
  for (int bitmap = 400; bitmap <= 408; ++bitmap) {
    (void)simtower::original_dib_view(g_resources.find("BITMAP", bitmap));
  }
  for (int bitmap = 410; bitmap <= 411; ++bitmap) {
    (void)simtower::original_dib_view(g_resources.find("BITMAP", bitmap));
  }
  for (int bitmap = 20256; bitmap <= 20265; ++bitmap) {
    (void)simtower::original_dib_view(g_resources.find("BITMAP", bitmap));
  }
  (void)simtower::parse_original_dialog(g_resources.find("DIALOG", 124));
  (void)simtower::parse_original_dialog(g_resources.find("DIALOG", 400));
  (void)simtower::parse_original_dtmp(g_resources.find("DTMP", 400));
  for (int dialog = 730; dialog <= 732; ++dialog) {
    (void)simtower::parse_original_dialog(
        g_resources.find("DIALOG", dialog));
    (void)simtower::parse_original_dtmp(g_resources.find("DTMP", dialog));
  }
  for (int dialog = 748; dialog <= 764; ++dialog) {
    (void)simtower::parse_original_dialog(
        g_resources.find("DIALOG", dialog));
    (void)simtower::parse_original_dtmp(g_resources.find("DTMP", dialog));
  }
  for (const int strings : {420, 710, 712, 713, 714, 715, 716}) {
    if (g_resources.find("STRL", strings).empty()) {
      throw std::runtime_error(
          "Original Facility Information strings are missing");
    }
  }
  for (int dialog = 3030; dialog <= 3034; ++dialog) {
    (void)simtower::parse_original_dialog(
        g_resources.find("DIALOG", dialog));
    (void)simtower::parse_original_dtmp(g_resources.find("DTMP", dialog));
  }
  (void)simtower::parse_original_dialog(g_resources.find("DIALOG", 3040));
  (void)simtower::parse_original_dtmp(g_resources.find("DTMP", 3040));
  if (g_resources.find("CLUT", 1000).size() < 2048U ||
      g_resources.find("CGPK", 2536).size() < 123U * 288U ||
      g_resources.find("CGPK", 2600).size() < 82U * 288U ||
      g_resources.find("CGPK", 2664).size() < 41U * 288U) {
    throw std::runtime_error("Original world graphics resources are truncated");
  }
}

}  // namespace

// Native program-entry/application-initialization/host-loop boundary for
// 1000:0000, 1128:0005, 1258:000b, and 1258:04e2.
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR command_line, int) {
  try {
    g_instance = instance;
    bool startup_sound_available = true;
    if (!initialize_original_startup_capabilities(startup_sound_available)) {
      return 0;
    }
    g_resources = simtower::OriginalResources::from_current_module();
    g_audio = std::make_unique<simtower::OriginalAudioRuntime>(g_resources);
    g_audio->set_sound_enabled(startup_sound_available);
    if (!initialize_original_audio()) {
      return 0;
    }
    // 1128:0005 initializes WAVMIX before 1128:02aa applies SIMTOWER.INI.
    apply_original_startup_profile();
    validate_original_startup_data();
    // 1020:0019/0e29/0f4f creates one process-wide 256-entry logical palette
    // before the four application window classes begin receiving messages.
    simtower::reset_original_palette_runtime(
        g_resources, nullptr, g_palette_runtime,
        original_runtime_coarse_tick());
    create_original_logical_palette();
    // 1128:0598 initializes 1208:0a8d's font bank after palette setup and
    // immediately before the custom-cursor initialization at 11e0:0cfb.
    simtower::initialize_original_font_cache();
    for (std::size_t index = 0;
         index < simtower::kOriginalCustomCursorResourceIds.size(); ++index) {
      g_original_cursors[index] = simtower::create_original_cursor(
          g_resources, simtower::kOriginalCustomCursorResourceIds[index]);
    }
    g_application_icon = simtower::create_original_icon(g_resources, "TOWER_APPICON");
    g_accelerators = simtower::create_original_accelerators(
        g_resources.find("ACCELERATOR", "TOWER_MENU"));
    HMENU menu = simtower::create_original_menu(
        g_resources.find("MENU", "TOWER_MENU"));

    register_original_classes();
    create_original_windows(menu);
    menu = nullptr;  // owned by the main window after successful creation

    // 1128:0042-00a7 creates the full-desktop BITMAP/128 splash, retains the
    // 1208:05e6 coarse start tick, and replaces it with BITMAP/256 only after
    // a signed-magnitude delta of at least 180 ticks (nominally 2.88 s).
    // 1128:003a selects Wait before 1010:0018 creates the first splash. The
    // create helper repeats it after clearing the splash class cursor, just
    // as 1128:004f-0063 does.
    SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    const bool splash_created = create_original_startup_splash(128);
    if (splash_created) {
      const auto splash_started =
          original_runtime_coarse_tick();
      while (std::bit_cast<std::int32_t>(
                 simtower::original_tick_magnitude_delta(
                     original_runtime_coarse_tick(),
                     splash_started)) <
             static_cast<std::int32_t>(
                 simtower::kOriginalStartupSplashMinimumTicks)) {
      }
      update_original_startup_splash(256);
    }

    // 1128:00c4 invokes 10d0:086c/0ac2 before WAVE/20000 and DIALOG/124.
    // This is a real temporary fresh-tower bootstrap: DS:31ca remains zero,
    // but frame 0x09e5 immediately animates the shared palette away from the
    // base CLUT and DS:31c4 keeps construction disabled. Preserve the visible
    // splash/startup-dialog palette without publishing an active document.
    const auto startup_bootstrap = simtower::make_original_new_tdt();
    reset_original_command_state(true);
    // 10d0:0ac2 still calls 1140:010d(0) and 1080:055d, but DS:31c4 makes
    // 1080:05a1's command composition a no-op until startup completes.
    refresh_original_rating_command(
        startup_bootstrap.header.rating, 0U, false);
    adjust_original_derived_map_focus_synchronously();
    simtower::reset_original_palette_runtime(
        g_resources, &startup_bootstrap, g_palette_runtime,
        original_runtime_coarse_tick());
    synchronize_original_logical_palette();
    set_original_fire_menu_enabled(
        simtower::original_fire_crew_menu_enabled_after_rebuild(
            startup_bootstrap));

    // 1128:00ce-00da plays WAVE/20000 once at priority 1 after the second
    // splash image and immediately before the original new/load startup path.
    (void)g_audio->play_resource(20000, 0, 1);

    std::array<char, 128> module_path{};
    (void)GetModuleFileNameA(instance, module_path.data(), 127U);
    std::string module_directory(module_path.data());
    const auto module_separator = module_directory.find_last_of('\\');
    if (module_separator != std::string::npos) {
      module_directory.resize(module_separator + 1U);
    }
    const auto native_command_line =
        simtower::normalize_native_startup_command_line(
            command_line ? command_line : "");
    const auto startup_target = simtower::original_startup_command_target(
        module_directory, native_command_line);
    if (startup_target) {
      // 1128:00e5 bypasses DIALOG/124 for a nonempty WinMain tail. 10d0:0225
      // itself creates a fresh tower after any failed direct load.
      (void)load_original_tower_from_path(
          g_main_window,
          std::filesystem::path(
              original_cp1252_to_wide(startup_target->path)),
          original_cp1252_to_wide(startup_target->file_title));
      destroy_original_startup_splash();
    } else {
      const int startup_choice = run_original_startup_dialog();
      destroy_original_startup_splash();
      if (startup_choice == 2 || startup_choice == -1) {
        // 1258:0095 invokes 10b8:0000/0039 without first destroying Main.
        // USER releases that still-hidden top-level window when WinMain exits.
        destroy_original_auxiliary_windows();
        destroy_original_process_resources();
        return startup_choice == 2 ? 0 : 1;
      }
      // 1128:01a5-01d3: a cancelled/failed startup Open falls through to
      // New. 10d0:0225 already performs that fallback after an accepted path.
      if (startup_choice == 3) {
        if (!open_original_tower(g_main_window) && !g_tower_document) {
          begin_original_new_tower(g_main_window);
        }
      } else if (startup_choice == 1) {
        begin_original_new_tower(g_main_window);
      }
    }
    show_original_windows();
    // 1258:04e2 publishes 1128:0005's successful result only after the
    // complete startup/show sequence returns; creation-time WM_SIZE messages
    // therefore cannot reveal or reorder the auxiliary palettes.
    g_main_message_runtime_initialized = true;

    // The Win16 app supplies 1258:0186 as its empty-queue callback. Preserve
    // that architecture with PeekMessage: simulation runs only when no user
    // message is waiting and never through a fabricated WM_TIMER.
    MSG message{};
    for (;;) {
      if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        // 1258:00bc-015c gives DS:31a0 Elevator Control precedence over
        // DS:31a4's active modal and gives either the accelerator attempt
        // before IsDialogMessage. Only the no-dialog branch targets Main.
        const auto route = simtower::original_message_loop_route_plan(
            g_main_window != nullptr,
            g_elevator_control_window != nullptr,
            g_original_active_modal_window != nullptr);
        HWND target = nullptr;
        switch (route.target) {
          case simtower::OriginalMessageLoopTarget::main_window:
            target = g_main_window;
            break;
          case simtower::OriginalMessageLoopTarget::elevator_control:
            target = g_elevator_control_window;
            break;
          case simtower::OriginalMessageLoopTarget::active_modal:
            target = g_original_active_modal_window;
            break;
          case simtower::OriginalMessageLoopTarget::none:
            break;
        }
        bool handled = false;
        if (route.try_accelerator) {
          handled = TranslateAcceleratorW(
              target, g_accelerators, &message) != 0;
        }
        if (!handled && route.try_dialog_navigation) {
          handled = IsDialogMessageW(target, &message) != 0;
        }
        if (!handled && route.translate_and_dispatch_if_unhandled) {
          TranslateMessage(&message);
          DispatchMessageW(&message);
        }
        // 1258:015c performs the WM_QUIT test after the selected routing
        // boundary; USER's accelerator/dialog functions leave it unhandled.
        if (message.message == WM_QUIT) {
          break;
        }
      } else {
        run_original_idle_simulation();
      }
    }

    // 1258:016e calls 10b8:0000 before 10b8:0039 after WM_QUIT. Native value/
    // RAII ownership releases the document, tenant/floor, people, elevator,
    // medical, and table banks represented by 10b8:011e at process return,
    // after this recovered visible-window and OS-resource order is replayed.
    destroy_original_auxiliary_windows();
    destroy_original_process_resources();
    return static_cast<int>(message.wParam);
  } catch (const std::exception& error) {
    // Exact native process-level translation of 1208:0dfc. FatalAppExit owns
    // termination, just as in the Win16 executable; its code is deliberately
    // zero and the caller's message is passed without an invented title.
    // Adjacent 1208:0dad is a wvsprintf/OutputDebugString formatter with no
    // inbound call or relocation, so it contributes no game behavior.
    constexpr auto plan = simtower::original_fatal_exit_plan();
    MessageBeep(plan.beep_type);
    FatalAppExitA(plan.exit_code, error.what());
    return plan.exit_code;
  }
}
