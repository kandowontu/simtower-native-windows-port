#include "original_dialog.hpp"
#include "original_dib.hpp"
#include "original_dtmp.hpp"
#include "original_elevator_control.hpp"
#include "original_resources.hpp"
#include "original_tdt.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
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
  for (std::size_t index = 0U; index < chars.size(); ++index) {
    bytes[index] = static_cast<std::byte>(
        static_cast<unsigned char>(chars[index]));
  }
  return bytes;
}

void assert_dib_size(const simtower::OriginalResources& resources,
                     int id,
                     int width,
                     int height) {
  const auto dib = simtower::original_dib_view(resources.find("BITMAP", id));
  assert(dib.width == width);
  assert(dib.height == height);
}

simtower::OriginalTdtDocument make_control_tower() {
  auto tower = simtower::make_original_new_tdt();
  auto& elevator = tower.elevators[0];
  elevator.used = 1U;
  elevator.type = 0U;
  elevator.cars = 2U;
  elevator.word_3c = 0U;
  elevator.bottom_floor = 10;
  elevator.top_floor = 40;
  elevator.car_records[0].exact_bytes[15] = std::byte{1};
  elevator.car_records[2].exact_bytes[15] = std::byte{1};
  elevator.car_home_floors[0] = std::byte{10};
  elevator.car_home_floors[2] = std::byte{24};
  return tower;
}

}  // namespace

