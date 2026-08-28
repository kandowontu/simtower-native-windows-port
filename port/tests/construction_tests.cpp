#include "original_construction.hpp"
#include "original_information.hpp"
#include "original_tdt.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

std::uint32_t load_u32(std::span<const std::byte> bytes,
                       std::size_t offset,
                       bool byte_swapped) {
  const auto byte = [&](std::size_t index) {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset + index]));
  };
  if (byte_swapped) {
    return (byte(0) << 24U) | (byte(1) << 16U) |
           (byte(2) << 8U) | byte(3);
  }
  return byte(0) | (byte(1) << 8U) |
         (byte(2) << 16U) | (byte(3) << 24U);
}

void store_u32(std::span<std::byte> bytes,
               std::size_t offset,
               std::uint32_t value,
               bool byte_swapped) {
  for (std::size_t index = 0U; index < 4U; ++index) {
    const auto shift = byte_swapped ? (3U - index) * 8U : index * 8U;
    bytes[offset + index] =
        static_cast<std::byte>((value >> shift) & 0xffU);
  }
}

simtower::OriginalYenTable construction_costs() {
  simtower::OriginalYenTable costs{};
  costs[0] = 5U;  // ordinary above/below-ground floor cells
  costs[1] = 2000U;  // standard elevator, exact YEN/1000 entry
  costs[42] = 6000U;  // express elevator, exact raw command entry
  costs[43] = 4000U;  // service elevator, exact raw command entry
  costs[3] = 200U;  // Single Hotel Room
  costs[4] = 500U;  // Twin Hotel Room
  costs[5] = 1000U;  // Hotel Suite
  costs[6] = 1200U;  // Restaurant
  costs[7] = 400U;  // Office
  costs[9] = 1600U;  // Condo
  costs[10] = 2400U;  // Retail Shop
  costs[11] = 300U;  // Parking segment
  costs[12] = 800U;  // Fast Food
  costs[13] = 3000U;  // Medical Center
  costs[14] = 500U;  // Security
  costs[15] = 500U;  // Housekeeping
  costs[17] = 1000U;  // SECOM Center
  costs[18] = 5000U;  // Movie Theater
  costs[20] = 5000U;  // Recycling Center
  costs[29] = 1000U;  // Party Hall
  costs[31] = 10000U;  // Metro Station
  costs[36] = 30000U;  // Cathedral
  costs[22] = 50U;  // Stairs
  costs[0x18] = 50U;  // exact YEN/1000 entry used by 1178:0583
  costs[27] = 200U;  // Escalator
  costs[0x2c] = 500U;  // Parking Ramp
  return costs;
}

simtower::OriginalPartTable part_table() {
  simtower::OriginalPartTable part{};
  part.words_52_to_ac[(0x90U - 0x52U) / 2U] = 800U;
  part.words_52_to_ac[(0x92U - 0x52U) / 2U] = 900U;
  part.words_52_to_ac[(0x94U - 0x52U) / 2U] = 700U;
  return part;
}

void assert_lobby(const simtower::OriginalTdtFloor& floor,
                  std::uint16_t left, std::uint16_t right) {
  assert(floor.left_edge == left);
  assert(floor.right_edge == right);
  assert(floor.tenants.size() == 1U);
  const auto& tenant = floor.tenants[0];
  assert(tenant.left == left);
  assert(tenant.right == right);
  assert(tenant.type == 0x18);
  assert(tenant.status == 0U);
  assert(tenant.variant == 0U);
  assert(tenant.preserved_07_to_0f[5] == std::byte{0});
  assert(tenant.preserved_07_to_0f[6] == std::byte{1});
  assert(tenant.preserved_07_to_0f[7] == std::byte{1});
  assert(tenant.preserved_07_to_0f[8] == std::byte{0xff});
  assert(tenant.rent_rate == 4U);
  assert(tenant.subtype == 0U);
  assert(floor.tenant_index[0] == 0U);
}

void assert_elevator(const simtower::OriginalTdtElevator& elevator,
                     std::uint8_t type,
                     std::uint8_t capacity,
                     std::uint16_t x,
                     std::uint8_t floor,
                     std::uint8_t cars) {
  assert(elevator.used == 1U);
  assert(elevator.type == type);
  assert(elevator.capacity == capacity);
  assert(elevator.cars == cars);
  for (std::size_t index = 0; index < 14U; ++index) {
    assert(elevator.schedule[index] == std::byte{1});
  }
  for (std::size_t index = 14U; index < 28U; ++index) {
    assert(elevator.schedule[index] == std::byte{5});
  }
  for (std::size_t index = 28U; index < elevator.schedule.size(); ++index) {
    assert(elevator.schedule[index] == std::byte{0});
  }
  assert(elevator.word_3c == 1U);
  assert(elevator.x == x);
  assert(elevator.top_floor == floor);
  assert(elevator.bottom_floor == floor);
  assert(elevator.serviced_floors[floor] == std::byte{1});
  assert(elevator.floor_records.size() == 1U);
  assert(elevator.floor_records[0].mapped_index ==
         simtower::original_elevator_floor_record_index(
             type, static_cast<std::int8_t>(floor),
             static_cast<std::int8_t>(floor), floor));
  assert(elevator.floor_records[0].floor == floor);
  for (std::size_t index = 0; index < elevator.car_records.size(); ++index) {
    const auto& exact = elevator.car_records[index].exact_bytes;
    assert(exact[0] == static_cast<std::byte>(floor));
    assert(exact[4] == std::byte{1});
    assert(exact[5] == static_cast<std::byte>(floor));
    assert(exact[6] == static_cast<std::byte>(floor));
    assert(exact[13] == static_cast<std::byte>(floor));
    assert(exact[15] == static_cast<std::byte>(index < cars ? 1 : 0));
    for (std::size_t byte = 16; byte < 226; ++byte) {
      assert(exact[byte] == std::byte{0xff});
    }
    for (std::size_t byte = 226; byte < exact.size(); ++byte) {
      assert(exact[byte] == std::byte{0});
    }
  }
}

void assert_standard_elevator(const simtower::OriginalTdtElevator& elevator,
                              std::uint16_t x,
                              std::uint8_t floor,
                              std::uint8_t cars) {
  assert_elevator(elevator, 1U, 0x15U, x, floor, cars);
}

void assert_pending_office(const simtower::OriginalTdtTenant& tenant,
                           std::uint16_t left,
                           std::uint8_t variant,
                           std::uint8_t key,
                           std::uint32_t people_start) {
  assert(tenant.left == left);
  assert(tenant.right == left + 9U);
  assert(tenant.type == -7);
  assert(tenant.status == 0U);
  assert(tenant.variant == variant);
  assert(tenant.exact_bytes[4] == std::byte{0xf9});
  assert(tenant.exact_bytes[5] == std::byte{0});
  assert(tenant.exact_bytes[6] == static_cast<std::byte>(variant));
  assert(tenant.exact_bytes[7] == std::byte{0});
  assert(tenant.exact_bytes[8] == static_cast<std::byte>(people_start));
  assert(tenant.exact_bytes[9] ==
         static_cast<std::byte>(people_start >> 8U));
  assert(tenant.exact_bytes[10] ==
         static_cast<std::byte>(people_start >> 16U));
  assert(tenant.exact_bytes[11] ==
         static_cast<std::byte>(people_start >> 24U));
  assert(tenant.exact_bytes[12] == static_cast<std::byte>(key));
  assert(tenant.exact_bytes[13] == std::byte{1});
  assert(tenant.exact_bytes[14] == std::byte{1});
  assert(tenant.exact_bytes[15] == std::byte{0xff});
  assert(tenant.rent_rate == 1U);
  assert(tenant.subtype == 0x0cU);
  assert(tenant.exact_bytes[16] == std::byte{1});
  assert(tenant.exact_bytes[17] == std::byte{0x0c});
}

void assert_pending_deferred_facility(
    const simtower::OriginalTdtTenant& tenant,
    std::uint8_t type,
    std::uint16_t left,
    std::uint16_t width,
    std::uint8_t variant,
    std::uint8_t key,
    std::uint32_t people_start) {
  assert(tenant.left == left);
  assert(tenant.right == left + width);
  assert(tenant.type == -static_cast<std::int8_t>(type));
  assert(tenant.status == 0U);
  assert(tenant.variant == variant);
  assert(tenant.exact_bytes[4] == static_cast<std::byte>(
      static_cast<std::uint8_t>(-static_cast<std::int16_t>(type))));
  assert(tenant.exact_bytes[12] == static_cast<std::byte>(key));
  assert(tenant.exact_bytes[13] == std::byte{1});
  assert(tenant.exact_bytes[14] == std::byte{1});
  assert(tenant.exact_bytes[15] == std::byte{0xff});
  const bool unit_rent_rate =
      (type >= 3U && type <= 5U) || type == 7U || type == 9U || type == 10U;
  const auto expected_rent_rate =
      static_cast<std::uint8_t>(unit_rent_rate ? 1U : 4U);
  assert(tenant.rent_rate == expected_rent_rate);
  assert(tenant.subtype == 0x0cU);
  assert(tenant.exact_bytes[16] == static_cast<std::byte>(expected_rent_rate));
  assert(tenant.exact_bytes[17] == std::byte{0x0c});
  const auto& exact = tenant.exact_bytes;
  const auto stored_start =
      static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(exact[8])) |
      (static_cast<std::uint32_t>(
           std::to_integer<std::uint8_t>(exact[9]))
       << 8U) |
      (static_cast<std::uint32_t>(
           std::to_integer<std::uint8_t>(exact[10]))
       << 16U) |
      (static_cast<std::uint32_t>(
           std::to_integer<std::uint8_t>(exact[11]))
       << 24U);
  assert(stored_start == people_start);
}

void assert_office_people(const simtower::OriginalTdtDocument& tower,
                          std::size_t start,
                          std::uint8_t floor,
                          std::uint8_t key) {
  for (std::size_t index = 0; index < 6U; ++index) {
    const auto& exact = tower.people[start + index].exact_bytes;
    assert(exact[0] == static_cast<std::byte>(floor));
    assert(exact[1] == static_cast<std::byte>(key));
    assert(exact[2] == static_cast<std::byte>(index));
    assert(exact[3] == std::byte{0});
    assert(exact[4] == std::byte{0xf9});
    for (std::size_t byte = 5; byte < exact.size(); ++byte) {
      assert(exact[byte] == std::byte{0});
    }
  }
}

simtower::OriginalTdtDocument make_metro_ready_tower(
    const simtower::OriginalYenTable& costs) {
  auto tower = simtower::make_original_new_tdt();
  assert(simtower::build_original_initial_lobby(
             tower, 100, 200, 1, costs)
             .succeeded());
  for (int floor = 9; floor >= 0; --floor) {
    assert(simtower::build_original_floor(
               tower, static_cast<std::int16_t>(floor), 120, 150, costs)
               .succeeded());
  }
  tower.header.frame_time = 0U;
  tower.header.balance = 30000;
  return tower;
}

simtower::OriginalTdtDocument make_cathedral_ready_tower(
    const simtower::OriginalYenTable& costs) {
  auto tower = simtower::make_original_new_tdt();
  assert(simtower::build_original_initial_lobby(
             tower, 100, 200, 1, costs)
             .succeeded());
  tower.header.balance = 1000000;
  for (int floor = 11; floor <= 108; ++floor) {
    assert(simtower::build_original_floor(
               tower, static_cast<std::int16_t>(floor), 120, 148, costs)
               .succeeded());
  }
  tower.header.frame_time = 0U;
  tower.header.balance = 50000;
  return tower;
}

simtower::OriginalTdtDocument make_sky_lobby_ready_tower(
    const simtower::OriginalYenTable& costs) {
  auto tower = simtower::make_original_new_tdt();
  assert(simtower::build_original_initial_lobby(
             tower, 100, 200, 1, costs)
             .succeeded());
  tower.header.balance = 1000000;
  for (int floor = 11; floor <= 23; ++floor) {
    assert(simtower::build_original_floor(
               tower, static_cast<std::int16_t>(floor), 100, 200, costs)
               .succeeded());
  }
  return tower;
}

}  // namespace

