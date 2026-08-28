#include "original_alert.hpp"
#include "original_dialog.hpp"
#include "original_dib.hpp"
#include "original_dtmp.hpp"
#include "original_find.hpp"
#include "original_resources.hpp"
#include "original_tdt_file.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <vector>

namespace {

void store_u32(std::span<std::byte> bytes,
               std::size_t offset,
               std::uint32_t value) {
  bytes[offset] = static_cast<std::byte>(value);
  bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
  bytes[offset + 2U] = static_cast<std::byte>(value >> 16U);
  bytes[offset + 3U] = static_cast<std::byte>(value >> 24U);
}

void store_u16(std::span<std::byte> bytes,
               std::size_t offset,
               std::uint16_t value) {
  bytes[offset] = static_cast<std::byte>(value);
  bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

simtower::OriginalTdtLinkName name(const char* text) {
  simtower::OriginalTdtLinkName result{};
  for (std::size_t index = 0; text[index] != 0 && index < 15U; ++index) {
    result.exact_bytes[index] = static_cast<std::byte>(text[index]);
  }
  return result;
}

simtower::OriginalTdtTenant tenant(std::uint16_t left,
                                  std::uint16_t right,
                                  std::int8_t type,
                                  std::uint8_t key) {
  simtower::OriginalTdtTenant result{};
  result.left = left;
  result.right = right;
  result.type = type;
  result.exact_bytes[0] = static_cast<std::byte>(left);
  result.exact_bytes[1] = static_cast<std::byte>(left >> 8U);
  result.exact_bytes[2] = static_cast<std::byte>(right);
  result.exact_bytes[3] = static_cast<std::byte>(right >> 8U);
  result.exact_bytes[4] = static_cast<std::byte>(type);
  result.exact_bytes[12] = static_cast<std::byte>(key);
  return result;
}

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

void place_tenant(simtower::OriginalTdtDocument& tower,
                  std::size_t floor,
                  simtower::OriginalTdtTenant value,
                  std::uint8_t key) {
  tower.floors[floor].tenants.push_back(value);
  tower.floors[floor].tenant_index[key] = static_cast<std::uint16_t>(
      tower.floors[floor].tenants.size() - 1U);
}

}  // namespace

int main(int argc, char** argv) {
  // Direct 10d8:0000 launcher coverage: the raw nonzero/zero selector chooses
  // Person 510/Tenant 520, is forwarded as 1/0, and discards DialogBox result.
  assert(simtower::original_find_launcher_contract(
             simtower::OriginalFindMode::person) ==
         (simtower::OriginalFindLauncherContract{510U, 1, true, false}));
  assert(simtower::original_find_launcher_contract(
             simtower::OriginalFindMode::tenant) ==
         (simtower::OriginalFindLauncherContract{520U, 0, true, false}));

  // Direct 10d8:0487 coverage: Find item 5 receives Win16 LB_GETCURSEL
  // (0x0420) with zero parameters; production maps only the message constant.
  assert(simtower::original_find_selection_query() ==
         (simtower::OriginalFindSelectionQuery{5U, 0x0420U, 0U, 0U}));
  assert(argc == 4);

  {
    // Direct 1188:05e3 coverage: the pointer-table wrapper resolves its slot
    // then LSTRCPYs only the NUL-terminated prefix of the fixed 16-byte name.
    auto fixed = name("ABCDEFGHIJKLMNO");
    assert(simtower::original_find_name_text(fixed) == "ABCDEFGHIJKLMNO");
    fixed.exact_bytes.fill(std::byte{'Z'});
    fixed.exact_bytes[0] = std::byte{'A'};
    fixed.exact_bytes[1] = std::byte{'B'};
    fixed.exact_bytes[2] = std::byte{0};
    fixed.exact_bytes[3] = std::byte{'X'};
    assert(simtower::original_find_name_text(fixed) == "AB");
    fixed.exact_bytes[0] = std::byte{0};
    assert(simtower::original_find_name_text(fixed).empty());
  }

  // FINDDIALOGFILTER 10d8:006f reaches 00a1-014b for WM_INITDIALOG and
  // returns TRUE without SetFocus. Initial focus remains the dialog manager's
  // choice rather than a native-only forced list-item-5 focus.
  using FindFocus = simtower::OriginalFindInitializationFocusPlan;
  assert((simtower::original_find_initialization_focus_plan() ==
          FindFocus{false, true}));

  // Complete FINDDIALOGFILTER 10d8:017b-031c command table. Remove/Go-To
  // ignore notifications; list notifications 1/2/3 select, resolve, cancel;
  // other item-5 notifications remain consumed.
  using FindAction = simtower::OriginalFindDialogCommandAction;
  using FindCommand = simtower::OriginalFindDialogCommandPlan;
  assert((simtower::original_find_dialog_command_plan(1U, 9U) ==
          FindCommand{FindAction::close, true}));
  assert((simtower::original_find_dialog_command_plan(3U, 9U) ==
          FindCommand{FindAction::remove, true}));
  assert((simtower::original_find_dialog_command_plan(4U, 9U) ==
          FindCommand{FindAction::resolve, true}));
  assert((simtower::original_find_dialog_command_plan(5U, 1U) ==
          FindCommand{FindAction::enable_actions, true}));
  assert((simtower::original_find_dialog_command_plan(5U, 2U) ==
          FindCommand{FindAction::resolve, true}));
  assert((simtower::original_find_dialog_command_plan(5U, 3U) ==
          FindCommand{FindAction::disable_actions, true}));
  assert((simtower::original_find_dialog_command_plan(5U, 4U) ==
          FindCommand{FindAction::none, true}));
  assert((simtower::original_find_dialog_command_plan(2U, 0U) ==
          FindCommand{FindAction::none, false}));
  assert((simtower::original_find_dialog_command_plan(99U, 0U) ==
          FindCommand{FindAction::none, false}));

  // Exact FINDDIALOGFILTER 10d8:00ec-0146/0323-0360 presentation contract.
  // Positive 1070:0231 renders BITMAP/510 and controls; it never adds the
  // generic 1068:0567 chrome that a negative DTMP argument would request.
  using FindPhase = simtower::OriginalFindPresentationPhase;
  using FindPresentation = simtower::OriginalFindPresentationPlan;
  assert((simtower::original_find_presentation_plan(
              FindPhase::initialization) ==
          FindPresentation{true, true, false, 12U}));
  assert((simtower::original_find_presentation_plan(FindPhase::paint) ==
          FindPresentation{true, true, false, 0U}));

  // Direct 10e0:0cea coverage: after selecting edit mode two, the successful
  // Find tail presents Command then Map, restores 11f8:3b94 preview scratch,
  // and finally enters 1080:0000's focused camera/full-refresh transaction.
  using FocusRefreshStep = simtower::OriginalFindFocusRefreshStep;
  assert((simtower::original_find_focus_refresh_order() ==
          std::array<FocusRefreshStep, 4>{
              FocusRefreshStep::command_repaint,
              FocusRefreshStep::map_repaint,
              FocusRefreshStep::restore_preview_scratch,
              FocusRefreshStep::camera_transform}));

  const auto pack = read_file(argv[3]);
  const simtower::OriginalResources resources(pack);
  for (const int resource_id : {510, 520}) {
    const auto dialog = simtower::parse_original_dialog(
        resources.find("DIALOG", resource_id));
    assert(dialog.items.size() == 4U);
    assert(dialog.items[0].id == 1U);
    assert(dialog.items[1].id == 3U);
    assert(dialog.items[2].id == 4U);
    assert(dialog.items[3].id == 5U);
    assert(dialog.items[0].window_class.kind ==
           simtower::OriginalDialogValue::Kind::ordinal);
    assert(dialog.items[0].window_class.ordinal == 0x80U);
    assert(dialog.items[3].window_class.kind ==
           simtower::OriginalDialogValue::Kind::ordinal);
    assert(dialog.items[3].window_class.ordinal == 0x83U);

    const auto dtmp = simtower::parse_original_dtmp(
        resources.find("DTMP", resource_id));
    assert(dtmp.bitmap_reference == "510");
    assert(dtmp.bitmap_resource_id == 510);
    assert(dtmp.rectangles ==
           std::vector<simtower::OriginalDtmpRect>({
               {222, 181, 293, 204}, {}, {7, 181, 78, 204},
               {114, 181, 185, 204}, {10, 10, 290, 170},
           }));
  }
  const auto background = simtower::original_dib_view(
      resources.find("BITMAP", 510));
  assert(background.width == 300);
  assert(background.height == 211);
  assert(background.bit_count == 8U);

  // Direct 10e0:06cd floor formatter coverage: above-ground floors are
  // one-based while stored floors 9..0 map to B1..B10.
  assert(simtower::original_find_floor_text(10) == "1");
  assert(simtower::original_find_floor_text(119) == "110");
  assert(simtower::original_find_floor_text(9) == "B1");
  assert(simtower::original_find_floor_text(0) == "B10");
  const auto outside_alert = simtower::parse_original_alert(
      resources.find("ALRT", 1002));
  assert(simtower::format_original_alert(
             outside_alert.message_template, {"Alice", {}, {}, {}}) ==
         "Alice is not in this tower.");
  const auto lobby_alert = simtower::parse_original_alert(
      resources.find("ALRT", 1003));
  assert(simtower::format_original_alert(
             lobby_alert.message_template, {"Alice", "B1", {}, {}}) ==
         "Alice is in the Lobby on Floor B1");

  // Direct 10d8:038e coverage: active saved tenant links remain in their
  // persisted slot order and pair with the name lane at the same index.
  const auto eld = simtower::load_original_tdt_file(argv[1]);
  const auto eld_entries = simtower::original_find_entries(
      eld, simtower::OriginalFindMode::tenant);
  assert(eld_entries.size() == 3U);
  assert(eld_entries[0].link == 2351U);
  assert(eld_entries[0].name == "Dave's Tavern");
  assert(eld_entries[1].name == "Dan's Dew Bar ");
  // Direct 10e0:0000/078d focus coverage: tenant.left plus the signed-type
  // table's exact cell width/2 produces the retained target coordinate.
  const auto eld_focus = simtower::resolve_original_find_tenant(
      eld, static_cast<std::uint16_t>(eld_entries[0].link), 640, 480);
  assert(eld_focus.focused());
  assert(eld_focus.floor == 25);
  assert(eld_focus.x == 171);
  assert(eld_focus.view ==
         simtower::original_facility_focus_view(171, 25, 640, 480));

  // FINDDIALOGFILTER 10d8:02ce and 10e0:04cf/051d retain the embedded target
  // for 300 coarse 16-ms ticks, including signed-magnitude wrap arithmetic.
  simtower::OriginalFindMarkerState marker{};
  assert(!marker.active());
  simtower::start_original_find_marker(marker, eld_focus, 42U, 1000U);
  assert(marker.active() && marker.cell_x == 171 && marker.floor == 25 &&
         marker.selected_person == 42U && marker.started_tick == 1000U);
  // Direct 1100:0000 and 10e0:0cc9 post-modal coverage: only an active person
  // Find coordinate whose retained dword equals the dialog person sets
  // DS:77c0. A tenant Find uses FFFFFFFF and therefore never matches an
  // ordinary person index.
  assert(simtower::original_person_information_sets_find_exit_latch(
      marker, 42U));
  assert(!simtower::original_person_information_sets_find_exit_latch(
      marker, 41U));
  auto tenant_marker = marker;
  tenant_marker.selected_person = 0xffffffffU;
  assert(!simtower::original_person_information_sets_find_exit_latch(
      tenant_marker, 42U));
  auto inactive_marker = marker;
  inactive_marker.cell_x = -1;
  assert(!simtower::original_person_information_sets_find_exit_latch(
      inactive_marker, 42U));
  assert(!simtower::expire_original_find_marker(marker, 1300U));
  assert(marker.active());
  assert(simtower::expire_original_find_marker(marker, 1301U));
  assert(!marker.active() && marker.cell_x == -1 && marker.floor == -1 &&
         marker.selected_person == 0xffffffffU && marker.started_tick == 0U);
  simtower::start_original_find_marker(marker, eld_focus, 0xffffffffU, 1000U);
  assert(!simtower::expire_original_find_marker(marker, 900U));
  assert(simtower::expire_original_find_marker(marker, 699U));
  simtower::OriginalFindResolution invalid{};
  simtower::start_original_find_marker(marker, invalid, 1U, 2000U);
  assert(!marker.active());

  simtower::start_original_find_marker(
      marker, eld_focus, 42U, simtower::original_coarse_tick(16U));
  assert(!simtower::expire_original_find_marker(
      marker, simtower::original_coarse_tick(4816U)));
  assert(simtower::expire_original_find_marker(
      marker, simtower::original_coarse_tick(4832U)));

  const auto empire = simtower::load_original_tdt_file(argv[2]);
  const auto empire_entries = simtower::original_find_entries(
      empire, simtower::OriginalFindMode::tenant);
  assert(empire_entries.size() == 3U);
  assert(empire_entries[0].name == "Jedi Temple");
  const auto cathedral = simtower::resolve_original_find_tenant(
      empire, static_cast<std::uint16_t>(empire_entries[0].link), 800, 600);
  assert(cathedral.focused());
  assert(cathedral.floor == 109);
  assert(cathedral.x == 187);

  {
    // Direct 10d8:0438 -> 1188:0884 tenant-name removal: free the selected
    // name, compact
    // both the word-link and pointer/name lanes, clear their new tail, and
    // preserve that exact ordering across serialization.
    auto edited = eld;
    assert(simtower::remove_original_find_entry(
        edited, simtower::OriginalFindMode::tenant, 1U));
    const auto entries = simtower::original_find_entries(
        edited, simtower::OriginalFindMode::tenant);
    assert(edited.header.tenant_link_count == 2U);
    assert(entries.size() == 2U);
    assert(entries[0].link == 2351U);
    assert(entries[1].link == 3766U);
    assert(entries[1].name == "Mama's Bistro");
    const auto reparsed = simtower::parse_original_tdt(
        simtower::serialize_original_tdt(edited));
    assert(reparsed.header.tenant_link_count == 2U);
    assert(simtower::original_find_entries(
               reparsed, simtower::OriginalFindMode::tenant) == entries);
  }

  auto tower = simtower::make_original_new_tdt();
  place_tenant(tower, 25U, tenant(159U, 183U, 6, 1U), 1U);
  place_tenant(tower, 38U, tenant(159U, 183U, 6, 6U), 6U);
  place_tenant(tower, 40U, tenant(159U, 185U, 13, 6U), 6U);

  tower.header.person_link_count = 2U;
  tower.post_elevator.dce4_person_indices[0] = 0;
  tower.post_elevator.dce4_person_indices[1] = 1;
  tower.person_link_names = {name("Alice"), name("Bob")};
  tower.people[0].exact_bytes[0] = std::byte{25};
  tower.people[0].exact_bytes[1] = std::byte{1};
  tower.people[0].exact_bytes[4] = std::byte{6};
  tower.people[0].exact_bytes[5] = std::byte{0};
  auto focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.focused() && focus.floor == 25 && focus.x == 171);

