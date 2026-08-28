#include "original_construction.hpp"
#include "original_simulation.hpp"
#include "original_tdt.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <bit>
#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <tuple>
#include <utility>

namespace {

bool has_call(const simtower::OriginalSimulationStep& step,
              std::uint16_t selector,
              std::uint16_t offset) {
  return std::ranges::any_of(step.calls, [&](const auto& call) {
    return call.selector == selector && call.offset == offset;
  });
}

std::size_t test_header_offset(const simtower::OriginalTdtDocument& document,
                               std::size_t version_20_offset) {
  std::size_t offset = version_20_offset -
                       (document.header.format_version >= 0x20U ? 0U : 2U);
  if (version_20_offset >= 60U &&
      document.header.format_version < 0x23U) {
    offset -= 2U;
  }
  return offset;
}

std::uint16_t test_header_word(
    const simtower::OriginalTdtDocument& document,
    std::size_t version_20_offset) {
  const auto offset = test_header_offset(document, version_20_offset);
  const auto first = std::to_integer<std::uint8_t>(
      document.header.exact_bytes[offset]);
  const auto second = std::to_integer<std::uint8_t>(
      document.header.exact_bytes[offset + 1U]);
  return document.header.byte_swapped
             ? static_cast<std::uint16_t>((first << 8U) | second)
             : static_cast<std::uint16_t>(first | (second << 8U));
}

void store_test_header_word(simtower::OriginalTdtDocument& document,
                            std::size_t version_20_offset,
                            std::uint16_t value) {
  const auto offset = test_header_offset(document, version_20_offset);
  if (document.header.byte_swapped) {
    document.header.exact_bytes[offset] = static_cast<std::byte>(value >> 8U);
    document.header.exact_bytes[offset + 1U] = static_cast<std::byte>(value);
  } else {
    document.header.exact_bytes[offset] = static_cast<std::byte>(value);
    document.header.exact_bytes[offset + 1U] =
        static_cast<std::byte>(value >> 8U);
  }
}

std::uint32_t test_header_dword(
    const simtower::OriginalTdtDocument& document,
    std::size_t version_20_offset) {
  const auto first = test_header_word(document, version_20_offset);
  const auto second = test_header_word(document, version_20_offset + 2U);
  return document.header.byte_swapped
             ? (static_cast<std::uint32_t>(first) << 16U) | second
             : static_cast<std::uint32_t>(first) |
                   (static_cast<std::uint32_t>(second) << 16U);
}

std::unique_ptr<simtower::OriginalTdtDocument> make_event_test_tower(
    bool byte_swapped = false) {
  auto tower = std::make_unique<simtower::OriginalTdtDocument>(
      simtower::make_original_new_tdt());
  if (byte_swapped) {
    tower->header.raw_version = 0x0024U;
    tower->header.byte_swapped = true;
  }
  tower->header.rating = 3U;
  tower->header.balance = 50'000;
  tower->header.other_income = 1'000;
  tower->header.construction_costs = 2'000;
  tower->header.frame_time = 100U;
  tower->header.lobby_height = 1U;
  tower->random_state = 1U;
  store_test_header_word(*tower, 48U, 1U);  // DS:b3fa Security count

  for (std::size_t floor_number = 10U; floor_number <= 30U;
       ++floor_number) {
    auto& floor = tower->floors[floor_number];
    floor.left_edge = 100U;
    floor.right_edge = 180U;
    simtower::OriginalTdtTenant tenant{};
    tenant.left = 100U;
    tenant.right = 120U;
    tenant.type = floor_number == 20U ? 14 : 7;
    tenant.exact_bytes[4] = static_cast<std::byte>(tenant.type);
    floor.tenants.push_back(tenant);
    floor.tenant_index[0] = 0U;
  }

  // One exact cf88 Security registration with six records beginning at zero.
  tower->post_elevator.cf88_words[0] = 20U;
  for (std::size_t index = 0; index < 6U; ++index) {
    auto& exact = tower->people[index].exact_bytes;
    exact[5] = std::byte{0x7f};
    exact[7] = std::byte{0x7f};
    exact[8] = std::byte{0x7f};
    for (const auto offset : {10U, 12U, 14U}) {
      exact[offset] = std::byte{0x55};
      exact[offset + 1U] = std::byte{0xaa};
    }
  }
  return tower;
}

}  // namespace