int main() {
  const auto costs = construction_costs();
  const auto part = part_table();

  // Direct 10a0:07b7 pointer-transform coverage: the initial 816x576 view is
  // 1092,3420. A pointer at (204,520) becomes world (1280,3924) after the
  // lobby's 16-pixel centering and 8x36 snap.
  assert(simtower::original_lobby_placement_from_client(
             204, 520, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({10, 160, 164}));
  assert(simtower::original_lobby_placement_from_client(
             196, 484, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 159, 163}));
  assert(simtower::original_floor_placement_from_client(
             192, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 161}));
  assert(simtower::original_office_placement_from_client(
             224, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 169}));
  assert(simtower::original_facility_placement_from_client(
             3, 204, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 164}));
  assert(simtower::original_facility_placement_from_client(
             4, 212, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 166}));
  assert(simtower::original_facility_placement_from_client(
             5, 228, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 170}));
  assert(simtower::original_facility_placement_from_client(
             9, 252, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 176}));
  assert(simtower::original_facility_placement_from_client(
             10, 236, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 172}));
  assert(simtower::original_facility_placement_from_client(
             6, 284, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 184}));
  assert(simtower::original_facility_placement_from_client(
             12, 252, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 176}));
  assert(simtower::original_facility_placement_from_client(
             14, 252, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 176}));
  assert(simtower::original_facility_placement_from_client(
             13, 292, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 186}));
  assert(simtower::original_facility_placement_from_client(
             22, 220, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 168}));
  assert(simtower::original_facility_placement_from_client(
             27, 220, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 168}));
  assert(simtower::original_facility_placement_from_client(
             1, 204, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 164}));
  assert(simtower::original_facility_placement_from_client(
             42, 212, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 166}));
  assert(simtower::original_facility_placement_from_client(
             43, 204, 468, 1092, 3420) ==
         simtower::OriginalLobbyPlacement({11, 160, 164}));
  // 11f8:3df4 distinguishes only Lobby. Every non-Lobby transform adds 12
  // after the vertical snap; at negative coordinates signed IDIV makes this
  // change the selected floor, while Lobby retains the unshifted quotient.
  assert(simtower::original_lobby_placement_from_client(
             16, -36, 0, 0) ==
         simtower::OriginalLobbyPlacement({120, 0, 4}));
  assert(simtower::original_floor_placement_from_client(
             4, -36, 0, 0) ==
         simtower::OriginalLobbyPlacement({119, 0, 1}));
  assert(simtower::original_office_placement_from_client(
             36, -36, 0, 0) ==
         simtower::OriginalLobbyPlacement({119, 0, 9}));
  assert(simtower::original_facility_placement_from_client(
             3, 16, -36, 0, 0) ==
         simtower::OriginalLobbyPlacement({119, 0, 4}));
  assert(simtower::original_facility_width_cells(1) == 4U);
  assert(simtower::original_facility_width_cells(42) == 6U);
  assert(simtower::original_facility_width_cells(43) == 4U);
  assert(simtower::original_facility_width_cells(3) == 4U);
  assert(simtower::original_facility_width_cells(7) == 9U);
  assert(simtower::original_facility_width_cells(15) == 15U);
  assert(simtower::original_facility_width_cells(18) == 24U);
  assert(simtower::original_facility_width_cells(19) == 24U);
  assert(simtower::original_facility_width_cells(20) == 25U);
  assert(simtower::original_facility_width_cells(21) == 25U);
  assert(simtower::original_facility_width_cells(29) == 24U);
  assert(simtower::original_facility_width_cells(30) == 24U);
  assert(simtower::original_facility_width_cells(31) == 30U);
  assert(simtower::original_facility_width_cells(32) == 30U);
  assert(simtower::original_facility_width_cells(33) == 30U);
  assert(simtower::original_facility_width_cells(36) == 28U);
  assert(simtower::original_facility_width_cells(37) == 28U);
  assert(simtower::original_facility_width_cells(38) == 28U);
  assert(simtower::original_facility_width_cells(39) == 28U);
  assert(simtower::original_facility_width_cells(40) == 28U);
  assert(simtower::original_facility_width_cells(22) == 8U);
  assert(simtower::original_facility_width_cells(27) == 8U);
  assert(simtower::original_facility_width_cells(0x2c) == 16U);
  assert(simtower::original_facility_width_cells(2) == 0U);

  {
    auto tower = simtower::make_original_new_tdt();
    const auto result = simtower::build_original_initial_lobby(
        tower, 100, 200, 1, costs);
    assert(result.succeeded());
    assert(result.cost == 5000);
    assert(tower.header.lobby_height == 1U);
    assert(tower.header.balance == 15000);
    assert(tower.header.construction_costs == -5000);
    assert_lobby(tower.floors[10], 100, 200);
    assert(tower.floors[11].tenants.empty());

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.header.lobby_height == 1U);
    assert(reparsed.header.balance == 15000);
    assert(reparsed.header.construction_costs == -5000);
    assert_lobby(reparsed.floors[10], 100, 200);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Direct 1198:0beb coverage: the floor-local admission scan compares the
    // Ramp's raw type byte only. A nonzero runtime status must not make the
    // existing type-0x2c record invisible to Parking construction.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    assert(simtower::build_original_parking_ramp(
               tower, 9, 120, costs)
               .succeeded());
    auto& ramp = tower.floors[9].tenants[0];
    assert(ramp.type == 0x2c);
    ramp.status = 0x80U;
    ramp.exact_bytes[5] = std::byte{0x80};
    const auto result = simtower::build_original_parking(
        tower, 9, 116, costs);
    assert(result.succeeded());
    assert(tower.floors[9].tenants[0].type == 11);
    assert(tower.floors[9].tenants[1].type == 0x2c);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    // Direct 1220:0d13 Movie-family initializer coverage: both deferred halves
    // reserve 56 records and activation rewrites them to family 18/state 27.
    const auto result = simtower::build_original_movie_theater(
        tower, 9, 120, costs);
    assert(result.succeeded());
    assert(result.cost == 5240);
    assert(tower.header.exact_bytes[54] == std::byte{1});
    assert(tower.header.exact_bytes[55] == std::byte{0});
    assert(tower.post_elevator.b92e_counter == 2U);
    assert_pending_deferred_facility(
        tower.floors[9].tenants[0], 18, 120, 24, 0, 0, 0);
    assert_pending_deferred_facility(
        tower.floors[8].tenants[0], 19, 120, 24, 0, 0, 56);
    for (std::size_t index = 0; index < 112U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      const bool upper = index < 56U;
      const auto ordinal = upper ? index : index - 56U;
      assert(exact[0] == (upper ? std::byte{9} : std::byte{8}));
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(ordinal));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == (upper ? std::byte{0xee} : std::byte{0xed}));
      for (std::size_t byte = 5; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }

    // Direct 11f0:0211/1180:0352 coverage: the queue decrements both retained
    // countdown bytes, dirties each activated tenant, and stops/restarts its
    // scan around the first zero while each pending Movie half becomes a seven-cell
    // entrance plus a 24-cell body while preserving the exact person/link
    // initialization and reverse record-shift behavior.
    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    assert(tower.floors[9].tenants.size() == 2U);
    assert(tower.floors[9].tenants[0].type == 34);
    assert(tower.floors[8].tenants[0].type == -19);
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    assert(tower.post_elevator.b92e_counter == 0U);

    for (std::uint8_t floor_number : {std::uint8_t{9}, std::uint8_t{8}}) {
      const auto& floor = tower.floors[floor_number];
      assert(floor.tenants.size() == 2U);
      const bool upper = floor_number == 9U;
      const auto& entrance = floor.tenants[0];
      const auto& body = floor.tenants[1];
      assert(entrance.left == 120U && entrance.right == 127U);
      assert(entrance.type == (upper ? 34 : 35));
      assert(entrance.status == 0U && entrance.variant == 0U);
      assert(entrance.exact_bytes[12] == std::byte{0});
      assert(entrance.exact_bytes[13] == std::byte{1});
      assert(entrance.exact_bytes[14] == std::byte{1});
      assert(entrance.exact_bytes[15] == std::byte{2});
      assert(entrance.rent_rate == 4U && entrance.subtype == 0U);
      assert(body.left == 127U && body.right == 151U);
      assert(body.type == (upper ? 18 : 19));
      assert(body.status == 0U && body.variant == 0U);
      assert(body.exact_bytes[12] == std::byte{1});
      assert(body.exact_bytes[13] == std::byte{1});
      assert(body.exact_bytes[14] == std::byte{1});
      assert(body.exact_bytes[15] == std::byte{2});
      assert(body.rent_rate == 4U && body.subtype == 0U);
      assert(floor.tenant_index[0] == 0U);
      assert(floor.tenant_index[1] == 1U);
    }

    for (std::size_t index = 0; index < 112U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      const bool upper = index < 56U;
      const auto ordinal = upper ? index : index - 56U;
      assert(exact[0] == (upper ? std::byte{9} : std::byte{8}));
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(ordinal));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == std::byte{18});
      assert(exact[5] == std::byte{0x27});
      for (std::size_t byte = 6; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }

    // Direct 1180:01ad/0282 basement linkage: upper activation allocates the
    // slot, lower activation finds the adjacent same-x upper half and fills
    // the remaining floor/key lane.
    const auto& service = tower.post_elevator.dc24_records[0];
    assert(service[0] == std::byte{9});
    assert(service[1] == std::byte{8});
    assert(service[2] == std::byte{0});
    assert(service[3] == std::byte{0});
    assert(service[4] == std::byte{0});
    assert(service[5] == std::byte{0});
    assert(service[6] == std::byte{0});
    // First MS C rand() result from seed one is 346; 346 % 14 == 10.
    assert(service[7] == std::byte{10});
    for (std::size_t byte = 8; byte < service.size(); ++byte) {
      assert(service[byte] == std::byte{0});
    }
    for (std::size_t index = 1;
         index < tower.post_elevator.dc24_records.size(); ++index) {
      assert(tower.post_elevator.dc24_records[index][0] == std::byte{0xfe});
    }

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.floors[9].tenants[0].type == 34);
    assert(reparsed.floors[9].tenants[1].type == 18);
    assert(reparsed.floors[8].tenants[0].type == 35);
    assert(reparsed.floors[8].tenants[1].type == 19);
    assert(reparsed.post_elevator.dc24_records[0] == service);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    auto capped = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               capped, 100, 200, 1, costs)
               .succeeded());
    capped.header.exact_bytes[54] = std::byte{0x10};
    capped.header.exact_bytes[55] = std::byte{0};
    const auto capped_before = simtower::serialize_original_tdt(capped);
    auto result = simtower::build_original_movie_theater(
        capped, 9, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::tenant_limit);
    assert(simtower::serialize_original_tdt(capped) == capped_before);

    auto poor = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               poor, 100, 200, 1, costs)
               .succeeded());
    poor.header.balance = 5239;
    const auto poor_before = simtower::serialize_original_tdt(poor);
    result = simtower::build_original_movie_theater(
        poor, 9, 120, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::insufficient_funds);
    assert(result.cost == 5240);
    assert(result.construction_status_code == 8U);
    assert(simtower::serialize_original_tdt(poor) == poor_before);

    auto invalid = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               invalid, 100, 200, 1, costs)
               .succeeded());
    result = simtower::build_original_movie_theater(
        invalid, 10, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    result = simtower::build_original_movie_theater(
        invalid, 11, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    result = simtower::build_original_movie_theater(
        invalid, 12, 120, costs);
    assert(result.succeeded());
    assert(result.cost == 5240);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());

    // Direct 1178:004e/027c coverage: the two-floor composite validates its
    // aligned pair, sums both facility charges plus newly exposed floor cells,
    // then applies one wrapping debit to balance and construction costs.
    const auto result = simtower::build_original_recycling_center(
        tower, 9, 120, costs);
    assert(result.succeeded());
    assert(result.cost == 5250);
    assert(tower.header.balance == 9750);
    assert(tower.header.construction_costs == -10250);
    assert(tower.header.exact_bytes[42] == std::byte{1});
    assert(tower.header.exact_bytes[43] == std::byte{0});
    assert(tower.post_elevator.b92e_counter == 2U);
    // 11f8:1fa5 constructs the selected upper type-20 half first below
    // ground, followed by the lower type-21 half.
    assert_pending_deferred_facility(
        tower.floors[9].tenants[0], 20, 120, 25, 0, 0, 0);
    assert_pending_deferred_facility(
        tower.floors[8].tenants[0], 21, 120, 25, 0, 0, 6);
    for (std::size_t index = 0; index < 12U; ++index) {
      const bool upper = index < 6U;
      const auto ordinal = upper ? index : index - 6U;
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == (upper ? std::byte{9} : std::byte{8}));
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(ordinal));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == (upper ? std::byte{0xec} : std::byte{0xeb}));
      for (std::size_t byte = 5; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }

    // Persisted b3f4 is already one, but 1088:02c8 only recognizes positive
    // activated halves. A second pair is therefore rejected byte-atomically
    // while the first pair remains in the deferred queue.
    const auto pending_bytes = simtower::serialize_original_tdt(tower);
    auto adjacent = simtower::build_original_recycling_center(
        tower, 7, 120, costs);
    assert(adjacent.status ==
           simtower::OriginalConstructionStatus::invalid_span);
    assert(simtower::serialize_original_tdt(tower) == pending_bytes);

    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    assert(tower.floors[9].tenants[0].type == 20);
    assert(tower.floors[8].tenants[0].type == -21);
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    assert(tower.post_elevator.b92e_counter == 0U);

    for (const std::uint8_t floor_number :
         {std::uint8_t{9}, std::uint8_t{8}}) {
      const auto& half = tower.floors[floor_number].tenants[0];
      assert(half.left == 120U && half.right == 145U);
      assert(half.type == (floor_number == 9U ? 20 : 21));
      assert(half.status == 0U && half.variant == 0U);
      assert(half.exact_bytes[12] == std::byte{0xff});
      assert(half.exact_bytes[13] == std::byte{1});
      assert(half.exact_bytes[14] == std::byte{1});
      assert(half.exact_bytes[15] == std::byte{0xff});
      assert(half.rent_rate == 4U && half.subtype == 0U);
      // 1228:0e30 does not clear the now-stale lookup entry.
      assert(tower.floors[floor_number].tenant_index[0] == 0U);
    }
    // The type-20/type-21 1228:075b activation branch deliberately leaves
    // all twelve reservation records negative and otherwise byte-exact.
    for (std::size_t index = 0; index < 12U; ++index) {
      const bool upper = index < 6U;
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[4] == (upper ? std::byte{0xec} : std::byte{0xeb}));
      for (std::size_t byte = 5; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }

    const auto activated_bytes = simtower::serialize_original_tdt(tower);
    auto misaligned = simtower::build_original_recycling_center(
        tower, 7, 121, costs);
    assert(misaligned.status ==
           simtower::OriginalConstructionStatus::invalid_span);
    assert(simtower::serialize_original_tdt(tower) == activated_bytes);
    auto too_far = simtower::build_original_recycling_center(
        tower, 5, 120, costs);
    assert(too_far.status ==
           simtower::OriginalConstructionStatus::invalid_span);
    assert(simtower::serialize_original_tdt(tower) == activated_bytes);

    adjacent = simtower::build_original_recycling_center(
        tower, 7, 120, costs);
    assert(adjacent.succeeded());
    assert(adjacent.cost == 5250);
    assert(tower.header.exact_bytes[42] == std::byte{2});
    assert(tower.floors[7].tenants[0].type == -20);
    assert(tower.floors[6].tenants[0].type == -21);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.header.exact_bytes[42] == std::byte{2});
    assert(reparsed.floors[9].tenants[0].type == 20);
    assert(reparsed.floors[8].tenants[0].type == 21);
    assert(reparsed.floors[7].tenants[0].type == -20);
    assert(reparsed.floors[6].tenants[0].type == -21);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    auto poor = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               poor, 100, 200, 1, costs)
               .succeeded());
    poor.header.balance = 5249;
    const auto before = simtower::serialize_original_tdt(poor);
    auto result = simtower::build_original_recycling_center(
        poor, 9, 120, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::insufficient_funds);
    assert(result.cost == 5250);
    assert(result.construction_status_code == 8U);
    assert(simtower::serialize_original_tdt(poor) == before);
    result = simtower::build_original_recycling_center(
        poor, 10, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    assert(result.construction_status_code == 12U);
    result = simtower::build_original_recycling_center(
        poor, 0, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    assert(result.construction_status_code == 20U);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());

    const auto result = simtower::build_original_party_hall(
        tower, 9, 120, costs);
    assert(result.succeeded());
    assert(result.cost == 1240);
    assert(tower.header.exact_bytes[54] == std::byte{1});
    assert(tower.header.exact_bytes[55] == std::byte{0});
    assert(tower.post_elevator.b92e_counter == 2U);
    // The basement branch of 11f8:1fa5 creates the selected upper type-29
    // half before the lower type-30 half.
    assert_pending_deferred_facility(
        tower.floors[9].tenants[0], 29, 120, 24, 0, 0, 0);
    assert_pending_deferred_facility(
        tower.floors[8].tenants[0], 30, 120, 24, 0, 0, 40);
    for (std::size_t index = 0; index < 80U; ++index) {
      const bool upper = index < 40U;
      const auto ordinal = upper ? index : index - 40U;
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == (upper ? std::byte{9} : std::byte{8}));
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(ordinal));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == (upper ? std::byte{0xe3} : std::byte{0xe2}));
      for (std::size_t byte = 5; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }

    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    assert(tower.floors[9].tenants[0].type == 29);
    assert(tower.floors[8].tenants[0].type == -30);
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    assert(tower.post_elevator.b92e_counter == 0U);

    for (const std::uint8_t floor_number :
         {std::uint8_t{9}, std::uint8_t{8}}) {
      const auto& half = tower.floors[floor_number].tenants[0];
      assert(half.left == 120U && half.right == 144U);
      assert(half.type == (floor_number == 9U ? 29 : 30));
      assert(half.status == 0U && half.variant == 0U);
      assert(half.exact_bytes[6] == std::byte{0});
      assert(half.exact_bytes[7] == std::byte{0});
      assert(half.exact_bytes[12] == std::byte{0});
      assert(half.exact_bytes[13] == std::byte{1});
      assert(half.exact_bytes[14] == std::byte{1});
      assert(half.exact_bytes[15] == std::byte{0xff});
      assert(half.rent_rate == 4U && half.subtype == 0U);
      assert(tower.floors[floor_number].tenant_index[0] == 0U);
    }
    // 1220:0d3a passes type 29/status 0x27 for both forty-person halves.
    for (std::size_t index = 0; index < 80U; ++index) {
      const bool upper = index < 40U;
      const auto ordinal = upper ? index : index - 40U;
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == (upper ? std::byte{9} : std::byte{8}));
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(ordinal));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == std::byte{29});
      assert(exact[5] == std::byte{0x27});
      for (std::size_t byte = 6; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }

    const auto& service = tower.post_elevator.dc24_records[0];
    assert(service[0] == std::byte{9});
    assert(service[1] == std::byte{8});
    assert(service[2] == std::byte{0});
    assert(service[3] == std::byte{0});
    for (std::size_t byte = 4; byte < 7; ++byte) {
      assert(service[byte] == std::byte{0});
    }
    // 1180:0073 randomizes byte seven only for transformed cinema entrances.
    assert(service[7] == std::byte{0xff});
    for (std::size_t byte = 8; byte < service.size(); ++byte) {
      assert(service[byte] == std::byte{0});
    }

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.header.exact_bytes[54] == std::byte{1});
    assert(reparsed.floors[9].tenants[0].type == 29);
    assert(reparsed.floors[8].tenants[0].type == 30);
    assert(reparsed.post_elevator.dc24_records[0] == service);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    auto capped = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               capped, 100, 200, 1, costs)
               .succeeded());
    capped.header.exact_bytes[54] = std::byte{0x10};
    capped.header.exact_bytes[55] = std::byte{0};
    const auto capped_before = simtower::serialize_original_tdt(capped);
    auto result = simtower::build_original_party_hall(
        capped, 9, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::tenant_limit);
    assert(simtower::serialize_original_tdt(capped) == capped_before);

    auto poor = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               poor, 100, 200, 1, costs)
               .succeeded());
    poor.header.balance = 1239;
    const auto poor_before = simtower::serialize_original_tdt(poor);
    result = simtower::build_original_party_hall(poor, 9, 120, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::insufficient_funds);
    assert(result.cost == 1240);
    assert(result.construction_status_code == 8U);
    assert(simtower::serialize_original_tdt(poor) == poor_before);

    auto floors = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               floors, 100, 200, 1, costs)
               .succeeded());
    result = simtower::build_original_party_hall(floors, 0, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    result = simtower::build_original_party_hall(floors, 10, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    result = simtower::build_original_party_hall(floors, 11, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    result = simtower::build_original_party_hall(floors, 12, 120, costs);
    assert(result.succeeded());
    assert(result.cost == 1240);
    // Above ground the shared constructor queues the lower half first.
    assert(floors.floors[11].tenants[0].type == -30);
    assert(floors.floors[12].tenants[0].type == -29);
    // Above ground reverses activation order, directly covering 0282's
    // lower-only allocation fallback and 01ad's same-x upper join path.
    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(floors) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(floors) ==
           simtower::OriginalPendingStepStatus::activated);
    assert(simtower::step_original_pending_construction(floors) ==
           simtower::OriginalPendingStepStatus::activated);
    const auto& above_service = floors.post_elevator.dc24_records[0];
    assert(above_service[0] == std::byte{12});
    assert(above_service[1] == std::byte{11});
    assert(above_service[2] == std::byte{0});
    assert(above_service[3] == std::byte{0});

    auto shared = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               shared, 100, 200, 1, costs)
               .succeeded());
    assert(simtower::build_original_movie_theater(
               shared, 9, 120, costs)
               .succeeded());
    assert(simtower::build_original_party_hall(
               shared, 7, 120, costs)
               .succeeded());
    assert(shared.header.exact_bytes[54] == std::byte{2});
    assert(shared.header.exact_bytes[55] == std::byte{0});
  }

  {
    // Direct 11f8:2f5a coverage for both decoded lookup tables. Only raw
    // types 1/24/42/43 bypass the initial ground-floor status 12 branch, so a
    // malformed zero-height Lobby cannot make Movie's type-19 lower half legal
    // on floor 10.
    auto malformed = simtower::make_original_new_tdt();
    const auto before = simtower::serialize_original_tdt(malformed);
    const auto ground_movie = simtower::build_original_movie_theater(
        malformed, 11, 120, costs);
    assert(ground_movie.status ==
           simtower::OriginalConstructionStatus::invalid_floor);
    assert(ground_movie.construction_status_code == 12U);
    assert(simtower::serialize_original_tdt(malformed) == before);

    // With persisted b3e8 at floor two, the common extension gate admits
    // floor one, but the original's second table deliberately rejects the
    // selected upper types 18, 20, and 29 there with status 14. Each paired
    // constructor works on a copy, so the rejection remains byte-atomic.
    auto deepest = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               deepest, 100, 200, 1, costs).succeeded());
    deepest.header.exact_bytes[30] = std::byte{2};
    deepest.header.exact_bytes[31] = std::byte{0};
    const auto deepest_before = simtower::serialize_original_tdt(deepest);

    auto movie = deepest;
    auto result = simtower::build_original_movie_theater(
        movie, 1, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    assert(result.construction_status_code == 14U);
    assert(simtower::serialize_original_tdt(movie) == deepest_before);

    auto recycling = deepest;
    result = simtower::build_original_recycling_center(
        recycling, 1, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    assert(result.construction_status_code == 14U);
    assert(simtower::serialize_original_tdt(recycling) == deepest_before);

    auto party = deepest;
    result = simtower::build_original_party_hall(party, 1, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    assert(result.construction_status_code == 14U);
    assert(simtower::serialize_original_tdt(party) == deepest_before);
  }

  {
    // Direct 11f8:20e7 coverage: the Metro constructor preflights and emits
    // its type-31/32/33 stack top-to-bottom with one shared phase variant.
    auto tower = make_metro_ready_tower(costs);
    const auto result = simtower::build_original_metro_station(
        tower, 2, 120, costs);
    assert(result.succeeded());
    assert(result.cost == 10150);
    assert(tower.header.balance == 19850);
    assert(tower.header.exact_bytes[30] == std::byte{2});
    assert(tower.header.exact_bytes[31] == std::byte{0});
    assert(tower.post_elevator.b92e_counter == 3U);
    assert_pending_deferred_facility(
        tower.floors[2].tenants[0], 31, 120, 30, 0, 0, 0);
    assert_pending_deferred_facility(
        tower.floors[1].tenants[0], 32, 120, 30, 0, 0, 6);
    assert_pending_deferred_facility(
        tower.floors[0].tenants[0], 33, 120, 30, 0, 0, 12);
    for (std::size_t index = 0; index < 252U; ++index) {
      const std::size_t family = index < 6U ? 0U : (index < 12U ? 1U : 2U);
      const auto ordinal = family == 0U ? index
                           : family == 1U ? index - 6U
                                          : index - 12U;
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == static_cast<std::byte>(2U - family));
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(ordinal));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == static_cast<std::byte>(0xe1U - family));
      for (std::size_t byte = 5; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }

    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    assert(tower.floors[2].tenants[0].type == 31);
    assert(tower.floors[1].tenants[0].type == -32);
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    assert(tower.floors[1].tenants[0].type == 32);
    assert(tower.floors[0].tenants[0].type == -33);
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    assert(tower.post_elevator.b92e_counter == 0U);

    for (const std::uint8_t floor_number :
         {std::uint8_t{2}, std::uint8_t{1}, std::uint8_t{0}}) {
      const auto& part = tower.floors[floor_number].tenants[
          floor_number == 0U ? 1U : 0U];
      assert(part.left == 120U && part.right == 150U);
      assert(part.type == 33 - static_cast<int>(floor_number));
      assert(part.status == 0U && part.variant == 0U);
      assert(part.exact_bytes[12] ==
             (floor_number == 0U ? std::byte{0} : std::byte{0xff}));
      assert(part.exact_bytes[13] == std::byte{1});
      assert(part.exact_bytes[14] == std::byte{1});
      assert(part.exact_bytes[15] == std::byte{0xff});
      assert(part.rent_rate == 4U && part.subtype == 0U);
    }
    // 11e8:0000 surrounds the activated type-33 bottom with invisible
    // type-45 boundary records and expands floor zero to the 0..0x177 span.
    const auto& metro_floor = tower.floors[0];
    assert(metro_floor.tenants.size() == 3U);
    assert(metro_floor.left_edge == 0U && metro_floor.right_edge == 0x0177U);
    assert(metro_floor.tenant_index[0] == 1U);
    const auto& left_boundary = metro_floor.tenants[0];
    const auto& metro_bottom = metro_floor.tenants[1];
    const auto& right_boundary = metro_floor.tenants[2];
    for (const auto* boundary : {&left_boundary, &right_boundary}) {
      assert(boundary->type == 45 && boundary->status == 0U);
      assert(boundary->exact_bytes[12] == std::byte{0xff});
      assert(boundary->exact_bytes[13] == std::byte{1});
      assert(boundary->exact_bytes[14] == std::byte{1});
      assert(boundary->exact_bytes[15] == std::byte{2});
      assert(boundary->rent_rate == 4U && boundary->subtype == 0U);
    }
    assert(left_boundary.left == 0U &&
           left_boundary.right == metro_bottom.left);
    assert(right_boundary.left == metro_bottom.right &&
           right_boundary.right == 0x0177U);
    assert(load_u32(left_boundary.exact_bytes, 8U, false) == 12U);
    assert(load_u32(right_boundary.exact_bytes, 8U, false) == 0U);
    // Types 31/32 take 1228:075b and keep their six negative reservations.
    for (std::size_t index = 0; index < 12U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[4] ==
             (index < 6U ? std::byte{0xe1} : std::byte{0xe0}));
      for (std::size_t byte = 5; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }
    // 1220:0d88 initializes all 240 bottom passengers as 21/01/FE/00.
    for (std::size_t index = 0; index < 240U; ++index) {
      const auto& exact = tower.people[12U + index].exact_bytes;
      assert(exact[0] == std::byte{0});
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(index));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == std::byte{33});
      assert(exact[5] == std::byte{1});
      assert(exact[6] == std::byte{0xfe});
      assert(exact[7] == std::byte{0});
      for (std::size_t byte = 8; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.header.exact_bytes[30] == std::byte{2});
    assert(reparsed.floors[2].tenants[0].type == 31);
    assert(reparsed.floors[1].tenants[0].type == 32);
    assert(reparsed.floors[0].tenants.size() == 3U);
    assert(reparsed.floors[0].tenants[0].type == 45);
    assert(reparsed.floors[0].tenants[1].type == 33);
    assert(reparsed.floors[0].tenants[2].type == 45);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);

    const auto built_before = simtower::serialize_original_tdt(tower);
    const auto duplicate = simtower::build_original_metro_station(
        tower, 2, 120, costs);
    assert(duplicate.status ==
           simtower::OriginalConstructionStatus::tenant_limit);
    assert(duplicate.construction_status_code == 17U);
    assert(simtower::serialize_original_tdt(tower) == built_before);
  }

  {
    auto invalid = make_metro_ready_tower(costs);
    const auto invalid_before = simtower::serialize_original_tdt(invalid);
    auto result = simtower::build_original_metro_station(
        invalid, 3, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    assert(result.construction_status_code == 15U);
    assert(simtower::serialize_original_tdt(invalid) == invalid_before);

    auto poor = make_metro_ready_tower(costs);
    poor.header.balance = 9999;
    result = simtower::build_original_metro_station(poor, 2, 120, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::insufficient_funds);
    assert(result.cost == 10150);
    assert(result.construction_status_code == 7U);
    // 11f8:3010 clears floor zero before 1178:011d checks funds.
    assert(poor.floors[0].tenants.empty());
    assert(poor.header.exact_bytes[30] == std::byte{0xff});
    assert(poor.header.exact_bytes[31] == std::byte{0xff});

    auto split_gate = make_metro_ready_tower(costs);
    split_gate.header.balance = 10000;
    result = simtower::build_original_metro_station(
        split_gate, 2, 120, costs);
    assert(result.succeeded() && result.cost == 10150);
    // 1178:011d compares the 10,000 facility and 150 floor previews to the
    // same starting balance independently, so the final debit may be below 0.
    assert(split_gate.header.balance == -150);

    auto late_phase = make_metro_ready_tower(costs);
    late_phase.header.frame_time = 1600U;
    result = simtower::build_original_metro_station(
        late_phase, 2, 120, costs);
    assert(result.succeeded());
    for (const std::uint8_t floor :
         {std::uint8_t{2}, std::uint8_t{1}, std::uint8_t{0}}) {
      assert(late_phase.floors[floor].tenants[0].variant == 1U);
    }

    auto wrapped_phase = make_metro_ready_tower(costs);
    wrapped_phase.header.frame_time = 0x8000U;
    result = simtower::build_original_metro_station(
        wrapped_phase, 2, 120, costs);
    assert(result.succeeded());
    for (const std::uint8_t floor :
         {std::uint8_t{2}, std::uint8_t{1}, std::uint8_t{0}}) {
      assert(wrapped_phase.floors[floor].tenants[0].variant == 0U);
    }
  }

  {
    // Direct 11f8:2291 and 1220:0d61 coverage: preflight the fixed five-floor
    // span, then emit type 40 at floor 109 through type 36 at floor 113 in
    // bottom-up queue order with eight family-36 people per part and one
    // shared phase variant.
    auto tower = make_cathedral_ready_tower(costs);
    const auto result = simtower::build_original_cathedral(
        tower, 113, 120, costs);
    assert(result.succeeded());
    assert(result.cost == 30700);
    assert(tower.header.balance == 19300);
    assert(tower.header.exact_bytes[34] == std::byte{113});
    assert(tower.header.exact_bytes[35] == std::byte{0});
    assert(tower.post_elevator.b92e_counter == 5U);

    for (std::size_t part = 0; part < 5U; ++part) {
      const auto floor = static_cast<std::uint8_t>(109U + part);
      const auto type = static_cast<std::uint8_t>(40U - part);
      assert(tower.floors[floor].tenants.size() == 1U);
      assert_pending_deferred_facility(
          tower.floors[floor].tenants[0], type, 120, 28, 0, 0,
          static_cast<std::uint32_t>(part * 8U));
      for (std::size_t ordinal = 0; ordinal < 8U; ++ordinal) {
        const auto& exact = tower.people[part * 8U + ordinal].exact_bytes;
        assert(exact[0] == static_cast<std::byte>(floor));
        assert(exact[1] == std::byte{0});
        assert(exact[2] == static_cast<std::byte>(ordinal));
        assert(exact[3] == std::byte{0});
        assert(exact[4] == static_cast<std::byte>(
            static_cast<std::uint8_t>(-static_cast<std::int16_t>(type))));
        for (std::size_t byte = 5; byte < exact.size(); ++byte) {
          assert(exact[byte] == std::byte{0});
        }
      }
    }

    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    for (std::size_t activation = 0; activation < 5U; ++activation) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::activated);
      assert(tower.floors[109U + activation].tenants[0].type ==
             static_cast<std::int8_t>(40 - activation));
      if (activation == 0U) {
        // Type 40 replaces the construction marker with its lookup key.
        assert(tower.header.exact_bytes[34] == std::byte{0});
        assert(tower.header.exact_bytes[35] == std::byte{0});
      }
    }
    assert(tower.post_elevator.b92e_counter == 0U);

    for (std::size_t part = 0; part < 5U; ++part) {
      const auto floor = static_cast<std::uint8_t>(109U + part);
      const auto type = static_cast<std::uint8_t>(40U - part);
      const auto& tenant = tower.floors[floor].tenants[0];
      assert(tenant.left == 120U && tenant.right == 148U);
      assert(tenant.type == static_cast<std::int8_t>(type));
      assert(tenant.status == 0U && tenant.variant == 0U);
      assert(tenant.exact_bytes[12] == std::byte{0});
      assert(tenant.exact_bytes[13] == std::byte{1});
      assert(tenant.exact_bytes[14] == std::byte{1});
      assert(tenant.exact_bytes[15] == std::byte{0xff});
      assert(tenant.rent_rate == 4U && tenant.subtype == 0U);

      for (std::size_t ordinal = 0; ordinal < 8U; ++ordinal) {
        const auto& exact = tower.people[part * 8U + ordinal].exact_bytes;
        assert(exact[0] == static_cast<std::byte>(floor));
        assert(exact[1] == std::byte{0});
        assert(exact[2] == static_cast<std::byte>(ordinal));
        assert(exact[3] == std::byte{0});
        assert(exact[4] == std::byte{36});
        assert(exact[5] == std::byte{0x27});
        assert(exact[6] == std::byte{0});
        assert(exact[7] == std::byte{0});
        for (std::size_t byte = 8; byte < exact.size(); ++byte) {
          assert(exact[byte] == std::byte{0});
        }
      }
    }

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.header.exact_bytes[34] == std::byte{0});
    for (std::size_t part = 0; part < 5U; ++part) {
      assert(reparsed.floors[109U + part].tenants[0].type ==
             static_cast<std::int8_t>(40 - part));
    }
    assert(simtower::serialize_original_tdt(reparsed) == bytes);

    const auto built_before = simtower::serialize_original_tdt(tower);
    const auto duplicate = simtower::build_original_cathedral(
        tower, 113, 120, costs);
    assert(duplicate.status ==
           simtower::OriginalConstructionStatus::tenant_limit);
    assert(duplicate.construction_status_code == 19U);
    assert(simtower::serialize_original_tdt(tower) == built_before);
  }

  {
    auto invalid = make_cathedral_ready_tower(costs);
    const auto invalid_before = simtower::serialize_original_tdt(invalid);
    auto result = simtower::build_original_cathedral(
        invalid, 112, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    assert(result.construction_status_code == 16U);
    assert(simtower::serialize_original_tdt(invalid) == invalid_before);

    auto unsupported = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               unsupported, 100, 200, 1, costs)
               .succeeded());
    unsupported.header.balance = 50000;
    const auto unsupported_before =
        simtower::serialize_original_tdt(unsupported);
    result = simtower::build_original_cathedral(
        unsupported, 113, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_span);
    assert(simtower::serialize_original_tdt(unsupported) ==
           unsupported_before);

    auto poor = make_cathedral_ready_tower(costs);
    poor.header.balance = 29999;
    const auto poor_before = simtower::serialize_original_tdt(poor);
    result = simtower::build_original_cathedral(poor, 113, 120, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::insufficient_funds);
    assert(result.cost == 30700);
    assert(result.construction_status_code == 7U);
    assert(simtower::serialize_original_tdt(poor) == poor_before);

    auto split_gate = make_cathedral_ready_tower(costs);
    split_gate.header.balance = 30000;
    result = simtower::build_original_cathedral(
        split_gate, 113, 120, costs);
    assert(result.succeeded() && result.cost == 30700);
    // 1178:011d compares the 30,000 facility and 700 floor previews to the
    // same starting balance independently, allowing the combined debit.
    assert(split_gate.header.balance == -700);

    // The caller's signed DS:b3a1 comparison selects variant one at phase
    // four and variant zero again for a high-bit frame producing a negative
    // signed phase.
    auto late_phase = make_cathedral_ready_tower(costs);
    late_phase.header.frame_time = 1600U;
    result = simtower::build_original_cathedral(
        late_phase, 113, 120, costs);
    assert(result.succeeded());
    for (std::uint8_t floor = 109U; floor <= 113U; ++floor) {
      assert(late_phase.floors[floor].tenants[0].variant == 1U);
    }

    auto wrapped_phase = make_cathedral_ready_tower(costs);
    wrapped_phase.header.frame_time = 0x8000U;
    result = simtower::build_original_cathedral(
        wrapped_phase, 113, 120, costs);
    assert(result.succeeded());
    for (std::uint8_t floor = 109U; floor <= 113U; ++floor) {
      assert(wrapped_phase.floors[floor].tenants[0].variant == 0U);
    }
  }

  {
    auto tower = make_cathedral_ready_tower(costs);
    assert(simtower::build_original_cathedral(
               tower, 113, 120, costs)
               .succeeded());
    // 1040:0000 calls 11f0:0016 before its floor scan, so a Cathedral still
    // in deferred construction is completed immediately at the daily reset.
    assert(simtower::reset_original_cathedral_for_day(tower) == 40U);
    assert(tower.post_elevator.b92e_counter == 0U);
    assert(tower.header.exact_bytes[34] == std::byte{0});
    for (std::size_t part = 0; part < 5U; ++part) {
      auto& tenant = tower.floors[109U + part].tenants[0];
      assert(tenant.type == static_cast<std::int8_t>(40 - part));
      tenant.variant = 2U;
      tenant.exact_bytes[6] = std::byte{2};
      for (std::size_t ordinal = 0; ordinal < 8U; ++ordinal) {
        auto& state = tower.people[part * 8U + ordinal].exact_bytes[5];
        assert(state == std::byte{0x20});
        state = ordinal % 2U == 0U ? std::byte{3} : std::byte{4};
      }
    }

    // Direct 1040:0179 coverage. Revision 0x24 stores runtime b406 at
    // serialized header offset 60; every type-36..40 part clears its frame,
    // dirties the record, advances exactly state-three participants, and
    // removes only flag bit two.
    tower.header.exact_bytes[60] = std::byte{0x0d};
    tower.header.exact_bytes[61] = std::byte{0};
    assert(simtower::close_original_cathedral_for_day(tower) == 20U);
    assert(tower.header.exact_bytes[60] == std::byte{9});
    assert(tower.header.exact_bytes[61] == std::byte{0});
    for (std::size_t part = 0; part < 5U; ++part) {
      const auto& tenant = tower.floors[109U + part].tenants[0];
      assert(tenant.variant == 0U);
      assert(tenant.exact_bytes[6] == std::byte{0});
      assert(tenant.exact_bytes[7] == std::byte{0});
      assert(tenant.exact_bytes[13] == std::byte{1});
      for (std::size_t ordinal = 0; ordinal < 8U; ++ordinal) {
        assert(tower.people[part * 8U + ordinal].exact_bytes[5] ==
               (ordinal % 2U == 0U ? std::byte{5} : std::byte{4}));
      }
    }

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.header.exact_bytes[60] == std::byte{9});
    assert(simtower::serialize_original_tdt(reparsed) == bytes);

    auto absent = simtower::make_original_new_tdt();
    const auto absent_before = simtower::serialize_original_tdt(absent);
    assert(simtower::reset_original_cathedral_for_day(absent) == 0U);
    assert(simtower::close_original_cathedral_for_day(absent) == 0U);
    assert(simtower::serialize_original_tdt(absent) == absent_before);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());

    // 11f8:26dd supplies status 2 and a one-cell initial span. Above-ground
    // support comes from the immediately lower Lobby story.
    // Direct 1178:01db coverage: the single-range Floor path combines the
    // facility and newly exposed cell prices, then debits balance and the
    // construction-cost accumulator when the total is nonzero.
    auto result = simtower::build_original_floor(tower, 11, 120, 121, costs);
    assert(result.succeeded());
    assert(result.cost == 5);
    assert(tower.floors[11].left_edge == 120U);
    assert(tower.floors[11].right_edge == 121U);
    assert(tower.floors[11].tenants.size() == 1U);
    const auto& first = tower.floors[11].tenants[0];
    assert(first.left == 120U && first.right == 121U);
    assert(first.type == 0 && first.status == 2U && first.variant == 0U);
    assert(first.exact_bytes[4] == std::byte{0});
    assert(first.exact_bytes[5] == std::byte{2});
    assert(first.exact_bytes[12] == std::byte{0xff});
    assert(first.exact_bytes[13] == std::byte{1});
    assert(first.exact_bytes[14] == std::byte{1});
    assert(first.exact_bytes[15] == std::byte{0xff});
    assert(first.exact_bytes[16] == std::byte{4});

    // A separate adjacent placement remains a distinct 11f8:17fd record.
    // During a real drag, 11f8:26dd instead resubmits the cumulative span and
    // 11f8:284d replaces the covered run with one record.
    // Direct 1178:0697 coverage for 284d's shared direct-amount transaction:
    // a nonzero overlap-path charge wraps both accounting dwords equally.
    const auto balance_before_direct_debit = tower.header.balance;
    const auto costs_before_direct_debit = tower.header.construction_costs;
    result = simtower::build_original_floor(tower, 11, 121, 130, costs);
    assert(result.succeeded() && result.cost == 45);
    assert(tower.header.balance == balance_before_direct_debit - 45);
    assert(tower.header.construction_costs ==
           costs_before_direct_debit - 45);
    assert(tower.floors[11].tenants.size() == 2U);
    assert(tower.floors[11].tenants[0].left == 120U);
    assert(tower.floors[11].tenants[0].right == 121U);
    assert(tower.floors[11].tenants[1].left == 121U);
    assert(tower.floors[11].tenants[1].right == 130U);

    // A disjoint request fills the unrepresented gap to the old edge and
    // charges all fifteen newly exposed cells, not only the requested five.
    // 11f8:30ef preserves the gap and the clicked interval as separate rows.
    result = simtower::build_original_floor(tower, 11, 140, 145, costs);
    assert(result.succeeded() && result.cost == 75);
    assert(tower.floors[11].tenants.size() == 4U);
    assert(tower.floors[11].tenants[0].left == 120U);
    assert(tower.floors[11].tenants[1].right == 130U);
    assert(tower.floors[11].tenants[2].left == 130U);
    assert(tower.floors[11].tenants[2].right == 140U);
    assert(tower.floors[11].tenants[3].left == 140U);
    assert(tower.floors[11].tenants[3].right == 145U);

    const auto balance_before_noop = tower.header.balance;
    result = simtower::build_original_floor(tower, 11, 125, 130, costs);
    assert(result.succeeded() && result.cost == 0);
    assert(tower.header.balance == balance_before_noop);
    assert(tower.floors[11].tenants.size() == 5U);
    assert(tower.floors[11].tenants[1].left == 121U);
    assert(tower.floors[11].tenants[1].right == 125U);
    assert(tower.floors[11].tenants[2].left == 125U);
    assert(tower.floors[11].tenants[2].right == 130U);

    // A supported next story and first basement story use the two distinct
    // 11f8:2e64 branches.
    result = simtower::build_original_floor(tower, 12, 125, 130, costs);
    assert(result.succeeded() && result.cost == 25);
    result = simtower::build_original_floor(tower, 9, 130, 140, costs);
    assert(result.succeeded() && result.cost == 50);
    assert(tower.floors[9].tenants[0].status == 2U);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.floors[11].tenants.size() == 5U);
    assert(reparsed.floors[11].tenants[0].left == 120U);
    assert(reparsed.floors[11].tenants[4].right == 145U);
    assert(reparsed.floors[11].tenants[0].status == 2U);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // 10a0:1310 and 11f8:30ef make only the automatically filled gap a
    // Lobby on the initial multi-story Lobby band. The explicitly clicked
    // Floor intervals on either side remain type zero.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 3, costs)
               .succeeded());
    assert(simtower::build_original_floor(tower, 10, 200, 215, costs)
               .succeeded());
    assert(simtower::build_original_floor(tower, 11, 200, 205, costs)
               .succeeded());
    const auto result =
        simtower::build_original_floor(tower, 11, 210, 215, costs);
    assert(result.succeeded());
    assert(tower.floors[11].tenants.size() == 4U);
    assert(tower.floors[11].tenants[0].type == 0x18);
    assert(tower.floors[11].tenants[1].type == 0);
    assert(tower.floors[11].tenants[1].left == 200U);
    assert(tower.floors[11].tenants[1].right == 205U);
    assert(tower.floors[11].tenants[2].type == 0x18);
    assert(tower.floors[11].tenants[2].left == 205U);
    assert(tower.floors[11].tenants[2].right == 210U);
    assert(tower.floors[11].tenants[2].exact_bytes[12] == std::byte{0});
    assert(tower.floors[11].tenants[3].type == 0);
    assert(tower.floors[11].tenants[3].left == 210U);
    assert(tower.floors[11].tenants[3].right == 215U);
  }

  {
    // Exact frame-zero Medical Center rebuild at 1170:008a/011f.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    assert(simtower::build_original_medical_center(
               tower, 11, 120, 0, costs)
               .succeeded());
    assert(tower.header.exact_bytes[52] == std::byte{1});
    assert(tower.header.exact_bytes[53] == std::byte{0});
    assert(tower.post_elevator.b92e_counter == 1U);

    // The scheduler first forces the complete deferred queue through
    // 11f0:0016, then rebuilds all medical indexes from dbfc.
    simtower::refresh_original_medical_for_day(tower);
    assert(tower.post_elevator.b92e_counter == 0U);
    assert(tower.floors[11].tenants[0].type == 13);
    assert(tower.post_elevator.dbfc_dwords[0] == 0x0000000bU);

    // Add an orphan and a second active record. Byte 2 is reset, byte 3 is
    // preserved, and only the orphan's floor byte becomes ff.
    tower.post_elevator.dbfc_dwords[1] = 0x00aaff14U;
    tower.post_elevator.dbfc_dwords[2] = 0x557a0114U;
    tower.header.exact_bytes[52] = std::byte{3};  // DS:b3fe
    tower.header.exact_bytes[53] = std::byte{0};
    tower.post_elevator.bd5a_count = 10U;
    tower.post_elevator.bd5c_entries.fill(9U);
    // Direct 1170:0663/0681/06a4 coverage: scan dbfc, count valid Medical
    // services, and clear every byte in the seven 0x16-byte route banks before
    // 1170:06f7 appends the rebuilt routes.
    tower.medical_route_index.fill(std::byte{0xcc});
    tower.header.rating = 3U;
    tower.post_elevator.b92d = 0U;

    simtower::refresh_original_medical_for_day(tower);
    assert(tower.post_elevator.dbfc_dwords[0] == 0x0000000bU);
    assert(tower.post_elevator.dbfc_dwords[1] == 0x00aaffffU);
    assert(tower.post_elevator.dbfc_dwords[2] == 0x55000114U);
    assert(tower.header.exact_bytes[52] == std::byte{2});
    assert(tower.header.exact_bytes[53] == std::byte{0});
    assert(tower.post_elevator.bd5a_count == 2U);
    assert(tower.post_elevator.bd5c_entries[0] == 0U);
    assert(tower.post_elevator.bd5c_entries[1] == 2U);
    for (std::size_t index = 2U;
         index < tower.post_elevator.bd5c_entries.size(); ++index) {
      assert(tower.post_elevator.bd5c_entries[index] == 0U);
    }
    // Direct 1170:06f7 coverage: floors 11 and 20 map through 11a8:166b to
    // groups zero and one, append the service word, and increment each count.
    assert(tower.medical_route_index[0] == std::byte{1});
    assert(tower.medical_route_index[1] == std::byte{0});
    assert(tower.medical_route_index[2] == std::byte{0});
    assert(tower.medical_route_index[3] == std::byte{0});
    assert(tower.medical_route_index[0x16] == std::byte{1});
    assert(tower.medical_route_index[0x17] == std::byte{0});
    assert(tower.medical_route_index[0x18] == std::byte{2});
    assert(tower.medical_route_index[0x19] == std::byte{0});
    assert(tower.post_elevator.b92d == 1U);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.post_elevator.dbfc_dwords[1] == 0x00aaffffU);
    assert(reparsed.post_elevator.dbfc_dwords[2] == 0x55000114U);
    assert(reparsed.post_elevator.bd5a_count == 2U);
    assert(reparsed.post_elevator.b92d == 1U);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);

    // Ratings below three do not raise b92d, but the table rebuild still runs.
    tower.header.rating = 2U;
    tower.post_elevator.b92d = 7U;
    simtower::refresh_original_medical_for_day(tower);
    assert(tower.post_elevator.b92d == 7U);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    assert(simtower::build_original_floor(tower, 11, 110, 160, costs)
               .succeeded());
    assert(simtower::build_original_office(tower, 11, 120, 0, costs)
               .succeeded());

    const auto before_overlap = simtower::serialize_original_tdt(tower);
    auto result = simtower::build_original_floor(tower, 11, 115, 130, costs);
    assert(result.status == simtower::OriginalConstructionStatus::occupied);
    assert(result.construction_status_code == 9U);
    assert(simtower::serialize_original_tdt(tower) == before_overlap);

    tower.header.balance = 0;
    const auto before_funds = simtower::serialize_original_tdt(tower);
    result = simtower::build_original_floor(tower, 11, 199, 200, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::insufficient_funds);
    assert(result.cost == 200);
    assert(result.construction_status_code == 8U);
    assert(simtower::serialize_original_tdt(tower) == before_funds);

    // Exact 11f8:284d priority: 1178:009e rejects this five-yen exposure
    // before 2e64 can report the missing supporting span.
    result = simtower::build_original_floor(tower, 12, 100, 101, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::insufficient_funds);
    assert(result.cost == 5 && result.construction_status_code == 8U);
    tower.header.balance = 1000;
    result = simtower::build_original_floor(tower, 12, 100, 101, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_span);
    assert(result.construction_status_code == 6U);
    result = simtower::build_original_floor(tower, 110, 120, 121, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    assert(result.construction_status_code == 5U);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    const auto result = simtower::build_original_medical_center(
        tower, 11, 120, 2, costs);
    assert(result.succeeded());
    assert(result.cost == 3130);
    assert(tower.header.exact_bytes[52] == std::byte{1});
    assert(tower.header.exact_bytes[53] == std::byte{0});
    assert_pending_deferred_facility(
        tower.floors[11].tenants[0], 13, 120, 26, 2, 0, 0);
    for (std::size_t index = 0; index < 6U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == std::byte{11});
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(index));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == std::byte{0xf3});
      for (std::size_t byte = 5; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }
    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    const auto& activated = tower.floors[11].tenants[0];
    assert(activated.type == 13);
    assert(activated.status == 0U);
    assert(activated.variant == 0U);
    assert(activated.exact_bytes[7] == std::byte{0});
    assert(tower.post_elevator.dbfc_dwords[0] == 0x0000000bU);
    for (std::size_t index = 1; index < 10U; ++index) {
      assert(tower.post_elevator.dbfc_dwords[index] == 0x0000ffffU);
    }
    assert(tower.post_elevator.bd5a_count == 1U);
    assert(tower.post_elevator.bd5c_entries[0] == 0U);
    assert(tower.medical_route_index[0] == std::byte{1});
    assert(tower.medical_route_index[1] == std::byte{0});
    assert(tower.medical_route_index[2] == std::byte{0});
    assert(tower.medical_route_index[3] == std::byte{0});
    // 1170:01bf does not call 1220:08fb: the six reservation records retain
    // their pending negative type bytes after the facility itself activates.
    for (std::size_t index = 0; index < 6U; ++index) {
      assert(tower.people[index].exact_bytes[4] == std::byte{0xf3});
    }
    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.post_elevator.dbfc_dwords[0] == 0x0000000bU);
    assert(reparsed.post_elevator.bd5a_count == 1U);
    assert(reparsed.post_elevator.bd5c_entries[0] == 0U);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    // Direct 1220:0c72 coverage: the Restaurant/Fast Food constructors seed
    // their linked commercial records and deferred service setup exactly once.
    auto result = simtower::build_original_restaurant(
        tower, 11, 110, costs);
    assert(result.succeeded());
    assert(result.cost == 1320);
    result = simtower::build_original_fast_food(tower, 11, 140, costs);
    assert(result.succeeded());
    assert(result.cost == 910);
    assert(tower.header.exact_bytes[46] == std::byte{2});
    assert(tower.header.exact_bytes[47] == std::byte{0});
    assert(tower.floors[11].tenants.size() == 3U);
    assert_pending_deferred_facility(
        tower.floors[11].tenants.front(), 6, 110, 24, 0, 0, 0);
    assert(tower.floors[11].tenants[1].type == 0);
    assert_pending_deferred_facility(
        tower.floors[11].tenants.back(), 12, 140, 16, 0, 1, 48);

    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);

    const auto& restaurant = tower.floors[11].tenants.front();
    const auto& fast_food = tower.floors[11].tenants.back();
    assert(restaurant.type == 6 && restaurant.status == 0U &&
           restaurant.variant == 0U);
    assert(fast_food.type == 12 && fast_food.status == 0U &&
           fast_food.variant == 1U);
    for (std::size_t index = 0; index < 96U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      const bool restaurant_person = index < 48U;
      const std::size_t family_index = restaurant_person ? index : index - 48U;
      assert(exact[0] == std::byte{11});
      assert(exact[1] == (restaurant_person ? std::byte{0} : std::byte{1}));
      assert(exact[2] == static_cast<std::byte>(family_index));
      assert(exact[3] == std::byte{0});
      assert(exact[4] ==
             (restaurant_person ? std::byte{6} : std::byte{12}));
      assert(exact[5] == std::byte{0x20});
      assert(exact[6] == std::byte{0xfe});
      for (std::size_t byte = 7; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }
    for (std::size_t service = 0; service < 2U; ++service) {
      const auto& exact = tower.retail[service].exact_bytes;
      assert(exact[0] == std::byte{11});
      assert(exact[1] == static_cast<std::byte>(service));
      assert(exact[2] == std::byte{3});
      assert(exact[3] == std::byte{0x0a});
      assert(exact[4] == std::byte{0x0a});
      assert(exact[5] == std::byte{0x0a});
      assert(exact[6] == std::byte{0});
      assert(exact[7] == std::byte{0x0a});
      assert(exact[8] == std::byte{0});
      assert(exact[9] == std::byte{0});
      assert(exact[10] == std::byte{0});
      assert(exact[11] == std::byte{0});
      assert(exact[12] == std::byte{0xff});
      assert(exact[13] == std::byte{0xff});
      assert(exact[14] == std::byte{0});
      assert(exact[15] == std::byte{0});
      assert(exact[16] == std::byte{0});
      assert(exact[17] == std::byte{0});
    }
    assert(tower.restaurant_service_variant == 1U);
    assert(tower.fast_food_service_variant == 1U);
    // 11a8:1596 places Restaurant and Fast Food service indices into their
    // type-specific floor-group blocks and increments each group count.
    assert(tower.post_elevator.dynamic_dd60[0] == std::byte{1});
    assert(tower.post_elevator.dynamic_dd60[1] == std::byte{0});
    assert(tower.post_elevator.dynamic_dd60[2] == std::byte{0});
    assert(tower.post_elevator.dynamic_dd60[3] == std::byte{0});
    assert(tower.post_elevator.dynamic_dd64[0] == std::byte{1});
    assert(tower.post_elevator.dynamic_dd64[1] == std::byte{0});
    assert(tower.post_elevator.dynamic_dd64[2] == std::byte{1});
    assert(tower.post_elevator.dynamic_dd64[3] == std::byte{0});

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    // Direct 1220:0cc5 Housekeeping-family initializer coverage: activation
    // writes six type-15/state-zero people with the unique byte-seven ff.
    const auto result = simtower::build_original_housekeeping(
        tower, 11, 120, costs);
    assert(result.succeeded());
    assert(result.cost == 575);
    assert_pending_deferred_facility(
        tower.floors[11].tenants[0], 15, 120, 15, 0, 0, 0);
    for (std::size_t index = 0; index < 6U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == std::byte{11});
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(index));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == std::byte{0xf1});
      for (std::size_t byte = 5; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }
    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    const auto& activated = tower.floors[11].tenants[0];
    assert(activated.type == 15);
    assert(activated.status == 0U);
    assert(activated.rent_rate == 4U);
    for (std::size_t index = 0; index < 6U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == std::byte{11});
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(index));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == std::byte{15});
      assert(exact[5] == std::byte{0});
      assert(exact[6] == std::byte{0xfe});
      assert(exact[7] == std::byte{0xff});
      for (std::size_t byte = 8; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }
    assert(tower.post_elevator.cf88_words[0] == 0x000bU);
    for (std::size_t index = 1; index < 10U; ++index) {
      assert(tower.post_elevator.cf88_words[index] == 0xffffU);
    }
    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.floors[11].tenants[0].type == 15);
    assert(reparsed.people[0].exact_bytes[7] == std::byte{0xff});
    assert(reparsed.post_elevator.cf88_words[0] == 0x000bU);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    const auto result = simtower::build_original_secom_center(
        tower, 11, 120, costs);
    assert(result.succeeded());
    assert(result.cost == 1010);
    assert_pending_deferred_facility(
        tower.floors[11].tenants[0], 17, 120, 2, 0, 0, 0);
    for (std::size_t index = 0; index < 6U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == std::byte{11});
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(index));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == std::byte{0xef});
      for (std::size_t byte = 5; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }
    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    const auto& activated = tower.floors[11].tenants[0];
    assert(activated.type == 17);
    assert(activated.status == 0U);
    assert(activated.exact_bytes[12] == std::byte{0xff});
    assert(activated.preserved_07_to_0f[5] == std::byte{0xff});
    assert(activated.subtype == 0U);
    // 1228:075b never invokes a 1220 person initializer for type 17.
    for (std::size_t index = 0; index < 6U; ++index) {
      assert(tower.people[index].exact_bytes[4] == std::byte{0xef});
    }
    for (const auto word : tower.post_elevator.cf88_words) {
      assert(word == 0xffffU);
    }
    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.floors[11].tenants[0].type == 17);
    assert(reparsed.floors[11].tenants[0].exact_bytes[12] ==
           std::byte{0xff});
    assert(reparsed.people[0].exact_bytes[4] == std::byte{0xef});
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    auto result = simtower::build_original_housekeeping(
        tower, 10, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    result = simtower::build_original_housekeeping(tower, 9, 120, costs);
    assert(result.succeeded());
    assert(result.cost == 575);
    assert_pending_deferred_facility(
        tower.floors[9].tenants[0], 15, 120, 15, 0, 0, 0);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    // 11a8:07d3 with 11a8:1812/17eb and 1060:07f7: the first tick after
    // 0x00f0 in phase zero activates Fast Food open, chooses the calendar
    // history lane, and immediately accounts for its ten-person service
    // population.
    tower.header.frame_time = 0x00f1U;
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    assert(simtower::build_original_fast_food(tower, 11, 120, costs)
               .succeeded());
    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    const auto& exact = tower.retail[0].exact_bytes;
    assert(exact[2] == std::byte{0});
    assert(exact[3] == std::byte{0});
    assert(exact[4] == std::byte{0x0a});
    assert(exact[5] == std::byte{0x0a});
    assert(exact[6] == std::byte{0x0a});
    assert(exact[7] == std::byte{0});
    assert(exact[8] == std::byte{0x0a});
    assert(exact[12] == std::byte{0xf5});
    assert(exact[13] == std::byte{0xff});
    assert(tower.post_elevator.finance.population_by_category[5] == 10);
    assert(tower.post_elevator.finance.total_population == 10);
  }

  {
    // Exact 11f8:07d8 dispatcher sentinels and branch priority:
    // 0c92 gates the shared Restaurant/Retail/Fast Food count at exactly
    // 0x0200, while 0c0d and 0c46 gate Security and Medical at exactly ten.
    // These checks precede the generic constructor's floor/span validation;
    // the original equality comparisons deliberately permit larger values.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());

    tower.header.exact_bytes[46] = std::byte{0x00};
    tower.header.exact_bytes[47] = std::byte{0x02};
    const auto commercial_before = simtower::serialize_original_tdt(tower);
    auto result = simtower::build_original_restaurant(
        tower, 10, 0xffffU, costs);
    assert(result.status == simtower::OriginalConstructionStatus::tenant_limit);
    assert(result.construction_status_code == 30U);
    assert(simtower::serialize_original_tdt(tower) == commercial_before);

    tower.header.exact_bytes[46] = std::byte{0x01};
    tower.header.exact_bytes[47] = std::byte{0x02};
    result = simtower::build_original_fast_food(tower, 11, 120, costs);
    assert(result.succeeded());
    assert(tower.header.exact_bytes[46] == std::byte{0x02});
    assert(tower.header.exact_bytes[47] == std::byte{0x02});

    auto security = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               security, 100, 200, 1, costs)
               .succeeded());
    security.header.exact_bytes[48] = std::byte{10};
    const auto security_before = simtower::serialize_original_tdt(security);
    result = simtower::build_original_security(
        security, 10, 0xffffU, costs);
    assert(result.status == simtower::OriginalConstructionStatus::tenant_limit);
    assert(result.construction_status_code == 30U);
    assert(simtower::serialize_original_tdt(security) == security_before);
    security.header.exact_bytes[48] = std::byte{11};
    result = simtower::build_original_security(security, 11, 120, costs);
    assert(result.succeeded());
    assert(security.header.exact_bytes[48] == std::byte{12});

    auto medical = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               medical, 100, 200, 1, costs)
               .succeeded());
    medical.header.exact_bytes[52] = std::byte{10};
    const auto medical_before = simtower::serialize_original_tdt(medical);
    result = simtower::build_original_medical_center(
        medical, 10, 0xffffU, 2, costs);
    assert(result.status == simtower::OriginalConstructionStatus::tenant_limit);
    assert(result.construction_status_code == 30U);
    assert(simtower::serialize_original_tdt(medical) == medical_before);
    medical.header.exact_bytes[52] = std::byte{11};
    result = simtower::build_original_medical_center(
        medical, 11, 120, 2, costs);
    assert(result.succeeded());
    assert(medical.header.exact_bytes[52] == std::byte{12});
  }

  {
    // Complete 11f8:07f3-0955 mouse-phase filter. The four captured tools
    // alone accept move/up; double-click falls through for every type using
    // the placement retained by its already-armed down.
    using InputMessage = simtower::OriginalConstructionInputMessage;
    using InputAction = simtower::OriginalConstructionInputAction;
    assert(simtower::original_construction_input_action(
               InputMessage::button_down, 7U, true) == InputAction::press);
    assert(simtower::original_construction_input_action(
               InputMessage::mouse_move, 7U, true) == InputAction::ignore);
    assert(simtower::original_construction_input_action(
               InputMessage::mouse_move, 24U, false) == InputAction::ignore);
    for (const std::uint16_t type : {0U, 11U, 24U, 44U}) {
      assert(simtower::original_continuous_construction_type(type));
      assert(simtower::original_construction_input_action(
                 InputMessage::mouse_move, type, true) ==
             InputAction::continuous_update);
      assert(simtower::original_construction_input_action(
                 InputMessage::button_up, type, false) ==
             InputAction::continuous_release);
    }
    assert(!simtower::original_continuous_construction_type(2U));
    for (const std::uint16_t type : {1U, 22U, 24U, 27U, 42U, 43U}) {
      assert(simtower::original_construction_rebuilds_routes(type));
    }
    assert(!simtower::original_construction_rebuilds_routes(0U));
    for (const std::uint16_t type : {0U, 2U, 11U, 24U, 44U}) {
      assert(!simtower::original_construction_plays_general_success_wave(type));
    }
    assert(simtower::original_construction_plays_general_success_wave(1U));
    assert(simtower::original_construction_plays_general_success_wave(43U));
    // Direct 11f8:0e21-0e67 coverage: the exact BeepOnly equality decision is
    // independent of the five-type WAVE/7000 exclusion and precedes it.
    using SuccessAudioPlan =
        simtower::OriginalConstructionSuccessAudioPlan;
    assert((simtower::original_construction_success_audio_plan(7U, false) ==
            SuccessAudioPlan{false, true}));
    assert((simtower::original_construction_success_audio_plan(7U, true) ==
            SuccessAudioPlan{true, true}));
    for (const std::uint16_t type : {0U, 2U, 11U, 24U, 44U}) {
      assert((simtower::original_construction_success_audio_plan(type, true) ==
              SuccessAudioPlan{true, false}));
    }
    assert(simtower::original_construction_input_action(
               InputMessage::button_up, 2U, false) == InputAction::ignore);
    assert(simtower::original_construction_input_action(
               InputMessage::double_click, 7U, true) ==
           InputAction::repeat_retained_placement);

    using ReleasePlan = simtower::OriginalConstructionReleasePlan;
    assert((simtower::original_construction_release_plan(
                7U, 1U, 20'000, 19'000) == ReleasePlan{}));
    assert((simtower::original_construction_release_plan(
                24U, 0U, 20'000, 19'000) ==
            ReleasePlan{true, false, false, true}));
    assert((simtower::original_construction_release_plan(
                24U, 1U, 20'000, 20'000) ==
            ReleasePlan{true, false, false, true}));
    assert((simtower::original_construction_release_plan(
                24U, 1U, 20'000, 19'000) ==
            ReleasePlan{true, true, true, false}));
    assert((simtower::original_construction_release_plan(
                0U, 0xffffU, 20'000, 19'999) ==
            ReleasePlan{true, true, true, false}));
    assert((simtower::original_construction_release_plan(
                11U, 1U, 20'000, 20'000) ==
            ReleasePlan{true, true, false, false}));
    assert((simtower::original_construction_release_plan(
                44U, 0U, 20'000, 19'000) ==
            ReleasePlan{true, false, false, true}));
  }

  {
    // 11f8:240d consumes the pointer interval retained by the preceding call;
    // the current position is not eligible until the following message.
    simtower::OriginalParkingDragRunState state{};
    auto plan = simtower::original_parking_drag_run_plan(
        state, 120, 124, 120, 124);
    assert((plan.unit_lefts == std::vector<std::int32_t>{120}));
    assert((plan.next_state == simtower::OriginalParkingDragRunState{
                                   true, 120, 124, 120, 124}));

    plan = simtower::original_parking_drag_run_plan(
        plan.next_state, 0, 0, 132, 136);
    assert(plan.unit_lefts.empty());
    assert((plan.next_state == simtower::OriginalParkingDragRunState{
                                   true, 120, 124, 132, 136}));
    plan = simtower::original_parking_drag_run_plan(
        plan.next_state, 0, 0, 132, 136);
    assert((plan.unit_lefts ==
            std::vector<std::int32_t>{124, 128, 132}));
    assert((plan.next_state == simtower::OriginalParkingDragRunState{
                                   true, 120, 136, 132, 136}));

    plan = simtower::original_parking_drag_run_plan(
        plan.next_state, 0, 0, 100, 104);
    assert(plan.unit_lefts.empty());
    plan = simtower::original_parking_drag_run_plan(
        plan.next_state, 0, 0, 100, 104);
    assert((plan.unit_lefts ==
            std::vector<std::int32_t>{116, 112, 108, 104, 100}));
    assert((plan.next_state == simtower::OriginalParkingDragRunState{
                                   true, 100, 136, 100, 104}));

    // A retained double-click can initialize from the down-time snap while
    // publishing a different current LPARAM interval at the helper tail.
    plan = simtower::original_parking_drag_run_plan(
        {}, 120, 124, 140, 144);
    assert((plan.unit_lefts == std::vector<std::int32_t>{120}));
    assert((plan.next_state == simtower::OriginalParkingDragRunState{
                                   true, 120, 124, 140, 144}));
  }

  {
    // 11f8:25a2 has the same one-message lag on its vertical range. Each
    // newly exposed floor is attempted in original high/low loop order using
    // the current call's c6 horizontal snap.
    using RampAttempt = simtower::OriginalParkingRampDragAttempt;
    simtower::OriginalParkingRampDragRunState state{};
    auto plan = simtower::original_parking_ramp_drag_run_plan(
        state, 9, 9, 120);
    assert((plan.attempts == std::vector<RampAttempt>{{9, 120}}));
    assert((plan.next_state == simtower::OriginalParkingRampDragRunState{
                                   true, 10, 9, 10, 9}));

    plan = simtower::original_parking_ramp_drag_run_plan(
        plan.next_state, 0, 6, 124);
    assert(plan.attempts.empty());
    assert((plan.next_state == simtower::OriginalParkingRampDragRunState{
                                   true, 10, 9, 7, 6}));
    plan = simtower::original_parking_ramp_drag_run_plan(
        plan.next_state, 0, 6, 128);
    assert((plan.attempts == std::vector<RampAttempt>{
                                 {8, 128}, {7, 128}, {6, 128}}));
    assert((plan.next_state == simtower::OriginalParkingRampDragRunState{
                                   true, 10, 6, 7, 6}));

    plan = simtower::original_parking_ramp_drag_run_plan(
        plan.next_state, 0, 11, 132);
    assert(plan.attempts.empty());
    plan = simtower::original_parking_ramp_drag_run_plan(
        plan.next_state, 0, 11, 136);
    assert((plan.attempts ==
            std::vector<RampAttempt>{{10, 136}, {11, 136}}));
    assert((plan.next_state == simtower::OriginalParkingRampDragRunState{
                                   true, 12, 6, 12, 11}));
  }

  {
    // Both captured helpers overwrite DX/BX for every unit and expose only
    // the final 17fd result to 07d8's shared 24cc increment.
    using Sound = simtower::OriginalCapturedHelperSound;
    using Completion = simtower::OriginalCapturedHelperCompletionPlan;
    assert((simtower::original_captured_helper_completion_plan(
                false, true, true) == Completion{}));
    assert((simtower::original_captured_helper_completion_plan(
                true, false, true) == Completion{}));
    assert((simtower::original_captured_helper_completion_plan(
                true, true, true) ==
            Completion{true, true, Sound::priority_five, true}));
    assert((simtower::original_captured_helper_completion_plan(
                true, true, false) ==
            Completion{true, true, Sound::reserved_if_idle, false}));
  }

  {
    // 26dd counts and auto-scrolls only for its final successful story. An
    // earlier overlapping story's nonzero-cost WAVE/7001 request has already
    // happened even when a later story overwrites the helper return with zero.
    using Sound = simtower::OriginalCapturedHelperSound;
    using Completion = simtower::OriginalFloorLobbyHelperCompletionPlan;
    assert((simtower::original_floor_lobby_helper_completion_plan(
                false, false, true) == Completion{}));
    assert((simtower::original_floor_lobby_helper_completion_plan(
                false, true, true) ==
            Completion{false, false, false, Sound::priority_five, true}));
    assert((simtower::original_floor_lobby_helper_completion_plan(
                false, true, false) ==
            Completion{false, false, false, Sound::reserved_if_idle, false}));
    assert((simtower::original_floor_lobby_helper_completion_plan(
                true, false, true) ==
            Completion{true, true, true, Sound::none, false}));
    assert((simtower::original_floor_lobby_helper_completion_plan(
                true, true, true) ==
            Completion{true, true, true, Sound::priority_five, true}));
    assert((simtower::original_floor_lobby_helper_completion_plan(
                true, true, false) ==
            Completion{true, true, true, Sound::reserved_if_idle, false}));

    // Direct 1038:002f plus 1178:01db/027c/0697 host-presentation coverage:
    // construction dirties the retained Main/tile transport, balance changes
    // repaint Info synchronously, and Map remains on 1080:09c3's independent
    // sixteen-tick cadence rather than receiving an invented invalidation.
    using MutationPlan =
        simtower::OriginalWorldMutationPresentationPlan;
    assert((simtower::original_world_mutation_presentation_plan(
                false, false) == MutationPlan{}));
    assert((simtower::original_world_mutation_presentation_plan(
                true, false) == MutationPlan{true, true, false, false}));
    assert((simtower::original_world_mutation_presentation_plan(
                true, true) == MutationPlan{true, true, true, false}));
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    // Direct 10f8:002d and 1220:0cec coverage: Security activation stores the
    // floor/key in the first free cf88 word and initializes six family-14,
    // state-one people while preserving the remaining slots.
    const auto result = simtower::build_original_security(
        tower, 11, 120, costs);
    assert(result.succeeded());
    assert(result.cost == 580);
    assert(tower.header.exact_bytes[48] == std::byte{1});
    assert(tower.header.exact_bytes[49] == std::byte{0});
    assert_pending_deferred_facility(
        tower.floors[11].tenants[0], 14, 120, 16, 0, 0, 0);
    for (std::size_t index = 0; index < 6U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == std::byte{11});
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(index));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == std::byte{0xf2});
      for (std::size_t byte = 5; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }
    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    const auto& activated = tower.floors[11].tenants[0];
    assert(activated.type == 14);
    assert(activated.status == 0U);
    assert(activated.rent_rate == 4U);
    for (std::size_t index = 0; index < 6U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == std::byte{11});
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(index));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == std::byte{14});
      assert(exact[5] == std::byte{1});
      for (std::size_t byte = 6; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }
    assert(tower.post_elevator.cf88_words[0] == 0x000bU);
    for (std::size_t index = 1; index < 10U; ++index) {
      assert(tower.post_elevator.cf88_words[index] == 0xffffU);
    }
    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());

    // A second basement story cannot be opened before the first provides
    // reversed support. Ground floor itself remains illegal for Retail.
    auto result = simtower::build_original_retail_shop(
        tower, 8, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_span);
    result = simtower::build_original_retail_shop(tower, 10, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    assert(tower.header.exact_bytes[46] == std::byte{0});
    assert(tower.header.exact_bytes[47] == std::byte{0});

    result = simtower::build_original_retail_shop(tower, 9, 120, costs);
    assert(result.succeeded());
    assert(result.cost == 2460);
    assert_pending_deferred_facility(
        tower.floors[9].tenants[0], 10, 120, 12, 0, 0, 0);

    result = simtower::build_original_retail_shop(tower, 8, 124, costs);
    assert(result.succeeded());
    assert(result.cost == 2460);
    assert(tower.header.exact_bytes[46] == std::byte{2});
    assert(tower.header.exact_bytes[47] == std::byte{0});
    assert_pending_deferred_facility(
        tower.floors[8].tenants[0], 10, 124, 12, 0, 0, 48);
    assert(tower.post_elevator.b92e_counter == 2U);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());

    // Direct 1220:0be3 coverage: Condo construction reserves three people;
    // activation rewrites their pending type and exact resident-state bytes.
    const auto pristine = simtower::serialize_original_tdt(tower);
    auto result = simtower::build_original_condo(tower, 11, 120, 3, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_span);
    assert(simtower::serialize_original_tdt(tower) == pristine);

    result = simtower::build_original_condo(tower, 11, 120, 2, costs);
    assert(result.succeeded());
    assert(result.cost == 1680);
    assert(tower.header.balance == 13320);
    assert(tower.header.construction_costs == -6680);
    assert(tower.floors[11].left_edge == 120U);
    assert(tower.floors[11].right_edge == 136U);
    assert(tower.floors[11].tenants.size() == 1U);
    assert_pending_deferred_facility(
        tower.floors[11].tenants[0], 9, 120, 16, 2, 0, 0);
    assert(tower.floors[11].tenant_index[0] == 0U);
    assert(tower.post_elevator.b92e_counter == 1U);
    for (std::size_t index = 0; index < 3U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == std::byte{11});
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(index));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == std::byte{0xf7});
      for (std::size_t byte = 5; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }

    // Direct 1220:049a and 1220:067c coverage: seed every dynamic byte so the
    // activation path must clear the entire owned span before rewriting each
    // pending 0xf7 record to the exact resident bytes.
    for (std::size_t index = 0; index < 3U; ++index) {
      auto& exact = tower.people[index].exact_bytes;
      for (std::size_t byte = 4U; byte < exact.size(); ++byte) {
        exact[byte] = static_cast<std::byte>(0x80U + index + byte);
      }
    }
    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    const auto& activated = tower.floors[11].tenants[0];
    assert(activated.type == 9);
    assert(activated.status == 0x20U);
    assert(activated.subtype == 0U);
    constexpr std::array<std::byte, 3> condo_byte_six = {
        std::byte{0x0a}, std::byte{0xfe}, std::byte{0x0a}};
    for (std::size_t index = 0; index < 3U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == std::byte{11});
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(index));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == std::byte{9});
      assert(exact[5] == std::byte{0x20});
      assert(exact[6] == condo_byte_six[index]);
      for (std::size_t byte = 7; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }
    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());

    const auto pristine = simtower::serialize_original_tdt(tower);
    auto result = simtower::build_original_retail_shop(
        tower, 10, 120, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::invalid_floor);
    assert(simtower::serialize_original_tdt(tower) == pristine);

    // Direct 1220:0c4b Retail wrapper coverage: construction reserves all 48
    // records and activation applies the common family-10 initializer.
    const auto retail_before = tower.retail[0].exact_bytes;
    result = simtower::build_original_retail_shop(tower, 11, 120, costs);
    assert(result.succeeded());
    assert(result.cost == 2460);
    assert(tower.header.balance == 12540);
    assert(tower.header.construction_costs == -7460);
    assert(tower.floors[11].left_edge == 120U);
    assert(tower.floors[11].right_edge == 132U);
    assert(tower.floors[11].tenants.size() == 1U);
    assert_pending_deferred_facility(
        tower.floors[11].tenants[0], 10, 120, 12, 0, 0, 0);
    assert(tower.floors[11].tenant_index[0] == 0U);
    assert(tower.post_elevator.b92e_counter == 1U);
    assert(tower.retail[0].exact_bytes == retail_before);
    for (std::size_t index = 0; index < 48U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == std::byte{11});
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(index));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == std::byte{0xf6});
      for (std::size_t byte = 5; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }

    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    const auto& activated = tower.floors[11].tenants[0];
    assert(activated.type == 10);
    assert(activated.status == 0U);
    assert(activated.variant == 0U);
    assert(activated.subtype == 0U);
    for (std::size_t index = 0; index < 48U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == std::byte{11});
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(index));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == std::byte{10});
      assert(exact[5] == std::byte{0x20});
      assert(exact[6] == std::byte{0xfe});
      for (std::size_t byte = 7; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }
    assert(tower.retail[0].exact_bytes == retail_before);
    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    auto result = simtower::extend_original_lobby(tower, 100, 104, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::lobby_not_initialized);

    result = simtower::build_original_initial_lobby(
        tower, 160, 164, 1, costs);
    assert(result.succeeded());
    assert(result.cost == 200);
    assert(!result.construction_sound_requested);  // empty floor -> 17fd
    assert(result.document_changed);
    result = simtower::extend_original_lobby(tower, 156, 172, costs);
    assert(result.succeeded());
    assert(result.cost == 600);
    assert(result.construction_sound_requested);  // overlap stays in 284d
    assert(tower.header.balance == 19200);
    assert(tower.header.construction_costs == -800);
    assert_lobby(tower.floors[10], 156, 172);

    // Returning toward the drag anchor cannot demolish already built cells.
    result = simtower::extend_original_lobby(tower, 160, 164, costs);
    assert(result.succeeded());
    assert(result.cost == 0);
    assert(!result.construction_sound_requested);
    assert(result.document_changed);
    const auto& split = tower.floors[10].tenants;
    assert(split.size() == 3U);
    assert(split[0].type == 0x18 && split[0].left == 156 &&
           split[0].right == 160);
    assert(split[1].type == 0x18 && split[1].left == 160 &&
           split[1].right == 164);
    assert(split[2].type == 0x18 && split[2].left == 164 &&
           split[2].right == 172);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.floors[10].left_edge == 156);
    assert(reparsed.floors[10].right_edge == 172);
    assert(reparsed.floors[10].tenants.size() == 3U);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Exact 11f8:284d type-24 replacement accepts both Lobby and ordinary
    // Floor records. 1178:0583 charges only exposure beyond the represented
    // floor edges, so converting an already-built adjacent Floor is free.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 160, 164, 1, costs)
               .succeeded());
    assert(simtower::build_original_floor(tower, 10, 164, 172, costs)
               .succeeded());
    assert(tower.floors[10].tenants.size() == 2U &&
           tower.floors[10].tenants[1].type == 0);
    const auto balance_before = tower.header.balance;
    const auto result =
        simtower::extend_original_lobby(tower, 160, 172, costs);
    assert(result.succeeded() && result.cost == 0);
    assert(tower.header.balance == balance_before);
    assert_lobby(tower.floors[10], 160, 172);
    assert(tower.floors[10].tenants.size() == 1U);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    auto result = simtower::build_original_initial_lobby(
        tower, 160, 164, 3, costs);
    assert(result.succeeded());
    assert(result.cost == 600);
    result = simtower::extend_original_lobby(tower, 156, 172, costs);
    assert(result.succeeded());
    assert(result.cost == 1800);
    assert(tower.header.balance == 17600);
    for (std::size_t floor = 10; floor <= 12; ++floor) {
      assert_lobby(tower.floors[floor], 156, 172);
    }
  }

  {
    auto tower = simtower::make_original_new_tdt();
    auto result = simtower::build_original_initial_lobby(
        tower, 160, 164, 1, costs);
    assert(result.succeeded());
    const auto before = simtower::serialize_original_tdt(tower);
    result = simtower::extend_original_lobby(tower, 0, 540, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::insufficient_funds);
    assert(simtower::serialize_original_tdt(tower) == before);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    const auto result = simtower::build_original_initial_lobby(
        tower, 50, 150, 3, costs);
    assert(result.succeeded());
    assert(result.cost == 15000);
    assert(tower.header.lobby_height == 3U);
    assert(tower.header.balance == 5000);
    assert(tower.header.construction_costs == -15000);
    assert_lobby(tower.floors[10], 50, 150);
    assert_lobby(tower.floors[11], 50, 150);
    assert_lobby(tower.floors[12], 50, 150);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    const auto result = simtower::build_original_initial_lobby(
        tower, 200, 100, 1, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_span);
    assert(result.construction_status_code == 20U);
    assert(result.document_changed);
    assert(tower.header.lobby_height == 1U);
    assert(tower.header.balance == 20000);
    assert(tower.floors[10].tenants.empty());
  }

  {
    auto tower = simtower::make_original_new_tdt();
    const auto result =
        simtower::build_original_initial_lobby(tower, 0, 541, 1, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_span);
    assert(tower.header.lobby_height == 1U);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    const auto original = simtower::serialize_original_tdt(tower);
    const auto result =
        simtower::build_original_initial_lobby(tower, 50, 150, 0, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::invalid_lobby_height);
    assert(!result.document_changed);
    assert(simtower::serialize_original_tdt(tower) == original);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    const auto result =
        simtower::build_original_initial_lobby(tower, 20, 520, 1, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::insufficient_funds);
    assert(result.cost == 25000);
    assert(result.construction_status_code == 8U);
    assert(tower.header.lobby_height == 1U);
    assert(tower.header.balance == 20000);
    assert(tower.floors[10].tenants.empty());
  }

  {
    // 26dd attempts both preview stories after the paid ground call fails.
    // Their missing-support status overwrites the earlier funds rejection.
    auto tower = simtower::make_original_new_tdt();
    const auto result =
        simtower::build_original_initial_lobby(tower, 50, 250, 3, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_span);
    assert(result.cost == 0);
    assert(result.construction_status_code == 3U);
    assert(result.document_changed);
    assert(tower.header.lobby_height == 3U);
    assert(tower.header.balance == 20000);
    assert(tower.floors[10].tenants.empty());
    assert(tower.floors[11].tenants.empty());
    assert(tower.floors[12].tenants.empty());
  }

  {
    auto tower = simtower::make_original_new_tdt();
    auto obstacle = simtower::OriginalTdtTenant{};
    obstacle.type = 7;
    obstacle.left = 50;
    obstacle.right = 150;
    obstacle.exact_bytes[0] = std::byte{50};
    obstacle.exact_bytes[2] = std::byte{150};
    obstacle.exact_bytes[4] = std::byte{7};
    tower.floors[11].left_edge = 50;
    tower.floors[11].right_edge = 150;
    tower.floors[11].tenants.push_back(obstacle);
    const auto result = simtower::build_original_initial_lobby(
        tower, 50, 150, 2, costs);
    assert(result.status == simtower::OriginalConstructionStatus::occupied);
    assert(result.cost == 10000);
    assert(result.document_changed);
    assert(tower.header.lobby_height == 2U);
    assert(tower.header.balance == 10000);
    assert_lobby(tower.floors[10], 50, 150);
    assert(tower.floors[11].tenants.size() == 1U &&
           tower.floors[11].tenants[0].type == 7);
  }

  {
    // 26dd does not stop after an occupied middle story. Floor 12 can succeed
    // using that occupied story as support, and its final return makes the
    // whole helper report success while preserving the partial tower.
    auto tower = simtower::make_original_new_tdt();
    auto obstacle = simtower::OriginalTdtTenant{};
    obstacle.type = 7;
    obstacle.left = 50;
    obstacle.right = 150;
    obstacle.exact_bytes[0] = std::byte{50};
    obstacle.exact_bytes[2] = std::byte{150};
    obstacle.exact_bytes[4] = std::byte{7};
    tower.floors[11].left_edge = 50;
    tower.floors[11].right_edge = 150;
    tower.floors[11].tenants.push_back(obstacle);
    const auto result = simtower::build_original_initial_lobby(
        tower, 50, 150, 3, costs);
    assert(result.succeeded());
    assert(result.cost == 15000);
    assert(!result.construction_sound_requested);
    assert(tower.header.lobby_height == 3U);
    assert(tower.header.balance == 5000);
    assert_lobby(tower.floors[10], 50, 150);
    assert(tower.floors[11].tenants.size() == 1U &&
           tower.floors[11].tenants[0].type == 7);
    assert_lobby(tower.floors[12], 50, 150);
  }

  {
    // An accepted overlapping ground extension reaches 284d's sound boundary
    // before the occupied preview story overwrites 26dd's final return.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 50, 150, 2, costs).succeeded());
    auto obstacle = simtower::OriginalTdtTenant{};
    obstacle.type = 7;
    obstacle.left = 50;
    obstacle.right = 150;
    obstacle.exact_bytes[0] = std::byte{50};
    obstacle.exact_bytes[2] = std::byte{150};
    obstacle.exact_bytes[4] = std::byte{7};
    tower.floors[11].tenants.assign(1U, obstacle);
    tower.floors[11].left_edge = 50;
    tower.floors[11].right_edge = 150;
    const auto result =
        simtower::extend_original_lobby(tower, 40, 160, costs);
    assert(result.status == simtower::OriginalConstructionStatus::occupied);
    assert(result.cost == 2000);
    assert(result.construction_sound_requested);
    assert(result.document_changed);
    assert(tower.header.balance == 8000);
    assert_lobby(tower.floors[10], 40, 160);
    assert(tower.floors[11].tenants.size() == 1U &&
           tower.floors[11].tenants[0].type == 7);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    auto first = simtower::build_original_initial_lobby(
        tower, 50, 150, 1, costs);
    assert(first.succeeded());
    const auto second = simtower::build_original_initial_lobby(
        tower, 200, 300, 1, costs);
    assert(second.status ==
           simtower::OriginalConstructionStatus::lobby_already_initialized);
  }

  // 10a0:12e0/1366 and 11f8:26dd permit type 24 above ground only at the
  // 15-floor sky-lobby cadence. On an unbuilt landing, 1178:009e charges
  // both the Lobby cells and the newly exposed ordinary-floor cells.
  {
    auto tower = make_sky_lobby_ready_tower(costs);
    const auto old_balance = tower.header.balance;
    const auto old_construction_costs = tower.header.construction_costs;
    auto result = simtower::build_original_sky_lobby(
        tower, 24, 120, 124, costs);
    assert(result.succeeded());
    assert(result.cost == 4 * (50 + 5));
    assert(tower.header.balance == old_balance - result.cost);
    assert(tower.header.construction_costs ==
           old_construction_costs - result.cost);
    assert_lobby(tower.floors[24], 120, 124);

    result = simtower::build_original_sky_lobby(
        tower, 24, 120, 130, costs);
    assert(result.succeeded());
    assert(result.cost == 6 * (50 + 5));
    assert_lobby(tower.floors[24], 120, 130);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert_lobby(reparsed.floors[24], 120, 130);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  // A sky Lobby inserted into an existing floor replaces just the covered
  // type-0 interval and preserves the two floor remainders and lookup keys.
  {
    auto tower = make_sky_lobby_ready_tower(costs);
    assert(simtower::build_original_floor(tower, 24, 100, 200, costs)
               .succeeded());
    const auto old_balance = tower.header.balance;
    const auto result = simtower::build_original_sky_lobby(
        tower, 24, 120, 124, costs);
    assert(result.succeeded());
    assert(result.cost == 4 * 50);
    assert(tower.header.balance == old_balance - result.cost);

    const auto& floor = tower.floors[24];
    assert(floor.left_edge == 100U);
    assert(floor.right_edge == 200U);
    assert(floor.tenants.size() == 3U);
    assert(floor.tenants[0].type == 0);
    assert(floor.tenants[0].left == 100U);
    assert(floor.tenants[0].right == 120U);
    assert(floor.tenants[1].type == 0x18);
    assert(floor.tenants[1].left == 120U);
    assert(floor.tenants[1].right == 124U);
    assert(floor.tenants[2].type == 0);
    assert(floor.tenants[2].left == 124U);
    assert(floor.tenants[2].right == 200U);
    assert(floor.tenants[1].exact_bytes[12] == std::byte{0});
    assert(floor.tenant_index[0] == 1U);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Exact 1178:035b + 11f8:284d behavior with two disjoint sky Lobbies:
    // only Lobby records containing a requested boundary trim the charge.
    // Internal existing Lobby cells are charged again when the bridge replaces
    // the complete Floor/Lobby run, and 1228:0d9a reuses the freed lowest key.
    auto tower = make_sky_lobby_ready_tower(costs);
    assert(simtower::build_original_floor(tower, 24, 100, 200, costs)
               .succeeded());
    assert(simtower::build_original_sky_lobby(
               tower, 24, 120, 124, costs)
               .succeeded());
    assert(simtower::build_original_sky_lobby(
               tower, 24, 130, 134, costs)
               .succeeded());
    const auto old_balance = tower.header.balance;
    const auto result = simtower::build_original_sky_lobby(
        tower, 24, 118, 136, costs);
    assert(result.succeeded() && result.cost == 18 * 50);
    assert(tower.header.balance == old_balance - result.cost);
    const auto& floor = tower.floors[24];
    assert(floor.tenants.size() == 3U);
    assert(floor.tenants[0].type == 0 && floor.tenants[0].left == 100U &&
           floor.tenants[0].right == 118U);
    assert(floor.tenants[1].type == 0x18 &&
           floor.tenants[1].left == 118U &&
           floor.tenants[1].right == 136U &&
           floor.tenants[1].exact_bytes[12] == std::byte{0});
    assert(floor.tenants[2].type == 0 && floor.tenants[2].left == 136U &&
           floor.tenants[2].right == 200U);
  }

  {
    auto tower = make_sky_lobby_ready_tower(costs);
    const auto pristine = simtower::serialize_original_tdt(tower);
    auto result = simtower::build_original_sky_lobby(
        tower, 23, 120, 124, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    assert(result.construction_status_code == 13U);
    result = simtower::build_original_sky_lobby(
        tower, 110, 120, 124, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    assert(result.construction_status_code == 5U);
    result = simtower::build_original_sky_lobby(
        tower, 24, 200, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_span);
    assert(result.construction_status_code == 20U);
    result = simtower::build_original_sky_lobby(
        tower, 24, 90, 104, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_span);
    assert(result.construction_status_code == 6U);
    assert(simtower::serialize_original_tdt(tower) == pristine);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    const auto pristine = simtower::serialize_original_tdt(tower);
    const auto result = simtower::build_original_sky_lobby(
        tower, 24, 120, 124, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_span);
    assert(result.construction_status_code == 3U);
    assert(simtower::serialize_original_tdt(tower) == pristine);
  }

  // 1178:009e distinguishes failure to afford the Lobby itself (STRL 7)
  // from affording it but not its new floor exposure (STRL 8).
  {
    auto tower = make_sky_lobby_ready_tower(costs);
    assert(simtower::build_original_floor(tower, 24, 100, 200, costs)
               .succeeded());
    tower.header.balance = 4 * 50 - 1;
    const auto pristine = simtower::serialize_original_tdt(tower);
    const auto result = simtower::build_original_sky_lobby(
        tower, 24, 120, 124, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::insufficient_funds);
    assert(result.cost == 4 * 50);
    assert(result.construction_status_code == 7U);
    assert(simtower::serialize_original_tdt(tower) == pristine);
  }

  {
    auto tower = make_sky_lobby_ready_tower(costs);
    tower.header.balance = 4 * 50;
    const auto pristine = simtower::serialize_original_tdt(tower);
    const auto result = simtower::build_original_sky_lobby(
        tower, 24, 120, 124, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::insufficient_funds);
    assert(result.cost == 4 * (50 + 5));
    assert(result.construction_status_code == 8U);
    assert(simtower::serialize_original_tdt(tower) == pristine);
  }

  {
    auto tower = make_sky_lobby_ready_tower(costs);
    assert(simtower::build_original_floor(tower, 24, 100, 200, costs)
               .succeeded());
    // Direct 11f8:321e coverage: an Office wholly contained by a type-zero
    // interval is admissible and splits that interval; the occupied overlap
    // immediately below is rejected without mutating the document.
    assert(simtower::build_original_office(tower, 24, 120, 0, costs)
               .succeeded());
    const auto& split = tower.floors[24].tenants;
    assert(split.size() == 3U);
    assert(split[0].type == 0 && split[0].left == 100U &&
           split[0].right == 120U);
    assert(split[1].type == -7 && split[1].left == 120U &&
           split[1].right == 129U);
    assert(split[2].type == 0 && split[2].left == 129U &&
           split[2].right == 200U);
    const auto pristine = simtower::serialize_original_tdt(tower);
    const auto result = simtower::build_original_sky_lobby(
        tower, 24, 120, 124, costs);
    assert(result.status == simtower::OriginalConstructionStatus::occupied);
    assert(result.construction_status_code == 9U);
    assert(simtower::serialize_original_tdt(tower) == pristine);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    const auto pristine = simtower::serialize_original_tdt(tower);
    auto result = simtower::build_original_standard_elevator(
        tower, 10, 120, costs, part);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_span);
    assert(simtower::serialize_original_tdt(tower) == pristine);

    result = simtower::build_original_initial_lobby(
        tower, 100, 200, 1, costs);
    assert(result.succeeded());
    // 11f8:0fea reaches its mode-one/1080:05a1 tail for a newly allocated
    // shaft, but the existing-shaft add-car success bypasses that UI tail.
    result = simtower::build_original_standard_elevator(
        tower, 10, 120, costs, part);
    assert(result.succeeded());
    assert(result.new_elevator_shaft_created);
    assert(result.cost == 2000);
    assert(tower.header.balance == 13000);
    assert(tower.header.construction_costs == -7000);
    assert_standard_elevator(tower.elevators[0], 120, 10, 1);
    // 11f8:15f7 detects that the lobby already covers the four cells and
    // therefore leaves its floor record byte-for-byte unchanged.
    assert_lobby(tower.floors[10], 100, 200);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert_standard_elevator(reparsed.elevators[0], 120, 10, 1);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);

    // Direct 1090:0192 coverage. A newly added car duplicates its home floor
    // at byte 13 and snapshots the current 28..41 floor-mode bank at byte 14;
    // it must not read the distinct 14..27 waiting-time bank.
    tower.header.frame_time = 0U;
    tower.header.current_day = 0;
    tower.elevators[0].schedule[14] = std::byte{99};
    tower.elevators[0].schedule[28] = std::byte{2};
    result = simtower::build_original_standard_elevator(
        tower, 10, 120, costs, part);
    assert(result.succeeded());
    assert(!result.new_elevator_shaft_created);
    assert(result.cost == 800);
    assert(tower.header.balance == 12200);
    assert(tower.header.construction_costs == -7800);
    assert(tower.elevators[0].car_records[0].exact_bytes[14] == std::byte{0});
    assert(tower.elevators[0].car_records[1].exact_bytes[13] == std::byte{10});
    assert(tower.elevators[0].car_records[1].exact_bytes[14] == std::byte{2});
    tower.elevators[0].schedule[14] = std::byte{5};
    tower.elevators[0].schedule[28] = std::byte{0};
    assert_standard_elevator(tower.elevators[0], 120, 10, 2);

    for (std::size_t car = 2; car < 8; ++car) {
      result = simtower::build_original_standard_elevator(
          tower, 10, 120, costs, part);
      assert(result.succeeded());
      assert(result.cost == 800);
    }
    assert_standard_elevator(tower.elevators[0], 120, 10, 8);
    const auto full = simtower::serialize_original_tdt(tower);
    result = simtower::build_original_standard_elevator(
        tower, 10, 120, costs, part);
    assert(result.status ==
           simtower::OriginalConstructionStatus::elevator_car_limit);
    assert(result.construction_status_code == 24U);
    assert(simtower::serialize_original_tdt(tower) == full);
  }

  // 1148:02c8 gates an added car on the persisted count byte before
  // 11f8:113f searches the eight raw records. Preserve both malformed-save
  // outcomes: count-full reports entry 24 despite a free record, while a
  // record-full/count-low disagreement fails silently.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    assert(simtower::build_original_standard_elevator(
               tower, 10, 120, costs, part)
               .succeeded());
    tower.elevators[0].cars = 8U;
    const auto count_full = simtower::serialize_original_tdt(tower);
    auto result = simtower::build_original_standard_elevator(
        tower, 10, 120, costs, part);
    assert(result.status ==
           simtower::OriginalConstructionStatus::elevator_car_limit);
    assert(result.construction_status_code == 24U);
    assert(simtower::serialize_original_tdt(tower) == count_full);

    tower.elevators[0].cars = 1U;
    for (auto& car : tower.elevators[0].car_records) {
      car.exact_bytes[15] = std::byte{1};
    }
    const auto records_full = simtower::serialize_original_tdt(tower);
    result = simtower::build_original_standard_elevator(
        tower, 10, 120, costs, part);
    assert(result.status ==
           simtower::OriginalConstructionStatus::elevator_car_limit);
    assert(result.construction_status_code == 0U);
    assert(simtower::serialize_original_tdt(tower) == records_full);
  }

  // Raw commands 42 and 43 are not facility type bytes. The 11f8 jump
  // table translates them to express type 0 and service type 2, with their
  // own capacities, widths, YEN shaft charges, and PART car charges.
  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    // Direct 10a0:1080 coverage: search for an existing matching shaft before
    // taking the new-slot allocation path.
    auto result = simtower::build_original_elevator(
        tower, 42U, 10, 120, costs, part);
    assert(result.succeeded() && result.cost == 6000);
    assert(tower.header.balance == 9000);
    assert(tower.header.construction_costs == -11000);
    assert_elevator(tower.elevators[0], 0U, 0x2aU, 120, 10, 1);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert_elevator(reparsed.elevators[0], 0U, 0x2aU, 120, 10, 1);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);

    result = simtower::build_original_elevator(
        tower, 42U, 10, 120, costs, part);
    assert(result.succeeded() && result.cost == 900);
    assert_elevator(tower.elevators[0], 0U, 0x2aU, 120, 10, 2);
    assert(tower.header.balance == 8100);

    const auto before_wrong_floor = simtower::serialize_original_tdt(tower);
    result = simtower::build_original_elevator(
        tower, 42U, 23, 120, costs, part);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    assert(simtower::serialize_original_tdt(tower) == before_wrong_floor);

    result = simtower::build_original_elevator(
        tower, 44U, 10, 120, costs, part);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_span);
    assert(simtower::serialize_original_tdt(tower) == before_wrong_floor);
  }

  {
    // Direct 1148:0277 coverage: all 24 occupied shaft slots report alert
    // entry 25 before collision or allocation can mutate the document.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    for (auto& elevator : tower.elevators) elevator.used = 1U;
    const auto before = simtower::serialize_original_tdt(tower);
    const auto result = simtower::build_original_standard_elevator(
        tower, 10, 120, costs, part);
    assert(result.status == simtower::OriginalConstructionStatus::elevator_limit);
    assert(result.construction_status_code == 25U);
    assert(simtower::serialize_original_tdt(tower) == before);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    auto result = simtower::build_original_elevator(
        tower, 43U, 10, 120, costs, part);
    assert(result.succeeded() && result.cost == 4000);
    assert_elevator(tower.elevators[0], 2U, 0x15U, 120, 10, 1);
    assert(tower.header.balance == 11000);

    result = simtower::build_original_elevator(
        tower, 43U, 10, 120, costs, part);
    assert(result.succeeded() && result.cost == 700);
    assert_elevator(tower.elevators[0], 2U, 0x15U, 120, 10, 2);
    assert(tower.header.balance == 10300);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert_elevator(reparsed.elevators[0], 2U, 0x15U, 120, 10, 2);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Direct 10a0:133b coverage: a three-story Lobby excludes floors 11 and
    // 12 from new-shaft allocation, while ground floor 10 remains legal.
    auto tower = simtower::make_original_new_tdt();
    auto result = simtower::build_original_initial_lobby(
        tower, 100, 200, 3, costs);
    assert(result.succeeded());
    result = simtower::build_original_standard_elevator(
        tower, 11, 120, costs, part);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);

    result = simtower::build_original_standard_elevator(
        tower, 10, 120, costs, part);
    assert(result.succeeded());
    const auto before_collision = simtower::serialize_original_tdt(tower);
    result = simtower::build_original_standard_elevator(
        tower, 10, 131, costs, part);
    assert(result.status == simtower::OriginalConstructionStatus::occupied);
    assert(simtower::serialize_original_tdt(tower) == before_collision);
    // At eight cells of shaft separation the original expanded candidate
    // rectangle merely touches and the second shaft is accepted.
    result = simtower::build_original_standard_elevator(
        tower, 10, 132, costs, part);
    assert(result.succeeded());
    assert_standard_elevator(tower.elevators[1], 132, 10, 1);
  }

  {
    // Direct 10a0:10e8 signed-shape regression. Its Stair collision rectangle
    // sign-extends byte +1 and uses SAR AX,1. Shape ff therefore has height
    // -1 and a floor-seven record occupies only [6,7); it must not become the
    // unsigned 127-story obstruction that the former native path created.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    auto& malformed = tower.post_elevator.stairs_bd70[0];
    malformed.used = 1U;
    malformed.shape = 0xffU;
    malformed.x = 120U;
    malformed.floor = 7;
    const auto result = simtower::build_original_standard_elevator(
        tower, 10, 120, costs, part);
    assert(result.succeeded());
    assert_standard_elevator(tower.elevators[0], 120, 10, 1);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    struct HotelCase {
      std::uint8_t type;
      std::uint16_t width;
      std::uint8_t variant;
      std::size_t people;
      std::int32_t cost;
    };
    constexpr std::array<HotelCase, 3> hotel_cases = {{
        {3, 4, 1, 2, 220},
        {4, 6, 3, 3, 530},
        {5, 10, 1, 3, 1050},
    }};

    for (const auto& test : hotel_cases) {
      tower = simtower::make_original_new_tdt();
      assert(simtower::build_original_initial_lobby(
                 tower, 100, 200, 1, costs)
                 .succeeded());
      auto result = simtower::build_original_hotel_room(
          tower, test.type, 11, 120, test.variant, costs);
      assert(result.succeeded());
      assert(result.cost == test.cost);
      assert(tower.header.balance == 15000 - test.cost);
      assert(tower.floors[11].tenants.size() == 1U);
      assert_pending_deferred_facility(
          tower.floors[11].tenants[0], test.type, 120, test.width,
          test.variant, 0, 0);
      assert(tower.floors[11].tenant_index[0] == 0U);
      assert(tower.post_elevator.b92e_counter == 1U);
      for (std::size_t index = 0; index < test.people; ++index) {
        const auto& exact = tower.people[index].exact_bytes;
        assert(exact[0] == std::byte{11});
        assert(exact[1] == std::byte{0});
        assert(exact[2] == static_cast<std::byte>(index));
        assert(exact[3] == std::byte{0});
        assert(exact[4] == static_cast<std::byte>(
            static_cast<std::uint8_t>(
                -static_cast<std::int16_t>(test.type))));
        for (std::size_t byte = 5; byte < exact.size(); ++byte) {
          assert(exact[byte] == std::byte{0});
        }
      }

      for (int step = 0; step < 11; ++step) {
        assert(simtower::step_original_pending_construction(tower) ==
               simtower::OriginalPendingStepStatus::advanced);
      }
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::activated);
      const auto& activated = tower.floors[11].tenants[0];
      assert(activated.type == static_cast<std::int8_t>(test.type));
      assert(activated.status == 0x20U);
      assert(activated.subtype == 0U);
      // Direct 1220:0b4e coverage: the first Hotel guest alone changes from
      // common initializer state 0x20 to 0x24.
      for (std::size_t index = 0; index < test.people; ++index) {
        const auto& exact = tower.people[index].exact_bytes;
        assert(exact[0] == std::byte{11});
        assert(exact[1] == std::byte{0});
        assert(exact[2] == static_cast<std::byte>(index));
        assert(exact[3] == std::byte{0});
        assert(exact[4] == static_cast<std::byte>(test.type));
        assert(exact[5] == (index == 0U ? std::byte{0x24}
                                        : std::byte{0x20}));
        assert(exact[6] == std::byte{0xfe});
        for (std::size_t byte = 7; byte < exact.size(); ++byte) {
          assert(exact[byte] == std::byte{0});
        }
      }
      const auto bytes = simtower::serialize_original_tdt(tower);
      const auto reparsed = simtower::parse_original_tdt(bytes);
      assert(simtower::serialize_original_tdt(reparsed) == bytes);
    }
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(tower.people_count == 512U);
    auto result = simtower::build_original_initial_lobby(
        tower, 100, 200, 1, costs);
    assert(result.succeeded());

    // Direct 1228:0000 shared post-initializer coverage: the Office path
    // verifies the negative construction type, status/variant, lookup key,
    // six-person start dword, subtype bytes, and deferred-queue entry.
    result = simtower::build_original_office(tower, 11, 120, 0, costs);
    assert(result.succeeded());
    assert(result.cost == 445);
    assert(tower.header.balance == 14555);
    assert(tower.header.construction_costs == -5445);
    const auto& office_floor = tower.floors[11];
    assert(office_floor.left_edge == 120U);
    assert(office_floor.right_edge == 129U);
    assert(office_floor.tenants.size() == 1U);
    assert_pending_office(office_floor.tenants[0], 120, 0, 0, 0);
    assert(office_floor.tenant_index[0] == 0U);
    assert_office_people(tower, 0, 11, 0);
    assert(tower.post_elevator.b92e_counter == 1U);
    assert(tower.post_elevator.b92e[2] == std::byte{11});
    assert(tower.post_elevator.b92e[12] == std::byte{0});
    assert(tower.post_elevator.b944_words[0] == 0x09e5U);

    result = simtower::build_original_office(tower, 11, 129, 1, costs);
    assert(result.succeeded());
    assert(result.cost == 445);
    assert(tower.floors[11].tenants.size() == 2U);
    assert_pending_office(tower.floors[11].tenants[1], 129, 1, 1, 6);
    assert(tower.floors[11].tenant_index[0] == 0U);
    assert(tower.floors[11].tenant_index[1] == 1U);
    assert_office_people(tower, 6, 11, 1);

    result = simtower::build_original_office(tower, 11, 140, 2, costs);
    assert(result.succeeded());
    assert(result.cost == 455);
    assert(tower.floors[11].tenants.size() == 4U);
    const auto& gap = tower.floors[11].tenants[2];
    assert(gap.left == 138U);
    assert(gap.right == 140U);
    assert(gap.type == 0);
    assert(gap.status == 2U);
    assert(gap.exact_bytes[12] == std::byte{0xff});
    assert_pending_office(tower.floors[11].tenants[3], 140, 2, 2, 12);
    assert(tower.floors[11].tenant_index[2] == 3U);
    assert(tower.post_elevator.b92e_counter == 3U);

    const auto before_overlap = simtower::serialize_original_tdt(tower);
    result = simtower::build_original_office(tower, 11, 125, 3, costs);
    assert(result.status == simtower::OriginalConstructionStatus::occupied);
    assert(simtower::serialize_original_tdt(tower) == before_overlap);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.people_count == 512U);
    assert_pending_office(reparsed.floors[11].tenants[0], 120, 0, 0, 0);
    assert_pending_office(reparsed.floors[11].tenants[1], 129, 1, 1, 6);
    assert_pending_office(reparsed.floors[11].tenants[3], 140, 2, 2, 12);
    assert_office_people(reparsed, 12, 11, 2);
    assert(reparsed.post_elevator.b92e_counter == 3U);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);

    // 1090:042f advances 11f0's twelve-step construction countdown. The
    // twelfth pass executes the Office branch of 11f0:00a0/1228:0103 and
    // 1220:0b27's exact six-record initialization.
    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(tower.floors[11].tenants[0].subtype == 1U);
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    const auto& activated = tower.floors[11].tenants[0];
    assert(activated.type == 7);
    assert(activated.status == 0x18U);
    assert(activated.subtype == 0U);
    assert(activated.exact_bytes[4] == std::byte{7});
    assert(activated.exact_bytes[5] == std::byte{0x18});
    assert(activated.exact_bytes[12] == std::byte{0});
    assert(activated.exact_bytes[13] == std::byte{1});
    assert(activated.exact_bytes[17] == std::byte{0});
    assert(tower.post_elevator.b92e_counter == 2U);
    assert(tower.post_elevator.b92e[0] == std::byte{2});
    assert(tower.post_elevator.b92e[1] == std::byte{1});
    for (std::size_t index = 0; index < 6; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == std::byte{11});
      assert(exact[1] == std::byte{0});
      assert(exact[2] == static_cast<std::byte>(index));
      assert(exact[3] == std::byte{0});
      assert(exact[4] == std::byte{7});
      assert(exact[5] == std::byte{0x20});
      assert(exact[6] == std::byte{0xfe});
      for (std::size_t byte = 7; byte < exact.size(); ++byte) {
        assert(exact[byte] == std::byte{0});
      }
    }
    const auto activated_bytes = simtower::serialize_original_tdt(tower);
    const auto activated_reparsed =
        simtower::parse_original_tdt(activated_bytes);
    assert(simtower::serialize_original_tdt(activated_reparsed) ==
           activated_bytes);
  }

  {
    // Direct 1198:07e6 rebuild coverage: the floor-9 seed ramp, matching ramp
    // chain, surrounding Parking connectivity, b3ee x gate, and final 01ab
    // derived-index rebuild are all observed below.
    auto tower = simtower::make_original_new_tdt();
    auto result = simtower::build_original_initial_lobby(
        tower, 100, 200, 1, costs);
    assert(result.succeeded());
    for (std::size_t index = 0; index < 251U; ++index) {
      tower.people[index].exact_bytes[4] = std::byte{1};
    }
    result = simtower::build_original_office(tower, 11, 120, 5, costs);
    assert(result.succeeded());
    // Start 251 plus six leaves only 255 spare slots, so 1238:013a invokes
    // one exact 256-record growth before returning the allocation. 1238:0230
    // passes the prior count and a literal 256 to 1238:029f, whose twelve
    // field stores cover all sixteen bytes of every newly appended record.
    assert(tower.people_count == 768U);
    assert(tower.people.size() == 768U);
    assert_pending_office(tower.floors[11].tenants[0], 120, 5, 0, 251);
    assert_office_people(tower, 251, 11, 0);
    for (std::size_t index = 512U; index < 768U; ++index) {
      for (const auto byte : tower.people[index].exact_bytes) {
        assert(byte == std::byte{0});
      }
    }
  }

  {
    auto tower = simtower::make_original_new_tdt();
    auto result = simtower::build_original_initial_lobby(
        tower, 100, 200, 1, costs);
    assert(result.succeeded());
    for (std::uint16_t index = 0; index < 10U; ++index) {
      result = simtower::build_original_office(
          tower, 11, static_cast<std::uint16_t>(100U + index * 9U),
          static_cast<std::uint8_t>(index % 6U), costs);
      assert(result.succeeded());
    }
    assert(tower.post_elevator.b92e_counter == 10U);
    assert(tower.post_elevator.b92e[1] == std::byte{0});

    // 11f0:004b calls 00a0 immediately at capacity, advances the head, and
    // reuses the vacated circular slot for the eleventh pending Office.
    result = simtower::build_original_office(tower, 11, 190, 4, costs);
    assert(result.succeeded());
    assert(tower.post_elevator.b92e_counter == 10U);
    assert(tower.post_elevator.b92e[0] == std::byte{10});
    assert(tower.post_elevator.b92e[1] == std::byte{1});
    assert(tower.post_elevator.b92e[2] == std::byte{11});
    assert(tower.post_elevator.b92e[12] == std::byte{10});
    assert(tower.floors[11].tenants.size() == 11U);
    assert(tower.floors[11].tenants[0].type == 7);
    assert(tower.floors[11].tenants[0].status == 0x18U);
    for (std::size_t index = 1; index < 11U; ++index) {
      assert(tower.floors[11].tenants[index].type == -7);
    }
    for (std::size_t index = 0; index < 6U; ++index) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[0] == std::byte{11});
      assert(exact[1] == std::byte{0});
      assert(exact[4] == std::byte{7});
      assert(exact[5] == std::byte{0x20});
      assert(exact[6] == std::byte{0xfe});
    }
    assert_pending_office(tower.floors[11].tenants[10], 190, 4, 10, 60);
    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    auto result = simtower::build_original_initial_lobby(
        tower, 100, 200, 1, costs);
    assert(result.succeeded());

    const auto before_invalid_ramp = simtower::serialize_original_tdt(tower);
    result = simtower::build_original_parking_ramp(tower, 8, 120, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    assert(result.construction_status_code == 31U);
    assert(simtower::serialize_original_tdt(tower) == before_invalid_ramp);
    result = simtower::build_original_parking(tower, 9, 116, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::parking_ramp_required);
    assert(result.construction_status_code == 34U);

    const auto balance_before_ramp = tower.header.balance;
    result = simtower::build_original_parking_ramp(tower, 9, 120, costs);
    assert(result.succeeded());
    assert(result.cost == 580);
    assert(tower.header.balance == balance_before_ramp - 580);
    assert(tower.floors[9].left_edge == 120U);
    assert(tower.floors[9].right_edge == 136U);
    assert(tower.floors[9].tenants.size() == 1U);
    const auto& first_ramp = tower.floors[9].tenants[0];
    assert(first_ramp.left == 120U);
    assert(first_ramp.right == 136U);
    assert(first_ramp.type == 0x2c);
    assert(first_ramp.status == 0U);
    assert(first_ramp.variant == 0U);
    assert(first_ramp.exact_bytes[12] == std::byte{0xff});
    assert(first_ramp.exact_bytes[13] == std::byte{1});
    assert(first_ramp.rent_rate == 4U);
    assert(first_ramp.subtype == 0U);
    assert(tower.header.exact_bytes[36] == std::byte{120});
    assert(tower.header.exact_bytes[37] == std::byte{0});

    result = simtower::build_original_parking_ramp(tower, 8, 121, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_span);

    result = simtower::build_original_parking(tower, 9, 116, costs);
    assert(result.succeeded());
    assert(result.cost == 320);
    assert(tower.floors[9].left_edge == 116U);
    assert(tower.floors[9].right_edge == 136U);
    assert(tower.floors[9].tenants.size() == 2U);
    const auto& first_parking = tower.floors[9].tenants[0];
    assert(first_parking.left == 116U);
    assert(first_parking.right == 120U);
    assert(first_parking.type == 11);
    assert(first_parking.status == 0U);
    assert(first_parking.variant == 0U);
    assert(first_parking.exact_bytes[12] == std::byte{0});
    assert(first_parking.exact_bytes[13] == std::byte{1});
    assert(first_parking.rent_rate == 4U);
    assert(first_parking.subtype == 0U);
    assert(tower.floors[9].tenant_index[0] == 0U);
    // Direct 1198:0252 coverage: the first free cf9c slot receives floor/key
    // plus a zero dword before parking-index connectivity is rebuilt.
    const auto& first_record = tower.post_elevator.cf9c_records[0];
    assert(first_record[0] == std::byte{9});
    assert(first_record[1] == std::byte{0});
    for (std::size_t byte = 2; byte < first_record.size(); ++byte) {
      assert(first_record[byte] == std::byte{0});
    }
    assert(tower.post_elevator.parking_connected == 1);
    assert(tower.post_elevator.parking_entries[0] == 0U);
    assert(tower.header.exact_bytes[50] == std::byte{1});
    assert(tower.header.exact_bytes[51] == std::byte{0});

    result = simtower::build_original_parking_ramp(tower, 8, 120, costs);
    assert(result.succeeded());
    assert(tower.floors[9].tenants[1].type == 0x2c);
    assert(tower.floors[9].tenants[1].status == 2U);
    assert(tower.floors[8].tenants.size() == 1U);
    assert(tower.floors[8].tenants[0].type == 0x2c);
    assert(tower.floors[8].tenants[0].status == 0U);

    result = simtower::build_original_parking(tower, 8, 116, costs);
    assert(result.succeeded());
    assert(tower.floors[8].tenants[0].type == 11);
    assert(tower.floors[8].tenants[0].status == 0U);
    assert(tower.post_elevator.cf9c_records[1][0] == std::byte{8});
    assert(tower.post_elevator.cf9c_records[1][1] == std::byte{0});
    assert(tower.post_elevator.parking_connected == 2);
    assert(tower.post_elevator.parking_entries[0] == 0U);
    assert(tower.post_elevator.parking_entries[1] == 1U);

    // 1198:09ce treats an empty span of four or more cells as a break in
    // connectivity on either side of the ramp.
    result = simtower::build_original_parking(tower, 9, 100, costs);
    assert(result.succeeded());
    assert(tower.floors[9].tenants.size() == 4U);
    assert(tower.floors[9].tenants[0].type == 11);
    assert(tower.floors[9].tenants[0].status == 1U);
    assert(tower.floors[9].tenants[1].type == 0);
    assert(tower.floors[9].tenants[1].left == 104U);
    assert(tower.floors[9].tenants[1].right == 116U);
    assert(tower.floors[9].tenants[2].status == 0U);
    assert(tower.post_elevator.parking_connected == 2);

    const auto parking_bytes = simtower::serialize_original_tdt(tower);
    const auto parking_reparsed = simtower::parse_original_tdt(parking_bytes);
    assert(parking_reparsed.floors[9].tenants[0].status == 1U);
    assert(parking_reparsed.floors[9].tenants[2].status == 0U);
    assert(parking_reparsed.floors[9].tenants[3].status == 2U);
    assert(parking_reparsed.post_elevator.parking_connected == 2);
    assert(parking_reparsed.post_elevator.parking_entries[0] == 0U);
    assert(parking_reparsed.post_elevator.parking_entries[1] == 1U);
    assert(simtower::serialize_original_tdt(parking_reparsed) == parking_bytes);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs)
               .succeeded());
    assert(simtower::build_original_parking_ramp(
               tower, 9, 120, costs)
               .succeeded());
    assert(simtower::build_original_parking(
               tower, 9, 116, costs)
               .succeeded());
    assert(tower.post_elevator.parking_connected == 1);

    auto& parking = tower.floors[9].tenants[0];
    // Direct 1198:07a5/07c3 coverage: the rebuild clears the index before
    // appending active records. 1198:00d9 writes categories 0..8 but
    // accidentally leaves category 9 intact, then includes all ten category
    // dwords in the wrapping total.
    tower.post_elevator.b846_series[1][9] = 7;
    parking.status = 1U;
    parking.exact_bytes[5] = std::byte{1};
    tower.post_elevator.parking_entries[1] = 0x7777U;
    simtower::refresh_original_parking_for_day(tower);
    assert(tower.post_elevator.parking_connected == 0);
    for (const auto entry : tower.post_elevator.parking_entries) {
      assert(entry == 0U);
    }
    for (std::size_t index = 0; index < 9U; ++index) {
      assert(tower.post_elevator.b846_series[1][index] == 0);
    }
    assert(tower.post_elevator.b846_series[1][9] == 7);
    assert(tower.post_elevator.b846_series[1][10] == 7);

    parking.status = 0U;
    parking.exact_bytes[5] = std::byte{0};
    simtower::refresh_original_parking_for_day(tower);
    assert(tower.post_elevator.parking_connected == 1);
    assert(tower.post_elevator.parking_entries[0] == 0U);
    assert(tower.post_elevator.b846_series[1][0] == 2);
    assert(tower.post_elevator.b846_series[1][1] == 1);
    assert(tower.post_elevator.b846_series[1][2] == 1);
    assert(tower.post_elevator.b846_series[1][3] == 2);
    for (std::size_t index = 4U; index < 9U; ++index) {
      assert(tower.post_elevator.b846_series[1][index] == 1);
    }
    assert(tower.post_elevator.b846_series[1][9] == 7);
    assert(tower.post_elevator.b846_series[1][10] == 18);

    // An occupied cf9c slot with lookup key ff is orphaned by 1198:01ab.
    tower.post_elevator.cf9c_records[0][1] = std::byte{0xff};
    simtower::refresh_original_parking_for_day(tower);
    assert(tower.post_elevator.cf9c_records[0][0] == std::byte{0xff});
    assert(tower.post_elevator.parking_connected == 0);
    assert(tower.header.exact_bytes[50] == std::byte{0});
    assert(tower.header.exact_bytes[51] == std::byte{0});
  }

  {
    // The original parking/ramp rebuild begins by activating every queued
    // construction via 11f0:0016.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_office(
               tower, 11, 120, 0, costs).succeeded());
    assert(tower.floors[11].tenants[0].type == -7);
    assert(tower.post_elevator.b92e_counter == 1U);
    assert(simtower::build_original_parking_ramp(
               tower, 9, 120, costs).succeeded());
    assert(tower.floors[11].tenants[0].type == 7);
    assert(tower.post_elevator.b92e_counter == 0U);
  }

  {
    // Direct 11f8:1452, 1178:0703, and 11b0:00a4/06a4 coverage: 0a21 stores a
    // normal Stair as shape 1 on the lower of its two floors, marks cf10's
    // stair-direction bit, rebuilds the first route summary, and debits the
    // exact type cost.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_floor(
               tower, 11, 120, 150, costs).succeeded());
    const auto balance_before = tower.header.balance;
    auto result = simtower::build_original_vertical_transport(
        tower, 22, 11, 124, costs);
    assert(result.succeeded());
    assert(result.cost == 50);
    assert(tower.header.balance == balance_before - 50);
    const auto& stair = tower.post_elevator.stairs_bd70[0];
    assert(stair.used == 1U);
    assert(stair.shape == 1U);
    assert(stair.x == 124U);
    assert(stair.floor == 10);
    assert(stair.byte_5 == 0U);
    assert(stair.word_6 == 0U);
    assert(stair.word_8 == 0U);
    assert(stair.exact_bytes[0] == std::byte{1});
    assert(stair.exact_bytes[1] == std::byte{1});
    assert(stair.exact_bytes[2] == std::byte{124});
    assert(stair.exact_bytes[3] == std::byte{0});
    assert(stair.exact_bytes[4] == std::byte{10});
    assert(tower.post_elevator.cf10[10] == std::byte{2});
    const auto& route = tower.post_elevator.routes_bff0[0];
    assert(route[0] == std::byte{0});
    assert(route[1] == std::byte{1});
    assert(route[2] == std::byte{11});
    assert(route[3] == std::byte{10});
    assert(tower.post_elevator.routes_bff0[1][2] == std::byte{0xff});
    assert(tower.post_elevator.routes_bff0[1][3] == std::byte{0xff});

    const auto after_first = simtower::serialize_original_tdt(tower);
    result = simtower::build_original_vertical_transport(
        tower, 22, 11, 124, costs);
    assert(result.status == simtower::OriginalConstructionStatus::occupied);
    assert(simtower::serialize_original_tdt(tower) == after_first);

    const auto reparsed = simtower::parse_original_tdt(after_first);
    assert(reparsed.post_elevator.stairs_bd70[0].used == 1U);
    assert(reparsed.post_elevator.stairs_bd70[0].shape == 1U);
    assert(reparsed.post_elevator.stairs_bd70[0].x == 124U);
    assert(reparsed.post_elevator.stairs_bd70[0].floor == 10);
    assert(reparsed.post_elevator.cf10[10] == std::byte{2});
    assert(simtower::serialize_original_tdt(reparsed) == after_first);
  }

  {
    // A three-story Lobby rewrites a Stair clicked anywhere inside its
    // interior into shape 5 spanning ground floor 10 through floor 13.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 3, costs).succeeded());
    assert(simtower::build_original_floor(
               tower, 13, 120, 150, costs).succeeded());
    const auto result = simtower::build_original_vertical_transport(
        tower, 22, 11, 124, costs);
    assert(result.succeeded());
    const auto& stair = tower.post_elevator.stairs_bd70[0];
    assert(stair.shape == 5U);
    assert(stair.floor == 10);
    assert(stair.x == 124U);
    assert(tower.post_elevator.cf10[10] == std::byte{2});
    assert(tower.post_elevator.cf10[11] == std::byte{2});
    assert(tower.post_elevator.cf10[12] == std::byte{2});

    // Exact 10c0:0d06 candidate/existing bottom-edge asymmetry still rejects
    // an overlapping lobby-spanning Stair before any record or YEN mutation.
    const auto before_collision = simtower::serialize_original_tdt(tower);
    const auto collision = simtower::build_original_vertical_transport(
        tower, 22, 12, 128, costs);
    assert(collision.status == simtower::OriginalConstructionStatus::occupied);
    assert(collision.construction_status_code == 23U);
    assert(simtower::serialize_original_tdt(tower) == before_collision);
  }

  {
    // Direct 10c0:0983 coverage. For two zero-height Stair/Escalator records,
    // the original does not compare their full 8x2 bounding boxes. It passes
    // each 4x1 diagonal half through 1208:0105: a four-cell horizontal offset
    // is therefore permitted, while a three-cell offset overlaps and reports
    // construction status 23 before mutating the free record or YEN.
    auto make_diagonal_fixture = [&]() {
      auto tower = simtower::make_original_new_tdt();
      assert(simtower::build_original_initial_lobby(
                 tower, 100, 200, 1, costs).succeeded());
      assert(simtower::build_original_floor(
                 tower, 11, 120, 160, costs).succeeded());
      auto& existing = tower.post_elevator.stairs_bd70[0];
      existing.used = 1U;
      existing.shape = 0U;
      existing.floor = 10;
      existing.exact_bytes[0] = std::byte{1};
      existing.exact_bytes[1] = std::byte{0};
      existing.exact_bytes[4] = std::byte{10};
      return tower;
    };

    auto adjacent = make_diagonal_fixture();
    adjacent.post_elevator.stairs_bd70[0].x = 128U;
    const auto adjacent_result = simtower::build_original_vertical_transport(
        adjacent, 22, 11, 124, costs);
    assert(adjacent_result.succeeded());
    assert(adjacent.post_elevator.stairs_bd70[1].used == 1U);
    assert(adjacent.post_elevator.stairs_bd70[1].x == 124U);

    auto overlapping = make_diagonal_fixture();
    overlapping.post_elevator.stairs_bd70[0].x = 127U;
    const auto before = simtower::serialize_original_tdt(overlapping);
    const auto overlap_result = simtower::build_original_vertical_transport(
        overlapping, 22, 11, 124, costs);
    assert(overlap_result.status ==
           simtower::OriginalConstructionStatus::occupied);
    assert(overlap_result.construction_status_code == 23U);
    assert(simtower::serialize_original_tdt(overlapping) == before);
  }

  {
    // Direct 11f8:3d5d/3d2d coverage: client coordinates acquire the shared
    // view offset before conversion to the eight-by-36 world grid.
    // 10c0:0606 does not use a rectangular hit target for a normal
    // Stair/Escalator. At local x=4 only local y=[36,60) is accepted.
    auto tower = simtower::make_original_new_tdt();
    auto& normal = tower.post_elevator.stairs_bd70[0];
    normal.used = 1U;
    normal.shape = 1U;
    normal.x = 124U;
    normal.floor = 10;
    auto hit = simtower::original_vertical_transport_hit_from_client(
        tower, 224, 124, 800, 3800);
    assert(hit == simtower::OriginalVerticalTransportHit({true, 0U, 10, 128}));
    assert(!simtower::original_vertical_transport_hit_from_client(
                tower, 224, 123, 800, 3800).hit);
    assert(!simtower::original_vertical_transport_hit_from_client(
                tower, 224, 148, 800, 3800).hit);
    assert(!simtower::original_vertical_transport_hit_from_client(
                tower, 288, 124, 800, 3800).hit);

    // The signed shape/2 branch for a lobby-spanning shape 5 accepts its
    // stored base through base+2 without applying the diagonal pixel band.
    normal.shape = 5U;
    hit = simtower::original_vertical_transport_hit_from_client(
        tower, 224, 62, 800, 3800);
    assert(hit == simtower::OriginalVerticalTransportHit({true, 0U, 12, 128}));
    assert(!simtower::original_vertical_transport_hit_from_client(
                tower, 224, 26, 800, 3800).hit);

    // The first used record wins when malformed/imported transports overlap.
    tower.post_elevator.stairs_bd70[1] = normal;
    hit = simtower::original_vertical_transport_hit_from_client(
        tower, 224, 62, 800, 3800);
    assert(hit.transport_index == 0U);

    // 10c0:04e0 clears the selected record and then derives cf10 entirely
    // from the remaining records, including overlapping Stair/Escalator bits.
    normal.word_6 = 9U;
    normal.word_8 = 7U;
    normal.exact_bytes[6] = std::byte{9};
    normal.exact_bytes[8] = std::byte{7};
    auto& escalator = tower.post_elevator.stairs_bd70[1];
    escalator.used = 1U;
    escalator.shape = 0U;
    escalator.floor = 11;
    auto& spanning_stair = tower.post_elevator.stairs_bd70[2];
    spanning_stair.used = 1U;
    spanning_stair.shape = 5U;
    spanning_stair.floor = 10;
    tower.post_elevator.cf10.fill(std::byte{0x7f});
    tower.post_elevator.routes_bff0[0][4] = std::byte{0x5a};
    assert(simtower::commit_original_vertical_transport_demolition(tower, 0U));
    assert(normal.used == 0U && normal.exact_bytes[0] == std::byte{0});
    assert(normal.word_6 == 0U && normal.word_8 == 0U);
    assert(normal.exact_bytes[6] == std::byte{0} &&
           normal.exact_bytes[7] == std::byte{0} &&
           normal.exact_bytes[8] == std::byte{0} &&
           normal.exact_bytes[9] == std::byte{0});
    assert(tower.post_elevator.cf10[9] == std::byte{0});
    assert(tower.post_elevator.cf10[10] == std::byte{2});
    assert(tower.post_elevator.cf10[11] == std::byte{3});
    assert(tower.post_elevator.cf10[12] == std::byte{2});
    assert(tower.post_elevator.cf10[13] == std::byte{0});
    assert(tower.post_elevator.routes_bff0[0][0] == std::byte{0});
    assert(tower.post_elevator.routes_bff0[0][1] == std::byte{1});
    assert(tower.post_elevator.routes_bff0[0][2] == std::byte{13});
    assert(tower.post_elevator.routes_bff0[0][3] == std::byte{10});
    assert(tower.post_elevator.routes_bff0[0][4] == std::byte{0x5a});
    assert(!simtower::commit_original_vertical_transport_demolition(tower, 0U));
  }

  {
    // Complete 10c0:04e0 transaction: 1218 dispatches an active person while
    // bd70 is still live, then the record and all derived routes are cleared.
    auto tower = simtower::make_original_new_tdt();
    tower.people_count = 1U;
    tower.people.resize(1U);
    auto& person = tower.people[0].exact_bytes;
    person[0] = std::byte{10};
    person[2] = std::byte{0};
    person[3] = std::byte{0};
    person[4] = std::byte{15};
    person[5] = std::byte{3};
    person[7] = std::byte{10};
    person[8] = std::byte{0};
    auto& stair = tower.post_elevator.stairs_bd70[0];
    stair.used = 1U;
    stair.shape = 1U;
    stair.floor = 10;
    stair.word_8 = 1U;
    stair.exact_bytes[0] = std::byte{1};
    stair.exact_bytes[1] = std::byte{1};
    stair.exact_bytes[4] = std::byte{10};
    stair.exact_bytes[8] = std::byte{1};
    tower.post_elevator.cf10[10] = std::byte{2};

    const auto result = simtower::remove_original_vertical_transport(
        tower, 0U, part);
    assert(result.removed);
    assert(result.family_dispatches.size() == 1U);
    assert(result.family_dispatches[0].person_index == 0U);
    assert(result.family_dispatches[0].status == simtower::
               OriginalPersonFamilyDispatchStatus::housekeeping);
    assert(result.family_dispatches[0].source == simtower::
               OriginalPersonFamilyDispatchSource::vertical_transport_1218);
    assert(tower.people[0].exact_bytes[5] == std::byte{1});
    assert(tower.post_elevator.stairs_bd70[0].used == 0U);
    assert(tower.post_elevator.cf10[10] == std::byte{0});

    const auto before = simtower::serialize_original_tdt(tower);
    assert(!simtower::remove_original_vertical_transport(
                tower, 64U, part).removed);
    assert(simtower::serialize_original_tdt(tower) == before);
  }

  {
    // 10a0:1397 returns a shaft hit throughout bottom-1..top+1 and uses
    // 1090:227b's inset, motion-adjusted car rectangle for the optional car.
    auto tower = simtower::make_original_new_tdt();
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.x = 120U;
    elevator.bottom_floor = 10;
    elevator.top_floor = 12;
    auto& first_car = elevator.car_records[0].exact_bytes;
    first_car[0] = std::byte{11};
    first_car[1] = std::byte{1};
    first_car[4] = std::byte{1};
    first_car[15] = std::byte{1};
    auto hit = simtower::original_elevator_hit_from_client(
        tower, 170, 100, 800, 3800);
    assert(hit == simtower::OriginalElevatorHit({true, 0U, 11, 0}));
    auto magnifier = simtower::select_original_magnifier_target(
        tower, 170, 100, 800, 3800);
    assert(magnifier.kind == simtower::OriginalMagnifierTargetKind::
                                  elevator_car_information);
    assert(magnifier.dialog_id == 762U &&
           magnifier.elevator_index == 0U &&
           magnifier.elevator_car_index == 0);

    // Direct 10a0:04b2 coverage: PTINRECT excludes the inset right edge; an
    // empty in-span shaft with word_3c zero falls through, while nonzero opens
    // Elevator Control.
    hit = simtower::original_elevator_hit_from_client(
        tower, 190, 100, 800, 3800);
    assert(hit == simtower::OriginalElevatorHit({true, 0U, 11, -1}));
    elevator.word_3c = 1U;
    magnifier = simtower::select_original_magnifier_target(
        tower, 190, 100, 800, 3800);
    assert(magnifier.kind ==
           simtower::OriginalMagnifierTargetKind::elevator_control);
    assert(magnifier.dialog_id == 400U &&
           magnifier.elevator_index == 0U);
    elevator.word_3c = 0U;
    assert(!simtower::select_original_magnifier_target(
                tower, 190, 100, 800, 3800).handled());
    assert(simtower::original_elevator_hit_from_client(
               tower, 170, 160, 800, 3800).hit);  // bottom-1
    assert(!simtower::original_elevator_hit_from_client(
                tower, 170, 196, 800, 3800).hit);  // bottom-2

    // Overlapping active car rectangles overwrite rather than breaking.
    elevator.car_records[1].exact_bytes = first_car;
    hit = simtower::original_elevator_hit_from_client(
        tower, 170, 100, 800, 3800);
    assert(hit.car_index == 1);

    // Type-zero express shafts use the distinct six-cell width branch.
    elevator.type = 0U;
    hit = simtower::original_elevator_hit_from_client(
        tower, 206, 100, 800, 3800);
    assert(hit.hit);
    assert(!simtower::original_elevator_hit_from_client(
                tower, 208, 100, 800, 3800).hit);
  }

  {
    // Mode-one Finger gates from 10a0:0000/0085/102d/1296. These are pure
    // pre-routing checks, so no native dialog or executable is involved.
    auto tower = simtower::make_original_new_tdt();
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.word_3c = 1U;
    elevator.type = 0U;
    elevator.bottom_floor = 9;
    elevator.top_floor = 40;
    tower.header.lobby_height = 3U;

    assert(simtower::original_elevator_service_floor_gate(tower, 24U, 10) ==
           simtower::OriginalElevatorServiceFloorGate::invalid_elevator);
    assert(simtower::original_elevator_service_floor_gate(tower, 0U, -1) ==
           simtower::OriginalElevatorServiceFloorGate::invalid_floor);
    assert(simtower::original_elevator_service_floor_gate(tower, 0U, 8) ==
           simtower::OriginalElevatorServiceFloorGate::outside_shaft);

    elevator.car_records[7].exact_bytes[15] = std::byte{1};
    elevator.car_home_floors[7] = std::byte{20};
    assert(simtower::original_elevator_service_floor_gate(tower, 0U, 20) ==
           simtower::OriginalElevatorServiceFloorGate::active_car_home);
    elevator.car_records[7].exact_bytes[15] = std::byte{0};

    // Type-zero landings above ten are only 24,39,...; Lobby upper stories
    // are 11 and 12 for lobby_height three. Existing stops skip exclusions.
    assert(simtower::original_elevator_service_floor_gate(tower, 0U, 24) ==
           simtower::OriginalElevatorServiceFloorGate::eligible);
    assert(simtower::original_elevator_service_floor_gate(tower, 0U, 39) ==
           simtower::OriginalElevatorServiceFloorGate::eligible);
    assert(simtower::original_elevator_service_floor_gate(tower, 0U, 23) ==
           simtower::OriginalElevatorServiceFloorGate::forbidden_new_stop);
    assert(simtower::original_elevator_service_floor_gate(tower, 0U, 11) ==
           simtower::OriginalElevatorServiceFloorGate::forbidden_new_stop);
    elevator.serviced_floors[24] = std::byte{1};
    assert(simtower::original_elevator_service_floor_gate(tower, 0U, 24) ==
           simtower::OriginalElevatorServiceFloorGate::eligible);
    elevator.word_3c = 0U;
    assert(simtower::original_elevator_service_floor_gate(tower, 0U, 23) ==
           simtower::OriginalElevatorServiceFloorGate::inactive_shaft);
  }

  {
    // Captured upper-cap path 10a0:0819. Automatic Lobby stories already
    // cover the shaft, cost nothing, receive no service byte, and still gain
    // the contiguous standard-elevator floor records persisted by 10d0.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 3, costs).succeeded());
    assert(simtower::build_original_standard_elevator(
               tower, 10, 120, costs, part).succeeded());
    const auto balance = tower.header.balance;
    const auto result = simtower::extend_original_elevator_shaft(
        tower, 0U, 12, costs);
    assert(result.succeeded());
    assert(result.cost == 0 && result.target_floor == 12);
    assert(!result.span_clamped);
    const auto& elevator = tower.elevators[0];
    assert(elevator.top_floor == 12 && elevator.bottom_floor == 10);
    assert(elevator.serviced_floors[10] == std::byte{1});
    assert(elevator.serviced_floors[11] == std::byte{0});
    assert(elevator.serviced_floors[12] == std::byte{0});
    assert(elevator.floor_records.size() == 3U);
    for (std::size_t index = 0; index < 3U; ++index) {
      assert(elevator.floor_records[index].floor ==
             static_cast<std::int8_t>(10 + index));
      assert(elevator.floor_records[index].mapped_index ==
             static_cast<std::int16_t>(index));
    }
    assert(tower.header.balance == balance);
    assert_lobby(tower.floors[11], 100, 200);
    assert_lobby(tower.floors[12], 100, 200);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.elevators[0].top_floor == 12);
    assert(reparsed.elevators[0].floor_records.size() == 3U);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Type-zero upper extension creates physical floor coverage on every
    // story but serializes waiting records and enables stops only at the
    // executable's sparse express sequence.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_elevator(
               tower, 42U, 10, 120, costs, part).succeeded());
    const auto result = simtower::extend_original_elevator_shaft(
        tower, 0U, 24, costs);
    assert(result.succeeded());
    assert(result.cost == 14 * 6 * 5);
    const auto& elevator = tower.elevators[0];
    assert(elevator.top_floor == 24 && elevator.bottom_floor == 10);
    assert(elevator.serviced_floors[11] == std::byte{0});
    assert(elevator.serviced_floors[23] == std::byte{0});
    assert(elevator.serviced_floors[24] == std::byte{1});
    assert(elevator.floor_records.size() == 2U);
    assert(elevator.floor_records[0].floor == 10);
    assert(elevator.floor_records[0].mapped_index == 9);
    assert(elevator.floor_records[1].floor == 24);
    assert(elevator.floor_records[1].mapped_index == 10);
    for (std::int16_t floor = 11; floor <= 24; ++floor) {
      const auto& coverage = tower.floors[static_cast<std::size_t>(floor)];
      assert(coverage.left_edge == 120 && coverage.right_edge == 126);
      assert(coverage.tenants.size() == 1U);
      assert(coverage.tenants[0].type == 0);
    }

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.elevators[0].floor_records.size() == 2U);
    assert(reparsed.elevators[0].floor_records[1].floor == 24);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Captured lower-cap path 10a0:0b87 prepends and remaps contiguous
    // service-elevator floor records while constructing downward coverage.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_elevator(
               tower, 43U, 10, 120, costs, part).succeeded());
    const auto result = simtower::extend_original_elevator_shaft(
        tower, 0U, 8, costs);
    assert(result.succeeded() && result.cost == 40);
    const auto& elevator = tower.elevators[0];
    assert(elevator.bottom_floor == 8 && elevator.top_floor == 10);
    assert(elevator.floor_records.size() == 3U);
    for (std::size_t index = 0; index < 3U; ++index) {
      assert(elevator.floor_records[index].floor ==
             static_cast<std::int8_t>(8 + index));
      assert(elevator.floor_records[index].mapped_index ==
             static_cast<std::int16_t>(index));
      assert(elevator.serviced_floors[8 + index] == std::byte{1});
    }
    assert(tower.floors[8].left_edge == 120);
    assert(tower.floors[8].right_edge == 124);
    assert(tower.floors[9].left_edge == 120);
    assert(tower.floors[9].right_edge == 124);

    const auto before_zero = simtower::serialize_original_tdt(tower);
    const auto invalid = simtower::extend_original_elevator_shaft(
        tower, 0U, 0, costs);
    assert(invalid.status ==
           simtower::OriginalConstructionStatus::invalid_floor);
    assert(simtower::serialize_original_tdt(tower) == before_zero);
  }

  {
    // The inward upper-cap path is exact for zero-person floors: endpoints,
    // service bytes, active-car home/current fields, owner counts, route data,
    // and the serialized floor-record stream all shrink atomically.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_standard_elevator(
               tower, 10, 120, costs, part).succeeded());
    assert(simtower::extend_original_elevator_shaft(
               tower, 0U, 14, costs).succeeded());
    const auto balance = tower.header.balance;
    auto& elevator = tower.elevators[0];
    auto& car = elevator.car_records[0].exact_bytes;
    elevator.car_home_floors[0] = std::byte{14};
    car[0] = std::byte{14};
    car[1] = std::byte{5};
    car[2] = std::byte{6};
    car[6] = std::byte{14};
    car[10] = std::byte{2};
    elevator.block_2a2[13] = std::byte{1};
    elevator.block_31a[14] = std::byte{1};

    const auto result =
        simtower::shrink_original_elevator_shaft_without_people(
            tower, 0U, simtower::OriginalElevatorShaftEnd::upper, 12);
    assert(result.succeeded() && result.cost == 0);
    assert(tower.header.balance == balance);
    const auto& shrunk = tower.elevators[0];
    const auto& shrunk_car = shrunk.car_records[0].exact_bytes;
    assert(shrunk.top_floor == 12 && shrunk.bottom_floor == 10);
    assert(shrunk.serviced_floors[13] == std::byte{0});
    assert(shrunk.serviced_floors[14] == std::byte{0});
    assert(shrunk.floor_records.size() == 3U);
    assert(shrunk.car_home_floors[0] == std::byte{12});
    assert(shrunk_car[0] == std::byte{12});
    assert(shrunk_car[1] == std::byte{0} &&
           shrunk_car[2] == std::byte{0});
    assert(shrunk_car[6] == std::byte{12});
    assert(shrunk.block_2a2[13] == std::byte{0});
    assert(shrunk.block_31a[14] == std::byte{0});
    assert(shrunk_car[10] == std::byte{0});

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.elevators[0].top_floor == 12);
    assert(reparsed.elevators[0].floor_records.size() == 3U);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Lower-cap shrinking preserves the upper records and remaps their
    // contiguous indices. Either a car destination or waiting ring blocks the
    // zero-person API before any byte changes.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_elevator(
               tower, 43U, 10, 120, costs, part).succeeded());
    assert(simtower::extend_original_elevator_shaft(
               tower, 0U, 8, costs).succeeded());
    tower.elevators[0].car_home_floors[0] = std::byte{8};
    tower.elevators[0].car_records[0].exact_bytes[0] = std::byte{8};
    tower.elevators[0].car_records[0].exact_bytes[1] = std::byte{4};
    tower.elevators[0].car_records[0].exact_bytes[2] = std::byte{3};
    tower.elevators[0].car_records[0].exact_bytes[6] = std::byte{8};

    auto result = simtower::shrink_original_elevator_shaft_without_people(
        tower, 0U, simtower::OriginalElevatorShaftEnd::lower, 9);
    assert(result.succeeded());
    assert(tower.elevators[0].bottom_floor == 9 &&
           tower.elevators[0].top_floor == 10);
    assert(tower.elevators[0].floor_records.size() == 2U);
    assert(tower.elevators[0].floor_records[0].floor == 9);
    assert(tower.elevators[0].floor_records[0].mapped_index == 0);
    assert(tower.elevators[0].floor_records[1].floor == 10);
    assert(tower.elevators[0].floor_records[1].mapped_index == 1);
    assert(tower.elevators[0].car_home_floors[0] == std::byte{9});
    assert(tower.elevators[0].car_records[0].exact_bytes[0] == std::byte{9});
    assert(tower.elevators[0].car_records[0].exact_bytes[6] == std::byte{9});

    assert(simtower::extend_original_elevator_shaft(
               tower, 0U, 8, costs).succeeded());
    tower.elevators[0].car_records[0].exact_bytes[226U + 8U] = std::byte{1};
    const auto occupied_before = simtower::serialize_original_tdt(tower);
    result = simtower::shrink_original_elevator_shaft_without_people(
        tower, 0U, simtower::OriginalElevatorShaftEnd::lower, 9);
    assert(result.status ==
           simtower::OriginalConstructionStatus::person_cleanup_required);
    assert(simtower::serialize_original_tdt(tower) == occupied_before);
    tower.elevators[0].car_records[0].exact_bytes[226U + 8U] = std::byte{0};
    tower.elevators[0].floor_records[0].exact_bytes[0] = std::byte{1};
    const auto waiting_before = simtower::serialize_original_tdt(tower);
    result = simtower::shrink_original_elevator_shaft_without_people(
        tower, 0U, simtower::OriginalElevatorShaftEnd::lower, 9);
    assert(result.status ==
           simtower::OriginalConstructionStatus::person_cleanup_required);
    assert(simtower::serialize_original_tdt(tower) == waiting_before);
  }

  {
    // Passenger-bearing upper shrinking preserves 0819's global order:
    // every removed floor's 1625 waiting ring (top down), endpoint/record
    // compaction, then every Direct 10a0:14fa car arrival (top down).
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_standard_elevator(
               tower, 10, 120, costs, part).succeeded());
    assert(simtower::extend_original_elevator_shaft(
               tower, 0U, 14, costs).succeeded());
    tower.people_count = 4U;
    tower.people.resize(4U);
    for (auto& person : tower.people) {
      person.exact_bytes[4] = std::byte{14};
      person.exact_bytes[5] = std::byte{0};
    }
    auto& elevator = tower.elevators[0];
    elevator.capacity = 4U;
    const auto queue = [&](std::int8_t floor, std::uint32_t person_index) {
      const auto found = std::find_if(
          elevator.floor_records.begin(), elevator.floor_records.end(),
          [&](const auto& record) { return record.floor == floor; });
      assert(found != elevator.floor_records.end());
      found->exact_bytes[0] = std::byte{1};
      found->exact_bytes[1] = std::byte{0};
      store_u32(found->exact_bytes, 4U, person_index,
                tower.header.byte_swapped);
    };
    queue(14, 0U);
    queue(13, 1U);
    auto& car = elevator.car_records[0].exact_bytes;
    car[3] = std::byte{2};
    car[12] = std::byte{2};
    car[184] = std::byte{14};
    car[185] = std::byte{13};
    car[226U + 14U] = std::byte{1};
    car[226U + 13U] = std::byte{1};
    store_u32(car, 16U, 2U, tower.header.byte_swapped);
    store_u32(car, 20U, 3U, tower.header.byte_swapped);
    auto cleanup_part = part;
    cleanup_part.words_00_to_40[2U] = 17U;

    const auto result = simtower::shrink_original_elevator_shaft(
        tower, 0U, simtower::OriginalElevatorShaftEnd::upper, 12,
        cleanup_part);
    assert(result.succeeded());
    assert(result.waiting_passengers == 2U &&
           result.car_passengers == 2U);
    assert(result.family_dispatches.size() == 4U);
    for (std::size_t index = 0U; index < 4U; ++index) {
      assert(result.family_dispatches[index].person_index == index);
    }
    assert(result.family_dispatches[0].source == simtower::
               OriginalPersonFamilyDispatchSource::dispatcher_16ab);
    assert(result.family_dispatches[1].source == simtower::
               OriginalPersonFamilyDispatchSource::dispatcher_16ab);
    assert(result.family_dispatches[2].source == simtower::
               OriginalPersonFamilyDispatchSource::elevator_car_0883);
    assert(result.family_dispatches[3].source == simtower::
               OriginalPersonFamilyDispatchSource::elevator_car_0883);
    assert(result.family_dispatches[0].status == simtower::
               OriginalPersonFamilyDispatchStatus::no_handler);
    assert(result.family_dispatches[2].status == simtower::
               OriginalPersonFamilyDispatchStatus::security);
    assert(tower.people[0].exact_bytes[10] == std::byte{0});
    assert(tower.people[1].exact_bytes[10] == std::byte{0});
    assert(tower.people[0].exact_bytes[12] == std::byte{17});
    assert(tower.people[1].exact_bytes[12] == std::byte{17});
    assert(tower.elevators[0].top_floor == 12);
    assert(tower.elevators[0].floor_records.size() == 3U);
    assert(tower.elevators[0].car_records[0]
               .exact_bytes[226U + 14U] == std::byte{0});
    assert(tower.elevators[0].car_records[0]
               .exact_bytes[226U + 13U] == std::byte{0});
  }

  {
    // Lower shrinking uses the same global wait-before-car split while
    // walking removed floors upward and preserving/remapping retained data.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_elevator(
               tower, 43U, 10, 120, costs, part).succeeded());
    assert(simtower::extend_original_elevator_shaft(
               tower, 0U, 7, costs).succeeded());
    tower.people_count = 4U;
    tower.people.resize(4U);
    for (auto& person : tower.people) {
      person.exact_bytes[4] = std::byte{14};
      person.exact_bytes[5] = std::byte{0};
    }
    auto& elevator = tower.elevators[0];
    elevator.capacity = 4U;
    const auto queue = [&](std::int8_t floor, std::uint32_t person_index) {
      const auto found = std::find_if(
          elevator.floor_records.begin(), elevator.floor_records.end(),
          [&](const auto& record) { return record.floor == floor; });
      assert(found != elevator.floor_records.end());
      found->exact_bytes[0] = std::byte{1};
      store_u32(found->exact_bytes, 4U, person_index,
                tower.header.byte_swapped);
    };
    queue(7, 0U);
    queue(8, 1U);
    const auto retained = std::find_if(
        elevator.floor_records.begin(), elevator.floor_records.end(),
        [](const auto& record) { return record.floor == 9; });
    assert(retained != elevator.floor_records.end());
    retained->exact_bytes[323] = std::byte{0x5a};
    auto& car = elevator.car_records[0].exact_bytes;
    car[3] = std::byte{2};
    car[12] = std::byte{2};
    car[184] = std::byte{7};
    car[185] = std::byte{8};
    car[226U + 7U] = std::byte{1};
    car[226U + 8U] = std::byte{1};
    store_u32(car, 16U, 2U, tower.header.byte_swapped);
    store_u32(car, 20U, 3U, tower.header.byte_swapped);

    const auto result = simtower::shrink_original_elevator_shaft(
        tower, 0U, simtower::OriginalElevatorShaftEnd::lower, 9, part);
    assert(result.succeeded());
    assert(result.family_dispatches.size() == 4U);
    for (std::size_t index = 0U; index < 4U; ++index) {
      assert(result.family_dispatches[index].person_index == index);
    }
    assert(tower.elevators[0].bottom_floor == 9);
    assert(tower.elevators[0].floor_records.size() == 2U);
    assert(tower.elevators[0].floor_records[0].floor == 9);
    assert(tower.elevators[0].floor_records[0].mapped_index == 0);
    assert(tower.elevators[0].floor_records[0].exact_bytes[323] ==
           std::byte{0x5a});
  }

  {
    // A later malformed removed-floor ring rolls back earlier translated
    // callbacks and structural changes because the native transaction works
    // on a document copy and publishes no host requests on failure.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_standard_elevator(
               tower, 10, 120, costs, part).succeeded());
    assert(simtower::extend_original_elevator_shaft(
               tower, 0U, 14, costs).succeeded());
    tower.people_count = 1U;
    tower.people.resize(1U);
    tower.people[0].exact_bytes[4] = std::byte{14};
    auto& records = tower.elevators[0].floor_records;
    const auto floor14 = std::find_if(
        records.begin(), records.end(),
        [](const auto& record) { return record.floor == 14; });
    const auto floor13 = std::find_if(
        records.begin(), records.end(),
        [](const auto& record) { return record.floor == 13; });
    assert(floor14 != records.end() && floor13 != records.end());
    floor14->exact_bytes[0] = std::byte{1};
    store_u32(floor14->exact_bytes, 4U, 0U, tower.header.byte_swapped);
    floor13->exact_bytes[0] = std::byte{1};
    store_u32(floor13->exact_bytes, 4U, 99U, tower.header.byte_swapped);
    const auto before = simtower::serialize_original_tdt(tower);

    const auto result = simtower::shrink_original_elevator_shaft(
        tower, 0U, simtower::OriginalElevatorShaftEnd::upper, 12, part);
    assert(result.shaft.status ==
           simtower::OriginalConstructionStatus::person_cleanup_required);
    assert(result.family_dispatches.empty());
    assert(result.waiting_passengers == 0U && result.car_passengers == 0U);
    assert(simtower::serialize_original_tdt(tower) == before);
  }

  {
    // Ordinary/service shafts clamp to 29 floors only after collision and
    // full requested-range funds preflight. Commit charges the clamped range.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_standard_elevator(
               tower, 10, 120, costs, part).succeeded());
    const auto balance = tower.header.balance;
    const auto result = simtower::extend_original_elevator_shaft(
        tower, 0U, 45, costs);
    assert(result.succeeded() && result.span_clamped);
    assert(result.target_floor == 39);
    assert(result.cost == 29 * 4 * 5);
    assert(tower.header.balance == balance - result.cost);
    assert(tower.elevators[0].top_floor == 39);
    assert(tower.elevators[0].floor_records.size() == 30U);
  }

  {
    // Both preflight failures are atomic. The funds path reports the exact
    // requested exposed-floor preview, and 10e8 sees an otherwise separate
    // shaft through the edited shaft's expanded horizontal rectangle.
    auto poor = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               poor, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_standard_elevator(
               poor, 10, 120, costs, part).succeeded());
    poor.header.balance = 19;
    const auto poor_before = simtower::serialize_original_tdt(poor);
    auto result = simtower::extend_original_elevator_shaft(
        poor, 0U, 11, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::insufficient_funds);
    assert(result.cost == 20);
    assert(simtower::serialize_original_tdt(poor) == poor_before);

    auto blocked = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               blocked, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_standard_elevator(
               blocked, 10, 120, costs, part).succeeded());
    auto& obstacle = blocked.elevators[1];
    obstacle.used = 1U;
    obstacle.word_3c = 1U;
    obstacle.type = 1U;
    obstacle.x = 130U;
    obstacle.bottom_floor = 20;
    obstacle.top_floor = 20;
    const auto blocked_before = simtower::serialize_original_tdt(blocked);
    result = simtower::extend_original_elevator_shaft(
        blocked, 0U, 20, costs);
    assert(result.status == simtower::OriginalConstructionStatus::occupied);
    assert(simtower::serialize_original_tdt(blocked) == blocked_before);
  }

  {
    // The safe add-stop half of 10a0:0085 writes boolean one and immediately
    // rebuilds 11b0's persisted route graph. Existing stops are not accepted
    // here because their original path must first execute 10a0:14cc cleanup.
    auto tower = simtower::make_original_new_tdt();
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.word_3c = 1U;
    elevator.type = 1U;
    elevator.x = 10U;
    elevator.bottom_floor = 10;
    elevator.top_floor = 20;
    elevator.block_c2.fill(std::byte{0xa5});
    elevator.serviced_floors[10] = std::byte{1};

    assert(simtower::add_original_elevator_service_floor(tower, 0U, 20));
    assert(elevator.serviced_floors[20] == std::byte{1});
    assert(load_u32(elevator.block_c2, 19U * 4U, false) == 0U);
    assert(!simtower::add_original_elevator_service_floor(tower, 0U, 20));
    assert(!simtower::add_original_elevator_service_floor(tower, 0U, 21));

    elevator.word_3c = 0U;
    assert(!simtower::add_original_elevator_service_floor(tower, 0U, 15));
    assert(elevator.serviced_floors[15] == std::byte{0});
  }

  {
    // Zero-person removal preserves 0085's graph-before-14cc order and then
    // performs 154a's exact owner/count/recompute tail for each active car.
    auto tower = simtower::make_original_new_tdt();
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.word_3c = 1U;
    elevator.type = 1U;
    elevator.bottom_floor = 10;
    elevator.top_floor = 20;
    elevator.serviced_floors[10] = std::byte{1};
    elevator.serviced_floors[15] = std::byte{1};
    auto& car = elevator.car_records[0].exact_bytes;
    car[15] = std::byte{1};
    car[0] = std::byte{10};
    elevator.car_home_floors[0] = std::byte{10};
    car[10] = std::byte{2};
    car[11] = std::byte{0};
    elevator.block_2a2[15] = std::byte{1};
    elevator.block_31a[15] = std::byte{1};

    simtower::OriginalTdtElevatorFloorRecord record{};
    record.floor = 15;
    record.mapped_index = simtower::original_elevator_floor_record_index(
        elevator.type, elevator.bottom_floor, elevator.top_floor, 15);
    elevator.floor_records.push_back(record);

    assert(!simtower::original_elevator_service_floor_has_people(
        tower, 0U, 15));
    assert(simtower::remove_original_elevator_service_floor_without_people(
        tower, 0U, 15));
    assert(elevator.serviced_floors[15] == std::byte{0});
    assert(elevator.block_2a2[15] == std::byte{0});
    assert(elevator.block_31a[15] == std::byte{0});
    assert(car[10] == std::byte{0} && car[11] == std::byte{0});
    assert(car[5] == std::byte{10});

    // Either active-car destination occupancy or either waiting-ring count
    // blocks the mutation without changing the serviced byte.
    elevator.serviced_floors[15] = std::byte{1};
    car[226U + 15U] = std::byte{1};
    assert(simtower::original_elevator_service_floor_has_people(
        tower, 0U, 15));
    assert(!simtower::remove_original_elevator_service_floor_without_people(
        tower, 0U, 15));
    assert(elevator.serviced_floors[15] == std::byte{1});
    car[226U + 15U] = std::byte{0};
    record = elevator.floor_records[0];
    elevator.floor_records[0].exact_bytes[2] = std::byte{1};
    assert(simtower::original_elevator_service_floor_has_people(
        tower, 0U, 15));
    assert(!simtower::remove_original_elevator_service_floor_without_people(
        tower, 0U, 15));
  }

  {
    // Person-bearing 0085 clears the service byte and route graph before
    // 14cc pops car passengers, releases owners, and drains up/down rings.
    // 0883 calls Security for the car; 16ab deliberately skips queued type14.
    auto tower = simtower::make_original_new_tdt();
    tower.people_count = 2U;
    tower.people.resize(2U);
    for (auto& person : tower.people) {
      person.exact_bytes[4] = std::byte{14};
      person.exact_bytes[5] = std::byte{0};
      person.exact_bytes[7] = std::byte{15};
    }
    tower.people[1].exact_bytes[10] = std::byte{100};
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.word_3c = 1U;
    elevator.type = 1U;
    elevator.capacity = 1U;
    elevator.bottom_floor = 10;
    elevator.top_floor = 20;
    elevator.serviced_floors[10] = std::byte{1};
    elevator.serviced_floors[15] = std::byte{1};
    auto& car = elevator.car_records[0].exact_bytes;
    car[0] = std::byte{10};
    car[3] = std::byte{1};
    car[12] = std::byte{1};
    car[15] = std::byte{1};
    car[184U] = std::byte{15};
    car[226U + 15U] = std::byte{1};
    car[16] = std::byte{0};
    car[17] = std::byte{0};
    car[18] = std::byte{0};
    car[19] = std::byte{0};
    elevator.car_home_floors[0] = std::byte{10};
    simtower::OriginalTdtElevatorFloorRecord record{};
    record.floor = 15;
    record.mapped_index = simtower::original_elevator_floor_record_index(
        elevator.type, elevator.bottom_floor, elevator.top_floor, 15);
    record.exact_bytes[0] = std::byte{1};
    record.exact_bytes[4] = std::byte{1};
    elevator.floor_records.push_back(record);
    simtower::OriginalPartTable part{};
    part.words_00_to_40[2U] = 17U;

    const auto result = simtower::remove_original_elevator_service_floor(
        tower, 0U, 15, part);
    assert(result.cleanup.status == simtower::
               OriginalElevatorFloorPeopleCleanupStatus::cleaned);
    assert(result.cleanup.car_passengers == 1U &&
           result.cleanup.waiting_passengers == 1U);
    assert(result.family_dispatches.size() == 2U);
    assert(result.family_dispatches[0].status == simtower::
               OriginalPersonFamilyDispatchStatus::security);
    assert(result.family_dispatches[1].status == simtower::
               OriginalPersonFamilyDispatchStatus::no_handler);
    auto& removed_elevator = tower.elevators[0];
    auto& removed_car = removed_elevator.car_records[0].exact_bytes;
    assert(removed_elevator.serviced_floors[15] == std::byte{0});
    assert(removed_car[3] == std::byte{0} &&
           removed_car[12] == std::byte{0});
    assert(removed_car[226U + 15U] == std::byte{0});
    assert(removed_elevator.floor_records[0].exact_bytes[0] == std::byte{0});
    assert(tower.people[1].exact_bytes[10] == std::byte{0} &&
           tower.people[1].exact_bytes[11] == std::byte{0});
    assert(tower.people[1].exact_bytes[12] == std::byte{17} &&
           tower.people[1].exact_bytes[13] == std::byte{0});

    // A malformed queued index fails on the working copy and leaves the
    // caller's serviced byte/ring byte-exact.
    removed_elevator.serviced_floors[15] = std::byte{1};
    removed_elevator.floor_records[0].exact_bytes[0] = std::byte{1};
    removed_elevator.floor_records[0].exact_bytes[1] = std::byte{0};
    removed_elevator.floor_records[0].exact_bytes[4] = std::byte{99};
    removed_elevator.floor_records[0].exact_bytes[5] = std::byte{0};
    removed_elevator.floor_records[0].exact_bytes[6] = std::byte{0};
    removed_elevator.floor_records[0].exact_bytes[7] = std::byte{0};
    const auto before = simtower::serialize_original_tdt(tower);
    const auto malformed = simtower::remove_original_elevator_service_floor(
        tower, 0U, 15, part);
    assert(malformed.cleanup.status ==
           simtower::OriginalElevatorFloorPeopleCleanupStatus::invalid);
    assert(simtower::serialize_original_tdt(tower) == before);
  }

  {
    // Exact 11b0:0cfe connectivity class and 10a0:0179 confirmation scan.
    auto tower = simtower::make_original_new_tdt();
    auto& source = tower.elevators[0];
    source.used = 1U;
    source.type = 1U;
    source.bottom_floor = 10;
    source.top_floor = 12;
    source.serviced_floors[10] = std::byte{1};

    // With no Stair/Escalator direction at or below the floor, the helper's
    // first branch passes without inspecting alternate shafts.
    assert(simtower::original_elevator_floor_connected_for_shaft_removal(
        tower, 0U, 10));
    assert(!simtower::original_elevator_shaft_demolition_requires_confirmation(
        tower, 0U));

    tower.post_elevator.cf10[10] = std::byte{1};
    assert(!simtower::original_elevator_floor_connected_for_shaft_removal(
        tower, 0U, 10));
    assert(simtower::original_elevator_shaft_demolition_requires_confirmation(
        tower, 0U));

    auto& alternate = tower.elevators[1];
    alternate.used = 1U;
    alternate.type = 0U;  // all non-type-2 shafts share one class
    alternate.serviced_floors[10] = std::byte{1};
    assert(simtower::original_elevator_floor_connected_for_shaft_removal(
        tower, 0U, 10));
    alternate.type = 2U;
    assert(!simtower::original_elevator_floor_connected_for_shaft_removal(
        tower, 0U, 10));

    source.serviced_floors[10] = std::byte{0};
    assert(simtower::original_elevator_floor_connected_for_shaft_removal(
        tower, 0U, 10));

    // 10a0:0201 returns the shaft's word_3c for a no-car hit inside its
    // stored span. Zero is a deliberate fall-through to the later 1058
    // Stair/Escalator and facility legs; nonzero consumes the click.
    using BulldozerAction = simtower::OriginalElevatorBulldozerAction;
    assert(simtower::original_elevator_bulldozer_action(
               false, -1, 1U, 10, 10, 12, 1U) ==
           BulldozerAction::miss);
    assert(simtower::original_elevator_bulldozer_action(
               true, -1, 1U, 10, 10, 12, 0U) ==
           BulldozerAction::pass_through);
    assert(simtower::original_elevator_bulldozer_action(
               true, -1, 1U, 10, 10, 12, 1U) ==
           BulldozerAction::consume);
    assert(simtower::original_elevator_bulldozer_action(
               true, 1, 2U, 10, 10, 12, 1U) ==
           BulldozerAction::remove_car);
    assert(simtower::original_elevator_bulldozer_action(
               true, 0, 1U, 10, 10, 12, 1U) ==
           BulldozerAction::remove_shaft);
    assert(simtower::original_elevator_bulldozer_action(
               true, -1, 1U, 9, 10, 12, 1U) ==
           BulldozerAction::remove_shaft);

    // 10a0:0201 protects the last car; the multi-car post-cleanup commit
    // changes only the selected active byte and header car count.
    source.cars = 2U;
    source.car_records[0].exact_bytes[15] = std::byte{1};
    source.car_records[1].exact_bytes[15] = std::byte{1};
    assert(simtower::commit_original_elevator_car_demolition(tower, 0U, 1U));
    assert(source.cars == 1U);
    assert(source.car_records[1].exact_bytes[15] == std::byte{0});
    assert(!simtower::commit_original_elevator_car_demolition(tower, 0U, 1U));
    assert(!simtower::commit_original_elevator_car_demolition(tower, 0U, 0U));
  }

  {
    // 10a0:036e cleans only the selected car on every shaft floor, retires
    // it, then immediately reassigns nonempty unowned rings to a survivor.
    auto tower = simtower::make_original_new_tdt();
    tower.people_count = 2U;
    tower.people.resize(2U);
    for (auto& person : tower.people) {
      person.exact_bytes[4] = std::byte{14};
      person.exact_bytes[5] = std::byte{0};
      person.exact_bytes[7] = std::byte{10};
    }
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.capacity = 1U;
    elevator.cars = 2U;
    elevator.word_3c = 1U;
    elevator.bottom_floor = 10;
    elevator.top_floor = 11;
    elevator.serviced_floors[10] = std::byte{1};
    elevator.serviced_floors[11] = std::byte{1};
    elevator.schedule.fill(std::byte{5});
    elevator.car_home_floors[0] = std::byte{11};
    elevator.car_home_floors[1] = std::byte{10};
    auto& survivor = elevator.car_records[0].exact_bytes;
    survivor[0] = std::byte{11};
    survivor[4] = std::byte{1};
    survivor[15] = std::byte{1};
    auto& removed = elevator.car_records[1].exact_bytes;
    removed[0] = std::byte{10};
    removed[3] = std::byte{1};
    removed[4] = std::byte{1};
    removed[10] = std::byte{1};
    removed[12] = std::byte{1};
    removed[15] = std::byte{1};
    removed[184] = std::byte{10};
    removed[226U + 10U] = std::byte{1};
    store_u32(removed, 16U, 0U, tower.header.byte_swapped);
    elevator.block_2a2[10] = std::byte{2};
    simtower::OriginalTdtElevatorFloorRecord record{};
    record.floor = 10;
    record.mapped_index = 0;
    record.exact_bytes[0] = std::byte{1};
    store_u32(record.exact_bytes, 4U, 1U, tower.header.byte_swapped);
    elevator.floor_records.push_back(record);
    record = {};
    record.floor = 11;
    record.mapped_index = 1;
    elevator.floor_records.push_back(record);

    // Direct 10a0:154a coverage through the selected-car demolition path:
    // only car one is drained/dispatched before its active byte is cleared.
    const auto result = simtower::remove_original_elevator_car(
        tower, 0U, 1U, part);
    assert(result.removed && !result.removed_entire_shaft);
    assert(result.car_passengers == 1U);
    assert(result.family_dispatches.size() == 1U);
    assert(result.family_dispatches[0].status == simtower::
               OriginalPersonFamilyDispatchStatus::security);
    assert(tower.elevators[0].cars == 1U);
    assert(tower.elevators[0].car_records[1].exact_bytes[15] ==
           std::byte{0});
    assert(tower.elevators[0].floor_records[0].exact_bytes[0] ==
           std::byte{1});
    assert(tower.elevators[0].block_2a2[10] == std::byte{1});
    assert(!simtower::remove_original_elevator_car(
                tower, 0U, 0U, part).removed);
  }

  {
    // Full 0201 shaft demolition clears service and routing first, then runs
    // 14cc bottom-to-top before 1090:00d9 resets every persisted car/ring.
    auto tower = simtower::make_original_new_tdt();
    tower.people_count = 2U;
    tower.people.resize(2U);
    for (auto& person : tower.people) {
      person.exact_bytes[4] = std::byte{14};
      person.exact_bytes[5] = std::byte{0};
    }
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.capacity = 1U;
    elevator.cars = 1U;
    elevator.word_3c = 1U;
    elevator.bottom_floor = 10;
    elevator.top_floor = 11;
    elevator.serviced_floors[10] = std::byte{1};
    elevator.serviced_floors[11] = std::byte{1};
    elevator.schedule.fill(std::byte{5});
    elevator.car_home_floors.fill(std::byte{10});
    auto& car = elevator.car_records[0].exact_bytes;
    car[0] = std::byte{10};
    car[3] = std::byte{1};
    car[4] = std::byte{1};
    car[12] = std::byte{1};
    car[15] = std::byte{1};
    car[184] = std::byte{10};
    car[226U + 10U] = std::byte{1};
    store_u32(car, 16U, 0U, tower.header.byte_swapped);
    for (std::int8_t floor = 10; floor <= 11; ++floor) {
      simtower::OriginalTdtElevatorFloorRecord record{};
      record.floor = floor;
      record.mapped_index = static_cast<std::int16_t>(floor - 10);
      if (floor == 11) {
        record.exact_bytes[0] = std::byte{1};
        store_u32(record.exact_bytes, 4U, 1U,
                  tower.header.byte_swapped);
      }
      elevator.floor_records.push_back(record);
    }
    elevator.block_2a2[10] = std::byte{1};
    elevator.block_31a[11] = std::byte{1};

    const auto result = simtower::remove_original_elevator_shaft(
        tower, 0U, part);
    assert(result.removed && result.removed_entire_shaft);
    assert(result.car_passengers == 1U &&
           result.waiting_passengers == 1U);
    assert(result.family_dispatches.size() == 2U);
    assert(result.family_dispatches[0].person_index == 0U);
    assert(result.family_dispatches[0].source == simtower::
               OriginalPersonFamilyDispatchSource::elevator_car_0883);
    assert(result.family_dispatches[1].person_index == 1U);
    assert(result.family_dispatches[1].source == simtower::
               OriginalPersonFamilyDispatchSource::dispatcher_16ab);
    assert(tower.elevators[0].used == 0U);
    assert(tower.elevators[0].block_2a2[10] == std::byte{0});
    assert(tower.elevators[0].block_31a[11] == std::byte{0});
    assert(tower.elevators[0].floor_records[1].exact_bytes[0] ==
           std::byte{0});
    assert(tower.elevators[0].car_records[0].exact_bytes[15] ==
           std::byte{1});

    // An invalid queued person index aborts the copy-backed transaction and
    // leaves the still-used source shaft byte-exact.
    auto malformed = simtower::make_original_new_tdt();
    auto& malformed_elevator = malformed.elevators[0];
    malformed_elevator.used = 1U;
    malformed_elevator.type = 1U;
    malformed_elevator.bottom_floor = 10;
    malformed_elevator.top_floor = 10;
    malformed_elevator.serviced_floors[10] = std::byte{1};
    simtower::OriginalTdtElevatorFloorRecord bad_record{};
    bad_record.floor = 10;
    bad_record.mapped_index = 0;
    bad_record.exact_bytes[0] = std::byte{1};
    store_u32(bad_record.exact_bytes, 4U, 999U,
              malformed.header.byte_swapped);
    malformed_elevator.floor_records.push_back(bad_record);
    const auto before = simtower::serialize_original_tdt(malformed);
    auto malformed_probe = malformed;
    assert(simtower::cleanup_original_elevator_service_floor_people(
               malformed_probe, 0U, 10, 0U, part).cleanup.status ==
           simtower::OriginalElevatorFloorPeopleCleanupStatus::invalid);
    assert(!simtower::remove_original_elevator_shaft(
                malformed, 0U, part).removed);
    assert(simtower::serialize_original_tdt(malformed) == before);
  }

  {
    // Direct 11b0:0763 boundary coverage: zero links stop immediately, bit
    // zero spans six floors, and crossing another parity stops at the exact
    // three-floor cutoff in either direction.
    std::array<std::byte, 0x78> links{};
    links.fill(std::byte{1});
    assert(simtower::original_route_boundary(links, 10, true) == 16);
    assert(simtower::original_route_boundary(links, 10, false) == 4);
    links[12] = std::byte{0};
    assert(simtower::original_route_boundary(links, 10, true) == 12);
    links.fill(std::byte{1});
    links[11] = std::byte{2};
    assert(simtower::original_route_boundary(links, 10, true) == 13);
    links.fill(std::byte{1});
    links[8] = std::byte{2};
    assert(simtower::original_route_boundary(links, 10, false) == 7);
  }

  {
    // Ordered 11b0:049f/00f2 graph rebuild. A type-24 Lobby span creates one
    // db9c transfer joining three elevators and vertical route zero.
    auto tower = simtower::make_original_new_tdt();
    simtower::OriginalTdtTenant lobby{};
    lobby.left = 0U;
    lobby.right = 100U;
    lobby.type = 0x18;
    tower.floors[10].tenants = {lobby};

    auto& first = tower.elevators[0];
    first.used = 1U;
    first.type = 1U;
    first.x = 10U;
    first.serviced_floors[10] = std::byte{1};
    first.serviced_floors[20] = std::byte{1};
    auto& second = tower.elevators[1];
    second.used = 1U;
    second.type = 1U;
    second.x = 20U;
    second.serviced_floors[10] = std::byte{1};
    second.serviced_floors[30] = std::byte{1};
    auto& express_class = tower.elevators[2];
    express_class.used = 1U;
    express_class.type = 2U;
    express_class.x = 30U;
    express_class.serviced_floors[10] = std::byte{1};
    express_class.serviced_floors[40] = std::byte{1};

    // A serialized used elevator contains one 324-byte floor record for
    // every mapped floor in its bottom..top range, independently of service.
    for (auto* elevator : {&first, &second, &express_class}) {
      elevator->bottom_floor = 10;
      elevator->top_floor = 40;
      for (std::int16_t floor = 10; floor <= 40; ++floor) {
        simtower::OriginalTdtElevatorFloorRecord record{};
        record.floor = static_cast<std::int8_t>(floor);
        record.mapped_index = simtower::original_elevator_floor_record_index(
            elevator->type, elevator->bottom_floor, elevator->top_floor,
            floor);
        elevator->floor_records.push_back(record);
      }
    }

    auto& route = tower.post_elevator.routes_bff0[0];
    route.fill(std::byte{0x5a});
    route[0] = std::byte{0};
    route[1] = std::byte{1};
    route[2] = std::byte{24};
    route[3] = std::byte{10};
    tower.elevators[0].block_c2.fill(std::byte{0xa5});
    for (auto& transfer : tower.post_elevator.db9c_records) {
      transfer.fill(std::byte{0x5a});
    }

    // Direct 11b0:0000/006d/00da coverage: clear all Elevator/route graph
    // dwords and each transfer dword/floor byte before repopulating the live
    // routes and transfer table, while preserving transfer byte five.
    simtower::rebuild_original_transport_route_graphs(tower);
    const auto& transfers = tower.post_elevator.db9c_records;
    assert(load_u32(transfers[0], 0U, false) == 0xe0000080U);
    assert(transfers[0][4] == std::byte{10});
    assert(transfers[0][5] == std::byte{0x5a});
    assert(load_u32(transfers[1], 0U, false) == 0U);
    assert(transfers[1][4] == std::byte{0xff});
    assert(transfers[1][5] == std::byte{0x5a});

    // Serviced floors contain the one-based transfer record. Elsewhere only
    // reachable non-type-2 transports survive in the MSB-first mask.
    assert(load_u32(first.block_c2, 10U * 4U, false) == 1U);
    assert(load_u32(first.block_c2, 20U * 4U, false) == 0U);
    assert(load_u32(first.block_c2, 30U * 4U, false) == 0x40000000U);
    assert(load_u32(first.block_c2, 40U * 4U, false) == 0U);
    assert(load_u32(express_class.block_c2, 30U * 4U, false) == 0U);
    assert(load_u32(route, 4U + 10U * 4U, false) == 1U);
    assert(load_u32(route, 4U + 20U * 4U, false) == 0U);
    assert(load_u32(route, 4U + 30U * 4U, false) == 0x40000000U);
    assert(load_u32(route, 4U + 40U * 4U, false) == 0U);

    // 11b0:0b8b reports the exact DIALOG/1005 argument. The shared same-type
    // Lobby transfer wins with code three; an isolated ordinary/type-2 stop
    // reports one/two respectively, while a vertical link suppresses it.
    assert(simtower::original_elevator_service_floor_warning_code(
               tower, 0U, 10) == 3U);
    assert(simtower::original_elevator_service_floor_warning_code(
               tower, 0U, 20) == 1U);
    assert(simtower::original_elevator_service_floor_warning_code(
               tower, 2U, 40) == 2U);
    tower.post_elevator.cf10[20] = std::byte{1};
    assert(simtower::original_elevator_service_floor_warning_code(
               tower, 0U, 20) == 0U);
    tower.post_elevator.cf10[20] = std::byte{0};

    second.serviced_floors[20] = std::byte{1};
    assert(simtower::original_elevator_service_floor_warning_code(
               tower, 0U, 20) == 0U);
    second.type = 0U;
    assert(simtower::original_elevator_service_floor_warning_code(
               tower, 0U, 20) == 1U);
    second.type = 1U;
    second.serviced_floors[20] = std::byte{0};

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(load_u32(reparsed.elevators[0].block_c2, 30U * 4U, false) ==
           0x40000000U);
    assert(load_u32(reparsed.post_elevator.routes_bff0[0],
                    4U + 30U * 4U, false) == 0x40000000U);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // The graph dwords use the TDT revision's opposite byte order while the
    // single-byte floor and route headers remain unchanged.
    auto tower = simtower::make_original_new_tdt();
    tower.header.byte_swapped = true;
    simtower::OriginalTdtTenant lobby{};
    lobby.left = 0U;
    lobby.right = 100U;
    lobby.type = 0x18;
    tower.floors[10].tenants = {lobby};
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.x = 10U;
    elevator.serviced_floors[10] = std::byte{1};
    simtower::rebuild_original_transport_route_graphs(tower);
    assert(load_u32(tower.post_elevator.db9c_records[0], 0U, true) ==
           0x80000000U);
    assert(load_u32(elevator.block_c2, 10U * 4U, true) == 1U);
    assert(simtower::original_elevator_service_floor_warning_code(
               tower, 0U, 10) == 1U);
  }

  {
    // 10c0:0775/087d permit an Escalator only when both landings resolve to
    // one of their literal commercial/public tenant types. Retail type 10
    // is accepted after deferred activation; Office type 7 is not.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_retail_shop(
               tower, 11, 120, costs).succeeded());
    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(tower) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(tower) ==
           simtower::OriginalPendingStepStatus::activated);
    assert(tower.floors[11].tenants[0].type == 10);
    auto result = simtower::build_original_vertical_transport(
        tower, 27, 11, 123, costs);
    assert(result.succeeded());
    assert(result.cost == 200);
    assert(tower.post_elevator.stairs_bd70[0].shape == 0U);
    assert(tower.post_elevator.stairs_bd70[0].floor == 10);
    assert(tower.post_elevator.cf10[10] == std::byte{1});

    auto bare = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               bare, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_office(
               bare, 11, 120, 0, costs).succeeded());
    for (int step = 0; step < 11; ++step) {
      assert(simtower::step_original_pending_construction(bare) ==
             simtower::OriginalPendingStepStatus::advanced);
    }
    assert(simtower::step_original_pending_construction(bare) ==
           simtower::OriginalPendingStepStatus::activated);
    result = simtower::build_original_vertical_transport(
        bare, 27, 11, 120, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::invalid_span);
  }

  {
    // Elevator/stair rectangles and the shared 64-record limit are checked
    // before any state mutation or YEN debit.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               tower, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_floor(
               tower, 11, 120, 160, costs).succeeded());
    assert(simtower::build_original_standard_elevator(
               tower, 11, 124, costs, part).succeeded());
    auto result = simtower::build_original_vertical_transport(
        tower, 22, 11, 124, costs);
    assert(result.status == simtower::OriginalConstructionStatus::occupied);
    assert(result.construction_status_code == 23U);

    auto capped = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               capped, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_floor(
               capped, 11, 120, 160, costs).succeeded());
    for (auto& record : capped.post_elevator.stairs_bd70) {
      record.used = 1U;
    }
    const auto capped_before = simtower::serialize_original_tdt(capped);
    result = simtower::build_original_vertical_transport(
        capped, 22, 11, 124, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::vertical_transport_limit);
    assert(result.construction_status_code == 27U);
    assert(simtower::serialize_original_tdt(capped) == capped_before);

    auto poor = simtower::make_original_new_tdt();
    assert(simtower::build_original_initial_lobby(
               poor, 100, 200, 1, costs).succeeded());
    assert(simtower::build_original_floor(
               poor, 11, 120, 160, costs).succeeded());
    poor.header.balance = 49;
    const auto poor_before = simtower::serialize_original_tdt(poor);
    result = simtower::build_original_vertical_transport(
        poor, 22, 11, 124, costs);
    assert(result.status ==
           simtower::OriginalConstructionStatus::insufficient_funds);
    assert(result.cost == 50);
    assert(result.construction_status_code == 7U);
    assert(simtower::serialize_original_tdt(poor) == poor_before);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    const auto pristine = simtower::serialize_original_tdt(tower);
    auto result = simtower::build_original_office(tower, 10, 120, 0, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_floor);
    result = simtower::build_original_office(tower, 11, 120, 0, costs);
    assert(result.status == simtower::OriginalConstructionStatus::invalid_span);
    assert(simtower::serialize_original_tdt(tower) == pristine);
  }

  {
    // Complete 10b0:0000 New/Open transient reset, including the previously
    // missing 1198:0000, 11a8:14c9, 24x 1090:00d9(-1), and 64x 10c0:0000
    // stages before 10b0:031a rebuilds people state.
    auto tower = simtower::make_original_new_tdt();
    for (auto& series : tower.post_elevator.b846_series) {
      series.fill(0x12345678);
    }
    tower.post_elevator.dynamic_dd5c.fill(std::byte{0x7a});
    tower.post_elevator.dynamic_dd60.fill(std::byte{0x7a});
    tower.post_elevator.dynamic_dd64.fill(std::byte{0x7a});

    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    tower.header.frame_time = 0U;
    tower.header.current_day = 0;
    elevator.schedule.fill(std::byte{0x22});
    elevator.schedule[14] = std::byte{0x4b};
    elevator.schedule[28] = std::byte{0x6c};
    elevator.block_2a2.fill(std::byte{0x6a});
    elevator.block_31a.fill(std::byte{0x6b});
    simtower::OriginalTdtElevatorFloorRecord floor_record{};
    floor_record.exact_bytes.fill(std::byte{0x5a});
    elevator.floor_records.push_back(floor_record);
    for (std::size_t index = 0U;
         index < elevator.car_home_floors.size(); ++index) {
      elevator.car_home_floors[index] =
          static_cast<std::byte>(4U + index);
      elevator.car_records[index].exact_bytes.fill(std::byte{0x55});
    }

    auto& transport = tower.post_elevator.stairs_bd70[0];
    transport.used = 1U;
    transport.word_6 = 0x1234U;
    transport.word_8 = 0x5678U;
    transport.exact_bytes.fill(std::byte{0x33});

    tower.people[0].exact_bytes.fill(std::byte{0x22});
    tower.people[0].exact_bytes[4] = std::byte{7};
    simtower::reset_original_loaded_simulation_state(tower);

    for (const auto& series : tower.post_elevator.b846_series) {
      assert(std::all_of(series.begin(), series.end(),
                         [](std::int32_t value) { return value == 0; }));
    }
    assert(std::all_of(tower.post_elevator.dynamic_dd5c.begin(),
                       tower.post_elevator.dynamic_dd5c.end(),
                       [](std::byte value) { return value == std::byte{0}; }));
    assert(std::all_of(tower.post_elevator.dynamic_dd60.begin(),
                       tower.post_elevator.dynamic_dd60.end(),
                       [](std::byte value) { return value == std::byte{0}; }));
    assert(std::all_of(tower.post_elevator.dynamic_dd64.begin(),
                       tower.post_elevator.dynamic_dd64.end(),
                       [](std::byte value) { return value == std::byte{0}; }));
    assert(std::all_of(elevator.block_2a2.begin(), elevator.block_2a2.end(),
                       [](std::byte value) { return value == std::byte{0}; }));
    assert(std::all_of(elevator.block_31a.begin(), elevator.block_31a.end(),
                       [](std::byte value) { return value == std::byte{0}; }));
    assert(std::all_of(elevator.floor_records[0].exact_bytes.begin(),
                       elevator.floor_records[0].exact_bytes.begin() + 4,
                       [](std::byte value) { return value == std::byte{0}; }));
    assert(elevator.floor_records[0].exact_bytes[4] == std::byte{0x5a});
    for (std::size_t index = 0U; index < elevator.car_records.size(); ++index) {
      const auto& car = elevator.car_records[index].exact_bytes;
      const auto home = static_cast<std::byte>(4U + index);
      assert(car[0] == home && car[4] == std::byte{1} && car[5] == home &&
             car[6] == home && car[13] == home &&
             car[14] == std::byte{0x6c} && car[15] == std::byte{0x55});
      assert(std::all_of(car.begin() + 16, car.begin() + 226,
                         [](std::byte value) { return value == std::byte{0xff}; }));
      assert(std::all_of(car.begin() + 226, car.end(),
                         [](std::byte value) { return value == std::byte{0}; }));
    }
    assert(transport.used == 1U && transport.word_6 == 0U &&
           transport.word_8 == 0U);
    assert(std::all_of(transport.exact_bytes.begin() + 6,
                       transport.exact_bytes.begin() + 10,
                       [](std::byte value) { return value == std::byte{0}; }));
    assert(transport.exact_bytes[5] == std::byte{0x33});
    const auto& person = tower.people[0].exact_bytes;
    assert(person[5] == std::byte{0x27} && person[7] == std::byte{0} &&
           person[8] == std::byte{0} && person[10] == std::byte{0} &&
           person[13] == std::byte{0});
  }

  {
    // Hidden 11f8:0955-098c initial floor-zero/cell-zero transaction. The
    // amount goes through 1178:076f and therefore updates other income too.
    auto tower = simtower::make_original_new_tdt();
    assert(simtower::apply_original_initial_balance_bonus(tower, 0, 0));
    assert(tower.header.balance == 40'000);
    assert(tower.header.other_income == 20'000);
    assert(!simtower::apply_original_initial_balance_bonus(tower, 0, 0));

    auto wrong_floor = simtower::make_original_new_tdt();
    assert(!simtower::apply_original_initial_balance_bonus(
        wrong_floor, 1, 0));
    auto wrong_cell = simtower::make_original_new_tdt();
    assert(!simtower::apply_original_initial_balance_bonus(
        wrong_cell, 0, 1));
    auto has_lobby = simtower::make_original_new_tdt();
    has_lobby.header.lobby_height = 1U;
    assert(!simtower::apply_original_initial_balance_bonus(
        has_lobby, 0, 0));
    auto occupied = simtower::make_original_new_tdt();
    occupied.floors[0].tenants.emplace_back();
    assert(!simtower::apply_original_initial_balance_bonus(
        occupied, 0, 0));
  }

  return 0;
}
