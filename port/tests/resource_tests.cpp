#include "original_alert.hpp"
#include "original_audio.hpp"
#include "original_resources.hpp"
#include "original_resources.generated.hpp"
#include "original_dialog.hpp"
#include "original_dib.hpp"
#include "original_dtmp.hpp"
#include "original_dtmp_runtime.hpp"
#include "original_font.hpp"
#include "original_information.hpp"
#include "original_startup.hpp"
#include "original_tables.hpp"
#include "original_time.hpp"
#include "original_ui.hpp"
#include "original_wave.hpp"

#include <cassert>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace {

std::vector<std::byte> read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  assert(stream.good());
  std::vector<char> characters(
      (std::istreambuf_iterator<char>(stream)),
      std::istreambuf_iterator<char>());
  std::vector<std::byte> bytes(characters.size());
  for (std::size_t index = 0; index < characters.size(); ++index) {
    bytes[index] = static_cast<std::byte>(static_cast<unsigned char>(characters[index]));
  }
  return bytes;
}

}  // namespace

int main(int argc, char** argv) {
  // Direct 11e0:0d44 cursor-bank teardown coverage. The native fixed array
  // owns the four handles before the original table's terminating null.
  assert(simtower::kOriginalCustomCursorResourceIds ==
         (std::array<std::uint16_t, 4>{1002U, 1003U, 1004U, 1005U}));

  // Direct 10b8:0039/011e outer teardown coverage without terminating this
  // test process or touching a live window/audio backend.
  using TeardownAction = simtower::OriginalProcessTeardownAction;
  assert(simtower::original_process_teardown_plan() ==
         (std::array<TeardownAction, 6>{
             TeardownAction::release_command_and_world_surfaces,
             TeardownAction::release_font_bank,
             TeardownAction::release_cursor_bank,
             TeardownAction::shutdown_audio,
             TeardownAction::release_palette,
             TeardownAction::release_runtime_owned_storage}));

  // Direct 1020:008f value-ownership half of the palette allocation/free pair.
  assert(simtower::original_palette_storage_contract() ==
         (simtower::OriginalPaletteStorageContract{0x0040U, 0x0400U, 256U}));
  assert(argc == 2);
  const auto packed = read_file(argv[1]);
  const simtower::OriginalResources resources(packed);

  // Direct 1258:0345 coverage: class registration order and every WNDCLASS
  // field that survives the Win16-to-Win32 boundary.
  using ClassProcedure = simtower::OriginalWindowClassProcedure;
  using ClassIcon = simtower::OriginalWindowClassIcon;
  using ClassCursor = simtower::OriginalWindowClassCursor;
  using ClassMenu = simtower::OriginalWindowClassMenu;
  using ClassSpec = simtower::OriginalWindowClassSpec;
  constexpr auto class_specs = simtower::original_window_class_specs();
  assert((class_specs[0] == ClassSpec{
      8U, 0U, ClassProcedure::main, ClassIcon::application_resource,
      ClassCursor::none, ClassMenu::tower_menu, L"Tower_MainWClass"}));
  assert((class_specs[1] == ClassSpec{
      0U, 4U, ClassProcedure::map, ClassIcon::none, ClassCursor::arrow,
      ClassMenu::none, L"Tower_MapWClass"}));
  assert((class_specs[2] == ClassSpec{
      0U, 4U, ClassProcedure::info, ClassIcon::none, ClassCursor::arrow,
      ClassMenu::none, L"Tower_InfoWClass"}));
  assert((class_specs[3] == ClassSpec{
      0U, 4U, ClassProcedure::command, ClassIcon::system_application,
      ClassCursor::arrow, ClassMenu::empty_string, L"CmdBtnWClass"}));

  // NAMEPEPLEDIALOGFILTER 1100:3a39 (3a69-3b8f/3c60-3c7f) and
  // NAMETENANTDIALOGFILTER 1100:3dc4 (3df4-3f06/3fd7-3ff6) do not issue
  // EM_SETSEL.
  // Initialization returns TRUE for the dialog manager's default focus; only
  // the later paint tail focuses edit item 4, preserving its caret/selection.
  using RenameFocusPhase = simtower::OriginalRenameDialogFocusPhase;
  using RenameFocusPlan = simtower::OriginalRenameDialogFocusPlan;
  assert((simtower::original_rename_dialog_focus_plan(
              RenameFocusPhase::initialize) ==
          RenameFocusPlan{false, false, true}));
  assert((simtower::original_rename_dialog_focus_plan(
              RenameFocusPhase::after_paint) ==
          RenameFocusPlan{true, false, true}));

  // Their complete WM_COMMAND tables at 1100:3c82-3d2d/3ff9-40a7 also ignore
  // notification codes. IDs 1/2/3 all close with result one; ID 4 only
  // refreshes the edit gate, and unknown IDs fall through FALSE.
  using RenameAction = simtower::OriginalRenameDialogCommandAction;
  using RenameCommand = simtower::OriginalRenameDialogCommandPlan;
  assert((simtower::original_rename_dialog_command_plan(1U) ==
          RenameCommand{RenameAction::save, true, 1}));
  assert((simtower::original_rename_dialog_command_plan(2U) ==
          RenameCommand{RenameAction::cancel, true, 1}));
  assert((simtower::original_rename_dialog_command_plan(3U) ==
          RenameCommand{RenameAction::remove, true, 1}));
  assert((simtower::original_rename_dialog_command_plan(4U) ==
          RenameCommand{RenameAction::refresh_edit_gate, true, 0}));
  assert((simtower::original_rename_dialog_command_plan(5U) ==
          RenameCommand{RenameAction::none, false, 0}));

  assert(simtower::generated::kResources.size() == 483);
  // Direct 1208:045c coverage: the original wrapper returns null when
  // FindResource fails and otherwise returns the LoadResource handle.  The
  // embedded-pack equivalent exposes the same found/missing distinction for
  // both numeric and named resources without a Win16 resource dependency.
  assert(!resources.find("BITMAP", 128).empty());
  assert(!resources.find("bitmap", 21256).empty());
  assert(!resources.find("PART", 1000).empty());
  assert(!resources.find("YEN", 1002).empty());
  assert(!resources.find("GROUP_ICON", "TOWER_APPICON").empty());
  assert(resources.find("BITMAP", -1).empty());

  // Direct 1030:0000/0043 and 1208:049d/0529 coverage. Both original DIB loaders
  // derive the pixel pointer as header.biSize + 0x400, unconditionally
  // assuming a 256-entry palette; the latter then copies the complete
  // width/height through the opaque 1248 path. Audit every embedded BITMAP
  // against that exact shape and the native parser's resolved pixel span.
  std::size_t embedded_bitmap_count = 0U;
  for (const auto& descriptor : simtower::generated::kResources) {
    if (descriptor.type != "BITMAP") continue;
    ++embedded_bitmap_count;
    assert(descriptor.numeric_id >= 0);
    const auto bitmap = resources.find("BITMAP", descriptor.numeric_id);
    const auto dib = simtower::original_dib_view(bitmap);
    assert(dib.info != nullptr);
    assert(dib.info->bmiHeader.biSize == 40U);
    assert(dib.info->bmiHeader.biPlanes == 1U);
    assert(dib.info->bmiHeader.biBitCount == 8U);
    assert(dib.info->bmiHeader.biCompression == BI_RGB);
    assert(dib.info->bmiHeader.biHeight > 0);
    assert(dib.info->bmiHeader.biClrUsed == 0U ||
           dib.info->bmiHeader.biClrUsed == 256U);
    constexpr std::size_t kOriginalBitmapPixelOffset = 40U + 0x400U;
    assert(bitmap.size() >= kOriginalBitmapPixelOffset);
    assert(dib.pixels.data() == bitmap.data() + kOriginalBitmapPixelOffset);
    const std::size_t row_bytes =
        (static_cast<std::size_t>(dib.width) + 3U) & ~std::size_t{3U};
    assert(dib.pixels.size() ==
           row_bytes * static_cast<std::size_t>(dib.height));
  }
  assert(embedded_bitmap_count == 242U);

  // Direct 1208:0603/063a coverage: these helpers reverse one Win16 word or
  // both words of a dword. Asymmetric bytes distinguish every source lane.
  constexpr std::array<std::byte, 6> kEndianProbe{
      std::byte{0x12}, std::byte{0x34}, std::byte{0x56},
      std::byte{0x78}, std::byte{0x9a}, std::byte{0xbc}};
  assert(simtower::original_be16(kEndianProbe, 0U) == 0x1234U);
  assert(simtower::original_be16(kEndianProbe, 4U) == 0x9abcU);
  assert(simtower::original_be32(kEndianProbe, 0U) == 0x12345678U);
  assert(simtower::original_be32(kEndianProbe, 2U) == 0x56789abcU);

  // Direct 1258:000b message-loop coverage: 00bc-015c gives the modeless
  // Elevator Control first refusal over an active modal, tries accelerators
  // before dialog navigation for either, and uses Main only when neither
  // specialized window exists.
  using MessageTarget = simtower::OriginalMessageLoopTarget;
  using MessageRoute = simtower::OriginalMessageLoopRoutePlan;
  assert((simtower::original_message_loop_route_plan(true, false, false) ==
          MessageRoute{MessageTarget::main_window, true, false, true}));
  assert((simtower::original_message_loop_route_plan(
              true, false, true) ==
          MessageRoute{MessageTarget::active_modal, true, true, true}));
  assert((simtower::original_message_loop_route_plan(
              true, true, false) ==
          MessageRoute{MessageTarget::elevator_control, true, true, true}));
  assert((simtower::original_message_loop_route_plan(
              true, true, true) ==
          MessageRoute{MessageTarget::elevator_control, true, true, true}));
  assert((simtower::original_message_loop_route_plan(false, false, false) ==
          MessageRoute{}));

  // Full MAINWNDPROC 1158:0000-057c audit: its 22-message table is exercised
  // below across shutdown, pointer, scroll, size, paint, and palette plans.
  // 1158:049e-04fb and direct 10d0:0604 keep close, session-end, and
  // destruction as distinct
  // transactions. A repeated WM_QUERYENDSESSION while DS:31c6 is already set
  // returns FALSE, and WM_DESTROY never shuts audio down.
  using ShutdownMessage = simtower::OriginalMainShutdownMessage;
  using ShutdownPlan = simtower::OriginalMainShutdownPlan;
  assert((simtower::original_main_shutdown_plan(
              ShutdownMessage::close, false, false) ==
          ShutdownPlan{true, false, false, false, false, false, 0}));
  assert((simtower::original_main_shutdown_plan(
              ShutdownMessage::close, false, true) ==
          ShutdownPlan{true, true, true, true, true, false, 0}));
  assert((simtower::original_main_shutdown_plan(
              ShutdownMessage::close, true, true) == ShutdownPlan{}));
  assert((simtower::original_main_shutdown_plan(
              ShutdownMessage::query_end_session, false, false) ==
          ShutdownPlan{true, false, false, false, false, false, 0}));
  assert((simtower::original_main_shutdown_plan(
              ShutdownMessage::query_end_session, false, true) ==
          ShutdownPlan{true, true, true, false, false, false, 1}));
  assert((simtower::original_main_shutdown_plan(
              ShutdownMessage::exit_command, false, true) ==
          ShutdownPlan{true, true, true, true, false, false, 0}));
  assert((simtower::original_main_shutdown_plan(
              ShutdownMessage::exit_command, true, false) ==
          ShutdownPlan{false, false, false, true, false, false, 0}));
  assert((simtower::original_main_shutdown_plan(
              ShutdownMessage::query_end_session, true, true) ==
          ShutdownPlan{}));
  assert((simtower::original_main_shutdown_plan(
              ShutdownMessage::destroy, false, false) ==
          ShutdownPlan{false, false, false, false, false, true, 0}));

  // 10b8:0000 destroys the three auxiliary windows Map, Info, Command.
  using Auxiliary = simtower::OriginalAuxiliaryWindow;
  assert((simtower::kOriginalAuxiliaryShutdownOrder ==
          std::array<Auxiliary, 3>{
              Auxiliary::map, Auxiliary::info, Auxiliary::command}));

  // Direct 1208:0a8d/0ba7 coverage: initialization seeds height nine with
  // default output precision; later entries use TrueType-only precision.
  // 0ba7 checks saturation before clamping/searching, so a full bank refuses
  // even a height already present.
  using FontAction = simtower::OriginalFontCacheAction;
  using FontDecision = simtower::OriginalFontCacheDecision;
  constexpr std::array<std::int16_t, 0> kNoFonts{};
  constexpr std::array<std::int16_t, 1> kInitialFont{9};
  constexpr std::array<std::int16_t, 3> kThreeFonts{9, 13, 16};
  constexpr std::array<std::int16_t, 10> kFullFontBank{
      9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
  assert((simtower::original_font_cache_decision(kNoFonts, -32768) ==
          FontDecision{FontAction::create_and_select, 0U, 9}));
  assert((simtower::original_font_cache_decision(kInitialFont, 8) ==
          FontDecision{FontAction::select_existing, 0U, 9}));
  assert((simtower::original_font_cache_decision(kThreeFonts, 13) ==
          FontDecision{FontAction::select_existing, 1U, 13}));
  assert((simtower::original_font_cache_decision(kThreeFonts, 14) ==
          FontDecision{FontAction::create_and_select, 3U, 14}));
  assert((simtower::original_font_cache_decision(kFullFontBank, 9) ==
          FontDecision{FontAction::no_selection, 10U, 9}));
  assert((simtower::original_font_creation_spec(9, true) ==
          simtower::OriginalFontCreationSpec{9, 1U, 0U}));
  assert((simtower::original_font_creation_spec(14, false) ==
          simtower::OriginalFontCreationSpec{14, 1U, 7U}));
  // Direct 1208:0b2b callback coverage: only exact-case Arial terminates the
  // enumeration; prefixes, suffixes, and case variants retain the fallback.
  static_assert(simtower::original_font_face_is_arial("Arial"));
  static_assert(!simtower::original_font_face_is_arial("arial"));
  static_assert(!simtower::original_font_face_is_arial("Arial Narrow"));
  static_assert(!simtower::original_font_face_is_arial("Arial "));

  // Direct 1208:0b6a lifecycle coverage. These GDI calls allocate and delete
  // only memory font handles; they do not create or display a window.
  simtower::initialize_original_font_cache();
  assert(simtower::original_cached_font(9) != nullptr);
  assert(simtower::original_cached_font(14) != nullptr);
  simtower::destroy_original_font_cache();
  simtower::initialize_original_font_cache();
  assert(simtower::original_cached_font(9) != nullptr);
  simtower::destroy_original_font_cache();

  // 11e0:0cfb loads GROUP_CURSOR/1002..1005 and 11e0:0d80 selects those
  // handles by the resource ID. Creating and inspecting them is a purely
  // memory/GDI test: it does not create or show a window.
  for (std::uint16_t id = 1002U; id <= 1005U; ++id) {
    const HCURSOR cursor = simtower::create_original_cursor(resources, id);
    assert(cursor != nullptr);
    ICONINFO info{};
    assert(GetIconInfo(cursor, &info));
    assert(!info.fIcon);
    const auto raw = resources.find("CURSOR", id - 1001U);
    assert(raw.size() >= 4U);
    const auto expected_hotspot_x = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(raw[0]) |
        (static_cast<std::uint16_t>(raw[1]) << 8U));
    const auto expected_hotspot_y = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(raw[2]) |
        (static_cast<std::uint16_t>(raw[3]) << 8U));
    assert(info.xHotspot == expected_hotspot_x);
    assert(info.yHotspot == expected_hotspot_y);
    if (info.hbmColor) DeleteObject(info.hbmColor);
    if (info.hbmMask) DeleteObject(info.hbmMask);
    assert(DestroyCursor(cursor));
  }
  assert(simtower::select_original_main_cursor(true, 0U, false) == 1005U);
  assert(simtower::select_original_main_cursor(true, 1U, false) == 1004U);
  assert(simtower::select_original_main_cursor(true, 1U, true) == 1000U);
  assert(simtower::select_original_main_cursor(true, 2U, false) == 1003U);
  assert(simtower::select_original_main_cursor(true, 3U, false) == 1002U);
  assert(simtower::select_original_main_cursor(false, 2U, false) == 1003U);
  assert(simtower::select_original_main_cursor(false, 1U, false) == 0U);

  // 1258:0505 first checks enabled Map/Info/Command screen rectangles, then
  // the non-iconic main client. Outside/disabled/iconic branches make no
  // 11e0:0d80 call and therefore preserve the cursor already installed.
  using CursorPointPlan = simtower::OriginalMainCursorPointPlan;
  assert((simtower::original_main_cursor_point_plan(
              true, true, false, false, false, false,
              true, false, true, 0U, false) ==
          CursorPointPlan{true, 0U}));
  assert((simtower::original_main_cursor_point_plan(
              true, false, false, false, false, false,
              true, false, true, 0U, false) ==
          CursorPointPlan{true, 1005U}));
  assert((simtower::original_main_cursor_point_plan(
              false, false, true, true, false, false,
              false, false, true, 1U, false) ==
          CursorPointPlan{true, 0U}));
  assert((simtower::original_main_cursor_point_plan(
              false, false, false, false, true, true,
              false, false, true, 1U, true) ==
          CursorPointPlan{true, 0U}));
  assert((simtower::original_main_cursor_point_plan(
              false, false, false, false, false, false,
              true, false, true, 1U, true) ==
          CursorPointPlan{true, 1000U}));
  assert((simtower::original_main_cursor_point_plan(
              false, false, false, false, false, false,
              false, false, true, 3U, false) == CursorPointPlan{}));
  assert((simtower::original_main_cursor_point_plan(
              false, false, false, false, false, false,
              true, true, true, 3U, false) == CursorPointPlan{}));

  // MAINWNDPROC 1158:0314 updates the cursor before its modal gate. The lock
  // suppresses only the later shared-world dispatch at 1158:032a.
  using MouseMovePlan = simtower::OriginalMainMouseMovePlan;
  assert((simtower::original_main_mouse_move_plan(false) ==
          MouseMovePlan{true, true}));
  assert((simtower::original_main_mouse_move_plan(true) ==
          MouseMovePlan{true, false}));

  // Direct 1258:0186 coverage: 0195-01c3 activates only for a non-iconic
  // active main window and
  // deactivates only when DS:0252's audio latch is currently set. Failed or
  // master-disabled 11c8:0aab attempts therefore retry on later idle passes.
  using IdleAudio = simtower::OriginalIdleAudioTransition;
  assert(simtower::original_idle_audio_transition(true, false, false) ==
         IdleAudio::activate);
  assert(simtower::original_idle_audio_transition(true, false, true) ==
         IdleAudio::none);
  assert(simtower::original_idle_audio_transition(false, false, true) ==
         IdleAudio::deactivate);
  assert(simtower::original_idle_audio_transition(true, true, true) ==
         IdleAudio::deactivate);
  assert(simtower::original_idle_audio_transition(false, true, false) ==
         IdleAudio::none);

  // 1058:033c's ordinary off/on branches own WAVMIX deactivation/activation.
  // The special disabled-plus-Map branch delegates to 11d0:0000 instead and
  // deliberately performs neither audio transition nor the common repaint.
  using TogglePath = simtower::OriginalConstructionTogglePath;
  using TogglePlan = simtower::OriginalConstructionTogglePlan;
  assert((simtower::original_construction_toggle_plan(true, 0U) ==
          TogglePlan{TogglePath::disable_construction,
                     false, false, true, false, IdleAudio::deactivate,
                     true, true, true}));
  assert((simtower::original_construction_toggle_plan(false, 0U) ==
          TogglePlan{TogglePath::enable_construction,
                     true, false, false, true, IdleAudio::activate,
                     true, true, false}));
  assert((simtower::original_construction_toggle_plan(false, 2U) ==
          TogglePlan{TogglePath::exit_map_overlay,
                     true, true, true, true, IdleAudio::none,
                     false, false, false}));

  // 1258:01c3-023a maintains Command TOPMOST only in the absence of an
  // elevator-control window. The elevator then owns the active/inactive
  // z-order branch and suppresses Command promotion entirely.
  using IdleElevatorOrder = simtower::OriginalIdleElevatorWindowOrder;
  using IdleWindowOrderPlan = simtower::OriginalIdleWindowOrderPlan;
  assert((simtower::original_idle_window_order_plan(
              true, false, false, true, false) ==
          IdleWindowOrderPlan{true, IdleElevatorOrder::none}));
  assert((simtower::original_idle_window_order_plan(
              true, false, true, true, false) == IdleWindowOrderPlan{}));
  assert((simtower::original_idle_window_order_plan(
              true, false, false, false, false) == IdleWindowOrderPlan{}));
  assert((simtower::original_idle_window_order_plan(
              false, false, false, true, false) == IdleWindowOrderPlan{}));
  assert((simtower::original_idle_window_order_plan(
              true, true, false, true, false) ==
          IdleWindowOrderPlan{
              false, IdleElevatorOrder::promote_topmost_and_activate}));
  assert((simtower::original_idle_window_order_plan(
              true, true, false, true, true) == IdleWindowOrderPlan{}));
  assert((simtower::original_idle_window_order_plan(
              false, true, false, true, false) ==
          IdleWindowOrderPlan{
              false, IdleElevatorOrder::insert_behind_main}));

  // 1258:0244-02d1 samples/runs only with construction enabled and no active
  // Elevator-Finger capture. Scheduler false selects 1090:03ab(0)'s preview-
  // only path; scheduler true selects the complete world frame.
  using IdleWorldPlan = simtower::OriginalIdleWorldPassPlan;
  assert((simtower::original_idle_world_pass_plan(
              true, false, false) ==
          IdleWorldPlan{true, true, false, true}));
  assert((simtower::original_idle_world_pass_plan(
              true, false, true) ==
          IdleWorldPlan{true, true, true, false}));
  assert((simtower::original_idle_world_pass_plan(
              false, false, true) == IdleWorldPlan{}));
  assert((simtower::original_idle_world_pass_plan(
              true, true, true) == IdleWorldPlan{}));

  // Direct 1158:00da/0a3c/0ae5/0ba8 coverage: these are pure retained-bitmap
  // presentation variants. Only the explicit
  // 1080:0a1e and 1090:03ab boundaries may advance presentation state, and
  // 1090's ordinary full frame deliberately does not rerun 1048:03a3 sky.
  using MainSurfacePass = simtower::OriginalMainSurfacePass;
  using MainSurfacePlan = simtower::OriginalMainSurfacePassPlan;
  assert((simtower::original_main_surface_pass_plan(
              MainSurfacePass::window_paint) == MainSurfacePlan{}));
  assert((simtower::original_main_surface_pass_plan(
          MainSurfacePass::preview_repaint) ==
          MainSurfacePlan{true, false, true, false, false, false, false}));
  assert((simtower::original_main_surface_pass_plan(
          MainSurfacePass::palette_repaint) ==
          MainSurfacePlan{true, false, false, false, false, false, false}));
  assert((simtower::original_main_surface_pass_plan(
          MainSurfacePass::simulation_frame) ==
          MainSurfacePlan{true, false, true, false, true, false, true}));
  assert((simtower::original_main_surface_pass_plan(
          MainSurfacePass::rebuild_without_sky) ==
          MainSurfacePlan{true, true, true, false, true, true, false}));
  assert((simtower::original_main_surface_pass_plan(
          MainSurfacePass::rebuild_with_sky) ==
          MainSurfacePlan{true, true, true, true, true, true, false}));
  assert(simtower::original_main_wm_paint_presents_backing(false));
  assert(!simtower::original_main_wm_paint_presents_backing(true));

  // Direct 1080:0a02 coverage: its three near calls are synchronous and
  // ordered—0a1e(1) rebuilds Main with sky, 09c3 repaints Map, then 05a1
  // repaints Command. This is not an unordered invalidation fan-out.
  using FullViewStep = simtower::OriginalFullViewRefreshStep;
  assert((simtower::original_full_view_refresh_order() ==
          std::array<FullViewStep, 3>{
              FullViewStep::main_rebuild_with_sky,
              FullViewStep::map_repaint,
              FullViewStep::command_repaint}));

  // Direct 1058:05f8 scrollbar-tail coverage: this is the narrower Main then
  // XOR-focus transaction, with no MAPWNDPROC or Command repaint between.
  using ScrollbarRefreshStep = simtower::OriginalScrollbarRefreshStep;
  assert((simtower::original_scrollbar_refresh_order() ==
          std::array<ScrollbarRefreshStep, 2>{
              ScrollbarRefreshStep::main_rebuild_with_sky,
              ScrollbarRefreshStep::map_focus_adjustment}));

  // Direct 1080:0000/0054 tail coverage. Camera publication calls the bare
  // three-step 0a02 transaction above and only then performs 055d's separate
  // Map-focus adjustment.
  using CameraRefreshStep = simtower::OriginalCameraRefreshStep;
  assert((simtower::original_camera_refresh_order() ==
          std::array<CameraRefreshStep, 4>{
              CameraRefreshStep::main_rebuild_with_sky,
              CameraRefreshStep::map_repaint,
              CameraRefreshStep::command_repaint,
              CameraRefreshStep::map_focus_adjustment}));

  // Direct 10d0:001d/062a transition-tail coverage. After 10d0:0ac2's
  // derived Map focus adjustment and Fire Crew menu update, both New and a
  // successful Open call 1080:0a02 (Main, Map, Command) and then 1118:0000
  // (Info). The second Map repaint is intentional and observable.
  using DocumentRefreshStep =
      simtower::OriginalDocumentTransitionRefreshStep;
  assert((simtower::original_document_transition_refresh_order() ==
          std::array<DocumentRefreshStep, 6>{
              DocumentRefreshStep::derived_map_focus_adjustment,
              DocumentRefreshStep::fire_menu_update,
              DocumentRefreshStep::main_rebuild_with_sky,
              DocumentRefreshStep::map_repaint,
              DocumentRefreshStep::command_repaint,
              DocumentRefreshStep::info_repaint}));

  // Direct 1080:055d coverage: the first 1058:094c XOR-erases DS:7796,
  // 1080:038e recomputes that rectangle, and the second XOR draws it. No
  // MAPWNDPROC repaint or Map audio-pump work belongs to this transaction.
  using DerivedFocusStep = simtower::OriginalDerivedMapFocusStep;
  assert((simtower::original_derived_map_focus_order() ==
          std::array<DerivedFocusStep, 3>{
              DerivedFocusStep::erase_previous_focus,
              DerivedFocusStep::recompute_focus,
              DerivedFocusStep::draw_recomputed_focus}));

  // Direct 11e0:008d/1208:0cf5 coverage: 008d selects DTMP/761 items 3 and
  // 6, then both current-position additions are 16-bit and wrap before
  // MOVETO. The transport-dialog call site uses literal (+8,+1).
  const auto transport_text_dtmp = simtower::parse_original_dtmp(
      resources.find("DTMP", 761));
  assert(transport_text_dtmp.rectangles.size() == 9U);
  assert((simtower::original_relative_gdi_position(
              static_cast<std::int16_t>(transport_text_dtmp.rectangles[2].left),
              static_cast<std::int16_t>(transport_text_dtmp.rectangles[2].top),
              8, 1) == simtower::OriginalGdiPosition{28, 13}));
  assert((simtower::original_relative_gdi_position(
              static_cast<std::int16_t>(transport_text_dtmp.rectangles[5].left),
              static_cast<std::int16_t>(transport_text_dtmp.rectangles[5].top),
              8, 1) == simtower::OriginalGdiPosition{226, 59}));
  assert((simtower::original_relative_gdi_position(100, -20, 8, 1) ==
          simtower::OriginalGdiPosition{108, -19}));
  assert((simtower::original_relative_gdi_position(32767, -32768, 1, -1) ==
          simtower::OriginalGdiPosition{-32768, 32767}));

  // Direct 1208:0dfc coverage without executing either side effect: beep
  // type 0x30 precedes FatalAppExit with exit code zero and the caller-owned
  // error string.
  assert((simtower::original_fatal_exit_plan() ==
          simtower::OriginalFatalExitPlan{0x30U, 0U}));

  // 1090:061f-06dc presents a dirty Main directly and then invalidates only
  // Command. Info has an unconditional direct-DC pass; Map belongs solely to
  // 046f's independent sixteen-tick refresh and is never in this fan-out.
  using FullFrameAuxPlan =
      simtower::OriginalFullFrameAuxiliaryPresentationPlan;
  assert((simtower::original_full_frame_auxiliary_presentation_plan(false) ==
          FullFrameAuxPlan{false, false, true, false}));
  assert((simtower::original_full_frame_auxiliary_presentation_plan(true) ==
          FullFrameAuxPlan{true, true, true, false}));

  // Direct 1090:03e7-0427 coverage: changed construction rectangles use an
  // immediate owned-DC 1158:0ae5 presentation, never queued invalidation.
  assert((simtower::original_preview_only_presentation_plan(true) ==
          simtower::OriginalPreviewOnlyPresentationPlan{false, false}));
  assert((simtower::original_preview_only_presentation_plan(false) ==
          simtower::OriginalPreviewOnlyPresentationPlan{true, false}));

  // 11f8:3b94/3c13 bracket both 1090 frame forms. Restore has two audio
  // checkpoints and latches DS:025c; redraw has three and clears it. Empty,
  // isolated, disabled, and command-mode-below-three calls are true no-ops.
  simtower::OriginalConstructionPreviewScratchState preview_scratch{};
  using PreviewScratchPlan =
      simtower::OriginalConstructionPreviewScratchPlan;
  assert((simtower::original_construction_preview_restore_plan(
              preview_scratch, false, false) ==
          PreviewScratchPlan{}));
  assert((simtower::original_construction_preview_restore_plan(
              preview_scratch, true, true) ==
          PreviewScratchPlan{}));
  assert((simtower::original_construction_preview_restore_plan(
              preview_scratch, false, true) ==
          PreviewScratchPlan{2U, true, false}));
  assert(preview_scratch.backing_saved);
  assert((simtower::original_construction_preview_restore_plan(
              preview_scratch, false, true) ==
          PreviewScratchPlan{}));
  assert((simtower::original_construction_preview_draw_plan(
              preview_scratch, false, 3U) ==
          PreviewScratchPlan{}));
  assert((simtower::original_construction_preview_draw_plan(
              preview_scratch, true, 2U) ==
          PreviewScratchPlan{}));
  assert(preview_scratch.backing_saved);
  assert((simtower::original_construction_preview_draw_plan(
              preview_scratch, true, 3U) ==
          PreviewScratchPlan{3U, false, true}));
  assert(!preview_scratch.backing_saved);

  // Direct 1090:061f-06ed ordering: Info draws before its transient status
  // may expire, and the fourth effects-palette call follows that expiry.
  using FullFrameTailStep = simtower::OriginalFullFrameTailStep;
  assert((simtower::original_full_frame_tail_order() ==
          std::array<FullFrameTailStep, 6>{
              FullFrameTailStep::present_main_if_dirty,
              FullFrameTailStep::invalidate_command_if_dirty,
              FullFrameTailStep::present_info_direct,
              FullFrameTailStep::expire_info_status,
              FullFrameTailStep::step_final_effect_palette,
              FullFrameTailStep::final_audio_checkpoint}));

  // 1258:0285-02c9 refreshes Elevator Control after every full frame only
  // when all three original HWND/init/backing gates are live. It deliberately
  // has no elevator-changed input.
  assert(simtower::original_idle_elevator_refresh_required(
      true, true, true, true));
  assert(!simtower::original_idle_elevator_refresh_required(
      false, true, true, true));
  assert(!simtower::original_idle_elevator_refresh_required(
      true, false, true, true));
  assert(!simtower::original_idle_elevator_refresh_required(
      true, true, false, true));
  assert(!simtower::original_idle_elevator_refresh_required(
      true, true, true, false));

  // 1078:0000 hides only enabled palettes on minimize, in Info/Command/Map
  // order, without changing the persisted View-menu state.
  using Auxiliary = simtower::OriginalAuxiliaryWindow;
  using Operation = simtower::OriginalAuxiliaryWindowOperation;
  using InsertAfter = simtower::OriginalAuxiliaryInsertAfter;
  using Action = simtower::OriginalAuxiliaryWindowAction;
  constexpr std::array<std::uint16_t, 22> kMainMessages = {
      0x0001U, 0x0002U, 0x0005U, 0x0006U, 0x000fU, 0x0010U,
      0x0011U, 0x001cU, 0x0024U, 0x0084U, 0x0104U, 0x0105U,
      0x0111U, 0x0114U, 0x0115U, 0x0200U, 0x0201U, 0x0202U,
      0x0203U, 0x030fU, 0x0311U, 0x03bdU,
  };
  // Direct 1158:06b9 coverage: literal parallel-table order, the generic
  // 3000..4001 dialog range after table exclusion, and DefWindowProc tails.
  constexpr std::array<std::uint16_t, 27> kMainCommands = {
      3000U,  3001U,  3002U,  9000U,  9001U,  9002U,  9003U,
      40001U, 40002U, 40003U, 40004U, 40005U, 40007U, 40008U,
      40009U, 40010U, 40011U, 40012U, 40013U, 40014U, 40015U,
      40016U, 40017U, 40018U, 40019U, 40020U, 40021U,
  };
  // Direct INFOWNDPROC 1120:0000 coverage: its parallel table has exactly ten
  // messages. In particular WM_COMMAND is a table entry that still reaches
  // DefWindowProc, while WM_CREATE and WM_LBUTTONUP are not table entries.
  constexpr std::array<std::uint16_t, 10> kInfoMessages = {
      0x0002U, 0x0006U, 0x000fU, 0x001cU, 0x0084U,
      0x0086U, 0x0111U, 0x0201U, 0x030fU, 0x0311U,
  };
  constexpr std::array<std::uint16_t, 13> kMapMessages = {
      0x0001U, 0x0002U, 0x0006U, 0x000fU, 0x001cU,
      0x0084U, 0x0086U, 0x0111U, 0x0200U, 0x0201U,
      0x0202U, 0x030fU, 0x0311U,
  };
  assert(simtower::kOriginalMainWindowMessages == kMainMessages);
  assert(simtower::kOriginalMainCommandIds == kMainCommands);
  assert(simtower::original_main_command_is_table_entry(3000U));
  assert(simtower::original_main_command_is_table_entry(40021U));
  assert(!simtower::original_main_command_is_table_entry(3003U));
  assert(!simtower::original_main_command_is_table_entry(4001U));
  assert(!simtower::original_main_command_is_table_entry(0xffffU));
  using MainCommandRoute = simtower::OriginalMainCommandRoute;
  assert(simtower::original_main_command_route(3000U) ==
         MainCommandRoute::table_entry);
  assert(simtower::original_main_command_route(3003U) ==
         MainCommandRoute::generic_dialog_then_def_window_proc);
  assert(simtower::original_main_command_route(4001U) ==
         MainCommandRoute::generic_dialog_then_def_window_proc);
  assert(simtower::original_main_command_route(2999U) ==
         MainCommandRoute::def_window_proc);
  assert(simtower::original_main_command_route(4002U) ==
         MainCommandRoute::def_window_proc);
  assert(simtower::original_main_command_route(0xffffU) ==
         MainCommandRoute::def_window_proc);
  assert(simtower::kOriginalInfoWindowMessages == kInfoMessages);
  assert(simtower::kOriginalMapWindowMessages == kMapMessages);
  const RECT info_client{0, 0, 431, 41};
  const RECT info_validation =
      simtower::original_palette_activation_validation_rect(info_client);
  assert(info_validation.left == 0 && info_validation.top == 0 &&
         info_validation.right == 431 && info_validation.bottom == 8);
  assert(simtower::original_auxiliary_window_actions(
             false, true, true, true) ==
         std::vector<Action>({
             {Auxiliary::info, Operation::hide, InsertAfter::top},
             {Auxiliary::command, Operation::hide, InsertAfter::top},
             {Auxiliary::map, Operation::hide, InsertAfter::top},
         }));
  assert(simtower::original_auxiliary_window_actions(
             false, false, true, false) ==
         std::vector<Action>({
             {Auxiliary::info, Operation::hide, InsertAfter::top},
         }));

  // Restore promotes Command to TOPMOST, then inserts both secondary
  // palettes directly behind it. With Command disabled they use HWND_TOP.
  assert(simtower::original_auxiliary_window_actions(
             true, true, true, true) ==
         std::vector<Action>({
             {Auxiliary::command, Operation::show, InsertAfter::topmost},
             {Auxiliary::info, Operation::show, InsertAfter::command},
             {Auxiliary::map, Operation::show, InsertAfter::command},
         }));
  assert(simtower::original_auxiliary_window_actions(
             true, false, true, true) ==
         std::vector<Action>({
             {Auxiliary::info, Operation::show, InsertAfter::top},
             {Auxiliary::map, Operation::show, InsertAfter::top},
         }));
  assert(simtower::original_auxiliary_window_actions(
             true, true, false, false) ==
         std::vector<Action>({
             {Auxiliary::command, Operation::show, InsertAfter::topmost},
         }));
  assert(simtower::original_auxiliary_window_actions(
             true, false, false, false).empty());

  // 1158:0886/08a6/08c4 and the three palette close boxes alter visibility
  // without touching the View-menu checkmarks. This odd stale-mark behavior
  // is observable and must not be normalized by the native adapter.
  using VisibilityTrigger = simtower::OriginalAuxiliaryVisibilityTrigger;
  using VisibilityPlan = simtower::OriginalAuxiliaryVisibilityPlan;
  assert((simtower::original_auxiliary_visibility_plan(
              VisibilityTrigger::menu_command, true) ==
          VisibilityPlan{false, Operation::hide, false}));
  assert((simtower::original_auxiliary_visibility_plan(
              VisibilityTrigger::menu_command, false) ==
          VisibilityPlan{true, Operation::show, false}));
  assert((simtower::original_auxiliary_visibility_plan(
              VisibilityTrigger::close_box, true) ==
          VisibilityPlan{false, Operation::hide, false}));
  assert((simtower::original_auxiliary_visibility_plan(
              VisibilityTrigger::close_box, false) ==
          VisibilityPlan{false, Operation::hide, false}));

  using ActivationInsertAfter =
      simtower::OriginalAuxiliaryActivationInsertAfter;
  using ActivationAction = simtower::OriginalAuxiliaryActivationAction;
  assert(simtower::original_auxiliary_activation_actions(
             false, true, true, true) ==
         std::vector<ActivationAction>({
             {Auxiliary::command, ActivationInsertAfter::main},
             {Auxiliary::info, ActivationInsertAfter::main},
             {Auxiliary::map, ActivationInsertAfter::main},
         }));
  assert(simtower::original_auxiliary_activation_actions(
             false, false, true, false) ==
         std::vector<ActivationAction>({
             {Auxiliary::info, ActivationInsertAfter::main},
         }));
  assert(simtower::original_auxiliary_activation_actions(
             true, true, true, true) ==
         std::vector<ActivationAction>({
             {Auxiliary::command, ActivationInsertAfter::top},
             {Auxiliary::command, ActivationInsertAfter::top},
             {Auxiliary::command, ActivationInsertAfter::topmost},
         }));
  assert(simtower::original_auxiliary_activation_actions(
             true, false, true, true) ==
         std::vector<ActivationAction>({
             {Auxiliary::command, ActivationInsertAfter::top},
             {Auxiliary::command, ActivationInsertAfter::top},
         }));

  // All three palette WM_ACTIVATEAPP handlers promote/show Main first only
  // for activation and only when DS:3258 holds a live window, then always
  // forward the incoming boolean to 1078:01e8.
  using AppActivationPlan = simtower::OriginalPaletteAppActivationPlan;
  assert((simtower::original_palette_app_activation_plan(true, true) ==
          AppActivationPlan{true, true}));
  assert((simtower::original_palette_app_activation_plan(true, false) ==
          AppActivationPlan{false, true}));
  assert((simtower::original_palette_app_activation_plan(false, true) ==
          AppActivationPlan{false, false}));

  // 1050:0063/1120:005e/1168:006a redirect only to DS:31a4's active modal,
  // not Main. Map inserts behind that modal first and Command also forwards
  // focus. These active redirects skip the DS:31a6 write; inactive Command
  // with a modal also returns early, unlike inactive Info and Map.
  using PalettePlan = simtower::OriginalPaletteWindowActivationPlan;
  assert((simtower::original_palette_window_activation_plan(
              Auxiliary::command, true, true, true, true) ==
          PalettePlan{false, true, true, false, false, false, false, false}));
  assert((simtower::original_palette_window_activation_plan(
              Auxiliary::info, true, true, true, true) ==
          PalettePlan{false, true, false, false, false, false, false, false}));
  assert((simtower::original_palette_window_activation_plan(
              Auxiliary::map, true, true, true, true) ==
          PalettePlan{true, true, false, false, false, false, false, false}));
  assert((simtower::original_palette_window_activation_plan(
              Auxiliary::command, false, true, true, true) ==
          PalettePlan{false, false, false, false, false, false, false, false}));
  assert((simtower::original_palette_window_activation_plan(
              Auxiliary::map, false, true, true, true) ==
          PalettePlan{true, false, false, false, false, false, true, false}));

  // With no modal, enabled Command is restored TOPMOST only when the previous
  // shared activation word was nonzero. The new word is written afterward;
  // inactive promotion includes NOACTIVATE, active promotion does not.
  assert((simtower::original_palette_window_activation_plan(
              Auxiliary::command, true, false, true, true) ==
          PalettePlan{false, false, false, true, true, false, true, true}));
  assert((simtower::original_palette_window_activation_plan(
              Auxiliary::command, false, false, true, true) ==
          PalettePlan{false, false, false, false, true, true, true, false}));
  assert((simtower::original_palette_window_activation_plan(
              Auxiliary::command, true, false, false, true) ==
          PalettePlan{false, false, false, true, false, false, true, true}));
  assert((simtower::original_palette_window_activation_plan(
              Auxiliary::command, true, false, true, false) ==
          PalettePlan{false, false, false, true, false, false, true, true}));
  assert((simtower::original_palette_window_activation_plan(
              Auxiliary::info, false, false, true, true) ==
          PalettePlan{false, false, false, false, false, false, true, false}));
  assert((simtower::original_palette_window_activation_plan(
              Auxiliary::map, true, false, true, false) ==
          PalettePlan{false, false, false, true, false, false, true, true}));

  // Complete top-level palette paint gates from CMDBTNWNDPROC 1050:010d,
  // INFOWNDPROC 1120:0215, and MAPWNDPROC 1168:016a. Command always expands
  // the update and realizes the palette before its content gates; Info does so
  // only while visible and drawable; Map preserves the region and likewise
  // realizes only when drawable.
  using PaintPlan = simtower::OriginalAuxiliaryPaintPlan;
  assert((simtower::original_auxiliary_paint_plan(
              Auxiliary::command, false, true, false) ==
          PaintPlan{true, true, false}));
  assert((simtower::original_auxiliary_paint_plan(
              Auxiliary::info, true, true, false) ==
          PaintPlan{true, true, true}));
  assert((simtower::original_auxiliary_paint_plan(
              Auxiliary::info, false, true, false) ==
          PaintPlan{false, false, false}));
  assert((simtower::original_auxiliary_paint_plan(
              Auxiliary::map, true, true, false) ==
          PaintPlan{false, true, true}));
  assert((simtower::original_auxiliary_paint_plan(
              Auxiliary::map, true, false, false) ==
          PaintPlan{false, false, false}));
  assert((simtower::original_auxiliary_paint_plan(
              Auxiliary::command, true, true, true) ==
          PaintPlan{true, true, false}));

  // MAINWNDPROC 1158:04fe/0508 realizes QUERYNEWPALETTE and non-self
  // PALETTECHANGED messages but returns zero for both. The three auxiliary
  // auxiliary procedures honor the startup/closing gates for both messages;
  // their QUERYNEWPALETTE branches always realize while PALETTECHANGED also
  // rejects a self-sent notification.
  using PaletteMessage = simtower::OriginalPaletteMessageKind;
  using PaletteMessagePlan = simtower::OriginalPaletteMessagePlan;
  assert((simtower::original_main_palette_message_plan(
              PaletteMessage::query_new_palette, true) ==
          PaletteMessagePlan{true, 0}));
  assert((simtower::original_main_palette_message_plan(
              PaletteMessage::palette_changed, false) ==
          PaletteMessagePlan{true, 0}));
  assert((simtower::original_main_palette_message_plan(
              PaletteMessage::palette_changed, true) ==
          PaletteMessagePlan{false, 0}));
  assert((simtower::original_auxiliary_palette_changed_plan(
              false, true, false) == PaletteMessagePlan{true, 0}));
  assert((simtower::original_auxiliary_palette_changed_plan(
              true, true, false) == PaletteMessagePlan{false, 0}));
  assert((simtower::original_auxiliary_palette_changed_plan(
              false, false, false) == PaletteMessagePlan{false, 0}));
  assert((simtower::original_auxiliary_palette_changed_plan(
              false, true, true) == PaletteMessagePlan{false, 0}));
  assert((simtower::original_auxiliary_palette_message_plan(
              PaletteMessage::query_new_palette, true, true, false) ==
          PaletteMessagePlan{true, 0}));
  assert((simtower::original_auxiliary_palette_message_plan(
              PaletteMessage::query_new_palette, false, false, false) ==
          PaletteMessagePlan{false, 0}));
  assert((simtower::original_auxiliary_palette_message_plan(
              PaletteMessage::query_new_palette, false, true, true) ==
          PaletteMessagePlan{false, 0}));

  // CMDBTNSUBWNDPROC's seven-message table at 1050:095c/096a routes only
  // WM_PALETTECHANGED to 090a. It returns TRUE on both paths, but only a
  // non-self message realizes and conditionally updates the existing colors.
  using SelectorPalettePlan =
      simtower::OriginalCommandSelectorPaletteChangedPlan;
  assert((simtower::original_command_selector_palette_changed_plan(true) ==
          SelectorPalettePlan{false, false, 1}));
  assert((simtower::original_command_selector_palette_changed_plan(false) ==
          SelectorPalettePlan{true, true, 1}));

  using PaletteSurface = simtower::OriginalPaletteSurface;
  assert(simtower::original_palette_repaint_order(false, true).empty());
  assert(simtower::original_palette_repaint_order(true, false).empty());
  assert(simtower::original_palette_repaint_order(true, true) ==
         std::vector<PaletteSurface>({
             PaletteSurface::map,
             PaletteSurface::info,
             PaletteSurface::command,
             PaletteSurface::main,
         }));

  // 1158:0c29 does not dispatch four WM_PAINTs. It directly invokes the Map,
  // Command, and Main presentation paths, reusing the caller's DC only for the
  // originating surface; Info alone invalidates and calls UpdateWindow.
  using RepaintMechanism = simtower::OriginalPaletteRepaintMechanism;
  using RepaintAction = simtower::OriginalPaletteRepaintAction;
  assert(simtower::original_palette_repaint_actions(
             true, true, PaletteSurface::main) ==
         std::vector<RepaintAction>({
             {PaletteSurface::map, RepaintMechanism::direct_dc,
              true, false, true, true},
             {PaletteSurface::info, RepaintMechanism::update_window,
              true, false, false, false},
             {PaletteSurface::command, RepaintMechanism::direct_dc,
              false, false, true, true},
             {PaletteSurface::main, RepaintMechanism::direct_dc,
              true, true, false, false},
         }));
  assert(simtower::original_palette_repaint_actions(
             true, true, PaletteSurface::map) ==
         std::vector<RepaintAction>({
             {PaletteSurface::map, RepaintMechanism::direct_dc,
              true, true, false, false},
             {PaletteSurface::info, RepaintMechanism::update_window,
              true, false, false, false},
             {PaletteSurface::command, RepaintMechanism::direct_dc,
              false, false, true, true},
             {PaletteSurface::main, RepaintMechanism::direct_dc,
              true, false, true, true},
         }));
  assert(simtower::original_palette_repaint_actions(
             false, true, PaletteSurface::main).empty());
  assert(simtower::original_palette_repaint_actions(
             true, false, PaletteSurface::main).empty());

  // 1158:029f is the sole press branch that arms DS:0242. The normal Windows
  // down/up/double-click sequence reaches 1158:028c with the latch cleared,
  // so the second press must not begin another edit/construction transaction.
  using PressPhase = simtower::OriginalMainPointerPressPhase;
  assert(simtower::original_main_pointer_begins_interaction(
      PressPhase::button_down, true));
  assert(!simtower::original_main_pointer_begins_interaction(
      PressPhase::button_down, false));
  assert(!simtower::original_main_pointer_begins_interaction(
      PressPhase::double_click, true));
  assert(!simtower::original_main_pointer_begins_interaction(
      PressPhase::double_click, false));

  // Direct 1058:0000 coverage: modifiers publish before every gate, and the
  // complete message/mode matrix preserves emergency, tool, and Build-state
  // priority through the final 11f8:07d8 construction dispatch.
  using InputModifiers = simtower::OriginalWorldInputModifiers;
  assert((simtower::original_world_input_modifiers(0U) ==
          InputModifiers{false, false}));
  assert((simtower::original_world_input_modifiers(0x0004U) ==
          InputModifiers{false, true}));
  assert((simtower::original_world_input_modifiers(0x0008U) ==
          InputModifiers{true, false}));
  assert((simtower::original_world_input_modifiers(0xffffU) ==
          InputModifiers{true, true}));

  using InputMessage = simtower::OriginalWorldInputMessage;
  using InputAction = simtower::OriginalWorldInputAction;
  using InputPlan = simtower::OriginalWorldInputPlan;
  assert((simtower::original_world_input_plan(
              InputMessage::button_down, false, false, false,
              3U, true, true) == InputPlan{}));
  assert((simtower::original_world_input_plan(
              InputMessage::button_down, true, true, true,
              3U, true, true) == InputPlan{}));
  assert((simtower::original_world_input_plan(
              InputMessage::button_down, true, false, true,
              3U, true, true) ==
          InputPlan{InputAction::emergency_feedback, false}));
  assert((simtower::original_world_input_plan(
              InputMessage::button_up, true, false, true,
              3U, true, false) == InputPlan{}));
  assert((simtower::original_world_input_plan(
              InputMessage::double_click, true, false, true,
              0U, true, true) == InputPlan{}));
  assert((simtower::original_world_input_plan(
              InputMessage::button_down, true, false, false,
              0U, true, true) ==
          InputPlan{InputAction::bulldozer, false}));
  assert((simtower::original_world_input_plan(
              InputMessage::button_down, true, false, false,
              0U, false, true) ==
          InputPlan{InputAction::bulldozer, false}));
  assert((simtower::original_world_input_plan(
              InputMessage::mouse_move, true, false, false,
              0U, true, true) == InputPlan{}));
  assert((simtower::original_world_input_plan(
              InputMessage::button_down, true, false, false,
              1U, true, true) ==
          InputPlan{InputAction::elevator_finger, false}));
  assert((simtower::original_world_input_plan(
              InputMessage::mouse_move, true, false, false,
              1U, true, false) == InputPlan{}));
  assert((simtower::original_world_input_plan(
              InputMessage::mouse_move, true, false, false,
              1U, true, true) ==
          InputPlan{InputAction::elevator_finger, false}));
  assert((simtower::original_world_input_plan(
              InputMessage::double_click, true, false, false,
              1U, true, false) ==
          InputPlan{InputAction::elevator_finger, false}));
  assert((simtower::original_world_input_plan(
              InputMessage::button_up, true, false, false,
              1U, false, false) ==
          InputPlan{InputAction::elevator_finger, false}));
  assert((simtower::original_world_input_plan(
              InputMessage::button_down, true, false, false,
              2U, false, true) ==
          InputPlan{InputAction::magnifier, true}));
  assert((simtower::original_world_input_plan(
              InputMessage::button_up, true, false, false,
              2U, false, false) ==
          InputPlan{InputAction::none, true}));
  assert((simtower::original_world_input_plan(
              InputMessage::mouse_move, true, false, false,
              2U, true, false) ==
          InputPlan{InputAction::none, true}));
  assert((simtower::original_world_input_plan(
              InputMessage::button_down, true, false, false,
              3U, false, true) == InputPlan{}));
  assert((simtower::original_world_input_plan(
              InputMessage::button_up, true, false, false,
              3U, true, false) ==
          InputPlan{InputAction::construction, false}));
  assert((simtower::original_world_input_plan(
              InputMessage::double_click, true, false, false,
              0xffffU, true, false) ==
          InputPlan{InputAction::construction, false}));

  // Direct 10a0:0544 coverage: every otherwise-unhandled down captures Main;
  // only the adjacent upper/lower cap selects a drag direction. On double-
  // click, a hit opens Control and clears DS:02a6, while a miss releases the
  // HWND capture but preserves that latch until the following button-up.
  using FingerPressPath = simtower::OriginalElevatorFingerPressPath;
  assert(simtower::original_elevator_finger_press_path(
             true, true, 11, 10, 12) ==
         FingerPressPath::service_floor);
  assert(simtower::original_elevator_finger_press_path(
             true, true, 13, 10, 12) ==
         FingerPressPath::capture_upper_cap);
  assert(simtower::original_elevator_finger_press_path(
             true, true, 9, 10, 12) ==
         FingerPressPath::capture_lower_cap);
  assert(simtower::original_elevator_finger_press_path(
             true, false, 11, 10, 12) ==
         FingerPressPath::capture_only);
  assert(simtower::original_elevator_finger_press_path(
             false, false, 0, 0, 0) ==
         FingerPressPath::capture_only);
  using FingerDoubleClickPlan =
      simtower::OriginalElevatorFingerDoubleClickPlan;
  assert((simtower::original_elevator_finger_double_click_plan(true) ==
          FingerDoubleClickPlan{true, true}));
  assert((simtower::original_elevator_finger_double_click_plan(false) ==
          FingerDoubleClickPlan{false, false}));

  // 1158:028c/029f/02d5 consult both the modal lock (DS:24b8) and active-paint
  // latch (DS:0244) before touching DS:0242. Double-click still reaches the
  // shared 1058 dispatcher, but deliberately does not arm the latch itself.
  using PointerMessagePhase = simtower::OriginalMainPointerMessagePhase;
  using PointerMessagePlan = simtower::OriginalMainPointerMessagePlan;
  assert((simtower::original_main_pointer_message_plan(
              PointerMessagePhase::button_down, false, false, true, false) ==
          PointerMessagePlan{true, true, false}));
  assert((simtower::original_main_pointer_message_plan(
              PointerMessagePhase::button_down, false, false, false, false) ==
          PointerMessagePlan{}));
  assert((simtower::original_main_pointer_message_plan(
              PointerMessagePhase::double_click, false, false, true, false) ==
          PointerMessagePlan{true, false, false}));
  assert((simtower::original_main_pointer_message_plan(
              PointerMessagePhase::button_up, false, false, false, true) ==
          PointerMessagePlan{true, false, true}));
  assert((simtower::original_main_pointer_message_plan(
              PointerMessagePhase::button_up, false, false, true, true) ==
          PointerMessagePlan{}));
  assert((simtower::original_main_pointer_message_plan(
              PointerMessagePhase::button_up, false, false, false, false) ==
          PointerMessagePlan{}));
  assert((simtower::original_main_pointer_message_plan(
              PointerMessagePhase::button_down, true, false, true, false) ==
          PointerMessagePlan{}));
  assert((simtower::original_main_pointer_message_plan(
              PointerMessagePhase::button_up, false, true, false, true) ==
          PointerMessagePlan{}));

  // MAINWNDPROC 1158:015b/01e7 uses 16-pixel line steps and a viewport-minus-
  // 16 page step. Codes 4/5 bypass that arithmetic and pass the raw message
  // field to SetScrollPos; this catches substituting SCROLLINFO.nTrackPos for
  // the final SB_THUMBPOSITION message in the Win32 bridge.
  assert(simtower::original_main_scroll_request_position(
             0U, 100, 999U, 640, 2360) == 84);
  assert(simtower::original_main_scroll_request_position(
             1U, 2352, 999U, 640, 2360) == 2360);
  assert(simtower::original_main_scroll_request_position(
             2U, 1000, 999U, 640, 2360) == 376);
  assert(simtower::original_main_scroll_request_position(
             3U, 1000, 999U, 640, 2360) == 1624);
  assert(simtower::original_main_scroll_request_position(
             2U, 100, 999U, 640, 2360) == 0);
  assert(simtower::original_main_scroll_request_position(
             4U, 100, 1731U, 640, 2360) == 1731);
  assert(simtower::original_main_scroll_request_position(
             5U, 100, 2047U, 640, 2360) == 2047);
  assert(!simtower::original_main_scroll_request_position(
      6U, 100, 999U, 640, 2360));

  // 1158:041c gates WM_SIZE on DS:02a4, which 1258:04e2 sets only after the
  // full 1128:0005 startup/show sequence. Unknown post-startup size codes
  // retain only the leading invalidation.
  using SizeDisposition = simtower::OriginalMainSizeDisposition;
  assert(simtower::original_main_size_disposition(false, 0U) ==
         SizeDisposition::ignore);
  assert(simtower::original_main_size_disposition(false, 1U) ==
         SizeDisposition::ignore);
  assert(simtower::original_main_size_disposition(true, 0U) ==
         SizeDisposition::restore_or_maximize);
  assert(simtower::original_main_size_disposition(true, 1U) ==
         SizeDisposition::minimize);
  assert(simtower::original_main_size_disposition(true, 2U) ==
         SizeDisposition::restore_or_maximize);
  assert(simtower::original_main_size_disposition(true, 3U) ==
         SizeDisposition::invalidate_only);

  // 1158:05ef installs max=world-client through Win16 SetScrollRange and
  // 1080:00d7 clamps the saved position. A zero Win32 page size preserves
  // the original fixed system thumb instead of inventing a proportional one.
  using ResizeState = simtower::OriginalMainScrollbarResizeState;
  assert((simtower::original_main_scrollbar_resize_state(1731, 640, 3000) ==
          ResizeState{0, 2360, 0U, 1731}));
  assert((simtower::original_main_scrollbar_resize_state(2500, 640, 3000) ==
          ResizeState{0, 2360, 0U, 2360}));
  assert((simtower::original_main_scrollbar_resize_state(-16, 640, 3000) ==
          ResizeState{0, 2360, 0U, 0}));
  assert((simtower::original_main_scrollbar_resize_state(3420, 576, 4320) ==
          ResizeState{0, 3744, 0U, 3420}));

  // 1128:02aa caps the desktop-derived client maxima at 816x576, 1128:08d6
  // applies the one-pixel startup extent convention, and 1158:0334 builds the
  // four tracking dimensions from the same bounds and system metrics.
  const simtower::OriginalMainNonclientMetrics classic_metrics{
      4, 4, 16, 16, 19, 19, 1, 1};
  using MainGeometry = simtower::OriginalMainWindowGeometry;
  assert((simtower::original_main_window_geometry(
              1024, 768, classic_metrics) ==
          MainGeometry{204, 53, 815, 575, 124, 219, 839, 637}));
  // On a small desktop the maximum is derived before applying the 816x576
  // caps. The old native host incorrectly retained the large-desktop limits.
  assert((simtower::original_main_window_geometry(
              640, 480, classic_metrics) ==
          MainGeometry{204, 53, 436, 425, 124, 219, 647, 487}));

  // 1128:05eb-08b2 constructs Command, Info, and Map in that order. The
  // command SETWINDOWPOS flags are literal 0x000a (not 0x000b): it becomes
  // topmost and is resized from its border-inflated 65x102 creation extent to
  // a raw 63x100 outer window. All three receive IDs and the same hidden-DC
  // font/alignment/background state before Main is created at 1128:08d6.
  using Auxiliary = simtower::OriginalAuxiliaryWindow;
  using StartupInsert = simtower::OriginalStartupAuxiliaryInsertAfter;
  using StartupSpec = simtower::OriginalStartupAuxiliaryWindowSpec;
  const auto startup_auxiliary =
      simtower::original_startup_auxiliary_window_specs(1, 1);
  assert((startup_auxiliary == std::array{
      StartupSpec{Auxiliary::command, 134, 178, 63, 100, 65, 102, 63, 100,
                  1000U, StartupInsert::topmost, 0x000aU, 9, 1U, 1U},
      StartupSpec{Auxiliary::info, 204, 4, 431, 49, 433, 51, 0, 0,
                  1001U, StartupInsert::top, 0x000bU, 9, 1U, 1U},
      StartupSpec{Auxiliary::map, 2, 4, 200, 314, 202, 316, 0, 0,
                  1002U, StartupInsert::top, 0x000bU, 9, 1U, 1U},
  }));
  const auto scaled_border_auxiliary =
      simtower::original_startup_auxiliary_window_specs(2, 3);
  assert(scaled_border_auxiliary[0].create_outer_width == 67);
  assert(scaled_border_auxiliary[0].create_outer_height == 106);
  assert(scaled_border_auxiliary[0].set_position_width == 63);
  assert(scaled_border_auxiliary[0].set_position_height == 100);
  assert(scaled_border_auxiliary[1].create_outer_width == 435);
  assert(scaled_border_auxiliary[1].create_outer_height == 55);
  assert(scaled_border_auxiliary[1].set_position_width == 0);
  assert(scaled_border_auxiliary[1].set_position_height == 0);

  std::size_t bitmap_count = 0;
  for (const auto& descriptor : simtower::generated::kResources) {
    if (descriptor.type != "BITMAP") {
      continue;
    }
    const auto dib = simtower::original_dib_view(
        resources.find("BITMAP", descriptor.numeric_id));
    assert(dib.width > 0);
    assert(dib.height != 0);
    assert(dib.bit_count == 8);
    ++bitmap_count;
  }
  assert(bitmap_count == 242);

  std::size_t wave_count = 0;
  std::size_t valid_wave_count = 0;
  std::uint64_t total_sample_bytes = 0;
  for (const auto& descriptor : simtower::generated::kResources) {
    if (descriptor.type != "WAVE") {
      continue;
    }
    ++wave_count;
    const auto resource = resources.find("WAVE", descriptor.numeric_id);
    const auto wave = simtower::parse_original_wave(resource);
    if (wave.logical_size == 0) {
      assert(descriptor.numeric_id == 8000 || descriptor.numeric_id == 9004 ||
             descriptor.numeric_id == 9007);
      continue;
    }
    assert(wave.logical_size <= resource.size());
    assert(wave.format_tag == 1);
    assert(wave.channels == 1);
    assert(wave.block_align == 1);
    assert(wave.bits_per_sample == 8);
    assert(wave.average_bytes_per_second == wave.samples_per_second);
    total_sample_bytes += wave.samples.size();
    ++valid_wave_count;
  }
  assert(wave_count == 58);
  assert(valid_wave_count == 55);
  assert(total_sample_bytes == 1638925);

  // Headless branch coverage for 11c8:0167/0597 arbitration and category
  // gates. No audio device or audible playback is opened by these tests.
  simtower::OriginalAudioArbiter audio;
  assert(audio.select_channel(1) == 0);
  audio.start(0, 1577, 1);
  assert(audio.select_channel(1) == 1);
  audio.start(1, 1384, 1);
  assert(audio.select_channel(1) == -1);
  assert(audio.select_channel(2) == 0);
  assert(audio.select_channel(5) == 1);
  // Direct 11c8:09d2 completion-state tail: after the backend frees the
  // completed wave, both active/priority words for the channel are cleared.
  audio.stop(0);
  assert(audio.select_channel(0) == 0);
  // Direct 11c8:02c0 gate coverage and 11c8:0135 ordering: the latter's fixed
  // loop invokes the gated stop for channel zero and then channel one—never a
  // dynamic count. These pure checks do not initialize waveOut.
  static_assert(!simtower::original_audio_stop_channel_allowed(
      false, true, true, 0U));
  static_assert(!simtower::original_audio_stop_channel_allowed(
      true, false, true, 0U));
  static_assert(!simtower::original_audio_stop_channel_allowed(
      true, true, false, 0U));
  static_assert(simtower::original_audio_stop_channel_allowed(
      true, true, true, 0U));
  static_assert(simtower::original_audio_stop_channel_allowed(
      true, true, true, 1U));
  static_assert(!simtower::original_audio_stop_channel_allowed(
      true, true, true, 2U));
  // Direct 11c8:0100 coverage without initializing waveOut: the reserved
  // priority-five request is submitted exactly when channel one is idle.
  static_assert(simtower::original_reserved_audio_should_submit(false));
  static_assert(!simtower::original_reserved_audio_should_submit(true));
  // Direct 11c8:0978 coverage without playback: zero leaves an ordinary
  // one-shot header, while nonzero counts enable a loop for that many total
  // waveOut passes.
  static_assert(simtower::original_audio_loop_plan(0U) ==
                simtower::OriginalAudioLoopPlan{});
  static_assert(simtower::original_audio_loop_plan(1U) ==
                (simtower::OriginalAudioLoopPlan{
                    WHDR_BEGINLOOP | WHDR_ENDLOOP, 1U}));
  static_assert(simtower::original_audio_loop_plan(0xffffU) ==
                (simtower::OriginalAudioLoopPlan{
                    WHDR_BEGINLOOP | WHDR_ENDLOOP, 0xffffU}));
  audio.stop_all();
  assert(!audio.channels()[0].active && !audio.channels()[1].active);
  assert(audio.channels()[0].resource_id == -1);
  assert(simtower::original_audio_priority_enabled(0, {true, false, false}));
  assert(!simtower::original_audio_priority_enabled(1, {true, false, false}));
  assert(simtower::original_audio_priority_enabled(5, {false, false, true}));
  static_assert(simtower::kOriginalAudioSaturationTicks == 600U);
  static_assert(simtower::kOriginalAudioSaturationNominalMs == 9600U);
  static_assert(!simtower::original_audio_saturation_elapsed(599U, 0U));
  static_assert(simtower::original_audio_saturation_elapsed(600U, 0U));
  static_assert(simtower::original_audio_saturation_elapsed(599U, 0xffffffffU));
  static_assert(!simtower::original_audio_saturation_elapsed(0U, 1U));

  // Direct 11e0:0e84 coverage: the unsigned deadline comparison is strict,
  // one due call advances by 0x30 rather than snapping to now, callback 03BD
  // is drained even with all categories disabled, and wraparound is literal.
  using PumpPlan = simtower::OriginalWaveMixPumpPlan;
  static_assert(simtower::original_wavemix_pump_plan(
                    100U, 147U, true, false, false) ==
                PumpPlan{false, false, false, 100U});
  static_assert(simtower::original_wavemix_pump_plan(
                    100U, 148U, false, false, false) ==
                PumpPlan{true, true, false, 148U});
  static_assert(simtower::original_wavemix_pump_plan(
                    100U, 1'000U, false, true, false) ==
                PumpPlan{true, true, true, 148U});
  static_assert(simtower::original_wavemix_pump_plan(
                    0xfffffff0U, 0x10U, true, true, true) ==
                PumpPlan{false, false, false, 0xfffffff0U});
  static_assert(simtower::original_wavemix_pump_plan(
                    0xfffffff0U, 0x20U, false, false, true) ==
                PumpPlan{true, true, true, 0x20U});

  // Native Fast Mode retains the reference Win16 full-frame host cadence
  // without changing 1200:0196's recovered gate/bypass behavior. The due
  // comparison is exact at 58 ms and remains valid across GetTickCount wrap.
  static_assert(simtower::kNativeFastModeFramePeriodMs == 58U);
  static_assert(!simtower::native_fast_mode_frame_due(1'000U, 1'057U));
  static_assert(simtower::native_fast_mode_frame_due(1'000U, 1'058U));
  static_assert(simtower::native_fast_mode_frame_due(1'000U, 2'000U));
  static_assert(!simtower::native_fast_mode_frame_due(0xfffffff0U, 0x29U));
  static_assert(simtower::native_fast_mode_frame_due(0xfffffff0U, 0x2aU));

  // Direct 11c8:08eb/0a31/0add sound-disabled lifecycle branches and
  // 11c8:0390 inactive-channel query. The runtime is deliberately disabled
  // before initialization, so this opens no device and produces no audible
  // output.
  simtower::OriginalAudioRuntime silent_audio(resources);
  silent_audio.set_sound_enabled(false);
  assert(silent_audio.initialize());
  assert(!silent_audio.active() && !silent_audio.sound_enabled());
  assert(!silent_audio.channel_active(0U));
  assert(!silent_audio.channel_active(1U));
  assert(!silent_audio.channel_active(2U));
  silent_audio.deactivate();
  silent_audio.shutdown();

  // 1128:0443-0535 startup profile ordering. These tests are pure and never
  // initialize waveOut or open an audio device.
  const simtower::OriginalSoundProfileValues shipped_sound_profile{
      true, 0U, 1U, 1U, 1U, 1U};
  static_assert(simtower::kOriginalStartupSavePathCapacity == 0x80U);
  static_assert(simtower::kOriginalShippedSaveDirectory ==
                L"C:\\Maxis\\Simtower");
  assert(simtower::original_sound_profile_state(true, shipped_sound_profile) ==
         simtower::OriginalSoundProfileState(
             {false, true, {true, true, true}}));
  assert(simtower::original_sound_profile_state(false, shipped_sound_profile) ==
         simtower::OriginalSoundProfileState{});
  assert(simtower::original_sound_profile_state(
             true, {false, 0U, 1U, 1U, 1U, 1U}) ==
         simtower::OriginalSoundProfileState{});
  assert(simtower::original_sound_profile_state(
             true, {true, 1U, 1U, 1U, 1U, 1U}) ==
         simtower::OriginalSoundProfileState(
             {true, false, {false, false, false}}));
  assert(simtower::original_sound_profile_state(
             true, {true, 2U, 0U, 0U, 3U, 4U}) ==
         simtower::OriginalSoundProfileState(
             {false, false, {false, true, true}}));
  assert(simtower::original_wave_resource_for_sound_event(3, 0) == 1577);
  assert(simtower::original_wave_resource_for_sound_event(6, 0) == 1384);
  assert(simtower::original_wave_resource_for_sound_event(6, -1) == 1385);
  assert(simtower::original_wave_resource_for_sound_event(9, 20) == 1576);
  assert(simtower::original_wave_resource_for_sound_event(9, 21) == 1577);
  assert(simtower::original_wave_resource_for_sound_event(10, 1) == 1640);
  assert(simtower::original_wave_resource_for_sound_event(11, 1) == 1705);
  assert(simtower::original_wave_resource_for_sound_event(17, 0) == 17);
  assert(simtower::original_wave_resource_for_sound_event(29, 0) == 2856);
  assert(simtower::original_contextual_wave_resource(-1, true, true, 0, 0) == 10002);
  assert(simtower::original_contextual_wave_resource(-1, true, false, 6, 3) == 10012);
  assert(simtower::original_contextual_wave_resource(-1, true, false, 9, 4) == 10011);
  assert(!simtower::original_contextual_wave_resource(-2, true, true, 0, 0));
  assert(simtower::original_should_attempt_ambient_sound(true, 0, -32));
  assert(!simtower::original_should_attempt_ambient_sound(true, 1, 32));
  assert(!simtower::original_should_attempt_ambient_sound(false, 0, 32));
  assert(simtower::original_ambient_probe(0, 100, 80, 75) ==
         simtower::OriginalAmbientProbe({39, 25, 36}));
  assert(simtower::original_ambient_probe(5, 100, 80, 75) ==
         simtower::OriginalAmbientProbe({60, 75, 15}));
  assert(!simtower::original_ambient_probe(6, 100, 80, 75));

  // Direct 11c8:06b6 coverage for its empty-location sentinels and every
  // facility jump-table family. These are pure decisions and open no audio.
  const std::array<simtower::OriginalLinkedSoundRecord, 2> linked_sounds = {{
      {0, 1}, {3, 1}}};
  const std::array<simtower::OriginalServiceSoundRecord, 3> service_sounds = {{
      {1, 0}, {2, 0}, {3, 7}}};
  simtower::OriginalFacilitySoundRecord facility{3, 1, 0};
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == 3);
  facility.phase = 8;
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == -2);
  facility.phase = 9;
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == 3);
  facility = {7, 8, 0};
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == -2);
  facility = {6, 0, 0};
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == 6);
  facility.linked_index = 1;
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == -2);
  facility = {29, 0, 1};
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == 29);
  facility = {18, 0, 2};
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == 9008);
  facility = {9, -127, 0};
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == 9);
  facility = {11, 1, 0};
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == -2);
  facility.phase = 2;
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == 11);
  facility = {12, 0, 0};
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == 12);
  facility = {30, 0, 1};
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == 30);
  facility = {19, 0, 2};
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == 9008);
  facility.linked_index = 3U;
  assert(simtower::original_sound_event_for_facility(0, &facility, linked_sounds,
                                                      service_sounds) == -2);
  assert(simtower::original_sound_event_for_facility(9, nullptr, linked_sounds,
                                                      service_sounds) == -2);
  assert(simtower::original_sound_event_for_facility(10, nullptr, linked_sounds,
                                                      service_sounds) == -1);

  std::size_t dialog_count = 0;
  std::size_t dialog_item_count = 0;
  for (const auto& descriptor : simtower::generated::kResources) {
    if (descriptor.type != "DIALOG") {
      continue;
    }
    const auto original = simtower::parse_original_dialog(
        descriptor.numeric_id >= 0
            ? resources.find("DIALOG", descriptor.numeric_id)
            : resources.find("DIALOG", descriptor.string_id));
    const auto native = simtower::build_native_dialog_template(original);
    assert(native.size() >= sizeof(DLGTEMPLATE));
    const auto* header = reinterpret_cast<const DLGTEMPLATE*>(native.data());
    assert(header->style == original.style);
    assert(header->cdit == original.items.size());
    dialog_item_count += original.items.size();
    ++dialog_count;
  }
  assert(dialog_count == 48);
  assert(dialog_item_count == 137);

  // 1068:0000 passes literal 8 to 1008:0085, whose 1008:0000 template
  // patch replaces the event DIALOG's embedded 10-point size while keeping
  // its original font face and enabling DS_SETFONT.
  auto event_dialog = simtower::parse_original_dialog(
      resources.find("DIALOG", 3010));
  assert((event_dialog.style & DS_SETFONT) != 0U);
  assert(event_dialog.font_point_size == 10U);
  const auto event_font_face = event_dialog.font_face;
  simtower::apply_original_dialog_font_point_size(event_dialog, 8U);
  assert((event_dialog.style & DS_SETFONT) != 0U);
  assert(event_dialog.font_point_size == 8U);
  assert(event_dialog.font_face == event_font_face);

  // Direct 1010:0018/014c/0304 coverage: the modal/modeless wrapper selects
  // the shared named DIALOG/TOWER_TITLE and centers the active DIB
  // over a black desktop-sized client. BITMAP/128 is the small first image;
  // BITMAP/256 exactly fills a 640x480 desktop in the second phase.
  for (const std::uint16_t message : {0x000fU, 0x0110U, 0x0201U}) {
    assert(simtower::original_startup_splash_handles_message(true, message));
  }
  for (const std::uint16_t message : {0x0002U, 0x000fU, 0x0110U}) {
    assert(simtower::original_startup_splash_handles_message(false, message));
  }
  assert(!simtower::original_startup_splash_handles_message(true, 0x0002U));
  assert(!simtower::original_startup_splash_handles_message(false, 0x0201U));
  assert(simtower::kOriginalStartupSplashModalDismissResult == 0U);
  assert(simtower::kOriginalStartupSplashMinimumTicks == 180U);
  assert(simtower::kOriginalStartupSplashNominalMinimumMs == 2880U);
  assert(simtower::original_startup_window_size(
             640, 480, false, 1, 1) ==
         simtower::OriginalStartupWindowSize({640, 480}));
  assert(simtower::original_startup_window_size(
             640, 480, true, 2, 3) ==
         simtower::OriginalStartupWindowSize({644, 486}));
  assert(simtower::original_startup_bitmap_placement(
             resources, 128, 640, 480) ==
         simtower::OriginalStartupBitmapPlacement({202, 141, 236, 198}));
  assert(simtower::original_startup_bitmap_placement(
             resources, 256, 640, 480) ==
         simtower::OriginalStartupBitmapPlacement({0, 0, 640, 480}));
  assert(simtower::original_startup_bitmap_placement(
             resources, 256, 320, 200) ==
         simtower::OriginalStartupBitmapPlacement({0, 0, 640, 480}));

  // 1128:01d9-0223 shows Map then Info, invokes 1050:03aa's one-time surface
  // preload, and only then shows Command and Main. The authoritative class
  // strings at 1128:05ba/05c8/05da identify DS:325a/325c/325e as
  // Command/Info/Map respectively. Arrow selection follows
  // all four windows; 1058:033c is skipped only when DS:783e is already one,
  // and 1080:05a1 remains the unconditional final Command composition and
  // synchronous DS:325a Command presentation boundary.
  using StartupShowAction = simtower::OriginalStartupShowAction;
  assert((simtower::original_startup_show_plan(true) ==
          std::array{
              StartupShowAction::show_map,
              StartupShowAction::show_info,
              StartupShowAction::preload_command_surfaces,
              StartupShowAction::show_command,
              StartupShowAction::show_main,
              StartupShowAction::select_arrow_cursor,
              StartupShowAction::none,
              StartupShowAction::compose_and_present_command}));
  assert((simtower::original_startup_show_plan(false) ==
          std::array{
              StartupShowAction::show_map,
              StartupShowAction::show_info,
              StartupShowAction::preload_command_surfaces,
              StartupShowAction::show_command,
              StartupShowAction::show_main,
              StartupShowAction::select_arrow_cursor,
              StartupShowAction::toggle_construction_mode,
              StartupShowAction::compose_and_present_command}));

  // Direct 1128:0ba3 coverage: floor-offset precompute precedes the complete
  // world/facility graphics sheet pack.
  using StartupPrecomputeAction = simtower::OriginalStartupPrecomputeAction;
  assert(simtower::original_startup_precompute_plan() ==
         (std::array<StartupPrecomputeAction, 2>{
             StartupPrecomputeAction::precompute_floor_offsets,
             StartupPrecomputeAction::pack_world_and_facility_sheets}));

  // 1258 copies a nonempty raw WinMain tail and retains the executable
  // directory through its final backslash. 1128 uses the supplied path as-is
  // only when that tail itself contains a backslash; forward slash is not a
  // separator in this recovered Win16 branch.
  using StartupTarget = simtower::OriginalStartupCommandTarget;
  assert(!simtower::original_startup_command_target(
      "C:\\MAXIS\\SIMTOWER\\", ""));
  assert((simtower::original_startup_command_target(
              "C:\\MAXIS\\SIMTOWER\\", "TOWER.TDT") ==
          StartupTarget{"C:\\MAXIS\\SIMTOWER\\TOWER.TDT", "TOWER.TDT"}));
  assert((simtower::original_startup_command_target(
              "C:\\MAXIS\\SIMTOWER\\", "D:\\SAVES\\TOWER.TDT") ==
          StartupTarget{"D:\\SAVES\\TOWER.TDT", "TOWER.TDT"}));
  assert((simtower::original_startup_command_target(
              "C:\\MAXIS\\SIMTOWER\\", "D:/SAVES/TOWER.TDT") ==
          StartupTarget{"C:\\MAXIS\\SIMTOWER\\D:/SAVES/TOWER.TDT",
                        "D:/SAVES/TOWER.TDT"}));

  // The native host removes only the matching quote pair that modern Windows
  // supplies around an association target containing spaces. The exact Win16
  // separator and module-directory rules above remain unchanged.
  assert(simtower::normalize_native_startup_command_line("").empty());
  assert(simtower::normalize_native_startup_command_line("TOWER.TDT") ==
         "TOWER.TDT");
  assert(simtower::normalize_native_startup_command_line(
             "\"D:\\Path With Spaces\\NATIVE.TDT\"") ==
         "D:\\Path With Spaces\\NATIVE.TDT");
  assert(simtower::normalize_native_startup_command_line(
             "\"D:\\SAVES\\NATIVE.TDT") ==
         "\"D:\\SAVES\\NATIVE.TDT");
  const auto quoted_native_target =
      simtower::original_startup_command_target(
          "C:\\MAXIS\\SIMTOWER\\",
          simtower::normalize_native_startup_command_line(
              "\"D:\\Path With Spaces\\NATIVE.TDT\""));
  assert((quoted_native_target ==
          StartupTarget{"D:\\Path With Spaces\\NATIVE.TDT", "NATIVE.TDT"}));

  // 1128:1139 orders five independently accepted display warnings before
  // its mandatory TrueType gate and final wave-device consent. A missing
  // TrueType implementation suppresses the separate enabled check, while an
  // accepted missing-wave warning alone disables sound. 12af-1301 appends
  // the exact legacy extension placeholder to the module filename.
  using CapabilityIssue = simtower::OriginalStartupCapabilityIssue;
  constexpr std::uint16_t kAllRasterCaps =
      simtower::kOriginalRasterCapBitBlt |
      simtower::kOriginalRasterCapDeviceIndependentBitmap |
      simtower::kOriginalRasterCapDibToDevice |
      simtower::kOriginalRasterCapStretchBlt;
  constexpr std::uint16_t kAllRasterizerFlags =
      simtower::kOriginalRasterizerTrueTypeAvailable |
      simtower::kOriginalRasterizerTrueTypeEnabled;
  assert((simtower::original_startup_capability_issues(
              {8U, kAllRasterCaps, kAllRasterizerFlags, true}) ==
          std::array{
              CapabilityIssue::none, CapabilityIssue::none,
              CapabilityIssue::none, CapabilityIssue::none,
              CapabilityIssue::none, CapabilityIssue::none,
              CapabilityIssue::none, CapabilityIssue::none}));
  assert((simtower::original_startup_capability_issues({0U, 0U, 0U, false}) ==
          std::array{
              CapabilityIssue::fewer_than_256_colors,
              CapabilityIssue::missing_bitblt,
              CapabilityIssue::missing_device_independent_bitmap,
              CapabilityIssue::missing_dib_to_device,
              CapabilityIssue::missing_stretchblt,
              CapabilityIssue::truetype_unsupported,
              CapabilityIssue::wave_output_unavailable,
              CapabilityIssue::none}));
  assert((simtower::original_startup_capability_issues(
              {8U, kAllRasterCaps,
               simtower::kOriginalRasterizerTrueTypeAvailable, false}) ==
          std::array{
              CapabilityIssue::truetype_disabled,
              CapabilityIssue::wave_output_unavailable,
              CapabilityIssue::none, CapabilityIssue::none,
              CapabilityIssue::none, CapabilityIssue::none,
              CapabilityIssue::none, CapabilityIssue::none}));
  assert(simtower::original_startup_capability_requires_consent(
      CapabilityIssue::fewer_than_256_colors));
  assert(simtower::original_startup_capability_requires_consent(
      CapabilityIssue::missing_stretchblt));
  assert(simtower::original_startup_capability_requires_consent(
      CapabilityIssue::wave_output_unavailable));
  assert(!simtower::original_startup_capability_requires_consent(
      CapabilityIssue::truetype_unsupported));
  assert(!simtower::original_startup_capability_requires_consent(
      CapabilityIssue::truetype_disabled));
  assert(simtower::kOriginalStartupLowColorMessage ==
         "Windows is not running with a 256-color display driver.  SimTower "
         "will run, but may display distorted color and animation.  Do you "
         "still want to run SimTower?");
  assert(simtower::kOriginalStartupMissingRasterMessage ==
         "The display driver on this system does not appear to support the "
         "functionality that SimTower requires.  If you continue, you may "
         "experience lockups or General Protection Faults.  Do you want to "
         "continue?");
  assert(simtower::kOriginalStartupTrueTypeUnsupportedMessage ==
         "You cannot run SimTower without TrueType Fonts. Your Windows does "
         "not support TrueType. ");
  assert(simtower::kOriginalStartupTrueTypeDisabledMessage ==
         "TrueType is not available.Please change settings  to \"Use "
         "TrueType Fonts\" in the Control Panels.");
  assert(simtower::kOriginalStartupNoWaveOutputMessage ==
         "SimTower did not detect a sound card or proper sound drivers on "
         "this system.  Do you want to run SimTower without sound support? ");
  assert(simtower::original_tdt_extension_profile_value(
             "C:\\MAXIS\\SIMTOWER\\SIMTOWER.EXE") ==
         "C:\\MAXIS\\SIMTOWER\\SIMTOWER.EXE ^.tdt");

  // 1128:1318 shifts GETFREESPACE by ten, adds 0x942K for the resident
  // image, and performs an unsigned comparison against 0x1770K. The first
  // passing raw byte count is therefore exactly 3,630K.
  constexpr std::uint32_t kFirstPassingFreeBytes = 3630U * 1024U;
  assert(simtower::kOriginalStartupResidentMemoryKb == 0x942U);
  assert(simtower::kOriginalStartupRequiredMemoryKb == 0x1770U);
  // Complete NEWORLOADDLOGFILTER 1018:0067/0096-01ee: the literal four-
  // message table uses distinct paint/control fonts and palette-realized
  // immediate/WM_PAINT surfaces with no class cursor. Only command IDs 1..3
  // are consumed; all other command IDs return FALSE.
  using NewLoadStyle = simtower::OriginalNewLoadDialogStyle;
  using NewLoadCommandPlan = simtower::OriginalNewLoadDialogCommandPlan;
  // Direct 1018:0000 launcher coverage: ordinal DIALOG/10000, Main owner,
  // zero original application parameter, and preserved DialogBox result.
  assert(simtower::original_new_load_launcher_contract() ==
         (simtower::OriginalNewLoadLauncherContract{10000U, 0, true, true}));
  using StartupModalOwner = simtower::OriginalStartupNativeModalOwner;
  assert(simtower::original_startup_native_modal_owner(false) ==
         StartupModalOwner::recovered_main_window);
  assert(simtower::original_startup_native_modal_owner(true) ==
         StartupModalOwner::active_startup_splash);
  using SplashTeardownStep = simtower::OriginalStartupSplashTeardownStep;
  // Direct 1010:00fd ownership/order coverage without creating a window.
  assert(simtower::original_startup_splash_teardown_plan(false) ==
         (std::array<SplashTeardownStep, 5>{}));
  assert(simtower::original_startup_splash_teardown_plan(true) ==
         (std::array<SplashTeardownStep, 5>{
             SplashTeardownStep::destroy_window,
             SplashTeardownStep::clear_window_handle,
             SplashTeardownStep::release_proc_instance,
             SplashTeardownStep::release_bitmap_resource_if_present,
             SplashTeardownStep::clear_proc_pointer,
         }));
  for (const std::uint16_t message :
       {0x000fU, 0x0019U, 0x0110U, 0x0111U}) {
    assert(simtower::original_new_load_dialog_handles_message(message));
  }
  for (const std::uint16_t message :
       {0x0010U, 0x0113U, 0x0201U, 0xffffU}) {
    assert(!simtower::original_new_load_dialog_handles_message(message));
  }
  assert(simtower::original_new_load_dialog_command_plan(1U) ==
         (NewLoadCommandPlan{true, true, 1U, true}));
  assert(simtower::original_new_load_dialog_command_plan(3U) ==
         (NewLoadCommandPlan{true, true, 3U, true}));
  assert(simtower::original_new_load_dialog_command_plan(0U) ==
         (NewLoadCommandPlan{false, false, 0U, false}));
  assert(simtower::original_new_load_dialog_command_plan(4U) ==
         (NewLoadCommandPlan{false, false, 0U, false}));
  assert(simtower::original_new_load_dialog_style() ==
         (NewLoadStyle{11, 13, true, true, true}));
  assert(simtower::original_startup_memory_kb(
             kFirstPassingFreeBytes - 1U) == 5999U);
  assert(!simtower::original_startup_memory_sufficient(
      kFirstPassingFreeBytes - 1U));
  assert(simtower::original_startup_memory_kb(
             kFirstPassingFreeBytes) == 6000U);
  assert(simtower::original_startup_memory_sufficient(
      kFirstPassingFreeBytes));
  assert(simtower::original_startup_memory_kb(0xffffffffU) == 4196673U);
  assert(simtower::original_startup_low_memory_message(
             kFirstPassingFreeBytes - 1U) ==
         "Windows doesn't have enough memory to run SimTower. SimTower for "
         "Windows needs at least 6000K of virtual memory to run.  SimTower "
         "found 5999K bytes of virtual memory free. Please see the README "
         "file on how to correct this situation.");

  // AHOTTADLOGFILTER 1068:00a1 calls 1068:0439's all-control caret pass with
  // the raw signed `%ld` value; its 1068:0175-0256 item-3 fallback then
  // applies the wrapping absolute value to #0. Cost suffix zeros remain.
  for (const std::uint16_t message :
       {0x000fU, 0x0019U, 0x0110U, 0x0111U, 0x0113U}) {
    assert(simtower::original_event_dialog_handles_message(message));
  }
  for (const std::uint16_t message :
       {0x0010U, 0x0100U, 0x0201U, 0xffffU}) {
    assert(!simtower::original_event_dialog_handles_message(message));
  }
  const auto bomb_dialog = simtower::parse_original_dialog(
      resources.find("DIALOG", 3020));
  const auto bomb_text = std::ranges::find_if(
      bomb_dialog.items, [](const auto& item) { return item.id == 3U; });
  assert(bomb_text != bomb_dialog.items.end());
  assert(bomb_text->text.kind ==
         simtower::OriginalDialogValue::Kind::text);
  assert(simtower::format_original_dialog_argument(
             bomb_text->text.text, -2'000) ==
         "Blackmail from Terrorists!\nThey demand $200000 or a hidden bomb "
         "will explode at 3 o'clock.");
  const auto fire_dialog = simtower::parse_original_dialog(
      resources.find("DIALOG", 3010));
  const auto fire_text = std::ranges::find_if(
      fire_dialog.items, [](const auto& item) { return item.id == 3U; });
  assert(fire_text != fire_dialog.items.end());
  assert(simtower::format_original_dialog_argument(
             fire_text->text.text, 8) ==
         "SECOM has sensed a fire on floor 8!\n"
         "Everyone should take emergency refuge!");
  assert(simtower::format_original_dialog_argument(
             "^0 then #000", -7) == "-7 then 700");
  assert(simtower::format_original_dialog_caret_arguments(
             "^0/^0", -7) == "-7/-7");
  assert(simtower::format_original_dialog_caret_arguments(
             "left^1right", -7) == "leftright");
  assert(simtower::format_original_dialog_argument(
             "none", -7) == "none");
  assert(simtower::format_original_dialog_argument(
             "^0", std::numeric_limits<std::int32_t>::min()) ==
         "-2147483648");
  assert(simtower::format_original_dialog_argument(
             "#0", std::numeric_limits<std::int32_t>::min()) ==
         "-2147483648");

  using EventAction = simtower::OriginalEventDialogAction;
  assert(simtower::original_event_dialog_action(0U, -100, 50, false) ==
         EventAction::ignore);
  assert(simtower::original_event_dialog_action(1U, -100, 50, false) ==
         EventAction::close_decline);
  assert(simtower::original_event_dialog_action(2U, 100, 0, false) ==
         EventAction::close_accept);
  assert(simtower::original_event_dialog_action(2U, -100, 100, false) ==
         EventAction::close_accept);
  assert(simtower::original_event_dialog_action(2U, -101, 100, false) ==
         EventAction::warn_insufficient_funds_then_close_decline);
  assert(simtower::original_event_dialog_action(2U, -101, 100, true) ==
         EventAction::close_decline);
  assert(simtower::original_event_dialog_action(
             2U, -1, std::numeric_limits<std::int32_t>::min(), false) ==
         EventAction::close_accept);

  // 11e0:0b52 uses outer-window dimensions and its own 43/80-pixel vertical
  // policy. These cover ordinary centering, Elevator Control's x=8 override,
  // the near-full-height fallback, bottom clamp, and signed half truncation.
  using ScreenRect = simtower::OriginalDialogScreenRect;
  assert(simtower::original_dialog_screen_position(
             ScreenRect{0, 0, 1024, 768},
             ScreenRect{100, 100, 500, 400}, 0) ==
         simtower::OriginalDialogScreenPosition({312, 234}));
  assert(simtower::original_dialog_screen_position(
             ScreenRect{0, 0, 1024, 768},
             ScreenRect{40, 50, 644, 530}, 8) ==
         simtower::OriginalDialogScreenPosition({8, 144}));
  assert(simtower::original_dialog_screen_position(
             ScreenRect{0, 0, 640, 480},
             ScreenRect{0, 0, 320, 420}, 0) ==
         simtower::OriginalDialogScreenPosition({160, 60}));
  assert(simtower::original_dialog_screen_position(
             ScreenRect{0, 0, 640, 480},
             ScreenRect{0, 0, 320, 500}, 0) ==
         simtower::OriginalDialogScreenPosition({160, -20}));
  assert(simtower::original_dialog_screen_position(
             ScreenRect{0, 0, 640, 480},
             ScreenRect{0, 0, 321, 101}, 0) ==
         simtower::OriginalDialogScreenPosition({159, 189}));

  // Exact b3ac facility-dialog group switch at 1100:03ac. The original
  // chooses DIALOG/(0x02ec + group), with a separate path for these null
  // types and the inactive signed-byte sentinel.
  assert(!simtower::original_facility_information_dialog_id(-1));
  assert(!simtower::original_facility_information_dialog_id(0));
  assert(!simtower::original_facility_information_dialog_id(24));
  assert(!simtower::original_facility_information_dialog_id(46));
  assert(simtower::original_facility_information_dialog_id(7) == 748);
  assert(simtower::original_facility_information_dialog_id(3) == 749);
  assert(simtower::original_facility_information_dialog_id(14) == 755);
  assert(simtower::original_facility_information_dialog_id(1) == 756);
  assert(simtower::original_facility_information_dialog_id(13) == 757);
  assert(simtower::original_facility_information_dialog_id(18) == 758);
  assert(simtower::original_facility_information_dialog_id(34) == 758);
  assert(simtower::original_facility_information_dialog_id(29) == 759);
  assert(simtower::original_facility_information_dialog_id(6) == 760);
  assert(simtower::original_facility_information_dialog_id(12) == 760);
  // Direct 1100:0e86/11da coverage: type zero selects DIALOG/761; every
  // other raw Elevator type selects 762, while Stair/Escalator stays at 761.
  assert(simtower::original_elevator_information_dialog_id(0) == 761);
  assert(simtower::original_elevator_information_dialog_id(1) == 762);
  assert(simtower::original_elevator_information_dialog_id(255) == 762);
  assert(simtower::original_vertical_transport_information_dialog_id() ==
         761);

  {
    auto tower = simtower::make_original_new_tdt();
    auto& elevator = tower.elevators[2];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.capacity = 21U;
    elevator.car_records[3].exact_bytes[3] = std::byte{8};
    simtower::OriginalMagnifierTarget target{};
    target.kind = simtower::OriginalMagnifierTargetKind::
        elevator_car_information;
    target.dialog_id = 762U;
    target.elevator_index = 2U;
    target.elevator_car_index = 3;
    auto text = simtower::original_transport_information_text(
        resources, tower, target);
    assert(text.valid);
    assert(text.primary == simtower::original_strl_entry(
                               resources.find("STRL", 400), 2U));
    assert(text.secondary == "8 / 21");

    auto& stair = tower.post_elevator.stairs_bd70[4];
    stair.used = 1U;
    stair.shape = 3U;
    stair.word_6 = 0xffffU;
    stair.word_8 = 2U;
    target = {};
    target.kind = simtower::OriginalMagnifierTargetKind::
        vertical_transport_information;
    target.dialog_id = 761U;
    target.vertical_transport_index = 4U;
    text = simtower::original_transport_information_text(
        resources, tower, target);
    assert(text.valid);
    assert(text.primary == simtower::original_strl_entry(
                               resources.find("STRL", 400), 5U));
    assert(text.secondary == "1");
  }
  assert(!resources.find(
              "DIALOG",
              simtower::original_elevator_information_dialog_id(0)).empty());
  assert(!resources.find(
              "DIALOG",
              simtower::original_elevator_information_dialog_id(1)).empty());

  // Direct 1070:051f/0570/05a1 ownership and 1070:061d layout coverage.
  // Native value lifetimes replace the five per-HWND resource slots and
  // their search/unlock/free/refcount tail. Parsing and releasing all 45 DTMP
  // values exercises the same bitmap/header/rectangle ownership boundary;
  // the complete array validation also covers 061d's one-based RECT copy.
  std::size_t dtmp_count = 0;
  std::size_t dtmp_rectangle_count = 0;
  for (const auto& descriptor : simtower::generated::kResources) {
    if (descriptor.type != "DTMP") {
      continue;
    }
    const auto dtmp = simtower::parse_original_dtmp(
        resources.find("DTMP", descriptor.numeric_id));
    dtmp_rectangle_count += dtmp.rectangles.size();
    ++dtmp_count;
  }
  assert(dtmp_count == 45);
  assert(dtmp_rectangle_count == 332);
  const auto startup_dtmp = simtower::parse_original_dtmp(resources.find("DTMP", 10000));
  assert(startup_dtmp.bitmap_reference.empty());
  assert(startup_dtmp.width_or_header == 260);
  assert(startup_dtmp.height_or_header == 118);
  assert(startup_dtmp.rectangles == std::vector<simtower::OriginalDtmpRect>({
      {16, 10, 244, 36}, {90, 82, 170, 108}, {16, 46, 244, 72}}));
  assert(simtower::original_dtmp_window_size(startup_dtmp, resources) ==
         simtower::OriginalDtmpWindowSize({260, 118}));
  // Direct 1070:0005 branch coverage. Bitmap-less DTMP/10000 realizes the
  // palette and updates the current text position before applying its header
  // size. This ordering is deliberately different from a positive bitmap ID.
  using DtmpConfigurationPlan = simtower::OriginalDtmpConfigurationPlan;
  assert(simtower::original_dtmp_configuration_plan(startup_dtmp) ==
         DtmpConfigurationPlan({false, true, true, true}));
  const auto elevator_dtmp = simtower::parse_original_dtmp(resources.find("DTMP", 520));
  assert(elevator_dtmp.bitmap_reference == "510");
  assert(elevator_dtmp.bitmap_resource_id == 510);
  assert(simtower::original_dtmp_configuration_plan(elevator_dtmp) ==
         DtmpConfigurationPlan({true, false, true, false}));
  const auto elevator_bitmap = simtower::original_dib_view(resources.find("BITMAP", 510));
  assert(simtower::original_dtmp_window_size(elevator_dtmp, resources) ==
         simtower::OriginalDtmpWindowSize(
              {static_cast<int>(elevator_bitmap.width),
               std::abs(static_cast<int>(elevator_bitmap.height))}));
  auto zero_header_dtmp = startup_dtmp;
  zero_header_dtmp.width_or_header = 0U;
  assert(simtower::original_dtmp_configuration_plan(zero_header_dtmp) ==
         DtmpConfigurationPlan({false, true, true, false}));

  {
    // Direct 1208:05a9 coverage: the helper fills only the supplied rectangle
    // with literal white through a temporary brush and preserves the caller's
    // selected brush.
    HDC dc = CreateCompatibleDC(nullptr);
    assert(dc != nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 8;
    info.bmiHeader.biHeight = -8;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(
        dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    assert(bitmap != nullptr && pixels != nullptr);
    const auto previous_bitmap = SelectObject(dc, bitmap);
    HBRUSH sentinel_brush = CreateSolidBrush(RGB(1, 2, 3));
    assert(sentinel_brush != nullptr);
    const auto previous_brush = SelectObject(dc, sentinel_brush);
    assert(previous_brush != nullptr);
    RECT complete{0, 0, 8, 8};
    FillRect(dc, &complete, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    RECT fill{2, 3, 6, 7};
    for (int repetition = 0; repetition < 32; ++repetition) {
      simtower::fill_original_white_rect(dc, fill);
      assert(GetCurrentObject(dc, OBJ_BRUSH) == sentinel_brush);
    }
    assert(GetPixel(dc, 1, 3) == RGB(0, 0, 0));
    assert(GetPixel(dc, 2, 3) == RGB(255, 255, 255));
    assert(GetPixel(dc, 5, 6) == RGB(255, 255, 255));
    assert(GetPixel(dc, 6, 6) == RGB(0, 0, 0));

    SelectObject(dc, previous_brush);
    SelectObject(dc, previous_bitmap);
    DeleteObject(sentinel_brush);
    DeleteObject(bitmap);
    DeleteDC(dc);
  }

  {
    // Exact 11e0:04c0/05d7 lifecycle, direct 11e0:0e00/0e22/0e60 solid-
    // brush wrappers and 11e0:06d9 frame coverage. Direct 1208:00dc covers
    // both negative InflateRect passes, while 11e0:0950 adds its missing-child
    // placeholder bevel. The recovered helpers create/select temporary drawing
    // objects, render the five-gray bevel, then restore both preexisting DC
    // objects and delete the temporary pen/brush. The native RAII translation
    // must leave the caller's selections intact and leak no GDI handles while
    // producing the same filled dialog center.
    HWND window = CreateWindowExW(
        0, L"STATIC", L"", WS_POPUP, 0, 0, 32, 24, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    assert(window != nullptr);
    HDC dc = CreateCompatibleDC(nullptr);
    assert(dc != nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 32;
    info.bmiHeader.biHeight = -24;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(
        dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    assert(bitmap != nullptr && pixels != nullptr);
    const auto previous_bitmap = SelectObject(dc, bitmap);
    HPEN sentinel_pen = CreatePen(PS_SOLID, 1, RGB(1, 2, 3));
    HBRUSH sentinel_brush = CreateSolidBrush(RGB(4, 5, 6));
    assert(sentinel_pen != nullptr && sentinel_brush != nullptr);
    const auto previous_pen = SelectObject(dc, sentinel_pen);
    const auto previous_brush = SelectObject(dc, sentinel_brush);
    assert(previous_pen != nullptr && previous_brush != nullptr);
    simtower::OriginalDtmp chrome_dtmp{};
    chrome_dtmp.rectangles.push_back({8, 6, 24, 18});

    // Direct 11e0:0633 coverage: the DTMP chrome renderer leaves the DC's
    // background color at exact white. Warm the process-wide GDI machinery
    // before measuring; Windows can allocate one cached object on first use.
    simtower::paint_original_dialog_chrome(
        window, dc, chrome_dtmp);
    assert(GetBkColor(dc) == RGB(255, 255, 255));
    assert(GetCurrentObject(dc, OBJ_PEN) == sentinel_pen);
    assert(GetCurrentObject(dc, OBJ_BRUSH) == sentinel_brush);
    const DWORD handles_before =
        GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int repetition = 0; repetition < 32; ++repetition) {
      simtower::paint_original_dialog_chrome(
          window, dc, chrome_dtmp);
      assert(GetCurrentObject(dc, OBJ_PEN) == sentinel_pen);
      assert(GetCurrentObject(dc, OBJ_BRUSH) == sentinel_brush);
    }
    const DWORD handles_after =
        GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    if (handles_before != 0U && handles_after != 0U) {
      assert(handles_after == handles_before);
    }
    assert(GetPixel(dc, 7, 12) == RGB(204, 204, 204));
    assert(GetPixel(dc, 20, 6) == RGB(89, 89, 89));
    assert(GetPixel(dc, 8, 10) == RGB(89, 89, 89));
    assert(GetPixel(dc, 10, 17) == RGB(255, 255, 255));
    assert(GetPixel(dc, 23, 10) == RGB(255, 255, 255));
    assert(GetPixel(dc, 16, 12) == RGB(230, 230, 230));

    SelectObject(dc, previous_brush);
    SelectObject(dc, previous_pen);
    SelectObject(dc, previous_bitmap);
    DeleteObject(sentinel_brush);
    DeleteObject(sentinel_pen);
    DeleteObject(bitmap);
    DeleteDC(dc);
    DestroyWindow(window);
  }

  // Direct 1208:017a ALRT parser coverage: enumerate every extracted alert,
  // proving the mode/preserved-word header and payload are accepted.
  std::size_t alert_count = 0;
  for (const auto& descriptor : simtower::generated::kResources) {
    if (descriptor.type != "ALRT") {
      continue;
    }
    const auto alert = simtower::parse_original_alert(
        resources.find("ALRT", descriptor.numeric_id));
    assert(alert.button_mode <= 4);
    assert(alert.preserved_word == 1);
    ++alert_count;
  }
  assert(alert_count == 6);
  // NAMEPEPLEDIALOGFILTER 1100:3cc0 and NAMETENANTDIALOGFILTER 1100:4035
  // pass ALRT/1000 plus STRL/1005 text to 11e0:0c9e. ALRT/1000 is the
  // OK-only ^0 carrier; ALRT/1005 is a distinct Yes/No carrier.
  const auto rename_alert = simtower::parse_original_alert(
      resources.find("ALRT", 1000));
  const auto confirmation_alert = simtower::parse_original_alert(
      resources.find("ALRT", 1005));
  assert(rename_alert.button_mode == 0U);
  assert(rename_alert.message_template == "^0");
  assert(confirmation_alert.button_mode == 4U);
  const auto save_alert = simtower::parse_original_alert(resources.find("ALRT", 1001));
  assert(save_alert.button_mode == 3);
  assert(save_alert.message_template == "Save the tower \"^0\" before ^1?");
  // Direct 1208:0133 presentation-plan coverage: resource lookup, formatting,
  // and native style are fixed before the host-only MessageBox call.
  assert((simtower::prepare_original_alert(
              resources, 1001,
              {"Sky Plaza", "quitting", "unused", "unused"}) ==
          simtower::OriginalAlertPresentation{
              3U, "Save the tower \"Sky Plaza\" before quitting?",
              MB_YESNOCANCEL | MB_ICONEXCLAMATION | MB_SYSTEMMODAL}));
  // Direct 1208:0274 formatter coverage: expand ^0..^3 and discard every
  // unsupported/trailing escape together with its selector byte.
  assert(simtower::format_original_alert(
             save_alert.message_template,
             {"Sky Plaza", "quitting", "unused", "unused"}) ==
         "Save the tower \"Sky Plaza\" before quitting?");
  assert(simtower::format_original_alert("a^4b^^c^", {"0", "1", "2", "3"}) ==
         "abc");
  // Direct 1208:0369 coverage: the raw ALRT button word is ORed with the
  // exact icon/system-modal flags, then modes 0..4 normalize MessageBox's
  // OK/Yes/No/Cancel identifiers through the original bounded jump table.
  assert(simtower::original_alert_message_box_style(0) ==
         (MB_OK | MB_ICONEXCLAMATION | MB_SYSTEMMODAL));
  assert(simtower::original_alert_message_box_style(1) ==
         (MB_OKCANCEL | MB_ICONEXCLAMATION | MB_SYSTEMMODAL));
  assert(simtower::original_alert_message_box_style(2) ==
         (MB_ABORTRETRYIGNORE | MB_ICONEXCLAMATION | MB_SYSTEMMODAL));
  assert(simtower::original_alert_message_box_style(3) ==
         (MB_YESNOCANCEL | MB_ICONEXCLAMATION | MB_SYSTEMMODAL));
  assert(simtower::original_alert_message_box_style(4) ==
         (MB_YESNO | MB_ICONEXCLAMATION | MB_SYSTEMMODAL));
  assert(simtower::original_alert_result(0, IDCANCEL) == 1);
  assert(simtower::original_alert_result(1, IDOK) == 1);
  assert(simtower::original_alert_result(1, IDCANCEL) == 2);
  assert(simtower::original_alert_result(2, IDABORT) == 1);
  assert(simtower::original_alert_result(3, IDYES) == 1);
  assert(simtower::original_alert_result(3, IDNO) == 2);
  assert(simtower::original_alert_result(3, IDCANCEL) == 3);
  assert(simtower::original_alert_result(4, IDYES) == 1);
  assert(simtower::original_alert_result(4, IDNO) == 2);
  assert(simtower::original_alert_result(5, IDYES) == 1);

  // Direct 1208:01a0 coverage: the resource starts with a big-endian count,
  // callers use one-based indices, and each preceding Pascal string advances
  // the cursor by its unsigned length plus the length byte itself.
  constexpr std::array<std::byte, 10> kSyntheticStrl = {
      std::byte{0}, std::byte{3}, std::byte{1}, std::byte{'A'},
      std::byte{0}, std::byte{3}, std::byte{'x'}, std::byte{'y'},
      std::byte{'z'}, std::byte{0}};
  assert(simtower::original_strl_entry(kSyntheticStrl, 1U) == "A");
  assert(simtower::original_strl_entry(kSyntheticStrl, 2U).empty());
  assert(simtower::original_strl_entry(kSyntheticStrl, 3U) == "xyz");
  const auto strl = resources.find("STRL", 1000);
  assert(!strl.empty());
  const auto count = simtower::original_be16(strl, 0);
  assert(count > 0);
  assert(!simtower::original_strl_entry(strl, 1).empty());
  assert(simtower::original_strl_entry(strl, 0).empty());
  assert(simtower::original_strl_entry(strl, static_cast<std::uint16_t>(count + 1)).empty());

  // Direct 1178:000c and 1190:0005 coverage: startup loads all three YEN banks
  // and converts all 33 leading words, four middle dwords, and 46 trailing
  // words from the 0xae-byte PART/1000 payload into gameplay globals.
  const auto part = simtower::original_part_table(resources.find("PART", 1000));
  constexpr std::array<std::uint16_t, 33> kExpectedPartHead = {
      300, 5, 0, 300, 0, 80, 80, 80, 150, 150, 200, 50, 35, 25, 60, 35, 50,
      25, 50, 35, 25, 60, 35, 50, 25, 25, 20, 60, 25, 30, 18, 16, 35};
  constexpr std::array<std::uint32_t, 4> kExpectedPartDwords = {
      300, 1000, 5000, 10000};
  constexpr std::array<std::uint16_t, 46> kExpectedPartTail = {
      1, 3, 7, 80, 1, 2, 80, 5, 100, 60, 40, 65476, 50, 30, 20, 65506,
      40, 80, 100, 0, 20, 100, 150, 60, 60, 40, 20, 40, 40, 40, 20, 800,
      1500, 500, 3000, 1500, 5000, 0, 30, 100, 2000, 3000, 10000, 2000,
      3000, 5000};
  assert(part.words_00_to_40 == kExpectedPartHead);
  assert(part.dwords_42_to_4e == kExpectedPartDwords);
  assert(part.words_52_to_ac == kExpectedPartTail);

  constexpr std::array<std::uint32_t, 45> kExpectedYen1000 = {
      5, 2000, 0, 200, 500, 1000, 2000, 400, 0, 800, 1000, 30, 1000, 5000,
      1000, 500, 0, 1000, 5000, 0, 5000, 0, 50, 0, 50, 0, 0, 200, 0, 1000,
      0, 10000, 0, 0, 0, 0, 30000, 0, 0, 0, 0, 0, 4000, 1000, 500};
  constexpr std::array<std::uint32_t, 45> kExpectedYen1001 = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 30, 20, 15, 5, 45, 30, 20,
      8, 90, 60, 40, 15, 0, 0, 0, 0, 150, 100, 50, 20, 0, 0, 0, 0,
      2000, 1500, 1000, 400, 200, 150, 100, 40, 0};
  constexpr std::array<std::uint32_t, 45> kExpectedYen1002 = {
      0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 200, 100, 0, 0, 0,
      0, 500, 0, 0, 0, 0, 0, 0, 50, 0, 0, 0, 1000, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 200, 100, 100};
  assert(simtower::original_yen_table(resources.find("YEN", 1000)) == kExpectedYen1000);
  assert(simtower::original_yen_table(resources.find("YEN", 1001)) == kExpectedYen1001);
  assert(simtower::original_yen_table(resources.find("YEN", 1002)) == kExpectedYen1002);

  // Direct 1140:0005/01df/022c/03f8/0470 coverage: initialize rating one,
  // parse and resolve all six count-prefixed rating TABLs, and derive their
  // exact outer Command-window heights,
  // including every high-byte TABM indirection and its one-based low byte.
  constexpr std::array<std::array<std::uint16_t, 8>, 6> kExpectedRatingLayouts = {{
      {0, 4, 7, 24, 11, 0, 0, 0},
      {0, 4, 7, 8, 24, 11, 21, 0},
      {0, 4, 7, 8, 24, 11, 16, 21},
      {0, 4, 7, 8, 24, 11, 16, 21},
      {0, 4, 7, 8, 24, 11, 16, 21},
      {0, 4, 7, 8, 24, 11, 16, 21},
  }};
  constexpr std::array<std::size_t, 6> kExpectedRatingCounts = {5, 7, 8, 8, 8, 8};
  for (std::size_t rating = 0; rating < kExpectedRatingLayouts.size(); ++rating) {
    const auto tabl = resources.find("TABL", static_cast<int>(1001U + rating));
    const auto encoded_entries = simtower::original_word_table(tabl);
    assert(encoded_entries.size() == kExpectedRatingCounts[rating]);
    assert(simtower::original_rating_window_height(
               static_cast<std::uint16_t>(encoded_entries.size())) ==
           static_cast<std::uint16_t>(((encoded_entries.size() + 1U) / 2U) * 32U + 61U));
    for (std::size_t index = 0; index < encoded_entries.size(); ++index) {
      assert(simtower::original_resolve_tabl_entry(tabl, index, resources) ==
             kExpectedRatingLayouts[rating][index]);
    }
  }

  return 0;
}
