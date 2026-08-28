#include "original_about.hpp"
#include "original_dialog.hpp"
#include "original_dib.hpp"
#include "original_resources.hpp"

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

std::vector<std::byte> read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  assert(stream);
  std::vector<char> chars((std::istreambuf_iterator<char>(stream)),
                          std::istreambuf_iterator<char>());
  std::vector<std::byte> bytes(chars.size());
  for (std::size_t index = 0; index < chars.size(); ++index) {
    bytes[index] = static_cast<std::byte>(
        static_cast<unsigned char>(chars[index]));
  }
  return bytes;
}

}  // namespace

int main(int argc, char** argv) {
  assert(argc == 2);
  const auto pack = read_file(argv[1]);
  const simtower::OriginalResources resources(pack);

  // Direct 1010:049e coverage. The launcher stops both channels, directly
  // deactivates the mixer without changing 11c8's active latch, compacts the
  // Win16 heap, runs the modal About box, then directly reactivates the mixer.
  using AboutLauncherStep = simtower::OriginalAboutLauncherStep;
  assert(simtower::original_about_launcher_plan() ==
         (std::array<AboutLauncherStep, 5>{
             AboutLauncherStep::stop_audio_channels,
             AboutLauncherStep::deactivate_mixer_backend,
             AboutLauncherStep::compact_global_heap,
             AboutLauncherStep::run_modal_dialog,
             AboutLauncherStep::activate_mixer_backend,
         }));

  using AboutAction = simtower::OriginalAboutDialogMessageAction;
  using AboutPlan = simtower::OriginalAboutDialogMessagePlan;
  assert(simtower::original_about_dialog_message_plan(0x000f) ==
         AboutPlan({AboutAction::paint, true}));
  assert(simtower::original_about_dialog_message_plan(0x0019) ==
         AboutPlan({AboutAction::control_color, true}));
  assert(simtower::original_about_dialog_message_plan(0x0100) ==
         AboutPlan({AboutAction::close, true}));
  assert(simtower::original_about_dialog_message_plan(0x0110) ==
         AboutPlan({AboutAction::initialize, true}));
  // The original timer route returns FALSE after posting WM_PAINT.
  assert(simtower::original_about_dialog_message_plan(0x0113) ==
         AboutPlan({AboutAction::timer, false}));
  assert(simtower::original_about_dialog_message_plan(0x0202) ==
         AboutPlan({AboutAction::close, true}));
  assert(simtower::original_about_dialog_message_plan(0x0205) ==
         AboutPlan({AboutAction::close, true}));
  assert(simtower::original_about_dialog_message_plan(0x0010) ==
         AboutPlan({AboutAction::unhandled, false}));
  assert(simtower::original_about_dialog_message_plan(0x0014) ==
         AboutPlan({AboutAction::unhandled, false}));
  assert(simtower::original_about_dialog_message_plan(0xffff) ==
         AboutPlan({AboutAction::unhandled, false}));

  // Direct ABOUTDLGPROC 1010:053f coverage: WM_INITDIALOG derives its custom
  // size from BITMAP/257, WM_PAINT owns the title/chrome, and each 55-ms
  // WM_TIMER pass scrolls one row from a centered 236x16 TEXT/128 line.  The
  // assertions below cover those resource, geometry, parsing, cadence, wrap,
  // and clipping invariants without opening the modal dialog.
  const auto dialog = simtower::parse_original_dialog(
      resources.find("DIALOG", "TOWER_TITLE"));
  assert(dialog.style == 0x90000042U);
  assert(dialog.width == 154 && dialog.height == 117);
  assert(dialog.font_point_size == 10U && dialog.font_face == "system");
  assert(dialog.items.empty());

  const auto bitmap = simtower::original_dib_view(
      resources.find("BITMAP", 257));
  assert(bitmap.width == 334 && bitmap.height == 270);
  assert(bitmap.bit_count == 8U);
  assert(simtower::derive_original_about_layout(resources) ==
         simtower::OriginalAboutLayout({
             604, 290,
             {10, 10, 344, 280},
             {354, 10, 594, 280},
             {356, 12, 592, 278},
             236, 16, 55U,
         }));
  // Direct 1010:0a3b/098f coverage for the retained line surface and painter
  // literals consumed by production.
  assert(simtower::original_about_line_style() ==
         simtower::OriginalAboutLineStyle({236, 16, 12, 230U, 0x0809U}));

  // Direct 11e0:0c10 coverage: normalize the current window rectangle,
  // center its signed Win16 extent, and truncate a negative odd delta to zero.
  assert(simtower::original_dialog_center_position(
             {0, 0, 640, 480}, {100, 50, 300, 150}) ==
         simtower::OriginalDialogScreenPosition({220, 190}));
  assert(simtower::original_dialog_center_position(
              {0, 0, 100, 100}, {50, 50, 153, 203}) ==
         simtower::OriginalDialogScreenPosition({-1, -26}));

  // Direct 1070:06cd coverage: convert the popup client's upper-left then
  // lower-right corner before constructing the ClipCursor screen rectangle.
  assert(simtower::original_dialog_rect_screen_conversion_order() ==
         (std::array<simtower::OriginalDialogRectScreenCorner, 2>{
             simtower::OriginalDialogRectScreenCorner::upper_left,
             simtower::OriginalDialogRectScreenCorner::lower_right}));

  // Direct 11e0:0000/0026 shared dialog-text wrapper coverage: reads reserve
  // 0xfe characters including NUL; writes forward the supplied text.
  assert(simtower::original_dialog_item_text_contract() ==
         (simtower::OriginalDialogItemTextContract{0x00feU, true}));

  // Direct 1010:0af1 coverage: the persistent TEXT/128 reader stops on CR,
  // consumes an optional LF, preserves blank rows, and terminates at NUL.
  const auto lines = simtower::original_about_credit_lines(resources);
  assert(lines.size() == 176U);
  assert(lines[0] == "SimTower v1.1b");
  assert(lines[10] == "\x95 Credits \x95");
  assert(lines[152] == "Maxis, Inc.");
  for (std::size_t index = 153U; index < lines.size(); ++index) {
    assert(lines[index].empty());
  }

  assert(simtower::original_about_visible_lines(lines, 0U).empty());
  assert(simtower::original_about_visible_lines(lines, 1U) ==
         std::vector<simtower::OriginalAboutScrollLine>({
             {"SimTower v1.1b", 265},
         }));
  assert(simtower::original_about_visible_lines(lines, 16U) ==
         std::vector<simtower::OriginalAboutScrollLine>({
             {"SimTower v1.1b", 250},
         }));
  assert(simtower::original_about_visible_lines(lines, 17U) ==
         std::vector<simtower::OriginalAboutScrollLine>({
             {"SimTower v1.1b", 249},
             {"SimTower was originally", 265},
         }));
  const auto scrolled = simtower::original_about_visible_lines(lines, 282U);
  assert(scrolled.size() == 17U);
  assert(scrolled.front() ==
         simtower::OriginalAboutScrollLine({lines[1], 0}));
  assert(scrolled.back() ==
         simtower::OriginalAboutScrollLine({lines[17], 256}));
  const auto wrapped = simtower::original_about_visible_lines(
      lines, lines.size() * 16U + 1U);
  assert(!wrapped.empty());
  assert(wrapped.back().text == lines[0]);
  assert(wrapped.back().top == 265);
  return 0;
}
