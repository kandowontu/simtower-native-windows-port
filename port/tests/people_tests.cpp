#include "original_construction.hpp"
#include "original_information.hpp"
#include "original_people.hpp"
#include "original_tdt.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace {

constexpr std::uint16_t byte_swap(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8U) | (value >> 8U));
}

void store_u16(std::span<std::byte> bytes,
               std::size_t offset,
               std::uint16_t value,
               bool byte_swapped) {
  if (byte_swapped) value = byte_swap(value);
  bytes[offset] = static_cast<std::byte>(value);
  bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

std::uint16_t load_u16(std::span<const std::byte> bytes,
                       std::size_t offset,
                       bool byte_swapped) {
  std::uint16_t value =
      static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
      static_cast<std::uint16_t>(
          std::to_integer<std::uint8_t>(bytes[offset + 1U]) << 8U);
  return byte_swapped ? byte_swap(value) : value;
}

std::uint32_t load_u32(std::span<const std::byte> bytes,
                       std::size_t offset,
                       bool byte_swapped) {
  std::uint32_t value =
      static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
      (static_cast<std::uint32_t>(
           std::to_integer<std::uint8_t>(bytes[offset + 1U]))
       << 8U) |
      (static_cast<std::uint32_t>(
           std::to_integer<std::uint8_t>(bytes[offset + 2U]))
       << 16U) |
      (static_cast<std::uint32_t>(
           std::to_integer<std::uint8_t>(bytes[offset + 3U]))
       << 24U);
  if (!byte_swapped) return value;
  return ((value & 0x000000ffU) << 24U) |
         ((value & 0x0000ff00U) << 8U) |
         ((value & 0x00ff0000U) >> 8U) |
         ((value & 0xff000000U) >> 24U);
}

void store_u32(std::span<std::byte> bytes,
               std::size_t offset,
               std::uint32_t value,
               bool byte_swapped) {
  if (byte_swapped) {
    value = ((value & 0x000000ffU) << 24U) |
            ((value & 0x0000ff00U) << 8U) |
            ((value & 0x00ff0000U) >> 8U) |
            ((value & 0xff000000U) >> 24U);
  }
  bytes[offset] = static_cast<std::byte>(value);
  bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
  bytes[offset + 2U] = static_cast<std::byte>(value >> 16U);
  bytes[offset + 3U] = static_cast<std::byte>(value >> 24U);
}

simtower::OriginalTdtDocument make_active_office() {
  auto tower = simtower::make_original_new_tdt();
  simtower::OriginalYenTable costs{};
  costs[0x18] = 100;
  costs[7] = 50;
  const auto lobby =
      simtower::build_original_initial_lobby(tower, 100, 200, 1, costs);
  assert(lobby.succeeded());
  const auto office =
      simtower::build_original_office(tower, 11, 120, 0, costs);
  assert(office.succeeded());
  for (int pass = 0; pass < 12; ++pass) {
    const auto status = simtower::step_original_pending_construction(tower);
    assert(status == (pass == 11
                          ? simtower::OriginalPendingStepStatus::activated
                          : simtower::OriginalPendingStepStatus::advanced));
  }
  return tower;
}

struct ElevatorCleanupTrace {
  std::array<std::size_t, 8> people{};
  std::size_t count{};
  bool skip_order_assertions{};
};

void trace_elevator_cleanup_dispatch(
    simtower::OriginalTdtDocument& document,
    std::size_t person_index,
    simtower::OriginalPersonFamilyDispatchSource source,
    void* raw_context) noexcept {
  auto& trace = *static_cast<ElevatorCleanupTrace*>(raw_context);
  trace.people[trace.count++] = person_index;
  if (trace.skip_order_assertions) return;
  assert(source == (person_index < 2U
                        ? simtower::OriginalPersonFamilyDispatchSource::
                              elevator_car_0883
                        : simtower::OriginalPersonFamilyDispatchSource::
                              dispatcher_16ab));
  auto& elevator = document.elevators[0];
  const auto& car = elevator.car_records[0].exact_bytes;
  const auto& queue = elevator.floor_records[0].exact_bytes;
  if (person_index == 0U) {
    assert(car[184] == std::byte{0xff});
    assert(car[3] == std::byte{2});
    assert(car[226U + 15U] == std::byte{2});
    assert(document.people[0].exact_bytes[7] == std::byte{15});
  } else if (person_index == 1U) {
    assert(car[184U + 1U] == std::byte{0xff});
    assert(car[3] == std::byte{1});
    assert(car[226U + 15U] == std::byte{1});
    assert(document.people[1].exact_bytes[7] == std::byte{15});
  } else if (person_index == 2U) {
    assert(queue[0] == std::byte{2} && queue[1] == std::byte{0});
    assert(load_u16(document.people[2].exact_bytes, 10U, false) == 0U);
    assert(load_u16(document.people[2].exact_bytes, 12U, false) ==
           0xc12cU);
  } else if (person_index == 3U) {
    assert(queue[0] == std::byte{1} && queue[1] == std::byte{1});
  } else if (person_index == 4U) {
    assert(queue[2] == std::byte{1} && queue[3] == std::byte{6});
  }
}

void initialize_route_person(simtower::OriginalTdtDocument& tower,
                             std::size_t person_index,
                             std::int8_t owner_floor,
                             std::uint8_t owner_key,
                             std::uint16_t route_x) {
  if (tower.people.size() <= person_index) {
    tower.people.resize(person_index + 1U);
  }
  tower.people_count = static_cast<std::uint32_t>(
      std::max<std::size_t>(tower.people_count, person_index + 1U));
  auto& floor = tower.floors[static_cast<std::size_t>(owner_floor)];
  floor.tenants.clear();
  floor.tenant_index.fill(0U);
  simtower::OriginalTdtTenant owner{};
  owner.exact_bytes[12] = static_cast<std::byte>(owner_key);
  owner.preserved_07_to_0f[5] = static_cast<std::byte>(owner_key);
  store_u16(owner.exact_bytes, 6U, route_x,
            tower.header.byte_swapped);
  owner.variant = std::to_integer<std::uint8_t>(owner.exact_bytes[6]);
  owner.preserved_07_to_0f[0] = owner.exact_bytes[7];
  floor.tenants.push_back(owner);
  floor.tenant_index[owner_key] = 0U;
  auto& person = tower.people[person_index].exact_bytes;
  person.fill(std::byte{0});
  person[0] = static_cast<std::byte>(owner_floor);
  person[1] = static_cast<std::byte>(owner_key);
}

const simtower::OriginalTdtDocument& original_new_tower_template() {
  static const auto tower = simtower::make_original_new_tdt();
  return tower;
}

void configure_cathedral_arrival_tower(
    simtower::OriginalTdtDocument& tower) {
  tower.header.rating = 5U;
  tower.header.frame_time = 700U;
  store_u16(tower.header.exact_bytes, 2U, 5U, false);
  store_u16(tower.header.exact_bytes, 34U, 0U, false);  // DS:b3ec key
  store_u16(tower.header.exact_bytes, 60U, 2U, false);  // DS:b406
  store_u32(tower.header.exact_bytes, 66U, 0x12345678U, false); // b40c
  tower.post_elevator.finance.total_population = 5000;
  for (std::size_t ordinal = 0U; ordinal < 5U; ++ordinal) {
    const auto floor_number = 109U + ordinal;
    auto& floor = tower.floors[floor_number];
    floor.tenants.clear();
    floor.tenant_index.fill(0U);
    simtower::OriginalTdtTenant tenant{};
    tenant.type = static_cast<std::int8_t>(40 - ordinal);
    tenant.exact_bytes[4] = static_cast<std::byte>(tenant.type);
    tenant.exact_bytes[12] = std::byte{0};
    tenant.preserved_07_to_0f[5] = std::byte{0};
    const auto people_start = static_cast<std::uint32_t>(ordinal * 8U);
    store_u32(tenant.exact_bytes, 8U, people_start, false);
    store_u16(tenant.exact_bytes, 6U, ordinal == 0U ? 3U : 1U, false);
    tenant.variant = std::to_integer<std::uint8_t>(tenant.exact_bytes[6]);
    tenant.preserved_07_to_0f[0] = tenant.exact_bytes[7];
    floor.tenants.push_back(tenant);
    floor.tenant_index[0] = 0U;
    for (std::size_t person = 0U; person < 8U; ++person) {
      auto& exact = tower.people[people_start + person].exact_bytes;
      exact[4] = std::byte{36};
      exact[5] = std::byte{3};
    }
  }
}

simtower::OriginalTdtDocument make_cathedral_arrival_tower() {
  auto tower = simtower::make_original_new_tdt();
  configure_cathedral_arrival_tower(tower);
  return tower;
}

std::unique_ptr<simtower::OriginalTdtDocument>
make_cathedral_arrival_tower_on_heap() {
  auto tower = std::make_unique<simtower::OriginalTdtDocument>(
      original_new_tower_template());
  configure_cathedral_arrival_tower(*tower);
  return tower;
}

std::unique_ptr<simtower::OriginalTdtDocument> make_cathedral_wrapper_tower(
    bool byte_swapped = false) {
  auto tower = std::make_unique<simtower::OriginalTdtDocument>(
      original_new_tower_template());
  tower->header.byte_swapped = byte_swapped;
  tower->header.current_day = 2U;  // DS:b3a0 calendar phase one
  tower->header.frame_time = 96U;  // DS:b3a1 day phase zero; scan lane zero
  tower->people_count = 1U;
  tower->people.resize(1U);

  auto& floor = tower->floors[109U];
  floor.tenants.clear();
  floor.tenant_index.fill(0U);
  simtower::OriginalTdtTenant tenant{};
  tenant.type = 40;
  tenant.exact_bytes[4] = std::byte{40};
  store_u16(tenant.exact_bytes, 6U, 100U, byte_swapped);
  store_u32(tenant.exact_bytes, 8U, 0U, byte_swapped);
  tenant.exact_bytes[12] = std::byte{0};
  tenant.preserved_07_to_0f[5] = std::byte{0};
  floor.tenants.push_back(tenant);
  floor.tenant_index[0] = 0U;

  auto& person = tower->people[0].exact_bytes;
  person.fill(std::byte{0});
  person[0] = std::byte{109};
  person[1] = std::byte{0};
  store_u16(person, 2U, 0U, byte_swapped);
  person[4] = std::byte{36};
  person[5] = std::byte{0x20};
  person[7] = std::byte{10};
  person[8] = std::byte{0xff};
  return tower;
}

std::unique_ptr<simtower::OriginalTdtDocument>
make_entertainment_person_tower(bool byte_swapped = false,
                                std::int8_t owner_type = 18,
                                std::int8_t person_type = 18) {
  auto tower = std::make_unique<simtower::OriginalTdtDocument>(
      original_new_tower_template());
  tower->header.byte_swapped = byte_swapped;
  tower->header.frame_time = 256U;
  tower->people_count = 1U;
  tower->people.resize(1U);

  auto& floor = tower->floors[10U];
  floor.tenants.clear();
  floor.tenant_index.fill(0U);
  simtower::OriginalTdtTenant owner{};
  owner.type = owner_type;
  owner.exact_bytes[4] = static_cast<std::byte>(owner_type);
  store_u16(owner.exact_bytes, 6U, 0U, byte_swapped);  // dc24 index
  store_u32(owner.exact_bytes, 8U, 0U, byte_swapped);
  owner.exact_bytes[12] = std::byte{0};
  owner.preserved_07_to_0f[5] = std::byte{0};
  floor.tenants.push_back(owner);
  floor.tenant_index[0] = 0U;

  simtower::OriginalTdtTenant commercial{};
  commercial.type = 12;
  commercial.exact_bytes[4] = std::byte{12};
  commercial.exact_bytes[12] = std::byte{1};
  commercial.preserved_07_to_0f[5] = std::byte{1};
  floor.tenants.push_back(commercial);
  floor.tenant_index[1] = 1U;

  auto& person = tower->people[0].exact_bytes;
  person.fill(std::byte{0});
  person[0] = std::byte{10};
  person[1] = std::byte{0};
  store_u16(person, 2U, 0U, byte_swapped);
  person[4] = static_cast<std::byte>(person_type);
  person[5] = std::byte{0x20};
  person[7] = std::byte{10};
  person[8] = std::byte{0xff};

  auto& entertainment = tower->post_elevator.dc24_records[0];
  entertainment.fill(std::byte{0});
  entertainment[0] = std::byte{10};
  entertainment[1] = std::byte{10};
  entertainment[2] = std::byte{0};
  entertainment[3] = std::byte{0};
  entertainment[4] = std::byte{1};
  entertainment[5] = std::byte{1};
  entertainment[6] = std::byte{1};
  entertainment[7] =
      static_cast<std::byte>(owner_type == 29 || owner_type == 30
                                 ? 0xffU
                                 : 0U);

  auto& service = tower->retail[0].exact_bytes;
  service.fill(std::byte{0});
  service[0] = std::byte{10};
  service[1] = std::byte{1};
  service[2] = std::byte{0};
  tower->post_elevator.dynamic_dd5c.fill(std::byte{0});
  tower->post_elevator.dynamic_dd60.fill(std::byte{0});
  tower->post_elevator.dynamic_dd64.fill(std::byte{0});
  store_u16(tower->post_elevator.dynamic_dd5c, 0U, 1U, byte_swapped);
  store_u16(tower->post_elevator.dynamic_dd5c, 2U, 0U, byte_swapped);
  return tower;
}

simtower::OriginalTdtDocument make_security_person_tower(
    bool byte_swapped = false) {
  auto tower = simtower::make_original_new_tdt();
  tower.header.byte_swapped = byte_swapped;
  tower.people_count = 6U;
  tower.people.resize(6U);

  auto& floor = tower.floors[17];
  floor.left_edge = 100U;
  floor.right_edge = 200U;
  floor.tenants.clear();
  floor.tenant_index.fill(0U);
  simtower::OriginalTdtTenant tenant{};
  tenant.left = 110U;
  tenant.right = 126U;
  tenant.type = 14;
  tenant.status = 16U;
  tenant.variant = 18U;
  store_u16(tenant.exact_bytes, 0U, tenant.left, byte_swapped);
  store_u16(tenant.exact_bytes, 2U, tenant.right, byte_swapped);
  tenant.exact_bytes[4] = std::byte{14};
  tenant.exact_bytes[5] = std::byte{16};
  store_u16(tenant.exact_bytes, 6U, 18U, byte_swapped);
  store_u32(tenant.exact_bytes, 8U, 0U, byte_swapped);
  tenant.exact_bytes[12] = std::byte{0};
  tenant.preserved_07_to_0f[0] = tenant.exact_bytes[7];
  tenant.preserved_07_to_0f[5] = std::byte{0};
  floor.tenants.push_back(tenant);
  floor.tenant_index[0] = 0U;
  tower.post_elevator.cf88_words[0] = 17U;

  for (std::size_t ordinal = 0U; ordinal < 6U; ++ordinal) {
    auto& person = tower.people[ordinal].exact_bytes;
    person.fill(std::byte{0});
    person[0] = std::byte{17};
    person[1] = std::byte{0};
    store_u16(person, 2U, static_cast<std::uint16_t>(ordinal), byte_swapped);
    person[4] = std::byte{14};
    person[5] = std::byte{0};
    person[7] = std::byte{17};
    store_u16(person, 14U, 198U, byte_swapped);
  }
  for (std::size_t floor_index = 0U; floor_index < 120U; ++floor_index) {
    store_u16(tower.header.exact_bytes, 80U + floor_index * 2U,
              0xffffU, byte_swapped);
    store_u16(tower.header.exact_bytes, 320U + floor_index * 2U,
              0xffffU, byte_swapped);
  }
  return tower;
}

void allocate_security_test_floor(simtower::OriginalTdtDocument& tower,
                                  std::size_t floor_number,
                                  std::uint16_t left,
                                  std::uint16_t right) {
  auto& floor = tower.floors[floor_number];
  floor.left_edge = left;
  floor.right_edge = right;
  if (floor.tenants.empty()) {
    simtower::OriginalTdtTenant marker{};
    marker.exact_bytes[12] = std::byte{0};
    floor.tenants.push_back(marker);
    floor.tenant_index[0] = 0U;
  }
}

void append_housekeeping_hotel(simtower::OriginalTdtDocument& tower,
                               std::size_t floor_number,
                               std::uint8_t key,
                               std::uint8_t status,
                               std::uint32_t people_start) {
  auto& floor = tower.floors[floor_number];
  if (floor.tenants.empty()) floor.tenant_index.fill(0U);
  simtower::OriginalTdtTenant room{};
  room.left = 140U;
  room.right = 146U;
  room.type = 3;
  room.status = status;
  store_u16(room.exact_bytes, 0U, room.left, tower.header.byte_swapped);
  store_u16(room.exact_bytes, 2U, room.right, tower.header.byte_swapped);
  room.exact_bytes[4] = std::byte{3};
  room.exact_bytes[5] = static_cast<std::byte>(status);
  store_u32(room.exact_bytes, 8U, people_start,
            tower.header.byte_swapped);
  room.exact_bytes[12] = static_cast<std::byte>(key);
  room.preserved_07_to_0f[5] = static_cast<std::byte>(key);
  floor.tenant_index[key] = static_cast<std::uint16_t>(floor.tenants.size());
  floor.tenants.push_back(room);
}

simtower::OriginalTdtDocument make_housekeeping_person_tower(
    bool byte_swapped = false) {
  auto tower = simtower::make_original_new_tdt();
  tower.header.byte_swapped = byte_swapped;
  tower.header.frame_time = 5U;
  tower.people_count = 8U;
  tower.people.resize(8U);

  auto& floor = tower.floors[17U];
  floor.left_edge = 100U;
  floor.right_edge = 200U;
  floor.tenants.clear();
  floor.tenant_index.fill(0U);
  simtower::OriginalTdtTenant owner{};
  owner.left = 110U;
  owner.right = 125U;
  owner.type = 15;
  store_u16(owner.exact_bytes, 0U, owner.left, byte_swapped);
  store_u16(owner.exact_bytes, 2U, owner.right, byte_swapped);
  owner.exact_bytes[4] = std::byte{15};
  store_u16(owner.exact_bytes, 6U, 120U, byte_swapped);
  store_u32(owner.exact_bytes, 8U, 0U, byte_swapped);
  owner.exact_bytes[12] = std::byte{0};
  owner.preserved_07_to_0f[5] = std::byte{0};
  floor.tenants.push_back(owner);
  floor.tenant_index[0] = 0U;
  append_housekeeping_hotel(tower, 17U, 1U, 0x28U, 6U);

  for (std::size_t ordinal = 0U; ordinal < 6U; ++ordinal) {
    auto& person = tower.people[ordinal].exact_bytes;
    person.fill(std::byte{0});
    person[0] = std::byte{17};
    person[1] = std::byte{0};
    store_u16(person, 2U, static_cast<std::uint16_t>(ordinal), byte_swapped);
    person[4] = std::byte{15};
    person[5] = std::byte{0};
    person[6] = std::byte{0xfe};
    person[7] = std::byte{0xff};
  }
  for (std::size_t index = 6U; index < 8U; ++index) {
    auto& guest = tower.people[index].exact_bytes;
    guest.fill(std::byte{0});
    guest[0] = std::byte{17};
    guest[1] = std::byte{1};
    guest[4] = std::byte{3};
    guest[5] = std::byte{0x24};
  }
  return tower;
}

std::unique_ptr<simtower::OriginalTdtDocument> make_metro_service_tower(
    bool byte_swapped = false) {
  auto tower = std::make_unique<simtower::OriginalTdtDocument>();
  *tower = simtower::make_original_new_tdt();
  tower->header.byte_swapped = byte_swapped;
  tower->header.frame_time = 200U;
  tower->people_count = 1U;
  tower->people.resize(1U);
  initialize_route_person(*tower, 0U, 0, 0U, 100U);
  auto& owner = tower->floors[0].tenants[0];
  owner.type = 33;
  owner.exact_bytes[4] = std::byte{33};
  store_u32(owner.exact_bytes, 8U, 0U, byte_swapped);

  auto& service_floor = tower->floors[2];
  service_floor.tenants.clear();
  service_floor.tenant_index.fill(0U);
  simtower::OriginalTdtTenant service_tenant{};
  service_tenant.left = 120U;
  service_tenant.right = 126U;
  service_tenant.type = 10;
  store_u16(service_tenant.exact_bytes, 0U, 120U, byte_swapped);
  store_u16(service_tenant.exact_bytes, 2U, 126U, byte_swapped);
  service_tenant.exact_bytes[4] = std::byte{10};
  service_tenant.exact_bytes[12] = std::byte{0};
  service_tenant.preserved_07_to_0f[5] = std::byte{0};
  service_floor.tenants.push_back(service_tenant);
  service_floor.tenant_index[0] = 0U;

  auto& service = tower->retail[0].exact_bytes;
  service.fill(std::byte{0});
  service[0] = std::byte{2};
  service[1] = std::byte{0};
  service[2] = std::byte{0};
  store_u16(service, 16U, 0U, byte_swapped);

  const auto add_only_service = [&](std::span<std::byte> block) {
    store_u16(block, 0U, 1U, byte_swapped);
    store_u16(block, 2U, 0U, byte_swapped);
  };
  add_only_service(tower->post_elevator.dynamic_dd5c);
  add_only_service(tower->post_elevator.dynamic_dd60);
  add_only_service(tower->post_elevator.dynamic_dd64);

  auto& person = tower->people[0].exact_bytes;
  person[4] = std::byte{33};
  person[5] = std::byte{1};
  person[6] = std::byte{0xfe};
  person[7] = std::byte{0};
  return tower;
}

std::unique_ptr<simtower::OriginalTdtDocument> make_food_service_tower(
    std::int8_t type = 12,
    std::int16_t owner_floor = 10,
    bool byte_swapped = false) {
  auto tower = std::make_unique<simtower::OriginalTdtDocument>();
  *tower = simtower::make_original_new_tdt();
  tower->header.byte_swapped = byte_swapped;
  tower->header.rating = 1U;
  tower->header.frame_time = 200U;
  tower->people_count = 1U;
  tower->people.resize(1U);

  auto& floor = tower->floors[static_cast<std::size_t>(owner_floor)];
  floor.tenants.clear();
  floor.tenant_index.fill(0U);
  simtower::OriginalTdtTenant owner{};
  owner.left = 100U;
  owner.right = 106U;
  owner.type = type;
  store_u16(owner.exact_bytes, 0U, owner.left, byte_swapped);
  store_u16(owner.exact_bytes, 2U, owner.right, byte_swapped);
  owner.exact_bytes[4] = static_cast<std::byte>(type);
  store_u16(owner.exact_bytes, 6U, 0U, byte_swapped);
  store_u32(owner.exact_bytes, 8U, 0U, byte_swapped);
  owner.exact_bytes[12] = std::byte{0};
  owner.variant = std::to_integer<std::uint8_t>(owner.exact_bytes[6]);
  std::copy(owner.exact_bytes.begin() + 7U,
            owner.exact_bytes.begin() + 16U,
            owner.preserved_07_to_0f.begin());
  floor.tenants.push_back(owner);
  floor.tenant_index[0] = 0U;

  auto& service = tower->retail[0].exact_bytes;
  service.fill(std::byte{0});
  service[0] = static_cast<std::byte>(owner_floor);
  service[1] = std::byte{0};
  service[2] = std::byte{0};
  service[6] = std::byte{10};
  service[7] = std::byte{0};
  store_u16(service, 12U, static_cast<std::uint16_t>(-11), byte_swapped);
  store_u16(service, 16U, 0U, byte_swapped);

  auto& person = tower->people[0].exact_bytes;
  person.fill(std::byte{0});
  person[0] = static_cast<std::byte>(owner_floor);
  person[1] = std::byte{0};
  store_u16(person, 2U, 0U, byte_swapped);
  person[4] = static_cast<std::byte>(type);
  person[5] = std::byte{0x20};
  person[6] = std::byte{0xfe};
  person[7] = std::byte{10};
  person[8] = std::byte{0xfe};
  return tower;
}

std::unique_ptr<simtower::OriginalTdtDocument> make_condo_person_tower(
    bool byte_swapped = false,
    std::uint8_t owner_key = 0U) {
  auto tower = std::make_unique<simtower::OriginalTdtDocument>(
      original_new_tower_template());
  tower->header.byte_swapped = byte_swapped;
  tower->header.current_day = 0U;
  tower->header.frame_time = 0U;
  tower->people_count = 3U;
  tower->people.resize(3U);

  auto& floor = tower->floors[10U];
  floor.tenants.clear();
  floor.tenant_index.fill(0U);
  simtower::OriginalTdtTenant owner{};
  owner.left = 100U;
  owner.right = 108U;
  owner.type = 9;
  owner.status = 3U;
  owner.rent_rate = 2U;
  store_u16(owner.exact_bytes, 0U, owner.left, byte_swapped);
  store_u16(owner.exact_bytes, 2U, owner.right, byte_swapped);
  owner.exact_bytes[4] = std::byte{9};
  owner.exact_bytes[5] = std::byte{3};
  store_u16(owner.exact_bytes, 6U, 100U, byte_swapped);
  store_u32(owner.exact_bytes, 8U, 0U, byte_swapped);
  owner.exact_bytes[12] = static_cast<std::byte>(owner_key);
  owner.exact_bytes[14] = std::byte{1};
  owner.exact_bytes[16] = std::byte{2};
  owner.variant = std::to_integer<std::uint8_t>(owner.exact_bytes[6]);
  std::copy(owner.exact_bytes.begin() + 7U,
            owner.exact_bytes.begin() + 16U,
            owner.preserved_07_to_0f.begin());
  floor.tenants.push_back(owner);
  floor.tenant_index[owner_key] = 0U;

  constexpr std::uint8_t kServiceKey = 5U;
  simtower::OriginalTdtTenant service_tenant{};
  service_tenant.left = 110U;
  service_tenant.right = 116U;
  service_tenant.type = 12;
  store_u16(service_tenant.exact_bytes, 0U, service_tenant.left,
            byte_swapped);
  store_u16(service_tenant.exact_bytes, 2U, service_tenant.right,
            byte_swapped);
  service_tenant.exact_bytes[4] = std::byte{12};
  service_tenant.exact_bytes[12] = std::byte{kServiceKey};
  service_tenant.preserved_07_to_0f[5] = std::byte{kServiceKey};
  floor.tenant_index[kServiceKey] =
      static_cast<std::uint16_t>(floor.tenants.size());
  floor.tenants.push_back(service_tenant);

  auto& service = tower->retail[0].exact_bytes;
  service.fill(std::byte{0});
  service[0] = std::byte{10};
  service[1] = std::byte{kServiceKey};
  service[2] = std::byte{0};
  service[6] = std::byte{10};
  store_u16(service, 12U, static_cast<std::uint16_t>(-4), byte_swapped);
  store_u16(service, 16U, 0U, byte_swapped);

  const auto add_only_service = [&](std::span<std::byte> block) {
    store_u16(block, 0U, 1U, byte_swapped);
    store_u16(block, 2U, 0U, byte_swapped);
  };
  add_only_service(tower->post_elevator.dynamic_dd5c);
  add_only_service(tower->post_elevator.dynamic_dd60);
  add_only_service(tower->post_elevator.dynamic_dd64);

  for (std::size_t ordinal = 0U; ordinal < 3U; ++ordinal) {
    auto& person = tower->people[ordinal].exact_bytes;
    person.fill(std::byte{0});
    person[0] = std::byte{10};
    person[1] = static_cast<std::byte>(owner_key);
    store_u16(person, 2U, static_cast<std::uint16_t>(ordinal), byte_swapped);
    person[4] = std::byte{9};
    person[5] = ordinal == 0U ? std::byte{0x20} : std::byte{0x10};
    person[6] = std::byte{0xfe};
    person[7] = std::byte{10};
    person[8] = std::byte{0xfe};
  }
  return tower;
}

std::unique_ptr<simtower::OriginalTdtDocument> make_hotel_person_tower(
    std::int8_t hotel_type = 4,
    bool byte_swapped = false) {
  auto tower = std::make_unique<simtower::OriginalTdtDocument>(
      original_new_tower_template());
  tower->header.byte_swapped = byte_swapped;
  tower->header.rating = 3U;
  tower->header.current_day = 0;
  tower->header.frame_time = 0U;
  tower->people_count = 3U;
  tower->people.resize(3U);

  auto& floor = tower->floors[10U];
  floor.tenants.clear();
  floor.tenant_index.fill(0U);
  simtower::OriginalTdtTenant owner{};
  owner.left = 100U;
  owner.right = 108U;
  owner.type = hotel_type;
  owner.status = 3U;
  owner.rent_rate = 2U;
  owner.subtype = 7U;
  store_u16(owner.exact_bytes, 0U, owner.left, byte_swapped);
  store_u16(owner.exact_bytes, 2U, owner.right, byte_swapped);
  owner.exact_bytes[4] = static_cast<std::byte>(hotel_type);
  owner.exact_bytes[5] = std::byte{3};
  store_u16(owner.exact_bytes, 6U, 100U, byte_swapped);
  store_u32(owner.exact_bytes, 8U, 0U, byte_swapped);
  owner.exact_bytes[12] = std::byte{0};
  owner.exact_bytes[14] = std::byte{1};
  owner.exact_bytes[16] = std::byte{2};
  owner.exact_bytes[17] = std::byte{7};
  owner.variant = std::to_integer<std::uint8_t>(owner.exact_bytes[6]);
  std::copy(owner.exact_bytes.begin() + 7U,
            owner.exact_bytes.begin() + 16U,
            owner.preserved_07_to_0f.begin());
  floor.tenants.push_back(owner);
  floor.tenant_index[0] = 0U;

  constexpr std::uint8_t kServiceKey = 5U;
  simtower::OriginalTdtTenant service_tenant{};
  service_tenant.left = 110U;
  service_tenant.right = 116U;
  service_tenant.type = 12;
  store_u16(service_tenant.exact_bytes, 0U, service_tenant.left,
            byte_swapped);
  store_u16(service_tenant.exact_bytes, 2U, service_tenant.right,
            byte_swapped);
  service_tenant.exact_bytes[4] = std::byte{12};
  service_tenant.exact_bytes[12] = std::byte{kServiceKey};
  service_tenant.preserved_07_to_0f[5] = std::byte{kServiceKey};
  floor.tenant_index[kServiceKey] =
      static_cast<std::uint16_t>(floor.tenants.size());
  floor.tenants.push_back(service_tenant);

  auto& service = tower->retail[0].exact_bytes;
  service.fill(std::byte{0});
  service[0] = std::byte{10};
  service[1] = std::byte{kServiceKey};
  service[2] = std::byte{0};
  service[6] = std::byte{10};
  store_u16(service, 12U, static_cast<std::uint16_t>(-4), byte_swapped);
  store_u16(service, 16U, 0U, byte_swapped);
  tower->post_elevator.dynamic_dd60.fill(std::byte{0});
  store_u16(tower->post_elevator.dynamic_dd60, 0U, 1U, byte_swapped);
  store_u16(tower->post_elevator.dynamic_dd60, 2U, 0U, byte_swapped);

  for (std::size_t ordinal = 0U; ordinal < 3U; ++ordinal) {
    auto& person = tower->people[ordinal].exact_bytes;
    person.fill(std::byte{0});
    person[0] = std::byte{10};
    person[1] = std::byte{0};
    store_u16(person, 2U, static_cast<std::uint16_t>(ordinal), byte_swapped);
    person[4] = static_cast<std::byte>(hotel_type);
    person[5] = std::byte{0x20};
    person[6] = std::byte{0xfe};
    person[7] = std::byte{10};
    person[8] = std::byte{0xfe};
  }

  auto& parking_floor = tower->floors[9U];
  parking_floor.tenants.clear();
  parking_floor.tenant_index.fill(0U);
  simtower::OriginalTdtTenant parking{};
  parking.left = 100U;
  parking.right = 104U;
  parking.type = 11;
  store_u16(parking.exact_bytes, 0U, parking.left, byte_swapped);
  store_u16(parking.exact_bytes, 2U, parking.right, byte_swapped);
  parking.exact_bytes[4] = std::byte{11};
  parking.exact_bytes[12] = std::byte{0};
  parking_floor.tenants.push_back(parking);
  parking_floor.tenant_index[0] = 0U;

  tower->post_elevator.parking_connected = 1;
  tower->post_elevator.parking_entries[0] = 0U;
  auto& parking_record = tower->post_elevator.cf9c_records[0];
  parking_record.fill(std::byte{0});
  parking_record[0] = std::byte{9};
  parking_record[1] = std::byte{0};
  const auto category = static_cast<std::size_t>(hotel_type - 2);
  tower->post_elevator.b846_series[1][category] = 10;
  return tower;
}

std::unique_ptr<simtower::OriginalTdtDocument> make_office_normal_person_tower(
    bool byte_swapped = false) {
  auto tower = std::make_unique<simtower::OriginalTdtDocument>(
      original_new_tower_template());
  tower->header.byte_swapped = byte_swapped;
  tower->header.rating = 3U;
  tower->header.current_day = 0;
  tower->header.frame_time = 0U;
  tower->people_count = 6U;
  tower->people.resize(6U);

  constexpr std::uint8_t kOwnerKey = 3U;
  constexpr std::uint8_t kServiceKey = 5U;
  constexpr std::uint8_t kMedicalKey = 6U;
  auto& floor = tower->floors[10U];
  floor.tenants.clear();
  floor.tenant_index.fill(0U);

  simtower::OriginalTdtTenant owner{};
  owner.left = 100U;
  owner.right = 108U;
  owner.type = 7;
  owner.status = 6U;
  owner.rent_rate = 2U;
  store_u16(owner.exact_bytes, 0U, owner.left, byte_swapped);
  store_u16(owner.exact_bytes, 2U, owner.right, byte_swapped);
  owner.exact_bytes[4] = std::byte{7};
  owner.exact_bytes[5] = std::byte{6};
  store_u16(owner.exact_bytes, 6U, 100U, byte_swapped);
  store_u32(owner.exact_bytes, 8U, 0U, byte_swapped);
  owner.exact_bytes[12] = std::byte{kOwnerKey};
  owner.exact_bytes[14] = std::byte{1};
  owner.exact_bytes[16] = std::byte{2};
  owner.variant = std::to_integer<std::uint8_t>(owner.exact_bytes[6]);
  std::copy(owner.exact_bytes.begin() + 7U,
            owner.exact_bytes.begin() + 16U,
            owner.preserved_07_to_0f.begin());
  floor.tenant_index[kOwnerKey] =
      static_cast<std::uint16_t>(floor.tenants.size());
  floor.tenants.push_back(owner);

  simtower::OriginalTdtTenant service_tenant{};
  service_tenant.left = 110U;
  service_tenant.right = 116U;
  service_tenant.type = 12;
  store_u16(service_tenant.exact_bytes, 0U, service_tenant.left,
            byte_swapped);
  store_u16(service_tenant.exact_bytes, 2U, service_tenant.right,
            byte_swapped);
  service_tenant.exact_bytes[4] = std::byte{12};
  service_tenant.exact_bytes[12] = std::byte{kServiceKey};
  service_tenant.preserved_07_to_0f[5] = std::byte{kServiceKey};
  floor.tenant_index[kServiceKey] =
      static_cast<std::uint16_t>(floor.tenants.size());
  floor.tenants.push_back(service_tenant);

  simtower::OriginalTdtTenant medical_tenant{};
  medical_tenant.left = 118U;
  medical_tenant.right = 124U;
  medical_tenant.type = 13;
  store_u16(medical_tenant.exact_bytes, 0U, medical_tenant.left,
            byte_swapped);
  store_u16(medical_tenant.exact_bytes, 2U, medical_tenant.right,
            byte_swapped);
  medical_tenant.exact_bytes[4] = std::byte{13};
  medical_tenant.exact_bytes[12] = std::byte{kMedicalKey};
  medical_tenant.preserved_07_to_0f[5] = std::byte{kMedicalKey};
  floor.tenant_index[kMedicalKey] =
      static_cast<std::uint16_t>(floor.tenants.size());
  floor.tenants.push_back(medical_tenant);

  auto& service = tower->retail[0].exact_bytes;
  service.fill(std::byte{0});
  service[0] = std::byte{10};
  service[1] = std::byte{kServiceKey};
  service[2] = std::byte{0};
  service[6] = std::byte{10};
  store_u16(service, 12U, static_cast<std::uint16_t>(-4), byte_swapped);
  store_u16(service, 16U, 0U, byte_swapped);
  tower->post_elevator.dynamic_dd64.fill(std::byte{0});
  store_u16(tower->post_elevator.dynamic_dd64, 0U, 1U, byte_swapped);
  store_u16(tower->post_elevator.dynamic_dd64, 2U, 0U, byte_swapped);

  tower->post_elevator.dbfc_dwords[0] =
      10U | (static_cast<std::uint32_t>(kMedicalKey) << 8U);
  tower->medical_route_index.fill(std::byte{0});
  store_u16(tower->medical_route_index, 0U, 1U, byte_swapped);
  store_u16(tower->medical_route_index, 2U, 0U, byte_swapped);
  tower->post_elevator.b92d = 1U;

  for (std::size_t ordinal = 0U; ordinal < 6U; ++ordinal) {
    auto& person = tower->people[ordinal].exact_bytes;
    person.fill(std::byte{0});
    person[0] = std::byte{10};
    person[1] = std::byte{kOwnerKey};
    store_u16(person, 2U, static_cast<std::uint16_t>(ordinal), byte_swapped);
    person[4] = std::byte{7};
    person[5] = std::byte{0x20};
    person[6] = std::byte{0xfe};
    person[7] = std::byte{10};
    person[8] = std::byte{0xfe};
  }

  auto& parking_floor = tower->floors[9U];
  parking_floor.tenants.clear();
  parking_floor.tenant_index.fill(0U);
  simtower::OriginalTdtTenant parking{};
  parking.left = 100U;
  parking.right = 104U;
  parking.type = 11;
  store_u16(parking.exact_bytes, 0U, parking.left, byte_swapped);
  store_u16(parking.exact_bytes, 2U, parking.right, byte_swapped);
  parking.exact_bytes[4] = std::byte{11};
  parking.exact_bytes[12] = std::byte{0};
  parking_floor.tenants.push_back(parking);
  parking_floor.tenant_index[0] = 0U;
  tower->post_elevator.parking_connected = 1;
  tower->post_elevator.parking_entries[0] = 0U;
  auto& parking_record = tower->post_elevator.cf9c_records[0];
  parking_record.fill(std::byte{0});
  parking_record[0] = std::byte{9};
  parking_record[1] = std::byte{0};
  tower->post_elevator.b846_series[1][0] = 10;
  return tower;
}

simtower::OriginalTdtDocument make_elevator_passenger_tower(
    bool byte_swapped = false) {
  auto tower = simtower::make_original_new_tdt();
  tower.header.byte_swapped = byte_swapped;
  tower.people_count = 4U;
  tower.people.resize(4U);
  for (auto& person : tower.people) person.exact_bytes.fill(std::byte{0});

  auto& elevator = tower.elevators[0];
  elevator.used = 1U;
  elevator.type = 1U;
  elevator.capacity = 4U;
  elevator.bottom_floor = 0;
  elevator.top_floor = 30;
  elevator.serviced_floors.fill(std::byte{0});
  elevator.block_c2.fill(std::byte{0});
  elevator.floor_records.clear();

  auto& car = elevator.car_records[0].exact_bytes;
  car.fill(std::byte{0});
  car[0] = std::byte{10};
  car[2] = std::byte{1};
  car[4] = std::byte{1};
  car[15] = std::byte{1};
  for (std::size_t slot = 0U; slot < 42U; ++slot) {
    store_u32(car, 16U + slot * 4U, 0xffffffffU, byte_swapped);
    car[184U + slot] = std::byte{0xff};
  }

  simtower::OriginalTdtElevatorFloorRecord floor_record{};
  floor_record.floor = 10;
  floor_record.mapped_index = simtower::original_elevator_floor_record_index(
      elevator.type, elevator.bottom_floor, elevator.top_floor, 10);
  elevator.floor_records.push_back(floor_record);
  return tower;
}

}  // namespace