int main() {
  using simtower::OriginalSimulationCall;
  using simtower::OriginalSimulationState;

  // Exercise one complete 1200:0196 cycle with values chosen to take every
  // conditional branch: day 419 triggers the 60/84/12-day events before the
  // day rolls to 420, which then triggers the three-day maintenance branch.
  // The resulting set is the complete native callback boundary, including
  // each exact argument signature.
  {
    OriginalSimulationState coverage{};
    coverage.frame_time = 0xffffU;
    coverage.current_day = 419;
    std::set<std::tuple<std::uint16_t, std::uint16_t, std::size_t>> emitted;
    for (std::uint32_t frame = 0; frame < 0x0a28U; ++frame) {
      const auto coverage_step = simtower::step_original_simulation(
          coverage, frame, true, true);
      assert(coverage_step.advanced);
      for (const auto& call : coverage_step.calls) {
        assert(simtower::original_simulation_call_supported(call));
        emitted.emplace(call.selector, call.offset, call.arguments.size());
      }
    }
    assert(emitted.size() == 36U);
    assert(!simtower::original_simulation_call_supported(0xffffU, 0xffffU, 0U));
    assert(!simtower::original_simulation_call_supported(
        0x11c8U, 0x0167U, 2U));
  }

  // Direct 1200:0543/0558 coverage. Day and calendar phase use signed
  // CWD/IDIV; high-bit values are negative, not late-day phases.
  assert(simtower::original_day_phase(0) == 0);
  assert(simtower::original_day_phase(399) == 0);
  assert(simtower::original_day_phase(400) == 1);
  assert(simtower::original_day_phase(0x09e5) == 6);
  assert(simtower::original_day_phase(0xfe70U) == -1);  // signed -400
  assert(simtower::original_day_phase(0x8000U) == -81);
  assert(simtower::original_day_phase(0xffffU) == 0);   // signed -1 / 400
  assert(simtower::original_calendar_phase(0) == 0);
  assert(simtower::original_calendar_phase(2) == 1);
  assert(simtower::original_calendar_phase(3) == 0);
  assert(simtower::original_calendar_phase(11) == 1);

  {
    // Direct 10c8:033e/03b0 coverage: floor-zero starts are deliberately
    // ignored, an unterminated run through floor 119 fails without consuming
    // rand(), and a valid range consumes one exact Microsoft-C value.
    auto tower = simtower::make_original_new_tdt();
    tower.floors[0].tenants.emplace_back();
    assert(simtower::select_original_event_floor(tower, 0) == -1);
    assert(tower.random_state == 1U);

    tower = simtower::make_original_new_tdt();
    for (std::size_t floor = 1U; floor <= 3U; ++floor) {
      tower.floors[floor].tenants.emplace_back();
    }
    assert(simtower::select_original_event_floor(tower, 4) == -1);
    assert(tower.random_state == 1U);
    assert(simtower::select_original_event_floor(tower, 1) == 2);
    assert(tower.random_state == 22'695'478U);

    tower = simtower::make_original_new_tdt();
    for (auto& floor : tower.floors) floor.tenants.emplace_back();
    assert(simtower::select_original_event_floor(tower, 1) == -1);
    assert(tower.random_state == 1U);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    auto& state = tower.post_elevator.version_18_dd6c;
    state[2] = std::byte{0xaa};
    state[3] = std::byte{0xbb};
    auto started = simtower::start_original_annual_effect(tower);
    assert(started.started && started.notification_code == 7U);
    assert(state[0] == std::byte{1} && state[1] == std::byte{0});
    // Direct 11b8:0028/0060 coverage: the start deliberately preserves dd6e
    // while initializing dd70/dd72; subsequent advances use the sprite's
    // right edge before clearing the complete state off-screen.
    assert(state[2] == std::byte{0xaa} && state[3] == std::byte{0xbb});
    assert(state[4] == std::byte{0x2c} && state[5] == std::byte{0x0b});
    assert(state[6] == std::byte{0x94} && state[7] == std::byte{0x08});

    started = simtower::start_original_annual_effect(tower);
    assert(!started.started && started.notification_code == 0U);
    assert(simtower::advance_original_annual_effect(tower));
    assert(state[4] == std::byte{0x22} && state[5] == std::byte{0x0b});
    assert(state[6] == std::byte{0x95} && state[7] == std::byte{0x08});

    // x==-130 advances to -140; adding BITMAP/904's 140-pixel right edge
    // produces zero and invokes the complete 11b8:0000 clear.
    state[4] = std::byte{0x7e};
    state[5] = std::byte{0xff};
    assert(simtower::advance_original_annual_effect(tower));
    assert(std::ranges::all_of(state, [](std::byte value) {
      return value == std::byte{0};
    }));
    assert(!simtower::advance_original_annual_effect(tower));

    // The eight-byte object exists at runtime for a pre-0x18 document even
    // though that revision does not serialize it.
    tower.header.raw_version = 0x1700U;
    tower.header.format_version = 0x17U;
    tower.post_elevator.version_18_dd6c.clear();
    started = simtower::start_original_annual_effect(tower);
    assert(started.started &&
           tower.post_elevator.version_18_dd6c.size() == 8U);
  }

  {
    // Complete 11c8:03ab/05e8/0671 ambient dispatch path. Seed 278 is the first small seed whose
    // initial Microsoft-runtime value passes the exact one-in-sixteen gate;
    // its next probe is zero and its third value is odd.
    auto tower = simtower::make_original_new_tdt();
    tower.header.exact_bytes[60] = std::byte{0};
    tower.header.exact_bytes[61] = std::byte{0};
    tower.random_state = 278U;
    tower.floors[12].tenants.clear();
    simtower::OriginalTdtTenant tenant{};
    tenant.left = 20U;
    tenant.right = 30U;
    tenant.type = 6;
    tenant.status = 0U;
    tenant.variant = 0U;
    tower.floors[12].tenants.push_back(tenant);
    // 11c8:07d2 uses the 18-byte Retail table at DS:b7e2. A deliberately
    // contradictory 16-byte people record must not affect the decision.
    tower.retail[0].exact_bytes[2] = std::byte{0};
    tower.retail[0].exact_bytes[9] = std::byte{1};
    tower.people_count = 1U;
    tower.people.resize(1U);
    tower.people[0].exact_bytes[2] = std::byte{0xff};
    tower.people[0].exact_bytes[9] = std::byte{0};

    auto resource = simtower::select_original_ambient_sound(
        tower, true, 0, 3600, 800, 500);
    assert(resource && *resource == 1385);
    assert(tower.random_state == 2'297'915'469U);

    // A fixed event consumes only the gate and probe values, not a third RNG
    // value in the 11c8:0426 switch.
    tower.random_state = 278U;
    tower.floors[12].tenants[0].type = 3;
    tower.floors[12].tenants[0].status = 1U;
    resource = simtower::select_original_ambient_sound(
        tower, true, 0, 3600, 800, 500);
    assert(resource && *resource == 1577);
    assert(tower.random_state == 388'546'716U);

    // Both static gates precede rand() in the executable.
    tower.random_state = 278U;
    resource = simtower::select_original_ambient_sound(
        tower, false, 0, 3600, 800, 500);
    assert(!resource && tower.random_state == 278U);
    tower.header.exact_bytes[60] = std::byte{1};
    resource = simtower::select_original_ambient_sound(
        tower, true, 0, 3600, 800, 500);
    assert(!resource && tower.random_state == 278U);
    tower.header.exact_bytes[60] = std::byte{0};

    // A failed one-in-sixteen test consumes exactly the first value.
    tower.random_state = 1U;
    resource = simtower::select_original_ambient_sound(
        tower, true, 0, 3600, 800, 500);
    assert(!resource && tower.random_state == 0x015a4e36U);

    // Empty above-ground probes select the three exact contextual resources.
    tower.floors[12].tenants.clear();
    tower.random_state = 278U;
    tower.post_elevator.version_18_dd6c[0] = std::byte{1};
    resource = simtower::select_original_ambient_sound(
        tower, true, 0, 3600, 800, 500);
    assert(resource && *resource == 10002);
    assert(tower.random_state == 388'546'716U);

    tower.post_elevator.version_18_dd6c[0] = std::byte{0};
    tower.header.current_day = 6;
    tower.header.frame_time = 0U;
    tower.random_state = 278U;
    resource = simtower::select_original_ambient_sound(
        tower, true, 0, 3600, 800, 500);
    assert(resource && *resource == 10012);

    tower.header.current_day = 9;
    tower.header.frame_time = 1600U;
    tower.random_state = 278U;
    resource = simtower::select_original_ambient_sound(
        tower, true, 0, 3600, 800, 500);
    assert(resource && *resource == 10011);

    // The same probe below coordinate ten produces the executable's -2
    // sentinel, so no contextual background sound is selected.
    tower.floors[8].tenants.clear();
    tower.random_state = 278U;
    resource = simtower::select_original_ambient_sound(
        tower, true, 0, 3744, 800, 500);
    assert(!resource);
    assert(tower.random_state == 388'546'716U);
  }

  {
    // Direct 11c8:03fb coverage for 1100:03ac Facility Information. Event
    // selection precedes 0426's master-sound gate; only variable resources
    // consume one shared Microsoft-runtime random value.
    auto tower = simtower::make_original_new_tdt();
    simtower::OriginalTdtTenant restaurant{};
    restaurant.type = 6;
    restaurant.variant = 0U;
    restaurant.exact_bytes[4] = std::byte{6};
    tower.floors[10].tenants.push_back(restaurant);
    tower.retail[0].exact_bytes[2] = std::byte{0};
    tower.retail[0].exact_bytes[9] = std::byte{1};
    tower.random_state = 1U;
    auto resource = simtower::select_original_facility_sound(
        tower, true, 10, 0U);
    assert(resource && *resource == 1384);
    assert(tower.random_state == 0x015a4e36U);

    tower.random_state = 1U;
    resource = simtower::select_original_facility_sound(
        tower, false, 10, 0U);
    assert(!resource && tower.random_state == 1U);
    tower.retail[0].exact_bytes[2] = std::byte{0xff};
    resource = simtower::select_original_facility_sound(
        tower, true, 10, 0U);
    assert(!resource && tower.random_state == 1U);

    tower.floors[10].tenants[0].type = 3;
    tower.floors[10].tenants[0].status = 1U;
    tower.random_state = 1U;
    resource = simtower::select_original_facility_sound(
        tower, true, 10, 0U);
    assert(resource && *resource == 1577);
    assert(tower.random_state == 1U);
    assert(!simtower::select_original_facility_sound(
        tower, true, -1, 0U));
    assert(!simtower::select_original_facility_sound(
        tower, true, 10, 1U));
  }

  {
    auto tower = simtower::make_original_new_tdt();
    tower.header.current_day = 4;
    tower.header.rating = 4U;
    tower.header.version_20_word = 0U;
    tower.header.exact_bytes[60] = std::byte{4};
    tower.header.exact_bytes[61] = std::byte{0};
    assert(!simtower::original_special_event_audio_active(tower));
    assert(!simtower::original_emergency_people_pass_active(tower));
    store_test_header_word(tower, 60U, 0x0001U);
    assert(simtower::original_emergency_people_pass_active(tower));
    store_test_header_word(tower, 60U, 0x0008U);
    assert(simtower::original_emergency_people_pass_active(tower));
    store_test_header_word(tower, 60U, 0x0010U);
    assert(!simtower::original_emergency_people_pass_active(tower));
    store_test_header_word(tower, 60U, 0x0004U);
    // Direct 1020:0dcb/0e0b coverage: raise only on the scheduled day/rating,
    // reject duplicate raises, then clear only bit four.
    assert(simtower::raise_original_periodic_b406_flag(tower));
    assert(simtower::original_special_event_audio_active(tower));
    assert(tower.header.version_20_word == 1U);
    assert(tower.header.exact_bytes[60] == std::byte{0x14});
    assert(!simtower::raise_original_periodic_b406_flag(tower));
    assert(simtower::clear_original_periodic_b406_flag(tower));
    assert(tower.header.exact_bytes[60] == std::byte{4});
    assert(!simtower::clear_original_periodic_b406_flag(tower));

    tower.header.current_day = 3;
    assert(!simtower::raise_original_periodic_b406_flag(tower));
    tower.header.current_day = 12;
    tower.header.rating = 5U;
    assert(!simtower::raise_original_periodic_b406_flag(tower));

    // Revision 0x22 omits b404, so runtime b406 is serialized at byte 58.
    tower = simtower::make_original_new_tdt();
    tower.header.raw_version = 0x2200U;
    tower.header.format_version = 0x22U;
    tower.header.exact_bytes.erase(tower.header.exact_bytes.begin() + 58,
                                   tower.header.exact_bytes.begin() + 60);
    tower.header.current_day = 4;
    tower.header.rating = 1U;
    tower.header.exact_bytes[58] = std::byte{0};
    tower.header.exact_bytes[59] = std::byte{0};
    assert(simtower::raise_original_periodic_b406_flag(tower));
    assert(tower.header.exact_bytes[58] == std::byte{0x10});
    assert(simtower::original_special_event_audio_active(tower));
    assert(!simtower::original_emergency_people_pass_active(tower));
    store_test_header_word(tower, 60U, 0x0008U);
    assert(simtower::original_emergency_people_pass_active(tower));
    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.header.format_version == 0x22U);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Direct 1240:01de coverage: day remainder three is an exact no-op; every
    // other day clears b928 and restores the b924 sentinel.
    auto tower = simtower::make_original_new_tdt();
    tower.header.current_day = 3;
    tower.post_elevator.b924 = 1234;
    tower.post_elevator.b928 = 7U;
    assert(!simtower::reset_original_periodic_b924_state(tower));
    assert(tower.post_elevator.b924 == 1234);
    assert(tower.post_elevator.b928 == 7U);
    tower.header.current_day = 4;
    assert(simtower::reset_original_periodic_b924_state(tower));
    assert(tower.post_elevator.b924 == -1);
    assert(tower.post_elevator.b928 == 0U);
    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.post_elevator.b924 == -1);
    assert(reparsed.post_elevator.b928 == 0U);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Direct 1060:003a coverage: the quarterly reset snapshots balance and
    // clears only income/maintenance, preserving the population accounting.
    auto tower = simtower::make_original_new_tdt();
    tower.header.balance = 0x12345678;
    tower.header.last_quarter_money = -1;
    tower.header.other_income = 91;
    tower.header.construction_costs = -73;
    auto& finance = tower.post_elevator.finance;
    for (std::size_t index = 0; index < 10U; ++index) {
      finance.population_by_category[index] =
          static_cast<std::int32_t>(index + 10U);
      finance.income_by_category[index] =
          static_cast<std::int32_t>(index + 20U);
      finance.maintenance_by_category[index] =
          -static_cast<std::int32_t>(index + 30U);
    }
    finance.total_population = 123;
    finance.total_income = 456;
    finance.total_maintenance = -789;
    const auto population = finance.population_by_category;

    simtower::reset_original_quarter_finance(tower);
    assert(tower.header.last_quarter_money == 0x12345678);
    assert(tower.header.balance == 0x12345678);
    assert(tower.header.other_income == 0);
    assert(tower.header.construction_costs == 0);
    assert(finance.population_by_category == population);
    assert(finance.total_population == 123);
    for (std::size_t index = 0; index < 10U; ++index) {
      assert(finance.income_by_category[index] == 0);
      assert(finance.maintenance_by_category[index] == 0);
    }
    assert(finance.total_income == 0);
    assert(finance.total_maintenance == 0);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.header.last_quarter_money == 0x12345678);
    assert(reparsed.post_elevator.finance.total_income == 0);
    assert(reparsed.post_elevator.finance.total_maintenance == 0);
    assert(reparsed.post_elevator.finance.total_population == 123);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Direct 1060:08be/0958 jump-table coverage: exercise every mapped key
    // and both sides of each recovered bounded/default selector.
    constexpr std::array<std::pair<std::uint16_t, std::int16_t>, 14>
        finance_mappings{{
        std::pair<std::uint16_t, std::int16_t>{7, 0},   {3, 1},
        {4, 2},   {5, 3},   {10, 4},  {12, 5}, {6, 6},
        {29, 7},  {30, 7},  {18, 8},  {19, 8}, {34, 8},
        {35, 8},  {9, 9},
    }};
    for (const auto& [type, category] : finance_mappings) {
      assert(simtower::original_finance_category_for_type(type) == category);
    }
    assert(simtower::original_finance_category_for_type(0) == -1);
    assert(simtower::original_finance_category_for_type(36) == -1);

    constexpr std::array<std::pair<std::uint16_t, std::int16_t>, 10>
        maintenance_mappings{{
        std::pair<std::uint16_t, std::int16_t>{24, 0}, {1, 1},  {42, 2},
        {43, 3}, {27, 4}, {44, 5}, {20, 6}, {31, 7}, {15, 8}, {14, 9},
    }};
    for (const auto& [type, category] : maintenance_mappings) {
      assert(simtower::original_maintenance_category_for_type(type) ==
             category);
    }
    assert(simtower::original_maintenance_category_for_type(0) == -1);
    assert(simtower::original_maintenance_category_for_type(45) == -1);

    // Direct 1060:07b3/0880 accounting-core coverage: mapped and unmapped
    // types update their exact bands/totals with 32-bit wrapping arithmetic.
    auto tower = simtower::make_original_new_tdt();
    auto& finance = tower.post_elevator.finance;
    finance.population_by_category[1] = 30;
    finance.total_population = 100;
    assert(simtower::clear_original_population_for_type(tower, 3));
    assert(finance.population_by_category[1] == 0);
    assert(finance.total_population == 70);
    assert(!simtower::clear_original_population_for_type(tower, 2));
    assert(finance.total_population == 70);

    simtower::add_original_population_for_type(tower, 3, -5);
    assert(finance.population_by_category[1] == -5);
    assert(finance.total_population == 65);
    simtower::add_original_population_for_type(tower, 2, 7);
    assert(finance.population_by_category[1] == -5);
    assert(finance.total_population == 72);

    finance.income_by_category[6] = std::numeric_limits<std::int32_t>::max();
    finance.total_income = std::numeric_limits<std::int32_t>::max();
    simtower::add_original_income_for_type(tower, 6, 1);
    assert(finance.income_by_category[6] ==
           std::numeric_limits<std::int32_t>::min());
    assert(finance.total_income == std::numeric_limits<std::int32_t>::min());
    tower.header.other_income = -10;
    simtower::add_original_income_for_type(tower, 2, 3);
    assert(tower.header.other_income == -7);
    assert(finance.total_income == std::numeric_limits<std::int32_t>::min());

    finance.maintenance_by_category[9] = -5;
    finance.total_maintenance = std::numeric_limits<std::int32_t>::max();
    simtower::add_original_maintenance_for_type(tower, 14, 9);
    assert(finance.maintenance_by_category[9] == 4);
    assert(finance.total_maintenance ==
           std::numeric_limits<std::int32_t>::min() + 8);
    simtower::add_original_maintenance_for_type(tower, 2, -8);
    assert(finance.maintenance_by_category[9] == 4);
    assert(finance.total_maintenance ==
           std::numeric_limits<std::int32_t>::min());

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.post_elevator.finance.population_by_category[1] == -5);
    assert(reparsed.post_elevator.finance.total_population == 72);
    assert(reparsed.post_elevator.finance.income_by_category[6] ==
           std::numeric_limits<std::int32_t>::min());
    assert(reparsed.header.other_income == -7);
    assert(reparsed.post_elevator.finance.maintenance_by_category[9] == 4);
    assert(reparsed.post_elevator.finance.total_maintenance ==
           std::numeric_limits<std::int32_t>::min());
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    simtower::OriginalTdtTenant metro_upper{};
    metro_upper.type = 31;
    metro_upper.exact_bytes[4] = std::byte{31};
    simtower::OriginalTdtTenant metro_middle = metro_upper;
    metro_middle.type = 32;
    metro_middle.exact_bytes[4] = std::byte{32};
    metro_middle.variant = 2U;
    metro_middle.exact_bytes[6] = std::byte{2};
    simtower::OriginalTdtTenant metro_lower = metro_upper;
    metro_lower.type = 33;
    metro_lower.exact_bytes[4] = std::byte{33};
    metro_lower.variant = 0U;
    metro_lower.preserved_07_to_0f[0] = std::byte{1};
    metro_lower.exact_bytes[7] = std::byte{1};
    simtower::OriginalTdtTenant unrelated = metro_upper;
    unrelated.type = 36;
    unrelated.exact_bytes[4] = std::byte{36};
    tower.floors[10].tenants = {
        metro_upper, metro_middle, metro_lower, unrelated};

    // Direct 11e8:0273 Metro-pulse coverage. The b3e8 no-Metro sentinel
    // prevents even the RNG advance.
    tower.header.exact_bytes[30] = std::byte{0xff};
    tower.header.exact_bytes[31] = std::byte{0xff};
    tower.random_state = 54U;
    auto pulse = simtower::pulse_original_metro_effects(tower);
    assert(pulse.touched == 0U && !pulse.play_transition_sound);
    assert(tower.random_state == 54U);

    // b406 bits zero or three have the same early-return behavior.
    tower.header.exact_bytes[30] = std::byte{2};
    tower.header.exact_bytes[31] = std::byte{0};
    tower.header.exact_bytes[60] = std::byte{8};
    pulse = simtower::pulse_original_metro_effects(tower);
    assert(pulse.touched == 0U && tower.random_state == 54U);

    // Seed one advances to 0x015a4e36 and rand() returns 346, so this call
    // consumes the RNG value but does not select the one-in-100 pulse.
    tower.header.exact_bytes[60] = std::byte{0};
    tower.random_state = 1U;
    pulse = simtower::pulse_original_metro_effects(tower);
    assert(pulse.touched == 0U && !pulse.play_transition_sound);
    assert(tower.random_state == 0x015a4e36U);

    // Seed 54 produces rand()==18700. Zero words become two, while every
    // nonzero word becomes zero; all Metro parts are marked dirty.
    tower.random_state = 54U;
    pulse = simtower::pulse_original_metro_effects(tower);
    assert(pulse.touched == 3U && pulse.play_transition_sound);
    assert(tower.random_state == 1'225'555'759U);
    const auto& tenants = tower.floors[10].tenants;
    assert(tenants[0].variant == 2U &&
           tenants[0].exact_bytes[7] == std::byte{0});
    assert(tenants[1].variant == 0U);
    assert(tenants[2].variant == 0U &&
           tenants[2].exact_bytes[7] == std::byte{0});
    assert(tenants[3].variant == 0U);
    for (std::size_t index = 0; index < 3U; ++index) {
      assert(tenants[index].exact_bytes[13] == std::byte{1});
    }
    assert(tenants[3].exact_bytes[13] == std::byte{0});

    // A pulse in which every part goes from two to zero emits no sound.
    for (std::size_t index = 0; index < 3U; ++index) {
      tower.floors[10].tenants[index].variant = 2U;
      tower.floors[10].tenants[index].exact_bytes[6] = std::byte{2};
    }
    tower.random_state = 54U;
    pulse = simtower::pulse_original_metro_effects(tower);
    assert(pulse.touched == 3U && !pulse.play_transition_sound);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.floors[10].tenants[0].variant == 0U);
    assert(reparsed.floors[10].tenants[2].exact_bytes[13] == std::byte{1});
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Direct 1178:0854/08ec YEN/1001 accounting coverage: ordinary add and
    // removal, tier-four/no-charge and malformed bounds, the add-only signed
    // 99,999,999 cap, and wrapping removal-side NEG.
    simtower::OriginalYenTable rent{};
    rent[28] = 150U;   // type 7, tier 0
    rent[29] = 100U;   // type 7, tier 1
    rent[36] = 2'000U; // type 9, tier 0
    rent[40] = 200U;   // type 10, tier 0

    auto tower = simtower::make_original_new_tdt();
    tower.header.balance = 1'000;
    simtower::add_original_rent_income(tower, rent, 7, 1);
    assert(tower.header.balance == 1'100);
    assert(tower.post_elevator.finance.income_by_category[0] == 100);
    assert(tower.post_elevator.finance.total_income == 100);

    simtower::remove_original_rent_income(tower, rent, 7, 0);
    assert(tower.header.balance == 950);
    assert(tower.post_elevator.finance.income_by_category[0] == -50);
    assert(tower.post_elevator.finance.total_income == -50);

    // Tier four is the hard-coded no-charge sentinel; malformed native
    // indices are bounded rather than reading outside the 180-byte table.
    simtower::add_original_rent_income(tower, rent, 7, 4);
    simtower::remove_original_rent_income(tower, rent, 44, 0);
    simtower::add_original_rent_income(tower, rent, 7, 5);
    assert(tower.header.balance == 950);

    // 1178:1377 caps only the add path.
    tower = simtower::make_original_new_tdt();
    tower.header.balance = 99'999'990;
    simtower::add_original_rent_income(tower, rent, 7, 0);
    assert(tower.header.balance == 99'999'999);
    assert(tower.post_elevator.finance.income_by_category[0] == 9);
    assert(tower.post_elevator.finance.total_income == 9);

    // Removal passes a wrapping 32-bit NEG to 1060:0837.
    rent[40] = std::bit_cast<std::uint32_t>(
        std::numeric_limits<std::int32_t>::min());
    tower = simtower::make_original_new_tdt();
    tower.header.balance = 0;
    simtower::remove_original_rent_income(tower, rent, 10, 0);
    assert(tower.header.balance == std::numeric_limits<std::int32_t>::min());
    assert(tower.post_elevator.finance.income_by_category[4] ==
           std::numeric_limits<std::int32_t>::min());
    assert(tower.post_elevator.finance.total_income ==
           std::numeric_limits<std::int32_t>::min());

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.header.balance == std::numeric_limits<std::int32_t>::min());
    assert(reparsed.post_elevator.finance.income_by_category[4] ==
           std::numeric_limits<std::int32_t>::min());
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Direct 1180:05af/0b3c day-start coverage: clear both entertainment
    // population categories, select the signed-quotient PART capacity tier,
    // and rewrite every live Movie/Party service record without touching an
    // inactive one.
    simtower::OriginalPartTable part{};
    part.words_52_to_ac[23] = 60U;  // PART +0x80
    part.words_52_to_ac[24] = 60U;
    part.words_52_to_ac[25] = 40U;
    part.words_52_to_ac[26] = 20U;
    part.words_52_to_ac[27] = 40U;  // PART +0x88
    part.words_52_to_ac[28] = 40U;
    part.words_52_to_ac[29] = 40U;
    part.words_52_to_ac[30] = 20U;

    auto tower = simtower::make_original_new_tdt();
    auto& finance = tower.post_elevator.finance;
    finance.population_by_category[8] = 123;  // Movie Theater
    finance.population_by_category[7] = 456;  // Party Hall
    finance.population_by_category[0] = 10;
    finance.total_population = 589;

    auto& movie_low = tower.post_elevator.dc24_records[0];
    movie_low.fill(std::byte{0x55});
    movie_low[0] = std::byte{9};
    movie_low[7] = std::byte{0};
    movie_low[9] = std::byte{0};
    auto& movie_high = tower.post_elevator.dc24_records[1];
    movie_high.fill(std::byte{0x55});
    movie_high[0] = std::byte{8};
    movie_high[7] = std::byte{8};
    movie_high[9] = std::byte{6};
    auto& party = tower.post_elevator.dc24_records[2];
    party.fill(std::byte{0x55});
    party[0] = std::byte{7};
    party[7] = std::byte{0xff};
    party[9] = std::byte{0x7f};
    const auto inactive_before = tower.post_elevator.dc24_records[3];

    simtower::reset_original_entertainment_for_day(tower, part);
    assert(movie_low[4] == std::byte{40} && movie_low[5] == std::byte{40});
    assert(movie_low[9] == std::byte{1});
    assert(movie_high[4] == std::byte{40} &&
           movie_high[5] == std::byte{40});
    assert(movie_high[9] == std::byte{7});
    assert(party[4] == std::byte{0} && party[5] == std::byte{50});
    assert(party[9] == std::byte{0x7f});
    for (const auto index : {0U, 1U, 2U}) {
      const auto& record = tower.post_elevator.dc24_records[index];
      assert(record[8] == std::byte{0});
      assert(record[10] == std::byte{0});
      assert(record[11] == std::byte{0});
    }
    assert(tower.post_elevator.dc24_records[3] == inactive_before);
    assert(finance.population_by_category[8] == 160);
    assert(finance.population_by_category[7] == 50);
    assert(finance.population_by_category[0] == 10);
    assert(finance.total_population == 220);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.post_elevator.dc24_records[0][4] == std::byte{40});
    assert(reparsed.post_elevator.dc24_records[2][5] == std::byte{50});
    assert(reparsed.post_elevator.finance.total_population == 220);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Direct 1180:06a8/090a and 1180:0f87 coverage: arrivals reset only the
    // selected service side; 0f87 dirties two-tenant Movie spans for a
    // nonnegative byte-seven, but only one tenant per Party half for its
    // negative sentinel. Both departure selectors perform the recovered
    // state-3/countdown pass. Side zero republishes state 1/2, while side one
    // closes the service and routes Movie/Party revenue through 1178:126c,
    // including both event suppressors.
    auto tower = simtower::make_original_new_tdt();
    tower.people.clear();
    tower.people.resize(112U);
    tower.people_count = 112U;

    const auto make_tenant = [](std::int8_t type,
                                std::uint32_t people_start,
                                std::uint8_t key) {
      simtower::OriginalTdtTenant tenant{};
      tenant.type = type;
      tenant.exact_bytes[4] = static_cast<std::byte>(type);
      tenant.exact_bytes[8] = static_cast<std::byte>(people_start);
      tenant.exact_bytes[9] = static_cast<std::byte>(people_start >> 8U);
      tenant.exact_bytes[10] = static_cast<std::byte>(people_start >> 16U);
      tenant.exact_bytes[11] = static_cast<std::byte>(people_start >> 24U);
      tenant.exact_bytes[12] = static_cast<std::byte>(key);
      std::copy(tenant.exact_bytes.begin() + 7,
                tenant.exact_bytes.begin() + 16,
                tenant.preserved_07_to_0f.begin());
      return tenant;
    };
    tower.floors[9].tenants = {
        make_tenant(34, 0U, 0U), make_tenant(18, 0U, 1U)};
    tower.floors[8].tenants = {
        make_tenant(35, 56U, 0U), make_tenant(19, 56U, 1U)};
    tower.floors[9].tenant_index[0] = 0U;
    tower.floors[8].tenant_index[0] = 0U;

    auto& service = tower.post_elevator.dc24_records[0];
    service.fill(std::byte{0});
    service[0] = std::byte{9};
    service[1] = std::byte{8};
    service[2] = std::byte{0};
    service[3] = std::byte{0};
    service[7] = std::byte{0};  // non-negative means Movie Theater

    simtower::begin_original_entertainment_arrivals(tower, 0U, 1U);
    assert(service[6] == std::byte{1});
    for (std::size_t index = 0; index < 56U; ++index) {
      assert(tower.people[index].exact_bytes[5] == std::byte{0x20});
    }
    for (std::size_t index = 56U; index < 112U; ++index) {
      assert(tower.people[index].exact_bytes[5] == std::byte{0});
    }
    for (const auto floor : {9U, 8U}) {
      assert(tower.floors[floor].tenants[0].exact_bytes[13] == std::byte{1});
      assert(tower.floors[floor].tenants[1].exact_bytes[13] == std::byte{1});
    }

    for (const auto floor : {9U, 8U}) {
      tower.floors[floor].tenants[0].exact_bytes[13] = std::byte{0};
      tower.floors[floor].tenants[0].preserved_07_to_0f[6] = std::byte{0};
      tower.floors[floor].tenants[1].exact_bytes[13] = std::byte{0};
      tower.floors[floor].tenants[1].preserved_07_to_0f[6] = std::byte{0};
    }
    service[7] = std::byte{0xff};
    simtower::begin_original_entertainment_arrivals(tower, 0U, 0U);
    for (const auto floor : {9U, 8U}) {
      assert(tower.floors[floor].tenants[0].exact_bytes[13] == std::byte{1});
      assert(tower.floors[floor].tenants[1].exact_bytes[13] == std::byte{0});
    }
    service[7] = std::byte{0};

    // 06a8 changes dc2a only when it is exactly zero. Its side-one pass
    // resets the second 56-person run without revisiting the first.
    service[6] = std::byte{7};
    tower.people[0].exact_bytes[5] = std::byte{0x55};
    tower.people[56].exact_bytes[5] = std::byte{0x66};
    simtower::begin_original_entertainment_arrivals(tower, 1U, 1U);
    assert(service[6] == std::byte{7});
    assert(tower.people[0].exact_bytes[5] == std::byte{0x55});
    for (std::size_t index = 56U; index < 112U; ++index) {
      assert(tower.people[index].exact_bytes[5] == std::byte{0x20});
    }

    tower.people[0].exact_bytes[5] = std::byte{3};
    tower.people[1].exact_bytes[5] = std::byte{3};
    tower.header.frame_time = 1500U;  // day phase three
    service[10] = std::byte{2};
    const auto arrival_finish = simtower::finish_original_entertainment_phase(
        tower, simtower::OriginalPartTable{}, 0U, 1U);
    assert(arrival_finish.codes.empty());
    assert(tower.people[0].exact_bytes[5] == std::byte{1});
    assert(tower.people[1].exact_bytes[5] == std::byte{1});
    assert(service[10] == std::byte{0});
    assert(service[6] == std::byte{1});

    // Direct 1180:0826 coverage: scan all sixteen dc24 slots, require a
    // nonnegative upper floor, matching signed byte-seven service group, and
    // signed state >=2 before writing state three and dirtying the linked
    // tenants. These sentinels pin every rejection gate, including 0x81.
    auto& negative_floor = tower.post_elevator.dc24_records[1];
    negative_floor.fill(std::byte{0});
    negative_floor[0] = std::byte{0xff};
    negative_floor[6] = std::byte{2};
    auto& other_group = tower.post_elevator.dc24_records[2];
    other_group.fill(std::byte{0});
    other_group[0] = std::byte{1};
    other_group[6] = std::byte{2};
    other_group[7] = std::byte{0xff};
    auto& signed_low_state = tower.post_elevator.dc24_records[3];
    signed_low_state.fill(std::byte{0});
    signed_low_state[0] = std::byte{1};
    signed_low_state[6] = std::byte{0x81};
    auto& state_one = tower.post_elevator.dc24_records[4];
    state_one.fill(std::byte{0});
    state_one[0] = std::byte{1};
    state_one[6] = std::byte{1};
    service[6] = std::byte{2};
    simtower::advance_original_entertainment_show(tower, 0U, 1U);
    assert(service[6] == std::byte{3});
    assert(negative_floor[6] == std::byte{2});
    assert(other_group[6] == std::byte{2});
    assert(signed_low_state[6] == std::byte{0x81});
    assert(state_one[6] == std::byte{1});
    for (auto* sentinel :
         {&negative_floor, &other_group, &signed_low_state, &state_one}) {
      (*sentinel)[0] = std::byte{0xfe};
    }

    simtower::OriginalPartTable part{};
    part.words_52_to_ac[16] = 20U;
    part.words_52_to_ac[17] = 40U;
    part.words_52_to_ac[18] = 60U;
    part.words_52_to_ac[19] = 100U;
    part.words_52_to_ac[20] = 200U;
    part.words_52_to_ac[21] = 300U;
    part.words_52_to_ac[22] = 400U;
    tower.header.balance = 1'000;
    tower.header.current_day = 0;
    tower.header.frame_time = 1900U;  // day phase four
    tower.people[56].exact_bytes[5] = std::byte{3};
    tower.people[57].exact_bytes[5] = std::byte{3};
    service[10] = std::byte{2};
    service[11] = std::byte{50};
    const auto movie_income =
        simtower::finish_original_entertainment_phase(tower, part, 1U, 1U);
    assert(movie_income.codes == std::vector<std::uint8_t>{7U});
    assert(service[6] == std::byte{0});
    assert(service[10] == std::byte{0});
    assert(tower.people[56].exact_bytes[5] == std::byte{5});
    assert(tower.people[57].exact_bytes[5] == std::byte{5});
    assert(tower.header.balance == 1'300);
    assert(tower.post_elevator.finance.population_by_category[8] == 0);
    assert(tower.post_elevator.finance.income_by_category[8] == 300);
    assert(tower.post_elevator.finance.total_income == 300);

    // The 60/84-day event sentinels suppress the income branch without
    // suppressing the state/dirty transitions around it.
    tower.header.current_day = 59;
    service[11] = std::byte{70};
    const auto event_day_income =
        simtower::finish_original_entertainment_phase(tower, part, 1U, 1U);
    assert(event_day_income.codes.empty());
    assert(tower.header.balance == 1'300);
    assert(tower.post_elevator.finance.total_income == 300);
  }

  {
    // Party Hall is the fourth 1178:126c table entry. A nonzero attendance
    // flag emits income status eight; a zero flag skips both income and text.
    auto tower = simtower::make_original_new_tdt();
    auto& party = tower.post_elevator.dc24_records[0];
    party.fill(std::byte{0});
    party[0] = std::byte{10};
    party[7] = std::byte{0xff};
    party[11] = std::byte{1};
    const auto income = simtower::finish_original_entertainment_phase(
        tower, simtower::OriginalPartTable{}, 1U, 0U);
    assert(income.codes == std::vector<std::uint8_t>{8U});
    assert(tower.header.balance == 20'200);

    party[11] = std::byte{0};
    const auto no_income = simtower::finish_original_entertainment_phase(
        tower, simtower::OriginalPartTable{}, 1U, 0U);
    assert(no_income.codes.empty());
    assert(tower.header.balance == 20'200);
  }

  {
    // Complete scheduled 11a8:0184 commercial family. These records exercise all
    // three route blocks, the orphan cleanup/count decrement, signed lane
    // caps/minimums, population rebuilds, and close-time PART revenue.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[15] = 20U;  // Fast Food lane-three cap
    part.words_00_to_40[28] = 30U;  // Retail lane-three cap
    part.words_00_to_40[24] = 30U;  // Restaurant lane-five cap
    part.words_00_to_40[13] = 10U;
    part.words_00_to_40[12] = 20U;
    part.words_00_to_40[11] = 30U;
    part.words_52_to_ac[15] = 100U;
    part.words_52_to_ac[14] = 200U;
    part.words_52_to_ac[13] = 300U;
    part.words_52_to_ac[12] = 400U;
    part.words_00_to_40[20] = 10U;
    part.words_00_to_40[19] = 20U;
    part.words_00_to_40[18] = 30U;
    part.words_52_to_ac[11] = 50U;
    part.words_52_to_ac[10] = 60U;
    part.words_52_to_ac[9] = 70U;
    part.words_52_to_ac[8] = 80U;

    // Direct 11a8:17eb/174e/16ac coverage: the weekday/weekend/version-20
    // lane chooses signed capacity words, while signed attendance thresholds
    // select four Restaurant/Fast Food revenue bands and Retail returns zero.
    part.words_00_to_40[16] = 21U;
    part.words_00_to_40[17] = 22U;
    part.words_00_to_40[22] = 28U;
    part.words_00_to_40[23] = 29U;
    part.words_00_to_40[29] = 31U;
    part.words_00_to_40[30] = 32U;
    auto selector = simtower::make_original_new_tdt();
    selector.header.current_day = 0;
    assert(simtower::original_commercial_lane(selector) == 3U);
    assert(simtower::original_commercial_capacity(selector, part, 12U) == 20);
    assert(simtower::original_commercial_capacity(selector, part, 6U) == 28);
    assert(simtower::original_commercial_capacity(selector, part, 10U) == 30);
    selector.header.current_day = 2;
    assert(simtower::original_commercial_lane(selector) == 4U);
    assert(simtower::original_commercial_capacity(selector, part, 12U) == 21);
    assert(simtower::original_commercial_capacity(selector, part, 6U) == 29);
    assert(simtower::original_commercial_capacity(selector, part, 10U) == 31);
    selector.header.version_20_word = 1U;
    assert(simtower::original_commercial_lane(selector) == 5U);
    assert(simtower::original_commercial_capacity(selector, part, 12U) == 22);
    assert(simtower::original_commercial_capacity(selector, part, 6U) == 30);
    assert(simtower::original_commercial_capacity(selector, part, 10U) == 32);
    assert(simtower::original_commercial_capacity(selector, part, 9U) == 0);
    assert(simtower::original_commercial_revenue(part, 12U, 9) == 100);
    assert(simtower::original_commercial_revenue(part, 12U, 10) == 200);
    assert(simtower::original_commercial_revenue(part, 12U, 20) == 300);
    assert(simtower::original_commercial_revenue(part, 12U, 30) == 400);
    assert(simtower::original_commercial_revenue(part, 6U, 9) == 50);
    assert(simtower::original_commercial_revenue(part, 6U, 10) == 60);
    assert(simtower::original_commercial_revenue(part, 6U, 20) == 70);
    assert(simtower::original_commercial_revenue(part, 6U, 30) == 80);
    assert(simtower::original_commercial_revenue(part, 10U, 30) == 0);

    auto tower = simtower::make_original_new_tdt();
    const auto make_commercial_tenant = [](std::int8_t type,
                                           std::uint8_t key) {
      simtower::OriginalTdtTenant tenant{};
      tenant.type = type;
      tenant.exact_bytes[4] = static_cast<std::byte>(type);
      tenant.exact_bytes[12] = static_cast<std::byte>(key);
      std::copy(tenant.exact_bytes.begin() + 7,
                tenant.exact_bytes.begin() + 16,
                tenant.preserved_07_to_0f.begin());
      return tenant;
    };
    tower.floors[10].tenants = {make_commercial_tenant(12, 0U)};
    tower.floors[20].tenants = {make_commercial_tenant(10, 0U)};
    tower.floors[11].tenants = {make_commercial_tenant(6, 0U)};
    tower.floors[10].tenant_index[0] = 0U;
    tower.floors[20].tenant_index[0] = 0U;
    tower.floors[11].tenant_index[0] = 0U;

    auto& fast_food = tower.retail[0].exact_bytes;
    fast_food[0] = std::byte{10};
    fast_food[1] = std::byte{0};
    fast_food[2] = std::byte{3};
    fast_food[3] = std::byte{25};
    fast_food[7] = std::byte{7};
    auto& retail = tower.retail[1].exact_bytes;
    retail[0] = std::byte{20};
    retail[1] = std::byte{0};
    retail[2] = std::byte{3};
    retail[3] = std::byte{5};
    retail[7] = std::byte{4};
    auto& restaurant = tower.retail[2].exact_bytes;
    restaurant[0] = std::byte{11};
    restaurant[1] = std::byte{0};
    restaurant[2] = std::byte{3};
    restaurant[5] = std::byte{40};
    restaurant[7] = std::byte{6};
    auto& orphan = tower.retail[3].exact_bytes;
    orphan[0] = std::byte{9};
    orphan[1] = std::byte{0xff};
    tower.header.exact_bytes[46] = std::byte{4};  // DS:b3f8
    tower.header.exact_bytes[47] = std::byte{0};
    tower.post_elevator.dynamic_dd5c.fill(std::byte{0xaa});
    tower.post_elevator.dynamic_dd60.fill(std::byte{0xaa});
    tower.post_elevator.dynamic_dd64.fill(std::byte{0xaa});
    auto& finance = tower.post_elevator.finance;
    finance.population_by_category[5] = 100;
    finance.population_by_category[4] = 200;
    finance.population_by_category[6] = 10;
    finance.total_population = 310;
    tower.header.current_day = 0;
    tower.header.version_20_word = 0U;  // lane three

    // Direct 11a8:02f2 coverage: reset the complete commercial record,
    // preserve its facility linkage, rebuild lane/population bookkeeping, and
    // retain the original byte-swapped financial fields.
    simtower::reset_original_commercial_for_day(tower, part);
    assert(orphan[0] == std::byte{0xff});
    assert(tower.header.exact_bytes[46] == std::byte{3});
    assert(fast_food[2] == std::byte{0});
    assert(fast_food[6] == std::byte{20});
    assert(fast_food[8] == std::byte{7});
    assert(fast_food[3] == std::byte{0});
    assert(fast_food[7] == std::byte{0});
    assert(fast_food[9] == std::byte{0});
    assert(fast_food[12] == std::byte{0xeb} &&
           fast_food[13] == std::byte{0xff});
    assert(retail[6] == std::byte{10});
    assert(retail[8] == std::byte{4});
    assert(finance.population_by_category[5] == 7);
    assert(finance.population_by_category[4] == 4);
    assert(finance.population_by_category[6] == 10);
    assert(finance.total_population == 21);
    assert(tower.floors[10].tenants[0].exact_bytes[13] == std::byte{1});
    assert(tower.floors[20].tenants[0].exact_bytes[13] == std::byte{1});
    assert(tower.post_elevator.dynamic_dd64[0] == std::byte{1});
    assert(tower.post_elevator.dynamic_dd64[1] == std::byte{0});
    assert(tower.post_elevator.dynamic_dd64[2] == std::byte{0});
    assert(tower.post_elevator.dynamic_dd64[3] == std::byte{0});
    constexpr std::size_t retail_group_one = 0x26eU;
    assert(tower.post_elevator.dynamic_dd5c[retail_group_one] ==
           std::byte{1});
    assert(tower.post_elevator.dynamic_dd5c[retail_group_one + 2U] ==
           std::byte{1});
    assert(std::ranges::all_of(
        tower.post_elevator.dynamic_dd60,
        [](std::byte value) { return value == std::byte{0}; }));

    // Direct 11a8:0250 coverage: scan the complete commercial bank, select
    // only linked type-six Restaurant tenants, and perform the 02f2 reset.
    tower.header.version_20_word = 1U;  // lane five
    simtower::reset_original_restaurants_for_evening(tower, part);
    assert(restaurant[2] == std::byte{0});
    assert(restaurant[6] == std::byte{30});
    assert(restaurant[8] == std::byte{6});
    assert(restaurant[5] == std::byte{0});
    assert(restaurant[12] == std::byte{0xe1} &&
           restaurant[13] == std::byte{0xff});
    assert(finance.population_by_category[6] == 6);
    assert(finance.total_population == 17);
    assert(tower.post_elevator.dynamic_dd60[0] == std::byte{1});
    assert(tower.post_elevator.dynamic_dd60[2] == std::byte{2});

    fast_food[16] = std::byte{25};
    fast_food[17] = std::byte{0};
    restaurant[16] = std::byte{35};
    restaurant[17] = std::byte{0};
    tower.header.balance = 1'000;
    tower.header.current_day = 0;
    // Direct 11a8:0554/0603 coverage: the paired 512-record close scans split
    // live non-Restaurant and Restaurant tenants while skipping the Retail
    // no-revenue case and applying 11a8:06b2 to each matching record.
    const auto fast_food_income =
        simtower::close_original_nonrestaurant_commercial(tower, part);
    assert(fast_food_income.codes == std::vector<std::uint8_t>{5U});
    assert(fast_food[2] == std::byte{3});
    assert(fast_food[10] == std::byte{0x2c});  // low byte of 300
    assert(retail[2] == std::byte{3} && retail[10] == std::byte{0});
    assert(tower.header.balance == 1'300);
    assert(finance.income_by_category[5] == 300);
    const auto restaurant_income =
        simtower::close_original_restaurants_for_night(tower, part);
    assert(restaurant_income.codes == std::vector<std::uint8_t>{4U});
    assert(restaurant[2] == std::byte{3});
    assert(restaurant[10] == std::byte{80});
    assert(tower.header.balance == 1'380);
    assert(finance.income_by_category[6] == 80);
    assert(finance.total_income == 380);

    // Direct 11a8:06b2 coverage. Event days suppress 1178:126c income, while
    // 06b2 still closes and
    // refreshes the exact service-result byte.
    tower.header.current_day = 59;
    fast_food[2] = std::byte{0};
    restaurant[2] = std::byte{0};
    const auto suppressed_fast_food =
        simtower::close_original_nonrestaurant_commercial(tower, part);
    const auto suppressed_restaurant =
        simtower::close_original_restaurants_for_night(tower, part);
    assert(suppressed_fast_food.codes.empty() &&
           suppressed_restaurant.codes.empty());
    assert(tower.header.balance == 1'380);
    assert(finance.total_income == 380);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.retail[0].exact_bytes[10] == std::byte{0x2c});
    assert(reparsed.retail[2].exact_bytes[10] == std::byte{80});
    assert(reparsed.post_elevator.dynamic_dd60[2] == std::byte{2});
    assert(reparsed.post_elevator.finance.total_income == 380);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);

    // Opposite-endian records retain the original word byte order for both
    // the negative arrival counter and close-time attendance selector.
    auto swapped = simtower::make_original_new_tdt();
    swapped.header.byte_swapped = true;
    swapped.floors[10].tenants = {make_commercial_tenant(12, 0U)};
    swapped.floors[10].tenant_index[0] = 0U;
    auto& swapped_record = swapped.retail[0].exact_bytes;
    swapped_record[0] = std::byte{10};
    swapped_record[1] = std::byte{0};
    swapped_record[2] = std::byte{3};
    swapped_record[3] = std::byte{20};
    swapped_record[7] = std::byte{1};
    simtower::reset_original_commercial_for_day(swapped, part);
    assert(swapped_record[12] == std::byte{0xff} &&
           swapped_record[13] == std::byte{0xeb});
    assert(swapped.post_elevator.dynamic_dd64[0] == std::byte{0} &&
           swapped.post_elevator.dynamic_dd64[1] == std::byte{1});
    swapped_record[16] = std::byte{0};
    swapped_record[17] = std::byte{25};
    const auto swapped_income =
        simtower::close_original_nonrestaurant_commercial(swapped, part);
    assert(swapped_income.codes == std::vector<std::uint8_t>{5U});
    assert(swapped.header.balance == 20'300);
  }

  {
    // Complete 1130:0000 pass with direct 1130:06e9 coverage across person,
    // Restaurant/Fast Food/Retail, and entertainment-backed satisfaction
    // branches on a non-three-day boundary.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[5] = 50U;
    part.words_00_to_40[8] = 100U;
    part.words_00_to_40[20] = 10U;
    part.words_00_to_40[19] = 20U;
    part.words_00_to_40[18] = 30U;
    part.words_00_to_40[13] = 10U;
    part.words_00_to_40[12] = 20U;
    part.words_00_to_40[11] = 30U;
    part.words_00_to_40[25] = 20U;
    part.words_00_to_40[26] = 40U;
    part.words_52_to_ac[27] = 40U;

    auto tower = simtower::make_original_new_tdt();
    tower.header.current_day = 1;
    tower.header.rating = 1U;
    tower.people.resize(160U);
    tower.people_count = 160U;
    const auto store_word = [](auto& exact, std::size_t offset,
                               std::uint16_t value) {
      exact[offset] = static_cast<std::byte>(value);
      exact[offset + 1U] = static_cast<std::byte>(value >> 8U);
    };
    const auto make_tenant = [&](std::int8_t type, std::uint16_t left,
                                 std::uint16_t right, std::uint32_t people,
                                 std::uint16_t service) {
      simtower::OriginalTdtTenant tenant{};
      tenant.left = left;
      tenant.right = right;
      tenant.type = type;
      tenant.rent_rate = 1U;
      store_word(tenant.exact_bytes, 0U, left);
      store_word(tenant.exact_bytes, 2U, right);
      tenant.exact_bytes[4] = static_cast<std::byte>(type);
      store_word(tenant.exact_bytes, 6U, service);
      tenant.exact_bytes[8] = static_cast<std::byte>(people);
      tenant.exact_bytes[9] = static_cast<std::byte>(people >> 8U);
      tenant.exact_bytes[10] = static_cast<std::byte>(people >> 16U);
      tenant.exact_bytes[11] = static_cast<std::byte>(people >> 24U);
      tenant.exact_bytes[16] = std::byte{1};
      std::copy(tenant.exact_bytes.begin() + 7,
                tenant.exact_bytes.begin() + 16,
                tenant.preserved_07_to_0f.begin());
      return tenant;
    };

    auto& floor = tower.floors[10];
    floor.tenants = {
        make_tenant(7, 0U, 10U, 100U, 0U),
        make_tenant(6, 12U, 20U, 0U, 0U),
        make_tenant(12, 22U, 30U, 0U, 1U),
        make_tenant(10, 32U, 40U, 0U, 2U),
        make_tenant(18, 42U, 50U, 0U, 0U),
        make_tenant(14, 52U, 60U, 0U, 0U)};
    for (std::size_t index = 0; index < floor.tenants.size(); ++index) {
      floor.tenants[index].exact_bytes[12] = static_cast<std::byte>(index);
      floor.tenants[index].preserved_07_to_0f[5] =
          static_cast<std::byte>(index);
      floor.tenant_index[index] = static_cast<std::uint16_t>(index);
    }
    for (std::size_t index = 100U; index < 106U; ++index) {
      tower.people[index].exact_bytes[9] = std::byte{2};
      store_word(tower.people[index].exact_bytes, 14U, 120U);
    }
    auto& restaurant = tower.retail[0].exact_bytes;
    restaurant[16] = std::byte{25};
    auto& fast_food = tower.retail[1].exact_bytes;
    fast_food[16] = std::byte{5};
    auto& retail = tower.retail[2].exact_bytes;
    retail[6] = std::byte{10};
    retail[16] = std::byte{15};
    auto& movie = tower.post_elevator.dc24_records[0];
    movie[7] = std::byte{0};
    movie[9] = std::byte{0};

    simtower::advance_original_tenants_at_midnight(
        tower, part, simtower::OriginalYenTable{});
    assert(floor.tenants[0].exact_bytes[15] == std::byte{0});
    assert(floor.tenants[1].exact_bytes[15] == std::byte{2});
    assert(floor.tenants[2].exact_bytes[15] == std::byte{0});
    assert(floor.tenants[3].exact_bytes[15] == std::byte{2});
    assert(floor.tenants[4].exact_bytes[15] == std::byte{1});
    assert(floor.tenants[5].exact_bytes[15] == std::byte{0xff});
    assert(floor.tenants[0].exact_bytes[14] == std::byte{0});
    assert(floor.tenants[1].exact_bytes[14] == std::byte{1});
    assert(floor.tenants[2].exact_bytes[14] == std::byte{0});
    assert(floor.tenants[3].exact_bytes[14] == std::byte{1});
    assert(floor.tenants[4].exact_bytes[14] == std::byte{1});
    assert(floor.tenants[5].exact_bytes[14] == std::byte{1});
  }

  {
    // 11d0:0000 invokes the exact 1130:00b5 all-floor satisfaction pass
    // whenever Map overlay one is selected. Even on a three-day boundary it
    // must not run 1130:09e5/0b92 departures, income, or age transitions.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[5] = 50U;
    part.words_00_to_40[8] = 100U;
    auto tower = simtower::make_original_new_tdt();
    tower.header.current_day = 3;
    tower.header.rating = 1U;
    tower.header.balance = 12'345;
    tower.people.resize(20U);
    tower.people_count = 20U;

    const auto store_word = [](auto& exact, std::size_t offset,
                               std::uint16_t value) {
      exact[offset] = static_cast<std::byte>(value);
      exact[offset + 1U] = static_cast<std::byte>(value >> 8U);
    };
    const auto set_person_scores = [&](std::size_t first,
                                       std::uint16_t score) {
      for (std::size_t index = first; index < first + 6U; ++index) {
        tower.people[index].exact_bytes[9] = std::byte{1};
        store_word(tower.people[index].exact_bytes, 14U, score);
      }
    };
    const auto make_office = [](std::uint16_t left,
                                std::uint32_t people_start) {
      simtower::OriginalTdtTenant tenant{};
      tenant.left = left;
      tenant.right = static_cast<std::uint16_t>(left + 8U);
      tenant.type = 7;
      tenant.status = 0U;
      tenant.rent_rate = 1U;
      tenant.subtype = 7U;
      tenant.exact_bytes[4] = std::byte{7};
      tenant.exact_bytes[8] = static_cast<std::byte>(people_start);
      tenant.exact_bytes[9] = static_cast<std::byte>(people_start >> 8U);
      tenant.exact_bytes[10] = static_cast<std::byte>(people_start >> 16U);
      tenant.exact_bytes[11] = static_cast<std::byte>(people_start >> 24U);
      tenant.exact_bytes[15] = std::byte{0xff};
      tenant.exact_bytes[16] = std::byte{1};
      return tenant;
    };

    set_person_scores(0U, 25U);
    set_person_scores(10U, 125U);
    tower.floors[20].tenants.push_back(make_office(100U, 0U));
    tower.floors[80].tenants.push_back(make_office(200U, 10U));

    simtower::refresh_original_map_tenant_satisfaction(tower, part);
    assert(tower.floors[20].tenants[0].exact_bytes[15] == std::byte{2});
    assert(tower.floors[20].tenants[0].exact_bytes[14] == std::byte{1});
    assert(tower.floors[80].tenants[0].exact_bytes[15] == std::byte{0});
    assert(tower.floors[80].tenants[0].exact_bytes[14] == std::byte{0});
    assert(tower.floors[20].tenants[0].status == 0U);
    assert(tower.floors[80].tenants[0].status == 0U);
    assert(tower.floors[20].tenants[0].subtype == 7U);
    assert(tower.floors[80].tenants[0].subtype == 7U);
    assert(tower.header.balance == 12'345);
  }

  {
    // Three-day 1130:09e5/0b92 path: grade-zero tenants depart and pair with
    // grade-two peers; surviving Office and Retail tenants age/pay rent.
    // Direct 1178:0d3f/1086/11da, 1130:0cec, and 11d8:03c4 coverage verifies Office,
    // Condo, and Retail status, dirty, age, population and rent departures
    // plus every record in
    // their six-person
    // Office, three-person Condo, and 48-person PART-sized Retail resets.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[5] = 100U;
    part.words_00_to_40[8] = 200U;
    part.words_00_to_40[25] = 20U;
    part.words_00_to_40[26] = 40U;
    simtower::OriginalYenTable rent{};
    rent[29] = 100U;  // Office type 7, tier 1
    rent[37] = 200U;  // Condo type 9, tier 1
    rent[41] = 300U;  // Retail type 10, tier 1

    auto tower = simtower::make_original_new_tdt();
    tower.header.current_day = 3;
    tower.header.frame_time = 1000U;
    tower.header.rating = 1U;
    tower.header.balance = 1'000;
    tower.people.resize(150U);
    tower.people_count = 150U;
    const auto store_word = [](auto& exact, std::size_t offset,
                               std::uint16_t value) {
      exact[offset] = static_cast<std::byte>(value);
      exact[offset + 1U] = static_cast<std::byte>(value >> 8U);
    };
    const auto make_tenant = [&](std::int8_t type, std::uint16_t left,
                                 std::uint16_t right, std::uint32_t people,
                                 std::uint16_t service) {
      simtower::OriginalTdtTenant tenant{};
      tenant.left = left;
      tenant.right = right;
      tenant.type = type;
      tenant.status = 0U;
      tenant.rent_rate = 1U;
      tenant.subtype = 1U;
      store_word(tenant.exact_bytes, 0U, left);
      store_word(tenant.exact_bytes, 2U, right);
      tenant.exact_bytes[4] = static_cast<std::byte>(type);
      store_word(tenant.exact_bytes, 6U, service);
      tenant.exact_bytes[8] = static_cast<std::byte>(people);
      tenant.exact_bytes[9] = static_cast<std::byte>(people >> 8U);
      tenant.exact_bytes[10] = static_cast<std::byte>(people >> 16U);
      tenant.exact_bytes[11] = static_cast<std::byte>(people >> 24U);
      tenant.exact_bytes[16] = std::byte{1};
      tenant.exact_bytes[17] = std::byte{1};
      std::copy(tenant.exact_bytes.begin() + 7,
                tenant.exact_bytes.begin() + 16,
                tenant.preserved_07_to_0f.begin());
      return tenant;
    };
    auto& floor = tower.floors[30];
    floor.tenants = {
        make_tenant(7, 0U, 8U, 0U, 0U),
        make_tenant(7, 20U, 28U, 10U, 0U),
        make_tenant(9, 50U, 58U, 20U, 0U),
        make_tenant(9, 70U, 78U, 30U, 0U),
        make_tenant(10, 100U, 108U, 40U, 0U),
        make_tenant(10, 120U, 128U, 90U, 1U)};
    for (std::size_t index = 0; index < floor.tenants.size(); ++index) {
      floor.tenants[index].exact_bytes[12] = static_cast<std::byte>(index);
      floor.tenants[index].preserved_07_to_0f[5] =
          static_cast<std::byte>(index);
      floor.tenant_index[index] = static_cast<std::uint16_t>(index);
    }
    const auto set_people = [&](std::size_t first, std::size_t count,
                                std::uint16_t score) {
      for (std::size_t index = first; index < first + count; ++index) {
        tower.people[index].exact_bytes[9] = std::byte{1};
        store_word(tower.people[index].exact_bytes, 14U, score);
      }
    };
    set_people(0U, 6U, 250U);
    set_people(10U, 6U, 0U);
    set_people(20U, 3U, 200U);
    set_people(30U, 3U, 0U);
    set_people(40U, 48U, 55U);
    set_people(90U, 48U, 55U);
    tower.retail[0].exact_bytes[2] = std::byte{0};
    tower.retail[0].exact_bytes[6] = std::byte{50};
    tower.retail[1].exact_bytes[2] = std::byte{0};
    tower.retail[1].exact_bytes[6] = std::byte{30};
    auto& finance = tower.post_elevator.finance;
    finance.population_by_category[0] = 12;
    finance.population_by_category[9] = 6;
    finance.population_by_category[4] = 20;
    finance.total_population = 38;

    simtower::advance_original_tenants_at_midnight(tower, part, rent);
    assert(floor.tenants[0].status == 0x10U);
    assert(floor.tenants[0].exact_bytes[15] == std::byte{1});
    assert(floor.tenants[1].exact_bytes[15] == std::byte{1});
    assert(floor.tenants[1].subtype == 2U);
    assert(floor.tenants[2].status == 0x18U);
    assert(floor.tenants[2].exact_bytes[15] == std::byte{1});
    assert(floor.tenants[3].exact_bytes[15] == std::byte{1});
    assert(floor.tenants[3].subtype == 2U);
    assert(tower.retail[0].exact_bytes[2] == std::byte{0xff});
    assert(floor.tenants[4].subtype == 0U);
    assert(floor.tenants[4].exact_bytes[15] == std::byte{1});
    assert(floor.tenants[5].subtype == 2U);
    assert(tower.header.balance == 1'200);
    assert(finance.income_by_category[0] == 100);
    assert(finance.income_by_category[9] == -200);
    assert(finance.income_by_category[4] == 300);
    assert(finance.total_income == 200);
    assert(finance.population_by_category[0] == 6);
    assert(finance.population_by_category[9] == 3);
    assert(finance.population_by_category[4] == 20);
    assert(finance.total_population == 29);
    for (const auto range :
         std::array<std::pair<std::size_t, std::size_t>, 6>{
             {{0U, 6U}, {10U, 6U}, {20U, 3U}, {30U, 3U},
              {40U, 48U}, {90U, 48U}}}) {
      for (std::size_t index = range.first;
           index < range.first + range.second; ++index) {
        assert(tower.people[index].exact_bytes[9] == std::byte{0});
        assert(tower.people[index].exact_bytes[14] == std::byte{0});
        assert(tower.people[index].exact_bytes[15] == std::byte{0});
      }
    }

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.floors[30].tenants[0].status == 0x10U);
    assert(reparsed.floors[30].tenants[5].subtype == 2U);
    assert(reparsed.retail[0].exact_bytes[2] == std::byte{0xff});
    assert(reparsed.post_elevator.finance.total_income == 200);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Ordered 1130:0109 Hotel loops: Direct 1130:0f57 grade-zero/two pairing,
    // direct 1130:0e5c checkout flag/age/status transition, and direct
    // 1130:0cec owned-metric clearing. Types 3/4/5 skip owner ordinal zero and
    // clear respectively one/two/two following guest metric records.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[5] = 100U;
    part.words_00_to_40[8] = 200U;
    auto tower = simtower::make_original_new_tdt();
    tower.header.rating = 1U;
    tower.header.frame_time = 1000U;
    tower.people.resize(50U);
    tower.people_count = 50U;
    const auto store_word = [](auto& exact, std::size_t offset,
                               std::uint16_t value) {
      exact[offset] = static_cast<std::byte>(value);
      exact[offset + 1U] = static_cast<std::byte>(value >> 8U);
    };
    const auto make_hotel = [&](std::int8_t type, std::uint16_t left,
                                std::uint32_t people, std::uint8_t status,
                                std::uint8_t flag, std::uint8_t age) {
      simtower::OriginalTdtTenant tenant{};
      tenant.left = left;
      tenant.right = static_cast<std::uint16_t>(left + 8U);
      tenant.type = type;
      tenant.status = status;
      tenant.rent_rate = 1U;
      tenant.subtype = age;
      store_word(tenant.exact_bytes, 0U, tenant.left);
      store_word(tenant.exact_bytes, 2U, tenant.right);
      tenant.exact_bytes[4] = static_cast<std::byte>(type);
      tenant.exact_bytes[5] = static_cast<std::byte>(status);
      tenant.exact_bytes[8] = static_cast<std::byte>(people);
      tenant.exact_bytes[9] = static_cast<std::byte>(people >> 8U);
      tenant.exact_bytes[10] = static_cast<std::byte>(people >> 16U);
      tenant.exact_bytes[11] = static_cast<std::byte>(people >> 24U);
      tenant.exact_bytes[14] = static_cast<std::byte>(flag);
      tenant.exact_bytes[16] = std::byte{1};
      tenant.exact_bytes[17] = static_cast<std::byte>(age);
      std::copy(tenant.exact_bytes.begin() + 7,
                tenant.exact_bytes.begin() + 16,
                tenant.preserved_07_to_0f.begin());
      return tenant;
    };
    auto& floor = tower.floors[40];
    floor.tenants = {
        make_hotel(3, 0U, 0U, 0U, 0U, 0U),
        make_hotel(3, 20U, 10U, 0U, 0U, 0U),
        make_hotel(4, 40U, 20U, 0x28U, 0U, 2U),
        make_hotel(5, 60U, 30U, 0x28U, 1U, 2U),
        make_hotel(4, 80U, 34U, 0U, 0U, 0U),
        make_hotel(4, 100U, 37U, 0U, 0U, 0U),
        make_hotel(5, 120U, 40U, 0U, 0U, 0U),
        make_hotel(5, 140U, 43U, 0U, 0U, 0U)};
    for (std::size_t index = 0; index < floor.tenants.size(); ++index) {
      floor.tenants[index].exact_bytes[12] = static_cast<std::byte>(index);
      floor.tenants[index].preserved_07_to_0f[5] =
          static_cast<std::byte>(index);
      floor.tenant_index[index] = static_cast<std::uint16_t>(index);
    }
    tower.people[1].exact_bytes[9] = std::byte{1};
    store_word(tower.people[1].exact_bytes, 14U, 250U);
    tower.people[11].exact_bytes[9] = std::byte{1};
    store_word(tower.people[11].exact_bytes, 14U, 0U);
    for (const auto index : {21U, 22U, 31U, 32U}) {
      tower.people[index].exact_bytes[9] = std::byte{1};
      store_word(tower.people[index].exact_bytes, 14U, 250U);
    }
    for (const auto index : {35U, 36U, 41U, 42U}) {
      tower.people[index].exact_bytes[9] = std::byte{1};
      store_word(tower.people[index].exact_bytes, 14U, 250U);
    }
    for (const auto index : {38U, 39U, 44U, 45U}) {
      tower.people[index].exact_bytes[9] = std::byte{1};
      store_word(tower.people[index].exact_bytes, 14U, 0U);
    }
    for (const auto owner_index : {0U, 10U, 34U, 37U, 40U, 43U}) {
      tower.people[owner_index].exact_bytes[9] = std::byte{0x7e};
      store_word(tower.people[owner_index].exact_bytes, 14U, 0x1234U);
    }

    simtower::advance_original_hotels_for_evening(tower, part);
    assert(floor.tenants[0].exact_bytes[15] == std::byte{1});
    assert(floor.tenants[1].exact_bytes[15] == std::byte{1});
    assert(floor.tenants[0].exact_bytes[14] == std::byte{1});
    assert(tower.people[1].exact_bytes[9] == std::byte{0});
    assert(tower.people[11].exact_bytes[9] == std::byte{0});
    assert(floor.tenants[2].status == 0x38U);
    assert(floor.tenants[2].subtype == 3U);
    assert(floor.tenants[2].exact_bytes[13] == std::byte{1});
    assert(floor.tenants[3].status == 0x28U);
    assert(floor.tenants[3].subtype == 0U);
    assert(floor.tenants[3].exact_bytes[14] == std::byte{0});
    assert(floor.tenants[3].exact_bytes[15] == std::byte{0});
    for (const auto tenant_index : {4U, 5U, 6U, 7U}) {
      assert(floor.tenants[tenant_index].exact_bytes[14] == std::byte{1});
      assert(floor.tenants[tenant_index].exact_bytes[15] == std::byte{1});
    }
    for (const auto index : {1U, 11U, 35U, 36U, 38U, 39U,
                             41U, 42U, 44U, 45U}) {
      assert(tower.people[index].exact_bytes[9] == std::byte{0});
      assert(tower.people[index].exact_bytes[14] == std::byte{0});
      assert(tower.people[index].exact_bytes[15] == std::byte{0});
    }
    for (const auto owner_index : {0U, 10U, 34U, 37U, 40U, 43U}) {
      assert(tower.people[owner_index].exact_bytes[9] == std::byte{0x7e});
      assert(tower.people[owner_index].exact_bytes[14] == std::byte{0x34});
      assert(tower.people[owner_index].exact_bytes[15] == std::byte{0x12});
    }
  }

  {
    // Direct 1178:0b44/09ee coverage across ordinary/Lobby tenants, all three
    // Elevator families, and both Stair/Escalator maintenance selectors.
    // Direct 1178:0a6a coverage includes its 16-bit endpoint subtraction,
    // signed width/rate multiplication, signed IDIV-by-ten, and type-24
    // accounting path.
    simtower::OriginalYenTable maintenance{};
    maintenance[1] = 100U;
    maintenance[14] = 200U;
    maintenance[22] = 0U;
    maintenance[24] = 0U;
    maintenance[27] = 50U;
    maintenance[42] = 200U;
    maintenance[43] = 100U;
    simtower::OriginalPartTable part{};
    part.words_52_to_ac[37] = 0U;
    part.words_52_to_ac[38] = 30U;
    part.words_52_to_ac[39] = 100U;

    auto tower = simtower::make_original_new_tdt();
    tower.header.balance = 10'000;
    tower.header.rating = 3U;
    tower.header.lobby_height = 2U;
    const auto add_tenant = [&](std::size_t floor, std::int8_t type,
                                std::uint16_t left,
                                std::uint16_t right) {
      simtower::OriginalTdtTenant tenant{};
      tenant.type = type;
      tenant.left = left;
      tenant.right = right;
      tenant.exact_bytes[4] = static_cast<std::byte>(type);
      tower.floors[floor].tenants.push_back(tenant);
    };
    add_tenant(10, 14, 100, 120);   // direct 200
    add_tenant(10, 24, 100, 120);   // 20 * 30 / 10 = 60
    add_tenant(9, 25, 100, 110);    // billed as type 24: 30
    add_tenant(11, 26, 100, 140);   // excluded upper Lobby story
    add_tenant(10, -14, 100, 120);  // pending record is excluded

    tower.elevators[0].used = 1U;
    tower.elevators[0].type = 0U;
    tower.elevators[0].cars = 2U;  // type 42: 400
    tower.elevators[0].bottom_floor = 1;
    tower.elevators[0].top_floor = 0;
    tower.elevators[1].used = 1U;
    tower.elevators[1].type = 1U;
    tower.elevators[1].cars = 3U;  // type 1: 300
    tower.elevators[1].bottom_floor = 1;
    tower.elevators[1].top_floor = 0;
    tower.elevators[2].used = 1U;
    tower.elevators[2].type = 2U;
    tower.elevators[2].cars = 4U;  // type 43: 400
    tower.elevators[2].bottom_floor = 1;
    tower.elevators[2].top_floor = 0;
    tower.post_elevator.stairs_bd70[0].used = 1U;
    tower.post_elevator.stairs_bd70[0].shape = 0U;  // type 27 * 1: 50
    tower.post_elevator.stairs_bd70[1].used = 1U;
    tower.post_elevator.stairs_bd70[1].shape = 2U;  // type 27 * 2: 100
    tower.post_elevator.stairs_bd70[2].used = 1U;
    tower.post_elevator.stairs_bd70[2].shape = 1U;  // type 22 * 1: 0

    simtower::charge_original_three_day_maintenance(tower, maintenance, part);
    const auto& finance = tower.post_elevator.finance;
    assert(tower.header.balance == 8'460);
    assert(finance.maintenance_by_category[0] == 90);
    assert(finance.maintenance_by_category[1] == 300);
    assert(finance.maintenance_by_category[2] == 400);
    assert(finance.maintenance_by_category[3] == 400);
    assert(finance.maintenance_by_category[4] == 150);
    assert(finance.maintenance_by_category[9] == 200);
    assert(finance.total_maintenance == 1'540);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.header.balance == 8'460);
    assert(reparsed.post_elevator.finance.total_maintenance == 1'540);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);

    // Direct 1178:097c/09ee debit coverage: these paths do not call 1178:1377;
    // even when
    // balance + charge exceeds 99,999,999, the complete charge is subtracted
    // and accounted rather than being reduced to the remaining headroom.
    tower = simtower::make_original_new_tdt();
    tower.header.balance = 99'999'990;
    tower.elevators[0].used = 1U;
    tower.elevators[0].type = 1U;
    tower.elevators[0].cars = 1U;
    simtower::charge_original_three_day_maintenance(tower, maintenance, part);
    assert(tower.header.balance == 99'999'890);
    assert(tower.post_elevator.finance.maintenance_by_category[1] == 100);
    assert(tower.post_elevator.finance.total_maintenance == 100);

    // 0-0x8000 wraps to signed -32768 before multiplication; IDIV truncates
    // -32768/10 toward zero to -3276, so the direct subtraction credits the
    // balance and records negative maintenance exactly as the Win16 path.
    tower = simtower::make_original_new_tdt();
    tower.header.balance = 1'000;
    tower.header.rating = 3U;
    part.words_52_to_ac[38] = 1U;
    add_tenant(10, 24, 0U, 0x8000U);
    simtower::charge_original_three_day_maintenance(tower, maintenance, part);
    assert(tower.header.balance == 4'276);
    assert(tower.post_elevator.finance.maintenance_by_category[0] == -3'276);
    assert(tower.post_elevator.finance.total_maintenance == -3'276);
  }

  {
    // Complete 10c8:006e bomb-event starter. The two MS C rand() values for
    // seed one select floor 17 and x 153 from the exact inclusive spans.
    simtower::OriginalPartTable part{};
    part.words_52_to_ac[5] = 2U;
    part.words_52_to_ac[40] = 2'000U;
    part.words_52_to_ac[41] = 3'000U;
    part.words_52_to_ac[42] = 10'000U;

    auto tower = make_event_test_tower();
    const auto offer = simtower::prepare_original_bomb_event(*tower, part);
    assert(offer.offered);
    assert(offer.floor == 17 && offer.x == 153 && offer.ransom == 3'000);
    assert(offer.dialog.dialog_id == 3020U);
    assert(offer.dialog.argument == -3'000);
    assert(offer.dialog.wave_resource == 10003);
    assert(tower->random_state == 2'156'045'615U);
    assert(test_header_word(*tower, 64U) == 17U);
    assert(test_header_word(*tower, 62U) == 153U);

    const auto search =
        simtower::resolve_original_bomb_event(*tower, part, 1U);
    assert(search.started && !search.paid);
    assert(search.direct_wave_resource == 0);
    assert(search.followup_dialog.dialog_id == 3022U);
    assert(search.followup_dialog.argument == 0);
    assert(search.followup_dialog.wave_resource == 10000);
    // The flag/deadline/Security tail is after the follow-up modal in
    // 10c8:0199-01b3, so preparation alone must leave it unapplied.
    assert(test_header_word(*tower, 60U) == 0U);
    assert(test_header_dword(*tower, 66U) == 0U);
    tower->security_event_accelerated = true;
    simtower::commit_original_bomb_event(*tower);
    assert(!tower->security_event_accelerated);
    assert(test_header_word(*tower, 60U) == 1U);
    assert(test_header_dword(*tower, 66U) == 1'200U);
    for (std::size_t index = 0; index < 6U; ++index) {
      const auto& exact = tower->people[index].exact_bytes;
      assert(exact[5] == std::byte{0});
      assert(exact[7] == static_cast<std::byte>(index < 3U ? 20U : 19U));
      assert(exact[8] == std::byte{0});
      for (const auto offset : {10U, 12U, 14U}) {
        assert(exact[offset] == std::byte{0});
        assert(exact[offset + 1U] == std::byte{0});
      }
    }
    const auto& security = tower->floors[20].tenants[0];
    assert(security.status == 19U);
    assert(security.variant == 20U);
    assert(security.preserved_07_to_0f[0] == std::byte{0});

    const auto bytes = simtower::serialize_original_tdt(*tower);
    const auto reparsed = std::make_unique<simtower::OriginalTdtDocument>(
        simtower::parse_original_tdt(bytes));
    assert(test_header_word(*reparsed, 60U) == 1U);
    assert(test_header_word(*reparsed, 62U) == 153U);
    assert(test_header_word(*reparsed, 64U) == 17U);
    assert(test_header_dword(*reparsed, 66U) == 1'200U);
    assert(simtower::serialize_original_tdt(*reparsed) == bytes);

    // Paying uses the rating-selected PART amount, plays the direct payment
    // sound, leaves b406/b40c inactive, and directly covers 1178:07e8's
    // wrapping balance plus non-construction-spending debit.
    tower = make_event_test_tower();
    assert(simtower::prepare_original_bomb_event(*tower, part).offered);
    const auto paid = simtower::resolve_original_bomb_event(*tower, part, 2U);
    assert(paid.paid && !paid.started);
    assert(paid.direct_wave_resource == 10015);
    assert(!paid.followup_dialog.valid());
    assert(tower->header.balance == 47'000);
    assert(tower->header.other_income == -2'000);
    assert(tower->header.construction_costs == 2'000);
    assert(test_header_word(*tower, 60U) == 0U);
    assert(test_header_dword(*tower, 66U) == 0U);

    // 10f8:033d with SECOM available: only ordinal zero searches; the
    // remaining five records are disabled and the Security facility tracks
    // the bomb floor. Bomb bit zero also has strict priority when Fire bit
    // three is supplied simultaneously.
    tower = make_event_test_tower();
    store_test_header_word(*tower, 32U, 5U);
    assert(simtower::prepare_original_bomb_event(*tower, part).offered);
    const auto secom =
        simtower::resolve_original_bomb_event(*tower, part, 1U);
    assert(secom.started && secom.followup_dialog.dialog_id == 3021U);
    assert(secom.followup_dialog.argument == 8);
    simtower::commit_original_bomb_event(*tower);
    assert(tower->people[0].exact_bytes[5] == std::byte{0});
    assert(tower->people[0].exact_bytes[7] == std::byte{20});
    for (std::size_t index = 1; index < 6U; ++index) {
      assert(tower->people[index].exact_bytes[5] == std::byte{1});
      assert(tower->people[index].exact_bytes[7] == std::byte{0});
    }
    assert(tower->floors[20].tenants[0].status == 16U);
    assert(tower->floors[20].tenants[0].variant == 17U);
    simtower::dispatch_original_security_response(*tower, 9U);
    assert(tower->people[0].exact_bytes[5] == std::byte{0});
    assert(tower->people[0].exact_bytes[7] == std::byte{20});
    for (std::size_t index = 1; index < 6U; ++index) {
      assert(tower->people[index].exact_bytes[5] == std::byte{1});
      assert(tower->people[index].exact_bytes[7] == std::byte{0});
    }

    // Leading gates are before rand(); the unsupported-rating switch is
    // after both coordinate selections and stores them despite no offer.
    tower = make_event_test_tower();
    store_test_header_word(*tower, 60U, 1U);
    assert(!simtower::prepare_original_bomb_event(*tower, part).offered);
    assert(tower->random_state == 1U);
    tower = make_event_test_tower();
    tower->header.rating = 1U;
    const auto unsupported =
        simtower::prepare_original_bomb_event(*tower, part);
    assert(!unsupported.offered);
    assert(unsupported.floor == 17 && unsupported.x == 153);
    assert(tower->random_state == 2'156'045'615U);
    assert(test_header_word(*tower, 64U) == 17U);
    assert(test_header_word(*tower, 62U) == 153U);

    // At b40c the active search becomes an explosion. Direct 10c8:0254
    // failure-followup coverage and direct 10c8:0000/02bd emit six
    // floors times forty x positions before Security is reset; two frames
    // later 0254 shows the failure dialog and forces the clock to 1500.
    tower = make_event_test_tower();
    assert(simtower::prepare_original_bomb_event(*tower, part).offered);
    assert(simtower::resolve_original_bomb_event(*tower, part, 1U).started);
    simtower::commit_original_bomb_event(*tower);
    tower->header.frame_time = 1'200U;
    auto bomb_tick = simtower::advance_original_bomb_event(*tower, part);
    assert(bomb_tick.changed && !bomb_tick.completed);
    assert(bomb_tick.sound_requests ==
           std::vector<simtower::OriginalEventSoundRequest>(
               {{10004, 0U, 3U, false}}));
    assert(bomb_tick.damage_requests.size() == 240U);
    assert(bomb_tick.damage_requests.front() ==
           simtower::OriginalFacilityDamageRequest({15, 133, 0U}));
    assert(bomb_tick.damage_requests.back() ==
           simtower::OriginalFacilityDamageRequest({20, 172, 0U}));
    assert(bomb_tick.security_dispatch_pending);
    assert(bomb_tick.security_dispatch_flags == 0U);
    assert(bomb_tick.focus_requested && bomb_tick.focus_floor == 17 &&
           bomb_tick.focus_x == 153);
    assert(test_header_word(*tower, 60U) == 0x41U);
    assert(test_header_dword(*tower, 66U) == 1'202U);
    simtower::dispatch_original_security_response(
        *tower, bomb_tick.security_dispatch_flags);
    for (std::size_t index = 0; index < 6U; ++index) {
      assert(tower->people[index].exact_bytes[5] == std::byte{1});
      assert(tower->people[index].exact_bytes[7] == std::byte{0});
    }
    tower->header.frame_time = 1'201U;
    assert(!simtower::advance_original_bomb_event(*tower, part).changed);
    tower->header.frame_time = 1'202U;
    bomb_tick = simtower::advance_original_bomb_event(*tower, part);
    assert(bomb_tick.changed && bomb_tick.completed);
    assert(bomb_tick.dialog.dialog_id == 3024U);
    assert(bomb_tick.dialog.argument == 8);
    assert(bomb_tick.dialog.wave_resource == 10000);
    assert(test_header_word(*tower, 60U) == 0U);
    assert(tower->header.frame_time == 1'202U);
    assert(bomb_tick.deferred_completion ==
           simtower::OriginalEventDeferredCompletion::bomb);
    simtower::complete_original_event_action(
        *tower, bomb_tick.deferred_completion);
    assert(tower->header.frame_time == 1'500U);

    // A Security person reaches the exact stored coordinate through
    // 10c8:01c4. The found outcome has no damage/sound and selects 3023.
    tower = make_event_test_tower();
    assert(simtower::prepare_original_bomb_event(*tower, part).offered);
    assert(simtower::resolve_original_bomb_event(*tower, part, 1U).started);
    simtower::commit_original_bomb_event(*tower);
    assert(!simtower::check_original_bomb_coordinate(
                *tower, part, 17, 152).changed);
    auto found = simtower::check_original_bomb_coordinate(
        *tower, part, 17, 153);
    assert(found.changed && found.damage_requests.empty() &&
           found.sound_requests.empty());
    assert(test_header_word(*tower, 60U) == 0x21U);
    assert(test_header_dword(*tower, 66U) == 102U);
    tower->header.frame_time = 102U;
    found = simtower::advance_original_bomb_event(*tower, part);
    assert(found.completed && found.dialog.dialog_id == 3023U);
    assert(found.dialog.argument == 8);
    assert(test_header_word(*tower, 60U) == 0U);
    simtower::complete_original_event_action(*tower,
                                            found.deferred_completion);
    assert(tower->header.frame_time == 1'500U);
  }

  {
    // Complete 10e8:0029 fire-event starter. Only the selected floor is
    // random; the x coordinate is exactly right_edge - 32.
    simtower::OriginalPartTable part{};
    part.words_52_to_ac[2] = 7U;
    part.words_52_to_ac[3] = 80U;
    part.words_52_to_ac[4] = 1U;
    part.words_52_to_ac[5] = 2U;
    part.words_52_to_ac[6] = 80U;
    part.words_52_to_ac[36] = 5'000U;

    auto tower = make_event_test_tower();
    const auto fire = simtower::prepare_original_fire_event(*tower, part, 0U);
    assert(fire.offered && fire.floor == 17 && fire.x == 148);
    assert(fire.dialog.dialog_id == 3011U);
    assert(fire.dialog.argument == 8);
    assert(fire.dialog.wave_resource == 10006);
    assert(tower->random_state == 22'695'478U);
    assert(test_header_word(*tower, 60U) == 0U);
    assert(test_header_word(*tower, 74U) == 148U);
    assert(test_header_word(*tower, 76U) == 17U);
    tower->security_event_accelerated = true;
    // Direct 10e8:0000 coverage: commit seeds both 120-word fire-coordinate
    // arrays to -1, then writes the selected floor's x coordinate.
    simtower::commit_original_fire_event(*tower, part);
    assert(!tower->security_event_accelerated);
    assert(test_header_word(*tower, 60U) == 8U);
    assert(test_header_word(*tower, 72U) == 0U);
    assert(test_header_word(*tower, 70U) == 100U);
    assert(test_header_word(*tower, 74U) == 148U);
    assert(test_header_word(*tower, 76U) == 17U);
    assert(test_header_word(*tower, 78U) == 0U);
    for (std::size_t index = 0; index < 120U; ++index) {
      const auto expected = index == 17U ? 148U : 0xffffU;
      assert(test_header_word(*tower, 80U + index * 2U) == expected);
      assert(test_header_word(*tower, 320U + index * 2U) == expected);
    }

    const auto bytes = simtower::serialize_original_tdt(*tower);
    const auto reparsed = std::make_unique<simtower::OriginalTdtDocument>(
        simtower::parse_original_tdt(bytes));
    assert(test_header_word(*reparsed, 60U) == 8U);
    assert(test_header_word(*reparsed, 70U) == 100U);
    assert(test_header_word(*reparsed, 74U) == 148U);
    assert(test_header_word(*reparsed, 76U) == 17U);
    assert(test_header_word(*reparsed, 80U + 34U) == 148U);
    assert(test_header_word(*reparsed, 320U + 34U) == 148U);
    assert(simtower::serialize_original_tdt(*reparsed) == bytes);

    const auto start_fire = [&part](simtower::OriginalTdtDocument& document,
                                    std::uint8_t phase) {
      const auto offer =
          simtower::prepare_original_fire_event(document, part, phase);
      if (offer.offered) {
        simtower::commit_original_fire_event(document, part);
      }
      return offer;
    };

    // SECOM changes the informational dialog and stores the exact PART timer.
    tower = make_event_test_tower();
    // Direct 10d0:0b03-0b31 reconstruction coverage: crew x zero is not enough
    // until b406 bit three is active.
    assert(!simtower::original_fire_crew_menu_enabled_after_rebuild(*tower));
    store_test_header_word(*tower, 32U, 5U);
    const auto sensed = start_fire(*tower, 3U);
    assert(sensed.offered && sensed.dialog.dialog_id == 3010U);
    assert(sensed.dialog.argument == 8);
    assert(test_header_word(*tower, 72U) == 80U);
    assert(simtower::original_fire_crew_menu_enabled_after_rebuild(*tower));

    // Direct 10e8:0147/01e2 coverage: the scheduled offer occurs exactly at
    // start+ddd6. Result one is No; every
    // other result hires. The later menu path at 01e2 hires only on two.
    assert(!simtower::original_fire_crew_offer_due(*tower, part));
    tower->header.frame_time = 102U;
    assert(simtower::original_fire_crew_offer_due(*tower, part));
    const auto crew_offer = simtower::original_fire_crew_offer(part);
    assert(crew_offer.dialog_id == 3012U && crew_offer.argument == -5'000 &&
           crew_offer.wave_resource == 10000);
    auto declined = simtower::resolve_original_fire_crew_offer(
        *tower, part, 1U, true);
    assert(declined.handled && !declined.hired);
    assert(declined.followup_dialog.dialog_id == 3014U);
    assert(declined.fire_menu_enabled && *declined.fire_menu_enabled);
    assert(declined.security_dispatch_pending &&
           declined.security_dispatch_flags == 8U);
    simtower::dispatch_original_security_response(
        *tower, declined.security_dispatch_flags);
    for (std::size_t index = 0; index < 6U; ++index) {
      assert(tower->people[index].exact_bytes[5] == std::byte{0});
      assert(tower->people[index].exact_bytes[7] == std::byte{20});
    }

    tower = make_event_test_tower();
    assert(start_fire(*tower, 0U).offered);
    auto hired = simtower::resolve_original_fire_crew_offer(
        *tower, part, 0U, true);
    assert(hired.handled && hired.hired && hired.focus_requested);
    assert(hired.focus_floor == 17 && hired.focus_x == 168);
    assert(hired.fire_menu_enabled && !*hired.fire_menu_enabled);
    assert(test_header_word(*tower, 78U) == 168U);
    assert(!simtower::original_fire_crew_menu_enabled_after_rebuild(*tower));
    assert(tower->header.balance == 45'000);
    assert(tower->header.other_income == -4'000);
    assert(tower->header.construction_costs == 2'000);
    store_test_header_word(*tower, 78U, 0U);
    const auto reconsider_no = simtower::resolve_original_fire_crew_offer(
        *tower, part, 1U, false);
    assert(reconsider_no.handled && !reconsider_no.hired &&
           !reconsider_no.followup_dialog.valid());
    const auto reconsider_yes = simtower::resolve_original_fire_crew_offer(
        *tower, part, 2U, false);
    assert(reconsider_yes.hired && test_header_word(*tower, 78U) == 168U);

    // Direct 10e8:076a coverage divides the visible coverage into left
    // [x,x+6) and right
    // [x+6,x+12). 07d6 tests the complete twelve-cell band for each stored
    // endpoint, so the coincident starting endpoints are both cleared by a
    // coordinate anywhere in [x,x+12).
    tower = make_event_test_tower();
    assert(start_fire(*tower, 0U).offered);
    assert(!simtower::original_fire_covers_coordinate(*tower, 17, 147));
    for (std::int16_t x = 148; x < 160; ++x) {
      assert(simtower::original_fire_covers_coordinate(*tower, 17, x));
    }
    assert(!simtower::original_fire_covers_coordinate(*tower, 17, 160));
    assert(simtower::extinguish_original_fire_at(*tower, 17, 153));
    assert(test_header_word(*tower, 320U + 34U) == 0xffffU);
    assert(test_header_word(*tower, 80U + 34U) == 0xffffU);
    assert(!simtower::extinguish_original_fire_at(*tower, 17, 154));

    // Direct 10e8:0304 coverage: damage the current endpoints before moving
    // them on ddd0's
    // seven-frame cadence. A new floor activates only at its ddd2 delay.
    tower = make_event_test_tower();
    assert(start_fire(*tower, 0U).offered);
    auto fire_tick = simtower::advance_original_fire_event(*tower, part);
    assert(fire_tick.damage_requests ==
           std::vector<simtower::OriginalFacilityDamageRequest>(
               {{17, 148, 0U}, {17, 160, 0U}}));
    assert(test_header_word(*tower, 320U + 34U) == 148U);
    assert(test_header_word(*tower, 80U + 34U) == 148U);
    tower->header.frame_time = 105U;
    fire_tick = simtower::advance_original_fire_event(*tower, part);
    assert(fire_tick.damage_requests.size() == 2U);
    assert(test_header_word(*tower, 320U + 34U) == 147U);
    assert(test_header_word(*tower, 80U + 34U) == 149U);
    tower->header.frame_time = 180U;
    fire_tick = simtower::advance_original_fire_event(*tower, part);
    assert(test_header_word(*tower, 320U + 36U) == 148U);
    assert(test_header_word(*tower, 80U + 36U) == 148U);

    // Direct 10e8:0450/0856 coverage: b412 suppresses all spread/deletion for
    // its countdown frame. The hired
    // crew still moves afterward, requests reserved WAVE/10009, and clears
    // every endpoint to its right through 0856.
    tower = make_event_test_tower();
    store_test_header_word(*tower, 32U, 5U);
    assert(start_fire(*tower, 0U).offered);
    assert(test_header_word(*tower, 72U) == 80U);
    fire_tick = simtower::advance_original_fire_event(*tower, part);
    assert(test_header_word(*tower, 72U) == 79U);
    assert(fire_tick.damage_requests.empty());
    hired = simtower::resolve_original_fire_crew_offer(
        *tower, part, 2U, true);
    assert(hired.hired);
    store_test_header_word(*tower, 320U + 36U, 175U);
    tower->header.frame_time = 101U;
    fire_tick = simtower::advance_original_fire_event(*tower, part);
    assert(test_header_word(*tower, 78U) == 167U);
    assert(test_header_word(*tower, 320U + 36U) == 0xffffU);
    assert(fire_tick.sound_requests ==
           std::vector<simtower::OriginalEventSoundRequest>(
               {{10009, 10U, 5U, true}}));
    store_test_header_word(*tower, 78U, 101U);
    tower->header.frame_time = 102U;
    fire_tick = simtower::advance_original_fire_event(*tower, part);
    assert(test_header_word(*tower, 78U) == 0U);
    assert(fire_tick.sound_requests.size() == 1U);

    // Direct 10e8:029f coverage: with both spread arrays inactive, 025a
    // clears flag eight, resets Security, grays the command, and raises time.
    tower = make_event_test_tower();
    assert(start_fire(*tower, 0U).offered);
    for (std::size_t index = 0; index < 120U; ++index) {
      store_test_header_word(*tower, 80U + index * 2U, 0xffffU);
      store_test_header_word(*tower, 320U + index * 2U, 0xffffU);
    }
    tower->header.frame_time = 400U;
    fire_tick = simtower::advance_original_fire_event(*tower, part);
    assert(fire_tick.completed && fire_tick.dialog.dialog_id == 3013U);
    assert(fire_tick.fire_menu_enabled && !*fire_tick.fire_menu_enabled);
    assert(test_header_word(*tower, 60U) == 0U);
    assert(tower->header.frame_time == 400U);
    assert(fire_tick.deferred_completion ==
           simtower::OriginalEventDeferredCompletion::fire);
    tower->security_event_accelerated = true;
    simtower::complete_original_event_action(
        *tower, fire_tick.deferred_completion);
    assert(!tower->security_event_accelerated);
    assert(tower->header.frame_time == 1'500U);

    // The day-phase gate precedes floor selection and RNG consumption.
    tower = make_event_test_tower();
    assert(!start_fire(*tower, 4U).offered);
    assert(tower->random_state == 1U);

    // 10d0:1518's opposite-endian revision-0x24 loader leaves gameplay-facing
    // memory in the normal little-endian runtime layout while the lossless
    // tooling stream retains the foreign source bytes.
    tower = make_event_test_tower(true);
    const auto swapped_fire = start_fire(*tower, 0U);
    assert(swapped_fire.offered);
    assert(test_header_word(*tower, 60U) == 8U);
    assert(test_header_word(*tower, 70U) == 100U);
    assert(test_header_word(*tower, 80U + 34U) == 148U);
    const auto swapped_bytes = simtower::serialize_original_tdt(*tower);
    const auto swapped_reparsed =
        std::make_unique<simtower::OriginalTdtDocument>(
            simtower::parse_original_tdt(swapped_bytes));
    assert(!swapped_reparsed->header.byte_swapped);
    assert(test_header_word(*swapped_reparsed, 60U) == 8U);
    assert(test_header_word(*swapped_reparsed, 70U) == 100U);
    assert(test_header_word(*swapped_reparsed, 80U + 34U) == 148U);
    assert(test_header_word(*swapped_reparsed, 320U + 34U) == 148U);
    assert(simtower::serialize_original_tdt_lossless(*swapped_reparsed) ==
           swapped_bytes);
    const auto normalized_bytes =
        simtower::serialize_original_tdt(*swapped_reparsed);
    assert(normalized_bytes != swapped_bytes);
    assert(normalized_bytes[0] == std::byte{0} &&
           normalized_bytes[1] == std::byte{0x24});
  }

  [] {
    // Complete 11f8:3528/35ac damage consumer. Start with an active Office so
    // person retirement, dce4/dd34 cleanup, population, record conversion,
    // floor lookup rebuild, and the post-success random WAVE are all visible.
    simtower::OriginalYenTable rent{};
    const auto make_tenant = [](
                                 std::int8_t type, std::uint16_t left,
                                 std::uint16_t right, std::uint8_t key,
                                 std::uint16_t linked,
                                 std::uint32_t people_start,
                                 std::uint8_t status = 0U) {
      simtower::OriginalTdtTenant tenant{};
      tenant.left = left;
      tenant.right = right;
      tenant.type = type;
      tenant.status = status;
      tenant.variant = static_cast<std::uint8_t>(linked);
      tenant.preserved_07_to_0f[0] =
          static_cast<std::byte>(linked >> 8U);
      tenant.preserved_07_to_0f[5] = static_cast<std::byte>(key);
      tenant.exact_bytes[0] = static_cast<std::byte>(left);
      tenant.exact_bytes[1] = static_cast<std::byte>(left >> 8U);
      tenant.exact_bytes[2] = static_cast<std::byte>(right);
      tenant.exact_bytes[3] = static_cast<std::byte>(right >> 8U);
      tenant.exact_bytes[4] = static_cast<std::byte>(type);
      tenant.exact_bytes[5] = static_cast<std::byte>(status);
      tenant.exact_bytes[6] = static_cast<std::byte>(linked);
      tenant.exact_bytes[7] = static_cast<std::byte>(linked >> 8U);
      for (std::size_t byte = 0; byte < 4U; ++byte) {
        tenant.exact_bytes[8U + byte] =
            static_cast<std::byte>(people_start >> (byte * 8U));
      }
      tenant.exact_bytes[12] = static_cast<std::byte>(key);
      tenant.exact_bytes[13] = std::byte{1};
      tenant.exact_bytes[14] = std::byte{1};
      tenant.exact_bytes[15] = std::byte{0xff};
      return tenant;
    };
    const auto store_tail_word = [](std::array<std::byte, 0x28>& bytes,
                                    std::size_t index,
                                    std::uint16_t value) {
      bytes[index * 2U] = static_cast<std::byte>(value);
      bytes[index * 2U + 1U] = static_cast<std::byte>(value >> 8U);
    };
    const auto load_tail_word = [](const std::array<std::byte, 0x28>& bytes,
                                   std::size_t index) {
      return static_cast<std::uint16_t>(
          std::to_integer<std::uint8_t>(bytes[index * 2U]) |
          (std::to_integer<std::uint8_t>(bytes[index * 2U + 1U]) << 8U));
    };

    auto tower = std::make_unique<simtower::OriginalTdtDocument>(
        simtower::make_original_new_tdt());
    tower->random_state = 1U;
    tower->header.frame_time = 200U;
    auto& office_floor = tower->floors[11];
    office_floor.left_edge = 100U;
    office_floor.right_edge = 110U;
    office_floor.tenants = {make_tenant(7, 100U, 110U, 0U, 0U, 0U)};
    office_floor.tenant_index[0] = 0U;
    tower->people_count = 6U;
    tower->people.resize(6U);
    for (auto& person : tower->people) {
      person.exact_bytes[4] = std::byte{7};
      person.exact_bytes[6] = std::byte{0x55};
      person.exact_bytes[8] = std::byte{0xff};
    }
    simtower::add_original_population_for_type(*tower, 7U, 6);
    tower->header.person_link_count = 2U;
    tower->post_elevator.dce4_person_indices[0] = 0;
    tower->post_elevator.dce4_person_indices[1] = 5;
    tower->header.tenant_link_count = 2U;  // DS:b404 dd34 count
    store_test_header_word(*tower, 58U, 2U);
    store_tail_word(tower->post_elevator.dce4_or_dd34, 0U,
                    static_cast<std::uint16_t>(11U * 94U));
    store_tail_word(tower->post_elevator.dce4_or_dd34, 1U, 77U);

    auto damage = simtower::apply_original_facility_damage(
        *tower, rent, 11, 102, 0U);
    assert(damage.allowed && damage.changed && damage.original_type == 7);
    assert(damage.converted_records == 1U);
    assert(damage.rebuilt_floors == std::vector<std::int16_t>({11}));
    assert(damage.sound_requests ==
           std::vector<simtower::OriginalEventSoundRequest>(
               {{10004, 0U, 2U, false}}));
    assert(office_floor.tenants[0].type == 47 &&
           office_floor.tenants[0].status == 0U &&
           office_floor.tenants[0].variant == 0U);
    assert(office_floor.tenants[0].exact_bytes[12] == std::byte{0xff} &&
           office_floor.tenants[0].exact_bytes[13] == std::byte{1} &&
           office_floor.tenants[0].exact_bytes[14] == std::byte{1} &&
           office_floor.tenants[0].exact_bytes[15] == std::byte{0xff} &&
           office_floor.tenants[0].rent_rate == 4U &&
           office_floor.tenants[0].subtype == 0U);
    for (const auto& person : tower->people) {
      assert(person.exact_bytes[4] == std::byte{0});
      assert(person.exact_bytes[6] == std::byte{0});
    }
    assert(tower->post_elevator.finance.total_population == 0);
    assert(tower->header.person_link_count == 0U);
    assert(tower->header.tenant_link_count == 1U);
    assert(test_header_word(*tower, 58U) == 1U);
    assert(load_tail_word(tower->post_elevator.dce4_or_dd34, 0U) == 77U);
    assert(load_tail_word(tower->post_elevator.dce4_or_dd34, 1U) == 0U);
    assert(tower->random_state == 0x015a4e36U);

    // Direct 11f8:3959/3a31/3a87 coverage. Replacement converts the middle
    // Office to type zero/status two with the literal trailing bytes, then
    // the rebuild coalesces the three adjacent type-zero records before the
    // two type-24 records. Each merge copies the later right edge, shifts the
    // remaining 18-byte records, decrements the count, and retries its index.
    tower = std::make_unique<simtower::OriginalTdtDocument>(
        simtower::make_original_new_tdt());
    auto& merge_floor = tower->floors[20];
    merge_floor.tenants = {
        make_tenant(0, 100U, 101U, 0xffU, 0U, 0U),
        make_tenant(7, 101U, 102U, 0U, 0U, 0U),
        make_tenant(0, 102U, 103U, 0xffU, 0U, 0U),
        make_tenant(24, 103U, 104U, 0xffU, 0U, 0U),
        make_tenant(24, 104U, 105U, 0xffU, 0U, 0U),
    };
    merge_floor.tenant_index[0] = 1U;
    damage = simtower::apply_original_facility_damage(
        *tower, rent, 20, 101, 1U);
    assert(damage.allowed && damage.changed && damage.converted_records == 1U);
    assert(merge_floor.tenants.size() == 2U);
    assert(merge_floor.tenants[0].type == 0 &&
           merge_floor.tenants[0].left == 100U &&
           merge_floor.tenants[0].right == 103U);
    assert(merge_floor.tenants[1].type == 24 &&
           merge_floor.tenants[1].left == 103U &&
           merge_floor.tenants[1].right == 105U);
    assert(merge_floor.tenants[0].exact_bytes[2] == std::byte{103} &&
           merge_floor.tenants[0].exact_bytes[3] == std::byte{0});
    assert(merge_floor.tenants[1].exact_bytes[2] == std::byte{105} &&
           merge_floor.tenants[1].exact_bytes[3] == std::byte{0});
    // 1228:0e30 overwrites only live non-FF keys; it does not clear stale
    // lookup slots whose keyed record was just converted/merged away.
    assert(merge_floor.tenant_index[0] == 1U);

    // Empty spans succeed without consuming rand; protected and pending
    // records reject with the two exact alert codes only for nonzero flags.
    tower->floors[12].tenants = {
        make_tenant(14, 100U, 110U, 0xffU, 0U, 0U)};
    damage = simtower::apply_original_facility_damage(
        *tower, rent, 12, 102, 1U);
    assert(!damage.allowed && !damage.changed && damage.alert_code == 21);
    tower->floors[13].tenants = {
        make_tenant(-1, 100U, 110U, 0xffU, 0U, 0U)};
    damage = simtower::apply_original_facility_damage(
        *tower, rent, 13, 102, 1U);
    assert(!damage.allowed && damage.alert_code == 33);
    tower->floors[14].tenants = {
        make_tenant(0, 100U, 110U, 0xffU, 0U, 0U)};
    const auto random_before_empty = tower->random_state;
    damage = simtower::apply_original_facility_damage(
        *tower, rent, 14, 102, 0U);
    assert(damage.allowed && !damage.changed);
    assert(tower->random_state == random_before_empty);

    // 1180:0e79/0ee3 retire and convert both entertainment halves, clear the
    // dc24 record, decrement b400, and still emit only one outer damage sound.
    tower = std::make_unique<simtower::OriginalTdtDocument>(
        simtower::make_original_new_tdt());
    tower->people_count = 112U;
    tower->people.resize(112U);
    for (auto& person : tower->people) person.exact_bytes[4] = std::byte{18};
    tower->floors[12].tenants = {
        make_tenant(18, 100U, 120U, 0U, 0U, 0U)};
    tower->floors[11].tenants = {
        make_tenant(19, 100U, 120U, 0U, 0U, 56U)};
    tower->floors[12].tenant_index[0] = 0U;
    tower->floors[11].tenant_index[0] = 0U;
    auto& entertainment = tower->post_elevator.dc24_records[0];
    entertainment.fill(std::byte{0});
    entertainment[0] = std::byte{12};
    entertainment[1] = std::byte{11};
    entertainment[2] = std::byte{0};
    entertainment[3] = std::byte{0};
    entertainment[7] = std::byte{0xff};
    store_test_header_word(*tower, 54U, 1U);
    damage = simtower::apply_original_facility_damage(
        *tower, rent, 12, 102, 0U);
    assert(damage.changed && damage.converted_records == 2U);
    assert(damage.rebuilt_floors ==
           std::vector<std::int16_t>({12, 11}));
    assert(tower->floors[12].tenants[0].type == 47 &&
           tower->floors[11].tenants[0].type == 47);
    assert(entertainment[0] == std::byte{0xfe} &&
           entertainment[1] == std::byte{0xfe});
    assert(test_header_word(*tower, 54U) == 0U);
    for (const auto& person : tower->people) {
      assert(person.exact_bytes[4] == std::byte{0});
    }
    assert(damage.sound_requests.size() == 1U);

    // 1088:038d converts the vertically paired Recycling records and lowers
    // the persisted center count once.
    tower = std::make_unique<simtower::OriginalTdtDocument>(
        simtower::make_original_new_tdt());
    tower->floors[12].tenants = {
        make_tenant(20, 100U, 125U, 0xffU, 0U, 0U)};
    tower->floors[11].tenants = {
        make_tenant(21, 100U, 125U, 0xffU, 0U, 0U)};
    store_test_header_word(*tower, 42U, 1U);
    damage = simtower::apply_original_facility_damage(
        *tower, rent, 12, 102, 0U);
    assert(damage.changed && damage.converted_records == 2U);
    assert(tower->floors[12].tenants[0].type == 47 &&
           tower->floors[11].tenants[0].type == 47);
    assert(test_header_word(*tower, 42U) == 0U);

    const std::array<simtower::OriginalFacilityDamageRequest, 2> repeated = {{
        {12, 102, 0U},
        {12, 102, 0U},
    }};
    const auto sequence = simtower::apply_original_facility_damage_sequence(
        *tower, rent, repeated);
    assert(sequence.attempts == 2U && sequence.allowed == 2U &&
           sequence.changed == 2U && sequence.converted_records == 2U &&
           sequence.sound_requests.size() == 2U);

  }();

  [] {
    // Direct 10c8:01f7/10e8:025a host-order coverage. Bomb plays its
    // explosion before damage and then dispatches/focuses; Fire applies
    // damage before its reserved crew sound and updates the menu afterward.
    using Operation = simtower::OriginalEventHostOperation;
    simtower::OriginalEventActionResult action{};
    action.security_dispatch_pending = true;
    action.focus_requested = true;
    action.fire_menu_enabled = false;
    action.dialog.dialog_id = 3004U;
    action.deferred_completion =
        simtower::OriginalEventDeferredCompletion::bomb;
    const auto bomb = simtower::original_bomb_event_host_plan(action);
    assert(bomb.operation_count == 6U);
    assert((std::ranges::equal(
        bomb.sequence(),
        std::array{
            Operation::play_sounds,
            Operation::apply_damage,
            Operation::dispatch_security,
            Operation::focus_coordinate,
            Operation::show_dialog,
            Operation::complete_deferred_action,
        })));

    action.deferred_completion =
        simtower::OriginalEventDeferredCompletion::fire;
    const auto fire = simtower::original_fire_event_host_plan(action);
    assert(fire.operation_count == 5U);
    assert((std::ranges::equal(
        fire.sequence(),
        std::array{
            Operation::apply_damage,
            Operation::play_sounds,
            Operation::update_fire_menu,
            Operation::show_dialog,
            Operation::complete_deferred_action,
        })));

    action = {};
    assert((std::ranges::equal(
        simtower::original_bomb_event_host_plan(action).sequence(),
        std::array{Operation::play_sounds, Operation::apply_damage})));
    assert((std::ranges::equal(
        simtower::original_fire_event_host_plan(action).sequence(),
        std::array{Operation::apply_damage, Operation::play_sounds})));
  }();

  [] {
      simtower::OriginalYenTable rent{};
      const auto make_tenant = [](
                                   std::int8_t type, std::uint16_t left,
                                   std::uint16_t right, std::uint8_t key,
                                   std::uint16_t linked,
                                   std::uint32_t people_start,
                                   std::uint8_t status = 0U) {
        simtower::OriginalTdtTenant tenant{};
        tenant.left = left;
        tenant.right = right;
        tenant.type = type;
        tenant.status = status;
        tenant.variant = static_cast<std::uint8_t>(linked);
        tenant.preserved_07_to_0f[0] = static_cast<std::byte>(linked >> 8U);
        tenant.preserved_07_to_0f[5] = static_cast<std::byte>(key);
        tenant.exact_bytes[0] = static_cast<std::byte>(left);
        tenant.exact_bytes[1] = static_cast<std::byte>(left >> 8U);
        tenant.exact_bytes[2] = static_cast<std::byte>(right);
        tenant.exact_bytes[3] = static_cast<std::byte>(right >> 8U);
        tenant.exact_bytes[4] = static_cast<std::byte>(type);
        tenant.exact_bytes[5] = static_cast<std::byte>(status);
        tenant.exact_bytes[6] = static_cast<std::byte>(linked);
        tenant.exact_bytes[7] = static_cast<std::byte>(linked >> 8U);
        for (std::size_t byte = 0; byte < 4U; ++byte) {
          tenant.exact_bytes[8U + byte] =
              static_cast<std::byte>(people_start >> (byte * 8U));
        }
        tenant.exact_bytes[12] = static_cast<std::byte>(key);
        tenant.exact_bytes[13] = std::byte{1};
        tenant.exact_bytes[14] = std::byte{1};
        tenant.exact_bytes[15] = std::byte{0xff};
        return tenant;
      };

      // 11f8:3d2d/3e3e adds the view point, divides into the 120-floor/eight-
      // pixel grid, and returns the first tenant whose right edge exceeds x.
      auto tower = std::make_unique<simtower::OriginalTdtDocument>(
          simtower::make_original_new_tdt());
      tower->floors[11].left_edge = 100U;
      tower->floors[11].right_edge = 110U;
      tower->floors[11].tenants = {
          make_tenant(7, 100U, 110U, 0U, 0U, 0U)};
      auto hit = simtower::original_facility_hit_from_client(
          *tower, 18, 90, 800, 3800);
      assert(hit.hit && hit.floor == 11 && hit.x == 102 &&
             hit.tenant_index == 0U);
      hit = simtower::original_facility_hit_from_client(
          *tower, 80, 90, 800, 3800);
      assert(!hit.hit);  // x == tenant.right is outside its half-open span.

      // Direct 11f8:0793 integration coverage: a successful point lookup
      // calls 35ac with the literal demolition flag one; a miss never issues
      // a damage attempt or mutates the document.
      auto clicked_tower = *tower;
      const auto clicked = simtower::apply_original_facility_click_damage(
          clicked_tower, rent, 18, 90, 800, 3800);
      assert(clicked.hit.hit && clicked.hit.floor == 11 &&
             clicked.hit.x == 102);
      assert(clicked.damage.allowed && clicked.damage.changed &&
             clicked.damage.original_type == 7 &&
             clicked.damage.converted_records == 1U);
      assert((clicked.damage.sound_requests ==
              std::vector<simtower::OriginalEventSoundRequest>(
                  {{7003, 0U, 4U, false}})));
      assert(clicked_tower.floors[11].tenants[0].type == 0 &&
             clicked_tower.floors[11].tenants[0].status == 2U);
      const auto clicked_bytes =
          simtower::serialize_original_tdt(clicked_tower);
      const auto missed = simtower::apply_original_facility_click_damage(
          clicked_tower, rent, 80, 90, 800, 3800);
      assert(!missed.hit.hit && !missed.damage.allowed &&
             !missed.damage.changed);
      assert(simtower::serialize_original_tdt(clicked_tower) ==
             clicked_bytes);

      // User-visible demolition/rebuild integration: Bulldozer flags=1 leaves
      // a type-zero/status-two span. Reapplying Floor there is a valid no-cost
      // operation, and a facility may immediately split and reuse that span.
      auto& support = clicked_tower.floors[10];
      support.left_edge = 100U;
      support.right_edge = 110U;
      support.tenants = {make_tenant(0, 100U, 110U, 0xffU, 0U, 0U, 2U)};
      simtower::OriginalYenTable construction_costs{};
      auto rebuilt_floor = simtower::build_original_floor(
          clicked_tower, 11, 100U, 110U, construction_costs);
      assert(rebuilt_floor.succeeded());
      assert(clicked_tower.floors[11].tenants.size() == 1U &&
             clicked_tower.floors[11].tenants[0].type == 0 &&
             clicked_tower.floors[11].tenants[0].status == 2U);
      const auto rebuilt_office = simtower::build_original_office(
          clicked_tower, 11, 100U, 0U, construction_costs);
      assert(rebuilt_office.succeeded());
      assert(clicked_tower.floors[11].tenants[0].type == -7 &&
             clicked_tower.floors[11].tenants[0].left == 100U &&
             clicked_tower.floors[11].tenants[0].right == 109U);

      // End-to-end regression for the visible edit interaction. 11f8:3da4
      // adds twelve pixels after snapping a non-Lobby tool, while 3e3e's
      // Bulldozer hit-test uses the unsnapped client point. The same visible
      // point must therefore build an Office on floor 11, hit that Office,
      // restore its type-zero span, and immediately rebuild in that span.
      auto interaction_tower = simtower::make_original_new_tdt();
      interaction_tower.header.balance = 1'000'000;
      interaction_tower.floors[10].left_edge = 100U;
      interaction_tower.floors[10].right_edge = 110U;
      interaction_tower.floors[10].tenants = {
          make_tenant(0, 100U, 110U, 0xffU, 0U, 0U, 2U)};
      interaction_tower.floors[11].left_edge = 100U;
      interaction_tower.floors[11].right_edge = 110U;
      interaction_tower.floors[11].tenants = {
          make_tenant(0, 100U, 110U, 0xffU, 0U, 0U, 2U)};
      const auto client_placement =
          simtower::original_office_placement_from_client(
              836, 3906, 0, 0);
      assert(client_placement.floor == 11 &&
             client_placement.left == 100 &&
             client_placement.right == 109);
      assert(simtower::build_original_office(
                 interaction_tower, client_placement.floor,
                 static_cast<std::uint16_t>(client_placement.left), 0U,
                 construction_costs)
                 .succeeded());
      simtower::activate_all_original_pending_facilities_for_schedule(
          interaction_tower);
      assert(interaction_tower.floors[11].tenants[0].type == 7);
      const auto client_bulldoze =
          simtower::apply_original_facility_click_damage(
              interaction_tower, rent, 836, 3906, 0, 0);
      assert(client_bulldoze.hit.hit && client_bulldoze.hit.floor == 11 &&
             client_bulldoze.hit.x == 104 &&
             client_bulldoze.damage.changed);
      assert(interaction_tower.floors[11].tenants.size() == 1U &&
             interaction_tower.floors[11].tenants[0].type == 0 &&
             interaction_tower.floors[11].tenants[0].left == 100U &&
             interaction_tower.floors[11].tenants[0].right == 110U);
      assert(simtower::build_original_office(
                 interaction_tower, client_placement.floor,
                 static_cast<std::uint16_t>(client_placement.left), 1U,
                 construction_costs)
                 .succeeded());
      assert(interaction_tower.floors[11].tenants[0].type == -7 &&
             interaction_tower.floors[11].tenants[0].left == 100U &&
             interaction_tower.floors[11].tenants[0].right == 109U);

      // The reported neighbor case exercises the less trivial post-demolition
      // shape. The automatic Floor gap, the demolished Office, and the Floor
      // to its right coalesce into one type-zero run; that larger run must
      // still split cleanly when the Office is rebuilt at its former x.
      auto neighbor_tower = simtower::make_original_new_tdt();
      neighbor_tower.header.balance = 1'000'000;
      auto& neighbor_support = neighbor_tower.floors[10];
      neighbor_support.left_edge = 100U;
      neighbor_support.right_edge = 150U;
      neighbor_support.tenants = {
          make_tenant(0, 100U, 150U, 0xffU, 0U, 0U, 2U)};
      auto& neighbor_floor = neighbor_tower.floors[11];
      neighbor_floor.left_edge = 100U;
      neighbor_floor.right_edge = 150U;
      neighbor_floor.tenants = {
          make_tenant(7, 100U, 109U, 0U, 0U, 0U, 0x10U),
          make_tenant(0, 109U, 120U, 0xffU, 0U, 0U, 2U),
          make_tenant(7, 120U, 129U, 1U, 0U, 6U, 0x10U),
          make_tenant(0, 129U, 150U, 0xffU, 0U, 0U, 2U),
      };
      neighbor_floor.tenant_index[0] = 0U;
      neighbor_floor.tenant_index[1] = 2U;
      const auto neighbor_demolition = simtower::apply_original_facility_damage(
          neighbor_tower, rent, 11, 124, 1U);
      assert(neighbor_demolition.changed &&
             neighbor_demolition.original_type == 7);
      assert(neighbor_floor.tenants.size() == 2U &&
             neighbor_floor.tenants[0].type == 7 &&
             neighbor_floor.tenants[1].type == 0 &&
             neighbor_floor.tenants[1].left == 109U &&
             neighbor_floor.tenants[1].right == 150U &&
             neighbor_floor.tenants[1].status == 2U);
      assert(simtower::build_original_office(
                 neighbor_tower, 11, 120U, 2U, construction_costs)
                 .succeeded());
      assert(neighbor_floor.tenants.size() == 4U &&
             neighbor_floor.tenants[1].type == 0 &&
             neighbor_floor.tenants[1].left == 109U &&
             neighbor_floor.tenants[1].right == 120U &&
             neighbor_floor.tenants[2].type == -7 &&
             neighbor_floor.tenants[2].left == 120U &&
             neighbor_floor.tenants[2].right == 129U &&
             neighbor_floor.tenants[3].type == 0 &&
             neighbor_floor.tenants[3].left == 129U &&
             neighbor_floor.tenants[3].right == 150U);

      // Screenshot regression: the reported 45-pixel story height establishes
      // 125% desktop scaling, so its 90-pixel white outline is an Office's
      // nine-cell footprint.  The bulldozed opening at its right is only four
      // cells (type 3), and the centered Office overlaps five cells of the
      // neighboring six-cell room (type 4).  That Office must remain occupied,
      // but rebuilding the demolished room at its exact old coordinate must
      // succeed immediately.
      auto hotel_neighbor_tower = simtower::make_original_new_tdt();
      hotel_neighbor_tower.header.balance = 1'000'000;
      auto& hotel_support = hotel_neighbor_tower.floors[10];
      hotel_support.left_edge = 100U;
      hotel_support.right_edge = 110U;
      hotel_support.tenants = {
          make_tenant(0, 100U, 110U, 0xffU, 0U, 0U, 2U)};
      auto& hotel_floor = hotel_neighbor_tower.floors[11];
      hotel_floor.left_edge = 100U;
      hotel_floor.right_edge = 110U;
      hotel_floor.tenants = {
          make_tenant(4, 100U, 106U, 0U, 0U, 0U, 0x18U),
          make_tenant(3, 106U, 110U, 1U, 0U, 3U, 0x18U),
      };
      hotel_floor.tenant_index[0] = 0U;
      hotel_floor.tenant_index[1] = 1U;
      const auto hotel_demolition = simtower::apply_original_facility_damage(
          hotel_neighbor_tower, rent, 11, 108, 1U);
      assert(hotel_demolition.changed &&
             hotel_demolition.original_type == 3);
      assert(hotel_floor.tenants.size() == 2U &&
             hotel_floor.tenants[0].type == 4 &&
             hotel_floor.tenants[0].left == 100U &&
             hotel_floor.tenants[0].right == 106U &&
             hotel_floor.tenants[1].type == 0 &&
             hotel_floor.tenants[1].left == 106U &&
             hotel_floor.tenants[1].right == 110U);
      const auto overlapping_office = simtower::build_original_office(
          hotel_neighbor_tower, 11, 101U, 0U, construction_costs);
      assert(overlapping_office.status ==
             simtower::OriginalConstructionStatus::occupied);
      assert(!overlapping_office.succeeded());
      const auto rebuilt_single = simtower::build_original_hotel_room(
          hotel_neighbor_tower, 3U, 11, 106U, 0U, construction_costs);
      assert(rebuilt_single.succeeded());
      assert(hotel_floor.tenants.size() == 2U &&
             hotel_floor.tenants[0].type == 4 &&
             hotel_floor.tenants[1].type == -3 &&
             hotel_floor.tenants[1].left == 106U &&
             hotel_floor.tenants[1].right == 110U);

      // The matching nine-cell case distinguishes a real stale-demolition
      // failure from the mixed six-plus-four footprint above: demolishing an
      // active Office frees all nine cells and the same placement works.
      auto screenshot_office_tower = simtower::make_original_new_tdt();
      screenshot_office_tower.header.balance = 1'000'000;
      screenshot_office_tower.floors[10].left_edge = 100U;
      screenshot_office_tower.floors[10].right_edge = 110U;
      screenshot_office_tower.floors[10].tenants = {
          make_tenant(0, 100U, 110U, 0xffU, 0U, 0U, 2U)};
      auto& screenshot_office_floor = screenshot_office_tower.floors[11];
      screenshot_office_floor.left_edge = 100U;
      screenshot_office_floor.right_edge = 110U;
      screenshot_office_floor.tenants = {
          make_tenant(7, 100U, 109U, 0U, 0U, 0U, 0x10U),
          make_tenant(0, 109U, 110U, 0xffU, 0U, 0U, 2U)};
      screenshot_office_floor.tenant_index[0] = 0U;
      const auto screenshot_office_demolition =
          simtower::apply_original_facility_damage(
              screenshot_office_tower, rent, 11, 105, 1U);
      assert(screenshot_office_demolition.changed &&
             screenshot_office_floor.tenants.size() == 1U &&
             screenshot_office_floor.tenants[0].type == 0 &&
             screenshot_office_floor.tenants[0].left == 100U &&
             screenshot_office_floor.tenants[0].right == 110U);
      assert(simtower::build_original_office(
                 screenshot_office_tower, 11, 100U, 0U, construction_costs)
                 .succeeded());
      assert(screenshot_office_floor.tenants.size() == 2U &&
             screenshot_office_floor.tenants[0].type == -7 &&
             screenshot_office_floor.tenants[0].left == 100U &&
             screenshot_office_floor.tenants[0].right == 109U &&
             screenshot_office_floor.tenants[1].type == 0 &&
             screenshot_office_floor.tenants[1].left == 109U &&
             screenshot_office_floor.tenants[1].right == 110U);

      // Direct 11f8:3437 coverage: Shift replacement's table expands type 18
      // down one floor and
      // deletes both coordinates in lower-to-upper order with flags=1.
      tower->floors[12].left_edge = 100U;
      tower->floors[12].right_edge = 101U;
      tower->floors[12].tenants = {
          make_tenant(7, 100U, 101U, 0U, 0U, 6U)};
      tower->floors[11].right_edge = 101U;
      tower->floors[11].tenants[0].right = 101U;
      tower->floors[11].tenants[0].exact_bytes[2] = std::byte{101};
      auto replacement = simtower::apply_original_replacement_demolition(
          *tower, rent, 18U, 12, 100, 101);
      assert(replacement.completed && replacement.attempts == 2U &&
             replacement.changed == 2U);
      assert(replacement.sound_requests ==
             std::vector<simtower::OriginalEventSoundRequest>(
                 {{7003, 0U, 4U, false}, {7003, 0U, 4U, false}}));
      assert(tower->floors[11].tenants[0].type == 0 &&
             tower->floors[12].tenants[0].type == 0);

      // Protected tenants abort the wrapper immediately; Lobby is a table
      // no-op and never issues even a damage attempt.
      tower->floors[12].tenants = {
          make_tenant(14, 100U, 101U, 0xffU, 0U, 0U)};
      replacement = simtower::apply_original_replacement_demolition(
          *tower, rent, 7U, 12, 100, 101);
      assert(!replacement.completed && replacement.attempts == 1U &&
             replacement.changed == 0U &&
             replacement.alert_codes == std::vector<std::int16_t>({21}));
      replacement = simtower::apply_original_replacement_demolition(
          *tower, rent, 24U, 12, 100, 101);
      assert(replacement.completed && replacement.attempts == 0U);
  }();

  [] {
    // Direct 1148:007e rating-progress coverage. 1140:0411 uses the four PART
    // dwords followed by the literal 15,000 threshold, and 1140:002d can
    // advance only one rating or prerequisite notification on each frame.
    simtower::OriginalPartTable part{};
    part.dwords_42_to_4e = {300U, 1000U, 5000U, 10000U};
    part.words_52_to_ac[43] = 2000U;
    part.words_52_to_ac[44] = 3000U;
    part.words_52_to_ac[45] = 5000U;

    auto tower = simtower::make_original_new_tdt();
    tower.post_elevator.finance.total_population = 299;
    auto rating = simtower::step_original_rating_progress(
        tower, part, 0U, 4U);
    assert(!rating.promoted && rating.desired_rating == 1U &&
           !rating.notification_code && !rating.dialog.valid());

    tower.post_elevator.finance.total_population = 15'000;
    tower.post_elevator.b923 = 1U;
    tower.post_elevator.b928 = 1U;
    tower.post_elevator.b924 = 123;
    tower.floors[6].left_edge = 10U;
    tower.floors[6].right_edge = 61U;
    tower.floors[6].tenants.resize(1U);
    rating = simtower::step_original_rating_progress(tower, part, 0U, 4U);
    assert(rating.promoted && rating.desired_rating == 6U &&
           rating.notification_code == 0U &&
           rating.dialog == simtower::OriginalEventDialogRequest(
                                {3030U, 0, 10000}));
    assert(tower.header.rating == 2U &&
           tower.post_elevator.b922_flag == 1U &&
           tower.post_elevator.b923 == 0U &&
           tower.post_elevator.b928 == 0U &&
           tower.post_elevator.b924 == -1 &&
           tower.post_elevator.b929 == 0U);

    // Rating two reports the missing Security requirement once. Any
    // successful construction clears b929, while type 14 also satisfies it.
    rating = simtower::step_original_rating_progress(tower, part, 0U, 4U);
    assert(!rating.promoted && rating.notification_code == 1U &&
           tower.post_elevator.b929 == 1U);
    rating = simtower::step_original_rating_progress(tower, part, 0U, 4U);
    assert(!rating.promoted && !rating.notification_code);
    auto completion = simtower::complete_original_rating_construction(
        tower, part, 14U);
    assert(completion.changed && !completion.treasure_awarded &&
           tower.post_elevator.b92a == 1U &&
           tower.post_elevator.b929 == 0U);
    rating = simtower::step_original_rating_progress(tower, part, 0U, 4U);
    assert(rating.promoted && tower.header.rating == 3U &&
           rating.dialog.dialog_id == 3031U);

    // Rating three has the exact Suite -> Recycling -> VIP -> late-day,
    // non-weekend -> Medical sequence. Silent gates do not alter Info text.
    rating = simtower::step_original_rating_progress(tower, part, 0U, 4U);
    assert(!rating.promoted && rating.notification_code == 2U);
    completion = simtower::complete_original_rating_construction(
        tower, part, 5U);
    assert(completion.changed && tower.post_elevator.b92b == 1U);
    rating = simtower::step_original_rating_progress(tower, part, 0U, 4U);
    assert(!rating.promoted && !rating.notification_code);
    tower.post_elevator.b92c = 1U;
    tower.post_elevator.b923 = 1U;
    rating = simtower::step_original_rating_progress(tower, part, 0U, 3U);
    assert(!rating.promoted && !rating.notification_code);
    rating = simtower::step_original_rating_progress(tower, part, 1U, 4U);
    assert(!rating.promoted && !rating.notification_code);
    rating = simtower::step_original_rating_progress(tower, part, 0U, 4U);
    assert(!rating.promoted && rating.notification_code == 6U);
    tower.post_elevator.b92d = 1U;
    rating = simtower::step_original_rating_progress(tower, part, 0U, 4U);
    assert(rating.promoted && tower.header.rating == 4U &&
           rating.dialog.dialog_id == 3032U);

    // Rating four additionally requires a Metro (signed b3e8 >= 0); rating
    // five never auto-promotes because the Cathedral family owns Tower rank.
    tower.post_elevator.b92c = 1U;
    tower.post_elevator.b92d = 1U;
    rating = simtower::step_original_rating_progress(tower, part, 0U, 4U);
    assert(!rating.promoted);
    tower.header.exact_bytes[30] = std::byte{0};
    tower.header.exact_bytes[31] = std::byte{0};
    rating = simtower::step_original_rating_progress(tower, part, 0U, 4U);
    assert(rating.promoted && tower.header.rating == 5U &&
           rating.dialog.dialog_id == 3033U);
    rating = simtower::step_original_rating_progress(tower, part, 0U, 4U);
    assert(!rating.promoted && tower.header.rating == 5U &&
           rating.desired_rating == 6U);

    // Direct 1148:003d/01a8/0163/020f coverage: buried treasure is granted once
    // when the selected basement floor exists and first exceeds rating*25.
    auto treasure_tower = simtower::make_original_new_tdt();
    treasure_tower.header.balance = 100;
    treasure_tower.post_elevator.b929 = 1U;
    treasure_tower.floors[7].left_edge = 25U;
    treasure_tower.floors[7].right_edge = 51U;
    treasure_tower.floors[7].tenants.resize(1U);
    completion = simtower::complete_original_rating_construction(
        treasure_tower, part, 0U);
    assert(completion.changed && completion.treasure_awarded &&
           completion.treasure_value == 2000 &&
           completion.dialog == simtower::OriginalEventDialogRequest(
                                    {3040U, 2000, 10001}));
    assert(treasure_tower.header.balance == 2100 &&
           treasure_tower.header.other_income == 2000 &&
           treasure_tower.post_elevator.b922_flag == 1U &&
           treasure_tower.post_elevator.b929 == 0U);
    completion = simtower::complete_original_rating_construction(
        treasure_tower, part, 0U);
    assert(!completion.changed && !completion.treasure_awarded &&
           treasure_tower.header.balance == 2100);

    // The hidden command-table entry calls 1148:020f directly. Unlike the
    // construction wrapper it does not require the basement-span predicate.
    auto direct_treasure = simtower::make_original_new_tdt();
    direct_treasure.header.rating = 2U;
    direct_treasure.header.balance = 100;
    part.words_52_to_ac[44] = 3000U;
    completion = simtower::award_original_rating_treasure(
        direct_treasure, part);
    assert(completion.changed && completion.treasure_awarded &&
           completion.treasure_value == 3000 &&
           completion.dialog == simtower::OriginalEventDialogRequest(
                                    {3040U, 3000, 10001}));
    assert(direct_treasure.header.balance == 3100 &&
           direct_treasure.header.other_income == 3000 &&
           direct_treasure.post_elevator.b922_flag == 1U);
    direct_treasure.header.rating = 4U;
    direct_treasure.post_elevator.b922_flag = 0U;
    completion = simtower::award_original_rating_treasure(
        direct_treasure, part);
    assert(completion.changed && !completion.treasure_awarded &&
           !completion.dialog.valid() &&
           direct_treasure.post_elevator.b922_flag == 1U);
  }();

  OriginalSimulationState state{};
  static_assert(simtower::kOriginalSimulationGateTicks == 6U);
  static_assert(simtower::kOriginalSimulationGateNominalMs == 96U);
  state.frame_time = 0;
  state.last_tick = 100;
  auto step = simtower::step_original_simulation(state, 105, false, false);
  assert(!step.advanced);
  assert(state.frame_time == 0);
  step = simtower::step_original_simulation(state, 106, false, false);
  assert(step.advanced);
  assert(state.frame_time == 1);
  // 1200:0529 takes a second coarse-clock sample after all scheduled calls,
  // rather than committing 01ac's entry sample before they execute. Model a
  // three-tick callback/modal interval and gate the following frame from its
  // completion time.
  assert(state.last_tick == 100U);
  simtower::finish_original_simulation_step(state, 109U);
  assert(state.last_tick == 109U);
  step = simtower::step_original_simulation(state, 114, false, false);
  assert(!step.advanced && state.frame_time == 1);
  step = simtower::step_original_simulation(state, 115, false, false);
  assert(step.advanced && state.frame_time == 2);
  simtower::finish_original_simulation_step(state, 115U);
  step = simtower::step_original_simulation(state, 115, true, false);
  assert(step.advanced);
  assert(state.frame_time == 3);

  // The 01b5-01c1 comparison is signed after the wrapping 32-bit addition.
  // Crossing INT32_MAX therefore makes the deadline negative and accepts a
  // still-positive entry tick, exactly as the original 386 CMP/JL does.
  state = {};
  state.last_tick = 0x7ffffffcU;
  step = simtower::step_original_simulation(
      state, 0x7fffffffU, false, false);
  assert(step.advanced && state.last_tick == 0x7ffffffcU);

  state = {};
  state.frame_time = 0x09f5;
  state.current_day = 0;
  step = simtower::step_original_simulation(state, 6, false, false);
  assert(state.frame_time == 0x09f6);
  assert(step.calls.back() ==
         OriginalSimulationCall({0x11c8, 0x0167, {0x1388, 4, 3}}));
  state.frame_time = 0x09f5;
  state.current_day = 4;
  state.last_tick = 0;
  step = simtower::step_original_simulation(state, 6, false, false);
  assert(step.calls.back() ==
         OriginalSimulationCall({0x11c8, 0x0167, {0x1389, 1, 3}}));

  state = {};
  state.frame_time = 0x0a27;
  step = simtower::step_original_simulation(state, 6, false, false);
  assert(state.frame_time == 0);
  assert(step.calls.size() == 6);
  assert(step.calls.front() == OriginalSimulationCall({0x1228, 0x0968, {}}));
  assert(step.calls.back() == OriginalSimulationCall({0x1020, 0x0dcb, {}}));

  state = {};
  state.frame_time = 0x004f;
  step = simtower::step_original_simulation(state, 6, false, true);
  assert(step.calls == std::vector<OriginalSimulationCall>({
      {0x11c8, 0x0167, {0x138d, 2, 3}}}));

  // The integrated host boundary must read revision-aware b406 itself. It
  // also applies the two direct 1200:0196 writes that are not far calls.
  auto integrated_tower = simtower::make_original_new_tdt();
  store_test_header_word(integrated_tower, 60U, 0x10U);
  state = {};
  state.frame_time = 0x004f;
  step = simtower::step_original_simulation(
      state, integrated_tower, 6, false);
  assert(step.calls == std::vector<OriginalSimulationCall>({
      {0x11c8, 0x0167, {0x138d, 2, 3}}}));
  assert(integrated_tower.header.frame_time == 0x0050U);

  integrated_tower.header.version_20_word = 7U;
  state = {};
  state.frame_time = 0x0a27U;
  step = simtower::step_original_simulation(
      state, integrated_tower, 6, false);
  assert(step.advanced && state.frame_time == 0U);
  assert(integrated_tower.header.version_20_word == 0U);

  integrated_tower.hotel_checkout_count = 19U;
  integrated_tower.hotel_checkout_effect_cadence = true;
  integrated_tower.hotel_checkout_effect_active = true;
  state = {};
  state.frame_time = 0x04afU;
  step = simtower::step_original_simulation(
      state, integrated_tower, 6, false);
  assert(step.advanced && state.frame_time == 0x04b0U);
  assert(integrated_tower.hotel_checkout_count == 0U);
  assert(integrated_tower.hotel_checkout_effect_cadence);
  assert(integrated_tower.hotel_checkout_effect_active);

  state = {};
  state.frame_time = 0x00ef;
  state.current_day = 419;
  step = simtower::step_original_simulation(state, 6, false, false);
  assert(has_call(step, 0x10e8, 0x0029));
  assert(has_call(step, 0x10c8, 0x006e));

  state = {};
  state.frame_time = 0x07cf;
  state.current_day = 11;
  step = simtower::step_original_simulation(state, 6, false, false);
  assert(has_call(step, 0x11b8, 0x0028));

  state = {};
  state.frame_time = 0x08fb;
  state.current_day = 11;
  step = simtower::step_original_simulation(state, 6, false, false);
  assert(step.day_changed);
  assert(state.current_day == 12);
  assert(state.calendar_phase == 0);

  state = {};
  state.frame_time = 0x08fb;
  state.current_day = 0x2ed3;
  step = simtower::step_original_simulation(state, 6, false, false);
  assert(state.current_day == 0);

  state = {};
  state.frame_time = 0x08fb;
  state.current_day = std::numeric_limits<std::int32_t>::max();
  step = simtower::step_original_simulation(state, 6, false, false);
  assert(step.day_changed &&
         state.current_day == std::numeric_limits<std::int32_t>::min() &&
         state.calendar_phase ==
             simtower::original_calendar_phase(state.current_day));

  {
    // Direct 1130:03f4/0630/069e coverage. Office averages six 1130:0360
    // signed quotients; rent values 0/1/2/other apply +5/0/-5/-12 before
    // 0630's amenity adjustment and zero clamp. The status/byte-14 gate
    // returns -1 before reading any person metrics.
    auto tower = simtower::make_original_new_tdt();
    tower.people.resize(8U);
    tower.people_count = 8U;
    const auto set_performance = [&](std::size_t index,
                                     std::int16_t numerator,
                                     std::int8_t divisor) {
      auto& exact = tower.people[index].exact_bytes;
      exact[9] = std::bit_cast<std::byte>(divisor);
      const auto word = std::bit_cast<std::uint16_t>(numerator);
      exact[14] = static_cast<std::byte>(word);
      exact[15] = static_cast<std::byte>(word >> 8U);
    };
    for (std::size_t index = 0U; index < 6U; ++index) {
      set_performance(index, 100, 2);
    }
    auto& floor = tower.floors[20];
    simtower::OriginalTdtTenant office{};
    office.left = 100U;
    office.right = 109U;
    office.type = 7;
    office.status = 0U;
    office.rent_rate = 1U;
    office.exact_bytes[4] = std::byte{7};
    office.exact_bytes[5] = std::byte{0};
    office.exact_bytes[16] = std::byte{1};
    floor.tenants = {office};
    assert(simtower::original_tenant_information_performance(
               tower, 20, 0U) == 50);
    floor.tenants[0].rent_rate = 0U;
    assert(simtower::original_tenant_information_performance(
               tower, 20, 0U) == 80);
    floor.tenants[0].rent_rate = 2U;
    assert(simtower::original_tenant_information_performance(
               tower, 20, 0U) == 20);
    floor.tenants[0].rent_rate = 3U;
    assert(simtower::original_tenant_information_performance(
               tower, 20, 0U) == 0);

    floor.tenants[0].rent_rate = 1U;
    simtower::OriginalTdtTenant amenity{};
    amenity.left = 111U;
    amenity.right = 112U;
    amenity.type = 6;
    amenity.exact_bytes[4] = std::byte{6};
    floor.tenants.push_back(amenity);
    assert(simtower::original_tenant_information_performance(
               tower, 20, 0U) == 110);
    floor.tenants[0].status = 0x10U;
    floor.tenants[0].exact_bytes[5] = std::byte{0x10};
    floor.tenants[0].exact_bytes[14] = std::byte{1};
    assert(simtower::original_tenant_information_performance(
               tower, 20, 0U) == -1);
    floor.tenants[0].exact_bytes[14] = std::byte{0};
    assert(simtower::original_tenant_information_performance(
               tower, 20, 0U) == 110);

    // Hotel types skip ordinal zero; type 3 evaluates only ordinal one.
    floor.tenants.resize(1U);
    auto& hotel = floor.tenants[0];
    hotel.type = 3;
    hotel.status = 0U;
    hotel.rent_rate = 1U;
    hotel.exact_bytes[4] = std::byte{3};
    hotel.exact_bytes[5] = std::byte{0};
    hotel.exact_bytes[14] = std::byte{0};
    set_performance(0U, 600, 2);
    set_performance(1U, 80, 2);
    assert(simtower::original_tenant_information_performance(
               tower, 20, 0U) == 40);
  }
  return 0;
}