  tower.people[0].exact_bytes[5] = std::byte{34};
  tower.people[0].exact_bytes[6] = std::byte{2};
  tower.retail[2].exact_bytes[0] = std::byte{38};
  tower.retail[2].exact_bytes[1] = std::byte{6};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.focused() && focus.floor == 38 && focus.x == 171);

  // Direct 10e0:0c72 Medical-person resolution through the selected dbfc
  // service entry.
  tower.people[0].exact_bytes[5] = std::byte{35};
  tower.people[0].exact_bytes[6] = std::byte{3};
  tower.post_elevator.dbfc_dwords[3] = 40U | (6U << 8U);
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.focused() && focus.floor == 40 && focus.x == 172);

  // Direct 10e0:09ce/0aa0/0ad8 coverage begins with the Stair/Escalator focus
  // formula; the following cases cover active Elevator cars and both wait
  // lanes through the complete resolution dispatcher.
  tower.people[0].exact_bytes[5] = std::byte{6};
  tower.people[0].exact_bytes[8] = std::byte{0};
  tower.post_elevator.stairs_bd70[0].used = 1U;
  tower.post_elevator.stairs_bd70[0].x = 100U;
  tower.post_elevator.stairs_bd70[0].floor = 20;
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.focused() && focus.floor == 21 && focus.x == 104);

  auto& elevator = tower.elevators[0];
  elevator.used = 1U;
  elevator.type = 0U;
  elevator.capacity = 42U;
  elevator.x = 200U;
  elevator.bottom_floor = 1;
  elevator.top_floor = 99;
  auto& car = elevator.car_records[0].exact_bytes;
  car[0] = std::byte{30};
  car[1] = std::byte{2};
  car[4] = std::byte{1};
  car[15] = std::byte{1};
  store_u32(car, 16U, 0U);
  tower.people[0].exact_bytes[8] = std::byte{0x40};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.focused() && focus.floor == 29 && focus.x == 203);

  car[15] = std::byte{0};
  elevator.type = 1U;
  elevator.bottom_floor = 20;
  elevator.top_floor = 30;
  elevator.floor_records.push_back({5, 25, {}});
  auto& queue = elevator.floor_records.back().exact_bytes;
  queue[0] = std::byte{1};
  queue[1] = std::byte{0};
  store_u32(queue, 4U, 0U);
  tower.floors[25].left_edge = 0U;
  tower.floors[25].right_edge = 540U;
  tower.people[0].exact_bytes[7] = std::byte{25};
  // Direct 10a8:1dd3/1582/165d coverage: the visible-shaft lookup selects the
  // Elevator before single-entry left/right scans establish their boundaries;
  // the two-entry cases below wrap cursor 39 to zero and advance across a
  // two-cell type-15 person before locating the target.
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.focused() && focus.floor == 25 && focus.x == 197);

  queue[0] = std::byte{0};
  queue[2] = std::byte{1};
  queue[3] = std::byte{0};
  store_u32(queue, 164U, 0U);
  tower.people[0].exact_bytes[8] = std::byte{0x58};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.focused() && focus.floor == 25 && focus.x == 206);

  tower.people_count = std::max<std::uint32_t>(tower.people_count, 2U);
  tower.people[1].exact_bytes[4] = std::byte{15};
  queue[0] = std::byte{2};
  queue[1] = std::byte{39};
  store_u32(queue, 4U + 39U * 4U, 1U);
  store_u32(queue, 4U, 0U);
  tower.people[0].exact_bytes[8] = std::byte{0x40};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.focused() && focus.floor == 25 && focus.x == 195);

  queue[0] = std::byte{0};
  queue[2] = std::byte{2};
  queue[3] = std::byte{39};
  store_u32(queue, 164U + 39U * 4U, 1U);
  store_u32(queue, 164U, 0U);
  tower.people[0].exact_bytes[8] = std::byte{0x58};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.focused() && focus.floor == 25 && focus.x == 208);

  {
    // 10a8:00a8/09e7 must use the renderer's exact diminishing-gap Shell
    // order for Find. For x=[200,100,100,100], it produces table indexes
    // [2,1,3,0], while stable sorting would produce [1,2,3,0] and make the
    // target index-two queue's equal-x left lane empty.
    auto shell_tower = simtower::make_original_new_tdt();
    shell_tower.people_count = 1U;
    auto& waiting = shell_tower.people[0].exact_bytes;
    waiting[4] = std::byte{6};
    waiting[5] = std::byte{6};
    waiting[7] = std::byte{25};
    waiting[8] = std::byte{0x42};
    shell_tower.floors[25].left_edge = 0U;
    shell_tower.floors[25].right_edge = 540U;

    constexpr std::array<std::uint16_t, 4> shaft_x{200U, 100U, 100U, 100U};
    for (std::size_t index = 0U; index < shaft_x.size(); ++index) {
      auto& shaft = shell_tower.elevators[index];
      shaft.used = 1U;
      shaft.type = 1U;
      shaft.x = shaft_x[index];
      shaft.bottom_floor = 25;
      shaft.top_floor = 25;
    }
    simtower::OriginalTdtElevatorFloorRecord target_queue{};
    target_queue.mapped_index = 0;
    target_queue.floor = 25;
    target_queue.exact_bytes[0] = std::byte{1};
    target_queue.exact_bytes[1] = std::byte{0};
    store_u32(target_queue.exact_bytes, 4U, 0U);
    shell_tower.elevators[2].floor_records.push_back(target_queue);

    const auto shell_focus = simtower::resolve_original_find_person(
        shell_tower, 0U, 640, 480);
    assert(shell_focus.focused() && shell_focus.floor == 25 &&
           shell_focus.x == 97);
  }

  tower.people[0].exact_bytes[5] = std::byte{32};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.kind ==
         simtower::OriginalFindResolutionKind::not_in_tower_alert);

  // Complete 10e0:0042 Movie/Party state-zero route through 10e0:0bc6.
  // Its focus center is selected by dc24 byte +7 (31/24 cells), not by the
  // destination tenant's type-width entry. Deliberately retain a type-6
  // destination so the two rules produce different x coordinates.
  tower.people[0].exact_bytes[4] = std::byte{18};
  tower.people[0].exact_bytes[5] = std::byte{0};
  store_u16(tower.floors[25].tenants[0].exact_bytes, 6U, 0U);
  tower.post_elevator.dc24_records[0][0] = std::byte{38};
  tower.post_elevator.dc24_records[0][2] = std::byte{6};
  tower.post_elevator.dc24_records[0][7] = std::byte{0};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.focused() && focus.floor == 38 && focus.x == 174);
  tower.post_elevator.dc24_records[0][7] = std::byte{0xff};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.focused() && focus.floor == 38 && focus.x == 171);

  // Direct 10e0:0669 fallback coverage reached through 10e0:01be ->
  // 10e0:0b61:
  // ALRT/1002 is selected for a missing Retail record, while ALRT/1003 is
  // selected when the record has a floor but its tenant key is -1.
  tower.people[0].exact_bytes[5] = std::byte{34};
  tower.people[0].exact_bytes[6] = std::byte{0xff};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.kind ==
         simtower::OriginalFindResolutionKind::not_in_tower_alert);
  tower.people[0].exact_bytes[6] = std::byte{2};
  tower.retail[2].exact_bytes[0] = std::byte{9};
  tower.retail[2].exact_bytes[1] = std::byte{0xff};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.kind == simtower::OriginalFindResolutionKind::lobby_alert &&
         focus.floor == 9 &&
         simtower::original_find_floor_text(focus.floor) == "B1");

  // 10e0:0814 falls back to the person's current-floor lobby alert when its
  // transit byte is negative. Unsupported person families do nothing.
  tower.people[0].exact_bytes[4] = std::byte{6};
  tower.people[0].exact_bytes[5] = std::byte{6};
  tower.people[0].exact_bytes[7] = std::byte{38};
  tower.people[0].exact_bytes[8] = std::byte{0xff};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.kind == simtower::OriginalFindResolutionKind::lobby_alert &&
         focus.floor == 38);
  tower.people[0].exact_bytes[4] = std::byte{8};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.kind == simtower::OriginalFindResolutionKind::invalid);

  // 10e0:0042/01f3 Housekeeping: states zero/one use the owner or Lobby.
  // State two instead reads the assigned-room floor from byte 6 and its key
  // from word 12, requires Hotel type 3..5 and the room's first guest in
  // state 3, and focuses that room. All failed gates report byte-7's Lobby.
  tower.people[0].exact_bytes[0] = std::byte{25};
  tower.people[0].exact_bytes[1] = std::byte{1};
  tower.people[0].exact_bytes[4] = std::byte{15};
  tower.people[0].exact_bytes[5] = std::byte{0};
  tower.people[0].exact_bytes[7] = std::byte{0xff};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.focused() && focus.floor == 25 && focus.x == 171);
  tower.people[0].exact_bytes[7] = std::byte{38};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.kind == simtower::OriginalFindResolutionKind::lobby_alert &&
         focus.floor == 38);
  tower.people[0].exact_bytes[5] = std::byte{2};
  tower.header.rating = 4U;
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.kind == simtower::OriginalFindResolutionKind::lobby_alert);
  tower.header.rating = 3U;
  tower.people[0].exact_bytes[6] = std::byte{38};
  store_u16(tower.people[0].exact_bytes, 12U, 6U);
  auto& assigned_room = tower.floors[38].tenants[0];
  assigned_room.type = 3;
  assigned_room.exact_bytes[4] = std::byte{3};
  store_u32(assigned_room.exact_bytes, 8U, 1U);
  tower.people[1].exact_bytes[5] = std::byte{3};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.focused() && focus.floor == 38 && focus.x == 161);
  tower.people[1].exact_bytes[5] = std::byte{2};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.kind == simtower::OriginalFindResolutionKind::lobby_alert &&
         focus.floor == 38);
  tower.people[1].exact_bytes[5] = std::byte{3};
  assigned_room.type = 6;
  assigned_room.exact_bytes[4] = std::byte{6};
  focus = simtower::resolve_original_find_person(tower, 0U, 640, 480);
  assert(focus.kind == simtower::OriginalFindResolutionKind::lobby_alert &&
         focus.floor == 38);

  // Direct 1188:0793 person-name removal: the dword person-link and allocated
  // name lanes compact together and the new link tail becomes -1.
  assert(simtower::remove_original_find_entry(
      tower, simtower::OriginalFindMode::person, 0U));
  const auto people = simtower::original_find_entries(
      tower, simtower::OriginalFindMode::person);
  assert(tower.header.person_link_count == 1U);
  assert(people.size() == 1U && people[0].link == 1U &&
         people[0].name == "Bob");
  assert(!simtower::remove_original_find_entry(
      tower, simtower::OriginalFindMode::person, 4U));
  return 0;
}