int main() {
  {
    // Direct 1198:06a6/0650 coverage: only byte 13's upper six bits denote an
    // assignment, and the signed word-12 floor field uses arithmetic SAR 10.
    auto tower = simtower::make_original_new_tdt();
    simtower::OriginalTdtPersonRecord person{};
    person.exact_bytes[12] = std::byte{3};
    person.exact_bytes[13] = std::byte{3};
    assert(!simtower::original_person_has_parking(person));
    assert(simtower::original_person_parking_floor(tower, person) == 10);

    store_u16(person.exact_bytes, 12U, 0x0c00U, false);
    assert(simtower::original_person_has_parking(person));
    assert(simtower::original_person_parking_floor(tower, person) == 7);
    store_u16(person.exact_bytes, 12U, 0xfc00U, false);
    assert(simtower::original_person_has_parking(person));
    assert(simtower::original_person_parking_floor(tower, person) == 11);
  }

  {
    // Direct 1170:056f coverage: signed floor bands, the empty-band fallback,
    // a single rand() modulo live count, signed entries, bounds rejection,
    // malformed counts, and opposite-endian persisted words.
    constexpr std::size_t kBankSize = 0x16U;
    auto tower = simtower::make_original_new_tdt();
    tower.medical_route_index.fill(std::byte{0});
    store_u16(tower.medical_route_index, 0U, 1U, false);
    store_u16(tower.medical_route_index, 2U, 3U, false);
    tower.random_state = 1U;
    assert(simtower::select_original_medical_service(tower, -20) == 3);
    assert(tower.random_state == 0x015a4e36U);

    // Floor 24 selects bank one. rand(1) is 346, so modulo three chooses its
    // second live word; no leading-bank data can affect this selection.
    tower = simtower::make_original_new_tdt();
    tower.medical_route_index.fill(std::byte{0});
    store_u16(tower.medical_route_index, kBankSize, 3U, false);
    store_u16(tower.medical_route_index, kBankSize + 2U, 1U, false);
    store_u16(tower.medical_route_index, kBankSize + 4U, 4U, false);
    store_u16(tower.medical_route_index, kBankSize + 6U, 8U, false);
    tower.random_state = 1U;
    assert(simtower::select_original_medical_service(tower, 24) == 4);
    assert(tower.random_state == 0x015a4e36U);

    // An empty selected bank falls back to bank zero and still consumes one
    // random result. Bank indices seven and above reject before consuming it.
    tower = simtower::make_original_new_tdt();
    tower.medical_route_index.fill(std::byte{0});
    store_u16(tower.medical_route_index, 0U, 1U, false);
    store_u16(tower.medical_route_index, 2U, 5U, false);
    tower.random_state = 1U;
    assert(simtower::select_original_medical_service(tower, 24) == 5);
    assert(tower.random_state == 0x015a4e36U);
    tower.random_state = 123U;
    assert(simtower::select_original_medical_service(tower, 114) == -1);
    assert(tower.random_state == 123U);

    // Zero and over-capacity counts reject without rand(); an invalid signed
    // or out-of-range selected service rejects after the single draw.
    tower = simtower::make_original_new_tdt();
    tower.random_state = 123U;
    assert(simtower::select_original_medical_service(tower, 10) == -1);
    assert(tower.random_state == 123U);
    store_u16(tower.medical_route_index, 0U, 11U, false);
    assert(simtower::select_original_medical_service(tower, 10) == -1);
    assert(tower.random_state == 123U);
    store_u16(tower.medical_route_index, 0U, 1U, false);
    store_u16(tower.medical_route_index, 2U, 0xffffU, false);
    assert(simtower::select_original_medical_service(tower, 10) == -1);
    assert(tower.random_state != 123U);
    store_u16(tower.medical_route_index, 2U,
              static_cast<std::uint16_t>(
                  tower.post_elevator.dbfc_dwords.size()),
              false);
    assert(simtower::select_original_medical_service(tower, 10) == -1);

    tower = simtower::make_original_new_tdt();
    tower.header.byte_swapped = true;
    tower.medical_route_index.fill(std::byte{0});
    store_u16(tower.medical_route_index, kBankSize, 1U, true);
    store_u16(tower.medical_route_index, kBankSize + 2U, 4U, true);
    assert(simtower::select_original_medical_service(tower, 24) == 4);
  }

  {
    // Direct 1198:06e7 eligibility coverage: rating three admits only the
    // type-5 ordinal-zero guest or the type-7 ordinal-two floor/key cadence.
    auto tower = simtower::make_original_new_tdt();
    simtower::OriginalTdtPersonRecord person{};
    simtower::OriginalTdtTenant owner{};
    tower.header.rating = 3U;
    owner.exact_bytes[4] = std::byte{5};
    store_u16(person.exact_bytes, 2U, 0U, false);
    assert(simtower::original_person_parking_eligible(
        tower, person, owner, 10, 0));
    store_u16(person.exact_bytes, 2U, 1U, false);
    assert(!simtower::original_person_parking_eligible(
        tower, person, owner, 10, 0));
    owner.exact_bytes[4] = std::byte{7};
    store_u16(person.exact_bytes, 2U, 2U, false);
    assert(simtower::original_person_parking_eligible(
        tower, person, owner, 10, 3));
    assert(!simtower::original_person_parking_eligible(
        tower, person, owner, 10, 2));
    tower.header.rating = 2U;
    assert(!simtower::original_person_parking_eligible(
        tower, person, owner, 10, 3));
    tower.header.rating = 3U;
    owner.exact_bytes[4] = std::byte{6};
    assert(!simtower::original_person_parking_eligible(
        tower, person, owner, 10, 3));
  }

  {
    // Exact 10b0:0072 first post-load pass, including its signed status
    // comparisons and referenced Retail/DC24 record resets.
    auto tower = simtower::make_original_new_tdt();
    auto& floor = tower.floors[50];
    floor.tenants.clear();
    const auto add = [&](std::int8_t type, std::uint8_t status,
                         std::uint16_t link) {
      simtower::OriginalTdtTenant tenant{};
      tenant.type = type;
      tenant.status = status;
      tenant.variant = static_cast<std::uint8_t>(link);
      tenant.exact_bytes[4] = static_cast<std::byte>(type);
      tenant.exact_bytes[5] = static_cast<std::byte>(status);
      store_u16(tenant.exact_bytes, 6U, link, false);
      tenant.preserved_07_to_0f[0] = tenant.exact_bytes[7];
      tenant.exact_bytes[13] = std::byte{0};
      floor.tenants.push_back(tenant);
    };
    add(3, 0x17U, 0U);
    add(4, 0x18U, 0U);
    add(5, 0x80U, 0U);  // signed comparison treats this as below 0x18
    add(7, 0x0fU, 0U);
    add(7, 0x10U, 0U);
    add(9, 0x17U, 0U);
    add(9, 0x18U, 0U);
    add(6, 0x44U, 0U);
    add(12, 0x44U, 1U);
    add(10, 0x44U, 2U);
    add(10, 0x44U, 3U);
    add(11, 0x44U, 0U);
    for (std::int8_t type : {18, 19, 29, 30, 34, 35}) {
      add(type, 0x44U, static_cast<std::uint16_t>(type == 18 ? 0 :
          type == 19 ? 1 : type == 29 ? 2 : type == 30 ? 3 :
          type == 34 ? 4 : 5));
    }
    for (std::int8_t type : {31, 32, 33, 36, 37, 38, 39, 40}) {
      add(type, 0x44U, 0x0101U);
    }
    add(20, 0x44U, 7U);  // not present in the original jump-table branches

    for (std::size_t index = 0U; index < 4U; ++index) {
      tower.retail[index].exact_bytes.fill(std::byte{0x55});
    }
    tower.retail[2].exact_bytes[2] = std::byte{0xff};
    for (std::size_t index = 0U; index < 6U; ++index) {
      tower.post_elevator.dc24_records[index].fill(std::byte{0x55});
    }

    assert(simtower::initialize_original_tenant_runtime_state(tower) == 22U);
    assert(floor.tenants[0].status == 0x28U);
    assert(floor.tenants[1].status == 0x18U);
    assert(floor.tenants[2].status == 0x28U);
    assert(floor.tenants[3].status == 0U);
    assert(floor.tenants[4].status == 0x10U);
    assert(floor.tenants[5].status == 0U);
    assert(floor.tenants[6].status == 0x18U);
    assert(floor.tenants[11].status == 0U);
    for (std::size_t index : {0U, 2U, 3U, 5U, 7U, 8U, 10U, 11U}) {
      assert(floor.tenants[index].exact_bytes[13] == std::byte{1});
    }
    for (std::size_t index : {1U, 4U, 6U, 9U}) {
      assert(floor.tenants[index].exact_bytes[13] == std::byte{0});
    }
    for (std::size_t index : {0U, 1U, 3U}) {
      const auto& record = tower.retail[index].exact_bytes;
      assert(record[2] == std::byte{3} && record[9] == std::byte{0});
    }
    assert(tower.retail[2].exact_bytes[2] == std::byte{0xff});
    assert(tower.retail[2].exact_bytes[9] == std::byte{0x55});
    for (std::size_t index = 0U; index < 6U; ++index) {
      const auto& record = tower.post_elevator.dc24_records[index];
      assert(record[4] == std::byte{0} && record[5] == std::byte{0});
      assert(record[6] == std::byte{0} && record[10] == std::byte{0});
      assert(record[3] == std::byte{0x55} && record[11] == std::byte{0x55});
    }
    for (std::size_t index = 18U; index < 26U; ++index) {
      assert(load_u16(floor.tenants[index].exact_bytes, 6U, false) == 0U);
      assert(floor.tenants[index].variant == 0U);
      assert(floor.tenants[index].exact_bytes[13] == std::byte{1});
    }
    assert(floor.tenants.back().type == 20);
    assert(floor.tenants.back().status == 0x44U);
    assert(load_u16(floor.tenants.back().exact_bytes, 6U, false) == 7U);
    assert(floor.tenants.back().exact_bytes[13] == std::byte{0});
  }

  {
    // Exact 10b0:031a post-load type table. Bytes 6 and (for residential,
    // Office, and Hotel people) 9/14/15 are deliberately retained.
    auto tower = simtower::make_original_new_tdt();
    constexpr std::array<std::int8_t, 13> types{
        3, 4, 5, 6, 7, 9, 10, 12, 15, 18, 29, 33, 36};
    tower.people_count = static_cast<std::uint32_t>(types.size() + 1U);
    tower.people.resize(tower.people_count);
    for (std::size_t index = 0U; index < tower.people.size(); ++index) {
      tower.people[index].exact_bytes.fill(std::byte{0x55});
      tower.people[index].exact_bytes[4] = static_cast<std::byte>(
          index < types.size() ? types[index] : 14);
      tower.people[index].exact_bytes[6] = std::byte{0x66};
      tower.people[index].exact_bytes[9] = std::byte{0x99};
      tower.people[index].exact_bytes[14] = std::byte{0xaa};
      tower.people[index].exact_bytes[15] = std::byte{0xbb};
    }
    assert(simtower::initialize_original_people_runtime_state(tower) ==
           types.size());
    for (std::size_t index = 0U; index < types.size(); ++index) {
      const auto& person = tower.people[index].exact_bytes;
      assert(person[6] == std::byte{0x66});
      const auto type = types[index];
      if (type == 3 || type == 4 || type == 5) {
        assert(person[5] == std::byte{0x26});
      } else if (type == 9) {
        assert(person[5] == std::byte{0x21});
      } else if (type == 15) {
        assert(person[5] == std::byte{0});
      } else {
        assert(person[5] == std::byte{0x27});
      }
      assert(person[7] == (type == 15 ? std::byte{0xff} : std::byte{0}));
      assert(person[8] == std::byte{0});
      assert(person[10] == std::byte{0} && person[11] == std::byte{0});
      assert(person[12] == std::byte{0} && person[13] == std::byte{0});
      const bool clears_tail = type == 6 || type == 10 || type == 12 ||
                               type == 15 || type == 18 || type == 29 ||
                               type == 33 || type == 36;
      assert(person[9] == (clears_tail ? std::byte{0} : std::byte{0x99}));
      assert(person[14] ==
             (clears_tail ? std::byte{0} : std::byte{0xaa}));
      assert(person[15] ==
             (clears_tail ? std::byte{0} : std::byte{0xbb}));
    }
    assert(tower.people.back().exact_bytes[5] == std::byte{0x55});
  }

  {
    // Direct 11b0:0805/08f2/0fa5 selection-chain coverage begins here and continues
    // through the Stair, bff0 transfer, and Elevator cases below: tracked and
    // untracked span gates, the 0x280 early cutoff, adjacent-leg fallback,
    // strict score replacement, and last-writer direction are all distinct.
    // 1210:0000 obtains route x through Direct 11b0:0f10 person floor/key ->
    // complete tenant word +6 before clamping negative route floors to ten.
    // Same-floor completion
    // performs only 11d8:0000's metric finalizer.
    auto tower = simtower::make_original_new_tdt();
    initialize_route_person(tower, 0U, 5, 0U, 100U);
    auto& person = tower.people[0].exact_bytes;
    person[9] = std::byte{7};
    store_u16(person, 10U, 99U, false);
    store_u16(person, 12U, 0xc064U, false);
    store_u16(person, 14U, 5U, false);
    simtower::OriginalPersonRouteRequest request{};
    request.source_floor = -1;
    request.destination_floor = -2;
    request.tracked_route = true;
    const auto result = simtower::route_original_person(tower, 0U, request);
    assert(result.status ==
           simtower::OriginalPersonRouteStatus::already_on_floor);
    assert(result.transport_index == -1 && !result.direction_up);
    assert(person[9] == std::byte{8});
    assert(load_u16(person, 10U, false) == 0U);
    assert(load_u16(person, 12U, false) == 0xc000U);
    assert(load_u16(person, 14U, false) == 105U);
  }

  {
    // No-route failure applies dd80, finalizes the metrics, and reports the
    // otherwise-GUI-only 10a8:1b58 request without opening a window.
    auto tower = simtower::make_original_new_tdt();
    initialize_route_person(tower, 0U, 5, 0U, 100U);
    auto& person = tower.people[0].exact_bytes;
    person[9] = std::byte{3};
    store_u16(person, 10U, 77U, false);
    store_u16(person, 12U, 0x8064U, false);
    store_u16(person, 14U, 10U, false);
    simtower::OriginalPersonRouteRequest request{};
    request.source_floor = 5;
    request.destination_floor = 10;
    request.no_route_delay = 50U;
    request.visualize_failure = true;
    const auto result = simtower::route_original_person(tower, 0U, request);
    assert(result.status == simtower::OriginalPersonRouteStatus::no_route);
    assert(result.failure_visualization_requested);
    assert(person[9] == std::byte{4});
    assert(load_u16(person, 10U, false) == 0U);
    assert(load_u16(person, 12U, false) == 0x8000U);
    assert(load_u16(person, 14U, false) == 160U);
  }

  {
    // Direct 11b0:0dc0/0e80 coverage: tracked spans allow six floors while
    // all-one; encountering the other parity before ordinal three blocks a
    // later floor. Odd-only spans require bit one and stop at three floors.
    auto tower = simtower::make_original_new_tdt();
    tower.post_elevator.cf10.fill(std::byte{1});
    assert(simtower::original_full_stair_span_available(tower, 5, 11));
    assert(!simtower::original_full_stair_span_available(tower, 5, 12));
    assert(!simtower::original_full_stair_span_available(tower, -1, 2));
    assert(!simtower::original_full_stair_span_available(tower, 118, 120));
    tower.post_elevator.cf10[6] = std::byte{0};
    assert(!simtower::original_full_stair_span_available(tower, 5, 8));
    tower.post_elevator.cf10.fill(std::byte{1});
    tower.post_elevator.cf10[7] = std::byte{2};
    assert(simtower::original_full_stair_span_available(tower, 5, 8));
    assert(!simtower::original_full_stair_span_available(tower, 5, 9));

    tower.post_elevator.cf10.fill(std::byte{2});
    assert(simtower::original_odd_stair_span_available(tower, 5, 8));
    assert(!simtower::original_odd_stair_span_available(tower, 5, 9));
    tower.post_elevator.cf10[6] = std::byte{1};
    assert(!simtower::original_odd_stair_span_available(tower, 5, 8));
  }

  {
    // Direct 11b0:141c/14c9 coverage: the scorer accepts the exact up/down
    // anchor floor, rejects even shapes in odd-only mode, multiplies wrapped
    // horizontal distance by eight, and adds 0x280 for odd shapes.
    simtower::OriginalTdtStairRecord stair{};
    stair.used = 1U;
    stair.shape = 2U;
    stair.x = 200U;
    stair.floor = 5;
    bool direction_up = false;
    assert(simtower::score_original_stair(
               stair, 5, 7, 100U, false, direction_up) == 800);
    assert(direction_up);
    assert(simtower::score_original_stair(
               stair, 7, 5, 100U, false, direction_up) == 800);
    assert(!direction_up);
    assert(!simtower::score_original_stair(
        stair, 5, 7, 100U, true, direction_up));
    stair.shape = 3U;
    assert(simtower::score_original_stair(
               stair, 5, 7, 100U, true, direction_up) == 1440);
    stair.floor = 6;
    assert(!simtower::score_original_stair(
        stair, 5, 7, 100U, false, direction_up));
    stair.used = 0U;
    assert(!simtower::score_original_stair(
        stair, 5, 7, 100U, false, direction_up));
  }

  {
    // Direct 11b0:0a21/0ad4 coverage: scan all sixteen db9c masks in order,
    // skip the source floor and nonintersecting graphs, then preserve the
    // first accepted transfer's signed direction. Invalid route bits fail.
    auto tower = simtower::make_original_new_tdt();
    bool direction_up = true;
    assert(!simtower::find_original_transfer_direction(
        tower, 32U, 5, 0x40000000U, direction_up));
    assert(direction_up);

    store_u32(tower.post_elevator.db9c_records[0], 0U, 0x80000000U,
              false);
    tower.post_elevator.db9c_records[0][4] = std::byte{9};
    store_u32(tower.post_elevator.db9c_records[1], 0U, 0xc0000000U,
              false);
    tower.post_elevator.db9c_records[1][4] = std::byte{5};
    store_u32(tower.post_elevator.db9c_records[2], 0U, 0xc0000000U,
              false);
    tower.post_elevator.db9c_records[2][4] = std::byte{3};
    store_u32(tower.post_elevator.db9c_records[3], 0U, 0xc0000000U,
              false);
    tower.post_elevator.db9c_records[3][4] = std::byte{8};
    assert(simtower::find_original_transfer_direction(
        tower, 0U, 5, 0x40000000U, direction_up));
    assert(!direction_up);

    tower.header.byte_swapped = true;
    for (auto& transfer : tower.post_elevator.db9c_records) {
      transfer.fill(std::byte{0});
    }
    store_u32(tower.post_elevator.db9c_records[0], 0U, 0xc0000000U, true);
    tower.post_elevator.db9c_records[0][4] = std::byte{7};
    assert(simtower::find_original_transfer_direction(
        tower, 0U, 5, 0x40000000U, direction_up));
    assert(direction_up);
  }

  {
    // Direct 1210:114f full Stair branch: shape 2 spans two floors, increments
    // the up counter, charges ddb8 * span, and adds the 30-unit horizontal-
    // distance band.
    auto tower = simtower::make_original_new_tdt();
    initialize_route_person(tower, 0U, 5, 0U, 100U);
    auto& stair = tower.post_elevator.stairs_bd70[2];
    stair.used = 1U;
    stair.shape = 2U;
    stair.x = 200U;
    stair.floor = 5;
    stair.word_6 = 4U;
    stair.word_8 = 6U;
    stair.exact_bytes[0] = std::byte{1};
    stair.exact_bytes[1] = std::byte{2};
    store_u16(stair.exact_bytes, 2U, 200U, false);
    stair.exact_bytes[4] = std::byte{5};
    store_u16(stair.exact_bytes, 6U, 4U, false);
    store_u16(stair.exact_bytes, 8U, 6U, false);
    tower.post_elevator.cf10[5] = std::byte{1};
    tower.post_elevator.cf10[6] = std::byte{1};
    store_u16(tower.people[0].exact_bytes, 12U, 0x800aU, false);
    simtower::OriginalPersonRouteRequest request{};
    request.source_floor = 5;
    request.destination_floor = 7;
    request.stair_even_delay = 7U;
    const auto result = simtower::route_original_person(tower, 0U, request);
    assert(result.status == simtower::OriginalPersonRouteStatus::stair);
    assert(result.transport_index == 2 && result.direction_up);
    assert(stair.word_6 == 5U && stair.word_8 == 6U);
    assert(load_u16(stair.exact_bytes, 6U, false) == 5U);
    assert(tower.people[0].exact_bytes[7] == std::byte{7});
    assert(tower.people[0].exact_bytes[8] == std::byte{2});
    assert(load_u16(tower.people[0].exact_bytes, 12U, false) ==
           0x8036U);
  }

  {
    // The zero bp+6 branch accepts only odd shapes and does not add its
    // ddb8/ddba travel metric, while bp+8's distance penalty remains active.
    auto tower = simtower::make_original_new_tdt();
    initialize_route_person(tower, 0U, 7, 0U, 100U);
    auto& stair = tower.post_elevator.stairs_bd70[3];
    stair.used = 1U;
    stair.shape = 3U;
    stair.x = 230U;
    stair.floor = 5;
    stair.word_8 = 9U;
    stair.exact_bytes[0] = std::byte{1};
    stair.exact_bytes[1] = std::byte{3};
    store_u16(stair.exact_bytes, 2U, 230U, false);
    stair.exact_bytes[4] = std::byte{5};
    store_u16(stair.exact_bytes, 8U, 9U, false);
    tower.post_elevator.cf10[5] = std::byte{2};
    tower.post_elevator.cf10[6] = std::byte{2};
    store_u16(tower.people[0].exact_bytes, 12U, 5U, false);
    simtower::OriginalPersonRouteRequest request{};
    request.source_floor = 7;
    request.destination_floor = 5;
    request.tracked_route = false;
    request.stair_odd_delay = 99U;
    const auto result = simtower::route_original_person(tower, 0U, request);
    assert(result.status == simtower::OriginalPersonRouteStatus::stair);
    assert(result.transport_index == 3 && !result.direction_up);
    assert(stair.word_8 == 10U);
    assert(tower.people[0].exact_bytes[7] == std::byte{5});
    assert(tower.people[0].exact_bytes[8] == std::byte{3});
    assert(load_u16(tower.people[0].exact_bytes, 12U, false) == 65U);
  }

  {
    // An active bff0 route is an intermediate selector only: it resolves the
    // first adjacent Stair leg instead of becoming a person transport index.
    auto tower = simtower::make_original_new_tdt();
    initialize_route_person(tower, 0U, 5, 0U, 100U);
    auto& route = tower.post_elevator.routes_bff0[0];
    route[1] = std::byte{1};
    route[2] = std::byte{10};
    route[3] = std::byte{5};
    auto& stair = tower.post_elevator.stairs_bd70[4];
    stair.used = 1U;
    stair.shape = 0U;
    stair.x = 100U;
    stair.floor = 5;
    stair.exact_bytes[0] = std::byte{1};
    stair.exact_bytes[4] = std::byte{5};
    simtower::OriginalPersonRouteRequest request{};
    request.source_floor = 5;
    request.destination_floor = 10;
    request.add_distance_penalty = false;
    const auto result = simtower::route_original_person(tower, 0U, request);
    assert(result.status == simtower::OriginalPersonRouteStatus::stair);
    assert(result.transport_index == 4 && result.direction_up);
    assert(tower.people[0].exact_bytes[7] == std::byte{6});
  }

  {
    // Direct 11b0:11af and 1210:11c2 standard-Elevator scorer/queue coverage:
    // apply the service/type gates and direct 0x280 penalty, enqueue at the
    // circular cursor, assign the empty lane, encode up as 0x40+shaft, then
    // stamp b3de.
    auto tower = simtower::make_original_new_tdt();
    initialize_route_person(tower, 0U, 5, 0U, 100U);
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.capacity = 21U;
    elevator.cars = 1U;
    elevator.x = 210U;
    elevator.bottom_floor = 0;
    elevator.top_floor = 20;
    elevator.serviced_floors[5] = std::byte{1};
    elevator.serviced_floors[10] = std::byte{1};
    elevator.car_home_floors.fill(std::byte{5});
    simtower::OriginalTdtElevatorFloorRecord floor_record{};
    floor_record.mapped_index = 5;
    floor_record.floor = 5;
    floor_record.exact_bytes[1] = std::byte{39};
    elevator.floor_records.push_back(floor_record);
    store_u16(tower.people[0].exact_bytes, 12U, 10U, false);
    simtower::OriginalPersonRouteRequest request{};
    request.source_floor = 5;
    request.destination_floor = 10;
    request.frame_time = 77U;
    const auto result = simtower::route_original_person(tower, 0U, request);
    assert(result.status == simtower::OriginalPersonRouteStatus::elevator);
    assert(result.transport_index == 0x40 && result.direction_up);
    assert(result.queue_assignment_created);
    const auto& queue = elevator.floor_records[0].exact_bytes;
    assert(queue[0] == std::byte{1} && queue[1] == std::byte{39});
    assert(load_u32(queue, 4U + 39U * 4U, false) == 0U);
    assert(elevator.block_2a2[5] == std::byte{1});
    assert(tower.people[0].exact_bytes[7] == std::byte{5});
    assert(tower.people[0].exact_bytes[8] == std::byte{0x40});
    assert(load_u16(tower.people[0].exact_bytes, 10U, false) == 77U);
    assert(load_u16(tower.people[0].exact_bytes, 12U, false) == 40U);
  }

  {
    // A full down ring mutates person floor/transport exactly but neither the
    // queue nor its owner, and charges dd7c only for the tracked branch.
    auto tower = simtower::make_original_new_tdt();
    initialize_route_person(tower, 0U, 10, 0U, 100U);
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.x = 100U;
    elevator.bottom_floor = 0;
    elevator.top_floor = 20;
    elevator.serviced_floors[5] = std::byte{1};
    elevator.serviced_floors[10] = std::byte{1};
    simtower::OriginalTdtElevatorFloorRecord floor_record{};
    floor_record.mapped_index = 10;
    floor_record.floor = 10;
    floor_record.exact_bytes[2] = std::byte{40};
    floor_record.exact_bytes[3] = std::byte{7};
    elevator.floor_records.push_back(floor_record);
    store_u16(tower.people[0].exact_bytes, 10U, 88U, false);
    store_u16(tower.people[0].exact_bytes, 12U, 0xc005U, false);
    simtower::OriginalPersonRouteRequest request{};
    request.source_floor = 10;
    request.destination_floor = 5;
    request.queue_full_delay = 25U;
    const auto result = simtower::route_original_person(tower, 0U, request);
    assert(result.status ==
           simtower::OriginalPersonRouteStatus::elevator_queue_full);
    assert(!result.direction_up && !result.queue_assignment_created);
    assert(elevator.floor_records[0].exact_bytes[2] == std::byte{40});
    assert(elevator.floor_records[0].exact_bytes[3] == std::byte{7});
    assert(tower.people[0].exact_bytes[7] == std::byte{10});
    assert(tower.people[0].exact_bytes[8] == std::byte{0xff});
    assert(load_u16(tower.people[0].exact_bytes, 10U, false) == 0U);
    assert(load_u16(tower.people[0].exact_bytes, 12U, false) == 0xc01eU);
  }

  {
    // Direct 1208:03e1/040f/0434 MSB-first mask coverage: db9c/block_c2 transfer
    // selection tests this Elevator's high bit, rejects the opposite lane,
    // and reaches a destination not directly served by the chosen shaft.
    auto tower = simtower::make_original_new_tdt();
    initialize_route_person(tower, 0U, 5, 0U, 100U);
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.x = 100U;
    elevator.bottom_floor = 0;
    elevator.top_floor = 20;
    elevator.serviced_floors[5] = std::byte{1};
    simtower::OriginalTdtElevatorFloorRecord floor_record{};
    floor_record.mapped_index = 5;
    floor_record.floor = 5;
    elevator.floor_records.push_back(floor_record);
    store_u32(elevator.block_c2, 10U * 4U, 0x40000000U, false);
    store_u32(tower.post_elevator.db9c_records[0], 0U, 0xc0000000U,
              false);
    tower.post_elevator.db9c_records[0][4] = std::byte{7};
    simtower::OriginalPersonRouteRequest request{};
    request.source_floor = 5;
    request.destination_floor = 10;
    request.add_distance_penalty = false;
    const auto result = simtower::route_original_person(tower, 0U, request);
    assert(result.status == simtower::OriginalPersonRouteStatus::elevator);
    assert(result.transport_index == 0x40 && result.direction_up);
    assert(elevator.floor_records[0].exact_bytes[0] == std::byte{1});
  }

  {
    // Byte-swapped express queues retain MSB-first dword persistence, use the
    // sparse floor-10 record, and skip standard/service walking penalties.
    auto tower = simtower::make_original_new_tdt();
    tower.header.byte_swapped = true;
    initialize_route_person(tower, 1U, 10, 0U, 400U);
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 0U;
    elevator.x = 10U;
    elevator.bottom_floor = 1;
    elevator.top_floor = 24;
    elevator.serviced_floors[10] = std::byte{1};
    elevator.serviced_floors[24] = std::byte{1};
    simtower::OriginalTdtElevatorFloorRecord floor_record{};
    floor_record.mapped_index = 9;
    floor_record.floor = 10;
    floor_record.exact_bytes[1] = std::byte{2};
    elevator.floor_records.push_back(floor_record);
    store_u16(tower.people[1].exact_bytes, 12U, 3U, true);
    simtower::OriginalPersonRouteRequest request{};
    request.source_floor = 10;
    request.destination_floor = 24;
    request.frame_time = 0x1234U;
    const auto result = simtower::route_original_person(tower, 1U, request);
    assert(result.status == simtower::OriginalPersonRouteStatus::elevator);
    const auto& queue = elevator.floor_records[0].exact_bytes;
    assert(queue[0] == std::byte{1} && queue[1] == std::byte{2});
    assert(load_u32(queue, 4U + 2U * 4U, true) == 1U);
    assert(load_u16(tower.people[1].exact_bytes, 10U, true) == 0x1234U);
    assert(load_u16(tower.people[1].exact_bytes, 12U, true) == 3U);
  }

  {
    // Literal 1220:16ab family jump table for signed person types 3..36.
    // Type 14 is deliberately absent here; only 1210:0883's separate source
    // may invoke the Security callback for an Elevator-car passenger.
    using DispatchStatus = simtower::OriginalPersonFamilyDispatchStatus;
    const auto expected_status = [](std::int16_t type) {
      switch (type) {
        case 3:
        case 4:
        case 5:
          return DispatchStatus::hotel;
        case 6:
        case 12:
          return DispatchStatus::food_service;
        case 7:
          return DispatchStatus::office;
        case 9:
          return DispatchStatus::condo;
        case 10:
          return DispatchStatus::retail;
        case 15:
          return DispatchStatus::housekeeping;
        case 18:
        case 29:
          return DispatchStatus::entertainment;
        case 33:
          return DispatchStatus::metro;
        case 36:
          return DispatchStatus::cathedral;
        default:
          return DispatchStatus::no_handler;
      }
    };
    simtower::OriginalPartTable part{};
    simtower::OriginalYenTable rent{};
    for (std::int16_t type = -1; type <= 40; ++type) {
      auto tower = simtower::make_original_new_tdt();
      tower.people_count = 1U;
      tower.people.resize(1U);
      auto& person = tower.people[0].exact_bytes;
      person[4] = static_cast<std::byte>(
          static_cast<std::uint8_t>(type));
      const auto dispatch = simtower::dispatch_original_person_family(
          tower, 0U, part, rent,
          simtower::OriginalPersonFamilyDispatchSource::dispatcher_16ab);
      assert(dispatch.person_type == type);
      assert(dispatch.status == expected_status(type));
    }
    auto tower = simtower::make_original_new_tdt();
    tower.people_count = 1U;
    tower.people.resize(1U);
    tower.people[0].exact_bytes[4] = std::byte{14};
    const auto security = simtower::dispatch_original_person_family(
        tower, 0U, part, rent,
        simtower::OriginalPersonFamilyDispatchSource::elevator_car_0883);
    assert(security.status == DispatchStatus::security);
  }

  {
    // 1220:67cf observes state zero and prioritizes b406's bomb bit over its
    // fire bit. Both 10f8 helpers first consume signed word-10/word-12 waits.
    simtower::OriginalPartTable part{};
    auto tower = make_security_person_tower();
    auto& person = tower.people[0].exact_bytes;
    person[5] = std::byte{1};
    auto step = simtower::step_original_security_person(tower, 0U, part);
    assert(step.status ==
           simtower::OriginalSecurityPersonStepStatus::inactive);
    person[5] = std::byte{0};
    step = simtower::step_original_security_person(tower, 0U, part);
    assert(step.status ==
           simtower::OriginalSecurityPersonStepStatus::no_event);
    store_u16(tower.header.exact_bytes, 60U, 9U, false);
    store_u16(person, 10U, 2U, false);
    step = simtower::step_original_security_person(tower, 0U, part);
    assert(step.status ==
           simtower::OriginalSecurityPersonStepStatus::countdown);
    assert(load_u16(person, 10U, false) == 1U);
    assert(!step.bomb_found && !step.fire_extinguished);
  }

  {
    // Bomb patrol at the left edge selects tenant status+1 first while at or
    // above the owning Security floor, skips extra Lobby stories, applies
    // 10f8:104a's exact travel wait, and decrements tenant word +6.
    simtower::OriginalPartTable part{};
    part.words_52_to_ac[1] = 3U;
    auto tower = make_security_person_tower();
    store_u16(tower.header.exact_bytes, 60U, 1U, false);
    auto& owner = tower.floors[17].tenants[0];
    owner.exact_bytes[5] = std::byte{21};
    owner.status = 21U;
    store_u16(owner.exact_bytes, 6U, 24U, false);
    allocate_security_test_floor(tower, 22U, 80U, 180U);
    auto& person = tower.people[0].exact_bytes;
    store_u16(person, 14U, 100U, false);
    const auto step =
        simtower::step_original_security_person(tower, 0U, part);
    assert(step.status ==
           simtower::OriginalSecurityPersonStepStatus::moved_floor);
    assert(person[7] == std::byte{22} && person[8] == std::byte{0});
    assert(load_u16(person, 10U, false) == 15U);
    assert(load_u16(person, 14U, false) == 178U);
    assert(load_u16(owner.exact_bytes, 6U, false) == 23U);
    assert(owner.exact_bytes[13] == std::byte{0});
  }

  {
    // Direct 10f8:0656 coverage: a bomb sweep decrements x before the
    // coordinate test. The exact found
    // path raises b406 bit 0x20, installs b40c's PART deadline, waits 100,
    // freezes every other cf88 responder, and emits only the 1080 effect.
    simtower::OriginalPartTable part{};
    part.words_52_to_ac[0] = 9U;
    part.words_52_to_ac[5] = 2U;
    auto tower = make_security_person_tower();
    tower.header.frame_time = 100U;
    store_u16(tower.header.exact_bytes, 60U, 1U, false);
    store_u16(tower.header.exact_bytes, 62U, 153U, false);
    store_u16(tower.header.exact_bytes, 64U, 17U, false);
    auto& person = tower.people[0].exact_bytes;
    store_u16(person, 14U, 154U, false);
    const auto step =
        simtower::step_original_security_person(tower, 0U, part);
    assert(step.status ==
           simtower::OriginalSecurityPersonStepStatus::bomb_found);
    assert(step.changed && step.bomb_found &&
           step.disabled_other_responders && step.effect.valid());
    assert(step.effect.floor == 17 && step.effect.x == 153);
    assert(load_u16(tower.header.exact_bytes, 60U, false) == 0x21U);
    assert(load_u32(tower.header.exact_bytes, 66U, false) == 102U);
    assert(load_u16(person, 12U, false) == 100U &&
           person[8] == std::byte{4});
    for (std::size_t index = 1U; index < 6U; ++index) {
      assert(tower.people[index].exact_bytes[5] == std::byte{1});
    }

    // A miss instead consumes PART +52 and toggles the zero/two walk frame.
    tower = make_security_person_tower();
    store_u16(tower.header.exact_bytes, 60U, 1U, false);
    store_u16(tower.header.exact_bytes, 62U, 153U, false);
    store_u16(tower.header.exact_bytes, 64U, 17U, false);
    auto& miss = tower.people[0].exact_bytes;
    store_u16(miss, 14U, 160U, false);
    const auto missed =
        simtower::step_original_security_person(tower, 0U, part);
    assert(missed.status ==
           simtower::OriginalSecurityPersonStepStatus::searching);
    assert(load_u16(miss, 14U, false) == 159U);
    assert(load_u16(miss, 12U, false) == 9U);
    assert(miss[8] == std::byte{2});
  }

  {
    // Exhausting both bomb patrol bounds is the sole zero return from
    // 10f8:0701; 67cf then changes the responder state to one.
    simtower::OriginalPartTable part{};
    auto tower = make_security_person_tower();
    store_u16(tower.header.exact_bytes, 60U, 1U, false);
    auto& owner = tower.floors[17].tenants[0];
    owner.exact_bytes[5] = std::byte{119};
    owner.status = 119U;
    store_u16(owner.exact_bytes, 6U, 1U, false);
    store_u16(tower.people[0].exact_bytes, 14U, 100U, false);
    const auto step =
        simtower::step_original_security_person(tower, 0U, part);
    assert(step.status ==
           simtower::OriginalSecurityPersonStepStatus::search_exhausted);
    assert(step.changed && tower.people[0].exact_bytes[5] == std::byte{1});
  }

  {
    // Fire responders sweep right-to-left. Entering either six-cell half
    // selects PART +60 and frame four; expiry invokes 10e8:07d6, clears both
    // coincident twelve-cell bands, and raises the process-only 77aa flag.
    simtower::OriginalPartTable part{};
    part.words_52_to_ac[0] = 9U;
    part.words_52_to_ac[7] = 3U;
    auto tower = make_security_person_tower();
    store_u16(tower.header.exact_bytes, 60U, 8U, false);
    store_u16(tower.header.exact_bytes, 80U + 17U * 2U, 148U, false);
    store_u16(tower.header.exact_bytes, 320U + 17U * 2U, 148U, false);
    auto& person = tower.people[0].exact_bytes;
    store_u16(person, 14U, 160U, false);
    auto step = simtower::step_original_security_person(tower, 0U, part);
    assert(step.status ==
           simtower::OriginalSecurityPersonStepStatus::searching);
    assert(load_u16(person, 14U, false) == 159U);
    assert(load_u16(person, 12U, false) == 3U &&
           person[8] == std::byte{4});
    store_u16(person, 12U, 1U, false);
    step = simtower::step_original_security_person(tower, 0U, part);
    assert(step.status ==
           simtower::OriginalSecurityPersonStepStatus::fire_extinguished);
    assert(step.fire_extinguished && tower.security_event_accelerated);
    assert(load_u16(tower.header.exact_bytes, 80U + 17U * 2U, false) ==
           0xffffU);
    assert(load_u16(tower.header.exact_bytes, 320U + 17U * 2U, false) ==
           0xffffU);
  }

  {
    // At a floor's left edge, person ordinal two scans floors 2,8,... in
    // six-floor strides. 10f8:0c06 intentionally calls 104a for every active
    // match, so the highest matching floor and its final-leg wait survive.
    simtower::OriginalPartTable part{};
    part.words_52_to_ac[1] = 2U;
    auto tower = make_security_person_tower();
    store_u16(tower.header.exact_bytes, 60U, 8U, false);
    allocate_security_test_floor(tower, 14U, 50U, 150U);
    allocate_security_test_floor(tower, 26U, 60U, 160U);
    store_u16(tower.header.exact_bytes, 320U + 14U * 2U, 90U, false);
    store_u16(tower.header.exact_bytes, 320U + 26U * 2U, 100U, false);
    auto& person = tower.people[2].exact_bytes;
    person[7] = std::byte{17};
    store_u16(person, 14U, 100U, false);
    const auto step =
        simtower::step_original_security_person(tower, 2U, part);
    assert(step.status ==
           simtower::OriginalSecurityPersonStepStatus::moved_floor);
    assert(person[7] == std::byte{26});
    assert(load_u16(person, 10U, false) == 26U);
    assert(load_u16(person, 14U, false) == 158U);
  }

  {
    // Away from the left edge, 10f8:0c06 treats two -1 fire words on the
    // current floor differently: it moves to only the first active floor in
    // the responder's six-floor partition and returns immediately.
    simtower::OriginalPartTable part{};
    part.words_52_to_ac[1] = 3U;
    auto tower = make_security_person_tower();
    store_u16(tower.header.exact_bytes, 60U, 8U, false);
    allocate_security_test_floor(tower, 7U, 40U, 140U);
    allocate_security_test_floor(tower, 13U, 60U, 160U);
    store_u16(tower.header.exact_bytes, 80U + 7U * 2U, 70U, false);
    store_u16(tower.header.exact_bytes, 80U + 13U * 2U, 90U, false);
    auto& person = tower.people[1].exact_bytes;
    person[7] = std::byte{17};
    store_u16(person, 14U, 150U, false);
    const auto step =
        simtower::step_original_security_person(tower, 1U, part);
    assert(step.status ==
           simtower::OriginalSecurityPersonStepStatus::moved_floor);
    assert(person[7] == std::byte{7});
    assert(load_u16(person, 10U, false) == 30U);
    assert(load_u16(person, 14U, false) == 138U);
  }

  {
    // Direct 1220:0f85 -> 1220:6764 coverage: walk every type-14 tenant's
    // six-record span.
    // Opposite-endian
    // person words and fire arrays preserve their logical values; once 77aa
    // is raised, a later 104a movement writes a zero travel delay.
    simtower::OriginalPartTable part{};
    part.words_52_to_ac[0] = 4U;
    part.words_52_to_ac[1] = 7U;
    part.words_52_to_ac[7] = 4U;
    auto tower = make_security_person_tower(true);
    store_u16(tower.header.exact_bytes, 60U, 8U, true);
    store_u16(tower.header.exact_bytes, 80U + 17U * 2U, 148U, true);
    store_u16(tower.header.exact_bytes, 320U + 17U * 2U, 148U, true);
    for (auto& person : tower.people) {
      store_u16(person.exact_bytes, 14U, 160U, true);
    }
    const auto pass = simtower::step_original_security_people(tower, part);
    assert(pass.responders == 6U && pass.changed == 6U);
    assert(pass.bombs_found == 0U && pass.effects.empty());
    assert(load_u16(tower.people[0].exact_bytes, 14U, true) == 159U);
    assert(load_u16(tower.people[0].exact_bytes, 12U, true) == 4U);

    tower.security_event_accelerated = true;
    allocate_security_test_floor(tower, 22U, 80U, 180U);
    auto& owner = tower.floors[17].tenants[0];
    owner.exact_bytes[5] = std::byte{21};
    owner.status = 21U;
    store_u16(tower.header.exact_bytes, 60U, 1U, true);
    auto& person = tower.people[0].exact_bytes;
    store_u16(person, 10U, 0U, true);
    store_u16(person, 12U, 0U, true);
    store_u16(person, 14U, 100U, true);
    const auto moved =
        simtower::step_original_security_person(tower, 0U, part);
    assert(moved.status ==
           simtower::OriginalSecurityPersonStepStatus::moved_floor);
    assert(load_u16(person, 10U, true) == 0U &&
           load_u16(person, 14U, true) == 178U);
  }

  {
    // 1220:6383 with 1150:0000/1150:01f5/1150:03f3: type-15 ordinal five
    // owns floors congruent to five modulo six. State zero restores the -1
    // construction floor to its owner, selects the dirty room on floor 17,
    // completes the same-floor service route, changes 0x28 to the
    // pre-phase-four 0x18 state, and starts the exact three ticks.
    simtower::OriginalPartTable part{};
    auto tower = make_housekeeping_person_tower();
    auto& person = tower.people[5].exact_bytes;
    const auto route = simtower::original_person_route_context(tower, part);
    auto step = simtower::step_original_housekeeping_person(
        tower, 5U, 17, route);
    assert(step.status ==
           simtower::OriginalHousekeepingPersonStepStatus::arrived_room);
    assert(step.changed && step.room_status_changed &&
           step.hotel_guest_state_changed);
    assert(step.selected_room_floor == 17 && step.selected_room_key == 1);
    assert(step.route.status ==
           simtower::OriginalPersonRouteStatus::already_on_floor);
    assert(person[5] == std::byte{2} && person[6] == std::byte{17} &&
           person[7] == std::byte{17});
    assert(load_u16(person, 10U, false) == 3U);
    assert(load_u16(person, 12U, false) == 1U);
    auto& room = tower.floors[17].tenants[1];
    assert(room.status == 0x18U && room.exact_bytes[5] == std::byte{0x18});
    assert(room.exact_bytes[13] == std::byte{1});
    assert(tower.people[6].exact_bytes[5] == std::byte{3});

    for (const auto expected : {2U, 1U, 0U}) {
      step = simtower::step_original_housekeeping_person(
          tower, 5U, 17, route);
      assert(step.status ==
             simtower::OriginalHousekeepingPersonStepStatus::cleaning_countdown);
      assert(load_u16(person, 10U, false) == expected);
    }
    step = simtower::step_original_housekeeping_person(
        tower, 5U, 17, route);
    assert(step.status ==
           simtower::OriginalHousekeepingPersonStepStatus::room_reopened);
    assert(step.hotel_guest_state_changed && person[5] == std::byte{0});
    assert(tower.people[6].exact_bytes[5] == std::byte{0x24});

    // With no dirty room left, the next callback selects -1 and state one;
    // the following same-floor return route resets the housekeeper to zero.
    step = simtower::step_original_housekeeping_person(
        tower, 5U, 17, route);
    assert(step.status ==
           simtower::OriginalHousekeepingPersonStepStatus::no_dirty_room);
    assert(person[5] == std::byte{1} && person[6] == std::byte{0xff});
    step = simtower::step_original_housekeeping_person(
        tower, 5U, 17, route);
    assert(step.status ==
           simtower::OriginalHousekeepingPersonStepStatus::returned_home);
    assert(person[5] == std::byte{0});
  }

  {
    // 1220:65a9-65bc rejects the same-floor cleaning transition once b3de
    // reaches 1500. Selection and routing have already happened, but the
    // state returns to zero and 1150:01f5 must not mutate the room or guest.
    simtower::OriginalPartTable part{};
    auto tower = make_housekeeping_person_tower();
    tower.header.frame_time = 1500U;
    auto& person = tower.people[5].exact_bytes;
    person[7] = std::byte{17};
    const auto step = simtower::step_original_housekeeping_person(
        tower, 5U, 17,
        simtower::original_person_route_context(tower, part));
    assert(step.status ==
           simtower::OriginalHousekeepingPersonStepStatus::route_failed);
    assert(step.selected_room_floor == 17 && step.selected_room_key == 1);
    assert(step.route.status ==
           simtower::OriginalPersonRouteStatus::already_on_floor);
    assert(step.changed && !step.room_status_changed &&
           !step.hotel_guest_state_changed);
    assert(person[5] == std::byte{0});
    assert(tower.floors[17].tenants[1].status == 0x28U);
    assert(tower.floors[17].tenants[1].exact_bytes[13] == std::byte{0});
    assert(tower.people[6].exact_bytes[5] == std::byte{0x24});
  }

  {
    // 1150:0000 searches from the current floor upward before restarting one
    // floor below and searching downward, but only within the ordinal's
    // modulo-six partition. A type-2 shaft is the exact common-resolver
    // transport admitted by the zero tracked-route argument.
    simtower::OriginalPartTable part{};
    auto tower = make_housekeeping_person_tower();
    tower.floors[17].tenants[1].status = 0x18U;
    tower.floors[17].tenants[1].exact_bytes[5] = std::byte{0x18};
    append_housekeeping_hotel(tower, 23U, 0U, 0x30U, 6U);
    append_housekeeping_hotel(tower, 11U, 0U, 0x28U, 6U);
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 2U;
    elevator.capacity = 21U;
    elevator.cars = 1U;
    elevator.x = 120U;
    elevator.bottom_floor = 0;
    elevator.top_floor = 30;
    elevator.serviced_floors[17] = std::byte{1};
    elevator.serviced_floors[23] = std::byte{1};
    elevator.car_home_floors[0] = std::byte{17};
    elevator.car_records[0].exact_bytes[0] = std::byte{17};
    elevator.car_records[0].exact_bytes[5] = std::byte{17};
    elevator.car_records[0].exact_bytes[6] = std::byte{17};
    elevator.car_records[0].exact_bytes[15] = std::byte{1};
    simtower::OriginalTdtElevatorFloorRecord floor_record{};
    floor_record.mapped_index = 17;
    floor_record.floor = 17;
    elevator.floor_records.push_back(floor_record);
    auto& person = tower.people[5].exact_bytes;
    person[7] = std::byte{17};
    const auto route = simtower::original_person_route_context(tower, part);
    auto step = simtower::step_original_housekeeping_person(
        tower, 5U, 17, route);
    assert(step.status ==
           simtower::OriginalHousekeepingPersonStepStatus::routed_to_room);
    assert(step.selected_room_floor == 23 && step.selected_room_key == 0);
    assert(step.route.status == simtower::OriginalPersonRouteStatus::elevator);
    assert(step.route.transport_index == 0x40 && step.route.direction_up);
    assert(person[5] == std::byte{3} && person[6] == std::byte{23} &&
           person[8] == std::byte{0x40});
    assert(load_u16(person, 10U, false) == 5U);

    // Car arrival dispatches 6383 directly even while byte eight retains its
    // Elevator encoding. Re-running on floor 23 finds the same room and
    // performs the same-floor completion/cleaning transition.
    person[7] = std::byte{23};
    step = simtower::step_original_housekeeping_person(
        tower, 5U, 17, route);
    assert(step.status ==
           simtower::OriginalHousekeepingPersonStepStatus::arrived_room);
    assert(tower.floors[23].tenants[0].status == 0x18U);

    // Once the upward room is no longer dirty, a fresh state-zero search
    // wraps downward and selects floor 11. No service path exists there, so
    // the original route-failure branch restores state zero.
    person[5] = std::byte{0};
    person[7] = std::byte{17};
    step = simtower::step_original_housekeeping_person(
        tower, 5U, 17, route);
    assert(step.status ==
           simtower::OriginalHousekeepingPersonStepStatus::route_failed);
    assert(step.selected_room_floor == 11 && person[5] == std::byte{0});
  }

  {
    // Completed odd-Stair/service legs release the original directional
    // counter before state four resolves the return route.
    simtower::OriginalPartTable part{};
    auto tower = make_housekeeping_person_tower();
    auto& stair = tower.post_elevator.stairs_bd70[2];
    stair.used = 1U;
    stair.floor = 17;
    stair.word_8 = 4U;
    stair.exact_bytes[0] = std::byte{1};
    stair.exact_bytes[4] = std::byte{17};
    store_u16(stair.exact_bytes, 8U, 4U, false);
    auto& person = tower.people[5].exact_bytes;
    person[5] = std::byte{4};
    person[7] = std::byte{17};
    person[8] = std::byte{2};
    const auto step = simtower::step_original_housekeeping_person(
        tower, 5U, 17,
        simtower::original_person_route_context(tower, part));
    assert(step.status ==
           simtower::OriginalHousekeepingPersonStepStatus::returned_home);
    assert(step.released_stair_counter && stair.word_8 == 3U);
    assert(load_u16(stair.exact_bytes, 8U, false) == 3U);
  }

  {
    // 1220:0daf begins at b3de modulo sixteen. At frame five only person five
    // in this eight-record fixture is visited. 1220:6297 suppresses
    // Elevator-held state three, late state zero, and the complete normal
    // pass during a bomb/fire event.
    simtower::OriginalPartTable part{};
    auto tower = make_housekeeping_person_tower();
    auto pass = simtower::step_original_housekeeping_people(tower, part);
    assert(pass.scanned == 1U && pass.dispatched == 1U &&
           pass.changed == 1U && pass.rooms_cleaned == 1U);

    tower = make_housekeeping_person_tower();
    tower.people[5].exact_bytes[5] = std::byte{3};
    tower.people[5].exact_bytes[7] = std::byte{17};
    tower.people[5].exact_bytes[8] = std::byte{0x40};
    pass = simtower::step_original_housekeeping_people(tower, part);
    assert(pass.scanned == 1U && pass.dispatched == 0U &&
           tower.people[5].exact_bytes[5] == std::byte{3});

    tower = make_housekeeping_person_tower();
    tower.header.frame_time = 1509U;  // still index five modulo sixteen
    pass = simtower::step_original_housekeeping_people(tower, part);
    assert(pass.scanned == 1U && pass.dispatched == 0U);

    tower = make_housekeeping_person_tower();
    store_u16(tower.header.exact_bytes, 60U, 8U, false);
    pass = simtower::step_original_housekeeping_people(tower, part);
    assert(pass.scanned == 0U && pass.dispatched == 0U);
    assert(tower.people[5].exact_bytes[5] == std::byte{0});
  }

  {
    // Opposite-endian ordinal, selected-room key, route words, and cleaning
    // countdown retain the same logical values.
    simtower::OriginalPartTable part{};
    auto tower = make_housekeeping_person_tower(true);
    auto& person = tower.people[5].exact_bytes;
    const auto step = simtower::step_original_housekeeping_person(
        tower, 5U, 17,
        simtower::original_person_route_context(tower, part));
    assert(step.status ==
           simtower::OriginalHousekeepingPersonStepStatus::arrived_room);
    assert(load_u16(person, 2U, true) == 5U);
    assert(load_u16(person, 10U, true) == 3U);
    assert(load_u16(person, 12U, true) == 1U);
    assert(tower.floors[17].tenants[1].status == 0x18U);
  }

  {
    // 1220:5227 with 11a8:1472/1498/12dc/1061/0cc2/0bd5/0f11/12a4: Metro state
    // one consumes exactly two Microsoft-runtime random values, one for the
    // commercial family and one for its group-zero entry. A same-floor
    // destination immediately enters the selected service; the paired exit
    // below exercises the same table's state-0x22 dwell/return branch.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[27U] = 50U;  // DS:ddb0 Retail dwell
    auto tower = make_metro_service_tower();
    auto& person = tower->people[0].exact_bytes;
    const auto step = simtower::step_original_metro_person(
        *tower, 0U, 0, part);
    assert(step.status ==
           simtower::OriginalMetroPersonStepStatus::arrived_service);
    assert(step.route.status ==
           simtower::OriginalPersonRouteStatus::already_on_floor);
    assert(step.service_index == 0 && step.service_population_changed &&
           step.service_tenant_marked_dirty);
    assert(person[5] == std::byte{0x22} && person[6] == std::byte{0});
    assert(load_u16(person, 10U, false) == 200U);
    const auto& service = tower->retail[0].exact_bytes;
    assert(service[2] == std::byte{1} && service[9] == std::byte{1});
    assert(load_u16(service, 16U, false) == 1U);
    assert(tower->floors[2].tenants[0].exact_bytes[13] == std::byte{1});
    std::uint32_t expected_random = 1U;
    expected_random = expected_random * 0x015a4e35U + 1U;
    expected_random = expected_random * 0x015a4e35U + 1U;
    assert(tower->random_state == expected_random);

    // 0f11 waits until the signed 16-bit dwell threshold, then drops the
    // 1->0 service edge and completes the already-on-Metro return leg.
    tower->header.frame_time = 249U;
    auto wait = simtower::step_original_metro_person(*tower, 0U, 0, part);
    assert(wait.status ==
           simtower::OriginalMetroPersonStepStatus::waiting_at_service);
    assert(!wait.released_stair_counter);
    assert(person[5] == std::byte{0x22} && service[9] == std::byte{1});
    tower->header.frame_time = 250U;
    const auto leave = simtower::step_original_metro_person(
        *tower, 0U, 0, part);
    assert(leave.status ==
           simtower::OriginalMetroPersonStepStatus::returned_home);
    assert(leave.service_population_changed &&
           leave.service_tenant_marked_dirty);
    assert(person[5] == std::byte{1} && person[7] == std::byte{2} &&
           person[8] == std::byte{0xfe});
    assert(load_u16(person, 10U, false) == 0U);
    assert(service[2] == std::byte{0} && service[9] == std::byte{0});
  }

  {
    // A service that becomes unavailable after selection takes 0cc2's exact
    // dd82 delay/finalizer path, including the second metric finalization
    // after the route resolver's same-floor completion.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[4U] = 17U;
    auto tower = make_metro_service_tower();
    auto& person = tower->people[0].exact_bytes;
    person[5] = std::byte{0x41};
    person[6] = std::byte{0};
    person[7] = std::byte{2};
    store_u16(person, 12U, 0xc005U, false);
    tower->retail[0].exact_bytes[2] = std::byte{3};
    const auto step = simtower::step_original_metro_person(
        *tower, 0U, 0, part);
    assert(step.status ==
           simtower::OriginalMetroPersonStepStatus::arrived_service);
    assert(!step.service_population_changed);
    assert(person[5] == std::byte{0x22} && person[7] == std::byte{2} &&
           person[8] == std::byte{0xfe});
    assert(person[9] == std::byte{2});
    assert(load_u16(person, 10U, false) == 200U);
    assert(load_u16(person, 12U, false) == 0xc000U);
    assert(load_u16(person, 14U, false) == 22U);
  }

  {
    // Population 40 takes 0cc2's full-service return two, retains state 41,
    // and places the person on the service floor without changing counters.
    simtower::OriginalPartTable part{};
    auto tower = make_metro_service_tower();
    auto& person = tower->people[0].exact_bytes;
    person[5] = std::byte{0x41};
    person[6] = std::byte{0};
    person[7] = std::byte{2};
    auto& service = tower->retail[0].exact_bytes;
    service[2] = std::byte{2};
    service[9] = std::byte{40};
    const auto step = simtower::step_original_metro_person(
        *tower, 0U, 0, part);
    assert(step.status ==
           simtower::OriginalMetroPersonStepStatus::service_full);
    assert(!step.service_population_changed);
    assert(person[5] == std::byte{0x41} && person[7] == std::byte{2} &&
           person[8] == std::byte{0xfe});
    assert(service[9] == std::byte{40});
  }

  {
    // An empty selected commercial family consumes only the family-choice
    // random value. 12dc returns -1 and the raw state table writes 0x27.
    simtower::OriginalPartTable part{};
    auto tower = make_metro_service_tower();
    tower->post_elevator.dynamic_dd5c.fill(std::byte{0});
    tower->post_elevator.dynamic_dd60.fill(std::byte{0});
    tower->post_elevator.dynamic_dd64.fill(std::byte{0});
    const auto step = simtower::step_original_metro_person(
        *tower, 0U, 0, part);
    assert(step.status ==
           simtower::OriginalMetroPersonStepStatus::no_destination);
    assert(step.service_index == -1);
    assert(tower->people[0].exact_bytes[5] == std::byte{0x27} &&
           tower->people[0].exact_bytes[6] == std::byte{0xff});
    assert(tower->random_state == 0x015a4e36U);
  }

  {
    // State 62 releases its completed downward Stair leg before choosing the
    // next adjacent leg; arrival on the following callback releases the new
    // counter and maps route return three back to state one.
    simtower::OriginalPartTable part{};
    auto tower = make_metro_service_tower();
    auto& person = tower->people[0].exact_bytes;
    person[5] = std::byte{0x62};
    person[7] = std::byte{3};
    person[8] = std::byte{5};
    auto& stair = tower->post_elevator.stairs_bd70[5];
    stair.used = 1U;
    stair.shape = 0U;
    stair.x = 100U;
    stair.floor = 2;
    stair.word_6 = 4U;
    stair.exact_bytes[0] = std::byte{1};
    stair.exact_bytes[1] = std::byte{0};
    store_u16(stair.exact_bytes, 2U, 100U, false);
    stair.exact_bytes[4] = std::byte{2};
    store_u16(stair.exact_bytes, 6U, 4U, false);
    tower->post_elevator.cf10[2] = std::byte{1};
    auto step = simtower::step_original_metro_person(*tower, 0U, 0, part);
    assert(step.status ==
           simtower::OriginalMetroPersonStepStatus::routed_home);
    assert(step.released_stair_counter);
    assert(step.route.status == simtower::OriginalPersonRouteStatus::stair);
    assert(stair.word_6 == 3U && stair.word_8 == 1U);
    assert(person[5] == std::byte{0x62} && person[7] == std::byte{2} &&
           person[8] == std::byte{5});
    step = simtower::step_original_metro_person(*tower, 0U, 0, part);
    assert(step.status ==
           simtower::OriginalMetroPersonStepStatus::returned_home);
    assert(step.released_stair_counter && stair.word_8 == 0U);
    assert(person[5] == std::byte{1});
  }

  {
    // Retail population and person timing words retain their logical values
    // in an opposite-endian TDT revision.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[27U] = 25U;
    auto tower = make_metro_service_tower(true);
    auto step = simtower::step_original_metro_person(*tower, 0U, 0, part);
    assert(step.status ==
           simtower::OriginalMetroPersonStepStatus::arrived_service);
    assert(load_u16(tower->retail[0].exact_bytes, 16U, true) == 1U);
    assert(load_u16(tower->people[0].exact_bytes, 10U, true) == 200U);
    tower->header.frame_time = 225U;
    step = simtower::step_original_metro_person(*tower, 0U, 0, part);
    assert(step.status ==
           simtower::OriginalMetroPersonStepStatus::returned_home);
    assert(load_u16(tower->people[0].exact_bytes, 10U, true) == 0U);
    assert(tower->retail[0].exact_bytes[9] == std::byte{0});
  }

  {
    // Direct 1220:50e2 coverage. The ordered 0daf subset retains its signed
    // time/random gates, 0x27 reset, event suppression, and explicit hold at
    // the cross-family timeout edge.
    simtower::OriginalPartTable part{};
    auto tower = make_metro_service_tower();
    tower->header.frame_time = 256U;
    tower->random_state = 1U;  // first rand is 346, not divisible by 36
    auto pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.scanned == 1U);
    assert(pass.dispatched == 0U);
    assert(pass.metro_dispatched == 0U);
    assert(tower->people[0].exact_bytes[5] == std::byte{1});
    assert(tower->random_state == 0x015a4e36U);

    tower = make_metro_service_tower();
    tower->header.frame_time = 256U;
    tower->random_state = 0U;  // gate rand returns zero
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.scanned == 1U && pass.dispatched == 1U &&
           pass.metro_dispatched == 1U && pass.changed == 1U);
    assert(tower->people[0].exact_bytes[5] == std::byte{0x22});

    tower = make_metro_service_tower();
    tower->header.frame_time = 2304U;
    tower->people[0].exact_bytes[5] = std::byte{0x27};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.changed == 1U && pass.dispatched == 0U);
    assert(tower->people[0].exact_bytes[5] == std::byte{1});

    tower = make_metro_service_tower();
    tower->header.frame_time = 0x8000U;
    tower->random_state = 1U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.dispatched == 0U && pass.changed == 0U);
    assert(tower->random_state == 1U);
    tower->people[0].exact_bytes[5] = std::byte{0x27};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.dispatched == 0U && pass.changed == 0U);
    assert(tower->people[0].exact_bytes[5] == std::byte{0x27});

    tower = make_metro_service_tower();
    tower->header.frame_time = 256U;
    tower->people[0].exact_bytes[5] = std::byte{0x41};
    tower->people[0].exact_bytes[8] = std::byte{0x40};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.elevator_timeout_checks == 1U &&
           pass.elevator_timeouts_triggered == 0U &&
           pass.dispatched == 0U && pass.changed == 0U);
    assert(tower->people[0].exact_bytes[5] == std::byte{0x41});

    store_u16(tower->header.exact_bytes, 60U, 1U, false);
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.scanned == 0U && pass.elevator_timeout_checks == 0U);
  }

  {
    // Direct 1220:4bde coverage: Restaurant/Fast Food state 20 first reserves
    // one scheduled customer via 11a8:10b3/1159, then enters the linked commercial
    // record on a same-floor route. The following cases cover its four-state
    // table, completed-Stair release, reservation rollback, route outcomes,
    // service capacity/dwell, history lanes, and opposite-endian words.
    // State five observes the type-12 dwell, returns to lobby, and applies
    // 1197's performance-band adjustment to the active history lane.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[5U] = 1U;   // rating 1/2 lower threshold
    part.words_00_to_40[8U] = 2U;   // rating 1/2 upper threshold
    part.words_00_to_40[14U] = 50U; // Fast Food dwell
    part.words_00_to_40[15U] = 10U; // Fast Food lane-three cap
    auto tower = make_food_service_tower();
    auto& person = tower->people[0].exact_bytes;
    auto& service = tower->retail[0].exact_bytes;
    auto step = simtower::step_original_food_service_person(
        *tower, 0U, 10, 0, part);
    assert(step.status ==
           simtower::OriginalFoodServicePersonStepStatus::arrived_service);
    assert(step.route.status ==
           simtower::OriginalPersonRouteStatus::already_on_floor);
    assert(step.service_index == 0 && step.reservation_changed &&
           step.service_population_changed &&
           step.service_tenant_marked_dirty);
    assert(person[5] == std::byte{0x05});
    assert(service[2] == std::byte{1} && service[6] == std::byte{9} &&
           service[7] == std::byte{1} && service[9] == std::byte{1});
    assert(load_u16(service, 16U, false) == 1U);

    tower->header.frame_time = 249U;
    step = simtower::step_original_food_service_person(
        *tower, 0U, 10, 0, part);
    assert(step.status ==
           simtower::OriginalFoodServicePersonStepStatus::waiting_at_service);
    assert(service[9] == std::byte{1});
    tower->header.frame_time = 250U;
    step = simtower::step_original_food_service_person(
        *tower, 0U, 10, 0, part);
    assert(step.status ==
           simtower::OriginalFoodServicePersonStepStatus::returned_home);
    assert(step.service_population_changed && step.service_history_changed);
    assert(person[5] == std::byte{0x27} && person[9] == std::byte{2});
    assert(service[2] == std::byte{0} && service[9] == std::byte{0} &&
           service[3] == std::byte{2});
  }

  {
    // A closed service terminates state 20 before reservation. A zero
    // scheduled population and 10b3's signed ordinal ceiling each retain the
    // state and all reservation counters byte-for-byte.
    simtower::OriginalPartTable part{};
    auto tower = make_food_service_tower();
    auto& person = tower->people[0].exact_bytes;
    auto& service = tower->retail[0].exact_bytes;
    service[2] = std::byte{3};
    auto step = simtower::step_original_food_service_person(
        *tower, 0U, 10, 0, part);
    assert(step.status ==
           simtower::OriginalFoodServicePersonStepStatus::service_closed);
    assert(person[5] == std::byte{0x27} && service[6] == std::byte{10} &&
           service[7] == std::byte{0});

    tower = make_food_service_tower();
    auto& zero_person = tower->people[0].exact_bytes;
    auto& zero_service = tower->retail[0].exact_bytes;
    zero_service[6] = std::byte{0};
    step = simtower::step_original_food_service_person(
        *tower, 0U, 10, 0, part);
    assert(step.status ==
           simtower::OriginalFoodServicePersonStepStatus::reservation_blocked);
    assert(zero_person[5] == std::byte{0x20} &&
           load_u16(zero_service, 16U, false) == 0U);

    tower = make_food_service_tower();
    auto& gated_person = tower->people[0].exact_bytes;
    auto& gated_service = tower->retail[0].exact_bytes;
    store_u16(gated_person, 2U, 4U, false);
    store_u16(gated_service, 12U, 0xfffeU, false); // limit is three
    step = simtower::step_original_food_service_person(
        *tower, 0U, 10, 0, part);
    assert(step.status ==
           simtower::OriginalFoodServicePersonStepStatus::reservation_blocked);
    assert(gated_service[6] == std::byte{10} &&
           gated_service[7] == std::byte{0} &&
           load_u16(gated_service, 16U, false) == 0U);
  }

  {
    // A failed initial route restores all three 10b3 counters through 1159
    // and clears the person metrics exactly as 4de6 does.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[3U] = 17U;
    auto tower = make_food_service_tower(12, 11);
    auto& person = tower->people[0].exact_bytes;
    auto& service = tower->retail[0].exact_bytes;
    store_u16(person, 12U, 0xc005U, false);
    store_u16(person, 14U, 9U, false);
    const auto step = simtower::step_original_food_service_person(
        *tower, 0U, 11, 0, part);
    assert(step.status ==
           simtower::OriginalFoodServicePersonStepStatus::route_failed);
    assert(step.route.status == simtower::OriginalPersonRouteStatus::no_route);
    assert(person[5] == std::byte{0x20} && person[9] == std::byte{0});
    assert(load_u16(person, 12U, false) == 0U &&
           load_u16(person, 14U, false) == 0U);
    assert(service[6] == std::byte{10} && service[7] == std::byte{0} &&
           load_u16(service, 16U, false) == 0U);
  }

  {
    // A completed state-45 leg executes 1130:0360 before 1197: 80/2 falls
    // below rating one's lower threshold, so lane three advances by two and
    // is capped by the type-12 PART value.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[5U] = 50U;
    part.words_00_to_40[8U] = 100U;
    part.words_00_to_40[15U] = 10U;
    auto tower = make_food_service_tower();
    auto& person = tower->people[0].exact_bytes;
    auto& service = tower->retail[0].exact_bytes;
    person[5] = std::byte{0x45};
    person[9] = std::byte{1};
    store_u16(person, 14U, 80U, false);
    service[3] = std::byte{3};
    auto step = simtower::step_original_food_service_person(
        *tower, 0U, 10, 0, part);
    assert(step.status ==
           simtower::OriginalFoodServicePersonStepStatus::returned_home);
    assert(step.service_history_changed && service[3] == std::byte{5});
    assert(person[5] == std::byte{0x27} && person[9] == std::byte{2});

    // The owner service word and person performance word retain their logical
    // meanings in the original opposite-endian save path.
    tower = make_food_service_tower(12, 10, true);
    auto& swapped_person = tower->people[0].exact_bytes;
    auto& swapped_service = tower->retail[0].exact_bytes;
    swapped_person[5] = std::byte{0x45};
    swapped_person[9] = std::byte{1};
    store_u16(swapped_person, 14U, 80U, true);
    swapped_service[3] = std::byte{3};
    step = simtower::step_original_food_service_person(
        *tower, 0U, 10, 0, part);
    assert(step.status ==
           simtower::OriginalFoodServicePersonStepStatus::returned_home);
    assert(swapped_service[3] == std::byte{5} &&
           load_u16(swapped_person, 14U, true) == 80U);
  }

  {
    // Direct 1220:49fa coverage: the ordered normal pass preserves its
    // distinct Fast Food and Restaurant gates, late 0x27 reset, and shared
    // timeout gate.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[15U] = 10U;
    auto tower = make_food_service_tower();
    tower->header.frame_time = 256U;
    tower->random_state = 1U; // first rand is 346, not divisible by 36
    auto pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.scanned == 1U && pass.dispatched == 0U &&
           pass.food_service_dispatched == 0U);
    assert(tower->people[0].exact_bytes[5] == std::byte{0x20});

    tower = make_food_service_tower();
    tower->header.frame_time = 0x8000U; // CWD/IDIV phase -81
    tower->random_state = 0U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.dispatched == 0U && pass.food_service_dispatched == 0U);
    assert(tower->people[0].exact_bytes[5] == std::byte{0x20} &&
           tower->random_state == 0U);

    tower = make_food_service_tower();
    tower->header.frame_time = 256U;
    tower->random_state = 0U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.dispatched == 1U && pass.food_service_dispatched == 1U &&
           pass.changed == 1U);
    assert(tower->people[0].exact_bytes[5] == std::byte{0x05});

    tower = make_food_service_tower(6);
    tower->header.frame_time = 1600U; // phase four Restaurant random gate
    tower->random_state = 0U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.food_service_dispatched == 1U && pass.changed == 1U);

    tower = make_food_service_tower(6);
    tower->header.frame_time = 2000U; // early phase five is unconditional
    tower->random_state = 123U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.food_service_dispatched == 1U &&
           tower->random_state == 123U);

    tower = make_food_service_tower();
    tower->header.frame_time = 2304U;
    tower->people[0].exact_bytes[5] = std::byte{0x27};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.dispatched == 0U && pass.changed == 1U &&
           tower->people[0].exact_bytes[5] == std::byte{0x20});

    tower = make_food_service_tower();
    tower->header.frame_time = 256U;
    tower->people[0].exact_bytes[5] = std::byte{0x60};
    tower->people[0].exact_bytes[8] = std::byte{0x40};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.elevator_timeout_checks == 1U &&
           pass.elevator_timeouts_triggered == 0U &&
           pass.food_service_dispatched == 0U && pass.changed == 0U);
  }

  {
    // An inactive Retail store activates only after the first viable customer
    // route. 1178:1140 adds rent/population, dirties the tenant, resets all
    // owned person metrics after routing, and requests UI visual code six.
    simtower::OriginalPartTable part{};
    simtower::OriginalYenTable rent{};
    rent[40U] = 123U; // type 10, rent tier zero
    auto tower = make_food_service_tower(10);
    auto& owner = tower->floors[10].tenants[0];
    auto& person = tower->people[0].exact_bytes;
    auto& service = tower->retail[0].exact_bytes;
    service[2] = std::byte{0xff};
    person[9] = std::byte{7};
    store_u16(person, 14U, 80U, false);
    const auto initial_balance = tower->header.balance;
    const auto initial_population =
        tower->post_elevator.finance.total_population;
    const auto step = simtower::step_original_retail_person(
        *tower, 0U, 10, 0, part, rent);
    assert(step.status == simtower::OriginalRetailPersonStepStatus::arrived_store);
    assert(step.store_activated && step.activation_visual_requested &&
           step.reservation_changed && step.service_population_changed);
    assert(step.route.status ==
           simtower::OriginalPersonRouteStatus::already_on_floor);
    assert(tower->header.balance == initial_balance + 123);
    assert(tower->post_elevator.finance.total_population ==
           initial_population + 10);
    assert(owner.exact_bytes[13] == std::byte{1});
    assert(person[5] == std::byte{0x05} && person[9] == std::byte{0} &&
           load_u16(person, 14U, false) == 0U);
    assert(service[2] == std::byte{1} && service[6] == std::byte{9} &&
           service[7] == std::byte{1} && service[9] == std::byte{1} &&
           load_u16(service, 16U, false) == 1U);
  }

  {
    // Inactive-store route failure takes 1220:4453's rollback path and does not
    // activate the store. The active-store failure instead keeps its reserved
    // customer and completes through the normal history cleanup.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[5U] = 1U;
    part.words_00_to_40[8U] = 2U;
    part.words_00_to_40[28U] = 10U;
    simtower::OriginalYenTable rent{};
    auto tower = make_food_service_tower(10, 11);
    auto& person = tower->people[0].exact_bytes;
    auto& service = tower->retail[0].exact_bytes;
    service[2] = std::byte{0xff};
    auto step = simtower::step_original_retail_person(
        *tower, 0U, 11, 0, part, rent);
    assert(step.status == simtower::OriginalRetailPersonStepStatus::route_failed);
    assert(!step.store_activated && !step.activation_visual_requested);
    assert(person[5] == std::byte{0x20} && service[2] == std::byte{0xff});
    assert(service[6] == std::byte{10} && service[7] == std::byte{0} &&
           load_u16(service, 16U, false) == 0U);

    tower = make_food_service_tower(10, 11);
    auto& active_person = tower->people[0].exact_bytes;
    auto& active_service = tower->retail[0].exact_bytes;
    active_service[2] = std::byte{0};
    step = simtower::step_original_retail_person(
        *tower, 0U, 11, 0, part, rent);
    assert(step.status == simtower::OriginalRetailPersonStepStatus::route_failed);
    assert(active_person[5] == std::byte{0x27});
    assert(active_service[6] == std::byte{9} &&
           active_service[7] == std::byte{1} &&
           load_u16(active_service, 16U, false) == 1U);
  }

  {
    // Opposite-endian Retail service links, reservation/attendance words, and
    // the post-route metric reset retain the same logical values.
    simtower::OriginalPartTable part{};
    simtower::OriginalYenTable rent{};
    rent[40U] = 77U;
    auto tower = make_food_service_tower(10, 10, true);
    auto& person = tower->people[0].exact_bytes;
    auto& service = tower->retail[0].exact_bytes;
    service[2] = std::byte{0xff};
    person[9] = std::byte{2};
    store_u16(person, 14U, 44U, true);
    const auto initial_balance = tower->header.balance;
    const auto step = simtower::step_original_retail_person(
        *tower, 0U, 10, 0, part, rent);
    assert(step.store_activated &&
           step.status == simtower::OriginalRetailPersonStepStatus::arrived_store);
    assert(load_u16(service, 16U, true) == 1U);
    assert(load_u16(person, 14U, true) == 0U);
    assert(tower->header.balance == initial_balance + 77);
  }

  {
    // Direct 1220:426c coverage: an inactive Retail store is suppressed unless
    // tenant byte 14 permits reopening. Once permitted, its phase-zero gate
    // consumes one rand and reports process-only visual six. The state-five
    // gate compares signed DS:b3a1, including a high-bit persisted clock.
    simtower::OriginalPartTable part{};
    simtower::OriginalYenTable rent{};
    rent[40U] = 50U;
    auto tower = make_food_service_tower(10);
    tower->header.frame_time = 256U;
    tower->random_state = 0U;
    tower->retail[0].exact_bytes[2] = std::byte{0xff};
    const auto initial_balance = tower->header.balance;
    auto pass = simtower::step_original_translated_people(
        *tower, part, rent);
    assert(pass.scanned == 1U && pass.retail_dispatched == 0U &&
           tower->random_state == 0U);

    tower->floors[10].tenants[0].exact_bytes[14] = std::byte{1};
    tower->floors[10].tenants[0].preserved_07_to_0f[7] = std::byte{1};
    pass = simtower::step_original_translated_people(*tower, part, rent);
    assert(pass.retail_dispatched == 1U && pass.changed == 1U &&
           pass.retail_activation_visual_requests == 1U);
    assert((pass.host_requests ==
           std::vector<simtower::OriginalPersonHostRequest>{
               {simtower::OriginalPersonHostRequestKind::income_status,
                6U, 0}}));
    assert(tower->people[0].exact_bytes[5] == std::byte{0x05});
    assert(tower->header.balance == initial_balance + 50);

    tower = make_food_service_tower(10);
    tower->header.frame_time = 256U;
    tower->random_state = 1U;
    pass = simtower::step_original_translated_people(*tower, part, rent);
    assert(pass.retail_dispatched == 0U &&
           tower->people[0].exact_bytes[5] == std::byte{0x20});

    tower = make_food_service_tower(10);
    tower->header.frame_time = 256U;
    tower->people[0].exact_bytes[5] = std::byte{0x60};
    tower->people[0].exact_bytes[8] = std::byte{0x40};
    pass = simtower::step_original_translated_people(*tower, part, rent);
    assert(pass.elevator_timeout_checks == 1U &&
           pass.elevator_timeouts_triggered == 0U &&
           pass.retail_dispatched == 0U && pass.changed == 0U);

    tower = make_food_service_tower(10);
    tower->header.frame_time = 0x8000U;
    tower->people[0].exact_bytes[5] = std::byte{0x05};
    pass = simtower::step_original_translated_people(*tower, part, rent);
    assert(pass.retail_dispatched == 0U && pass.changed == 0U);
    assert(tower->people[0].exact_bytes[5] == std::byte{0x05});
  }

  {
    // Direct 1180:0c29/0ce7/0d49/0d96 coverage: state 20 reserves the dc24
    // side selected by Movie/Party sign and
    // owning half type. A same-floor arrival advances phase 1 to 2, dirties
    // both Movie halves, and increments both exact attendance bytes.
    simtower::OriginalPartTable part{};
    auto tower = make_entertainment_person_tower();
    auto& person = tower->people[0].exact_bytes;
    auto& entertainment = tower->post_elevator.dc24_records[0];
    auto step = simtower::step_original_entertainment_person(
        *tower, 0U, 10, 0, part);
    assert(step.status == simtower::OriginalEntertainmentPersonStepStatus::
                              arrived_entertainment);
    assert(step.route.status ==
           simtower::OriginalPersonRouteStatus::already_on_floor);
    assert(step.entertainment_capacity_changed &&
           step.entertainment_record_changed && step.changed);
    assert(entertainment[4] == std::byte{0} &&
           entertainment[6] == std::byte{2} &&
           entertainment[10] == std::byte{1} &&
           entertainment[11] == std::byte{1});
    assert(person[5] == std::byte{3});
    assert(tower->floors[10].tenants[0].exact_bytes[13] == std::byte{1} &&
           tower->floors[10].tenants[1].exact_bytes[13] == std::byte{1});

    tower = make_entertainment_person_tower();
    tower->post_elevator.dc24_records[0][4] = std::byte{0};
    step = simtower::step_original_entertainment_person(
        *tower, 0U, 10, 0, part);
    assert(step.status == simtower::OriginalEntertainmentPersonStepStatus::
                              capacity_unavailable);
    assert(!step.changed &&
           tower->people[0].exact_bytes[5] == std::byte{0x20});

    // Initial no-route failure restores the reservation and clears only the
    // three metric fields at 9/12/14; a continuation failure does not reserve
    // and falls to state 0x27.
    tower = make_entertainment_person_tower();
    tower->post_elevator.dc24_records[0][0] = std::byte{30};
    auto& failed_person = tower->people[0].exact_bytes;
    failed_person[9] = std::byte{4};
    store_u16(failed_person, 12U, 20U, false);
    store_u16(failed_person, 14U, 30U, false);
    step = simtower::step_original_entertainment_person(
        *tower, 0U, 10, 0, part);
    assert(step.status ==
           simtower::OriginalEntertainmentPersonStepStatus::route_failed);
    assert(tower->post_elevator.dc24_records[0][4] == std::byte{1});
    assert(failed_person[5] == std::byte{0x20} &&
           failed_person[9] == std::byte{0} &&
           load_u16(failed_person, 12U, false) == 0U &&
           load_u16(failed_person, 14U, false) == 0U);
    failed_person[5] = std::byte{0x60};
    failed_person[7] = std::byte{11};
    failed_person[8] = std::byte{0xff};
    step = simtower::step_original_entertainment_person(
        *tower, 0U, 10, 0, part);
    assert(step.status ==
               simtower::OriginalEntertainmentPersonStepStatus::route_failed &&
           failed_person[5] == std::byte{0x27});
  }

  {
    // Party Hall uses dc24's negative-selector destination and the second
    // capacity lane for its type-30 half. The owner link remains logical in
    // an opposite-endian save.
    simtower::OriginalPartTable part{};
    auto tower = make_entertainment_person_tower(true, 30, 29);
    auto& entertainment = tower->post_elevator.dc24_records[0];
    entertainment[4] = std::byte{7};
    entertainment[5] = std::byte{1};
    const auto step = simtower::step_original_entertainment_person(
        *tower, 0U, 10, 0, part);
    assert(step.status == simtower::OriginalEntertainmentPersonStepStatus::
                              arrived_entertainment);
    assert(entertainment[4] == std::byte{7} &&
           entertainment[5] == std::byte{0});
    assert(tower->people[0].exact_bytes[5] == std::byte{3});
  }

  {
    // States 1/41 select one commercial family and floor band, enter through
    // 0cc2, then states 22/62 honor the exact PART dwell before returning to
    // dc24 byte 1. With a one-entry group this consumes exactly two rand calls.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[14U] = 5U;
    auto tower = make_entertainment_person_tower();
    tower->random_state = 0U;
    auto& person = tower->people[0].exact_bytes;
    auto& service = tower->retail[0].exact_bytes;
    person[5] = std::byte{1};
    auto step = simtower::step_original_entertainment_person(
        *tower, 0U, 10, 0, part);
    assert(step.status ==
           simtower::OriginalEntertainmentPersonStepStatus::arrived_service);
    assert(person[5] == std::byte{0x22} && person[6] == std::byte{0});
    assert(service[9] == std::byte{1} && service[2] == std::byte{1});
    assert(tower->random_state == 0x015a4e36U);

    tower->header.frame_time = 260U;
    step = simtower::step_original_entertainment_person(
        *tower, 0U, 10, 0, part);
    assert(step.status == simtower::OriginalEntertainmentPersonStepStatus::
                              waiting_at_service);
    assert(person[5] == std::byte{0x22} && service[9] == std::byte{1});
    tower->header.frame_time = 261U;
    step = simtower::step_original_entertainment_person(
        *tower, 0U, 10, 0, part);
    assert(step.status == simtower::OriginalEntertainmentPersonStepStatus::
                              returned_from_service);
    assert(person[5] == std::byte{0x27} && service[9] == std::byte{0} &&
           service[2] == std::byte{0});

    tower = make_entertainment_person_tower();
    tower->random_state = 0U;
    tower->people[0].exact_bytes[5] = std::byte{1};
    tower->retail[0].exact_bytes[9] = std::byte{40};
    tower->retail[0].exact_bytes[2] = std::byte{2};
    step = simtower::step_original_entertainment_person(
        *tower, 0U, 10, 0, part);
    assert(step.status ==
               simtower::OriginalEntertainmentPersonStepStatus::service_full &&
           tower->people[0].exact_bytes[5] == std::byte{0x41});
  }

  {
    // Exact 1220:5734-5838 ordering: a state in the continuation band releases
    // its completed Stair slot before the eight-state lookup rejects it.
    simtower::OriginalPartTable part{};
    auto tower = make_entertainment_person_tower();
    auto& person = tower->people[0].exact_bytes;
    person[5] = std::byte{0x64};
    person[7] = std::byte{10};
    person[8] = std::byte{0};
    auto& stair = tower->post_elevator.stairs_bd70[0];
    stair.floor = 10;
    stair.word_8 = 2U;
    store_u16(stair.exact_bytes, 8U, 2U, false);
    const auto step = simtower::step_original_entertainment_person(
        *tower, 0U, 10, 0, part);
    assert(step.status ==
           simtower::OriginalEntertainmentPersonStepStatus::unhandled_state);
    assert(step.released_stair_counter && step.changed);
    assert(stair.word_8 == 1U &&
           load_u16(stair.exact_bytes, 8U, false) == 1U);
  }

  {
    // Direct 1220:55b8 coverage: states 1/5/22 dispatch directly, state 20
    // randomizes only during signed day phases 0..3 after 0xf0 and resets at
    // phase four, while the shared Elevator timeout owns states 40+. Both
    // type-18 and type-29 records use this path.
    simtower::OriginalPartTable part{};
    auto tower = make_entertainment_person_tower();
    tower->random_state = 1U;  // 346 % 6 != 0
    auto pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.scanned == 1U && pass.entertainment_dispatched == 0U &&
           tower->people[0].exact_bytes[5] == std::byte{0x20});

    tower = make_entertainment_person_tower();
    tower->random_state = 0U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.entertainment_dispatched == 1U && pass.changed == 1U &&
           tower->people[0].exact_bytes[5] == std::byte{3});

    tower = make_entertainment_person_tower();
    tower->header.frame_time = 1600U;
    tower->random_state = 123U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.entertainment_dispatched == 0U && pass.changed == 1U &&
           tower->people[0].exact_bytes[5] == std::byte{0x27} &&
           tower->random_state == 123U);

    tower = make_entertainment_person_tower();
    tower->header.frame_time = 0x8000U;  // signed phase -81
    tower->random_state = 123U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.entertainment_dispatched == 0U && pass.changed == 0U &&
           tower->people[0].exact_bytes[5] == std::byte{0x20} &&
           tower->random_state == 123U);

    tower = make_entertainment_person_tower(false, 29, 29);
    tower->header.frame_time = 0U;
    tower->people[0].exact_bytes[5] = std::byte{5};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.entertainment_dispatched == 1U &&
           tower->people[0].exact_bytes[5] == std::byte{0x27});

    tower = make_entertainment_person_tower();
    tower->header.frame_time = 0U;
    tower->people[0].exact_bytes[5] = std::byte{0x60};
    tower->people[0].exact_bytes[8] = std::byte{0x40};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.entertainment_dispatched == 0U &&
           pass.elevator_timeout_checks == 1U &&
           pass.elevator_timeouts_triggered == 0U && pass.changed == 0U);
  }

  {
    // Exact 1220:3c09 states 0/40 share the lobby route, but only the initial
    // state decrements Condo occupancy through 1220:6f98. State 1 below
    // exercises the parallel 1220:71fe decrement. Direct 1220:7100 then makes
    // state 4 require both other resident ordinals at exact state 0x10 before
    // committing the synchronized owner/person 0x10 band.
    simtower::OriginalPartTable part{};
    auto tower = make_condo_person_tower();
    auto& owner = tower->floors[10].tenants[0];
    auto& person = tower->people[0].exact_bytes;
    person[5] = std::byte{0};
    auto step = simtower::step_original_condo_person(
        *tower, 0U, 10, 0, 0U, part, {});
    assert(step.status ==
           simtower::OriginalCondoPersonStepStatus::arrived_lobby);
    assert(step.route.status ==
           simtower::OriginalPersonRouteStatus::already_on_floor);
    assert(step.owner_status_changed && step.changed);
    assert(owner.status == 2U && owner.exact_bytes[5] == std::byte{2} &&
           owner.exact_bytes[13] == std::byte{1});
    assert(person[5] == std::byte{0x21});

    tower = make_condo_person_tower();
    auto& continuation_owner = tower->floors[10].tenants[0];
    tower->people[0].exact_bytes[5] = std::byte{0x40};
    step = simtower::step_original_condo_person(
        *tower, 0U, 10, 0, 0U, part, {});
    assert(step.status ==
           simtower::OriginalCondoPersonStepStatus::arrived_lobby);
    assert(continuation_owner.status == 3U &&
           tower->people[0].exact_bytes[5] == std::byte{0x21});

    tower->people[0].exact_bytes[5] = std::byte{4};
    step = simtower::step_original_condo_person(
        *tower, 0U, 10, 0, 0U, part, {});
    assert(step.status ==
           simtower::OriginalCondoPersonStepStatus::resident_synchronized);
    assert(tower->people[0].exact_bytes[5] == std::byte{0x10});
    assert(continuation_owner.status == 0x10U &&
           continuation_owner.exact_bytes[5] == std::byte{0x10});

    step = simtower::step_original_condo_person(
        *tower, 0U, 10, 0, 0U, part, {});
    assert(step.status ==
           simtower::OriginalCondoPersonStepStatus::resident_synchronized);
    assert(tower->people[0].exact_bytes[5] == std::byte{0});
    assert(continuation_owner.status == 3U);
  }

  {
    // 1220:3c09-3d0d releases completed Stair accounting before its exact
    // twelve-state lookup. A continuation-band value outside CS:423c is
    // unhandled only after that shared transit side effect.
    simtower::OriginalPartTable part{};
    auto tower = make_condo_person_tower();
    auto& person = tower->people[0].exact_bytes;
    person[5] = std::byte{0x64};
    person[7] = std::byte{10};
    person[8] = std::byte{0};
    auto& stair = tower->post_elevator.stairs_bd70[0];
    stair.floor = 10;
    stair.word_8 = 2U;
    store_u16(stair.exact_bytes, 8U, 2U, false);
    const auto step = simtower::step_original_condo_person(
        *tower, 0U, 10, 0, 0U, part, {});
    assert(step.status ==
           simtower::OriginalCondoPersonStepStatus::unhandled_state);
    assert(step.released_stair_counter && step.changed);
    assert(stair.word_8 == 1U &&
           load_u16(stair.exact_bytes, 8U, false) == 1U);
  }

  {
    // States 1/41 use 1230's fixed Condo commercial family and floor band.
    // The same-floor path enters through 0cc2; states 22/62 leave after the
    // exact PART dwell and complete through 7005's owner-status increment.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[14U] = 5U;
    auto tower = make_condo_person_tower();
    tower->random_state = 0U;
    auto& owner = tower->floors[10].tenants[0];
    auto& person = tower->people[0].exact_bytes;
    auto& service = tower->retail[0].exact_bytes;
    person[5] = std::byte{1};
    auto step = simtower::step_original_condo_person(
        *tower, 0U, 10, 0, 0U, part, {});
    assert(step.status ==
           simtower::OriginalCondoPersonStepStatus::arrived_service);
    assert(step.service_index == 0 && step.service_population_changed &&
           step.service_tenant_marked_dirty);
    assert(owner.status == 2U && person[5] == std::byte{0x22});
    assert(service[9] == std::byte{1} && service[2] == std::byte{1});
    assert(tower->random_state == 1U);

    tower->header.frame_time = 4U;
    step = simtower::step_original_condo_person(
        *tower, 0U, 10, 0, 0U, part, {});
    assert(step.status ==
           simtower::OriginalCondoPersonStepStatus::waiting_at_service);
    assert(person[5] == std::byte{0x22} && service[9] == std::byte{1});
    tower->header.frame_time = 5U;
    step = simtower::step_original_condo_person(
        *tower, 0U, 10, 0, 0U, part, {});
    assert(step.status ==
           simtower::OriginalCondoPersonStepStatus::arrived_home);
    assert(person[5] == std::byte{4} && owner.status == 3U);
    assert(service[9] == std::byte{0} && service[2] == std::byte{0});

    tower = make_condo_person_tower();
    auto& continuation = tower->people[0].exact_bytes;
    continuation[5] = std::byte{0x41};
    continuation[6] = std::byte{0};
    step = simtower::step_original_condo_person(
        *tower, 0U, 10, 0, 0U, part, {});
    assert(step.status ==
           simtower::OriginalCondoPersonStepStatus::arrived_service);
    assert(tower->floors[10].tenants[0].status == 3U);
    continuation[5] = std::byte{0x62};
    step = simtower::step_original_condo_person(
        *tower, 0U, 10, 0, 0U, part, {});
    assert(step.status ==
           simtower::OriginalCondoPersonStepStatus::arrived_home);
    assert(continuation[5] == std::byte{4} &&
           tower->floors[10].tenants[0].status == 4U);
  }

  {
    // Inactive state 20 reactivation is the persisted 1178:0fe3 sequence:
    // tiered rent, three residents, visual code 3, dirty owner, and all three
    // owned metric tails reset. Opposite-endian words retain logical values.
    simtower::OriginalPartTable part{};
    simtower::OriginalYenTable rent{};
    rent[38U] = 77U;  // type 9, rent tier two
    auto tower = make_condo_person_tower(true);
    auto& owner = tower->floors[10].tenants[0];
    owner.status = 0x18U;
    owner.exact_bytes[5] = std::byte{0x18};
    for (auto& resident : tower->people) {
      resident.exact_bytes[9] = std::byte{7};
      store_u16(resident.exact_bytes, 14U, 99U, true);
    }
    const auto initial_balance = tower->header.balance;
    const auto initial_population =
        tower->post_elevator.finance.total_population;
    const auto step = simtower::step_original_condo_person(
        *tower, 0U, 10, 0, 0U, part, rent);
    assert(step.status ==
           simtower::OriginalCondoPersonStepStatus::arrived_home);
    assert(step.condo_activated && step.activation_visual_requested &&
           step.owner_status_changed && step.changed);
    assert(owner.status == 1U && owner.exact_bytes[13] == std::byte{1});
    assert(tower->header.balance == initial_balance + 77);
    assert(tower->post_elevator.finance.total_population ==
           initial_population + 3);
    assert(tower->people[0].exact_bytes[5] == std::byte{4});
    for (const auto& resident : tower->people) {
      assert(resident.exact_bytes[9] == std::byte{0});
      assert(load_u16(resident.exact_bytes, 14U, true) == 0U);
    }

    auto dispatched_tower = make_condo_person_tower(true);
    auto& dispatched_owner = dispatched_tower->floors[10].tenants[0];
    dispatched_owner.status = 0x18U;
    dispatched_owner.exact_bytes[5] = std::byte{0x18};
    const auto dispatch = simtower::dispatch_original_person_family(
        *dispatched_tower, 0U, part, rent);
    assert((dispatch.host_requests ==
           std::vector<simtower::OriginalPersonHostRequest>{
               {simtower::OriginalPersonHostRequestKind::income_status,
                3U, 0}}));

    // Active and continuation home legs share their route table but never
    // repeat reactivation. State 21/61 also finish through the same 7005 path.
    for (const auto state : {0x60U, 0x21U, 0x61U}) {
      tower = make_condo_person_tower();
      auto& active_owner = tower->floors[10].tenants[0];
      tower->people[0].exact_bytes[5] = static_cast<std::byte>(state);
      const auto active_step = simtower::step_original_condo_person(
          *tower, 0U, 10, 0, 0U, part, rent);
      assert(active_step.status ==
             simtower::OriginalCondoPersonStepStatus::arrived_home);
      assert(!active_step.condo_activated &&
             tower->people[0].exact_bytes[5] == std::byte{4});
      assert(active_owner.status == 4U);
    }
  }

  {
    // Exact 1220:38e1 preserves its distinct day/calendar gates: state zero
    // randomizes only in phase zero, the calendar-one resident goes idle
    // after phase four, every CS:3bed state maps to its own recovered branch,
    // and Elevator waits defer to 1637.
    simtower::OriginalPartTable part{};
    auto tower = make_condo_person_tower();
    tower->random_state = 1U;  // 346 % 12 != 0
    tower->people[0].exact_bytes[5] = std::byte{0};
    auto pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.scanned == 1U && pass.condo_dispatched == 0U &&
           tower->people[0].exact_bytes[5] == std::byte{0});

    tower = make_condo_person_tower();
    tower->random_state = 0U;
    tower->people[0].exact_bytes[5] = std::byte{0};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 1U && pass.changed == 1U &&
           tower->people[0].exact_bytes[5] == std::byte{0x21});

    tower = make_condo_person_tower();
    tower->header.current_day = 2U;
    tower->header.frame_time = 2000U;  // phase five, lane zero
    tower->people[0].exact_bytes[5] = std::byte{1};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 0U && pass.changed == 1U &&
           tower->people[0].exact_bytes[5] == std::byte{4});

    // State 4 is 3b97: phase five randomizes residents zero/one, but ordinal
    // two dispatches immediately. Phase six is unconditional only after the
    // strict 0x0960 endpoint.
    tower = make_condo_person_tower();
    tower->header.frame_time = 2000U;
    tower->random_state = 1U;  // first Microsoft-RNG result is 346, not /12
    tower->people[0].exact_bytes[5] = std::byte{4};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 0U && tower->random_state != 1U &&
           tower->people[0].exact_bytes[5] == std::byte{4});

    tower = make_condo_person_tower();
    tower->header.frame_time = 2002U;  // scan ordinal two in phase five
    tower->random_state = 1U;
    tower->people[2].exact_bytes[5] = std::byte{4};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.scanned == 1U && pass.condo_dispatched == 1U &&
           tower->random_state == 1U &&
           tower->people[2].exact_bytes[5] == std::byte{0x10});

    tower = make_condo_person_tower();
    tower->header.frame_time = 2416U;
    tower->random_state = 1U;
    tower->people[0].exact_bytes[5] = std::byte{4};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 1U && tower->random_state == 1U);

    // State 0x10 is 39ba: phases zero through four always dispatch, phase
    // five does not, and the random cadence resumes strictly after 0x0a06.
    tower = make_condo_person_tower();
    tower->header.frame_time = 800U;
    tower->random_state = 1U;
    tower->people[0].exact_bytes[5] = std::byte{0x10};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 1U && tower->random_state == 1U);

    tower = make_condo_person_tower();
    tower->header.frame_time = 2000U;
    tower->people[0].exact_bytes[5] = std::byte{0x10};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 0U);

    tower = make_condo_person_tower();
    tower->header.frame_time = 2576U;
    tower->random_state = 0U;
    tower->people[0].exact_bytes[5] = std::byte{0x10};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 1U && tower->random_state == 1U);

    // State 0x20 is 3ace and tests the owning tenant's byte 14 before
    // dispatching in phases below five.
    tower = make_condo_person_tower();
    tower->header.frame_time = 1200U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 1U);

    tower = make_condo_person_tower();
    tower->header.frame_time = 1200U;
    tower->floors[10].tenants[0].exact_bytes[14] = std::byte{0};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 0U);

    tower = make_condo_person_tower();
    tower->header.frame_time = 2000U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 0U);

    // State 0x21 is 3b0c. Its departure phase depends on resident ordinal,
    // not on the tenant key: ordinal two uses phase three, others phase four.
    tower = make_condo_person_tower(false, 2U);
    tower->header.frame_time = 1200U;
    tower->random_state = 0U;
    tower->people[0].exact_bytes[5] = std::byte{0x21};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 0U && tower->random_state == 0U);

    tower = make_condo_person_tower();
    tower->header.frame_time = 1600U;
    tower->random_state = 0U;
    tower->people[0].exact_bytes[5] = std::byte{0x21};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 1U && tower->random_state == 1U);

    tower = make_condo_person_tower();
    tower->header.frame_time = 1202U;
    tower->random_state = 0U;
    tower->people[2].exact_bytes[5] = std::byte{0x21};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.scanned == 1U && pass.condo_dispatched == 1U &&
           tower->random_state == 1U);

    // State 0x22 is 3b8e and dispatches unconditionally from phase three.
    tower = make_condo_person_tower();
    tower->header.frame_time = 800U;
    tower->people[0].exact_bytes[5] = std::byte{0x22};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 0U);

    tower = make_condo_person_tower();
    tower->header.frame_time = 1200U;
    tower->random_state = 1U;
    tower->people[0].exact_bytes[5] = std::byte{0x22};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 1U && tower->random_state == 1U);

    tower = make_condo_person_tower(true);
    tower->header.frame_time = 0U;
    tower->people[0].exact_bytes[5] = std::byte{0x60};
    tower->people[0].exact_bytes[8] = std::byte{0x40};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.condo_dispatched == 0U &&
           pass.elevator_timeout_checks == 1U &&
           pass.elevator_timeouts_triggered == 0U && pass.changed == 0U);
  }

  {
    // Direct 1220:6e7d coverage: 3154's state 4 waits for the paired Hotel
    // guest (ordinal 3-n), while type 3 and owner status low-bits one bypass
    // that wait. State 10 converts a synchronized room into the type-specific
    // departure occupancy and state five.
    simtower::OriginalPartTable part{};
    auto tower = make_hotel_person_tower(4);
    auto& owner = tower->floors[10].tenants[0];
    tower->people[1].exact_bytes[5] = std::byte{4};
    tower->people[2].exact_bytes[5] = std::byte{0x10};
    auto step = simtower::step_original_hotel_person(
        *tower, 1U, 10, 0, 1U, part, {});
    assert(step.status ==
           simtower::OriginalHotelPersonStepStatus::guest_synchronized);
    assert(step.owner_status_changed && step.changed);
    assert(owner.status == 0x10U &&
           tower->people[1].exact_bytes[5] == std::byte{0x10});

    step = simtower::step_original_hotel_person(
        *tower, 1U, 10, 0, 1U, part, {});
    assert(step.status ==
           simtower::OriginalHotelPersonStepStatus::departure_prepared);
    assert(owner.status == 2U &&
           tower->people[1].exact_bytes[5] == std::byte{5});

    tower = make_hotel_person_tower(3);
    tower->floors[10].tenants[0].status = 3U;
    tower->floors[10].tenants[0].exact_bytes[5] = std::byte{3};
    tower->people[1].exact_bytes[5] = std::byte{4};
    step = simtower::step_original_hotel_person(
        *tower, 1U, 10, 0, 1U, part, {});
    assert(step.status ==
           simtower::OriginalHotelPersonStepStatus::guest_synchronized);
    assert(tower->floors[10].tenants[0].status == 0x10U);

    tower = make_hotel_person_tower(4);
    tower->floors[10].tenants[0].status = 9U;
    tower->floors[10].tenants[0].exact_bytes[5] = std::byte{9};
    tower->people[1].exact_bytes[5] = std::byte{4};
    assert(tower->people[2].exact_bytes[5] == std::byte{0x20});
    step = simtower::step_original_hotel_person(
        *tower, 1U, 10, 0, 1U, part, {});
    assert(step.status ==
           simtower::OriginalHotelPersonStepStatus::guest_synchronized);
    assert(tower->floors[10].tenants[0].status == 0x10U);
  }

  {
    // Direct 1220:6d82/7005 coverage. The parallel Hotel/Condo finish
    // helpers map owner status 0x10 to 1 before signed day phase four and 9
    // at/after it, otherwise increment the byte with wrap, always dirtying
    // serialized tenant byte 13.
    simtower::OriginalPartTable part{};
    const auto check_hotel = [&](std::uint16_t frame_time,
                                 std::uint8_t initial,
                                 std::uint8_t expected) {
      auto tower = make_hotel_person_tower(4);
      tower->header.frame_time = frame_time;
      auto& owner = tower->floors[10].tenants[0];
      owner.status = initial;
      owner.exact_bytes[5] = static_cast<std::byte>(initial);
      owner.exact_bytes[13] = std::byte{0};
      tower->people[1].exact_bytes[5] = std::byte{0x60};
      const auto step = simtower::step_original_hotel_person(
          *tower, 1U, 10, 0, 1U, part, {});
      assert(step.status ==
             simtower::OriginalHotelPersonStepStatus::arrived_hotel);
      assert(owner.status == expected &&
             owner.exact_bytes[5] == static_cast<std::byte>(expected) &&
             owner.exact_bytes[13] == std::byte{1});
    };
    check_hotel(0U, 0x10U, 1U);
    check_hotel(1599U, 0x10U, 1U);
    check_hotel(1600U, 0x10U, 9U);
    check_hotel(0U, 0xffU, 0U);

    const auto check_condo = [&](std::uint16_t frame_time,
                                 std::uint8_t initial,
                                 std::uint8_t expected) {
      auto tower = make_condo_person_tower();
      tower->header.frame_time = frame_time;
      auto& owner = tower->floors[10].tenants[0];
      owner.status = initial;
      owner.exact_bytes[5] = static_cast<std::byte>(initial);
      owner.exact_bytes[13] = std::byte{0};
      tower->people[0].exact_bytes[5] = std::byte{0x60};
      const auto step = simtower::step_original_condo_person(
          *tower, 0U, 10, 0, 0U, part, {});
      assert(step.status ==
             simtower::OriginalCondoPersonStepStatus::arrived_home);
      assert(owner.status == expected &&
             owner.exact_bytes[5] == static_cast<std::byte>(expected) &&
             owner.exact_bytes[13] == std::byte{1});
    };
    check_condo(0U, 0x10U, 1U);
    check_condo(1599U, 0x10U, 1U);
    check_condo(1600U, 0x10U, 9U);
    check_condo(0U, 0xffU, 0U);
  }

  {
    // Hotel states 1/41 call 1230:0000 with literal commercial family one;
    // states 22/62 dwell and return through 1230:0244 before 6d82 restores
    // the owner occupancy band.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[14U] = 5U;
    auto tower = make_hotel_person_tower(4);
    tower->random_state = 0U;
    auto& owner = tower->floors[10].tenants[0];
    auto& person = tower->people[1].exact_bytes;
    auto& service = tower->retail[0].exact_bytes;
    person[5] = std::byte{1};
    auto step = simtower::step_original_hotel_person(
        *tower, 1U, 10, 0, 1U, part, {});
    assert(step.status ==
           simtower::OriginalHotelPersonStepStatus::arrived_service);
    assert(step.service_index == 0 && step.service_population_changed &&
           step.service_tenant_marked_dirty);
    assert(owner.status == 2U && person[5] == std::byte{0x22});
    assert(service[9] == std::byte{1} && service[2] == std::byte{1});

    tower->header.frame_time = 4U;
    step = simtower::step_original_hotel_person(
        *tower, 1U, 10, 0, 1U, part, {});
    assert(step.status ==
           simtower::OriginalHotelPersonStepStatus::waiting_at_service);
    tower->header.frame_time = 5U;
    step = simtower::step_original_hotel_person(
        *tower, 1U, 10, 0, 1U, part, {});
    assert(step.status ==
           simtower::OriginalHotelPersonStepStatus::arrived_hotel);
    assert(person[5] == std::byte{4} && owner.status == 3U);
    assert(service[9] == std::byte{0} && service[2] == std::byte{0});
  }

  {
    // The last type-3 guest runs the exact 1178:0eac checkout: dirty closed
    // room, subtype/occupancy clear, tiered rent, population removal, process
    // cadence, and host visual code two. Type-4 inactive arrival performs the
    // complementary +2 activation without rent. Exact 1220:3154-3861 then
    // chooses state 1/4 from the tenant key, independently of person ordinal.
    simtower::OriginalPartTable part{};
    simtower::OriginalYenTable rent{};
    rent[14U] = 77U;  // type 3, tier two
    auto tower = make_hotel_person_tower(3);
    auto& owner = tower->floors[10].tenants[0];
    owner.status = 1U;
    owner.exact_bytes[5] = std::byte{1};
    tower->people[1].exact_bytes[5] = std::byte{5};
    const auto initial_balance = tower->header.balance;
    const auto initial_population =
        tower->post_elevator.finance.total_population;
    auto step = simtower::step_original_hotel_person(
        *tower, 1U, 10, 0, 1U, part, rent);
    assert(step.status ==
           simtower::OriginalHotelPersonStepStatus::departed_hotel);
    assert(step.room_checked_out && step.checkout_visual_requested &&
           step.owner_status_changed && step.changed);
    assert((step.host_requests ==
           std::vector<simtower::OriginalPersonHostRequest>{
               {simtower::OriginalPersonHostRequestKind::income_status,
                2U, 0}}));
    assert(owner.status == 0x28U && owner.exact_bytes[14] == std::byte{0} &&
           owner.exact_bytes[17] == std::byte{0} && owner.subtype == 0U);
    assert(tower->header.balance == initial_balance + 77);
    assert(tower->post_elevator.finance.total_population ==
           initial_population - 1);
    assert(tower->hotel_checkout_count == 1U &&
           tower->hotel_checkout_effect_active &&
           !tower->hotel_checkout_effect_cadence);
    // Direct 1118:0143 latch/audio coverage: repaint always follows a raised
    // DS:779e, WAVE/10013 is selected only when DS:02aa is nonzero, and the
    // painter restores cadence one before clearing the one-shot latch.
    auto checkout_presentation =
        simtower::consume_original_hotel_checkout_presentation(*tower);
    assert(checkout_presentation.repaint_balance &&
           !checkout_presentation.play_cash_sound);
    assert(!tower->hotel_checkout_effect_active &&
           tower->hotel_checkout_effect_cadence);
    assert(!simtower::consume_original_hotel_checkout_presentation(*tower)
                .repaint_balance);
    tower->hotel_checkout_effect_active = true;
    tower->hotel_checkout_effect_cadence = true;
    checkout_presentation =
        simtower::consume_original_hotel_checkout_presentation(*tower);
    assert(checkout_presentation.repaint_balance &&
           checkout_presentation.play_cash_sound);
    assert(!tower->hotel_checkout_effect_active &&
           tower->hotel_checkout_effect_cadence);

    // Direct 1178:0df9 coverage: inactive-room arrival selects the day-phase
    // status, clears subtype, dirties the tenant, and adds the type-specific
    // guest population before the occupancy transition.
    tower = make_hotel_person_tower(4, true);
    auto& active_owner = tower->floors[10].tenants[0];
    active_owner.status = 0x18U;
    active_owner.exact_bytes[5] = std::byte{0x18};
    tower->people[1].exact_bytes[5] = std::byte{0x20};
    const auto before_activation =
        tower->post_elevator.finance.total_population;
    step = simtower::step_original_hotel_person(
        *tower, 1U, 10, 0, 1U, part, rent);
    assert(step.status ==
           simtower::OriginalHotelPersonStepStatus::arrived_hotel);
    assert(step.room_activated && step.owner_status_changed && step.changed);
    assert(active_owner.status == 1U && active_owner.subtype == 0U &&
           tower->people[1].exact_bytes[5] == std::byte{1});
    assert(tower->post_elevator.finance.total_population ==
           before_activation + 2);

    tower = make_hotel_person_tower(4);
    auto& odd_key_owner = tower->floors[10].tenants[0];
    odd_key_owner.exact_bytes[12] = std::byte{1};
    odd_key_owner.preserved_07_to_0f[5] = std::byte{1};
    tower->floors[10].tenant_index[1] = 0U;
    tower->people[0].exact_bytes[1] = std::byte{1};
    step = simtower::step_original_hotel_person(
        *tower, 0U, 10, 1, 0U, part, rent);
    assert(step.status ==
           simtower::OriginalHotelPersonStepStatus::arrived_hotel);
    assert(tower->people[0].exact_bytes[5] == std::byte{4});
  }

  {
    // Direct 1198:06e7/0621/031a: eligibility admits the type-5 ordinal-zero
    // guest, the connected selector chooses its indexed Parking record,
    // increments the Hotel b846 lane, encodes the parking floor, and dirties
    // the tenant. 0489 reverses every persisted mutation on outbound failure.
    simtower::OriginalPartTable part{};
    auto tower = make_hotel_person_tower(5);
    tower->random_state = 0U;
    auto& person = tower->people[0].exact_bytes;
    auto& parking = tower->floors[9].tenants[0];
    person[5] = std::byte{0x20};
    auto step = simtower::step_original_hotel_person(
        *tower, 0U, 10, 0, 0U, part, {});
    assert(step.status ==
           simtower::OriginalHotelPersonStepStatus::route_failed);
    assert(step.parking_changed && step.notification_code == 0U);
    assert((load_u16(person, 12U, false) & 0xfc00U) == 0x0400U);
    assert(tower->post_elevator.b846_series[0][3] == 1 &&
           tower->post_elevator.b846_series[0][10] == 1);
    assert(load_u32(tower->post_elevator.cf9c_records[0], 2U, false) == 0U);
    assert(parking.status == 10U && parking.exact_bytes[13] == std::byte{1});

    person[5] = std::byte{5};
    step = simtower::step_original_hotel_person(
        *tower, 0U, 10, 0, 0U, part, {});
    assert(step.status ==
           simtower::OriginalHotelPersonStepStatus::route_failed);
    assert(step.parking_changed &&
           (load_u16(person, 12U, false) & 0xfc00U) == 0U);
    assert(tower->post_elevator.b846_series[0][3] == 0 &&
           tower->post_elevator.b846_series[0][10] == 0);
    assert(parking.status == 0U);

    tower = make_hotel_person_tower(5);
    tower->post_elevator.parking_connected = 0;
    tower->people[0].exact_bytes[5] = std::byte{0x20};
    step = simtower::step_original_hotel_person(
        *tower, 0U, 10, 0, 0U, part, {});
    assert(step.status ==
           simtower::OriginalHotelPersonStepStatus::parking_unavailable);
    assert(step.notification_code == 5U &&
           tower->people[0].exact_bytes[5] == std::byte{0x26});
  }

  {
    // Direct 1240:0000/00d1/0130/020d coverage inside the periodic type-5
    // visitor: the
    // nonzero b928 flag and exact 32-bit b924 person index select the special
    // completion path. It begins only after a parked ordinal-one departure on
    // day remainder three, emits 3002/3003, and updates the serialized
    // b923/b924/b928 triplet before parking cleanup.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[9U] = 100U;
    auto tower = make_hotel_person_tower(5);
    tower->header.current_day = 3;
    auto& person = tower->people[1].exact_bytes;
    person[5] = std::byte{0x45};
    person[7] = std::byte{9};
    store_u16(person, 12U, 0x0400U, false);
    store_u32(tower->post_elevator.cf9c_records[0], 2U, 1U, false);
    tower->post_elevator.b846_series[0][3] = 1;
    tower->post_elevator.b846_series[0][10] = 1;
    tower->floors[9].tenants[0].status = 2U;
    tower->floors[9].tenants[0].exact_bytes[5] = std::byte{2};
    auto step = simtower::step_original_hotel_person(
        *tower, 1U, 10, 0, 1U, part, {});
    assert(step.status ==
           simtower::OriginalHotelPersonStepStatus::departed_hotel);
    assert(step.process_requests.size() == 1U &&
           step.process_requests[0].transaction_code == 3000U &&
           step.process_requests[0].amount == 10000);
    assert((step.host_requests ==
           std::vector<simtower::OriginalPersonHostRequest>{
               {simtower::OriginalPersonHostRequestKind::hotel_dialog,
                3000U, 10000}}));
    assert(tower->post_elevator.b928 == 1U &&
           tower->post_elevator.b924 == 1);

    tower = make_hotel_person_tower(5);
    tower->header.current_day = 3;
    auto& scored = tower->people[1].exact_bytes;
    scored[5] = std::byte{0x45};
    scored[7] = std::byte{9};
    scored[9] = std::byte{2};
    store_u16(scored, 12U, 0x0400U, false);
    store_u16(scored, 14U, 100U, false);
    store_u32(tower->post_elevator.cf9c_records[0], 2U, 1U, false);
    tower->post_elevator.b846_series[0][3] = 1;
    tower->post_elevator.b846_series[0][10] = 1;
    tower->post_elevator.b928 = 1U;
    tower->post_elevator.b924 = 1;
    step = simtower::step_original_hotel_person(
        *tower, 1U, 10, 0, 1U, part, {});
    assert(step.process_requests.size() == 1U &&
           step.process_requests[0].transaction_code == 3002U);
    assert((step.host_requests ==
           std::vector<simtower::OriginalPersonHostRequest>{
               {simtower::OriginalPersonHostRequestKind::hotel_dialog,
                3002U, 0}}));
    assert(tower->post_elevator.b923 == 1U &&
           tower->post_elevator.b928 == 0U &&
           tower->post_elevator.b924 == -1);
  }

  {
    // Direct 1220:2e92 coverage: the exact wrapper excludes ordinal zero,
    // retains phase-specific random gates and direct state rewrites, and
    // defers Elevator timeout mutation at the shared cross-family boundary.
    simtower::OriginalPartTable part{};
    auto tower = make_hotel_person_tower(4);
    tower->header.frame_time = 1U;  // lane/ordinal one, phase zero
    tower->people[1].exact_bytes[5] = std::byte{5};
    tower->random_state = 1U;       // 346 % 12 != 0
    auto pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.scanned == 1U && pass.hotel_dispatched == 0U &&
           tower->people[1].exact_bytes[5] == std::byte{5});

    tower = make_hotel_person_tower(4);
    tower->header.frame_time = 1U;
    tower->people[1].exact_bytes[5] = std::byte{5};
    tower->random_state = 0U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.hotel_dispatched == 1U && pass.changed == 1U &&
           tower->people[1].exact_bytes[5] == std::byte{0x20});

    tower = make_hotel_person_tower(4);
    tower->header.frame_time = 2001U;  // phase five, lane one
    tower->people[1].exact_bytes[5] = std::byte{1};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.hotel_dispatched == 0U && pass.changed == 1U &&
           tower->people[1].exact_bytes[5] == std::byte{4});

    tower = make_hotel_person_tower(4);
    tower->header.frame_time = 2001U;
    tower->people[1].exact_bytes[5] = std::byte{0x20};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.hotel_dispatched == 1U &&
           tower->people[1].exact_bytes[5] == std::byte{1});

    // State 0x10 is the 2fa9 table branch: it always dispatches below phase
    // five, pauses through phase five/early six, then resumes random dispatch
    // strictly after frame 0x0a06.
    tower = make_hotel_person_tower(4);
    tower->header.frame_time = 801U;
    tower->random_state = 1U;
    tower->people[1].exact_bytes[5] = std::byte{0x10};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.hotel_dispatched == 1U && tower->random_state == 1U);

    tower = make_hotel_person_tower(4);
    tower->header.frame_time = 2001U;
    tower->people[1].exact_bytes[5] = std::byte{0x10};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.hotel_dispatched == 0U);

    tower = make_hotel_person_tower(4);
    tower->header.frame_time = 2577U;
    tower->random_state = 0U;
    tower->people[1].exact_bytes[5] = std::byte{0x10};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.hotel_dispatched == 1U && tower->random_state == 1U);

    tower = make_hotel_person_tower(5);
    tower->header.frame_time = 2305U;
    tower->people[1].exact_bytes[5] = std::byte{0x26};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.hotel_dispatched == 0U && pass.changed == 1U &&
           tower->people[1].exact_bytes[5] == std::byte{0x20});

    tower = make_hotel_person_tower(4);
    tower->header.frame_time = 0U;
    tower->people[0].exact_bytes[5] = std::byte{5};
    tower->random_state = 0U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.hotel_dispatched == 0U && pass.changed == 0U &&
           tower->random_state == 0U);

    tower = make_hotel_person_tower(4, true);
    tower->header.frame_time = 1U;
    tower->people[1].exact_bytes[5] = std::byte{0x60};
    tower->people[1].exact_bytes[8] = std::byte{0x40};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.hotel_dispatched == 0U &&
           pass.elevator_timeout_checks == 1U &&
           pass.elevator_timeouts_triggered == 0U && pass.changed == 0U);

    tower = make_hotel_person_tower(3);
    tower->header.frame_time = 1U;
    tower->people[1].exact_bytes[5] = std::byte{5};
    tower->floors[10].tenants[0].status = 1U;
    tower->floors[10].tenants[0].exact_bytes[5] = std::byte{1};
    tower->random_state = 0U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.hotel_dispatched == 1U &&
           pass.hotel_checkout_visual_requests == 1U);
    assert((pass.host_requests ==
           std::vector<simtower::OriginalPersonHostRequest>{
               {simtower::OriginalPersonHostRequestKind::income_status,
                2U, 0}}));
  }

  {
    // Direct 1220:23e4-24e8 boundary coverage: completed-Stair accounting
    // runs before the exact sixteen-entry Office state table. An unknown
    // state therefore remains unhandled but still releases its transit slot.
    simtower::OriginalPartTable part{};
    auto tower = make_office_normal_person_tower();
    auto& person = tower->people[1].exact_bytes;
    person[5] = std::byte{0x64};
    person[7] = std::byte{10};
    person[8] = std::byte{0};
    auto& stair = tower->post_elevator.stairs_bd70[0];
    stair.floor = 10;
    stair.word_8 = 2U;
    store_u16(stair.exact_bytes, 8U, 2U, false);
    const auto step = simtower::step_original_office_normal_person(
        *tower, 1U, 10, 3, 1U, part, {});
    assert(step.status ==
           simtower::OriginalOfficeNormalPersonStepStatus::unhandled_state);
    assert(step.released_stair_counter && step.changed);
    assert(stair.word_8 == 1U &&
           load_u16(stair.exact_bytes, 8U, false) == 1U);
  }

  {
    // Office states 1/41 use commercial family two. Same-floor arrival
    // occupies the service, 22 observes its exact dwell timer, and return
    // restores Office occupancy before selecting ordinal one's state zero.
    simtower::OriginalPartTable part{};
    part.words_00_to_40[14U] = 5U;
    auto tower = make_office_normal_person_tower();
    auto& owner = tower->floors[10U].tenants[0];
    auto& person = tower->people[1].exact_bytes;
    auto& service = tower->retail[0].exact_bytes;
    person[5] = std::byte{1};
    auto step = simtower::step_original_office_normal_person(
        *tower, 1U, 10, 3, 1U, part, {});
    assert(step.status ==
           simtower::OriginalOfficeNormalPersonStepStatus::arrived_service);
    assert(step.service_index == 0 && step.service_population_changed &&
           step.service_tenant_marked_dirty);
    assert(person[5] == std::byte{0x22} && owner.status == 5U);
    assert(service[9] == std::byte{1} && service[2] == std::byte{1});

    tower->header.frame_time = 4U;
    step = simtower::step_original_office_normal_person(
        *tower, 1U, 10, 3, 1U, part, {});
    assert(step.status ==
           simtower::OriginalOfficeNormalPersonStepStatus::waiting_at_service);
    tower->header.frame_time = 5U;
    step = simtower::step_original_office_normal_person(
        *tower, 1U, 10, 3, 1U, part, {});
    assert(step.status ==
           simtower::OriginalOfficeNormalPersonStepStatus::arrived_office);
    assert(person[5] == std::byte{0} && owner.status == 6U);
    assert(service[9] == std::byte{0} && service[2] == std::byte{0});
  }

  {
    // Direct 1170:0291/0414/0522/061c coverage inside Office
    // states 2/42/23/63:
    // grouped random selection, signed 40-person counter, 0414's 16-tick
    // dwell/population/last-patient dirty transition, and no-service code six.
    simtower::OriginalPartTable part{};
    auto tower = make_office_normal_person_tower(true);
    auto& owner = tower->floors[10U].tenants[0];
    auto& person = tower->people[1].exact_bytes;
    person[5] = std::byte{2};
    tower->random_state = 0U;
    auto step = simtower::step_original_office_normal_person(
        *tower, 1U, 10, 3, 1U, part, {});
    assert(step.status ==
           simtower::OriginalOfficeNormalPersonStepStatus::arrived_medical);
    assert(step.medical_service_index == 0 &&
           step.medical_population_changed &&
           step.medical_tenant_marked_dirty);
    assert(person[5] == std::byte{0x23} && owner.status == 5U);
    assert(((tower->post_elevator.dbfc_dwords[0] >> 16U) & 0xffU) == 1U);
    assert(load_u16(person, 10U, true) == 0U);

    tower->header.frame_time = 15U;
    step = simtower::step_original_office_normal_person(
        *tower, 1U, 10, 3, 1U, part, {});
    assert(step.status ==
           simtower::OriginalOfficeNormalPersonStepStatus::waiting_at_medical);
    assert(person[5] == std::byte{0x23});
    tower->header.frame_time = 16U;
    step = simtower::step_original_office_normal_person(
        *tower, 1U, 10, 3, 1U, part, {});
    assert(step.status ==
           simtower::OriginalOfficeNormalPersonStepStatus::arrived_office);
    assert(step.medical_population_changed &&
           step.medical_tenant_marked_dirty);
    assert(person[5] == std::byte{0} && owner.status == 6U);
    assert(((tower->post_elevator.dbfc_dwords[0] >> 16U) & 0xffU) == 0U);

    // If the selected Center becomes unavailable before arrival, 0291 calls
    // 11d8:02f7(dd82) and 11d8:0000 after the same-floor route already ran
    // its own finalizer. This means two completed metrics, with the original
    // upper-six flag bits preserved and the timer left at zero.
    tower = make_office_normal_person_tower(true);
    auto& unavailable = tower->people[1].exact_bytes;
    unavailable[5] = std::byte{2};
    unavailable[9] = std::byte{0};
    store_u16(unavailable, 10U, 0x1234U, true);
    store_u16(unavailable, 12U, 0xc005U, true);
    store_u16(unavailable, 14U, 7U, true);
    tower->post_elevator.dbfc_dwords[0] = 10U | (0xffU << 8U);
    tower->header.frame_time = 20U;
    part.words_00_to_40[4U] = 37U;
    tower->random_state = 0U;
    step = simtower::step_original_office_normal_person(
        *tower, 1U, 10, 3, 1U, part, {});
    assert(step.status ==
           simtower::OriginalOfficeNormalPersonStepStatus::arrived_medical);
    assert(step.medical_service_index == 0 &&
           !step.medical_population_changed &&
           !step.medical_tenant_marked_dirty);
    assert(unavailable[5] == std::byte{0x23} &&
           unavailable[7] == std::byte{10} &&
           unavailable[8] == std::byte{0xfd});
    assert(unavailable[9] == std::byte{2});
    assert(load_u16(unavailable, 10U, true) == 0U);
    assert(load_u16(unavailable, 12U, true) == 0xc000U);
    assert(load_u16(unavailable, 14U, true) == 49U);

    tower = make_office_normal_person_tower();
    tower->medical_route_index.fill(std::byte{0});
    tower->post_elevator.b92d = 1U;
    tower->people[1].exact_bytes[5] = std::byte{2};
    step = simtower::step_original_office_normal_person(
        *tower, 1U, 10, 3, 1U, part, {});
    assert(step.status ==
           simtower::OriginalOfficeNormalPersonStepStatus::arrived_medical);
    assert(step.notification_code == 6U &&
           tower->post_elevator.b92d == 0U);
    assert(tower->people[1].exact_bytes[5] == std::byte{0x23});

    // The ordered one-sixteenth wrapper retains the exact 1118:09be code so
    // the native host can update DS:784c in person-table dispatch order.
    tower = make_office_normal_person_tower();
    tower->header.frame_time = 801U;  // phase two, scan lane one
    tower->medical_route_index.fill(std::byte{0});
    tower->post_elevator.b92d = 1U;
    tower->people[1].exact_bytes[5] = std::byte{2};
    const auto pass =
        simtower::step_original_translated_people(*tower, part, {});
    assert(pass.office_normal_dispatched == 1U &&
           pass.office_medical_notifications == 1U);
    assert(pass.notification_codes.size() == 1U &&
           pass.notification_codes[0] == 6U);
    assert((pass.host_requests ==
           std::vector<simtower::OriginalPersonHostRequest>{
               {simtower::OriginalPersonHostRequestKind::notification_status,
                6U, 0}}));
  }

  {
    // Direct 1170:0635 coverage: ordinal-one same-floor Office arrival draws
    // only at rating three or above; rand zero selects Medical state two,
    // while rating two skips RNG and selects ordinary state one.
    simtower::OriginalPartTable part{};
    auto tower = make_office_normal_person_tower();
    tower->people[1].exact_bytes[5] = std::byte{0x60};
    tower->random_state = 0U;
    auto step = simtower::step_original_office_normal_person(
        *tower, 1U, 10, 3, 1U, part, {});
    assert(step.status ==
           simtower::OriginalOfficeNormalPersonStepStatus::arrived_office);
    assert(tower->people[1].exact_bytes[5] == std::byte{2});
    assert(tower->random_state == 1U);

    tower = make_office_normal_person_tower();
    tower->header.rating = 2U;
    tower->people[1].exact_bytes[5] = std::byte{0x60};
    tower->random_state = 0x12345678U;
    step = simtower::step_original_office_normal_person(
        *tower, 1U, 10, 3, 1U, part, {});
    assert(step.status ==
           simtower::OriginalOfficeNormalPersonStepStatus::arrived_office);
    assert(tower->people[1].exact_bytes[5] == std::byte{1});
    assert(tower->random_state == 0x12345678U);
  }

  {
    // Inactive state-60 arrival runs 1178:0cb4 exactly once: Office rent,
    // +6 population, status/dirty activation, six-person metric reset, host
    // visual code one, then 6bef's occupancy increment.
    simtower::OriginalPartTable part{};
    simtower::OriginalYenTable rent{};
    rent[30U] = 77U;  // Office type 7, tier 2
    auto tower = make_office_normal_person_tower();
    tower->header.rating = 2U;
    auto& owner = tower->floors[10U].tenants[0];
    owner.status = 0x10U;
    owner.exact_bytes[5] = std::byte{0x10};
    auto& person = tower->people[2].exact_bytes;
    person[5] = std::byte{0x60};
    person[7] = std::byte{10};
    for (auto& employee : tower->people) {
      employee.exact_bytes[9] = std::byte{9};
      store_u16(employee.exact_bytes, 14U, 99U, false);
    }
    const auto initial_balance = tower->header.balance;
    const auto initial_population =
        tower->post_elevator.finance.total_population;
    const auto step = simtower::step_original_office_normal_person(
        *tower, 2U, 10, 3, 2U, part, rent);
    assert(step.status ==
           simtower::OriginalOfficeNormalPersonStepStatus::arrived_office);
    assert(step.office_activated && step.activation_visual_requested &&
           step.owner_status_changed && step.changed);
    assert(owner.status == 1U && person[5] == std::byte{1});
    assert(tower->header.balance == initial_balance + 77);
    assert(tower->post_elevator.finance.total_population ==
           initial_population + 6);
    for (const auto& employee : tower->people) {
      assert(employee.exact_bytes[9] == std::byte{0});
      assert(load_u16(employee.exact_bytes, 14U, false) == 0U);
    }

    auto dispatched_tower = make_office_normal_person_tower();
    auto& dispatched_owner = dispatched_tower->floors[10U].tenants[0];
    dispatched_owner.status = 0x10U;
    dispatched_owner.exact_bytes[5] = std::byte{0x10};
    dispatched_tower->people[2].exact_bytes[5] = std::byte{0x60};
    dispatched_tower->people[2].exact_bytes[7] = std::byte{10};
    const auto dispatch = simtower::dispatch_original_person_family(
        *dispatched_tower, 2U, part, rent);
    assert((dispatch.host_requests ==
           std::vector<simtower::OriginalPersonHostRequest>{
               {simtower::OriginalPersonHostRequestKind::income_status,
                1U, 0}}));
  }

  {
    // Eligible ordinal two uses 1198:031a/002f before its state-20 route, keeps
    // the assigned parking record while in transit, and 0489 reverses all
    // parking accounting after state-45 reaches the parking floor.
    simtower::OriginalPartTable part{};
    auto tower = make_office_normal_person_tower();
    tower->random_state = 0U;
    auto& person = tower->people[2].exact_bytes;
    person[5] = std::byte{0x20};
    auto& stair = tower->post_elevator.stairs_bd70[0];
    stair.used = 1U;
    stair.shape = 0U;
    stair.x = 100U;
    stair.floor = 9;
    stair.exact_bytes[0] = std::byte{1};
    stair.exact_bytes[1] = std::byte{0};
    store_u16(stair.exact_bytes, 2U, 100U, false);
    stair.exact_bytes[4] = std::byte{9};
    auto step = simtower::step_original_office_normal_person(
        *tower, 2U, 10, 3, 2U, part, {});
    assert(step.status ==
           simtower::OriginalOfficeNormalPersonStepStatus::routed_to_office);
    assert(step.parking_changed && person[5] == std::byte{0x60});
    assert((load_u16(person, 12U, false) & 0xfc00U) == 0x0400U);
    assert(tower->post_elevator.b846_series[0][0] == 1 &&
           tower->post_elevator.b846_series[0][10] == 1);
    assert(load_u32(tower->post_elevator.cf9c_records[0], 2U, false) == 2U);

    person[5] = std::byte{0x45};
    person[7] = std::byte{9};
    person[8] = std::byte{0xff};
    step = simtower::step_original_office_normal_person(
        *tower, 2U, 10, 3, 2U, part, {});
    assert(step.status ==
           simtower::OriginalOfficeNormalPersonStepStatus::departed_office);
    assert(step.parking_changed && person[5] == std::byte{0x27});
    assert((load_u16(person, 12U, false) & 0xfc00U) == 0U);
    assert(tower->post_elevator.b846_series[0][0] == 0 &&
           tower->post_elevator.b846_series[0][10] == 0);
    assert(tower->floors[9U].tenants[0].status == 0U);
  }

  {
    // Direct 1220:2068 coverage: retain every captured phase/random gate,
    // the direct 5/27/20 rewrites, and the shared Elevator-timeout boundary.
    simtower::OriginalPartTable part{};
    auto tower = make_office_normal_person_tower();
    tower->people[0].exact_bytes[5] = std::byte{0};
    tower->random_state = 1U;
    auto pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.scanned == 1U && pass.office_normal_dispatched == 0U &&
           tower->random_state == 0x015a4e36U);

    tower = make_office_normal_person_tower();
    tower->people[0].exact_bytes[5] = std::byte{0};
    tower->random_state = 0U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.office_normal_dispatched == 1U && pass.changed == 1U);
    assert(tower->people[0].exact_bytes[5] == std::byte{0x21});

    tower = make_office_normal_person_tower();
    tower->header.frame_time = 1600U;  // phase four, lane zero
    tower->people[0].exact_bytes[5] = std::byte{0};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.office_normal_dispatched == 0U && pass.changed == 1U &&
           tower->people[0].exact_bytes[5] == std::byte{5});

    tower = make_office_normal_person_tower();
    tower->header.frame_time = 400U;  // phase one, lane zero
    tower->people[0].exact_bytes[5] = std::byte{0x20};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.office_normal_dispatched == 1U &&
           tower->people[0].exact_bytes[5] == std::byte{0});

    tower = make_office_normal_person_tower();
    tower->header.frame_time = 2304U;
    tower->people[0].exact_bytes[5] = std::byte{0x27};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.office_normal_dispatched == 0U && pass.changed == 1U &&
           tower->people[0].exact_bytes[5] == std::byte{0x20});

    tower = make_office_normal_person_tower(true);
    tower->header.frame_time = 0U;
    tower->people[0].exact_bytes[5] = std::byte{0x60};
    tower->people[0].exact_bytes[8] = std::byte{0x40};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.office_normal_dispatched == 0U &&
           pass.elevator_timeout_checks == 1U &&
           pass.elevator_timeouts_triggered == 0U && pass.changed == 0U);
  }

  {
    // Direct 1220:5edd coverage: its calendar-one morning gate consumes the
    // Microsoft rand exactly after frame 0x50. Its fallthrough after frame
    // 0xf0 can call 6037 a second time for the same person in one scan.
    simtower::OriginalPartTable part{};
    auto tower = make_cathedral_wrapper_tower();
    tower->random_state = 1U;  // first result 346, not divisible by 12
    auto pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.scanned == 1U && pass.cathedral_dispatched == 0U &&
           tower->people[0].exact_bytes[5] == std::byte{0x20});
    assert(tower->random_state == 0x015a4e36U);

    tower = make_cathedral_wrapper_tower();
    tower->random_state = 0U;  // first result zero
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.dispatched == 1U && pass.cathedral_dispatched == 1U &&
           pass.changed == 1U);
    assert(tower->people[0].exact_bytes[5] == std::byte{0x27});

    tower = make_cathedral_wrapper_tower();
    tower->header.frame_time = 256U;
    tower->random_state = 0U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.dispatched == 2U && pass.cathedral_dispatched == 2U &&
           pass.changed == 1U);
    assert(tower->people[0].exact_bytes[5] == std::byte{0x27});

    // Once the signed day phase is positive, 5edd writes 0x27 directly and
    // does not consume rand or enter the raw family.
    tower = make_cathedral_wrapper_tower();
    tower->header.frame_time = 704U;
    tower->random_state = 123U;
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.cathedral_dispatched == 0U && pass.changed == 1U &&
           tower->people[0].exact_bytes[5] == std::byte{0x27});
    assert(tower->random_state == 123U);

    // State five is unconditional. A zero timestamp leaves Elevator-waiting
    // states unarmed at 1637, including an opposite-endian owner record.
    tower = make_cathedral_wrapper_tower(true);
    tower->header.frame_time = 0U;
    tower->people[0].exact_bytes[5] = std::byte{5};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.cathedral_dispatched == 1U && pass.changed == 1U &&
           tower->people[0].exact_bytes[5] == std::byte{0x27});

    tower = make_cathedral_wrapper_tower();
    tower->header.frame_time = 0U;
    tower->people[0].exact_bytes[5] = std::byte{0x60};
    tower->people[0].exact_bytes[8] = std::byte{0x40};
    pass = simtower::step_original_translated_people(*tower, part);
    assert(pass.cathedral_dispatched == 0U &&
           pass.elevator_timeout_checks == 1U &&
           pass.elevator_timeouts_triggered == 0U && pass.changed == 0U);
  }

  {
    // The ordered wrapper carries 6037's 1040:00f0 ceremony result back to
    // the host without replacing its original focus/channel/WAVE requests.
    auto tower = make_cathedral_arrival_tower_on_heap();
    tower->header.frame_time = 256U;
    auto& person = tower->people[0].exact_bytes;
    person[0] = std::byte{109};
    person[1] = std::byte{0};
    store_u16(person, 2U, 0U, false);
    person[5] = std::byte{0x60};
    person[7] = std::byte{109};
    person[8] = std::byte{0xff};
    simtower::OriginalPartTable part{};
    part.dwords_42_to_4e = {1000U, 3000U, 6000U, 10000U};
    const auto pass =
        simtower::step_original_translated_people(*tower, part);
    assert(pass.cathedral_dispatched == 1U &&
           pass.cathedral_arrival_checks == 1U &&
           pass.cathedral_ceremony.has_value());
    assert(pass.cathedral_ceremony->status ==
           simtower::OriginalCathedralArrivalStatus::ceremony_started);
    assert(pass.cathedral_ceremony->effect_floor == 111 &&
           pass.cathedral_ceremony->wave_resource == 10008);
    assert(tower->header.rating == 6U && person[5] == std::byte{3});
  }

  {
    // Direct 1220:6037 coverage: Cathedral state 0x20 uses the literal lobby
    // floor 10, resolves only the first route leg toward 109, and maps route
    // returns 0..2 to state 0x60.
    auto tower = simtower::make_original_new_tdt();
    initialize_route_person(tower, 0U, 109, 0U, 100U);
    auto& person = tower.people[0].exact_bytes;
    person[4] = std::byte{36};
    person[5] = std::byte{0x20};
    person[7] = std::byte{99};  // ignored by the initial state
    auto& route = tower.post_elevator.routes_bff0[0];
    route[1] = std::byte{1};
    route[2] = std::byte{109};
    route[3] = std::byte{10};
    auto& stair = tower.post_elevator.stairs_bd70[6];
    stair.used = 1U;
    stair.shape = 0U;
    stair.x = 100U;
    stair.floor = 10;
    stair.exact_bytes[0] = std::byte{1};
    stair.exact_bytes[4] = std::byte{10};
    simtower::OriginalPersonRouteRequest context{};
    context.add_distance_penalty = false;  // overridden to true by state 20
    const auto step = simtower::step_original_cathedral_person(
        tower, 0U, context);
    assert(step.status ==
           simtower::OriginalCathedralPersonStepStatus::routed);
    assert(step.route.status == simtower::OriginalPersonRouteStatus::stair);
    assert(person[5] == std::byte{0x60});
    assert(person[7] == std::byte{11} && person[8] == std::byte{6});
    assert(!step.cathedral_arrival_check_requested);
  }

  {
    // State 0x60 at floor 109 maps route return 3 to state 3 and emits the
    // exact separate 1040:00f0 ceremony-check boundary.
    auto tower = simtower::make_original_new_tdt();
    initialize_route_person(tower, 0U, 109, 0U, 100U);
    auto& person = tower.people[0].exact_bytes;
    person[4] = std::byte{36};
    person[5] = std::byte{0x60};
    person[7] = std::byte{109};
    person[8] = std::byte{0xff};
    person[9] = std::byte{2};
    store_u16(person, 12U, 20U, false);
    const auto step = simtower::step_original_cathedral_person(
        tower, 0U, {});
    assert(step.status ==
           simtower::OriginalCathedralPersonStepStatus::arrived_cathedral);
    assert(step.route.status ==
           simtower::OriginalPersonRouteStatus::already_on_floor);
    assert(person[5] == std::byte{3});
    assert(person[9] == std::byte{3});
    assert(step.cathedral_arrival_check_requested);
  }

  {
    // State 0x45 releases a completed Stair counter before routing. At lobby
    // floor 10, return 3 maps to the shared idle/failure state 0x27.
    auto tower = simtower::make_original_new_tdt();
    initialize_route_person(tower, 0U, 109, 0U, 100U);
    auto& person = tower.people[0].exact_bytes;
    person[4] = std::byte{36};
    person[5] = std::byte{0x45};
    person[7] = std::byte{10};
    person[8] = std::byte{7};
    auto& stair = tower.post_elevator.stairs_bd70[7];
    stair.floor = 10;
    stair.word_8 = 4U;
    store_u16(stair.exact_bytes, 8U, 4U, false);
    const auto step = simtower::step_original_cathedral_person(
        tower, 0U, {});
    assert(step.released_stair_counter);
    assert(stair.word_8 == 3U &&
           load_u16(stair.exact_bytes, 8U, false) == 3U);
    assert(step.status ==
           simtower::OriginalCathedralPersonStepStatus::returned_to_lobby);
    assert(person[5] == std::byte{0x27});
  }

  {
    // The four recovered switch keys are literal: other type-36 states are
    // byte-exact no-ops, while the family guard rejects non-Cathedral people.
    auto tower = simtower::make_original_new_tdt();
    initialize_route_person(tower, 0U, 109, 0U, 100U);
    tower.people[0].exact_bytes[4] = std::byte{36};
    tower.people[0].exact_bytes[5] = std::byte{0x21};
    auto step = simtower::step_original_cathedral_person(tower, 0U, {});
    assert(step.status ==
           simtower::OriginalCathedralPersonStepStatus::unhandled_state);
    assert(tower.people[0].exact_bytes[5] == std::byte{0x21});
    tower.people[0].exact_bytes[4] = std::byte{33};
    step = simtower::step_original_cathedral_person(tower, 0U, {});
    assert(step.status ==
           simtower::OriginalCathedralPersonStepStatus::not_cathedral);
  }

  {
    // 1040:03bb counts exactly eight people from each of the five Cathedral
    // records. The fortieth arrival directly covers 1040:02b5's b406/b40c,
    // rating, frame, effect, channel-stop, and WAVE/10008 boundary in order.
    auto tower = make_cathedral_arrival_tower();
    simtower::OriginalPartTable part{};
    part.dwords_42_to_4e = {1000U, 3000U, 6000U, 10000U};
    const auto arrival =
        simtower::apply_original_cathedral_arrival_check(tower, part);
    assert(arrival.status ==
           simtower::OriginalCathedralArrivalStatus::ceremony_started);
    assert(arrival.arrived_people == 40U);
    assert(arrival.effect_floor == 111 && arrival.effect_x == 17U);
    assert(arrival.rating_changed && arrival.repaint_requested &&
           arrival.stop_both_audio_channels);
    assert(arrival.wave_resource == 10008 && arrival.wave_repeat == 5U &&
           arrival.wave_priority == 4U);
    assert(tower.header.rating == 6U);
    assert(load_u16(tower.header.exact_bytes, 2U, false) == 6U);
    assert(load_u16(tower.header.exact_bytes, 60U, false) == 6U);
    assert(load_u32(tower.header.exact_bytes, 66U, false) == 0U);
    for (std::size_t floor_number = 109U; floor_number <= 113U;
         ++floor_number) {
      const auto& tenant = tower.floors[floor_number].tenants[0];
      assert(tenant.variant == 2U);
      assert(load_u16(tenant.exact_bytes, 6U, false) == 2U);
      assert(tenant.exact_bytes[13] == std::byte{1});
    }
    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.header.rating == 6U);
    assert(load_u16(reparsed.header.exact_bytes, 60U, false) == 6U);
    assert(load_u32(reparsed.header.exact_bytes, 66U, false) == 0U);
    assert(reparsed.floors[109].tenants[0].variant == 2U);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Before all forty arrive, or when population implies a rating above the
    // current one, 00f0 writes only frame word three on the bottom part.
    simtower::OriginalPartTable part{};
    part.dwords_42_to_4e = {1000U, 3000U, 6000U, 10000U};
    auto tower = make_cathedral_arrival_tower();
    tower.people[39].exact_bytes[5] = std::byte{0x20};
    auto arrival =
        simtower::apply_original_cathedral_arrival_check(tower, part);
    assert(arrival.status ==
           simtower::OriginalCathedralArrivalStatus::waiting_for_people);
    assert(arrival.arrived_people == 39U);
    assert(tower.floors[109].tenants[0].variant == 3U);
    assert(tower.header.rating == 5U);
    assert(load_u16(tower.header.exact_bytes, 60U, false) == 2U);

    tower = make_cathedral_arrival_tower();
    tower.header.rating = 1U;
    tower.post_elevator.finance.total_population = 1500;
    arrival = simtower::apply_original_cathedral_arrival_check(tower, part);
    assert(arrival.status ==
           simtower::OriginalCathedralArrivalStatus::population_rating_gate);
    assert(arrival.arrived_people == 0U);
    assert(tower.floors[109].tenants[0].variant == 3U);
    assert(load_u32(tower.header.exact_bytes, 66U, false) == 0x12345678U);
  }

  {
    // The frame-800 cutoff and already-six branch return before every
    // ceremony mutation, exactly as 00f0 and 02b5 do.
    simtower::OriginalPartTable part{};
    part.dwords_42_to_4e = {1000U, 3000U, 6000U, 10000U};
    auto tower = make_cathedral_arrival_tower();
    tower.header.frame_time = 800U;
    auto arrival =
        simtower::apply_original_cathedral_arrival_check(tower, part);
    assert(arrival.status ==
           simtower::OriginalCathedralArrivalStatus::outside_arrival_window);
    assert(tower.floors[109].tenants[0].variant == 3U);
    assert(tower.header.rating == 5U);

    tower = make_cathedral_arrival_tower();
    tower.header.rating = 6U;
    arrival = simtower::apply_original_cathedral_arrival_check(tower, part);
    assert(arrival.status ==
           simtower::OriginalCathedralArrivalStatus::already_maximum_rating);
    assert(arrival.arrived_people == 40U);
    assert(load_u16(tower.header.exact_bytes, 60U, false) == 2U);
    assert(load_u32(tower.header.exact_bytes, 66U, false) == 0x12345678U);
  }

  {
    // PART/1000 offsets 02/06/3e/40 and the persisted clock/day reconstruct
    // the exact process route context. The complete family wrapper then lets
    // the fortieth state-60 arrival invoke 1040:00f0 in the same call.
    auto tower = make_cathedral_arrival_tower();
    tower.header.current_day = 2;
    tower.people[0].exact_bytes[0] = std::byte{109};
    tower.people[0].exact_bytes[1] = std::byte{0};
    tower.people[0].exact_bytes[5] = std::byte{0x60};
    tower.people[0].exact_bytes[7] = std::byte{109};
    simtower::OriginalPartTable part{};
    part.words_00_to_40[1] = 11U;
    part.words_00_to_40[3] = 22U;
    part.words_00_to_40[31] = 33U;
    part.words_00_to_40[32] = 44U;
    part.dwords_42_to_4e = {1000U, 3000U, 6000U, 10000U};
    const auto context = simtower::original_person_route_context(tower, part);
    assert(context.frame_time == 700U && context.queue_full_delay == 11U &&
           context.no_route_delay == 22U &&
           context.stair_even_delay == 33U &&
           context.stair_odd_delay == 44U);
    assert(context.calendar_phase == 1U && context.day_phase == 1U);

    const auto dispatch = simtower::dispatch_original_cathedral_person(
        tower, 0U, part);
    assert(dispatch.step.status ==
           simtower::OriginalCathedralPersonStepStatus::arrived_cathedral);
    assert(dispatch.arrival.has_value());
    assert(dispatch.arrival->status ==
           simtower::OriginalCathedralArrivalStatus::ceremony_started);
    assert(tower.people[0].exact_bytes[5] == std::byte{3});
    assert(tower.header.rating == 6U);
  }

  {
    // 1210:1ac5 is only the slot pop. It returns the saved dword and clears
    // the parallel destination/person fields without touching car counts.
    auto tower = simtower::make_original_new_tdt();
    auto& car = tower.elevators[23].car_records[7].exact_bytes;
    car.fill(std::byte{0x5a});
    car[3] = std::byte{9};
    car[12] = std::byte{4};
    car[226U + 17U] = std::byte{3};
    store_u32(car, 16U + 41U * 4U, 0x12345678U, false);
    car[184U + 41U] = std::byte{17};

    const auto popped = simtower::pop_original_elevator_car_passenger_slot(
        tower, 23U, 7U, 41U);
    assert(popped && *popped == 0x12345678U);
    assert(load_u32(car, 16U + 41U * 4U, false) == 0xffffffffU);
    assert(car[184U + 41U] == std::byte{0xff});
    assert(car[3] == std::byte{9} && car[12] == std::byte{4});
    assert(car[226U + 17U] == std::byte{3});

    const auto unchanged = car;
    assert(!simtower::pop_original_elevator_car_passenger_slot(
        tower, 24U, 0U, 0U));
    assert(!simtower::pop_original_elevator_car_passenger_slot(
        tower, 0U, 8U, 0U));
    assert(!simtower::pop_original_elevator_car_passenger_slot(
        tower, 0U, 0U, 42U));
    assert(car == unchanged);
  }

  {
    // The helper follows the TDT's revision-aware byte order and returns an
    // engaged optional even when the original slot already held ffffffff.
    auto tower = simtower::make_original_new_tdt();
    tower.header.byte_swapped = true;
    auto& car = tower.elevators[0].car_records[0].exact_bytes;
    store_u32(car, 16U + 6U * 4U, 0x89abcdefU, true);
    car[184U + 6U] = std::byte{33};
    const auto popped = simtower::pop_original_elevator_car_passenger_slot(
        tower, 0U, 0U, 6U);
    assert(popped && *popped == 0x89abcdefU);
    assert(load_u32(car, 16U + 6U * 4U, true) == 0xffffffffU);
    assert(car[184U + 6U] == std::byte{0xff});

    const auto empty = simtower::pop_original_elevator_car_passenger_slot(
        tower, 0U, 0U, 6U);
    assert(empty && *empty == 0xffffffffU);
  }

  {
    // Complete container/ring order of 10a0:14cc. Direct 10a0:1625 coverage
    // proves the car slot is cleared
    // before its family callback but counts are decremented afterward. Then
    // 1625 advances each ring cursor, applies 11d8:02f7, dispatches, and only
    // then decrements that ring count.
    auto tower = simtower::make_original_new_tdt();
    tower.people_count = 5U;
    tower.people.resize(5U);
    for (auto& person : tower.people) person.exact_bytes[4] = std::byte{7};
    for (std::size_t index = 2U; index < 5U; ++index) {
      store_u16(tower.people[index].exact_bytes, 10U, 0x1234U, false);
      store_u16(tower.people[index].exact_bytes, 12U, 0xc064U, false);
    }

    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.capacity = 2U;
    elevator.bottom_floor = 10;
    elevator.top_floor = 20;
    elevator.serviced_floors[10] = std::byte{1};
    auto& car = elevator.car_records[0].exact_bytes;
    car[0] = std::byte{10};
    car[3] = std::byte{2};
    car[12] = std::byte{1};
    car[15] = std::byte{1};
    car[226U + 15U] = std::byte{2};
    store_u32(car, 16U, 0U, false);
    store_u32(car, 20U, 1U, false);
    car[184U] = std::byte{15};
    car[185U] = std::byte{15};
    store_u16(car, 10U, 2U, false);
    elevator.block_2a2[15] = std::byte{1};
    elevator.block_31a[15] = std::byte{1};
    elevator.car_home_floors[0] = std::byte{10};

    simtower::OriginalTdtElevatorFloorRecord record{};
    record.floor = 15;
    record.mapped_index = simtower::original_elevator_floor_record_index(
        elevator.type, elevator.bottom_floor, elevator.top_floor, 15);
    record.exact_bytes[0] = std::byte{2};
    record.exact_bytes[1] = std::byte{39};
    store_u32(record.exact_bytes, 4U + 39U * 4U, 2U, false);
    store_u32(record.exact_bytes, 4U, 3U, false);
    record.exact_bytes[2] = std::byte{1};
    record.exact_bytes[3] = std::byte{5};
    store_u32(record.exact_bytes, 164U + 5U * 4U, 4U, false);
    elevator.floor_records.push_back(record);

    const auto before = simtower::serialize_original_tdt(tower);
    auto result = simtower::cleanup_original_elevator_service_floor_people(
        tower, 0U, 15, 250U, nullptr);
    assert(result.status == simtower::
               OriginalElevatorFloorPeopleCleanupStatus::dispatch_required);
    assert(simtower::serialize_original_tdt(tower) == before);

    ElevatorCleanupTrace trace{};
    result = simtower::cleanup_original_elevator_service_floor_people(
        tower, 0U, 15, 250U, trace_elevator_cleanup_dispatch, &trace);
    assert(result.status == simtower::
               OriginalElevatorFloorPeopleCleanupStatus::cleaned);
    assert(result.car_passengers == 2U);
    assert(result.waiting_passengers == 3U);
    assert(trace.count == 5U);
    assert((trace.people ==
            std::array<std::size_t, 8>{0U, 1U, 2U, 3U, 4U, 0U, 0U, 0U}));
    assert(car[3] == std::byte{0});
    assert(car[12] == std::byte{0});
    assert(car[226U + 15U] == std::byte{0});
    assert(load_u32(car, 16U, false) == 0xffffffffU);
    assert(load_u32(car, 20U, false) == 0xffffffffU);
    assert(elevator.block_2a2[15] == std::byte{0});
    assert(elevator.block_31a[15] == std::byte{0});
    assert(load_u16(car, 10U, false) == 0U);
    const auto& drained = elevator.floor_records[0].exact_bytes;
    assert(drained[0] == std::byte{0} && drained[1] == std::byte{1});
    assert(drained[2] == std::byte{0} && drained[3] == std::byte{6});
    assert(load_u32(drained, 4U + 39U * 4U, false) == 2U);
    assert(load_u32(drained, 164U + 5U * 4U, false) == 4U);
  }

  {
    // Service-elevator type two skips 11d8:02f7 while retaining the same
    // waiting-ring callback/count order; dword access follows swapped TDTs.
    auto tower = simtower::make_original_new_tdt();
    tower.header.byte_swapped = true;
    tower.people_count = 1U;
    tower.people.resize(1U);
    store_u16(tower.people[0].exact_bytes, 10U, 0x1234U, true);
    store_u16(tower.people[0].exact_bytes, 12U, 0xc064U, true);
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 2U;
    elevator.bottom_floor = 10;
    elevator.top_floor = 10;
    simtower::OriginalTdtElevatorFloorRecord record{};
    record.floor = 10;
    record.mapped_index = simtower::original_elevator_floor_record_index(
        elevator.type, elevator.bottom_floor, elevator.top_floor, 10);
    record.exact_bytes[0] = std::byte{1};
    store_u32(record.exact_bytes, 4U, 0U, true);
    elevator.floor_records.push_back(record);
    ElevatorCleanupTrace trace{};
    trace.skip_order_assertions = true;
    const auto result =
        simtower::cleanup_original_elevator_service_floor_people(
            tower, 0U, 10, 250U, trace_elevator_cleanup_dispatch, &trace);
    assert(result.status == simtower::
               OriginalElevatorFloorPeopleCleanupStatus::cleaned);
    assert(load_u16(tower.people[0].exact_bytes, 10U, true) == 0x1234U);
    assert(load_u16(tower.people[0].exact_bytes, 12U, true) == 0xc064U);
  }

  {
    // The self-contained cleanup uses 0883 for car arrivals and 16ab for
    // waiting rings. Their tables differ only for Security: the car passenger
    // calls 67cf, while the queued type-14 record is deliberately a no-op.
    auto tower = simtower::make_original_new_tdt();
    tower.people_count = 2U;
    tower.people.resize(2U);
    for (auto& person : tower.people) {
      person.exact_bytes[4] = std::byte{14};
      person.exact_bytes[5] = std::byte{0};
      person.exact_bytes[7] = std::byte{10};
    }
    tower.people[0].exact_bytes[5] = std::byte{1};
    tower.people[0].exact_bytes[7] = std::byte{27};
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.capacity = 1U;
    elevator.bottom_floor = 10;
    elevator.top_floor = 10;
    auto& car = elevator.car_records[0].exact_bytes;
    car[0] = std::byte{10};
    car[3] = std::byte{1};
    car[12] = std::byte{1};
    car[15] = std::byte{1};
    car[184U] = std::byte{10};
    car[226U + 10U] = std::byte{1};
    store_u32(car, 16U, 0U, false);
    simtower::OriginalTdtElevatorFloorRecord record{};
    record.floor = 10;
    record.mapped_index = simtower::original_elevator_floor_record_index(
        elevator.type, elevator.bottom_floor, elevator.top_floor, 10);
    record.exact_bytes[0] = std::byte{1};
    store_u32(record.exact_bytes, 4U, 1U, false);
    elevator.floor_records.push_back(record);

    simtower::OriginalPartTable part{};
    const auto result =
        simtower::cleanup_original_elevator_service_floor_people(
            tower, 0U, 10, 0U, part);
    assert(result.cleanup.status == simtower::
               OriginalElevatorFloorPeopleCleanupStatus::cleaned);
    assert(result.cleanup.car_passengers == 1U &&
           result.cleanup.waiting_passengers == 1U);
    assert(result.family_dispatches.size() == 2U);
    assert(result.family_dispatches[0].source == simtower::
               OriginalPersonFamilyDispatchSource::elevator_car_0883);
    assert(result.family_dispatches[0].status == simtower::
               OriginalPersonFamilyDispatchStatus::security);
    assert(result.family_dispatches[1].source == simtower::
               OriginalPersonFamilyDispatchSource::dispatcher_16ab);
    assert(result.family_dispatches[1].status == simtower::
               OriginalPersonFamilyDispatchStatus::no_handler);
    assert(car[3] == std::byte{0} && car[12] == std::byte{0});
    assert(car[226U + 10U] == std::byte{0});
    assert(elevator.floor_records[0].exact_bytes[0] == std::byte{0});
    assert(tower.people[0].exact_bytes[7] == std::byte{27});
  }

  {
    // 1090:0dfc immediately accepts a stopped car at the requested floor.
    simtower::OriginalTdtElevator elevator{};
    elevator.schedule.fill(std::byte{5});
    elevator.car_home_floors.fill(std::byte{0});
    auto& immediate = elevator.car_records[2].exact_bytes;
    immediate[0] = std::byte{10};
    immediate[1] = std::byte{0};
    immediate[4] = std::byte{0};
    immediate[14] = std::byte{1};
    immediate[15] = std::byte{1};
    auto selected = simtower::select_original_elevator_assignment_car(
        elevator, 10, true, 0U, 0U);
    assert(selected.status == simtower::
               OriginalElevatorAssignmentSelectionStatus::immediate_service);
    assert(selected.car_index == 2U);

    // Mode zero still takes the immediate path when its direction matches.
    immediate[14] = std::byte{0};
    immediate[4] = std::byte{1};
    selected = simtower::select_original_elevator_assignment_car(
        elevator, 10, true, 0U, 0U);
    assert(selected.status == simtower::
               OriginalElevatorAssignmentSelectionStatus::immediate_service);
    assert(selected.car_index == 2U);
  }

  {
    // The schedule byte arbitrates between a nearby idle car and a farther
    // same-direction car. Equal distances retain the first scanned car.
    simtower::OriginalTdtElevator elevator{};
    elevator.schedule.fill(std::byte{5});
    auto& idle = elevator.car_records[0].exact_bytes;
    idle[0] = std::byte{8};
    idle[1] = std::byte{0};
    idle[15] = std::byte{1};
    elevator.car_home_floors[0] = std::byte{8};

    auto& same = elevator.car_records[1].exact_bytes;
    same[0] = std::byte{0};
    same[4] = std::byte{1};
    same[10] = std::byte{1};
    same[15] = std::byte{1};

    auto selected = simtower::select_original_elevator_assignment_car(
        elevator, 10, true, 0U, 0U);
    assert(selected.status ==
           simtower::OriginalElevatorAssignmentSelectionStatus::assign_car);
    assert(selected.car_index == 0U);
    elevator.schedule[14] = std::byte{9};
    selected = simtower::select_original_elevator_assignment_car(
        elevator, 10, true, 0U, 0U);
    assert(selected.car_index == 1U);

    // The opposite-direction wrap formula uses byte 13's pivot floor.
    elevator = {};
    elevator.schedule.fill(std::byte{5});
    auto& wrap = elevator.car_records[2].exact_bytes;
    wrap[0] = std::byte{20};
    wrap[4] = std::byte{0};
    wrap[10] = std::byte{1};
    wrap[13] = std::byte{5};
    wrap[15] = std::byte{1};
    selected = simtower::select_original_elevator_assignment_car(
        elevator, 10, true, 0U, 0U);
    assert(selected.car_index == 2U);

    // Preserve 1090:10d0's literal car-zero fallback and reject malformed
    // clock indices before touching the 56-byte schedule.
    elevator = {};
    elevator.schedule.fill(std::byte{5});
    auto& only_idle = elevator.car_records[3].exact_bytes;
    only_idle[0] = std::byte{1};
    only_idle[15] = std::byte{1};
    elevator.car_home_floors[3] = std::byte{1};
    selected = simtower::select_original_elevator_assignment_car(
        elevator, 10, true, 0U, 0U);
    assert(selected.car_index == 0U);
    selected = simtower::select_original_elevator_assignment_car(
        elevator, 10, true, 2U, 0U);
    assert(selected.status ==
           simtower::OriginalElevatorAssignmentSelectionStatus::invalid);
  }

  {
    // 1090:0a4c stores the one-based owner, increments word 10, and leaves
    // an existing owner untouched before running any selector logic.
    auto tower = simtower::make_original_new_tdt();
    auto& elevator = tower.elevators[0];
    elevator.bottom_floor = 0;
    elevator.top_floor = 10;
    elevator.schedule.fill(std::byte{5});
    auto& car = elevator.car_records[0].exact_bytes;
    car[0] = std::byte{0};
    car[4] = std::byte{1};
    car[10] = std::byte{1};
    car[15] = std::byte{1};
    assert(simtower::assign_original_elevator_waiting_floor(
        tower, 0U, 5, true, 0U, 0U));
    assert(elevator.block_2a2[5] == std::byte{1});
    assert(load_u16(car, 10U, false) == 2U);
    assert(!simtower::assign_original_elevator_waiting_floor(
        tower, 0U, 5, true, 0U, 0U));
    assert(load_u16(car, 10U, false) == 2U);

    // A stopped car consumes the call immediately and creates no owner.
    auto& immediate = elevator.car_records[1].exact_bytes;
    immediate[0] = std::byte{6};
    immediate[1] = std::byte{0};
    immediate[14] = std::byte{1};
    immediate[15] = std::byte{1};
    assert(!simtower::assign_original_elevator_waiting_floor(
        tower, 0U, 6, false, 0U, 0U));
    assert(elevator.block_31a[6] == std::byte{0});
  }

  {
    // Assignment-count arithmetic follows a byte-swapped TDT revision.
    auto tower = simtower::make_original_new_tdt();
    tower.header.byte_swapped = true;
    auto& elevator = tower.elevators[0];
    elevator.bottom_floor = 0;
    elevator.top_floor = 10;
    elevator.schedule.fill(std::byte{5});
    auto& car = elevator.car_records[0].exact_bytes;
    car[0] = std::byte{8};
    car[4] = std::byte{0};
    car[15] = std::byte{1};
    store_u16(car, 10U, 1U, true);
    assert(simtower::assign_original_elevator_waiting_floor(
        tower, 0U, 4, false, 0U, 0U));
    assert(elevator.block_31a[4] == std::byte{1});
    assert(load_u16(car, 10U, true) == 2U);
  }

  {
    // Exact 1090:1553 primary-target branches through 0bcf: idle cars use
    // their home floor, ordinary upward cars take the first up assignment,
    // full cars ignore waiting calls but retain passenger destinations, and
    // express modes force their opposite endpoint before scanning.
    auto tower = simtower::make_original_new_tdt();
    auto& elevator = tower.elevators[0];
    elevator.bottom_floor = 2;
    elevator.top_floor = 8;
    elevator.capacity = 2U;
    elevator.car_home_floors[0] = std::byte{4};
    auto& car = elevator.car_records[0].exact_bytes;
    car[0] = std::byte{5};
    car[4] = std::byte{1};
    elevator.block_2a2[7] = std::byte{1};

    simtower::recompute_original_elevator_car_state(tower, 0U, 0U);
    assert(car[5] == std::byte{4});

    store_u16(car, 10U, 1U, false);
    simtower::recompute_original_elevator_car_state(tower, 0U, 0U);
    assert(car[5] == std::byte{7});

    car[3] = std::byte{2};
    car[226U + 6U] = std::byte{1};
    simtower::recompute_original_elevator_car_state(tower, 0U, 0U);
    assert(car[5] == std::byte{6});

    car[3] = std::byte{0};
    car[226U + 6U] = std::byte{0};
    car[14] = std::byte{1};
    car[0] = std::byte{5};
    car[4] = std::byte{1};
    simtower::recompute_original_elevator_car_state(tower, 0U, 0U);
    assert(car[5] == std::byte{8});

    car[14] = std::byte{2};
    car[4] = std::byte{0};
    simtower::recompute_original_elevator_car_state(tower, 0U, 0U);
    assert(car[5] == std::byte{2});
  }

  {
    // Direct 1090:1f4c coverage through the production recompute host. The
    // secondary scan runs from top downward for an upward car and bottom
    // upward for a downward car, independently of the nearer primary target.
    auto tower = simtower::make_original_new_tdt();
    auto& elevator = tower.elevators[0];
    elevator.bottom_floor = 2;
    elevator.top_floor = 8;
    elevator.capacity = 8U;
    elevator.car_home_floors[0] = std::byte{4};
    auto& car = elevator.car_records[0].exact_bytes;
    car[0] = std::byte{5};
    car[4] = std::byte{1};
    store_u16(car, 10U, 1U, false);
    car[226U + 6U] = std::byte{1};
    elevator.block_2a2[8] = std::byte{1};
    simtower::recompute_original_elevator_car_state(tower, 0U, 0U);
    assert(car[5] == std::byte{6});
    assert(car[13] == std::byte{8});

    car[4] = std::byte{0};
    car[226U + 6U] = std::byte{0};
    elevator.block_2a2[8] = std::byte{0};
    car[226U + 4U] = std::byte{1};
    elevator.block_31a[2] = std::byte{1};
    simtower::recompute_original_elevator_car_state(tower, 0U, 0U);
    assert(car[5] == std::byte{4});
    assert(car[13] == std::byte{2});

    store_u16(car, 10U, 0U, false);
    car[12] = std::byte{0};
    car[226U + 4U] = std::byte{0};
    elevator.block_31a[2] = std::byte{0};
    simtower::recompute_original_elevator_car_state(tower, 0U, 0U);
    assert(car[13] == std::byte{4});
  }

  {
    // Direct 1090:1d2f/13cc direction-arbitration and assignment-release
    // coverage. At-target cars reverse at each endpoint or prefer the sole
    // opposite-direction lane request; each change releases owners using the
    // new direction, with nonzero mode releasing both lanes.
    auto tower = simtower::make_original_new_tdt();
    auto& elevator = tower.elevators[0];
    elevator.bottom_floor = 2;
    elevator.top_floor = 8;
    elevator.capacity = 8U;
    auto& car = elevator.car_records[0].exact_bytes;
    car[7] = std::byte{1};

    elevator.car_home_floors[0] = std::byte{8};
    car[0] = std::byte{8};
    car[4] = std::byte{1};
    elevator.block_31a[8] = std::byte{1};
    store_u16(car, 10U, 1U, false);
    simtower::recompute_original_elevator_car_state(tower, 0U, 0U);
    assert(car[5] == std::byte{8} && car[4] == std::byte{0});
    assert(elevator.block_31a[8] == std::byte{0});
    assert(load_u16(car, 10U, false) == 0U);

    elevator.car_home_floors[0] = std::byte{2};
    car[0] = std::byte{2};
    car[4] = std::byte{0};
    elevator.block_2a2[2] = std::byte{1};
    store_u16(car, 10U, 1U, false);
    simtower::recompute_original_elevator_car_state(tower, 0U, 0U);
    assert(car[5] == std::byte{2} && car[4] == std::byte{1});
    assert(elevator.block_2a2[2] == std::byte{0});
    assert(load_u16(car, 10U, false) == 0U);

    elevator.car_home_floors[0] = std::byte{5};
    car[0] = std::byte{5};
    car[4] = std::byte{1};
    elevator.block_31a[5] = std::byte{1};
    store_u16(car, 10U, 1U, false);
    simtower::recompute_original_elevator_car_state(tower, 0U, 0U);
    assert(car[5] == std::byte{5} && car[4] == std::byte{0});
    assert(elevator.block_31a[5] == std::byte{0});
    assert(load_u16(car, 10U, false) == 0U);

    auto& other_car = elevator.car_records[1].exact_bytes;
    elevator.car_home_floors[1] = std::byte{4};
    other_car[0] = std::byte{4};
    store_u16(other_car, 10U, 1U, false);
    elevator.car_home_floors[0] = std::byte{8};
    car[0] = std::byte{8};
    car[4] = std::byte{1};
    car[14] = std::byte{1};
    elevator.block_2a2[8] = std::byte{2};
    elevator.block_31a[8] = std::byte{1};
    store_u16(car, 10U, 1U, false);
    simtower::recompute_original_elevator_car_state(tower, 0U, 0U);
    assert(car[4] == std::byte{0});
    assert(elevator.block_2a2[8] == std::byte{0});
    assert(elevator.block_31a[8] == std::byte{0});
    assert(load_u16(car, 10U, false) == 0U);
    assert(load_u16(other_car, 10U, false) == 0U);
  }

  {
    // Exact type/state/index filters before the family calls in 1218:0000.
    auto tower = simtower::make_original_new_tdt();
    tower.people_count = 19U;
    tower.people.resize(19U);
    constexpr std::array<std::int8_t, 19> types = {
        3, 4, 5, 6, 7, 9, 10, 12, 18, 29, 33, 36, 15,
        14, 19, 30, 34, 37, 40};
    for (std::size_t index = 0; index < types.size(); ++index) {
      auto& exact = tower.people[index].exact_bytes;
      exact[4] = static_cast<std::byte>(types[index]);
      exact[5] = std::byte{0x40};
      exact[8] = std::byte{17};
    }
    assert(simtower::count_original_vertical_transport_cleanup_people(
               tower, 17U) == 13U);

    // The family comparisons are signed: 0x80 is below 0x40, byte-8 0xff
    // is -1, and only Housekeeping accepts states 3..0x3f.
    tower.people[0].exact_bytes[5] = std::byte{0x80};
    tower.people[1].exact_bytes[8] = std::byte{0xff};
    tower.people[12].exact_bytes[5] = std::byte{3};
    assert(simtower::count_original_vertical_transport_cleanup_people(
               tower, 17U) == 11U);
    tower.people[12].exact_bytes[5] = std::byte{2};
    assert(simtower::count_original_vertical_transport_cleanup_people(
               tower, 17U) == 10U);
    assert(simtower::count_original_vertical_transport_cleanup_people(
               tower, 64U) == 0U);

    // The complete 1218 pass calls those ten surviving records in person-
    // table order through the raw family callbacks while the Stair record is
    // still live. Its source tag remains distinct from 16ab and 0883.
    auto& stair = tower.post_elevator.stairs_bd70[17U];
    stair.used = 1U;
    stair.floor = 10;
    stair.word_6 = 20U;
    stair.word_8 = 20U;
    store_u16(stair.exact_bytes, 6U, 20U, false);
    store_u16(stair.exact_bytes, 8U, 20U, false);
    simtower::OriginalPartTable cleanup_part{};
    const auto cleanup = simtower::cleanup_original_vertical_transport_people(
        tower, 17U, cleanup_part);
    assert(cleanup.valid_transport_index);
    assert(cleanup.family_dispatches.size() == 10U);
    for (std::size_t index = 0U;
         index < cleanup.family_dispatches.size(); ++index) {
      assert(cleanup.family_dispatches[index].person_index == index + 2U);
      assert(cleanup.family_dispatches[index].source == simtower::
                 OriginalPersonFamilyDispatchSource::vertical_transport_1218);
    }
    assert(!simtower::cleanup_original_vertical_transport_people(
                tower, 64U, cleanup_part).valid_transport_index);

    // 1218 compares only through the persisted count even if a malformed
    // in-memory table is longer.
    tower.people_count = 1U;
    assert(simtower::count_original_vertical_transport_cleanup_people(
               tower, 17U) == 0U);
  }

  {
    // Direct 1220:6bef/6cb6 coverage. Entry wraps eight to one and otherwise
    // increments with byte width. Exit decrements with the same wrap and maps
    // an exact zero to eight only at day phase four or later; every branch
    // synchronizes the serialized status and dirty bytes.
    auto tower = simtower::make_original_new_tdt();
    simtower::OriginalTdtTenant owner{};
    owner.status = 8U;
    owner.exact_bytes[5] = std::byte{8};
    simtower::enter_original_office(owner);
    assert(owner.status == 1U && owner.exact_bytes[5] == std::byte{1});
    assert(owner.exact_bytes[13] == std::byte{1} &&
           owner.preserved_07_to_0f[6] == std::byte{1});

    owner.status = 0xffU;
    owner.exact_bytes[5] = std::byte{0xff};
    simtower::enter_original_office(owner);
    assert(owner.status == 0U && owner.exact_bytes[5] == std::byte{0});

    tower.header.frame_time = 0U;
    owner.status = 1U;
    owner.exact_bytes[5] = std::byte{1};
    simtower::leave_original_office(tower, owner);
    assert(owner.status == 0U && owner.exact_bytes[5] == std::byte{0});

    tower.header.frame_time = 1600U;
    owner.status = 1U;
    owner.exact_bytes[5] = std::byte{1};
    simtower::leave_original_office(tower, owner);
    assert(owner.status == 8U && owner.exact_bytes[5] == std::byte{8});
    owner.status = 0U;
    owner.exact_bytes[5] = std::byte{0};
    simtower::leave_original_office(tower, owner);
    assert(owner.status == 0xffU && owner.exact_bytes[5] == std::byte{0xff});
  }

  {
    // Direct 1198:00a9 coverage: Office departure decrements category zero and
    // its total with the same wrapping accounting transaction.
    auto tower = make_active_office();
    auto& tenant = tower.floors[11].tenants[0];
    auto& person = tower.people[0].exact_bytes;
    tenant.status = 8;
    tenant.exact_bytes[5] = std::byte{8};
    person[5] = std::byte{0x40};
    person[9] = std::byte{0xff};
    store_u16(person, 10, 20, false);
    store_u16(person, 12, 0x8064, false);
    store_u16(person, 14, 1000, false);

    assert(simtower::step_original_office_person(tower, 0, 50) ==
           simtower::OriginalOfficePersonStepStatus::entered_office);
    assert(person[5] == std::byte{5});
    assert(person[9] == std::byte{0});
    assert(load_u16(person, 10, false) == 0U);
    assert(load_u16(person, 12, false) == 0x8000U);
    assert(load_u16(person, 14, false) == 1130U);
    assert(tenant.status == 1U);
    assert(tenant.exact_bytes[5] == std::byte{1});
    assert(tenant.exact_bytes[13] == std::byte{1});
    assert(tenant.preserved_07_to_0f[6] == std::byte{1});
  }

  {
    auto tower = make_active_office();
    auto& tenant = tower.floors[11].tenants[0];
    auto& person = tower.people[2].exact_bytes;
    person[5] = std::byte{0x45};
    person[13] = std::byte{0xfc};
    store_u16(person, 12, 0xc055, false);
    tenant.status = 6;
    tenant.exact_bytes[5] = std::byte{6};
    tower.post_elevator.b846_series[0][0] = 10;
    tower.post_elevator.b846_series[0][10] = 20;
    tower.post_elevator.parking_connected = 1;
    auto& route = tower.post_elevator.cf9c_records[0];
    route[0] = person[0];
    route[1] = person[1];
    store_u32(route, 2, 2, false);

    assert(simtower::step_original_office_person(tower, 2, 0) ==
           simtower::OriginalOfficePersonStepStatus::left_office);
    assert(person[5] == std::byte{0x26});
    assert(load_u16(person, 12, false) == 0U);
    assert(tower.post_elevator.b846_series[0][0] == 9);
    assert(tower.post_elevator.b846_series[0][10] == 19);
    assert(tenant.status == 0U);
    assert(tenant.exact_bytes[13] == std::byte{1});
    assert(route[2] == std::byte{0} && route[3] == std::byte{0} &&
           route[4] == std::byte{0} && route[5] == std::byte{0});
  }

  {
    auto tower = make_active_office();
    auto& person = tower.people[4].exact_bytes;
    person[5] = std::byte{0x60};
    assert(simtower::step_original_office_person(tower, 4, 0) ==
           simtower::OriginalOfficePersonStepStatus::left_office);
    assert(person[5] == std::byte{0x26});
    assert(tower.post_elevator.b846_series[0][0] == 0);
  }

  {
    // 1220:1aed applies the common metric pair before Condo's five-key
    // arrival table, then executes 7005 and stores state four.
    auto tower = make_active_office();
    auto& owner = tower.floors[11].tenants[0];
    owner.type = 9;
    owner.exact_bytes[4] = std::byte{9};
    owner.status = 0U;
    owner.exact_bytes[5] = std::byte{0};
    auto& person = tower.people[0].exact_bytes;
    person[4] = std::byte{9};
    person[5] = std::byte{0x60};
    person[9] = std::byte{0};
    store_u16(person, 10U, 100U, false);
    store_u16(person, 12U, 0x8064U, false);
    store_u16(person, 14U, 10U, false);
    simtower::OriginalPartTable part{};
    const auto dispatch = simtower::dispatch_original_transit_person(
        tower, 0U, 120U, part);
    assert(dispatch.status ==
           simtower::OriginalTransitPersonDispatchStatus::condo_arrived);
    assert(dispatch.common_update_applied && dispatch.owner_status_changed &&
           dispatch.changed);
    assert(person[5] == std::byte{4} && person[9] == std::byte{1});
    assert(load_u16(person, 10U, false) == 0U);
    assert(load_u16(person, 12U, false) == 0x8000U);
    assert(load_u16(person, 14U, false) == 130U);
    assert(owner.status == 1U && owner.exact_bytes[13] == std::byte{1});
  }

  {
    // 1aed's commercial branch stores state 27 and calls 11a8:1197 using
    // the owner type and serialized +6 service word after metric finalizing.
    auto tower = make_food_service_tower();
    auto& person = tower->people[0].exact_bytes;
    person[5] = std::byte{0x60};
    person[9] = std::byte{1};
    store_u16(person, 10U, 100U, false);
    store_u16(person, 12U, 0U, false);
    store_u16(person, 14U, 200U, false);
    simtower::OriginalPartTable part{};
    part.words_00_to_40[5U] = 50U;
    part.words_00_to_40[8U] = 100U;
    const auto dispatch = simtower::dispatch_original_transit_person(
        *tower, 0U, 120U, part);
    assert(dispatch.status == simtower::
               OriginalTransitPersonDispatchStatus::commercial_completed);
    assert(dispatch.common_update_applied && dispatch.service_index == 0);
    assert(person[5] == std::byte{0x27});
  }

  {
    // Direct 1210:1d56 coverage: byte 8 code 0x40 selects Elevator zero's
    // first lane. 1210:1b41 pops every up-ring entry ahead of the target,
    // advances the
    // cursor/count before each 1aed call, and leaves stale slot dwords. Two
    // Office arrivals therefore advance status 8 -> 1 -> 2 in queue order.
    auto tower = make_active_office();
    tower.header.frame_time = 120U;
    auto& owner = tower.floors[11].tenants[0];
    owner.status = 8U;
    owner.exact_bytes[5] = std::byte{8};
    for (const std::size_t index : {0U, 1U}) {
      auto& person = tower.people[index].exact_bytes;
      person[5] = std::byte{0x40};
      person[7] = std::byte{11};
      person[8] = std::byte{0x40};
      store_u16(person, 10U, 100U, false);
    }
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.bottom_floor = 11;
    elevator.top_floor = 11;
    elevator.serviced_floors[11] = std::byte{1};
    simtower::OriginalTdtElevatorFloorRecord record{};
    record.floor = 11;
    record.mapped_index = simtower::original_elevator_floor_record_index(
        elevator.type, elevator.bottom_floor, elevator.top_floor, 11);
    record.exact_bytes[0] = std::byte{2};
    record.exact_bytes[1] = std::byte{39};
    store_u32(record.exact_bytes, 4U + 39U * 4U, 1U, false);
    store_u32(record.exact_bytes, 4U, 0U, false);
    elevator.floor_records.push_back(record);
    simtower::OriginalPartTable part{};
    part.words_00_to_40[0U] = 10U;

    const auto timeout = simtower::step_original_elevator_wait_timeout(
        tower, 0U, part);
    assert(timeout.status ==
           simtower::OriginalElevatorWaitTimeoutStatus::dispatched);
    assert(timeout.dispatch.status == simtower::
               OriginalElevatorWaitingDispatchStatus::ring_dispatched);
    assert((timeout.dispatch.person_indices ==
            std::vector<std::size_t>{1U, 0U}));
    assert(timeout.dispatch.view_slot_restore_requested &&
           timeout.dispatch.view_floor == 11);
    assert(timeout.dispatch.dispatches.size() == 2U);
    assert(timeout.dispatch.dispatches[0].status == simtower::
               OriginalTransitPersonDispatchStatus::office_arrived);
    assert(timeout.dispatch.dispatches[1].status == simtower::
               OriginalTransitPersonDispatchStatus::office_arrived);
    const auto& queue = elevator.floor_records[0].exact_bytes;
    assert(queue[0] == std::byte{0} && queue[1] == std::byte{1});
    assert(load_u32(queue, 4U + 39U * 4U, false) == 1U);
    assert(load_u32(queue, 4U, false) == 0U);
    assert(tower.people[1].exact_bytes[5] == std::byte{5});
    assert(tower.people[0].exact_bytes[5] == std::byte{5});
    assert(owner.status == 2U);
  }

  {
    // 1220:1637 uses signed 16-bit elapsed/threshold comparison. Before the
    // threshold it mutates neither the ring nor the person; a missing target
    // is rejected atomically instead of reproducing the original's unbounded
    // pointer loop on malformed native input.
    auto tower = make_active_office();
    tower.header.frame_time = 120U;
    auto& person = tower.people[0].exact_bytes;
    person[5] = std::byte{0x40};
    person[7] = std::byte{11};
    person[8] = std::byte{0x40};
    store_u16(person, 10U, 100U, false);
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.bottom_floor = 11;
    elevator.top_floor = 11;
    simtower::OriginalTdtElevatorFloorRecord record{};
    record.floor = 11;
    record.mapped_index = simtower::original_elevator_floor_record_index(
        elevator.type, elevator.bottom_floor, elevator.top_floor, 11);
    record.exact_bytes[0] = std::byte{1};
    store_u32(record.exact_bytes, 4U, 0U, false);
    elevator.floor_records.push_back(record);
    simtower::OriginalPartTable part{};
    part.words_00_to_40[0U] = 30U;
    const auto before_person = person;
    const auto before_queue = elevator.floor_records[0].exact_bytes;
    auto timeout = simtower::step_original_elevator_wait_timeout(
        tower, 0U, part);
    assert(timeout.status ==
           simtower::OriginalElevatorWaitTimeoutStatus::pending);
    assert(person == before_person &&
           elevator.floor_records[0].exact_bytes == before_queue);

    part.words_00_to_40[0U] = 10U;
    store_u32(elevator.floor_records[0].exact_bytes, 4U, 1U, false);
    const auto malformed_queue = elevator.floor_records[0].exact_bytes;
    timeout = simtower::step_original_elevator_wait_timeout(
        tower, 0U, part);
    assert(timeout.status ==
           simtower::OriginalElevatorWaitTimeoutStatus::malformed_queue);
    assert(person == before_person &&
           elevator.floor_records[0].exact_bytes == malformed_queue);
  }

  {
    // Direct 1210:1d56 second-lane boundary: 0x58 subtracts 0x40 and then 24
    // to select Elevator zero. The down ring and dwords retain opposite byte
    // order; Housekeeping skips 11d8 and 1aed writes state zero/floor ff.
    auto tower = simtower::make_original_new_tdt();
    tower.header.byte_swapped = true;
    tower.header.frame_time = 120U;
    tower.people_count = 2U;
    tower.people.resize(2U);
    for (auto& record : tower.people) {
      auto& person = record.exact_bytes;
      person[4] = std::byte{15};
      person[5] = std::byte{3};
      person[7] = std::byte{10};
      person[8] = std::byte{0x58};
      store_u16(person, 10U, 100U, true);
    }
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.bottom_floor = 10;
    elevator.top_floor = 10;
    simtower::OriginalTdtElevatorFloorRecord record{};
    record.floor = 10;
    record.mapped_index = simtower::original_elevator_floor_record_index(
        elevator.type, elevator.bottom_floor, elevator.top_floor, 10);
    record.exact_bytes[2] = std::byte{2};
    record.exact_bytes[3] = std::byte{39};
    store_u32(record.exact_bytes, 164U + 39U * 4U, 1U, true);
    store_u32(record.exact_bytes, 164U, 0U, true);
    elevator.floor_records.push_back(record);
    simtower::OriginalPartTable part{};
    part.words_00_to_40[0U] = 10U;
    const auto timeout = simtower::step_original_elevator_wait_timeout(
        tower, 0U, part);
    assert(timeout.status ==
           simtower::OriginalElevatorWaitTimeoutStatus::dispatched);
    assert((timeout.dispatch.person_indices ==
            std::vector<std::size_t>{1U, 0U}));
    const auto& queue = elevator.floor_records[0].exact_bytes;
    assert(queue[2] == std::byte{0} && queue[3] == std::byte{1});
    for (const auto& record : tower.people) {
      assert(record.exact_bytes[5] == std::byte{0});
      assert(record.exact_bytes[7] == std::byte{0xff});
      assert(load_u16(record.exact_bytes, 10U, true) == 100U);
    }
  }

  {
    // The normal 0daf wrapper now invokes the complete 1637/1b41/1aed path
    // instead of merely counting a deferred Elevator timeout.
    auto tower = make_active_office();
    tower.header.frame_time = 128U;
    auto& owner = tower.floors[11].tenants[0];
    owner.status = 8U;
    owner.exact_bytes[5] = std::byte{8};
    auto& person = tower.people[0].exact_bytes;
    person[5] = std::byte{0x40};
    person[7] = std::byte{11};
    person[8] = std::byte{0x40};
    store_u16(person, 10U, 100U, false);
    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.bottom_floor = 11;
    elevator.top_floor = 11;
    simtower::OriginalTdtElevatorFloorRecord record{};
    record.floor = 11;
    record.mapped_index = simtower::original_elevator_floor_record_index(
        elevator.type, elevator.bottom_floor, elevator.top_floor, 11);
    record.exact_bytes[0] = std::byte{1};
    store_u32(record.exact_bytes, 4U, 0U, false);
    elevator.floor_records.push_back(record);
    simtower::OriginalPartTable part{};
    part.words_00_to_40[0U] = 20U;
    const auto pass =
        simtower::step_original_translated_people(tower, part);
    assert(pass.elevator_timeout_checks == 1U);
    assert(pass.elevator_timeouts_triggered == 1U);
    assert(pass.elevator_transit_people_dispatched == 1U);
    assert(pass.elevator_timeout_view_requests == 1U);
    assert(pass.dispatched == 1U && pass.changed == 1U);
    assert(pass.office_normal_dispatched == 0U);
    assert(person[5] == std::byte{5} && owner.status == 1U);
    assert(elevator.floor_records[0].exact_bytes[0] == std::byte{0});
  }

  {
    auto tower = make_active_office();
    tower.people[0].exact_bytes[5] = std::byte{0x40};
    tower.people[0].exact_bytes[7] = std::byte{9};
    tower.people[0].exact_bytes[8] = std::byte{10};
    store_u16(tower.people[0].exact_bytes, 12, 0xffff, false);
    tower.people[6].exact_bytes[4] = std::byte{3};
    tower.people[6].exact_bytes[5] = std::byte{0x77};
    assert(simtower::reset_original_office_people_for_day(tower) == 6U);
    assert(tower.people[0].exact_bytes[5] == std::byte{0x20});
    assert(tower.people[0].exact_bytes[7] == std::byte{0});
    assert(tower.people[0].exact_bytes[8] == std::byte{0});
    assert(load_u16(tower.people[0].exact_bytes, 12, false) == 0U);
    assert(tower.people[6].exact_bytes[5] == std::byte{0x77});
  }

  {
    // Direct 1220:0000 coverage: exercise every live jump-table family, both
    // short/full clearing tails, and 0140/01c3's signed tenant-status branch.
    auto tower = simtower::make_original_new_tdt();
    auto& floor = tower.floors[11];
    floor.tenants.clear();
    floor.tenant_index.fill(0U);
    const auto add_tenant = [&](std::uint8_t key, std::int8_t type,
                                std::uint8_t status) {
      simtower::OriginalTdtTenant tenant{};
      tenant.type = type;
      tenant.status = status;
      tenant.exact_bytes[4] = static_cast<std::byte>(type);
      tenant.exact_bytes[5] = static_cast<std::byte>(status);
      tenant.exact_bytes[12] = static_cast<std::byte>(key);
      floor.tenant_index[key] =
          static_cast<std::uint16_t>(floor.tenants.size());
      floor.tenants.push_back(tenant);
    };
    add_tenant(0U, 3, 0x17U);
    add_tenant(1U, 3, 0x18U);
    add_tenant(2U, 9, 0x17U);
    add_tenant(3U, 9, 0x18U);
    add_tenant(4U, 3, 0x80U);
    add_tenant(5U, 9, 0xffU);

    const auto initialize = [&](std::size_t index, std::uint8_t type,
                                std::uint8_t key, std::uint16_t ordinal) {
      auto& exact = tower.people[index].exact_bytes;
      exact.fill(std::byte{0xaa});
      exact[0] = std::byte{11};
      exact[1] = static_cast<std::byte>(key);
      store_u16(exact, 2U, ordinal, false);
      exact[4] = static_cast<std::byte>(type);
      exact[5] = std::byte{0x99};
      exact[7] = std::byte{7};
      exact[8] = std::byte{8};
      exact[9] = std::byte{9};
      store_u16(exact, 10U, 0x1010U, false);
      store_u16(exact, 12U, 0x1212U, false);
      store_u16(exact, 14U, 0x1414U, false);
    };
    initialize(0U, 3U, 0U, 0U);
    initialize(1U, 3U, 0U, 1U);
    initialize(2U, 4U, 1U, 1U);
    initialize(3U, 9U, 2U, 0U);
    initialize(4U, 9U, 3U, 0U);
    initialize(5U, 6U, 0U, 0U);
    initialize(6U, 7U, 0U, 0U);
    initialize(7U, 10U, 0U, 0U);
    initialize(8U, 12U, 0U, 0U);
    initialize(9U, 14U, 0U, 0U);
    initialize(10U, 15U, 0U, 0U);
    initialize(11U, 18U, 0U, 0U);
    initialize(12U, 29U, 0U, 0U);
    initialize(13U, 33U, 0U, 0U);
    initialize(14U, 36U, 0U, 0U);
    initialize(15U, 3U, 4U, 1U);
    initialize(16U, 9U, 5U, 0U);
    initialize(17U, 13U, 0U, 0U);
    const auto untouched = tower.people[17].exact_bytes;

    assert(simtower::reset_original_people_for_day(tower) == 17U);
    const std::array<std::uint8_t, 17> expected_states = {
        0x24U, 0x10U, 0x20U, 0x10U, 0x20U,
        0x20U, 0x20U, 0x20U, 0x20U, 0x01U,
        0x00U, 0x27U, 0x27U, 0x01U, 0x27U,
        0x10U, 0x10U,
    };
    for (std::size_t index = 0; index < expected_states.size(); ++index) {
      assert(tower.people[index].exact_bytes[5] ==
             static_cast<std::byte>(expected_states[index]));
      assert(tower.people[index].exact_bytes[6] == std::byte{0xaa});
    }

    // Hotel and Condo take the short tail: only bytes 7 and 8 are cleared.
    for (const std::size_t index : {0U, 1U, 2U, 3U, 4U, 15U, 16U}) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[7] == std::byte{0} && exact[8] == std::byte{0});
      assert(exact[9] == std::byte{9});
      assert(load_u16(exact, 10U, false) == 0x1010U);
      assert(load_u16(exact, 12U, false) == 0x1212U);
      assert(load_u16(exact, 14U, false) == 0x1414U);
    }
    // Office additionally clears word 12, but preserves byte 9 and words
    // 10/14 exactly as loc_00a4 does.
    {
      const auto& exact = tower.people[6].exact_bytes;
      assert(exact[7] == std::byte{0} && exact[8] == std::byte{0});
      assert(exact[9] == std::byte{9});
      assert(load_u16(exact, 10U, false) == 0x1010U);
      assert(load_u16(exact, 12U, false) == 0U);
      assert(load_u16(exact, 14U, false) == 0x1414U);
    }
    // Every other handled family takes the full common tail. Housekeeping
    // then restores byte 7 to ff after the common clearing block.
    for (const std::size_t index :
         {5U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U}) {
      const auto& exact = tower.people[index].exact_bytes;
      assert(exact[7] == (index == 10U ? std::byte{0xff} : std::byte{0}));
      assert(exact[8] == std::byte{0} && exact[9] == std::byte{0});
      assert(load_u16(exact, 10U, false) == 0U);
      assert(load_u16(exact, 12U, false) == 0U);
      assert(load_u16(exact, 14U, false) == 0U);
    }
    assert(tower.people[17].exact_bytes == untouched);
  }

  {
    // Scheduled 1220:1059 span selection plus direct 1220:1518 low-code
    // 1210:1184 Stair release and 11d8:0000 metric finalizer. Type 18 is
    // deliberately absent from 10af's live arg-zero branches; Housekeeping
    // releases transit at state three but skips both metric helpers.
    auto tower = simtower::make_original_new_tdt();
    auto& floor = tower.floors[5];
    floor.tenants.clear();
    floor.tenant_index.fill(0U);
    const auto add_tenant = [&](std::uint8_t key, std::int8_t type,
                                std::uint32_t people_start) {
      simtower::OriginalTdtTenant tenant{};
      tenant.type = type;
      tenant.exact_bytes[4] = static_cast<std::byte>(type);
      tenant.exact_bytes[12] = static_cast<std::byte>(key);
      store_u32(tenant.exact_bytes, 8U, people_start, false);
      floor.tenant_index[key] =
          static_cast<std::uint16_t>(floor.tenants.size());
      floor.tenants.push_back(tenant);
    };
    add_tenant(0U, 3, 0U);
    add_tenant(1U, 15, 10U);
    add_tenant(2U, 34, 20U);
    add_tenant(3U, 18, 100U);

    const auto initialize = [&](std::size_t index, std::uint8_t type,
                                std::uint8_t state, std::uint8_t transit) {
      auto& exact = tower.people[index].exact_bytes;
      exact[4] = static_cast<std::byte>(type);
      exact[5] = static_cast<std::byte>(state);
      exact[7] = std::byte{5};
      exact[8] = static_cast<std::byte>(transit);
      exact[9] = std::byte{7};
      store_u16(exact, 10U, 9U, false);
      store_u16(exact, 12U, 0x8405U, false);
      store_u16(exact, 14U, 10U, false);
    };
    initialize(0U, 3U, 0x40U, 2U);
    initialize(1U, 3U, 0x3fU, 2U);
    initialize(10U, 15U, 3U, 3U);
    initialize(20U, 34U, 0x40U, 4U);
    initialize(100U, 18U, 0x40U, 5U);

    auto initialize_stair = [&](std::size_t index, std::int8_t stair_floor) {
      auto& stair = tower.post_elevator.stairs_bd70[index];
      stair.floor = stair_floor;
      stair.word_6 = 7U;
      stair.word_8 = 4U;
      stair.exact_bytes[4] = static_cast<std::byte>(stair_floor);
      store_u16(stair.exact_bytes, 6U, stair.word_6, false);
      store_u16(stair.exact_bytes, 8U, stair.word_8, false);
    };
    initialize_stair(2U, 5);
    initialize_stair(3U, 4);
    initialize_stair(4U, 5);
    initialize_stair(5U, 5);

    assert(simtower::sweep_original_people_transit(tower, 0x00f0U) == 3U);
    assert(tower.post_elevator.stairs_bd70[2].word_8 == 3U);
    assert(tower.post_elevator.stairs_bd70[3].word_6 == 6U);
    assert(tower.post_elevator.stairs_bd70[4].word_8 == 3U);
    assert(tower.post_elevator.stairs_bd70[5].word_8 == 4U);
    assert(load_u16(tower.post_elevator.stairs_bd70[2].exact_bytes,
                    8U, false) == 3U);
    assert(tower.people[0].exact_bytes[9] == std::byte{8});
    assert(load_u16(tower.people[0].exact_bytes, 10U, false) == 0U);
    assert(load_u16(tower.people[0].exact_bytes, 12U, false) == 0x8400U);
    assert(load_u16(tower.people[0].exact_bytes, 14U, false) == 15U);
    assert(tower.people[1].exact_bytes[9] == std::byte{7});
    assert(tower.people[10].exact_bytes[9] == std::byte{7});
    assert(load_u16(tower.people[10].exact_bytes, 12U, false) == 0x8405U);
    assert(tower.people[20].exact_bytes[9] == std::byte{8});
    assert(tower.people[100].exact_bytes[9] == std::byte{7});
  }

  {
    // Direct 1210:1c46 and 11d8:00fc coverage. Both 1210:15ea Elevator
    // waiting lanes use forty-entry circular queues. Draining advances the
    // cursor, removes the matching person, then rebuilds the retained order
    // from that new cursor. The first person exercises 00fc's 300 cap; the
    // second proves that a negative signed difference is not lower-clamped
    // and can borrow from word 12's upper flag bits before 11d8:0000 consumes
    // its resulting low ten bits.
    auto tower = simtower::make_original_new_tdt();
    auto& floor = tower.floors[5];
    floor.tenants.clear();
    floor.tenant_index.fill(0U);
    simtower::OriginalTdtTenant tenant{};
    tenant.type = 6;
    tenant.exact_bytes[4] = std::byte{6};
    tenant.exact_bytes[12] = std::byte{0};
    store_u32(tenant.exact_bytes, 8U, 0U, false);
    floor.tenants.push_back(tenant);
    floor.tenant_index[0] = 0U;

    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.capacity = 3U;
    elevator.bottom_floor = 5;
    elevator.top_floor = 5;
    elevator.car_home_floors.fill(std::byte{5});
    simtower::OriginalTdtElevatorFloorRecord record{};
    record.mapped_index = 0;
    record.floor = 5;
    record.exact_bytes[0] = std::byte{3};
    record.exact_bytes[1] = std::byte{38};
    store_u32(record.exact_bytes, 4U + 38U * 4U, 4U, false);
    store_u32(record.exact_bytes, 4U + 39U * 4U, 0U, false);
    store_u32(record.exact_bytes, 4U, 7U, false);
    record.exact_bytes[2] = std::byte{2};
    record.exact_bytes[3] = std::byte{39};
    store_u32(record.exact_bytes, 164U + 39U * 4U, 1U, false);
    store_u32(record.exact_bytes, 164U, 9U, false);
    elevator.floor_records.push_back(record);

    for (std::size_t index = 0; index < 2U; ++index) {
      auto& exact = tower.people[index].exact_bytes;
      exact[4] = std::byte{6};
      exact[5] = std::byte{0x40};
      exact[7] = std::byte{5};
      exact[8] = static_cast<std::byte>(index == 0U ? 0x40U : 0x58U);
      exact[9] = std::byte{1};
      store_u16(exact, 10U, index == 0U ? 20U : 246U, false);
      store_u16(exact, 12U, index == 0U ? 0x8064U : 0x8405U, false);
      store_u16(exact, 14U, 1U, false);
    }

    assert(simtower::sweep_original_people_transit(tower, 0x00f0U) == 2U);
    const auto& queue = elevator.floor_records[0].exact_bytes;
    assert(queue[0] == std::byte{2} && queue[1] == std::byte{1});
    assert(load_u32(queue, 4U + 1U * 4U, false) == 4U);
    assert(load_u32(queue, 4U + 2U * 4U, false) == 7U);
    assert(queue[2] == std::byte{1} && queue[3] == std::byte{1});
    assert(load_u32(queue, 164U + 1U * 4U, false) == 9U);
    const auto& capped = tower.people[0].exact_bytes;
    assert(capped[9] == std::byte{2});
    assert(load_u16(capped, 10U, false) == 0U);
    assert(load_u16(capped, 12U, false) == 0x8000U);
    assert(load_u16(capped, 14U, false) == 301U);
    const auto& negative = tower.people[1].exact_bytes;
    assert(negative[9] == std::byte{2});
    assert(load_u16(negative, 10U, false) == 0U);
    assert(load_u16(negative, 12U, false) == 0x8000U);
    assert(load_u16(negative, 14U, false) == 1024U);

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.elevators[0].floor_records[0].exact_bytes[0] ==
           std::byte{2});
    assert(load_u32(reparsed.elevators[0].floor_records[0].exact_bytes,
                    4U + 1U * 4U, false) == 4U);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // 1210:18fa in-car removal updates slot, floor-occupancy, and aggregate
    // bytes before the 1090:0bcf target/direction/secondary-target recompute.
    auto tower = simtower::make_original_new_tdt();
    auto& floor = tower.floors[5];
    floor.tenants.clear();
    floor.tenant_index.fill(0U);
    simtower::OriginalTdtTenant tenant{};
    tenant.type = 6;
    tenant.exact_bytes[4] = std::byte{6};
    tenant.exact_bytes[12] = std::byte{0};
    store_u32(tenant.exact_bytes, 8U, 0U, false);
    floor.tenants.push_back(tenant);
    floor.tenant_index[0] = 0U;

    auto& elevator = tower.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.capacity = 3U;
    elevator.bottom_floor = 0;
    elevator.top_floor = 10;
    elevator.car_home_floors.fill(std::byte{2});
    for (std::int16_t floor_number = 0; floor_number <= 10;
         ++floor_number) {
      simtower::OriginalTdtElevatorFloorRecord record{};
      record.mapped_index = floor_number;
      record.floor = static_cast<std::int8_t>(floor_number);
      elevator.floor_records.push_back(record);
    }
    auto& car = elevator.car_records[0].exact_bytes;
    car.fill(std::byte{0});
    car[0] = std::byte{5};
    car[3] = std::byte{1};
    car[4] = std::byte{1};
    car[5] = std::byte{7};
    car[12] = std::byte{1};
    car[13] = std::byte{7};
    car[15] = std::byte{1};
    store_u32(car, 16U, 0U, false);
    car[184] = std::byte{7};
    car[226U + 7U] = std::byte{1};

    auto& person = tower.people[0].exact_bytes;
    person[4] = std::byte{6};
    person[5] = std::byte{0x40};
    person[7] = std::byte{5};
    person[8] = std::byte{0x40};
    store_u16(person, 12U, 5U, false);
    assert(simtower::sweep_original_people_transit(tower, 0U) == 1U);
    assert(car[3] == std::byte{0});
    assert(load_u32(car, 16U, false) == 0xffffffffU);
    assert(car[184] == std::byte{0xff});
    assert(car[226U + 7U] == std::byte{0});
    assert(car[12] == std::byte{0});
    assert(car[5] == std::byte{2});
    assert(car[4] == std::byte{0});
    assert(car[13] == std::byte{2});

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(load_u32(reparsed.elevators[0].car_records[0].exact_bytes,
                    16U, false) == 0xffffffffU);
    assert(reparsed.elevators[0].car_records[0].exact_bytes[5] ==
           std::byte{2});
    assert(simtower::serialize_original_tdt(reparsed) == bytes);
  }

  {
    // Office's 1198:0489 path runs for all six owned records even below the
    // transit-finalization threshold.
    auto tower = make_active_office();
    auto& tenant = tower.floors[11].tenants[0];
    auto& person = tower.people[0].exact_bytes;
    person[5] = std::byte{0x20};
    person[13] = std::byte{0xfc};
    store_u16(person, 12U, 0xc055U, false);
    tenant.status = 6U;
    tenant.exact_bytes[5] = std::byte{6};
    tower.post_elevator.b846_series[0][0] = 10;
    tower.post_elevator.b846_series[0][10] = 20;
    tower.post_elevator.parking_connected = 1;
    auto& route = tower.post_elevator.cf9c_records[0];
    route[0] = person[0];
    route[1] = person[1];
    store_u32(route, 2U, 0U, false);
    assert(simtower::sweep_original_people_transit(tower, 0x0960U) == 0U);
    assert(tower.post_elevator.b846_series[0][0] == 9);
    assert(tower.post_elevator.b846_series[0][10] == 19);
    assert(load_u16(person, 12U, false) == 0x0055U);
    assert(tenant.status == 0U && tenant.exact_bytes[13] == std::byte{1});
    assert(load_u32(route, 2U, false) == 0U);
  }

  {
    // Opposite-endian logical words and tenant person-start dwords retain
    // their source byte order throughout the sweep and serializer.
    auto tower = simtower::make_original_new_tdt();
    tower.header.byte_swapped = true;
    auto& floor = tower.floors[5];
    floor.tenants.clear();
    floor.tenant_index.fill(0U);
    simtower::OriginalTdtTenant tenant{};
    tenant.type = 3;
    tenant.exact_bytes[4] = std::byte{3};
    tenant.exact_bytes[12] = std::byte{0};
    store_u32(tenant.exact_bytes, 8U, 0U, true);
    floor.tenants.push_back(tenant);
    floor.tenant_index[0] = 0U;
    auto& person = tower.people[0].exact_bytes;
    person[4] = std::byte{3};
    person[5] = std::byte{0x40};
    person[7] = std::byte{5};
    person[8] = std::byte{1};
    store_u16(person, 12U, 0x4003U, true);
    store_u16(person, 14U, 9U, true);
    auto& stair = tower.post_elevator.stairs_bd70[1];
    stair.floor = 5;
    stair.word_8 = 5U;
    stair.exact_bytes[4] = std::byte{5};
    store_u16(stair.exact_bytes, 8U, 5U, true);
    assert(simtower::sweep_original_people_transit(tower, 0U) == 1U);
    assert(stair.word_8 == 4U);
    assert(load_u16(stair.exact_bytes, 8U, true) == 4U);
    assert(load_u16(person, 12U, true) == 0x4000U);
    assert(load_u16(person, 14U, true) == 12U);
  }

  {
    auto tower = simtower::make_original_new_tdt();
    auto& floor = tower.floors[50];
    floor.tenants.clear();
    const auto add = [&](std::int8_t type, std::uint8_t status,
                         std::uint8_t variant) {
      simtower::OriginalTdtTenant tenant{};
      tenant.type = type;
      tenant.status = status;
      tenant.variant = variant;
      tenant.exact_bytes[4] = static_cast<std::byte>(type);
      tenant.exact_bytes[5] = static_cast<std::byte>(status);
      tenant.exact_bytes[6] = static_cast<std::byte>(variant);
      tenant.exact_bytes[13] = std::byte{0};
      floor.tenants.push_back(tenant);
    };
    add(3, 0x20U, 0U);
    add(4, 0x30U, 0U);
    add(5, 0x40U, 0U);
    add(3, 0x10U, 0U);
    add(7, 0x18U, 0U);
    add(7, 0x10U, 0U);
    add(9, 0x20U, 0U);
    add(9, 0x18U, 0U);
    add(13, 0U, 5U);
    add(31, 0U, 1U);
    add(32, 0U, 1U);
    add(33, 0U, 1U);
    add(36, 0U, 1U);
    add(37, 0U, 1U);
    add(38, 0U, 1U);
    add(39, 0U, 1U);
    add(40, 0U, 3U);
    add(6, 0x44U, 7U);

    // Direct 1228:0968 coverage: Hotel, Office, Condo, Medical, Metro, and
    // Cathedral jump-table tails plus the calendar-dependent Office branch.
    assert(simtower::advance_original_facilities_for_day(tower, 0U) == 15U);
    const std::array<std::uint8_t, 8> expected_status = {
        0x18U, 0x28U, 0x38U, 0x10U,
        0x10U, 0x00U, 0x18U, 0x18U,
    };
    for (std::size_t index = 0; index < expected_status.size(); ++index) {
      assert(floor.tenants[index].status == expected_status[index]);
      assert(floor.tenants[index].exact_bytes[5] ==
             static_cast<std::byte>(expected_status[index]));
    }
    for (const std::size_t index :
         {0U, 1U, 2U, 4U, 5U, 6U, 8U, 9U, 10U, 11U, 12U, 13U, 14U,
          15U, 16U}) {
      assert(floor.tenants[index].exact_bytes[13] == std::byte{1});
    }
    assert(floor.tenants[3].exact_bytes[13] == std::byte{0});
    assert(floor.tenants[7].exact_bytes[13] == std::byte{0});
    // Type 13 reaches 0ada and is only dirtied; unlike 31..33/36..40 it does
    // not execute 0abd's complete +0x0c word clear.
    assert(floor.tenants[8].variant == 5U);
    assert(floor.tenants[8].exact_bytes[6] == std::byte{5});
    assert(floor.tenants[8].exact_bytes[7] == std::byte{0});
    for (std::size_t index = 9U; index <= 16U; ++index) {
      assert(floor.tenants[index].variant == 0U);
      assert(floor.tenants[index].exact_bytes[6] == std::byte{0});
      assert(floor.tenants[index].exact_bytes[7] == std::byte{0});
    }
    assert(floor.tenants[17].status == 0x44U);
    assert(floor.tenants[17].variant == 7U);
    assert(floor.tenants[17].exact_bytes[13] == std::byte{0});

    // A later invocation with the alternate calendar phase takes the other
    // Office branch; the always-touched service/special types are visited too.
    // Medical Center preserves +0x0c while the remaining types reset it.
    assert(simtower::advance_original_facilities_for_day(tower, 1U) == 11U);
    assert(floor.tenants[4].status == 8U);
    assert(floor.tenants[5].status == 8U);

    floor.tenants[0].status = 0x18U;
    floor.tenants[1].status = 0x28U;
    floor.tenants[2].status = 0x38U;
    floor.tenants[4].status = 0x10U;
    floor.tenants[5].status = 0U;
    floor.tenants[6].status = 0x18U;
    floor.tenants[7].status = 5U;
    for (std::size_t index = 0; index <= 7U; ++index) {
      floor.tenants[index].exact_bytes[5] =
          static_cast<std::byte>(floor.tenants[index].status);
    }
    // Direct 1228:0b59 coverage: all five jump-table families, the signed
    // low-three-bit Hotel Suite status gate, and the shared dirty byte.
    assert(simtower::advance_original_facilities_for_evening(tower) == 16U);
    const std::array<std::uint8_t, 8> expected_evening = {
        0x20U, 0x30U, 0x40U, 0x10U,
        0x18U, 0x08U, 0x20U, 0x0dU,
    };
    for (std::size_t index = 0; index < expected_evening.size(); ++index) {
      assert(floor.tenants[index].status == expected_evening[index]);
      assert(floor.tenants[index].exact_bytes[5] ==
             static_cast<std::byte>(expected_evening[index]));
    }
    for (std::size_t index = 9U; index <= 16U; ++index) {
      assert(floor.tenants[index].variant == 1U);
      assert(floor.tenants[index].exact_bytes[6] == std::byte{1});
      assert(floor.tenants[index].exact_bytes[7] == std::byte{0});
    }
    // 1228:0d1b also only dirties Medical Center during the evening sweep;
    // the +0x0c word remains the five written by the fixture.
    assert(floor.tenants[8].variant == 5U);
    assert(floor.tenants[8].exact_bytes[6] == std::byte{5});
    assert(floor.tenants[8].exact_bytes[7] == std::byte{0});
    assert(floor.tenants[8].exact_bytes[13] == std::byte{1});
  }

  {
    // Pre-midnight facility status sweep at 1228:086b.
    auto tower = simtower::make_original_new_tdt();
    auto& floor = tower.floors[25];
    const auto add = [&](std::int8_t type, std::uint8_t status) {
      simtower::OriginalTdtTenant tenant{};
      tenant.type = type;
      tenant.status = status;
      tenant.exact_bytes[4] = static_cast<std::byte>(type);
      tenant.exact_bytes[5] = static_cast<std::byte>(status);
      tenant.exact_bytes[13] = std::byte{0};
      floor.tenants.push_back(tenant);
    };
    add(3, 0U);
    add(4, 0x17U);
    add(5, 0x18U);
    add(7, 0x0fU);
    add(7, 0x10U);
    add(9, 5U);
    add(9, 0x18U);
    add(6, 1U);

    assert(simtower::prepare_original_facilities_for_night(tower) == 4U);
    const std::array<std::uint8_t, 8> expected = {
        0x10U, 0x10U, 0x18U, 0x08U, 0x10U, 0x10U, 0x18U, 0x01U,
    };
    for (std::size_t index = 0; index < expected.size(); ++index) {
      assert(floor.tenants[index].status == expected[index]);
      assert(floor.tenants[index].exact_bytes[5] ==
             static_cast<std::byte>(expected[index]));
      const bool touched = index == 0U || index == 1U || index == 3U ||
                           index == 5U;
      assert(floor.tenants[index].exact_bytes[13] ==
             (touched ? std::byte{1} : std::byte{0}));
    }
  }

  {
    // Self-contained Hotel pair repair at 1130:01e2. The exact loop skips a
    // low-status/non-Hotel next slot but revisits an already-high Hotel next.
    auto tower = simtower::make_original_new_tdt();
    auto& floor = tower.floors[25];
    const auto add = [&](std::int8_t type, std::uint8_t status) {
      simtower::OriginalTdtTenant tenant{};
      tenant.type = type;
      tenant.status = status;
      tenant.exact_bytes[4] = static_cast<std::byte>(type);
      tenant.exact_bytes[5] = static_cast<std::byte>(status);
      tenant.exact_bytes[13] = std::byte{0};
      tenant.exact_bytes[14] = std::byte{7};
      tenant.exact_bytes[15] = std::byte{8};
      tenant.preserved_07_to_0f[6] = std::byte{0};
      tenant.preserved_07_to_0f[7] = std::byte{7};
      tenant.preserved_07_to_0f[8] = std::byte{8};
      floor.tenants.push_back(tenant);
    };
    add(3, 0x38U);  // writes low-status next, which is then skipped
    add(4, 0x00U);
    add(5, 0x38U);  // rewrites previous; high-status next is revisited
    add(5, 0x38U);  // rewrites previous, then skips non-Hotel next
    add(6, 0x00U);
    add(5, 0x38U);  // no adjacent Hotel writes

    assert(simtower::repair_original_hotel_pair_states(tower, 4U) == 3U);
    assert(floor.tenants[0].status == 0x38U);
    assert(floor.tenants[1].status == 0x40U);
    assert(floor.tenants[2].status == 0x40U);
    assert(floor.tenants[3].status == 0x38U);
    assert(floor.tenants[5].status == 0x38U);
    for (const std::size_t index : {1U, 2U}) {
      assert(floor.tenants[index].exact_bytes[13] == std::byte{1});
      assert(floor.tenants[index].exact_bytes[14] == std::byte{0});
      assert(floor.tenants[index].exact_bytes[15] == std::byte{0xff});
      assert(floor.tenants[index].preserved_07_to_0f[6] == std::byte{1});
      assert(floor.tenants[index].preserved_07_to_0f[7] == std::byte{0});
      assert(floor.tenants[index].preserved_07_to_0f[8] == std::byte{0xff});
    }

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.floors[25].tenants[1].status == 0x40U);
    assert(reparsed.floors[25].tenants[2].status == 0x40U);
    assert(reparsed.floors[25].tenants[2].exact_bytes[15] == std::byte{0xff});
    assert(simtower::serialize_original_tdt(reparsed) == bytes);

    tower = simtower::make_original_new_tdt();
    simtower::OriginalTdtTenant first{};
    first.type = 3;
    first.status = 0x38U;
    first.exact_bytes[4] = std::byte{3};
    first.exact_bytes[5] = std::byte{0x38};
    simtower::OriginalTdtTenant second = first;
    second.status = 0U;
    second.exact_bytes[5] = std::byte{0};
    tower.floors[0].tenants = {first, second};
    assert(simtower::repair_original_hotel_pair_states(tower, 3U) == 1U);
    assert(tower.floors[0].tenants[1].status == 0x38U);
  }

  {
    // Scheduled dce4 filters at 1188:0977/0a20. The table contains persisted
    // person-record indices; each removal shifts and retries the same slot.
    auto tower = simtower::make_original_new_tdt();
    constexpr std::array<std::int8_t, 10> types = {
        6, 7, 10, 3, 4, 5, 36, 9, 12, 18};
    tower.header.person_link_count =
        static_cast<std::uint16_t>(types.size());
    for (std::size_t index = 0; index < types.size(); ++index) {
      tower.people[index].exact_bytes[4] = static_cast<std::byte>(types[index]);
      tower.post_elevator.dce4_person_indices[index] =
          static_cast<std::int32_t>(index);
    }

    assert(simtower::remove_original_nightly_person_links(tower) == 5U);
    assert(tower.header.person_link_count == 5U);
    const std::array<std::int32_t, 5> after_night = {1, 3, 4, 5, 7};
    assert(std::equal(after_night.begin(), after_night.end(),
                      tower.post_elevator.dce4_person_indices.begin()));
    for (std::size_t index = 5U; index < 10U; ++index) {
      assert(tower.post_elevator.dce4_person_indices[index] == -1);
    }

    assert(simtower::remove_original_hotel_person_links(tower) == 3U);
    assert(tower.header.person_link_count == 2U);
    assert(tower.post_elevator.dce4_person_indices[0] == 1);
    assert(tower.post_elevator.dce4_person_indices[1] == 7);
    for (std::size_t index = 2U; index < 10U; ++index) {
      assert(tower.post_elevator.dce4_person_indices[index] == -1);
    }

    const auto bytes = simtower::serialize_original_tdt(tower);
    const auto reparsed = simtower::parse_original_tdt(bytes);
    assert(reparsed.header.person_link_count == 2U);
    assert(reparsed.post_elevator.dce4_person_indices[0] == 1);
    assert(reparsed.post_elevator.dce4_person_indices[1] == 7);
    assert(reparsed.post_elevator.dce4_person_indices[2] == -1);
    assert(simtower::serialize_original_tdt(reparsed) == bytes);

    // Revision 0x22 stores the first ten indices in the 0x28-byte dce4 block
    // and omits both the twenty-entry block and header b404 word.
    tower = simtower::make_original_new_tdt();
    tower.header.raw_version = 0x2200U;
    tower.header.format_version = 0x22U;
    tower.header.exact_bytes.erase(tower.header.exact_bytes.begin() + 58,
                                   tower.header.exact_bytes.begin() + 60);
    tower.header.person_link_count = 3U;
    tower.post_elevator.dce4_person_indices[0] = 12;
    tower.post_elevator.dce4_person_indices[1] = 34;
    tower.post_elevator.dce4_person_indices[2] = 56;
    const auto old_bytes = simtower::serialize_original_tdt(tower);
    const auto old_reparsed = simtower::parse_original_tdt(old_bytes);
    assert(old_reparsed.header.format_version == 0x22U);
    assert(old_reparsed.header.person_link_count == 3U);
    assert(old_reparsed.post_elevator.dce4_person_indices[0] == 12);
    assert(old_reparsed.post_elevator.dce4_person_indices[1] == 34);
    assert(old_reparsed.post_elevator.dce4_person_indices[2] == 56);
    assert(simtower::serialize_original_tdt(old_reparsed) == old_bytes);
  }

  {
    // 11f8:35ac always calls 1220:10af with a literal nonzero cleanup flag.
    // Verify the Hotel path's transit finalizer, dce4 removals, parking-owner
    // cleanup, person retirement, and tracked-visitor reset as one operation.
    auto tower = simtower::make_original_new_tdt();
    tower.people_count = 4U;
    tower.people.resize(4U);

    simtower::OriginalTdtTenant hotel{};
    hotel.left = 100U;
    hotel.right = 108U;
    hotel.type = 3;
    hotel.exact_bytes[4] = std::byte{3};
    hotel.exact_bytes[12] = std::byte{0};
    hotel.preserved_07_to_0f[5] = std::byte{0};
    store_u32(hotel.exact_bytes, 8U, 0U, false);
    tower.floors[11].tenants = {hotel};
    tower.floors[11].tenant_index[0] = 0U;

    for (std::size_t index = 0; index < 2U; ++index) {
      auto& person = tower.people[index].exact_bytes;
      person[4] = std::byte{3};
      person[6] = std::byte{0x55};
      person[8] = std::byte{0xff};
    }
    tower.people[0].exact_bytes[5] = std::byte{0x40};
    store_u16(tower.people[1].exact_bytes, 12U, 0xfc44U, false);

    simtower::OriginalTdtTenant parking{};
    parking.type = 11;
    parking.status = 2U;
    parking.exact_bytes[4] = std::byte{11};
    parking.exact_bytes[5] = std::byte{2};
    parking.exact_bytes[12] = std::byte{1};
    tower.floors[9].tenants = {parking};
    tower.floors[9].tenant_index[1] = 0U;
    tower.post_elevator.parking_connected = 1;
    auto& parking_owner = tower.post_elevator.cf9c_records[0];
    parking_owner.fill(std::byte{0});
    parking_owner[0] = std::byte{9};
    parking_owner[1] = std::byte{1};
    store_u32(parking_owner, 2U, 1U, false);

    tower.header.person_link_count = 3U;
    tower.post_elevator.dce4_person_indices[0] = 0;
    tower.post_elevator.dce4_person_indices[1] = 1;
    tower.post_elevator.dce4_person_indices[2] = 3;
    tower.post_elevator.b923 = 7U;
    tower.post_elevator.b928 = 1U;
    tower.post_elevator.b924 = 1;

    // Direct 1240:0198 coverage: deleting the tracked Hotel owner clears the
    // periodic visitor globals and requests information notification 3003.
    const auto cleanup = simtower::cleanup_original_facility_people(
        tower, 11, 0U, 25U);
    assert(cleanup.finalized == 1U && cleanup.retired == 2U);
    assert(cleanup.cleared_periodic_visitor &&
           cleanup.notification_code == 3003U);
    assert(tower.people[0].exact_bytes[4] == std::byte{0} &&
           tower.people[0].exact_bytes[6] == std::byte{0});
    assert(tower.people[1].exact_bytes[4] == std::byte{0} &&
           tower.people[1].exact_bytes[6] == std::byte{0});
    assert(load_u16(tower.people[1].exact_bytes, 12U, false) == 0x44U);
    assert(tower.header.person_link_count == 1U);
    assert(tower.post_elevator.dce4_person_indices[0] == 3);
    assert(tower.post_elevator.dce4_person_indices[1] == -1);
    assert(tower.floors[9].tenants[0].status == 0U);
    assert(tower.floors[9].tenants[0].exact_bytes[13] == std::byte{1});
    assert(load_u32(parking_owner, 2U, false) == 0U);
    assert(tower.post_elevator.b923 == 0U &&
           tower.post_elevator.b928 == 0U &&
           tower.post_elevator.b924 == -1);

    // Direct 11f8:3383 coverage: types 11/13 return from 1220:10af and
    // 24..26 are additionally protected from its retirement tail. None may
    // retire a nominally linked person span.
    for (const std::int8_t protected_type : {11, 13, 24, 25, 26}) {
      tower.floors[11].tenants[0].type = protected_type;
      tower.floors[11].tenants[0].exact_bytes[4] =
          static_cast<std::byte>(protected_type);
      tower.people[0].exact_bytes[4] = static_cast<std::byte>(protected_type);
      const auto protected_cleanup = simtower::cleanup_original_facility_people(
          tower, 11, 0U, 25U);
      assert(protected_cleanup.finalized == 0U &&
             protected_cleanup.retired == 0U);
      assert(tower.people[0].exact_bytes[4] ==
             static_cast<std::byte>(protected_type));
    }
  }

  {
    // Direct 1228:07c5 coverage through the real 1220:10af facility-removal
    // consumer. Exercise every non-default table result plus a default type;
    // the returned retirement count is the exact person span selected by the
    // original 38-entry jump table.
    struct PersonSpanCase {
      std::int8_t type;
      std::size_t count;
    };
    constexpr std::array span_cases{
        PersonSpanCase{3, 2U},   PersonSpanCase{4, 3U},
        PersonSpanCase{5, 3U},   PersonSpanCase{6, 48U},
        PersonSpanCase{7, 6U},   PersonSpanCase{9, 3U},
        PersonSpanCase{10, 48U}, PersonSpanCase{12, 48U},
        PersonSpanCase{18, 56U}, PersonSpanCase{19, 56U},
        PersonSpanCase{29, 40U}, PersonSpanCase{30, 40U},
        PersonSpanCase{33, 240U}, PersonSpanCase{34, 56U},
        PersonSpanCase{35, 56U}, PersonSpanCase{36, 8U},
        PersonSpanCase{37, 8U},  PersonSpanCase{38, 8U},
        PersonSpanCase{39, 8U},  PersonSpanCase{40, 8U},
    };
    for (const auto& entry : span_cases) {
      auto tower = simtower::make_original_new_tdt();
      tower.header.rating = 2U;
      auto& floor = tower.floors[10];
      floor.tenants.clear();
      floor.tenant_index.fill(0U);
      simtower::OriginalTdtTenant owner{};
      owner.type = entry.type;
      owner.exact_bytes[4] = static_cast<std::byte>(entry.type);
      owner.exact_bytes[12] = std::byte{0};
      store_u32(owner.exact_bytes, 8U, 0U, false);
      floor.tenants.push_back(owner);
      const auto cleanup = simtower::cleanup_original_facility_people(
          tower, 10, 0U, 0U);
      assert(cleanup.retired == entry.count);
    }
  }

  {
    // Scheduled Recycling Center state machine at 1088:0000/00de/01d1.
    auto tower = simtower::make_original_new_tdt();
    auto& floor = tower.floors[9];
    const auto add = [&](std::int8_t type, std::uint8_t status) {
      simtower::OriginalTdtTenant tenant{};
      tenant.type = type;
      tenant.status = status;
      tenant.exact_bytes[4] = static_cast<std::byte>(type);
      tenant.exact_bytes[5] = static_cast<std::byte>(status);
      tenant.exact_bytes[13] = std::byte{0};
      floor.tenants.push_back(tenant);
    };
    add(20, 4U);
    add(21, 4U);
    add(6, 0x44U);

    // The complete family is gated below a three-star rating and leaves even
    // the process/save-backed b92c byte untouched.
    tower.header.rating = 2U;
    store_u16(tower.header.exact_bytes, 42U, 2U, false);  // DS:b3f4
    tower.post_elevator.b92c = 7U;
    tower.post_elevator.finance.total_population = 5000;
    auto phase = simtower::advance_original_recycling_phase(tower, 5U);
    assert(phase.touched == 0U && phase.notification_code == 0U);
    assert(tower.post_elevator.b92c == 7U);
    assert(floor.tenants[0].status == 4U &&
           floor.tenants[1].status == 4U);

    // A valid rating with no center invokes information code 3 and lowers
    // b92c without touching tenant bytes.
    tower.header.rating = 3U;
    store_u16(tower.header.exact_bytes, 42U, 0U, false);
    phase = simtower::advance_original_recycling_phase(tower, 5U);
    assert(phase.touched == 0U && phase.notification_code == 3U);
    assert(tower.post_elevator.b92c == 0U);

    // 1088:0250 uses total population divided by the persisted center count.
    // Quotients 499 and 500 select phases one and two respectively.
    store_u16(tower.header.exact_bytes, 42U, 2U, false);
    tower.post_elevator.finance.total_population = 999;
    phase = simtower::advance_original_recycling_phase(tower, 5U);
    assert(phase.touched == 2U && phase.notification_code == 0U);
    assert(tower.post_elevator.b92c == 1U);
    assert(floor.tenants[0].status == 1U &&
           floor.tenants[1].status == 1U);
    assert(floor.tenants[0].exact_bytes[13] == std::byte{1} &&
           floor.tenants[1].exact_bytes[13] == std::byte{1});
    tower.post_elevator.finance.total_population = 1000;
    phase = simtower::advance_original_recycling_phase(tower, 5U);
    assert(phase.touched == 2U);
    assert(floor.tenants[0].status == 2U &&
           floor.tenants[1].status == 2U);

    // When the computed phase exceeds the scheduled request, the request is
    // used, b92c falls, and a tenant already on frame five is deliberately
    // skipped. Requested phase five also emits information code 4.
    floor.tenants[0].status = 2U;
    floor.tenants[0].exact_bytes[5] = std::byte{2};
    floor.tenants[1].status = 5U;
    floor.tenants[1].exact_bytes[5] = std::byte{5};
    tower.post_elevator.finance.total_population = 5000;  // 2500/center => 6
    phase = simtower::advance_original_recycling_phase(tower, 5U);
    assert(phase.touched == 1U && phase.notification_code == 4U);
    assert(tower.post_elevator.b92c == 0U);
    assert(floor.tenants[0].status == 5U &&
           floor.tenants[1].status == 5U);

    floor.tenants[0].status = 3U;
    floor.tenants[0].exact_bytes[5] = std::byte{3};
    phase = simtower::advance_original_recycling_phase(tower, 0U);
    assert(phase.touched == 1U && phase.notification_code == 0U);
    assert(floor.tenants[0].status == 0U &&
           floor.tenants[1].status == 5U);

    // b92c and both translated tenant states survive the native save path.
    const auto reparsed = simtower::parse_original_tdt(
        simtower::serialize_original_tdt(tower));
    assert(reparsed.post_elevator.b92c == 0U);
    assert(reparsed.floors[9].tenants[0].status == 0U);
    assert(reparsed.floors[9].tenants[1].status == 5U);

    // Midnight pass: active upper/lower halves become 0/6 and request the
    // single WAVE/2280 signal; frame 0020 then clears only lower frame six.
    floor.tenants[0].status = 3U;
    floor.tenants[0].exact_bytes[5] = std::byte{3};
    floor.tenants[1].status = 4U;
    floor.tenants[1].exact_bytes[5] = std::byte{4};
    tower.post_elevator.b92c = 1U;
    auto reset = simtower::reset_original_recycling_for_day(tower);
    assert(reset.touched == 2U && reset.notification_code == 0U &&
           reset.play_transition_sound);
    assert(floor.tenants[0].status == 0U &&
           floor.tenants[1].status == 6U);
    assert(simtower::finish_original_recycling_day_start(tower) == 1U);
    assert(floor.tenants[1].status == 0U);

    tower.post_elevator.b92c = 0U;
    floor.tenants[0].status = 4U;
    floor.tenants[0].exact_bytes[5] = std::byte{4};
    reset = simtower::reset_original_recycling_for_day(tower);
    assert(reset.touched == 0U && !reset.play_transition_sound);
    assert(floor.tenants[0].status == 4U);

    store_u16(tower.header.exact_bytes, 42U, 0U, false);
    tower.post_elevator.b92c = 1U;
    reset = simtower::reset_original_recycling_for_day(tower);
    assert(reset.notification_code == 3U && reset.touched == 0U);
    assert(tower.post_elevator.b92c == 0U);
  }

  {
    // Exact 10a8:0aae waiting-person hit geometry. The process-local floor
    // cache begins in Elevator table order and Shell-sorts by x, so reverse
    // the two shafts here and verify that the left shaft still owns the
    // expected first/second-lane pixels.
    auto tower = simtower::make_original_new_tdt();
    auto& floor = tower.floors[10];
    floor.left_edge = 10U;
    floor.right_edge = 100U;
    tower.people_count = 6U;
    tower.people.resize(6U);

    auto& right = tower.elevators[0];
    right.used = 1U;
    right.type = 1U;
    right.x = 70U;
    right.bottom_floor = 10;
    right.top_floor = 10;

    auto& left = tower.elevators[1];
    left.used = 1U;
    left.type = 1U;
    left.x = 40U;
    left.bottom_floor = 10;
    left.top_floor = 10;
    simtower::OriginalTdtElevatorFloorRecord waiting{};
    waiting.floor = 10;
    waiting.mapped_index = 0;
    waiting.exact_bytes[0] = std::byte{3};
    waiting.exact_bytes[1] = std::byte{39};
    waiting.exact_bytes[2] = std::byte{3};
    waiting.exact_bytes[3] = std::byte{7};
    store_u32(waiting.exact_bytes, 4U + 39U * 4U, 0U, false);
    store_u32(waiting.exact_bytes, 4U, 1U, false);
    store_u32(waiting.exact_bytes, 8U, 2U, false);
    store_u32(waiting.exact_bytes, 164U + 7U * 4U, 3U, false);
    store_u32(waiting.exact_bytes, 164U + 8U * 4U, 4U, false);
    store_u32(waiting.exact_bytes, 164U + 9U * 4U, 5U, false);
    left.floor_records.push_back(waiting);

    tower.people[0].exact_bytes[4] = std::byte{7};   // forced width one
    tower.people[1].exact_bytes[4] = std::byte{15};  // forced width two
    tower.people[2].exact_bytes[4] = std::byte{6};
    store_u16(tower.people[2].exact_bytes, 2U, 5U, false);  // width two
    tower.people[3].exact_bytes[4] = std::byte{7};
    tower.people[4].exact_bytes[4] = std::byte{9};
    store_u16(tower.people[4].exact_bytes, 2U, 1U, false);  // width two
    tower.people[5].exact_bytes[4] = std::byte{9};
    store_u16(tower.people[5].exact_bytes, 2U, 2U, false);  // width one

    const auto hit = [&](int cell_x) {
      return simtower::original_elevator_waiting_person_hit_from_client(
          tower, cell_x * 8, 124, 0, 3800);
    };
    assert(hit(37) == simtower::OriginalElevatorWaitingPersonHit(
                          {true, 1U, 0U, 10,
                           simtower::OriginalElevatorWaitingLane::first, 0U}));
    assert(hit(36).person_index == 1U && hit(36).queue_ordinal == 1U);
    assert(hit(34).person_index == 2U && hit(34).queue_ordinal == 2U);
    assert(!hit(33).hit);
    assert(hit(46).person_index == 3U &&
           hit(46).lane == simtower::OriginalElevatorWaitingLane::second);
    assert(hit(47).person_index == 4U && hit(47).queue_ordinal == 1U);
    assert(hit(49).person_index == 5U && hit(49).queue_ordinal == 2U);
    assert(!hit(50).hit);

    simtower::OriginalTdtTenant owner{};
    owner.left = 10U;
    owner.right = 100U;
    owner.type = 14;
    owner.exact_bytes[0] = std::byte{10};
    owner.exact_bytes[2] = std::byte{100};
    owner.exact_bytes[4] = std::byte{14};
    owner.exact_bytes[12] = std::byte{0};
    floor.tenants.push_back(owner);
    floor.tenant_index[0] = 0U;
    tower.people[0].exact_bytes[0] = std::byte{10};
    tower.people[0].exact_bytes[1] = std::byte{0};
    auto magnifier = simtower::select_original_magnifier_target(
        tower, 37 * 8, 124, 0, 3800);
    assert(magnifier.kind == simtower::OriginalMagnifierTargetKind::
                                  waiting_person_information);
    assert(magnifier.dialog_id == 764U && magnifier.person_index == 0U &&
           magnifier.elevator_index == 1U);
    floor.tenants[0].type = 7;
    floor.tenants[0].exact_bytes[4] = std::byte{7};
    magnifier = simtower::select_original_magnifier_target(
        tower, 37 * 8, 124, 0, 3800);
    assert(magnifier.dialog_id == 763U);

    // Outside both waiting clusters, the final 11f8:0750 leg selects the
    // exact facility-dialog group from the hit tenant.
    magnifier = simtower::select_original_magnifier_target(
        tower, 80 * 8, 124, 0, 3800);
    assert(magnifier.kind ==
           simtower::OriginalMagnifierTargetKind::facility_information);
    assert(magnifier.dialog_id == 748U && magnifier.floor == 10 &&
           magnifier.tenant_index == 0U);

    // The original assumes a 0..39 cursor and a live person pointer. Native
    // bounds checks reject malformed persisted data without selecting an
    // unrelated record.
    left.floor_records[0].exact_bytes[1] = std::byte{40};
    assert(!hit(37).hit);
    left.floor_records[0].exact_bytes[1] = std::byte{39};
    store_u32(left.floor_records[0].exact_bytes, 4U + 39U * 4U, 99U, false);
    assert(!hit(37).hit);
  }

  {
    // Direct 1210:0f0e and 1180:0dcc destination-dispatch coverage. Its
    // 1220:685d/692c/69ae/6aba/6b11 helpers select the family-specific final
    // floor, then 11b0:092f accepts it directly when this Elevator serves it.
    auto tower = make_elevator_passenger_tower();
    auto& elevator = tower.elevators[0];
    auto& person = tower.people[0].exact_bytes;
    const auto expect_direct = [&](std::int16_t floor) {
      elevator.serviced_floors[static_cast<std::size_t>(floor)] =
          std::byte{1};
      const auto selected =
          simtower::select_original_elevator_boarding_destination(
              tower, 0U, 0U, 10, floor > 10);
      assert(selected.status == simtower::
                 OriginalElevatorBoardingDestinationStatus::selected);
      assert(selected.final_destination == floor &&
             selected.car_destination == floor);
      elevator.serviced_floors[static_cast<std::size_t>(floor)] =
          std::byte{0};
    };

    person[0] = std::byte{15};
    person[4] = std::byte{6};
    person[5] = std::byte{0x60};
    expect_direct(15);
    person[5] = std::byte{0x40};
    expect_direct(10);
    person[5] = std::byte{0x45};
    expect_direct(10);  // no encoded parking allocation

    tower.retail[2].exact_bytes[0] = std::byte{18};
    person[4] = std::byte{18};
    person[5] = std::byte{0x41};
    person[6] = std::byte{2};
    expect_direct(18);

    person[4] = std::byte{15};
    person[5] = std::byte{3};
    person[6] = std::byte{24};
    expect_direct(24);
    person[5] = std::byte{4};
    person[0] = std::byte{16};
    expect_direct(16);

    person[4] = std::byte{33};
    person[5] = std::byte{0x60};
    expect_direct(109);
    person[4] = std::byte{36};
    person[5] = std::byte{0x62};
    person[0] = std::byte{20};
    expect_direct(22);

    tower.post_elevator.dbfc_dwords[1] = 17U;
    person[4] = std::byte{7};
    person[5] = std::byte{0x42};
    person[6] = std::byte{1};
    expect_direct(17);

    auto& owner_floor = tower.floors[20];
    owner_floor.tenants.clear();
    owner_floor.tenant_index.fill(0U);
    simtower::OriginalTdtTenant owner{};
    owner.exact_bytes[12] = std::byte{3};
    store_u16(owner.exact_bytes, 6U, 0U, false);
    owner_floor.tenants.push_back(owner);
    owner_floor.tenant_index[3] = 0U;
    tower.post_elevator.dc24_records[0][0] = std::byte{25};
    tower.post_elevator.dc24_records[0][1] = std::byte{26};
    tower.post_elevator.dc24_records[0][7] = std::byte{0};
    person[0] = std::byte{20};
    person[1] = std::byte{3};
    person[4] = std::byte{29};
    person[5] = std::byte{0x60};
    expect_direct(25);

    person[4] = std::byte{6};
    person[5] = std::byte{0x30};
    assert(simtower::select_original_elevator_boarding_destination(
               tower, 0U, 0U, 10, true)
               .status == simtower::OriginalElevatorBoardingDestinationStatus::
                              unsupported_family_state);
    assert(simtower::select_original_elevator_boarding_destination(
               tower, 4U, 0U, 10, true)
               .status == simtower::
                              OriginalElevatorBoardingDestinationStatus::
                                  invalid_person);
  }

  {
    // When the final floor is not directly served, 11b0:092f intersects the
    // destination's route graph with the first db9c transfer record containing
    // this Elevator's high-bit and matching the current travel direction.
    auto tower = make_elevator_passenger_tower();
    auto& elevator = tower.elevators[0];
    auto& person = tower.people[0].exact_bytes;
    person[4] = std::byte{15};
    person[5] = std::byte{3};
    person[6] = std::byte{20};
    store_u32(elevator.block_c2, 20U * 4U, 0x40000000U, false);
    store_u32(tower.post_elevator.db9c_records[0], 0U, 0xc0000000U,
              false);
    tower.post_elevator.db9c_records[0][4] = std::byte{15};
    auto selected = simtower::select_original_elevator_boarding_destination(
        tower, 0U, 0U, 10, true);
    assert(selected.status == simtower::
               OriginalElevatorBoardingDestinationStatus::selected);
    assert(selected.final_destination == 20 &&
           selected.car_destination == 15);
    selected = simtower::select_original_elevator_boarding_destination(
        tower, 0U, 0U, 10, false);
    assert(selected.status ==
           simtower::OriginalElevatorBoardingDestinationStatus::no_route);
  }

  {
    // 1210:0351 consumes an opening-door lane in circular order, applies
    // 11d8:01f1 (including the two-story Lobby discount), and fills the first
    // free parallel person/destination slot through direct 1210:1a3b coverage,
    // plus destination occupancy.
    auto tower = make_elevator_passenger_tower();
    tower.header.frame_time = 200U;
    tower.header.lobby_height = 2U;
    auto& elevator = tower.elevators[0];
    elevator.serviced_floors[15] = std::byte{1};
    auto& queue = elevator.floor_records[0].exact_bytes;
    queue[0] = std::byte{1};
    queue[1] = std::byte{39};
    store_u32(queue, 4U + 39U * 4U, 0U, false);
    auto& person = tower.people[0].exact_bytes;
    person[0] = std::byte{15};
    person[4] = std::byte{6};
    person[5] = std::byte{0x60};
    store_u16(person, 10U, 100U, false);
    store_u16(person, 12U, 0xc014U, false);

    simtower::OriginalPartTable part{};
    const auto result = simtower::step_original_elevator_car_passengers(
        tower, 0U, 0U, part);
    assert(result.status ==
           simtower::OriginalElevatorPassengerStepStatus::transferred);
    assert(result.boarded == 1U && result.rejected == 0U &&
           result.alighted == 0U);
    assert(result.boarding_visual ==
           simtower::OriginalElevatorPassengerVisualEvent(
               {0U, 10, true, true, 0U}));
    assert(queue[0] == std::byte{0} && queue[1] == std::byte{0});
    const auto& car = elevator.car_records[0].exact_bytes;
    assert(car[3] == std::byte{1} && car[12] == std::byte{1});
    assert(car[184] == std::byte{15} && car[226U + 15U] == std::byte{1});
    assert(load_u32(car, 16U, false) == 0U);
    assert(load_u16(person, 10U, false) == 0U);
    assert(load_u16(person, 12U, false) == 0xc05fU);
  }

  {
    // Direct 1210:1332 isolation coverage: the ring advances before family
    // dispatch, the consumed dword becomes the sign-extended projected wait,
    // and the person record is untouched. 11d8:0423 treats a wrapped -1 tick
    // as signed and reduces it to zero at a two-story Lobby.
    auto tower = make_elevator_passenger_tower();
    tower.header.frame_time = 99U;
    tower.header.lobby_height = 2U;
    auto& elevator = tower.elevators[0];
    elevator.serviced_floors[15] = std::byte{1};
    auto& queue = elevator.floor_records[0].exact_bytes;
    queue[0] = std::byte{1};
    queue[1] = std::byte{39};
    store_u32(queue, 4U + 39U * 4U, 0U, false);
    auto& person = tower.people[0].exact_bytes;
    person[0] = std::byte{15};
    person[4] = std::byte{6};
    person[5] = std::byte{0x60};
    store_u16(person, 10U, 100U, false);
    store_u16(person, 12U, 0xc014U, false);
    const auto person_before = person;

    simtower::OriginalPartTable part{};
    const auto result = simtower::step_original_elevator_car_passengers(
        tower, 0U, 0U, part, {}, true);
    assert(result.status ==
           simtower::OriginalElevatorPassengerStepStatus::transferred);
    assert(result.boarded == 1U && result.rejected == 0U &&
           result.family_dispatches.empty() && !result.boarding_visual);
    assert(queue[0] == std::byte{0} && queue[1] == std::byte{0});
    assert(load_u32(queue, 4U + 39U * 4U, false) == 20U);
    assert(person == person_before);
  }

  {
    // The same signed 11d8:0423 boundary applies to normal 11d8:01f1 metric
    // finalization. With no old low-ten-bit wait, -1 discounts to zero rather
    // than wrapping into a huge delay.
    auto tower = make_elevator_passenger_tower();
    tower.header.frame_time = 99U;
    tower.header.lobby_height = 2U;
    auto& elevator = tower.elevators[0];
    elevator.serviced_floors[15] = std::byte{1};
    auto& queue = elevator.floor_records[0].exact_bytes;
    queue[0] = std::byte{1};
    queue[1] = std::byte{39};
    store_u32(queue, 4U + 39U * 4U, 0U, false);
    auto& person = tower.people[0].exact_bytes;
    person[0] = std::byte{15};
    person[4] = std::byte{6};
    person[5] = std::byte{0x60};
    store_u16(person, 10U, 100U, false);
    store_u16(person, 12U, 0xc000U, false);

    simtower::OriginalPartTable part{};
    const auto result = simtower::step_original_elevator_car_passengers(
        tower, 0U, 0U, part);
    assert(result.boarded == 1U);
    assert(load_u16(person, 10U, false) == 0U);
    assert(load_u16(person, 12U, false) == 0xc000U);
  }

  {
    // 1210:07a6 calls 1210:0883 while door state is five; 0883 removes every
    // matching destination, calls its family table, and emits the right-side
    // visual using the direction captured before any boarding reversal.
    auto tower = make_elevator_passenger_tower();
    auto& car = tower.elevators[0].car_records[0].exact_bytes;
    car[2] = std::byte{5};
    car[3] = std::byte{1};
    car[4] = std::byte{0};
    car[12] = std::byte{1};
    car[184] = std::byte{10};
    car[226U + 10U] = std::byte{1};
    store_u32(car, 16U, 0U, false);
    tower.people[0].exact_bytes[4] = std::byte{0};
    simtower::OriginalPartTable part{};
    const auto result = simtower::step_original_elevator_car_passengers(
        tower, 0U, 0U, part);
    assert(result.status ==
           simtower::OriginalElevatorPassengerStepStatus::transferred);
    assert(result.alighted == 1U && result.boarded == 0U);
    assert(result.family_dispatches.empty());
    assert(result.alighting_visual ==
           simtower::OriginalElevatorPassengerVisualEvent(
               {0U, 10, false, false, 0U}));
    assert(car[3] == std::byte{0} && car[12] == std::byte{0});
    assert(car[184] == std::byte{0xff} &&
           load_u32(car, 16U, false) == 0xffffffffU &&
           car[226U + 10U] == std::byte{0});
  }

  {
    // Exact 1210:0883 slot loop: every matching passenger exits in slot order,
    // aggregates reach zero once after the loop, and 10a8:022b sees only the
    // final popped passenger. Type 14 takes the separate 67cf branch and is the
    // sole supported family whose byte 7 is not overwritten with this floor.
    auto tower = make_elevator_passenger_tower();
    auto& car = tower.elevators[0].car_records[0].exact_bytes;
    car[2] = std::byte{5};
    car[3] = std::byte{3};
    car[4] = std::byte{1};
    car[12] = std::byte{1};
    car[184] = std::byte{10};
    car[186] = std::byte{10};
    car[187] = std::byte{10};
    car[226U + 10U] = std::byte{3};
    store_u32(car, 16U, 0U, false);
    store_u32(car, 24U, 1U, false);
    store_u32(car, 28U, 2U, false);
    tower.people[0].exact_bytes[4] = std::byte{14};
    tower.people[0].exact_bytes[5] = std::byte{1};
    tower.people[0].exact_bytes[7] = std::byte{27};
    tower.people[1].exact_bytes[4] = std::byte{0};
    tower.people[1].exact_bytes[7] = std::byte{28};
    tower.people[2].exact_bytes[4] = std::byte{14};
    tower.people[2].exact_bytes[5] = std::byte{1};
    tower.people[2].exact_bytes[7] = std::byte{29};

    simtower::OriginalPartTable part{};
    std::vector<std::size_t> callback_people{};
    std::vector<std::uint8_t> callback_car_counts{};
    const auto result = simtower::step_original_elevator_car_passengers(
        tower, 0U, 0U, part, {}, false, [&](const auto& dispatch) {
          callback_people.push_back(dispatch.person_index);
          callback_car_counts.push_back(
              std::to_integer<std::uint8_t>(car[3]));
        });
    assert(result.status ==
           simtower::OriginalElevatorPassengerStepStatus::transferred);
    assert(result.alighted == 3U && result.boarded == 0U);
    assert(result.family_dispatches.size() == 2U);
    assert(result.family_dispatches[0].person_index == 0U &&
           result.family_dispatches[0].status ==
               simtower::OriginalPersonFamilyDispatchStatus::security);
    assert(result.family_dispatches[1].person_index == 2U &&
           result.family_dispatches[1].status ==
               simtower::OriginalPersonFamilyDispatchStatus::security);
    assert((callback_people == std::vector<std::size_t>{0U, 2U}));
    // Each callback occurs before that slot's aggregate decrement: the
    // intervening unsupported type-zero passenger has already reduced 3->1.
    assert((callback_car_counts == std::vector<std::uint8_t>{3U, 1U}));
    assert(result.alighting_visual ==
           simtower::OriginalElevatorPassengerVisualEvent(
               {0U, 10, false, true, 2U}));
    assert(car[3] == std::byte{0} && car[12] == std::byte{0} &&
           car[226U + 10U] == std::byte{0});
    assert(car[184] == std::byte{0xff} && car[186] == std::byte{0xff} &&
           car[187] == std::byte{0xff});
    assert(load_u32(car, 16U, false) == 0xffffffffU &&
           load_u32(car, 24U, false) == 0xffffffffU &&
           load_u32(car, 28U, false) == 0xffffffffU);
    assert(tower.people[0].exact_bytes[7] == std::byte{27} &&
           tower.people[1].exact_bytes[7] == std::byte{28} &&
           tower.people[2].exact_bytes[7] == std::byte{29});
  }

  {
    // A passenger for which 0f0e finds no family destination still leaves the
    // waiting ring. The original applies 01f1 first, then dd7e/02f7, invokes
    // dispatcher 16ab, and lets this rejected person own the transfer visual.
    auto tower = make_elevator_passenger_tower();
    tower.header.frame_time = 100U;
    auto& queue = tower.elevators[0].floor_records[0].exact_bytes;
    queue[0] = std::byte{1};
    store_u32(queue, 4U, 0U, false);
    auto& person = tower.people[0].exact_bytes;
    person[4] = std::byte{6};
    person[5] = std::byte{0x30};
    store_u16(person, 10U, 100U, false);
    store_u16(person, 12U, 0xc005U, false);
    simtower::OriginalPartTable part{};
    part.words_00_to_40[2] = 7U;
    const auto result = simtower::step_original_elevator_car_passengers(
        tower, 0U, 0U, part);
    assert(result.boarded == 0U && result.rejected == 1U);
    assert(result.family_dispatches.size() == 1U);
    assert(result.family_dispatches[0].source == simtower::
               OriginalPersonFamilyDispatchSource::dispatcher_16ab);
    assert(result.boarding_visual->person_index == 0U);
    assert(queue[0] == std::byte{0} && queue[1] == std::byte{1});
    assert(load_u16(person, 10U, false) == 0U);
    assert(load_u16(person, 12U, false) == 0xc00cU);
  }

  {
    // With no pending car destination, an empty current-direction lane makes
    // 0351 reverse the car and consume the opposite queue.
    auto tower = make_elevator_passenger_tower();
    auto& elevator = tower.elevators[0];
    elevator.serviced_floors[5] = std::byte{1};
    auto& queue = elevator.floor_records[0].exact_bytes;
    queue[2] = std::byte{1};
    queue[3] = std::byte{7};
    store_u32(queue, 164U + 7U * 4U, 0U, false);
    auto& person = tower.people[0].exact_bytes;
    person[0] = std::byte{5};
    person[4] = std::byte{6};
    person[5] = std::byte{0x60};
    simtower::OriginalPartTable part{};
    const auto result = simtower::step_original_elevator_car_passengers(
        tower, 0U, 0U, part);
    assert(result.boarded == 1U &&
           !result.boarding_visual->direction_up);
    const auto& car = elevator.car_records[0].exact_bytes;
    assert(car[4] == std::byte{0} && car[184] == std::byte{5});
    assert(queue[2] == std::byte{0} && queue[3] == std::byte{8});
  }

  {
    // Nonzero car byte 14 is the special-service path: both lanes may board
    // in one state-one cycle, and 10a8:022b retains only the last visual.
    auto tower = make_elevator_passenger_tower();
    auto& elevator = tower.elevators[0];
    elevator.capacity = 2U;
    elevator.serviced_floors[5] = std::byte{1};
    elevator.serviced_floors[15] = std::byte{1};
    auto& car = elevator.car_records[0].exact_bytes;
    car[14] = std::byte{1};
    auto& queue = elevator.floor_records[0].exact_bytes;
    queue[0] = std::byte{1};
    queue[1] = std::byte{39};
    store_u32(queue, 4U + 39U * 4U, 0U, false);
    queue[2] = std::byte{1};
    queue[3] = std::byte{4};
    store_u32(queue, 164U + 4U * 4U, 1U, false);
    tower.people[0].exact_bytes[0] = std::byte{15};
    tower.people[1].exact_bytes[0] = std::byte{5};
    for (std::size_t index = 0U; index < 2U; ++index) {
      tower.people[index].exact_bytes[4] = std::byte{6};
      tower.people[index].exact_bytes[5] = std::byte{0x60};
    }
    simtower::OriginalPartTable part{};
    const auto result = simtower::step_original_elevator_car_passengers(
        tower, 0U, 0U, part);
    assert(result.boarded == 2U && result.rejected == 0U);
    assert(result.boarding_visual ==
           simtower::OriginalElevatorPassengerVisualEvent(
               {0U, 10, true, false, 1U}));
    assert(car[3] == std::byte{2} && car[12] == std::byte{2});
    assert(car[226U + 5U] == std::byte{1} &&
           car[226U + 15U] == std::byte{1});
  }

  {
    // Native guards reject inconsistent persisted car aggregates before the
    // exact original mutation order can underflow them.
    auto tower = make_elevator_passenger_tower();
    auto& car = tower.elevators[0].car_records[0].exact_bytes;
    car[2] = std::byte{5};
    car[226U + 10U] = std::byte{1};
    const auto before = car;
    simtower::OriginalPartTable part{};
    const auto result = simtower::step_original_elevator_car_passengers(
        tower, 0U, 0U, part);
    assert(result.status ==
           simtower::OriginalElevatorPassengerStepStatus::malformed_state);
    assert(car == before);
  }

  {
    // All passenger dwords and wait words follow the TDT's revision-aware
    // byte order; the parallel byte fields remain unchanged.
    auto tower = make_elevator_passenger_tower(true);
    tower.header.frame_time = 150U;
    auto& elevator = tower.elevators[0];
    elevator.serviced_floors[15] = std::byte{1};
    auto& queue = elevator.floor_records[0].exact_bytes;
    queue[0] = std::byte{1};
    queue[1] = std::byte{39};
    store_u32(queue, 4U + 39U * 4U, 0U, true);
    auto& person = tower.people[0].exact_bytes;
    person[0] = std::byte{15};
    person[4] = std::byte{6};
    person[5] = std::byte{0x60};
    store_u16(person, 10U, 100U, true);
    store_u16(person, 12U, 0x8014U, true);
    simtower::OriginalPartTable part{};
    const auto result = simtower::step_original_elevator_car_passengers(
        tower, 0U, 0U, part);
    assert(result.boarded == 1U);
    const auto& car = elevator.car_records[0].exact_bytes;
    assert(load_u32(car, 16U, true) == 0U && car[184] == std::byte{15});
    assert(load_u16(person, 10U, true) == 0U &&
           load_u16(person, 12U, true) == 0x8046U);
  }

  {
    // Direct 1090:06fb coverage, including 1090:209f class zero and
    // 1090:10e4: a one-floor approach moves once, arms the five-tick settle
    // counter, and consumes the one-shot WAVE/0x1772 latch.
    auto tower = make_elevator_passenger_tower();
    auto& car = tower.elevators[0].car_records[0].exact_bytes;
    car[2] = std::byte{0};
    car[0] = std::byte{10};
    car[5] = std::byte{11};
    car[6] = std::byte{10};
    car[7] = std::byte{1};
    auto step =
        simtower::step_original_elevator_car_state(tower, 0U, 0U);
    assert(step.status == simtower::OriginalElevatorCarStepStatus::moved);
    assert(step.floor_before == 10 && step.floor_after == 11 &&
           step.motion_class == 0U && step.movement_sound_requested);
    assert(car[0] == std::byte{11} && car[1] == std::byte{5} &&
           car[7] == std::byte{0});

    step = simtower::step_original_elevator_car_state(tower, 0U, 0U);
    assert(step.status ==
           simtower::OriginalElevatorCarStepStatus::countdown_advanced);
    assert(step.motion_class == 0U && car[1] == std::byte{4} &&
           car[0] == std::byte{11});
  }

  {
    // Type-zero Express cars use class three only when both target and prior
    // stop are over four floors away, producing the original three-floor hop.
    auto tower = make_elevator_passenger_tower();
    auto& elevator = tower.elevators[0];
    elevator.type = 0U;
    auto& car = elevator.car_records[0].exact_bytes;
    car[2] = std::byte{0};
    car[0] = std::byte{10};
    car[5] = std::byte{20};
    car[6] = std::byte{0};
    car[7] = std::byte{1};
    bool movement_sound_called = false;
    const auto step = simtower::step_original_elevator_car_state(
        tower, 0U, 0U, [&] {
          movement_sound_called = true;
          assert(car[7] == std::byte{1});
        });
    assert(step.status == simtower::OriginalElevatorCarStepStatus::moved);
    assert(step.movement_sound_requested && movement_sound_called &&
           car[7] == std::byte{0});
    assert(step.motion_class == 3U && step.floor_after == 13);
    assert(car[0] == std::byte{13} && car[1] == std::byte{0});
  }

  {
    // At-target, non-full cars enter door state five. 13cc releases the
    // current up owner, word 8 records the first arrival tick, and byte 7
    // latches until the next movement sound.
    auto tower = make_elevator_passenger_tower();
    tower.header.frame_time = 321U;
    auto& elevator = tower.elevators[0];
    auto& car = elevator.car_records[0].exact_bytes;
    car[2] = std::byte{0};
    car[5] = std::byte{10};
    elevator.block_2a2[10] = std::byte{1};
    store_u16(car, 10U, 1U, false);
    const auto step =
        simtower::step_original_elevator_car_state(tower, 0U, 0U);
    assert(step.status ==
           simtower::OriginalElevatorCarStepStatus::doors_opened);
    assert(car[2] == std::byte{5} && car[7] == std::byte{1});
    assert(load_u16(car, 8U, false) == 321U);
    assert(elevator.block_2a2[10] == std::byte{0});
    assert(load_u16(car, 10U, false) == 0U);
  }

  {
    // Door state one reaches zero, recomputes the target, and reopens while
    // exact 1090:23a5's home/special-floor dwell threshold has not elapsed.
    // Beyond the signed schedule value times thirty, it remains closed.
    auto tower = make_elevator_passenger_tower();
    auto& elevator = tower.elevators[0];
    elevator.car_home_floors[0] = std::byte{10};
    elevator.schedule[42] = std::byte{5};
    auto& car = elevator.car_records[0].exact_bytes;
    car[2] = std::byte{1};
    car[5] = std::byte{10};
    tower.header.frame_time = 100U;
    store_u16(car, 8U, 100U, false);
    auto step =
        simtower::step_original_elevator_car_state(tower, 0U, 0U);
    assert(step.status ==
           simtower::OriginalElevatorCarStepStatus::door_advanced);
    assert(car[2] == std::byte{1});

    tower.header.frame_time = 251U;
    step = simtower::step_original_elevator_car_state(tower, 0U, 0U);
    assert(step.status ==
           simtower::OriginalElevatorCarStepStatus::door_advanced);
    assert(car[2] == std::byte{0});

    // 2461-246d's 16-bit CWD/XOR/SUB leaves abs(0x8000) as signed -32768;
    // the following signed JLE therefore reopens instead of departing.
    car[2] = std::byte{1};
    car[5] = std::byte{10};
    tower.header.frame_time = 0U;
    store_u16(car, 8U, 0x8000U, false);
    step = simtower::step_original_elevator_car_state(tower, 0U, 0U);
    assert(step.status ==
           simtower::OriginalElevatorCarStepStatus::door_advanced);
    assert(car[2] == std::byte{1});
  }

  {
    // A full car with no occupant for its nominal target skips opening,
    // recomputes the next occupied/assigned target, and continues motion.
    auto tower = make_elevator_passenger_tower();
    auto& elevator = tower.elevators[0];
    auto& car = elevator.car_records[0].exact_bytes;
    car[2] = std::byte{0};
    car[3] = std::byte{4};
    car[5] = std::byte{10};
    car[6] = std::byte{5};
    car[12] = std::byte{1};
    car[226U + 15U] = std::byte{4};
    const auto step =
        simtower::step_original_elevator_car_state(tower, 0U, 0U);
    assert(step.status == simtower::OriginalElevatorCarStepStatus::moved);
    assert(car[2] == std::byte{0} && car[0] == std::byte{11} &&
           car[5] == std::byte{15});
  }

  {
    // Direct 1090:12c9/151c coverage. A moving car releases only owner bytes
    // equal to its one-based car number; a nonzero mode byte releases both
    // lanes and decrements the selected car's word-10 count once per released
    // lane before recomputing that car's route state.
    auto tower = make_elevator_passenger_tower();
    auto& elevator = tower.elevators[0];
    auto& car = elevator.car_records[0].exact_bytes;
    car[2] = std::byte{0};
    car[5] = std::byte{15};
    car[6] = std::byte{0};
    car[14] = std::byte{1};
    elevator.block_2a2[10] = std::byte{2};
    elevator.block_31a[10] = std::byte{1};
    store_u16(car, 10U, 1U, false);
    const auto step =
        simtower::step_original_elevator_car_state(tower, 0U, 0U);
    assert(step.status == simtower::OriginalElevatorCarStepStatus::moved);
    assert(elevator.block_2a2[10] == std::byte{2});
    assert(elevator.block_31a[10] == std::byte{0});
    assert(load_u16(car, 10U, false) == 0U);
  }

  {
    // Departure snapshots an unowned old-floor queue before moving, then
    // 0a4c assigns its lane and recomputes the selected car afterward.
    auto tower = make_elevator_passenger_tower();
    auto& elevator = tower.elevators[0];
    elevator.schedule.fill(std::byte{5});
    elevator.car_home_floors[0] = std::byte{0};
    auto& car = elevator.car_records[0].exact_bytes;
    car[2] = std::byte{0};
    car[5] = std::byte{15};
    car[6] = std::byte{0};
    auto& queue = elevator.floor_records[0].exact_bytes;
    queue[0] = std::byte{1};
    const auto step =
        simtower::step_original_elevator_car_state(tower, 0U, 0U);
    assert(step.status == simtower::OriginalElevatorCarStepStatus::moved);
    assert(step.assignments_created == 1U);
    assert(elevator.block_2a2[10] == std::byte{1});
    assert(load_u16(car, 10U, false) == 1U);
  }

  {
    // 1090:03ab opens all cars first, then the same frame's 07a6/0351 pair
    // alights and boards. 10a8:02aa consumes the boarding slot before the
    // alighting slot for the same floor/Elevator pair.
    auto tower = make_elevator_passenger_tower();
    tower.header.frame_time = 100U;
    auto& elevator = tower.elevators[0];
    elevator.serviced_floors[15] = std::byte{1};
    auto& car = elevator.car_records[0].exact_bytes;
    car[2] = std::byte{0};
    car[5] = std::byte{10};
    car[3] = std::byte{1};
    car[12] = std::byte{1};
    car[184] = std::byte{10};
    car[226U + 10U] = std::byte{1};
    store_u32(car, 16U, 0U, false);
    tower.people[0].exact_bytes[4] = std::byte{0};
    auto& waiting = tower.people[1].exact_bytes;
    waiting[0] = std::byte{15};
    waiting[4] = std::byte{6};
    waiting[5] = std::byte{0x60};
    store_u16(waiting, 10U, 100U, false);
    auto& queue = elevator.floor_records[0].exact_bytes;
    queue[0] = std::byte{1};
    store_u32(queue, 4U, 1U, false);
    simtower::OriginalPartTable part{};
    const auto frame =
        simtower::step_original_elevator_frame(tower, part);
    assert(frame.elevators_scanned == 1U && frame.cars_scanned == 1U &&
           frame.cars_changed == 1U && frame.changed);
    assert(frame.alighted == 1U && frame.boarded == 1U &&
           frame.rejected == 0U);
    const std::vector expected_visuals = {
        simtower::OriginalElevatorPassengerVisualEvent(
            {0U, 10, true, true, 1U}),
        simtower::OriginalElevatorPassengerVisualEvent(
            {0U, 10, false, true, 0U}),
    };
    assert(frame.transfer_visuals == expected_visuals);
    assert(car[3] == std::byte{1} && car[12] == std::byte{1});
    assert(car[226U + 10U] == std::byte{0} &&
           car[226U + 15U] == std::byte{1});
  }

  {
    // 10a8:0000 gives every floor a 24-Elevator x-sorted table, and 022b
    // owns two cache dwords for every table slot. Distinct Elevator/floor
    // events therefore coexist; only a later car using the same
    // floor/Elevator/side key overwrites an earlier event.
    auto tower = make_elevator_passenger_tower();
    const auto elevator_template = tower.elevators[0];
    tower.people_count = 7U;
    tower.people.resize(7U);
    for (auto& person : tower.people) person.exact_bytes.fill(std::byte{0});

    const auto configure = [&](std::size_t elevator_index,
                               std::int16_t floor,
                               std::uint16_t x,
                               std::size_t alighting_person,
                               std::size_t boarding_person) {
      auto& elevator = tower.elevators[elevator_index];
      elevator = elevator_template;
      elevator.x = x;
      elevator.serviced_floors.fill(std::byte{0});
      elevator.serviced_floors[static_cast<std::size_t>(floor + 5)] =
          std::byte{1};
      elevator.floor_records[0].floor = static_cast<std::int8_t>(floor);
      elevator.floor_records[0].mapped_index =
          simtower::original_elevator_floor_record_index(
              elevator.type, elevator.bottom_floor, elevator.top_floor,
              floor);
      elevator.floor_records[0].exact_bytes.fill(std::byte{0});
      auto& queue = elevator.floor_records[0].exact_bytes;
      queue[0] = std::byte{1};
      store_u32(queue, 4U, static_cast<std::uint32_t>(boarding_person),
                false);

      auto& car = elevator.car_records[0].exact_bytes;
      car[0] = static_cast<std::byte>(floor);
      car[2] = std::byte{0};
      car[3] = std::byte{1};
      car[4] = std::byte{1};
      car[5] = static_cast<std::byte>(floor);
      car[12] = std::byte{1};
      car[184] = static_cast<std::byte>(floor);
      car[226U + static_cast<std::size_t>(floor)] = std::byte{1};
      store_u32(car, 16U, static_cast<std::uint32_t>(alighting_person),
                false);

      tower.people[alighting_person].exact_bytes[4] = std::byte{0};
      auto& waiting = tower.people[boarding_person].exact_bytes;
      waiting[0] = static_cast<std::byte>(floor + 5);
      waiting[4] = std::byte{6};
      waiting[5] = std::byte{0x60};
    };
    configure(0U, 10, 160U, 0U, 1U);
    configure(1U, 10, 100U, 2U, 3U);
    configure(2U, 15, 130U, 4U, 5U);

    // A later active car at Elevator 0/floor 10 replaces only that pair's
    // alighting slot; all five other cache slots survive.
    auto& later_car = tower.elevators[0].car_records[1].exact_bytes;
    later_car = tower.elevators[0].car_records[0].exact_bytes;
    store_u32(later_car, 16U, 6U, false);
    tower.people[6].exact_bytes[4] = std::byte{0};

    // 1090:04c0-0542 is per shaft, not two global passes: Elevator 0's
    // passenger mutation must be visible at its 053d checkpoint while
    // Elevator 1's passenger is still untouched. Each used Elevator produces
    // exactly one checkpoint after both of its inner car loops.
    const auto first_boarding_before = tower.people[1].exact_bytes;
    const auto second_boarding_before = tower.people[3].exact_bytes;
    std::size_t checkpoint_count = 0U;
    const simtower::OriginalPartTable part{};
    const auto frame = simtower::step_original_elevator_frame(
        tower, part, {}, false, simtower::OriginalElevatorFrameHostHooks{
        {}, {}, [&] {
          if (checkpoint_count == 0U) {
            assert(tower.people[1].exact_bytes != first_boarding_before);
            assert(tower.people[3].exact_bytes == second_boarding_before);
          } else if (checkpoint_count == 1U) {
            assert(tower.people[3].exact_bytes != second_boarding_before);
          }
          ++checkpoint_count;
        }});
    assert(checkpoint_count == 3U);
    assert(frame.elevators_scanned == 3U && frame.cars_scanned == 4U);
    assert(frame.alighted == 4U && frame.boarded == 3U &&
           frame.rejected == 0U);
    const std::vector expected_visuals = {
        simtower::OriginalElevatorPassengerVisualEvent(
            {2U, 15, true, true, 5U}),
        simtower::OriginalElevatorPassengerVisualEvent(
            {2U, 15, false, true, 4U}),
        simtower::OriginalElevatorPassengerVisualEvent(
            {1U, 10, true, true, 3U}),
        simtower::OriginalElevatorPassengerVisualEvent(
            {1U, 10, false, true, 2U}),
        simtower::OriginalElevatorPassengerVisualEvent(
            {0U, 10, true, true, 1U}),
        simtower::OriginalElevatorPassengerVisualEvent(
            {0U, 10, false, true, 6U}),
    };
    assert(frame.transfer_visuals == expected_visuals);
  }

  return 0;
}