int main(int argc, char** argv) {
  // Direct 1098:0000 modeless-launch coverage. Preserve the initiating packed
  // Main-client point beside the elevator index and the literal owner/resource
  // and requested-left values consumed by ELVDLOGMAIN initialization.
  assert(simtower::original_elevator_control_launch_contract(
             7U, -123, 456) ==
         (simtower::OriginalElevatorControlLaunchContract{
             7U, -123, 456, 400U, 8, true}));
  assert(argc == 2);
  const auto pack = read_file(argv[1]);
  const simtower::OriginalResources resources(pack);

  // ELVDLOGMAIN 1098:08c9 writes the shared DS:31a6 activation latch and
  // immediately repairs its own z-order. The inactive branch prefers
  // GetTopWindow(control), falling back to main only when it returns null.
  using ActivationInsertAfter =
      simtower::OriginalElevatorControlActivationInsertAfter;
  using ActivationPlan = simtower::OriginalElevatorControlActivationPlan;
  using ClosePlan = simtower::OriginalElevatorControlClosePlan;
  using CloseWindow = simtower::OriginalElevatorControlCloseWindow;
  using StockBrush = simtower::OriginalElevatorControlStockBrush;

  assert(simtower::original_elevator_control_close_plan() ==
         (ClosePlan{
             true,
             true,
             {CloseWindow::map, CloseWindow::command,
              CloseWindow::info, CloseWindow::main},
             false,
         }));

  for (const std::uint16_t message :
       {0x000fU, 0x0110U, 0x0200U, 0x0202U, 0x0205U}) {
    assert(simtower::original_elevator_popup_handles_message(message));
  }
  for (const std::uint16_t message :
       {0x0006U, 0x000fU, 0x0019U, 0x0110U,
        0x0115U, 0x0200U, 0x0201U, 0x0202U}) {
    assert(simtower::original_elevator_control_handles_message(message));
  }
  for (const std::uint16_t message :
       {0x0000U, 0x0010U, 0x0014U, 0x0201U, 0x0311U, 0xffffU}) {
    assert(!simtower::original_elevator_popup_handles_message(message));
  }
  for (const std::uint16_t message :
       {0x0000U, 0x0010U, 0x0014U, 0x0204U, 0x0205U, 0x0311U, 0xffffU}) {
    assert(!simtower::original_elevator_control_handles_message(message));
  }

  // Direct exported-boundary coverage for ELVDLOGMAIN 1098:0628. Its Win16
  // WM_CTLCOLOR branch maps type five (the only child, scrollbar item 7) to
  // WHITE_BRUSH and all other types to NULL_BRUSH.
  assert(simtower::original_elevator_control_stock_brush(true) ==
         StockBrush::white);
  assert(simtower::original_elevator_control_stock_brush(false) ==
         StockBrush::null);
  assert((simtower::original_elevator_control_activation_plan(true, false) ==
          ActivationPlan{true, ActivationInsertAfter::topmost, false}));
  assert((simtower::original_elevator_control_activation_plan(true, true) ==
          ActivationPlan{true, ActivationInsertAfter::topmost, false}));
  assert((simtower::original_elevator_control_activation_plan(false, true) ==
          ActivationPlan{false, ActivationInsertAfter::first_child, true}));
  assert((simtower::original_elevator_control_activation_plan(false, false) ==
          ActivationPlan{false, ActivationInsertAfter::main, true}));

  // Direct 1098:0068/12e9 coverage: the painter/full-refresh root composes its
  // 200x428 backing and
  // 200x110 overlay atlas from DIALOG/DTMP/400, full variants
  // BITMAP/400/401/410/411, and the three 24x21 phase modes 402..404.  The
  // direct-DIB painter uses the same resources and DTMP source rectangles;
  // these assertions pin every input dimension and the key packed regions.
  // The modeless Elevator Control has one native child: scrollbar item seven.
  const auto dialog = simtower::parse_original_dialog(
      resources.find("DIALOG", 400));
  assert(dialog.style == 0x92c00040UL);
  assert(dialog.x == 0 && dialog.y == 0);
  assert(dialog.width == 343 && dialog.height == 364);
  assert(dialog.caption.text == "Elevator");
  assert(dialog.font_point_size == 10U);
  assert(dialog.font_face == "Arial");
  assert(dialog.items.size() == 1U);
  assert(dialog.items[0].id == 7U);
  assert(dialog.items[0].x == 60 && dialog.items[0].y == 96);
  assert(dialog.items[0].width == 8 && dialog.items[0].height == 100);
  assert(dialog.items[0].style == 0x40010001UL);

  const auto dtmp = simtower::parse_original_dtmp(
      resources.find("DTMP", 400));
  assert(dtmp.bitmap_reference == "400");
  assert(dtmp.bitmap_resource_id == 400);
  // With a numeric bitmap prefix these two header words are unused and the
  // BITMAP itself supplies the 200x428 client size.
  assert(dtmp.width_or_header == 0U);
  assert(dtmp.height_or_header == 0U);
  assert(dtmp.rectangles.size() == 44U);
  assert((dtmp.rectangles[0] ==
          simtower::OriginalDtmpRect{115, 404, 182, 423}));
  assert((dtmp.rectangles[1] ==
          simtower::OriginalDtmpRect{17, 404, 83, 423}));
  assert((dtmp.rectangles[4] ==
          simtower::OriginalDtmpRect{161, 206, 173, 227}));
  assert((dtmp.rectangles[5] ==
          simtower::OriginalDtmpRect{18, 195, 136, 391}));
  assert((dtmp.rectangles[6] ==
          simtower::OriginalDtmpRect{135, 195, 151, 391}));
  assert((dtmp.rectangles[11] ==
          simtower::OriginalDtmpRect{29, 4, 101, 20}));
  assert((dtmp.rectangles[12] ==
          simtower::OriginalDtmpRect{102, 4, 171, 20}));
  assert((dtmp.rectangles[22] ==
          simtower::OriginalDtmpRect{29, 43, 52, 64}));
  assert((dtmp.rectangles[27] ==
          simtower::OriginalDtmpRect{149, 43, 172, 64}));
  assert((dtmp.rectangles[40] ==
          simtower::OriginalDtmpRect{76, 93, 96, 116}));
  assert((dtmp.rectangles[41] ==
          simtower::OriginalDtmpRect{76, 148, 96, 171}));
  assert((dtmp.rectangles[42] ==
          simtower::OriginalDtmpRect{0xffff, 0xffff, 0xffff, 0xffff}));
  assert((dtmp.rectangles[43] ==
          simtower::OriginalDtmpRect{221, 11, 258, 79}));

  assert_dib_size(resources, 400, 200, 428);
  assert_dib_size(resources, 401, 200, 428);
  assert_dib_size(resources, 410, 200, 428);
  assert_dib_size(resources, 411, 200, 428);
  for (int id = 402; id <= 404; ++id) assert_dib_size(resources, id, 24, 21);
  assert_dib_size(resources, 405, 82, 67);
  assert_dib_size(resources, 406, 82, 67);
  assert_dib_size(resources, 407, 12, 21);
  assert_dib_size(resources, 408, 12, 21);
  for (int id = 20256; id <= 20265; ++id) {
    assert_dib_size(resources, id, 16, 16);
  }

  auto tower = make_control_tower();
  auto state = simtower::make_original_elevator_control_state(
      tower, 0U, 1U, 6U);
  assert(state.valid);
  assert(state.elevator_index == 0U);
  assert(state.schedule_bank == 1U);
  assert(state.day_phase == 5U);
  assert(state.scroll_min == 10);
  assert(state.scroll_max == 26);
  assert(state.scroll_position == 26);
  // 1098:0786-0793 changes only exact phase six to five; every other signed
  // DS:b3a1 byte is copied rather than native-clamped.
  auto raw_phase_state = simtower::make_original_elevator_control_state(
      tower, 0U, 1U, static_cast<std::int8_t>(-1));
  assert(raw_phase_state.valid && raw_phase_state.day_phase == 0xffU);
  raw_phase_state = simtower::make_original_elevator_control_state(
      tower, 0U, 1U, static_cast<std::int8_t>(7));
  assert(raw_phase_state.day_phase == 7U);
  assert(simtower::original_elevator_control_title(resources, tower, 0U) ==
         "Express Elevator");

  auto& elevator = tower.elevators[0];
  elevator.schedule[26] = std::byte{99};
  // Direct 1098:1502 coverage: bank one reads the six mode bytes at
  // 28+7+phase and maps values 0/1/2 to BITMAP/402/403/404 in DTMP items
  // 23..28 before 1098:1498 frames the selected day phase.
  for (std::uint8_t phase = 0U; phase < 6U; ++phase) {
    elevator.schedule[35U + phase] = static_cast<std::byte>(phase % 3U);
    auto phase_state = state;
    phase_state.day_phase = phase;
    assert(simtower::original_elevator_control_floor_mode(
               elevator, phase_state) == phase % 3U);
  }
  elevator.schedule[54] = std::byte{1};
  // Direct 1098:27bd/2893 coverage: the two painters use bank*7+phase at
  // schedule bases 14 and 42, format with "%2d", and multiply the latter by
  // thirty only after CBW sign-extension. High-bit bytes must therefore
  // display negative values rather than native unsigned 255/7650 artifacts.
  assert(simtower::original_elevator_control_waiting_value(elevator, state) ==
         99U);
  assert(simtower::original_elevator_control_waiting_text(elevator, state) ==
         "99");
  assert(simtower::original_elevator_control_departure_units(elevator, state) ==
         1U);
  assert(simtower::original_elevator_control_departure_text(elevator, state) ==
         "30");
  elevator.schedule[26] = std::byte{0xff};
  elevator.schedule[54] = std::byte{0xff};
  assert(simtower::original_elevator_control_waiting_value(elevator, state) ==
         -1);
  assert(simtower::original_elevator_control_waiting_text(elevator, state) ==
         "-1");
  assert(simtower::original_elevator_control_departure_units(elevator, state) ==
         -1);
  assert(simtower::original_elevator_control_departure_text(elevator, state) ==
         "-30");
  elevator.schedule[26] = std::byte{99};
  elevator.schedule[54] = std::byte{1};
  assert(simtower::original_elevator_control_floor_mode(elevator, state) ==
         2U);
  assert(simtower::original_elevator_control_adjust_waiting(tower, state, 1));
  assert(!simtower::original_elevator_control_adjust_waiting(tower, state, 1));
  assert(simtower::original_elevator_control_adjust_waiting(tower, state,
                                                            -200));
  assert(simtower::original_elevator_control_waiting_value(elevator, state) ==
         1U);
  assert(simtower::original_elevator_control_waiting_text(elevator, state) ==
         " 1");
  assert(simtower::original_elevator_control_adjust_departure(tower, state,
                                                              8));
  assert(simtower::original_elevator_control_departure_units(elevator, state) ==
         3U);
  assert(simtower::original_elevator_control_departure_text(elevator, state) ==
         "90");
  assert(!simtower::original_elevator_control_adjust_departure(tower, state,
                                                               1));
  assert(simtower::original_elevator_control_set_floor_mode(tower, state, 0U));
  assert(!simtower::original_elevator_control_set_floor_mode(tower, state, 0U));
  assert(!simtower::original_elevator_control_set_floor_mode(tower, state, 3U));
  // Direct 1098:13e4/2215 bank-change transaction: accept only banks zero/one;
  // the production path invalidates all fifteen rows and redraws the selected
  // bank's matching DTMP/510 button region from BITMAP/401.
  assert(simtower::original_elevator_control_select_bank(state, 0U));
  assert(!simtower::original_elevator_control_select_bank(state, 2U));
  assert(simtower::original_elevator_control_select_phase(state, 4U));
  assert(!simtower::original_elevator_control_select_phase(state, 6U));

  // Direct 1098:15c6 coverage: zero selects BITMAP/408 and each toggle flips
  // the exact word_3c flag and the displayed resource to/from BITMAP/407.
  assert(simtower::original_elevator_control_show_bitmap(elevator) == 408U);
  assert(simtower::original_elevator_control_toggle_show(tower, 0U));
  assert(elevator.word_3c == 1U);
  assert(simtower::original_elevator_control_show_bitmap(elevator) == 407U);
  assert(simtower::original_elevator_control_toggle_show(tower, 0U));
  assert(elevator.word_3c == 0U);

  // The scrollbar direction is intentionally inverted relative to the
  // displayed floors: lowering the position raises the visible floor band.
  assert(simtower::original_elevator_control_has_scrollbar(state));
  assert(simtower::original_elevator_control_visible_lowest_floor(
             elevator, state) == 10);
  assert(simtower::original_elevator_control_scroll(
      state, simtower::OriginalElevatorControlScrollCommand::page_up));
  assert(state.scroll_position == 12);
  assert(simtower::original_elevator_control_visible_lowest_floor(
             elevator, state) == 24);
  assert(simtower::original_elevator_control_scroll(
      state, simtower::OriginalElevatorControlScrollCommand::line_up));
  assert(state.scroll_position == 11);
  assert(simtower::original_elevator_control_visible_floor(
             elevator, state, 14) == 39);
  assert(simtower::original_elevator_control_scroll(
      state, simtower::OriginalElevatorControlScrollCommand::thumb_track,
      10));
  assert(simtower::original_elevator_control_visible_floor(
             elevator, state, 14) == 40);
  assert(!simtower::original_elevator_control_scroll(
      state, simtower::OriginalElevatorControlScrollCommand::line_up));

  // Direct 1098:16a4/1644 coverage: DTMP rectangle six supplies the framed
  // grid origin; each cell is a 13-pixel step containing a 12x12 rectangle,
  // with row zero inverted to visual row fourteen and column -1 reserved for
  // floor labels. The compacted active columns exercised below determine
  // 16a4's vertical line count; its horizontal count is always fourteen.
  assert((simtower::original_elevator_control_cell_rect(-1, 0) ==
          simtower::OriginalDtmpRect{19, 378, 31, 390}));
  assert((simtower::original_elevator_control_cell_rect(0, 0) ==
          simtower::OriginalDtmpRect{32, 378, 44, 390}));
  assert((simtower::original_elevator_control_cell_rect(1, 14) ==
          simtower::OriginalDtmpRect{45, 196, 57, 208}));

  // 1098:1e33 outlines the current car floor through the same inverted
  // scrolling transform. It must not substitute the car's home floor.
  elevator.car_records[0].exact_bytes[0] = std::byte{26};
  elevator.car_home_floors[0] = std::byte{40};
  assert((simtower::original_elevator_control_current_car_frame(
              tower, state, 0U, 0) ==
          simtower::OriginalDtmpRect{32, 378, 44, 390}));
  elevator.car_records[2].exact_bytes[0] = std::byte{40};
  assert((simtower::original_elevator_control_current_car_frame(
              tower, state, 2U, 1) ==
          simtower::OriginalDtmpRect{45, 196, 57, 208}));
  elevator.car_records[2].exact_bytes[0] = std::byte{25};
  assert(!simtower::original_elevator_control_current_car_frame(
              tower, state, 2U, 1));
  elevator.car_records[2].exact_bytes[15] = std::byte{0};
  assert(!simtower::original_elevator_control_current_car_frame(
              tower, state, 2U, 1));
  elevator.car_records[2].exact_bytes[15] = std::byte{1};

  // Direct 1098:1f45/1f9d/1ff5 grid-scan coverage: all eight car records are
  // compacted before the service column and active-car columns are tested
  // across all fifteen inverted-scroll rows.
  auto hit = simtower::original_elevator_control_grid_hit(
      tower, state, 20, 379);
  assert((hit == simtower::OriginalElevatorControlGridHit{
      simtower::OriginalElevatorControlGridKind::service_floor,
      26, 0, -1, -1}));
  hit = simtower::original_elevator_control_grid_hit(tower, state, 33, 379);
  assert(hit.kind == simtower::OriginalElevatorControlGridKind::car);
  assert(hit.floor == 26 && hit.car_index == 0 && hit.visual_column == 0);
  hit = simtower::original_elevator_control_grid_hit(tower, state, 46, 197);
  assert(hit.kind == simtower::OriginalElevatorControlGridKind::car);
  assert(hit.floor == 40 && hit.car_index == 2 && hit.visual_column == 1);
  assert(!simtower::original_elevator_control_grid_hit(
              tower, state, 80, 250).hit());
  // Direct 1098:17c7/1895/226e coverage: the parent emits all fifteen visible
  // rows and each cell plan preserves above-shaft gray, exact service
  // inversion (including BLACK_PEN diagonals), the skipped floor zero, and
  // the 100-floor font inset.
  elevator.serviced_floors[26] = std::byte{1};
  auto floor_cell = simtower::original_elevator_control_floor_cell_plan(
      elevator, state, 0);
  assert((floor_cell == simtower::OriginalElevatorControlFloorCellPlan{
      26, false, true, "17", false, 1, 1}));
  elevator.serviced_floors[26] = std::byte{0};
  floor_cell = simtower::original_elevator_control_floor_cell_plan(
      elevator, state, 0);
  assert(!floor_cell.above_top && !floor_cell.serviced &&
         floor_cell.label == "17");
  elevator.top_floor = 39;
  floor_cell = simtower::original_elevator_control_floor_cell_plan(
      elevator, state, 14);
  assert(floor_cell.floor == 40 && floor_cell.above_top &&
         floor_cell.label.empty());
  elevator.bottom_floor = 94;
  elevator.top_floor = 109;
  auto high_state = simtower::make_original_elevator_control_state(
      tower, 0U, 0U, static_cast<std::int8_t>(0));
  floor_cell = simtower::original_elevator_control_floor_cell_plan(
      elevator, high_state, 14);
  assert(floor_cell.floor == 108 && floor_cell.label == "99" &&
         !floor_cell.small_font && floor_cell.horizontal_inset == 1 &&
         floor_cell.vertical_inset == 1);
  high_state.scroll_position = 94;
  floor_cell = simtower::original_elevator_control_floor_cell_plan(
      elevator, high_state, 14);
  assert(floor_cell.floor == 109 && floor_cell.label == "100" &&
         floor_cell.small_font && floor_cell.horizontal_inset == 3 &&
         floor_cell.vertical_inset == -1);
  elevator.bottom_floor = 10;
  elevator.top_floor = 40;

  assert(simtower::original_elevator_control_floor_label(10) == "1");
  assert(simtower::original_elevator_control_floor_label(9) == "-1");
  assert(simtower::original_elevator_control_floor_label(0) == "-10");
  assert(simtower::original_elevator_control_floor_label(24) == "15");

  // Direct 1098:1a5b/11e0:0430 coverage: every selector priority is translated
  // through the exact +0x4e20 bias to BITMAP/20256..20265.
  auto& car = elevator.car_records[0].exact_bytes;
  car[0] = std::byte{10};
  car[3] = std::byte{0};
  car[4] = std::byte{1};
  car[10] = std::byte{0};
  car[11] = std::byte{0};
  car[12] = std::byte{0};
  car[13] = std::byte{20};
  elevator.car_home_floors[0] = std::byte{10};
  assert(simtower::original_elevator_control_car_bitmap(tower, 0U, 0U, 10) ==
         20263U);
  car[12] = std::byte{1};
  assert(simtower::original_elevator_control_car_bitmap(tower, 0U, 0U, 10) ==
         20264U);
  car[3] = std::byte{1};
  assert(simtower::original_elevator_control_car_bitmap(tower, 0U, 0U, 10) ==
         20256U);
  assert(simtower::original_elevator_control_car_bitmap(tower, 0U, 0U, 20) ==
         20260U);
  car[226U + 21U] = std::byte{1};
  assert(simtower::original_elevator_control_car_bitmap(tower, 0U, 0U, 21) ==
         20262U);
  elevator.block_2a2[22] = std::byte{1};
  assert(simtower::original_elevator_control_car_bitmap(tower, 0U, 0U, 22) ==
         20258U);
  elevator.block_31a[23] = std::byte{1};
  assert(simtower::original_elevator_control_car_bitmap(tower, 0U, 0U, 23) ==
         20259U);
  car[4] = std::byte{0};
  car[0] = std::byte{11};
  car[3] = std::byte{0};
  elevator.car_home_floors[0] = std::byte{12};
  assert(simtower::original_elevator_control_car_bitmap(tower, 0U, 0U, 11) ==
         20265U);
  car[3] = std::byte{1};
  assert(simtower::original_elevator_control_car_bitmap(tower, 0U, 0U, 11) ==
         20257U);
  assert(simtower::original_elevator_control_car_bitmap(tower, 0U, 0U, 20) ==
         20261U);
  elevator.block_31a[25] = std::byte{1};
  assert(simtower::original_elevator_control_car_bitmap(tower, 0U, 0U, 25) ==
         20259U);
  elevator.block_2a2[26] = std::byte{1};
  assert(simtower::original_elevator_control_car_bitmap(tower, 0U, 0U, 26) ==
         20258U);

  // The remaining 1098:1ff5 action paths call 10a0:0085 for a service cell
  // and 10a0:1296 before replacing the selected active car's home floor.
  // Unlike Finger, the embedded control still edits a hidden shaft.
  elevator.car_home_floors[0] = std::byte{10};
  elevator.car_home_floors[2] = std::byte{24};
  assert(simtower::original_elevator_control_service_floor_gate(
             tower, 0U, 39) ==
         simtower::OriginalElevatorServiceFloorGate::eligible);
  assert(simtower::original_elevator_service_floor_gate(tower, 0U, 39) ==
         simtower::OriginalElevatorServiceFloorGate::inactive_shaft);
  assert(simtower::original_elevator_control_add_service_floor(
      tower, 0U, 39));
  assert(elevator.serviced_floors[39] == std::byte{1});
  assert(simtower::original_elevator_control_service_floor_gate(
             tower, 0U, 24) ==
         simtower::OriginalElevatorServiceFloorGate::active_car_home);
  assert(simtower::original_elevator_control_service_floor_gate(
             tower, 0U, 20) ==
         simtower::OriginalElevatorServiceFloorGate::forbidden_new_stop);
  elevator.serviced_floors[39] = std::byte{1};
  assert(simtower::original_elevator_control_set_car_home(
      tower, 0U, 0U, 39));
  assert(elevator.car_home_floors[0] == std::byte{39});
  elevator.serviced_floors[20] = std::byte{1};
  assert(!simtower::original_elevator_control_set_car_home(
      tower, 0U, 0U, 20));

  // Direct 1098:22f8 ELVPOPUP hit and raster coverage. Hit testing uses
  // (y*3)/(height+1), but 26f2's transparent visual blit uses three fixed
  // height/3 bands at 0, 22, and 44.
  assert(simtower::original_elevator_control_popup_selection(
             0, 0, simtower::kOriginalElevatorControlPopupWidth,
             simtower::kOriginalElevatorControlPopupHeight) == 0U);
  assert(simtower::original_elevator_control_popup_selection(
             simtower::kOriginalElevatorControlPopupWidth, 23,
             simtower::kOriginalElevatorControlPopupWidth,
             simtower::kOriginalElevatorControlPopupHeight) == 1U);
  assert(simtower::original_elevator_control_popup_selection(
             40, simtower::kOriginalElevatorControlPopupHeight,
             simtower::kOriginalElevatorControlPopupWidth,
             simtower::kOriginalElevatorControlPopupHeight) == 2U);
  assert(!simtower::original_elevator_control_popup_selection(
              -1, 0, simtower::kOriginalElevatorControlPopupWidth,
              simtower::kOriginalElevatorControlPopupHeight));
  assert(!simtower::original_elevator_control_popup_selection(
              0, simtower::kOriginalElevatorControlPopupHeight + 1,
              simtower::kOriginalElevatorControlPopupWidth,
              simtower::kOriginalElevatorControlPopupHeight));
  assert(simtower::original_elevator_control_popup_highlight(
             0U, simtower::kOriginalElevatorControlPopupWidth,
             simtower::kOriginalElevatorControlPopupHeight) ==
         simtower::OriginalDtmpRect({0, 0, 82, 22}));
  assert(simtower::original_elevator_control_popup_highlight(
             1U, simtower::kOriginalElevatorControlPopupWidth,
             simtower::kOriginalElevatorControlPopupHeight) ==
         simtower::OriginalDtmpRect({0, 22, 82, 44}));
  assert(simtower::original_elevator_control_popup_highlight(
             2U, simtower::kOriginalElevatorControlPopupWidth,
             simtower::kOriginalElevatorControlPopupHeight) ==
         simtower::OriginalDtmpRect({0, 44, 82, 66}));
  assert(!simtower::original_elevator_control_popup_highlight(
      3U, simtower::kOriginalElevatorControlPopupWidth,
      simtower::kOriginalElevatorControlPopupHeight));

  tower.elevators[3].used = 2U;
  const auto saved_block_c2 = elevator.block_c2;
  const auto saved_block_2a2 = elevator.block_2a2;
  const auto saved_floor_records = elevator.floor_records;
  const auto saved_car_records = elevator.car_records;
  const auto saved_service = elevator.serviced_floors;
  const auto saved_home = elevator.car_home_floors;
  bool build_mode = true;
  // Direct 10f0:0000/009c snapshot coverage: all 24 used bytes, the selected
  // Elevator record, and the build-mode flag are retained before isolation.
  assert(simtower::begin_original_elevator_control_isolation(
      tower, state, build_mode));
  assert(state.isolation_active);
  assert(state.saved_elevator_record.has_value());
  assert(!build_mode);
  for (std::size_t index = 0U; index < tower.elevators.size(); ++index) {
    assert(tower.elevators[index].used == (index == 0U ? 1U : 0U));
  }
  assert(!simtower::begin_original_elevator_control_isolation(
      tower, state, build_mode));

  // The preview is free to mutate the selected shaft. Resume restores the
  // complete saved record except for the four editable schedule arrays and
  // word_3c, exactly as 10f0:0719 does before its 0x345a copy-back.
  elevator.block_c2[3] = std::byte{0x81};
  elevator.block_2a2[39] = std::byte{7};
  elevator.serviced_floors[39] = std::byte{0};
  elevator.car_home_floors[0] = std::byte{24};
  elevator.car_records[0].exact_bytes[4] = std::byte{0};
  elevator.schedule[0] = std::byte{77};
  elevator.schedule[13] = std::byte{66};
  elevator.schedule[14] = std::byte{55};
  elevator.schedule[27] = std::byte{44};
  elevator.schedule[28] = std::byte{2};
  elevator.schedule[41] = std::byte{1};
  elevator.schedule[42] = std::byte{3};
  elevator.schedule[55] = std::byte{2};
  elevator.word_3c = 1U;
  assert(simtower::resume_original_elevator_control_isolation(
      tower, state, build_mode));
  assert(!state.isolation_active);
  assert(!state.saved_elevator_record.has_value());
  assert(build_mode);
  assert(elevator.block_c2 == saved_block_c2);
  assert(elevator.block_2a2 == saved_block_2a2);
  assert(elevator.floor_records.size() == saved_floor_records.size());
  for (std::size_t index = 0U; index < saved_floor_records.size(); ++index) {
    assert(elevator.floor_records[index].mapped_index ==
           saved_floor_records[index].mapped_index);
    assert(elevator.floor_records[index].floor ==
           saved_floor_records[index].floor);
    assert(elevator.floor_records[index].exact_bytes ==
           saved_floor_records[index].exact_bytes);
  }
  for (std::size_t index = 0U; index < saved_car_records.size(); ++index) {
    assert(elevator.car_records[index].exact_bytes ==
           saved_car_records[index].exact_bytes);
  }
  assert(elevator.serviced_floors == saved_service);
  assert(elevator.car_home_floors == saved_home);
  assert(elevator.schedule[0] == std::byte{77});
  assert(elevator.schedule[13] == std::byte{66});
  assert(elevator.schedule[14] == std::byte{55});
  assert(elevator.schedule[27] == std::byte{44});
  assert(elevator.schedule[28] == std::byte{2});
  assert(elevator.schedule[41] == std::byte{1});
  assert(elevator.schedule[42] == std::byte{3});
  assert(elevator.schedule[55] == std::byte{2});
  assert(elevator.word_3c == 1U);
  assert(tower.elevators[3].used == 2U);
  assert(tower.elevators[0].used == 1U);
  assert(tower.elevators[3].used == 2U);
  assert(!simtower::resume_original_elevator_control_isolation(
      tower, state, build_mode));

  {
    // 10f0:0318 runs twice the PART-selected rating threshold with b3ae set,
    // then restores the clock, owner maps, four floor-ring header bytes, and
    // every car byte except +0x0f. Its synthetic queue dword survives until
    // 10f0:0719 restores the complete saved shaft.
    auto preview_tower = make_control_tower();
    preview_tower.header.rating = 1U;
    preview_tower.header.frame_time = 10U;
    preview_tower.header.lobby_height = 1U;
    auto& preview_elevator = preview_tower.elevators[0];
    preview_elevator.capacity = 4U;
    preview_elevator.serviced_floors[10] = std::byte{1};
    preview_elevator.car_home_floors[0] = std::byte{10};
    preview_elevator.car_records[2].exact_bytes[15] = std::byte{0};
    auto& preview_car = preview_elevator.car_records[0].exact_bytes;
    preview_car.fill(std::byte{0});
    preview_car[0] = std::byte{10};
    preview_car[4] = std::byte{1};
    preview_car[5] = std::byte{10};
    preview_car[6] = std::byte{10};
    preview_car[13] = std::byte{10};
    preview_car[15] = std::byte{1};
    std::fill(preview_car.begin() + 184U,
              preview_car.begin() + 226U, std::byte{0xff});
    simtower::OriginalTdtElevatorFloorRecord waiting{};
    waiting.mapped_index = simtower::original_elevator_floor_record_index(
        preview_elevator.type, preview_elevator.bottom_floor,
        preview_elevator.top_floor, 10);
    waiting.floor = 10;
    waiting.exact_bytes[0] = std::byte{1};
    preview_elevator.floor_records.push_back(waiting);
    preview_tower.people[0].exact_bytes[10] = std::byte{7};
    preview_tower.people[0].exact_bytes[12] = std::byte{5};
    const auto saved_preview_car = preview_car;
    const auto saved_preview_person = preview_tower.people[0].exact_bytes;

    simtower::OriginalPartTable preview_part{};
    preview_part.words_00_to_40[8] = 1U;
    assert(simtower::original_elevator_control_preview_frame_count(
               preview_part, 1U) == 2U);
    assert(simtower::original_elevator_control_preview_frame_count(
               preview_part, 2U) == 2U);
    preview_part.words_00_to_40[9] = 3U;
    assert(simtower::original_elevator_control_preview_frame_count(
               preview_part, 3U) == 6U);
    preview_part.words_00_to_40[10] = 4U;
    assert(simtower::original_elevator_control_preview_frame_count(
               preview_part, 4U) == 8U);
    preview_part.words_00_to_40[10] = 0x4000U;
    assert(simtower::original_elevator_control_preview_frame_count(
               preview_part, 4U) == 0U);
    preview_part.words_00_to_40[8] = 1U;

    auto preview_state = simtower::make_original_elevator_control_state(
        preview_tower, 0U, 0U, 0U);
    bool preview_build_mode = true;
    assert(simtower::begin_original_elevator_control_isolation(
        preview_tower, preview_state, preview_build_mode));
    const auto preview =
        simtower::prepare_original_elevator_control_preview(
            preview_tower, preview_state, preview_part);
    assert(preview.prepared && preview.frames == 2U);
    assert(preview_tower.header.frame_time == 10U);
    assert(preview_tower.people[0].exact_bytes == saved_preview_person);
    assert(preview_elevator.floor_records[0].exact_bytes[0] ==
           std::byte{1});
    assert(preview_elevator.floor_records[0].exact_bytes[1] ==
           std::byte{0});
    assert(preview_elevator.floor_records[0].exact_bytes[4] ==
           std::byte{9});
    assert(preview_elevator.floor_records[0].exact_bytes[5] ==
           std::byte{0});
    assert(preview_car == saved_preview_car);
    assert(simtower::resume_original_elevator_control_isolation(
        preview_tower, preview_state, preview_build_mode));
    assert(preview_build_mode);
    assert(preview_elevator.floor_records[0].exact_bytes[4] ==
           std::byte{0});
  }

  return 0;
}
