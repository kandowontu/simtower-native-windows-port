#include "original_people.hpp"

#include "original_simulation.hpp"
#include "original_tables.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace simtower {
namespace {

constexpr std::uint16_t byte_swap(std::uint16_t value) noexcept {
  return static_cast<std::uint16_t>((value << 8U) | (value >> 8U));
}

constexpr std::uint32_t byte_swap(std::uint32_t value) noexcept {
  return ((value & 0x000000ffU) << 24U) |
         ((value & 0x0000ff00U) << 8U) |
         ((value & 0x00ff0000U) >> 8U) |
         ((value & 0xff000000U) >> 24U);
}

std::uint16_t load_u16(std::span<const std::byte> bytes,
                       std::size_t offset,
                       bool byte_swapped) noexcept {
  std::uint16_t value =
      static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
      static_cast<std::uint16_t>(
          std::to_integer<std::uint8_t>(bytes[offset + 1U]) << 8U);
  return byte_swapped ? byte_swap(value) : value;
}

std::uint32_t load_u32(std::span<const std::byte> bytes,
                       std::size_t offset,
                       bool byte_swapped) noexcept {
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
  return byte_swapped ? byte_swap(value) : value;
}

void store_u16(std::span<std::byte> bytes,
               std::size_t offset,
               std::uint16_t value,
               bool byte_swapped) noexcept {
  if (byte_swapped) {
    value = byte_swap(value);
  }
  bytes[offset] = static_cast<std::byte>(value);
  bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

void store_u32(std::span<std::byte> bytes,
               std::size_t offset,
               std::uint32_t value,
               bool byte_swapped) noexcept {
  if (byte_swapped) {
    value = byte_swap(value);
  }
  bytes[offset] = static_cast<std::byte>(value);
  bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
  bytes[offset + 2U] = static_cast<std::byte>(value >> 16U);
  bytes[offset + 3U] = static_cast<std::byte>(value >> 24U);
}

OriginalTdtTenant* find_original_tenant(OriginalTdtDocument& document,
                                        std::uint8_t floor_number,
                                        std::uint8_t key) noexcept {
  if (floor_number >= document.floors.size() ||
      key >= OriginalTdtFloor::kIndexCapacity) {
    return nullptr;
  }
  auto& floor = document.floors[floor_number];
  const std::size_t index = floor.tenant_index[key];
  if (index >= floor.tenants.size() ||
      floor.tenants[index].exact_bytes[12] != static_cast<std::byte>(key)) {
    return nullptr;
  }
  return &floor.tenants[index];
}

const OriginalTdtTenant* find_original_tenant(
    const OriginalTdtDocument& document,
    std::uint8_t floor_number,
    std::uint8_t key) noexcept {
  if (floor_number >= document.floors.size() ||
      key >= OriginalTdtFloor::kIndexCapacity) {
    return nullptr;
  }
  const auto& floor = document.floors[floor_number];
  const std::size_t index = floor.tenant_index[key];
  if (index >= floor.tenants.size() ||
      floor.tenants[index].exact_bytes[12] != static_cast<std::byte>(key)) {
    return nullptr;
  }
  return &floor.tenants[index];
}

void mark_original_tenant_changed(OriginalTdtTenant& tenant) noexcept {
  tenant.exact_bytes[13] = std::byte{1};
  tenant.preserved_07_to_0f[6] = std::byte{1};
}

void set_original_tenant_status(OriginalTdtTenant& tenant,
                                std::uint8_t status) noexcept {
  tenant.status = status;
  tenant.exact_bytes[5] = static_cast<std::byte>(status);
}

bool is_original_hotel_type(std::int8_t type) noexcept {
  return type >= 3 && type <= 5;
}

void set_original_hotel_pair_state(OriginalTdtTenant& tenant,
                                   std::uint8_t status) noexcept {
  set_original_tenant_status(tenant, status);
  tenant.exact_bytes[15] = std::byte{0xff};
  tenant.preserved_07_to_0f[8] = std::byte{0xff};
  tenant.exact_bytes[14] = std::byte{0};
  tenant.preserved_07_to_0f[7] = std::byte{0};
  mark_original_tenant_changed(tenant);
}

template <std::size_t Size>
std::size_t remove_original_person_links_by_type(
    OriginalTdtDocument& document,
    const std::array<std::int8_t, Size>& types) noexcept {
  auto& links = document.post_elevator.dce4_person_indices;
  auto& count = document.header.person_link_count;
  const std::size_t bounded_count =
      std::min<std::size_t>(count, links.size());
  count = static_cast<std::uint16_t>(bounded_count);

  std::size_t removed = 0;
  std::size_t index = 0;
  while (index < count) {
    const auto person_index = links[index];
    bool matches = false;
    if (person_index >= 0 &&
        static_cast<std::size_t>(person_index) < document.people.size()) {
      const auto type = std::bit_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(
              document.people[static_cast<std::size_t>(person_index)]
                  .exact_bytes[4]));
      matches = std::find(types.begin(), types.end(), type) != types.end();
    }
    if (!matches) {
      ++index;
      continue;
    }

    for (std::size_t source = index + 1U; source < count; ++source) {
      links[source - 1U] = links[source];
    }
    if (index < document.person_link_names.size()) {
      document.person_link_names.erase(
          document.person_link_names.begin() +
          static_cast<std::ptrdiff_t>(index));
    }
    --count;
    links[count] = -1;
    ++removed;
    // 1188:09e9/0a85 retries the same slot after 0793 shifts the array.
  }
  return removed;
}

std::size_t original_header_runtime_offset(
    const OriginalTdtDocument& document,
    std::size_t version_20_offset) noexcept {
  std::size_t offset = version_20_offset -
                       (document.header.format_version >= 0x20U ? 0U : 2U);
  if (version_20_offset >= 60U &&
      document.header.format_version < 0x23U) {
    offset -= 2U;
  }
  return offset;
}

std::uint16_t load_original_header_word(
    const OriginalTdtDocument& document,
    std::size_t version_20_offset) noexcept {
  const auto offset =
      original_header_runtime_offset(document, version_20_offset);
  if (offset + 2U > document.header.exact_bytes.size()) {
    return 0U;
  }
  return load_u16(std::span<const std::byte>(document.header.exact_bytes),
                  offset, document.header.byte_swapped);
}

void store_original_header_word(OriginalTdtDocument& document,
                                std::size_t version_20_offset,
                                std::uint16_t value) noexcept {
  const auto offset =
      original_header_runtime_offset(document, version_20_offset);
  if (offset + 2U <= document.header.exact_bytes.size()) {
    store_u16(std::span<std::byte>(document.header.exact_bytes), offset,
              value, document.header.byte_swapped);
  }
}

void store_original_header_dword(OriginalTdtDocument& document,
                                 std::size_t version_20_offset,
                                 std::uint32_t value) noexcept {
  const auto offset =
      original_header_runtime_offset(document, version_20_offset);
  if (offset + 4U <= document.header.exact_bytes.size()) {
    store_u32(std::span<std::byte>(document.header.exact_bytes), offset,
              value, document.header.byte_swapped);
  }
}

std::uint8_t original_recycling_population_phase(
    const OriginalTdtDocument& document,
    std::int16_t center_count) noexcept {
  // 1088:0250 uses signed IDIV and then six half-open 500-person bands.
  const std::int32_t per_center =
      document.post_elevator.finance.total_population / center_count;
  if (per_center < 500) return 1U;
  if (per_center < 1000) return 2U;
  if (per_center < 1500) return 3U;
  if (per_center < 2000) return 4U;
  if (per_center < 2500) return 5U;
  return 6U;
}

void apply_original_person_common_update(OriginalTdtPersonRecord& person,
                                         std::uint16_t movement_delta,
                                         bool byte_swapped) noexcept {
  auto exact = std::span<std::byte>(person.exact_bytes);

  // 11d8:00fc preserves word 12's upper six bits and replaces its lower ten
  // with signed min(low + DS:b3de - word10, 300), using 16-bit arithmetic.
  const std::uint16_t old_word12 = load_u16(exact, 12, byte_swapped);
  const std::uint16_t word10 = load_u16(exact, 10, byte_swapped);
  const auto difference = static_cast<std::int16_t>(
      static_cast<std::uint16_t>((old_word12 & 0x03ffU) + movement_delta -
                                 word10));
  const std::uint16_t bounded =
      difference >= 300 ? 300U : static_cast<std::uint16_t>(difference);
  store_u16(exact, 12,
            static_cast<std::uint16_t>((old_word12 & 0xfc00U) + bounded),
            byte_swapped);
  store_u16(exact, 10, 0, byte_swapped);

  // 11d8:0000 advances the byte counter, accumulates the lower-ten-bit
  // distance into word 14, and clears that distance while retaining flags.
  exact[9] = static_cast<std::byte>(
      std::to_integer<std::uint8_t>(exact[9]) + 1U);
  const std::uint16_t word12 = load_u16(exact, 12, byte_swapped);
  const std::uint16_t word14 = load_u16(exact, 14, byte_swapped);
  store_u16(exact, 14,
            static_cast<std::uint16_t>(word14 + (word12 & 0x03ffU)),
            byte_swapped);
  store_u16(exact, 10, 0, byte_swapped);
  store_u16(exact, 12, word12 & 0xfc00U, byte_swapped);
}

void decrement_original_office_population(OriginalTdtDocument& document) {
  // 1060:08be maps type 7 to category zero. 1198:00a9 then decrements the
  // first b846 category dword and its following total with x86 wraparound.
  auto& category = document.post_elevator.b846_series[0][0];
  auto& total = document.post_elevator.b846_series[0][10];
  category = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(category) - 1U);
  total = static_cast<std::int32_t>(static_cast<std::uint32_t>(total) - 1U);
}

bool increment_original_parking_population(
    OriginalTdtDocument& document,
    std::int8_t facility_type) noexcept {
  // Exact persisted accounting from 1198:002f after its floor/key lookup has
  // resolved the owning tenant type: enforce the category ceiling, then
  // increment both that category and the shared total.
  const auto category = original_finance_category_for_type(
      static_cast<std::uint16_t>(static_cast<std::uint8_t>(facility_type)));
  if (category < 0) return false;
  const auto index = static_cast<std::size_t>(category);
  auto& current = document.post_elevator.b846_series[0][index];
  const auto maximum = document.post_elevator.b846_series[1][index];
  if (current >= maximum) return false;
  current = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(current) + 1U);
  auto& total = document.post_elevator.b846_series[0][10U];
  total = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(total) + 1U);
  return true;
}

void decrement_original_parking_population(
    OriginalTdtDocument& document,
    std::int8_t facility_type) noexcept {
  const auto category = original_finance_category_for_type(
      static_cast<std::uint16_t>(static_cast<std::uint8_t>(facility_type)));
  if (category < 0) return;
  auto& current = document.post_elevator.b846_series[0]
      [static_cast<std::size_t>(category)];
  current = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(current) - 1U);
  auto& total = document.post_elevator.b846_series[0][10U];
  total = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(total) - 1U);
}

OriginalOfficePersonStepStatus remove_original_office_presence(
    OriginalTdtDocument& document,
    std::size_t person_index,
    OriginalTdtPersonRecord& person) {
  auto exact = std::span<std::byte>(person.exact_bytes);
  if ((std::to_integer<std::uint8_t>(exact[13]) & 0xfcU) == 0U) {
    return OriginalOfficePersonStepStatus::left_office;
  }

  auto* home = find_original_tenant(
      document, std::to_integer<std::uint8_t>(exact[0]),
      std::to_integer<std::uint8_t>(exact[1]));
  if (!home || home->type != 7) {
    return OriginalOfficePersonStepStatus::malformed_tenant_link;
  }
  decrement_original_office_population(document);
  store_u16(exact, 12, load_u16(exact, 12, document.header.byte_swapped) &
                           0x03ffU,
            document.header.byte_swapped);

  const auto connected = document.post_elevator.parking_connected;
  if (connected < 0) {
    return OriginalOfficePersonStepStatus::left_office;
  }
  if (static_cast<std::size_t>(connected) >
      document.post_elevator.cf9c_records.size()) {
    return OriginalOfficePersonStepStatus::malformed_route_table;
  }
  for (std::size_t index = 0; index < static_cast<std::size_t>(connected);
       ++index) {
    auto& record = document.post_elevator.cf9c_records[index];
    if (record[0] == std::byte{0xff} ||
        load_u32(record, 2, document.header.byte_swapped) != person_index) {
      continue;
    }
    const auto route_floor = std::to_integer<std::uint8_t>(record[0]);
    const auto route_key = std::to_integer<std::uint8_t>(record[1]);
    if (route_key == 0xffU) {
      continue;
    }
    auto* route_tenant =
        find_original_tenant(document, route_floor, route_key);
    if (!route_tenant) {
      return OriginalOfficePersonStepStatus::malformed_tenant_link;
    }
    if (route_tenant->status != 1U) {
      set_original_tenant_status(*route_tenant, 0U);
      mark_original_tenant_changed(*route_tenant);
    }
    store_u32(record, 2, 0, document.header.byte_swapped);
    break;
  }
  return OriginalOfficePersonStepStatus::left_office;
}

std::int16_t signed_byte(std::byte value) noexcept {
  return std::bit_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(value));
}

std::uint32_t original_tenant_people_start(
    const OriginalTdtTenant& tenant,
    bool byte_swapped) noexcept {
  return load_u32(tenant.exact_bytes, 8U, byte_swapped);
}

std::size_t original_person_span(std::int8_t type) noexcept {
  // Literal 1228:07c5 table for types 3..40.
  switch (type) {
    case 3:
      return 2U;
    case 4:
    case 5:
    case 9:
      return 3U;
    case 6:
    case 10:
    case 12:
      return 48U;
    case 18:
    case 19:
    case 34:
    case 35:
      return 56U;
    case 29:
    case 30:
      return 40U;
    case 33:
      return 240U;
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
      return 8U;
    default:
      return 6U;
  }
}

bool original_floor_in_elevator_range(std::int16_t floor) noexcept {
  return floor >= 0 && floor < 120;
}

std::uint8_t original_elevator_occupancy(
    const OriginalTdtElevatorCarRecord& car,
    std::int16_t floor) noexcept {
  if (!original_floor_in_elevator_range(floor)) return 0U;
  return std::to_integer<std::uint8_t>(
      car.exact_bytes[226U + static_cast<std::size_t>(floor)]);
}

std::int16_t original_elevator_assignment(
    const std::array<std::byte, 120>& assignments,
    std::int16_t floor) noexcept {
  if (!original_floor_in_elevator_range(floor)) return 0;
  return signed_byte(assignments[static_cast<std::size_t>(floor)]);
}

std::int16_t select_original_elevator_car_target(
    const OriginalTdtElevator& elevator,
    std::size_t car_index,
    bool byte_swapped) noexcept {
  // Exact 1090:1553 primary car-target selector, including express modes,
  // full-car request suppression, and the four directional scan bands.
  if (car_index >= elevator.car_records.size()) return -1;
  const auto& car = elevator.car_records[car_index];
  const auto& exact = car.exact_bytes;
  const auto current = signed_byte(exact[0]);
  const auto direction = signed_byte(exact[4]);
  const auto passenger_count = signed_byte(exact[3]);
  const auto capacity = static_cast<std::int16_t>(elevator.capacity);
  const auto top = static_cast<std::int16_t>(elevator.top_floor);
  const auto bottom = static_cast<std::int16_t>(elevator.bottom_floor);
  const auto mode = signed_byte(exact[14]);
  const auto assigned_car = static_cast<std::int16_t>(car_index + 1U);
  const bool full = passenger_count == capacity;

  if (load_u16(exact, 10U, byte_swapped) == 0U &&
      exact[12] == std::byte{0}) {
    return signed_byte(elevator.car_home_floors[car_index]);
  }

  const auto occupied = [&](std::int16_t floor) {
    return original_elevator_occupancy(car, floor) != 0U;
  };
  const auto assigned_up = [&](std::int16_t floor) {
    return original_elevator_assignment(elevator.block_2a2, floor) ==
           assigned_car;
  };
  const auto assigned_down = [&](std::int16_t floor) {
    return original_elevator_assignment(elevator.block_31a, floor) ==
           assigned_car;
  };

  if (mode == 1) {
    if (direction != 0) {
      if (current != top || exact[1] != std::byte{0}) return top;
    } else if (current == bottom && exact[1] == std::byte{0}) {
      return top;
    }
    for (auto floor = current; floor >= bottom; --floor) {
      if (occupied(floor) ||
          (!full && (assigned_down(floor) || assigned_up(floor)))) {
        return floor;
      }
    }
    return bottom;
  }

  if (mode == 2) {
    if (direction == 0) {
      if (current != bottom || exact[1] != std::byte{0}) return bottom;
    } else if (current == top && exact[1] == std::byte{0}) {
      return bottom;
    }
    for (auto floor = current; floor <= top; ++floor) {
      if (occupied(floor) ||
          (!full && (assigned_up(floor) || assigned_down(floor)))) {
        return floor;
      }
    }
    return top;
  }

  if (direction != 0) {
    for (auto floor = current; floor <= top; ++floor) {
      if (occupied(floor) || (!full && assigned_up(floor))) return floor;
    }
    if (!full) {
      for (auto floor = top; floor >= current; --floor) {
        if (assigned_down(floor)) return floor;
      }
    }
    for (auto floor = static_cast<std::int16_t>(current - 1);
         floor >= bottom; --floor) {
      if ((!full && assigned_down(floor)) || occupied(floor)) return floor;
    }
    if (!full) {
      for (auto floor = bottom; floor < current; ++floor) {
        if (assigned_up(floor)) return floor;
      }
    }
    return -1;
  }

  for (auto floor = current; floor >= bottom; --floor) {
    if (occupied(floor) || (!full && assigned_down(floor))) return floor;
  }
  if (!full) {
    for (auto floor = bottom; floor <= current; ++floor) {
      if (assigned_up(floor)) return floor;
    }
  }
  for (auto floor = static_cast<std::int16_t>(current + 1);
       floor <= top; ++floor) {
    if ((!full && assigned_up(floor)) || occupied(floor)) return floor;
  }
  if (!full) {
    for (auto floor = top; floor > current; --floor) {
      if (assigned_down(floor)) return floor;
    }
  }
  return -1;
}

std::int16_t select_original_elevator_car_secondary_target(
    const OriginalTdtElevator& elevator,
    std::size_t car_index) noexcept {
  // Exact 1090:1f4c secondary-target scan: upward cars search top-to-current,
  // downward cars bottom-to-current, and an empty scan returns the home floor.
  if (car_index >= elevator.car_records.size()) return -1;
  const auto& car = elevator.car_records[car_index];
  const auto current = signed_byte(car.exact_bytes[0]);
  const auto direction = signed_byte(car.exact_bytes[4]);
  const auto top = static_cast<std::int16_t>(elevator.top_floor);
  const auto bottom = static_cast<std::int16_t>(elevator.bottom_floor);
  const auto assigned_car = static_cast<std::int16_t>(car_index + 1U);
  const auto matches = [&](std::int16_t floor) {
    return original_elevator_occupancy(car, floor) != 0U ||
           original_elevator_assignment(elevator.block_2a2, floor) ==
               assigned_car ||
           original_elevator_assignment(elevator.block_31a, floor) ==
               assigned_car;
  };
  if (direction != 0) {
    for (auto floor = top; floor >= current; --floor) {
      if (matches(floor)) return floor;
    }
  } else {
    for (auto floor = bottom; floor <= current; ++floor) {
      if (matches(floor)) return floor;
    }
  }
  return signed_byte(elevator.car_home_floors[car_index]);
}

void recompute_original_elevator_car(OriginalTdtElevator& elevator,
                                     std::size_t car_index,
                                     bool byte_swapped,
                                     unsigned depth = 0U) noexcept;

void decrement_original_elevator_assignment_owner(
    OriginalTdtElevator& elevator,
    std::size_t car_index,
    bool byte_swapped,
    unsigned depth) noexcept {
  // Exact 1090:151c decrements the car's word at elevator +0x2994 and then
  // immediately calls 1090:0bcf to recompute that car's route state.
  if (car_index >= elevator.car_records.size()) return;
  auto& exact = elevator.car_records[car_index].exact_bytes;
  store_u16(
      exact, 10U,
      static_cast<std::uint16_t>(
          load_u16(exact, 10U, byte_swapped) - 1U),
      byte_swapped);
  if (depth < elevator.car_records.size()) {
    recompute_original_elevator_car(
        elevator, car_index, byte_swapped, depth + 1U);
  }
}

void clear_original_elevator_floor_assignments(
    OriginalTdtElevator& elevator,
    std::size_t car_index,
    std::int16_t floor,
    bool byte_swapped,
    unsigned depth) noexcept {
  // Exact 1090:13cc direction-sensitive assignment release/recompute helper.
  if (car_index >= elevator.car_records.size() ||
      !original_floor_in_elevator_range(floor)) {
    return;
  }
  auto& car = elevator.car_records[car_index].exact_bytes;
  const auto mode = signed_byte(car[14]);
  const auto direction = signed_byte(car[4]);
  const auto release = [&](std::array<std::byte, 120>& assignments) {
    auto& entry = assignments[static_cast<std::size_t>(floor)];
    if (entry == std::byte{0}) return;
    const auto owner = static_cast<std::int16_t>(signed_byte(entry) - 1);
    entry = std::byte{0};
    if (owner == static_cast<std::int16_t>(car_index)) {
      store_u16(
          car, 10U,
          static_cast<std::uint16_t>(
              load_u16(car, 10U, byte_swapped) - 1U),
          byte_swapped);
    } else if (owner >= 0) {
      decrement_original_elevator_assignment_owner(
          elevator, static_cast<std::size_t>(owner), byte_swapped, depth);
    }
  };
  if (mode != 0 || direction != 0) release(elevator.block_2a2);
  if (mode != 0 || direction == 0) release(elevator.block_31a);
}

std::int16_t update_original_elevator_car_direction(
    OriginalTdtElevator& elevator,
    std::size_t car_index,
    bool byte_swapped,
    unsigned depth) noexcept {
  // Exact 1090:1d2f car-direction arbitration: target comparison, end-stop
  // reversal, opposite-lane request preference, and 13cc owner cleanup.
  auto& car = elevator.car_records[car_index].exact_bytes;
  const auto old_direction = signed_byte(car[4]);
  const auto current = signed_byte(car[0]);
  const auto target = signed_byte(car[5]);
  auto direction = old_direction;
  if (current != target) {
    direction = target > current ? 1 : 0;
  } else if (car[7] != std::byte{0}) {
    const auto top = static_cast<std::int16_t>(elevator.top_floor);
    const auto bottom = static_cast<std::int16_t>(elevator.bottom_floor);
    if (current == top && direction != 0) {
      direction = 0;
    } else if (current == bottom && direction == 0) {
      direction = 1;
    } else if (car[14] == std::byte{0} &&
               original_floor_in_elevator_range(current)) {
      if (direction != 0) {
        if ((elevator.block_2a2[static_cast<std::size_t>(current)] ==
             std::byte{0}) &&
            (elevator.block_31a[static_cast<std::size_t>(current)] !=
             std::byte{0})) {
          direction = 0;
        }
      } else if ((elevator.block_31a[static_cast<std::size_t>(current)] ==
                  std::byte{0}) &&
                 (elevator.block_2a2[static_cast<std::size_t>(current)] !=
                  std::byte{0})) {
        direction = 1;
      }
    }
  }
  car[4] = static_cast<std::byte>(direction);
  if (direction != old_direction) {
    clear_original_elevator_floor_assignments(
        elevator, car_index, current, byte_swapped, depth);
  }
  return direction;
}

void recompute_original_elevator_car(OriginalTdtElevator& elevator,
                                     std::size_t car_index,
                                     bool byte_swapped,
                                     unsigned depth) noexcept {
  if (car_index >= elevator.car_records.size() ||
      depth > elevator.car_records.size()) {
    return;
  }
  auto& car = elevator.car_records[car_index].exact_bytes;
  const auto target = select_original_elevator_car_target(
      elevator, car_index, byte_swapped);
  car[5] = static_cast<std::byte>(target);
  // In valid original state 1090:1553 always selects a serviced floor. The
  // executable's malformed-state fallback also consumes process-only calendar
  // globals that are intentionally not part of the TDT stream.
  if (target < elevator.bottom_floor || target > elevator.top_floor) return;
  (void)update_original_elevator_car_direction(
      elevator, car_index, byte_swapped, depth);
  car[13] = static_cast<std::byte>(
      select_original_elevator_car_secondary_target(elevator, car_index));
}

bool remove_original_person_from_elevator_car(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t person_index) noexcept {
  // Exact 1210:18fa active-car/slot scan and removal transaction, including
  // aggregate, destination-floor, and distinct-floor occupancy maintenance.
  if (elevator_index >= document.elevators.size()) return false;
  auto& elevator = document.elevators[elevator_index];
  const auto capacity = std::min<std::size_t>(elevator.capacity, 42U);
  for (std::size_t car_index = 0;
       car_index < elevator.car_records.size(); ++car_index) {
    auto& exact = elevator.car_records[car_index].exact_bytes;
    if (exact[15] == std::byte{0}) continue;
    for (std::size_t slot = 0; slot < capacity; ++slot) {
      if (load_u32(exact, 16U + slot * 4U,
                   document.header.byte_swapped) != person_index) {
        continue;
      }
      const auto floor = signed_byte(exact[184U + slot]);
      const auto removed = pop_original_elevator_car_passenger_slot(
          document, elevator_index, car_index, slot);
      if (!removed || *removed != person_index) return false;
      exact[3] = static_cast<std::byte>(
          std::to_integer<std::uint8_t>(exact[3]) - 1U);
      const auto occupancy_offset = 226 + floor;
      if (occupancy_offset >= 0 &&
          occupancy_offset < static_cast<std::int16_t>(exact.size())) {
        auto& occupancy = exact[static_cast<std::size_t>(occupancy_offset)];
        occupancy = static_cast<std::byte>(
            std::to_integer<std::uint8_t>(occupancy) - 1U);
        if (occupancy == std::byte{0}) {
          exact[12] = static_cast<std::byte>(
              std::to_integer<std::uint8_t>(exact[12]) - 1U);
        }
      }
      recompute_original_elevator_car(
          elevator, car_index, document.header.byte_swapped);
      return true;
    }
  }
  return false;
}

void remove_original_person_from_elevator_queue(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int8_t floor,
    bool first_lane,
    std::size_t person_index) noexcept {
  if (elevator_index >= document.elevators.size()) return;
  auto& elevator = document.elevators[elevator_index];
  const auto mapped = original_elevator_floor_record_index(
      elevator.type, elevator.bottom_floor, elevator.top_floor, floor);
  if (mapped < 0) return;
  const auto found = std::find_if(
      elevator.floor_records.begin(), elevator.floor_records.end(),
      [&](const OriginalTdtElevatorFloorRecord& record) {
        return record.mapped_index == mapped;
      });
  if (found == elevator.floor_records.end()) return;
  auto& exact = found->exact_bytes;
  const std::size_t count_offset = first_lane ? 0U : 2U;
  const std::size_t cursor_offset = first_lane ? 1U : 3U;
  const std::size_t table_offset = first_lane ? 4U : 164U;
  const auto count = std::min<std::size_t>(
      std::to_integer<std::uint8_t>(exact[count_offset]), 40U);
  auto cursor = static_cast<std::size_t>(
      std::to_integer<std::uint8_t>(exact[cursor_offset]) % 40U);
  std::array<std::uint32_t, 40> retained{};
  std::size_t retained_count = 0U;
  for (std::size_t index = 0; index < count; ++index) {
    const auto passenger = load_u32(
        exact, table_offset + cursor * 4U,
        document.header.byte_swapped);
    if (passenger != person_index && retained_count < retained.size()) {
      retained[retained_count++] = passenger;
    }
    cursor = (cursor + 1U) % 40U;
  }
  exact[cursor_offset] = static_cast<std::byte>(cursor);
  // 1210:15ea assumes the person is present and decrements unconditionally.
  const auto new_count = count == 0U ? 0U : count - 1U;
  exact[count_offset] = static_cast<std::byte>(new_count);
  const auto write_cursor = cursor;
  for (std::size_t index = 0; index < new_count; ++index) {
    const auto destination = (write_cursor + index) % 40U;
    store_u32(exact, table_offset + destination * 4U, retained[index],
              document.header.byte_swapped);
  }
}

void apply_original_person_metric_finalizer(
    OriginalTdtPersonRecord& person,
    bool byte_swapped) noexcept {
  auto exact = std::span<std::byte>(person.exact_bytes);
  exact[9] = static_cast<std::byte>(
      std::to_integer<std::uint8_t>(exact[9]) + 1U);
  const auto distance = load_u16(exact, 12U, byte_swapped);
  store_u16(exact, 14U,
            static_cast<std::uint16_t>(
                load_u16(exact, 14U, byte_swapped) +
                (distance & 0x03ffU)),
            byte_swapped);
  store_u16(exact, 10U, 0U, byte_swapped);
  store_u16(exact, 12U, distance & 0xfc00U, byte_swapped);
}

void add_original_person_waiting_delay(OriginalTdtPersonRecord& person,
                                       std::uint16_t delay,
                                       bool byte_swapped) noexcept {
  auto exact = std::span<std::byte>(person.exact_bytes);
  // Exact 11d8:02f7: preserve word 12's flag bits, cap its low ten bits plus
  // DS:dd7e at 300, and clear word 10. This is distinct from 11d8:00fc.
  const auto old = load_u16(exact, 12U, byte_swapped);
  const auto sum = static_cast<std::uint16_t>((old & 0x03ffU) + delay);
  const auto bounded = sum < 300U ? sum : 300U;
  store_u16(exact, 12U,
            static_cast<std::uint16_t>((old & 0xfc00U) + bounded),
            byte_swapped);
  store_u16(exact, 10U, 0U, byte_swapped);
}

bool original_car_arrival_dispatches_family(std::byte raw_type) noexcept {
  switch (signed_byte(raw_type)) {
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 9:
    case 10:
    case 12:
    case 14:
    case 15:
    case 18:
    case 29:
    case 33:
    case 36:
      return true;
    default:
      return false;
  }
}

bool original_car_arrival_sets_person_floor(std::byte raw_type) noexcept {
  // 1210:0883 writes byte 7 before every table-dispatched family, but its
  // separate type-14 Security branch jumps straight to 1220:67cf.
  return signed_byte(raw_type) != 14;
}

void release_original_person_transit(OriginalTdtDocument& document,
                                     std::size_t person_index,
                                     std::uint16_t frame_time) noexcept {
  if (person_index >= document.people.size()) return;
  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);
  const auto transit = signed_byte(exact[8]);
  if (transit >= 0 && transit < 0x40) {
    auto& stair = document.post_elevator.stairs_bd70[
        static_cast<std::size_t>(transit)];
    if (stair.floor == signed_byte(exact[7])) {
      stair.word_8 = static_cast<std::uint16_t>(stair.word_8 - 1U);
      store_u16(stair.exact_bytes, 8U, stair.word_8,
                document.header.byte_swapped);
    } else {
      stair.word_6 = static_cast<std::uint16_t>(stair.word_6 - 1U);
      store_u16(stair.exact_bytes, 6U, stair.word_6,
                document.header.byte_swapped);
    }
  } else if (transit >= 0x40) {
    auto elevator_index = static_cast<std::size_t>(transit - 0x40);
    bool first_lane = true;
    if (elevator_index >= 24U) {
      elevator_index -= 24U;
      first_lane = false;
    }
    if (!remove_original_person_from_elevator_car(
            document, elevator_index, person_index)) {
      remove_original_person_from_elevator_queue(
          document, elevator_index, signed_byte(exact[7]), first_lane,
          person_index);
    }
    if (exact[4] != std::byte{15}) {
      // 1210:1c46 calls both 11d8:00fc and 11d8:0000.
      apply_original_person_common_update(
          person, frame_time, document.header.byte_swapped);
      return;
    }
  }

  // The outer 1220:1518 call supplies the 11d8:0000 half for Stair records.
  if (exact[4] != std::byte{15}) {
    apply_original_person_metric_finalizer(
        person, document.header.byte_swapped);
  }
}

std::size_t finalize_original_facility_people(
    OriginalTdtDocument& document,
    OriginalTdtFloor& floor,
    const OriginalTdtTenant& source_tenant,
    std::uint16_t frame_time) noexcept {
  const auto source_type = source_tenant.type;
  bool office = false;
  std::int16_t threshold = 0x40;
  enum class SpanKind { none, hotel, selected } span_kind = SpanKind::none;
  switch (source_type) {
    case 3:
    case 4:
    case 5:
      span_kind = SpanKind::hotel;
      break;
    case 7:
      office = true;
      span_kind = SpanKind::selected;
      break;
    case 6:
    case 9:
    case 10:
    case 12:
    case 29:
    case 30:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
      span_kind = SpanKind::selected;
      break;
    case 15:
      threshold = 3;
      span_kind = SpanKind::selected;
      break;
    default:
      return 0U;
  }

  const auto key = signed_byte(source_tenant.exact_bytes[12]);
  if (key < 0 ||
      static_cast<std::size_t>(key) >= floor.tenant_index.size()) {
    return 0U;
  }
  const auto target_index = floor.tenant_index[static_cast<std::size_t>(key)];
  if (target_index >= floor.tenants.size()) return 0U;
  const auto& target_tenant = floor.tenants[target_index];
  const auto people_start = original_tenant_people_start(
      target_tenant, document.header.byte_swapped);
  const auto count =
      span_kind == SpanKind::hotel
          ? (target_tenant.type == 3 ? 2U : 3U)
          : original_person_span(target_tenant.type);

  std::size_t finalized = 0U;
  for (std::size_t ordinal = 0; ordinal < count; ++ordinal) {
    const auto wide_index = static_cast<std::uint64_t>(people_start) + ordinal;
    if (wide_index >= document.people.size() ||
        wide_index >= document.people_count) {
      break;
    }
    const auto person_index = static_cast<std::size_t>(wide_index);
    auto& person = document.people[person_index];
    if (signed_byte(person.exact_bytes[5]) >= threshold) {
      release_original_person_transit(document, person_index, frame_time);
      ++finalized;
    }
    if (office) {
      (void)remove_original_office_presence(document, person_index, person);
    }
  }
  return finalized;
}

bool remove_original_person_link(OriginalTdtDocument& document,
                                 std::size_t person_index) noexcept {
  auto& links = document.post_elevator.dce4_person_indices;
  auto& count = document.header.person_link_count;
  count = static_cast<std::uint16_t>(
      std::min<std::size_t>(count, links.size()));
  for (std::size_t index = 0; index < count; ++index) {
    if (links[index] != static_cast<std::int32_t>(person_index)) continue;
    for (std::size_t source = index + 1U; source < count; ++source) {
      links[source - 1U] = links[source];
    }
    if (index < document.person_link_names.size()) {
      document.person_link_names.erase(
          document.person_link_names.begin() +
          static_cast<std::ptrdiff_t>(index));
    }
    --count;
    links[count] = -1;
    return true;
  }
  return false;
}

void remove_original_hotel_person_parking(
    OriginalTdtDocument& document,
    std::size_t person_index) noexcept {
  if (person_index >= document.people.size()) return;
  auto person = std::span<std::byte>(
      document.people[person_index].exact_bytes);
  if ((std::to_integer<std::uint8_t>(person[13]) & 0xfcU) == 0U) return;

  // 1198:0489 decrements b846 using the owning Hotel/Office type before it
  // clears the person's upper parking bits. The previous translation omitted
  // this accounting half of the helper.
  if (const auto* owner = find_original_tenant(
          document, std::to_integer<std::uint8_t>(person[0]),
          std::to_integer<std::uint8_t>(person[1]))) {
    decrement_original_parking_population(
        document,
        std::bit_cast<std::int8_t>(
            std::to_integer<std::uint8_t>(owner->exact_bytes[4])));
  }

  store_u16(person, 12U,
            static_cast<std::uint16_t>(
                load_u16(person, 12U, document.header.byte_swapped) & 0x03ffU),
            document.header.byte_swapped);

  auto& tail = document.post_elevator;
  const auto count = std::min<std::size_t>(
      tail.parking_connected < 0 ? 0U
                                 : static_cast<std::size_t>(tail.parking_connected),
      tail.cf9c_records.size());
  for (std::size_t index = 0; index < count; ++index) {
    auto& record = tail.cf9c_records[index];
    if (signed_byte(record[0]) < 0 ||
        load_u32(record, 2U, document.header.byte_swapped) != person_index) {
      continue;
    }
    const auto floor_number = signed_byte(record[0]);
    const auto key = signed_byte(record[1]);
    if (floor_number >= 0 &&
        static_cast<std::size_t>(floor_number) < document.floors.size() &&
        key >= 0 &&
        static_cast<std::size_t>(key) <
            OriginalTdtFloor::kIndexCapacity) {
      auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
      const auto tenant_index =
          floor.tenant_index[static_cast<std::size_t>(key)];
      if (tenant_index < floor.tenants.size()) {
        auto& parking = floor.tenants[tenant_index];
        if (parking.status != 1U) {
          set_original_tenant_status(parking, 0U);
          mark_original_tenant_changed(parking);
        }
      }
    }
    store_u32(record, 2U, 0U, document.header.byte_swapped);
    break;
  }
}

std::int16_t original_wrapped_absolute_difference(
    std::uint16_t first,
    std::uint16_t second) noexcept {
  const auto difference = static_cast<std::int16_t>(
      static_cast<std::uint16_t>(first - second));
  if (difference >= 0) return difference;
  return static_cast<std::int16_t>(
      static_cast<std::uint16_t>(0U -
                                 static_cast<std::uint16_t>(difference)));
}

std::int16_t original_wrapped_route_score(
    std::int16_t horizontal_distance,
    std::uint16_t base) noexcept {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(
      static_cast<std::uint16_t>(horizontal_distance) * 8U + base));
}

std::uint32_t original_route_bit(std::size_t index) noexcept {
  // 1208:040f sets, 1208:03e1 tests, and 1208:0434 clears this MSB-first
  // route/elevator mask; native callers use the returned mask for all three.
  return index < 32U ? (0x80000000U >> index) : 0U;
}

std::optional<std::uint16_t> original_person_route_x(
    const OriginalTdtDocument& document,
    std::size_t person_index) noexcept {
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return std::nullopt;
  const auto& person = document.people[person_index].exact_bytes;
  const auto floor_number = signed_byte(person[0]);
  const auto key = signed_byte(person[1]);
  if (floor_number < 0 || key < 0 ||
      static_cast<std::size_t>(floor_number) >= document.floors.size() ||
      static_cast<std::size_t>(key) >= OriginalTdtFloor::kIndexCapacity) {
    return std::nullopt;
  }
  const auto& floor =
      document.floors[static_cast<std::size_t>(floor_number)];
  const auto tenant_index = floor.tenant_index[static_cast<std::size_t>(key)];
  if (tenant_index >= floor.tenants.size()) return std::nullopt;
  // 11b0:0f10 reads the complete word at tenant +6. Some facility families
  // reuse it as an index/state word; retaining that alias is intentional.
  return load_u16(floor.tenants[tenant_index].exact_bytes, 6U,
                  document.header.byte_swapped);
}

bool original_route_floor_valid(std::int16_t floor) noexcept {
  return floor >= 0 && floor < 120;
}

}  // namespace

bool original_full_stair_span_available(
    const OriginalTdtDocument& document,
    std::int16_t source_floor,
    std::int16_t destination_floor) noexcept {
  // Exact 11b0:0dc0 tracked-route full Stair-span predicate, including its
  // six-floor limit and the ordinal-three parity cutoff.
  if (!original_route_floor_valid(source_floor) ||
      !original_route_floor_valid(destination_floor)) {
    return false;
  }
  if (std::abs(static_cast<int>(destination_floor) - source_floor) > 6) {
    return false;
  }
  bool saw_other_parity = false;
  const auto first = std::min(source_floor, destination_floor);
  const auto last = std::max(source_floor, destination_floor);
  std::int16_t ordinal = 0;
  for (auto floor = first; floor < last; ++floor, ++ordinal) {
    const auto value = std::to_integer<std::uint8_t>(
        document.post_elevator.cf10[static_cast<std::size_t>(floor)]);
    if (value == 0U) return false;
    if ((value & 1U) == 0U) saw_other_parity = true;
    if (saw_other_parity && ordinal >= 3) return false;
  }
  return true;
}

bool original_odd_stair_span_available(
    const OriginalTdtDocument& document,
    std::int16_t source_floor,
    std::int16_t destination_floor) noexcept {
  // Exact 11b0:0e80 untracked-route odd Stair-span predicate.
  if (!original_route_floor_valid(source_floor) ||
      !original_route_floor_valid(destination_floor)) {
    return false;
  }
  if (std::abs(static_cast<int>(destination_floor) - source_floor) > 6) {
    return false;
  }
  const auto first = std::min(source_floor, destination_floor);
  const auto last = std::max(source_floor, destination_floor);
  std::int16_t ordinal = 0;
  for (auto floor = first; floor < last; ++floor, ++ordinal) {
    const auto value = std::to_integer<std::uint8_t>(
        document.post_elevator.cf10[static_cast<std::size_t>(floor)]);
    if ((value & 2U) == 0U || ordinal >= 3) return false;
  }
  return true;
}

std::optional<std::int16_t> score_original_stair(
    const OriginalTdtStairRecord& stair,
    std::int16_t source_floor,
    std::int16_t destination_floor,
    std::uint16_t person_x,
    bool odd_only,
    bool& direction_up) noexcept {
  // Exact 11b0:141c general and 11b0:14c9 odd-only Stair candidate scorers.
  if (stair.used == 0U) return std::nullopt;
  const auto shape = static_cast<std::int16_t>(stair.shape);
  if (odd_only && (shape & 1) == 0) return std::nullopt;

  direction_up = source_floor < destination_floor;
  if (direction_up) {
    if (static_cast<std::int16_t>(stair.floor) != source_floor) {
      return std::nullopt;
    }
  } else {
    const auto lower = static_cast<std::int16_t>(
        source_floor - (shape >> 1) - 1);
    if (static_cast<std::int16_t>(stair.floor) != lower) {
      return std::nullopt;
    }
  }
  const auto distance = original_wrapped_absolute_difference(
      stair.x, person_x);
  return original_wrapped_route_score(
      distance, (shape & 1) != 0 ? 0x0280U : 0U);
}

namespace {

bool original_vertical_route_contains_floor(
    const std::array<std::byte, 0x1e4>& route,
    std::int16_t floor) noexcept {
  if (route[1] == std::byte{0}) return false;
  const auto top = signed_byte(route[2]);
  const auto bottom = signed_byte(route[3]);
  return bottom <= floor && top >= floor;
}

}  // namespace

bool find_original_transfer_direction(
    const OriginalTdtDocument& document,
    std::size_t source_bit,
    std::int16_t source_floor,
    std::uint32_t destination_graph,
    bool& direction_up) noexcept {
  // Exact shared form of 11b0:0a21/0ad4's scans of the sixteen db9c transfer
  // masks: require the source bit, reject the current floor, intersect the
  // destination graph, and return whether the first accepted transfer lies
  // above the source.
  const auto bit = original_route_bit(source_bit);
  if (bit == 0U) return false;
  for (const auto& transfer : document.post_elevator.db9c_records) {
    const auto mask = load_u32(transfer, 0U, document.header.byte_swapped);
    if ((mask & bit) == 0U) continue;
    const auto transfer_floor = signed_byte(transfer[4]);
    if (transfer_floor == source_floor) continue;
    const auto other_transports = mask & ~bit;
    if ((destination_graph & other_transports) == 0U) continue;
    direction_up = transfer_floor > source_floor;
    return true;
  }
  return false;
}

namespace {

std::optional<std::int16_t> score_original_vertical_route(
    const OriginalTdtDocument& document,
    std::size_t route_index,
    std::int16_t source_floor,
    std::int16_t destination_floor,
    bool& direction_up) noexcept {
  // Exact 11b0:0805/08f2 route-family gate and direction selection for one
  // bff0 record, including direct-range service, per-floor graph lookup and
  // 11b0:0ad4 transfer fallback. A valid route has the original score zero.
  if (route_index >= document.post_elevator.routes_bff0.size() ||
      !original_route_floor_valid(source_floor) ||
      !original_route_floor_valid(destination_floor)) {
    return std::nullopt;
  }
  const auto& route = document.post_elevator.routes_bff0[route_index];
  if (route[1] == std::byte{0} ||
      !original_vertical_route_contains_floor(route, source_floor)) {
    return std::nullopt;
  }
  if (original_vertical_route_contains_floor(route, destination_floor)) {
    direction_up = source_floor < destination_floor;
    return 0;
  }

  const auto destination_graph = load_u32(
      route, 4U + static_cast<std::size_t>(destination_floor) * 4U,
      document.header.byte_swapped);
  if (destination_graph == 0U) return std::nullopt;
  const auto source_graph = load_u32(
      route, 4U + static_cast<std::size_t>(source_floor) * 4U,
      document.header.byte_swapped);
  if (source_graph != 0U) {
    const auto direct = static_cast<std::uint16_t>(source_graph);
    if (direct == 0U ||
        direct > document.post_elevator.db9c_records.size()) {
      return std::nullopt;
    }
    const auto mask = load_u32(
        document.post_elevator.db9c_records[direct - 1U], 0U,
        document.header.byte_swapped);
    if ((mask & destination_graph) == destination_graph) {
      return std::nullopt;
    }
  }
  if (!find_original_transfer_direction(
          document, 24U + route_index, source_floor, destination_graph,
          direction_up)) {
    return std::nullopt;
  }
  return 0;
}

const OriginalTdtElevatorFloorRecord* find_original_elevator_floor_record(
    const OriginalTdtElevator& elevator,
    std::int16_t mapped_index) noexcept {
  const auto found = std::find_if(
      elevator.floor_records.begin(), elevator.floor_records.end(),
      [&](const OriginalTdtElevatorFloorRecord& record) {
        return record.mapped_index == mapped_index;
      });
  return found == elevator.floor_records.end() ? nullptr : &*found;
}

OriginalTdtElevatorFloorRecord* find_original_elevator_floor_record(
    OriginalTdtElevator& elevator,
    std::int16_t mapped_index) noexcept {
  const auto found = std::find_if(
      elevator.floor_records.begin(), elevator.floor_records.end(),
      [&](const OriginalTdtElevatorFloorRecord& record) {
        return record.mapped_index == mapped_index;
      });
  return found == elevator.floor_records.end() ? nullptr : &*found;
}

std::optional<std::int16_t> score_original_elevator_route(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t source_floor,
    std::int16_t destination_floor,
    std::uint16_t person_x,
    bool tracked_route,
    bool& direction_up) noexcept {
  // Exact 11b0:11af Elevator candidate scorer used by 11b0:0fa5: serviced-
  // floor/type gates, 10a0:17ee floor-record mapping, db9c transfer rejection,
  // direction-lane counts, and the 0x280/0x3e8/0xbb8/0x1770 penalties.
  if (elevator_index >= document.elevators.size() ||
      !original_route_floor_valid(source_floor) ||
      !original_route_floor_valid(destination_floor)) {
    return std::nullopt;
  }
  const auto& elevator = document.elevators[elevator_index];
  if (elevator.used == 0U ||
      elevator.serviced_floors[static_cast<std::size_t>(source_floor)] ==
          std::byte{0} ||
      ((elevator.type != 2U) != tracked_route)) {
    return std::nullopt;
  }
  const auto mapped = original_elevator_floor_record_index(
      elevator.type, elevator.bottom_floor, elevator.top_floor, source_floor);
  const auto* floor_record =
      find_original_elevator_floor_record(elevator, mapped);
  if (mapped < 0 || floor_record == nullptr) return std::nullopt;

  const bool directly_served =
      elevator.serviced_floors[static_cast<std::size_t>(destination_floor)] !=
      std::byte{0};
  if (directly_served) {
    direction_up = source_floor < destination_floor;
  } else {
    const auto destination_graph = load_u32(
        elevator.block_c2,
        static_cast<std::size_t>(destination_floor) * 4U,
        document.header.byte_swapped);
    if (destination_graph == 0U) return std::nullopt;
    const auto source_graph = load_u32(
        elevator.block_c2, static_cast<std::size_t>(source_floor) * 4U,
        document.header.byte_swapped);
    if (source_graph != 0U) {
      const auto direct = static_cast<std::uint16_t>(source_graph);
      if (direct == 0U ||
          direct > document.post_elevator.db9c_records.size()) {
        return std::nullopt;
      }
      const auto mask = load_u32(
          document.post_elevator.db9c_records[direct - 1U], 0U,
          document.header.byte_swapped);
      if ((mask & destination_graph) == destination_graph) {
        return std::nullopt;
      }
    }
    if (!find_original_transfer_direction(
            document, elevator_index, source_floor, destination_graph,
            direction_up)) {
      return std::nullopt;
    }
  }

  const auto& exact = floor_record->exact_bytes;
  const auto count = signed_byte(exact[direction_up ? 0U : 2U]);
  if (elevator.type == 0U) {
    return static_cast<std::int16_t>(
        static_cast<std::uint16_t>(count +
            (directly_served ? 0x0280U : 0x0bb8U)));
  }
  const auto distance = original_wrapped_absolute_difference(
      elevator.x, person_x);
  if (count == 40) {
    return original_wrapped_route_score(
        distance, directly_served ? 0x03e8U : 0x1770U);
  }
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(
      original_wrapped_route_score(
          distance, directly_served ? 0x0280U : 0x0bb8U) + count));
}

struct OriginalRouteSelection {
  std::int16_t transport_index{-1};
  bool direction_up{};
};

OriginalRouteSelection select_original_person_transport(
    const OriginalTdtDocument& document,
    std::int16_t source_floor,
    std::int16_t destination_floor,
    std::uint16_t person_x,
    bool tracked_route) noexcept {
  // Exact 11b0:0fa5 transport selector and original last-writer direction
  // behavior across its Stair, bff0 and Elevator candidate scans.
  std::int16_t best_score = 0x7fff;
  std::int16_t best_transport = -1;
  bool direction_up = false;
  const auto consider = [&](std::optional<std::int16_t> score,
                            std::int16_t transport) {
    if (score && *score < best_score) {
      best_score = *score;
      best_transport = transport;
    }
  };

  if (tracked_route) {
    const bool adjacent =
        std::abs(static_cast<int>(source_floor) - destination_floor) == 1;
    if (adjacent || original_full_stair_span_available(
                        document, source_floor, destination_floor)) {
      for (std::size_t index = 0;
           index < document.post_elevator.stairs_bd70.size(); ++index) {
        consider(score_original_stair(
                     document.post_elevator.stairs_bd70[index], source_floor,
                     destination_floor, person_x, false, direction_up),
                 static_cast<std::int16_t>(index));
      }
      if (best_transport >= 0 && best_score < 0x0280) {
        return {best_transport, direction_up};
      }
    }

    // 0fa5 scans transfer routes only when no initial Stair candidate was
    // found. A costly Stair skips directly to the Elevator comparison while
    // remaining the incumbent score.
    if (best_transport < 0) {
      for (std::size_t index = 0;
           index < document.post_elevator.routes_bff0.size(); ++index) {
        consider(score_original_vertical_route(
                     document, index, source_floor, destination_floor,
                     direction_up),
                 static_cast<std::int16_t>(index));
      }
      if (best_transport >= 0) {
        const auto adjacent_floor = static_cast<std::int16_t>(
            source_floor + (direction_up ? 1 : -1));
        best_score = 0x7fff;
        best_transport = -1;
        for (std::size_t index = 0;
             index < document.post_elevator.stairs_bd70.size(); ++index) {
          consider(score_original_stair(
                       document.post_elevator.stairs_bd70[index],
                       source_floor, adjacent_floor, person_x, false,
                       direction_up),
                   static_cast<std::int16_t>(index));
        }
        if (best_transport >= 0 && best_score < 0x0280) {
          return {best_transport, direction_up};
        }
      }
    }
  } else {
    const bool adjacent =
        std::abs(static_cast<int>(source_floor) - destination_floor) == 1;
    if (adjacent || original_odd_stair_span_available(
                        document, source_floor, destination_floor)) {
      for (std::size_t index = 0;
           index < document.post_elevator.stairs_bd70.size(); ++index) {
        consider(score_original_stair(
                     document.post_elevator.stairs_bd70[index], source_floor,
                     destination_floor, person_x, true, direction_up),
                 static_cast<std::int16_t>(index));
      }
      if (best_transport >= 0) {
        // 14c9 does not own the output pointer; 0fa5 writes this literal
        // comparison after the scan.
        direction_up = source_floor < destination_floor;
        return {best_transport, direction_up};
      }
    }
  }

  for (std::size_t index = 0; index < document.elevators.size(); ++index) {
    consider(score_original_elevator_route(
                 document, index, source_floor, destination_floor, person_x,
                 tracked_route, direction_up),
             static_cast<std::int16_t>(index + 0x40U));
  }
  return {best_transport, direction_up};
}

bool enqueue_original_elevator_waiting_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::size_t elevator_index,
    std::int16_t floor,
    bool direction_up,
    std::uint8_t calendar_phase,
    std::int8_t day_phase,
    bool& assignment_created) noexcept {
  // Exact 1210:11c2 two-lane forty-entry ring insertion: write at
  // (cursor+count)%40, assign a car only for the first waiter, increment the
  // selected count, and leave the cursor unchanged.
  assignment_created = false;
  if (elevator_index >= document.elevators.size() ||
      !original_route_floor_valid(floor) ||
      person_index > 0xffffffffU) {
    return false;
  }
  auto& elevator = document.elevators[elevator_index];
  const auto mapped = original_elevator_floor_record_index(
      elevator.type, elevator.bottom_floor, elevator.top_floor, floor);
  auto* floor_record = find_original_elevator_floor_record(elevator, mapped);
  if (mapped < 0 || floor_record == nullptr) return false;
  auto& exact = floor_record->exact_bytes;
  const std::size_t count_offset = direction_up ? 0U : 2U;
  const std::size_t cursor_offset = direction_up ? 1U : 3U;
  const std::size_t table_offset = direction_up ? 4U : 164U;
  const auto count = signed_byte(exact[count_offset]);
  const auto cursor = signed_byte(exact[cursor_offset]);
  if (count < 0 || count >= 40 || cursor < 0 || cursor >= 40) return false;
  const auto slot = static_cast<std::size_t>((count + cursor) % 40);
  store_u32(exact, table_offset + slot * 4U,
            static_cast<std::uint32_t>(person_index),
            document.header.byte_swapped);
  if (count == 0) {
    assignment_created = assign_original_elevator_waiting_floor(
        document, elevator_index, floor, direction_up, calendar_phase,
        day_phase);
  }
  exact[count_offset] = static_cast<std::byte>(count + 1);
  return true;
}

bool original_security_extra_lobby_floor(
    const OriginalTdtDocument& document,
    std::int16_t floor) noexcept {
  if (floor < 11) return false;
  const auto lobby_limit = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(document.header.lobby_height + 10U));
  return floor < lobby_limit;
}

bool original_security_floor_usable(
    const OriginalTdtDocument& document,
    std::int16_t floor) noexcept {
  return floor >= 0 && floor < static_cast<std::int16_t>(document.floors.size()) &&
         !document.floors[static_cast<std::size_t>(floor)].tenants.empty() &&
         !original_security_extra_lobby_floor(document, floor);
}

bool original_security_floor_has_fire(
    const OriginalTdtDocument& document,
    std::int16_t floor) noexcept {
  if (floor < 0 || floor >= static_cast<std::int16_t>(document.floors.size())) {
    return false;
  }
  const auto index = static_cast<std::size_t>(floor);
  return std::bit_cast<std::int16_t>(
             load_original_header_word(document, 320U + index * 2U)) >= 0 ||
         std::bit_cast<std::int16_t>(
             load_original_header_word(document, 80U + index * 2U)) >= 0;
}

bool original_security_fire_words_are_sentinel(
    const OriginalTdtDocument& document,
    std::int16_t floor) noexcept {
  if (floor < 0 || floor >= static_cast<std::int16_t>(document.floors.size())) {
    return false;
  }
  const auto index = static_cast<std::size_t>(floor);
  return load_original_header_word(document, 320U + index * 2U) == 0xffffU &&
         load_original_header_word(document, 80U + index * 2U) == 0xffffU;
}

bool move_original_security_person(
    OriginalTdtDocument& document,
    OriginalTdtPersonRecord& person,
    std::int16_t destination_floor,
    std::int16_t owner_floor,
    const OriginalPartTable& part) noexcept {
  if (destination_floor < 0 ||
      destination_floor >= static_cast<std::int16_t>(document.floors.size())) {
    return false;
  }
  auto exact = std::span<std::byte>(person.exact_bytes);
  const auto current_floor = signed_byte(exact[7]);
  const auto delta = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
      static_cast<std::uint16_t>(destination_floor) -
      static_cast<std::uint16_t>(current_floor)));
  const auto magnitude = delta < 0
                             ? static_cast<std::uint16_t>(
                                   0U - static_cast<std::uint16_t>(delta))
                             : static_cast<std::uint16_t>(delta);
  const auto movement_word = document.security_event_accelerated
                                 ? 0U
                                 : part.words_52_to_ac[1U];
  auto delay = static_cast<std::uint16_t>(magnitude * movement_word);
  if (current_floor < owner_floor) {
    delay = static_cast<std::uint16_t>(delay + movement_word);
  }
  store_u16(exact, 10U, delay, document.header.byte_swapped);
  exact[7] = static_cast<std::byte>(destination_floor);
  exact[8] = std::byte{0};
  store_u16(
      exact, 14U,
      static_cast<std::uint16_t>(
          document.floors[static_cast<std::size_t>(destination_floor)]
              .right_edge -
          2U),
      document.header.byte_swapped);
  return true;
}

void decrement_original_security_tenant_variant(
    OriginalTdtTenant& tenant,
    bool byte_swapped) noexcept {
  const auto variant = static_cast<std::uint16_t>(
      load_u16(tenant.exact_bytes, 6U, byte_swapped) - 1U);
  store_u16(tenant.exact_bytes, 6U, variant, byte_swapped);
  tenant.variant = std::to_integer<std::uint8_t>(tenant.exact_bytes[6]);
  tenant.preserved_07_to_0f[0] = tenant.exact_bytes[7];
}

void increment_original_security_tenant_status(
    OriginalTdtTenant& tenant) noexcept {
  const auto status = static_cast<std::uint8_t>(
      std::to_integer<std::uint8_t>(tenant.exact_bytes[5]) + 1U);
  tenant.exact_bytes[5] = static_cast<std::byte>(status);
  tenant.status = status;
}

void disable_other_original_security_responders(
    OriginalTdtDocument& document,
    std::size_t current_person) noexcept {
  // Exact 10f8:0656 ten-Security/four-byte-person-index exclusion sweep: set
  // each of the other six-person responder records to state 1.
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  for (const auto slot : document.post_elevator.cf88_words) {
    const auto floor_number = static_cast<std::uint8_t>(slot);
    const auto key = static_cast<std::uint8_t>(slot >> 8U);
    if (floor_number == 0xffU) continue;
    auto* tenant = find_original_tenant(document, floor_number, key);
    if (!tenant) continue;
    const auto people_start = original_tenant_people_start(
        *tenant, document.header.byte_swapped);
    for (std::size_t ordinal = 0U; ordinal < 6U; ++ordinal) {
      const auto person_index =
          static_cast<std::uint64_t>(people_start) + ordinal;
      if (person_index >= person_limit) break;
      if (person_index != current_person) {
        document.people[static_cast<std::size_t>(person_index)]
            .exact_bytes[5] = std::byte{1};
      }
    }
  }
}

OriginalSecurityPersonStepResult step_original_bomb_security_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPartTable& part) noexcept {
  OriginalSecurityPersonStepResult result{};
  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);

  auto countdown = load_u16(exact, 10U, document.header.byte_swapped);
  if (std::bit_cast<std::int16_t>(countdown) > 0) {
    store_u16(exact, 10U, static_cast<std::uint16_t>(countdown - 1U),
              document.header.byte_swapped);
    result.status = OriginalSecurityPersonStepStatus::countdown;
    result.changed = true;
    return result;
  }
  countdown = load_u16(exact, 12U, document.header.byte_swapped);
  if (std::bit_cast<std::int16_t>(countdown) > 0) {
    store_u16(exact, 12U, static_cast<std::uint16_t>(countdown - 1U),
              document.header.byte_swapped);
    result.status = OriginalSecurityPersonStepStatus::countdown;
    result.changed = true;
    return result;
  }

  const auto owner_floor = signed_byte(exact[0]);
  const auto owner_key = std::to_integer<std::uint8_t>(exact[1]);
  const auto current_floor = signed_byte(exact[7]);
  if (current_floor < 0 ||
      current_floor >= static_cast<std::int16_t>(document.floors.size())) {
    result.status = OriginalSecurityPersonStepStatus::malformed_floor;
    return result;
  }
  auto& current = document.floors[static_cast<std::size_t>(current_floor)];
  auto position = load_u16(exact, 14U, document.header.byte_swapped);
  if (std::bit_cast<std::int16_t>(position) <=
      std::bit_cast<std::int16_t>(current.left_edge)) {
    if (owner_floor < 0 ||
        owner_floor >= static_cast<std::int16_t>(document.floors.size())) {
      result.status = OriginalSecurityPersonStepStatus::malformed_owner;
      return result;
    }
    auto* owner = find_original_tenant(
        document, static_cast<std::uint8_t>(owner_floor), owner_key);
    if (!owner) {
      result.status = OriginalSecurityPersonStepStatus::malformed_owner;
      return result;
    }
    const auto low = static_cast<std::int16_t>(
        signed_byte(owner->exact_bytes[5]) + 1);
    const auto high = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
        load_u16(owner->exact_bytes, 6U, document.header.byte_swapped) - 1U));
    const auto try_low = [&]() {
      if (!original_security_floor_usable(document, low) ||
          !move_original_security_person(document, person, low, owner_floor,
                                         part)) {
        return false;
      }
      decrement_original_security_tenant_variant(
          *owner, document.header.byte_swapped);
      return true;
    };
    const auto try_high = [&]() {
      if (!original_security_floor_usable(document, high) ||
          !move_original_security_person(document, person, high, owner_floor,
                                         part)) {
        return false;
      }
      increment_original_security_tenant_status(*owner);
      return true;
    };
    const bool moved = current_floor >= owner_floor
                           ? (try_low() || try_high())
                           : (try_high() || try_low());
    result.status = moved ? OriginalSecurityPersonStepStatus::moved_floor
                          : OriginalSecurityPersonStepStatus::search_exhausted;
    result.changed = moved;
    return result;
  }

  position = static_cast<std::uint16_t>(position - 1U);
  store_u16(exact, 14U, position, document.header.byte_swapped);
  const auto action = check_original_bomb_coordinate(
      document, part, current_floor, std::bit_cast<std::int16_t>(position));
  if (action.changed) {
    store_u16(exact, 12U, 100U, document.header.byte_swapped);
    exact[8] = std::byte{4};
    disable_other_original_security_responders(document, person_index);
    result.status = OriginalSecurityPersonStepStatus::bomb_found;
    result.changed = true;
    result.bomb_found = true;
    result.disabled_other_responders = true;
    result.effect = {current_floor, std::bit_cast<std::int16_t>(position)};
    return result;
  }

  store_u16(exact, 12U, part.words_52_to_ac[0U],
            document.header.byte_swapped);
  exact[8] = exact[8] == std::byte{0} ? std::byte{2} : std::byte{0};
  result.status = OriginalSecurityPersonStepStatus::searching;
  result.changed = true;
  return result;
}

OriginalSecurityPersonStepResult step_original_fire_security_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPartTable& part) noexcept {
  OriginalSecurityPersonStepResult result{};
  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);

  auto countdown = load_u16(exact, 10U, document.header.byte_swapped);
  if (std::bit_cast<std::int16_t>(countdown) > 0) {
    store_u16(exact, 10U, static_cast<std::uint16_t>(countdown - 1U),
              document.header.byte_swapped);
    result.status = OriginalSecurityPersonStepStatus::countdown;
    result.changed = true;
    return result;
  }
  countdown = load_u16(exact, 12U, document.header.byte_swapped);
  if (std::bit_cast<std::int16_t>(countdown) > 0) {
    countdown = static_cast<std::uint16_t>(countdown - 1U);
    store_u16(exact, 12U, countdown, document.header.byte_swapped);
    result.status = OriginalSecurityPersonStepStatus::countdown;
    result.changed = true;
    if (countdown == 0U && exact[8] == std::byte{4}) {
      const auto floor = signed_byte(exact[7]);
      const auto x = std::bit_cast<std::int16_t>(
          load_u16(exact, 14U, document.header.byte_swapped));
      if (extinguish_original_fire_at(document, floor, x)) {
        document.security_event_accelerated = true;
        result.status = OriginalSecurityPersonStepStatus::fire_extinguished;
        result.fire_extinguished = true;
      }
    }
    return result;
  }

  const auto owner_floor = signed_byte(exact[0]);
  const auto ordinal = std::bit_cast<std::int16_t>(
      load_u16(exact, 2U, document.header.byte_swapped));
  const auto current_floor = signed_byte(exact[7]);
  if (owner_floor < 0 || ordinal < 0 || ordinal >= 6) {
    result.status = OriginalSecurityPersonStepStatus::malformed_owner;
    return result;
  }
  if (current_floor < 0 ||
      current_floor >= static_cast<std::int16_t>(document.floors.size())) {
    result.status = OriginalSecurityPersonStepStatus::malformed_floor;
    return result;
  }

  const auto move_to = [&](std::int16_t floor) {
    return move_original_security_person(document, person, floor, owner_floor,
                                         part);
  };
  auto position = load_u16(exact, 14U, document.header.byte_swapped);
  const auto left_edge = std::bit_cast<std::int16_t>(
      document.floors[static_cast<std::size_t>(current_floor)].left_edge);
  if (std::bit_cast<std::int16_t>(position) <= left_edge) {
    bool moved = false;
    for (std::int16_t floor = ordinal;
         floor < static_cast<std::int16_t>(document.floors.size());
         floor = static_cast<std::int16_t>(floor + 6)) {
      if (original_security_floor_has_fire(document, floor)) {
        moved = move_to(floor) || moved;
      }
    }
    result.status = moved ? OriginalSecurityPersonStepStatus::moved_floor
                          : OriginalSecurityPersonStepStatus::searching;
    result.changed = moved;
    return result;
  }

  if (original_security_fire_words_are_sentinel(document, current_floor)) {
    for (std::int16_t floor = ordinal;
         floor < static_cast<std::int16_t>(document.floors.size());
         floor = static_cast<std::int16_t>(floor + 6)) {
      if (original_security_floor_has_fire(document, floor) && move_to(floor)) {
        result.status = OriginalSecurityPersonStepStatus::moved_floor;
        result.changed = true;
        return result;
      }
    }
  }

  position = static_cast<std::uint16_t>(position - 1U);
  store_u16(exact, 14U, position, document.header.byte_swapped);
  if (original_fire_covers_coordinate(
          document, current_floor, std::bit_cast<std::int16_t>(position))) {
    store_u16(exact, 12U, part.words_52_to_ac[7U],
              document.header.byte_swapped);
    exact[8] = std::byte{4};
  } else {
    store_u16(exact, 12U, part.words_52_to_ac[0U],
              document.header.byte_swapped);
    exact[8] = exact[8] == std::byte{0} ? std::byte{2} : std::byte{0};
  }
  result.status = OriginalSecurityPersonStepStatus::searching;
  result.changed = true;
  return result;
}

bool release_original_completed_stair(
    OriginalTdtDocument& document,
    OriginalTdtPersonRecord& person) noexcept {
  auto exact = std::span<std::byte>(person.exact_bytes);
  if (signed_byte(exact[5]) < 3) return false;
  const auto transit = signed_byte(exact[8]);
  if (transit < 0 || transit >= 0x40) return false;
  const auto stair_index = static_cast<std::size_t>(transit);
  if (stair_index >= document.post_elevator.stairs_bd70.size()) return false;

  auto& stair = document.post_elevator.stairs_bd70[stair_index];
  if (static_cast<std::int16_t>(stair.floor) == signed_byte(exact[7])) {
    stair.word_8 = static_cast<std::uint16_t>(stair.word_8 - 1U);
    store_u16(stair.exact_bytes, 8U, stair.word_8,
              document.header.byte_swapped);
  } else {
    stair.word_6 = static_cast<std::uint16_t>(stair.word_6 - 1U);
    store_u16(stair.exact_bytes, 6U, stair.word_6,
              document.header.byte_swapped);
  }
  return true;
}

struct OriginalHousekeepingRoomSelection {
  std::int16_t floor{-1};
  std::int16_t key{-1};
};

std::optional<OriginalHousekeepingRoomSelection>
find_original_housekeeping_room(
    OriginalTdtDocument& document,
    OriginalTdtPersonRecord& person) noexcept {
  auto exact = std::span<std::byte>(person.exact_bytes);
  const auto ordinal = static_cast<std::int16_t>(
      load_u16(exact, 2U, document.header.byte_swapped));
  auto current_floor = static_cast<std::int16_t>(signed_byte(exact[7]));

  const auto inspect_floor = [&](std::int16_t floor_number)
      -> std::optional<OriginalHousekeepingRoomSelection> {
    if (floor_number < 0 || floor_number >= 120 ||
        floor_number % 6 != ordinal) {
      return std::nullopt;
    }
    auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
    for (const auto& tenant : floor.tenants) {
      const auto type = signed_byte(tenant.exact_bytes[4]);
      const auto status =
          std::to_integer<std::uint8_t>(tenant.exact_bytes[5]);
      if (type < 3 || type > 5 || (status != 0x28U && status != 0x30U)) {
        continue;
      }
      const auto key = signed_byte(tenant.exact_bytes[12]);
      store_u16(exact, 12U,
                static_cast<std::uint16_t>(static_cast<std::int16_t>(key)),
                document.header.byte_swapped);
      return OriginalHousekeepingRoomSelection{floor_number, key};
    }
    return std::nullopt;
  };

  for (; current_floor < 120; ++current_floor) {
    if (const auto room = inspect_floor(current_floor)) return room;
  }
  current_floor = static_cast<std::int16_t>(signed_byte(exact[7]) - 1);
  for (; current_floor >= 0; --current_floor) {
    if (const auto room = inspect_floor(current_floor)) return room;
  }
  return std::nullopt;
}

OriginalTdtTenant* original_housekeeping_target_room(
    OriginalTdtDocument& document,
    const OriginalTdtPersonRecord& person) noexcept {
  const auto exact = std::span<const std::byte>(person.exact_bytes);
  const auto floor = signed_byte(exact[6]);
  const auto key = static_cast<std::int16_t>(
      load_u16(exact, 12U, document.header.byte_swapped));
  if (floor < 0 || floor >= 120 || key < 0 ||
      key >= static_cast<std::int16_t>(OriginalTdtFloor::kIndexCapacity)) {
    return nullptr;
  }
  return find_original_tenant(document, static_cast<std::uint8_t>(floor),
                              static_cast<std::uint8_t>(key));
}

bool clean_original_housekeeping_room(
    OriginalTdtDocument& document,
    OriginalTdtPersonRecord& housekeeper,
    bool& guest_changed) noexcept {
  auto* room = original_housekeeping_target_room(document, housekeeper);
  if (!room) return false;
  const auto type = signed_byte(room->exact_bytes[4]);
  const auto status = std::to_integer<std::uint8_t>(room->exact_bytes[5]);
  if (type < 3 || type > 5 || (status != 0x28U && status != 0x30U)) {
    return false;
  }

  set_original_tenant_status(
      *room, original_day_phase(document.header.frame_time) < 4 ? 0x18U
                                                                 : 0x20U);
  mark_original_tenant_changed(*room);
  const auto first_guest = original_tenant_people_start(
      *room, document.header.byte_swapped);
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (first_guest < person_limit) {
    document.people[static_cast<std::size_t>(first_guest)].exact_bytes[5] =
        std::byte{3};
    guest_changed = true;
  }
  return true;
}

bool reopen_original_housekeeping_room(
    OriginalTdtDocument& document,
    OriginalTdtPersonRecord& housekeeper,
    bool& guest_changed) noexcept {
  auto* room = original_housekeeping_target_room(document, housekeeper);
  if (!room) return false;
  const auto type = signed_byte(room->exact_bytes[4]);
  if (type < 3 || type > 5) return false;

  const auto first_guest = original_tenant_people_start(
      *room, document.header.byte_swapped);
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (first_guest < person_limit) {
    document.people[static_cast<std::size_t>(first_guest)].exact_bytes[5] =
        std::byte{0x24};
    guest_changed = true;
  }
  mark_original_tenant_changed(*room);
  return true;
}

std::optional<std::size_t> resolve_original_owned_person(
    OriginalTdtDocument& document,
    std::int16_t floor_number,
    std::int16_t key,
    std::uint16_t ordinal) noexcept {
  if (floor_number < 0 || floor_number >= 120 || key < 0 ||
      key >= static_cast<std::int16_t>(OriginalTdtFloor::kIndexCapacity)) {
    return std::nullopt;
  }
  const auto* tenant = find_original_tenant(
      document, static_cast<std::uint8_t>(floor_number),
      static_cast<std::uint8_t>(key));
  if (!tenant) return std::nullopt;
  const auto start = original_tenant_people_start(
      *tenant, document.header.byte_swapped);
  const auto index = static_cast<std::uint64_t>(start) + ordinal;
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (index >= person_limit) return std::nullopt;
  return static_cast<std::size_t>(index);
}

std::uint16_t next_original_people_random(
    OriginalTdtDocument& document) noexcept {
  // Microsoft C 7.0/Visual C++ 1.x rand() at 1000:3a2f.
  document.random_state = document.random_state * 0x015a4e35U + 1U;
  return static_cast<std::uint16_t>(
      (document.random_state >> 16U) & 0x7fffU);
}

std::int16_t select_original_commercial_service(
    OriginalTdtDocument& document,
    std::size_t family,
    std::size_t group) noexcept {
  std::span<const std::byte> block;
  std::size_t group_size{};
  switch (family) {
    case 0U:
      block = document.post_elevator.dynamic_dd5c;
      group_size = 0x26eU;
      break;
    case 1U:
      block = document.post_elevator.dynamic_dd60;
      group_size = 0x12eU;
      break;
    case 2U:
      block = document.post_elevator.dynamic_dd64;
      group_size = 0x1ceU;
      break;
    default:
      return -1;
  }

  auto group_count = [&](std::size_t selected_group) -> std::int16_t {
    const auto offset = selected_group * group_size;
    if (offset + 2U > block.size()) return 0;
    return std::bit_cast<std::int16_t>(
        load_u16(block, offset, document.header.byte_swapped));
  };
  if (group_count(group) == 0) group = 0U;
  const auto count = group_count(group);
  if (count <= 0) return -1;

  const auto ordinal = static_cast<std::size_t>(
      next_original_people_random(document) %
      static_cast<std::uint16_t>(count));
  const auto offset = group * group_size + 2U + ordinal * 2U;
  if (offset + 2U > block.size()) return -1;
  const auto selected = std::bit_cast<std::int16_t>(
      load_u16(block, offset, document.header.byte_swapped));
  if (selected < 0 ||
      selected >= static_cast<std::int16_t>(document.retail.size())) {
    return -1;
  }
  const auto status = signed_byte(
      document.retail[static_cast<std::size_t>(selected)].exact_bytes[2]);
  return status == -1 || status == 3 ? -1 : selected;
}

std::int16_t select_original_metro_service(
    OriginalTdtDocument& document) noexcept {
  // 11a8:1472 first chooses Retail/Restaurant/Fast Food, then 12dc selects
  // from group zero. The group-zero fallback is intentionally the same group
  // for Metro and therefore does not consume another rand() when it is empty.
  const auto family = static_cast<std::size_t>(
      next_original_people_random(document) % 3U);
  return select_original_commercial_service(document, family, 0U);
}

std::optional<std::size_t> original_entertainment_record_index(
    const OriginalTdtDocument& document,
    const OriginalTdtTenant& owner) noexcept {
  const auto index = std::bit_cast<std::int16_t>(
      load_u16(owner.exact_bytes, 6U, document.header.byte_swapped));
  if (index < 0 ||
      index >= static_cast<std::int16_t>(
                   document.post_elevator.dc24_records.size())) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(index);
}

std::size_t original_entertainment_capacity_lane(
    const std::array<std::byte, 0x0c>& record,
    std::int8_t owner_type) noexcept {
  // 1180:0ce7 chooses byte 4/5 from the paired-record sentinel and the exact
  // 18/34/29 facility-type split before testing and decrementing capacity.
  if (signed_byte(record[7]) >= 0) {
    return owner_type == 18 || owner_type == 34 ? 4U : 5U;
  }
  return owner_type == 29 ? 4U : 5U;
}

bool reserve_original_entertainment_capacity(
    std::array<std::byte, 0x0c>& record,
    std::int8_t owner_type) noexcept {
  const auto lane = original_entertainment_capacity_lane(record, owner_type);
  if (record[lane] == std::byte{0}) return false;
  record[lane] = static_cast<std::byte>(
      std::to_integer<std::uint8_t>(record[lane]) - 1U);
  return true;
}

void restore_original_entertainment_capacity(
    std::array<std::byte, 0x0c>& record,
    std::int8_t owner_type) noexcept {
  // Exact 1180:0d49 rollback uses the same paired/single and owner-type lane
  // selection as 0ce7, then performs an unchecked wrapping byte increment.
  const auto lane = original_entertainment_capacity_lane(record, owner_type);
  record[lane] = static_cast<std::byte>(
      std::to_integer<std::uint8_t>(record[lane]) + 1U);
}

void mark_original_entertainment_record_tenants(
    OriginalTdtDocument& document,
    const std::array<std::byte, 0x0c>& record) noexcept {
  const std::size_t adjacent_count = signed_byte(record[7]) >= 0 ? 2U : 1U;
  for (std::size_t side = 0U; side < 2U; ++side) {
    const auto floor_number = signed_byte(record[side]);
    const auto key = signed_byte(record[2U + side]);
    if (floor_number < 0 || floor_number >= 120 || key < 0 ||
        key >= static_cast<std::int16_t>(OriginalTdtFloor::kIndexCapacity)) {
      continue;
    }
    auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
    const auto first = floor.tenant_index[static_cast<std::size_t>(key)];
    if (first >= floor.tenants.size() ||
        floor.tenants[first].exact_bytes[12] !=
            static_cast<std::byte>(key)) {
      continue;
    }
    for (std::size_t offset = 0U;
         offset < adjacent_count && first + offset < floor.tenants.size();
         ++offset) {
      mark_original_tenant_changed(floor.tenants[first + offset]);
    }
  }
}

void enter_original_entertainment_record(
    OriginalTdtDocument& document,
    std::array<std::byte, 0x0c>& record) noexcept {
  // Exact 1180:0c29 entertainment-arrival update: promote state 1 to 2 and
  // dirty linked tenants once, then increment both visit counters every time.
  if (record[6] == std::byte{1}) {
    record[6] = std::byte{2};
    mark_original_entertainment_record_tenants(document, record);
  }
  record[10] = static_cast<std::byte>(
      std::to_integer<std::uint8_t>(record[10]) + 1U);
  record[11] = static_cast<std::byte>(
      std::to_integer<std::uint8_t>(record[11]) + 1U);
}

void decrement_original_condo_status(OriginalTdtTenant& owner) noexcept {
  // Shared native form of 1220:6f98/71fe: decrement the owning tenant's
  // status byte and raise its dirty byte after a resident/guest transition.
  set_original_tenant_status(
      owner, static_cast<std::uint8_t>(owner.status - 1U));
  mark_original_tenant_changed(owner);
}

void finish_original_condo_resident(
    OriginalTdtDocument& document,
    OriginalTdtTenant& owner) noexcept {
  // Exact 1220:7005 Condo owner transition. This is byte-for-byte parallel to
  // Hotel's 1220:6d82: status 0x10 becomes 1/9 at the day-phase boundary,
  // otherwise it increments, and the tenant dirty byte is always set.
  if (owner.status == 0x10U) {
    set_original_tenant_status(
        owner, original_day_phase(document.header.frame_time) < 4 ? 1U
                                                                   : 9U);
  } else {
    set_original_tenant_status(
        owner, static_cast<std::uint8_t>(owner.status + 1U));
  }
  mark_original_tenant_changed(owner);
}

bool synchronize_original_condo_residents(
    OriginalTdtDocument& document,
    OriginalTdtTenant& owner,
    std::uint16_t owner_ordinal) noexcept {
  // Exact 1220:7100 three-resident synchronization gate. Unless the owner's
  // low status bits already equal one, both other ordinals must be in state
  // 0x10 before the owner is moved to 0x10 and marked dirty.
  if ((owner.status & 7U) != 1U) {
    const auto start = original_tenant_people_start(
        owner, document.header.byte_swapped);
    const auto person_limit =
        std::min<std::size_t>(document.people_count, document.people.size());
    for (std::uint16_t ordinal = 0U; ordinal < 3U; ++ordinal) {
      if (ordinal == owner_ordinal) continue;
      const auto index = static_cast<std::uint64_t>(start) + ordinal;
      if (index >= person_limit ||
          document.people[static_cast<std::size_t>(index)].exact_bytes[5] !=
              std::byte{0x10}) {
        return false;
      }
    }
  }
  set_original_tenant_status(owner, 0x10U);
  mark_original_tenant_changed(owner);
  return true;
}

void activate_original_condo(
    OriginalTdtDocument& document,
    OriginalTdtTenant& owner,
    const OriginalYenTable& rent_income) noexcept {
  // Exact persisted portion of 1178:0fe3. The native caller consumes its
  // separate 1118:0a49 income-status request; this routine preserves the rent,
  // phase-selected status, dirty mark, population, and 1130:0cec metric reset.
  add_original_rent_income(
      document, rent_income, 9U,
      std::to_integer<std::uint8_t>(owner.exact_bytes[16]));
  set_original_tenant_status(
      owner, original_day_phase(document.header.frame_time) < 4 ? 0U : 8U);
  mark_original_tenant_changed(owner);
  add_original_population_for_type(document, 9U, 3);

  const auto start = original_tenant_people_start(
      owner, document.header.byte_swapped);
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  for (std::size_t ordinal = 0U; ordinal < 3U; ++ordinal) {
    const auto index = static_cast<std::uint64_t>(start) + ordinal;
    if (index >= person_limit) break;
    auto& exact =
        document.people[static_cast<std::size_t>(index)].exact_bytes;
    exact[9] = std::byte{0};
    store_u16(exact, 14U, 0U, document.header.byte_swapped);
  }
}

}  // namespace

bool original_person_has_parking(
    const OriginalTdtPersonRecord& person) noexcept {
  // Exact 1198:06a6 predicate: any of byte 13's upper six bits denotes a
  // live parking assignment.
  return (std::to_integer<std::uint8_t>(person.exact_bytes[13]) & 0xfcU) !=
         0U;
}

std::int16_t original_person_parking_floor(
    const OriginalTdtDocument& document,
    const OriginalTdtPersonRecord& person) noexcept {
  // Exact 1198:0650 decode: unassigned people use lobby floor ten; assigned
  // people use 10 minus signed word-12's upper-six-bit value.
  if (!original_person_has_parking(person)) return 10;
  const auto encoded = std::bit_cast<std::int16_t>(
      load_u16(person.exact_bytes, 12U, document.header.byte_swapped));
  return static_cast<std::int16_t>(10 - (encoded >> 10U));
}

bool original_person_parking_eligible(
    const OriginalTdtDocument& document,
    const OriginalTdtPersonRecord& person,
    const OriginalTdtTenant& owner,
    std::int16_t owner_floor,
    std::int16_t owner_key) noexcept {
  // Exact 1198:06e7 rating/type/ordinal predicate, including signed IDIV
  // remainder for the Office floor-plus-key cadence.
  const auto ordinal = std::bit_cast<std::int16_t>(
      load_u16(person.exact_bytes, 2U, document.header.byte_swapped));
  const auto owner_type = signed_byte(owner.exact_bytes[4]);
  return document.header.rating >= 3U &&
         ((owner_type == 5 && ordinal == 0) ||
          (owner_type == 7 && (owner_floor + owner_key) % 4 == 1 &&
           ordinal == 2));
}

namespace {

struct OriginalHotelParkingAllocation {
  bool assigned{};
  bool changed{};
  bool malformed{};
  std::uint8_t notification_code{};
};

OriginalHotelParkingAllocation allocate_original_hotel_parking(
    OriginalTdtDocument& document,
    std::size_t person_index,
    OriginalTdtTenant& owner,
    std::int16_t owner_floor,
    std::int16_t owner_key) noexcept {
  OriginalHotelParkingAllocation result{};
  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);
  if (load_u16(exact, 12U, document.header.byte_swapped) != 0U) {
    result.changed = true;
  }
  // 1198:031a performs this write before its eligibility test.
  store_u16(exact, 12U, 0U, document.header.byte_swapped);

  const bool eligible = original_person_parking_eligible(
      document, person, owner, owner_floor, owner_key);
  if (!eligible) return result;
  const auto owner_type = signed_byte(owner.exact_bytes[4]);

  auto& tail = document.post_elevator;
  const auto connected = tail.parking_connected;
  if (connected <= 0 ||
      connected > static_cast<std::int16_t>(tail.parking_entries.size())) {
    result.notification_code = 5U;
    return result;
  }
  // Exact 1198:0621 connected-parking selector (the native PRNG result is the
  // nonnegative magnitude produced by the original rand/abs sequence).
  const auto selected_ordinal = static_cast<std::size_t>(
      next_original_people_random(document) %
      static_cast<std::uint16_t>(connected));
  const auto selected = tail.parking_entries[selected_ordinal];
  if (selected >= tail.cf9c_records.size()) {
    result.notification_code = 5U;
    result.malformed = true;
    return result;
  }
  auto& record = tail.cf9c_records[selected];
  const auto parking_floor = signed_byte(record[0]);
  const auto parking_key = signed_byte(record[1]);
  if (parking_floor < 0 || parking_floor >= 120 || parking_key < 0 ||
      parking_key >=
          static_cast<std::int16_t>(OriginalTdtFloor::kIndexCapacity)) {
    result.notification_code = 5U;
    result.malformed = true;
    return result;
  }
  auto* parking = find_original_tenant(
      document, static_cast<std::uint8_t>(parking_floor),
      static_cast<std::uint8_t>(parking_key));
  if (!parking) {
    result.notification_code = 5U;
    result.malformed = true;
    return result;
  }
  if (!increment_original_parking_population(document, owner_type)) {
    result.notification_code = 5U;
    return result;
  }

  store_u32(record, 2U, static_cast<std::uint32_t>(person_index),
            document.header.byte_swapped);
  store_u16(exact, 12U,
            static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(10 - parking_floor) << 10U),
            document.header.byte_swapped);
  const auto status = static_cast<std::uint8_t>(
      next_original_people_random(document) % 13U + 2U);
  set_original_tenant_status(*parking, status);
  mark_original_tenant_changed(*parking);
  result.assigned = true;
  result.changed = true;
  return result;
}

void decrement_original_hotel_status(OriginalTdtTenant& owner) noexcept {
  set_original_tenant_status(
      owner, static_cast<std::uint8_t>(owner.status - 1U));
  mark_original_tenant_changed(owner);
}

void finish_original_hotel_guest(
    OriginalTdtDocument& document,
    OriginalTdtTenant& owner) noexcept {
  // Exact 1220:6d82 Hotel owner transition used by normal arrival, commercial
  // return, and the shared 1220:1aed transit arrival: 0x10 becomes 1 before
  // day phase four or 9 afterward; every other status increments; byte 19 is
  // dirtied in all cases.
  if (owner.status == 0x10U) {
    set_original_tenant_status(
        owner, original_day_phase(document.header.frame_time) < 4 ? 1U
                                                                   : 9U);
  } else {
    set_original_tenant_status(
        owner, static_cast<std::uint8_t>(owner.status + 1U));
  }
  mark_original_tenant_changed(owner);
}

bool synchronize_original_hotel_guests(
    OriginalTdtDocument& document,
    OriginalTdtTenant& owner,
    std::uint16_t owner_ordinal) noexcept {
  // Exact 1220:6e7d paired-guest gate. A type-3 room or an owner status whose
  // low three bits equal one synchronizes directly; otherwise the routine
  // examines guest ordinal (3-current) for state 0x10 before setting the
  // owner to 0x10 and dirtying it.
  bool synchronized = signed_byte(owner.exact_bytes[4]) == 3 ||
                      (owner.status & 7U) == 1U;
  if (!synchronized) {
    const auto counterpart =
        static_cast<std::uint16_t>(3U - owner_ordinal);
    const auto start = original_tenant_people_start(
        owner, document.header.byte_swapped);
    const auto candidate = static_cast<std::uint64_t>(start) + counterpart;
    const auto limit =
        std::min<std::size_t>(document.people_count, document.people.size());
    synchronized = candidate < limit &&
                   document.people[static_cast<std::size_t>(candidate)]
                           .exact_bytes[5] == std::byte{0x10};
  }
  if (!synchronized) return false;
  set_original_tenant_status(owner, 0x10U);
  mark_original_tenant_changed(owner);
  return true;
}

void activate_original_hotel_room(
    OriginalTdtDocument& document,
    OriginalTdtTenant& owner) noexcept {
  // Exact 1178:0df9 activation: choose status 0/8 from day phase, dirty the
  // tenant, clear subtype byte 17, and add one type-3 or two type-4/5 guests.
  set_original_tenant_status(
      owner, original_day_phase(document.header.frame_time) < 4 ? 0U : 8U);
  mark_original_tenant_changed(owner);
  owner.exact_bytes[17] = std::byte{0};
  owner.subtype = 0U;
  add_original_population_for_type(
      document, static_cast<std::uint8_t>(owner.type),
      owner.type == 3 ? 1 : 2);
}

void checkout_original_hotel_room(
    OriginalTdtDocument& document,
    OriginalTdtTenant& owner,
    const OriginalYenTable& rent_income) noexcept {
  set_original_tenant_status(
      owner, original_day_phase(document.header.frame_time) < 4 ? 0x28U
                                                                 : 0x30U);
  mark_original_tenant_changed(owner);
  owner.exact_bytes[14] = std::byte{0};
  owner.preserved_07_to_0f[7] = std::byte{0};
  owner.exact_bytes[17] = std::byte{0};
  owner.subtype = 0U;
  add_original_rent_income(
      document, rent_income, static_cast<std::uint8_t>(owner.type),
      std::to_integer<std::uint8_t>(owner.exact_bytes[16]));

  document.hotel_checkout_effect_active = true;
  document.hotel_checkout_count = static_cast<std::uint16_t>(
      document.hotel_checkout_count + 1U);
  if (document.hotel_checkout_count < 20U) {
    document.hotel_checkout_effect_cadence =
        document.hotel_checkout_count % 2U == 0U;
  } else {
    document.hotel_checkout_effect_cadence =
        document.hotel_checkout_count % 8U == 0U;
  }
  add_original_population_for_type(
      document, static_cast<std::uint8_t>(owner.type),
      owner.type == 3 ? -1 : -2);
}

bool original_hotel_periodic_visitor_matches(
    const OriginalTdtDocument& document,
    std::size_t person_index) noexcept {
  // Exact 1240:020d predicate over DS:b928 and the tracked DS:b924 person.
  return document.post_elevator.b928 != 0U &&
         document.post_elevator.b924 ==
             static_cast<std::int32_t>(person_index);
}

void append_original_hotel_process_request(
    OriginalHotelPersonStepResult& result,
    std::uint16_t code,
    std::int32_t amount = 0) {
  result.process_requests.push_back({code, amount});
  result.host_requests.push_back(
      {OriginalPersonHostRequestKind::hotel_dialog, code, amount});
}

void append_original_income_status_request(
    std::vector<OriginalPersonHostRequest>& requests,
    std::uint16_t code) {
  requests.push_back(
      {OriginalPersonHostRequestKind::income_status, code, 0});
}

void append_original_notification_status_request(
    std::vector<OriginalPersonHostRequest>& requests,
    std::uint16_t code) {
  if (code == 0U) return;
  requests.push_back(
      {OriginalPersonHostRequestKind::notification_status, code, 0});
}

void clear_original_hotel_periodic_visitor(
    OriginalTdtDocument& document,
    std::size_t person_index,
    OriginalHotelPersonStepResult& result) {
  if (!original_hotel_periodic_visitor_matches(document, person_index)) return;
  document.post_elevator.b923 = 0U;
  document.post_elevator.b928 = 0U;
  document.post_elevator.b924 = -1;
  result.periodic_visitor_changed = true;
  result.changed = true;
  append_original_hotel_process_request(result, 3003U);
}

void notify_original_hotel_periodic_arrival(
    const OriginalTdtDocument& document,
    std::size_t person_index,
    OriginalHotelPersonStepResult& result) {
  if (original_hotel_periodic_visitor_matches(document, person_index)) {
    append_original_hotel_process_request(result, 3001U);
  }
}

void finish_original_hotel_periodic_visit(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPartTable& part,
    OriginalHotelPersonStepResult& result) {
  if (!original_hotel_periodic_visitor_matches(document, person_index)) return;
  const auto& person = document.people[person_index];
  const auto divisor = signed_byte(person.exact_bytes[9]);
  std::int16_t score{};
  if (divisor != 0) {
    const auto numerator = std::bit_cast<std::int16_t>(
        load_u16(person.exact_bytes, 14U, document.header.byte_swapped));
    score = static_cast<std::int16_t>(numerator / divisor);
  }
  std::size_t threshold_index = 10U;
  if (document.header.rating <= 2U) {
    threshold_index = 8U;
  } else if (document.header.rating == 3U) {
    threshold_index = 9U;
  }
  const auto threshold = std::bit_cast<std::int16_t>(
      part.words_00_to_40[threshold_index]);
  if (score <= threshold) {
    document.post_elevator.b923 = 1U;
    append_original_hotel_process_request(result, 3002U);
  } else {
    document.post_elevator.b923 = 0U;
    append_original_hotel_process_request(result, 3003U);
  }
  document.post_elevator.b928 = 0U;
  document.post_elevator.b924 = -1;
  result.periodic_visitor_changed = true;
  result.changed = true;
}

void start_original_hotel_periodic_visit(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalTdtTenant& owner,
    std::int16_t owner_floor,
    OriginalHotelPersonStepResult& result) {
  const auto& person = document.people[person_index];
  if (document.header.current_day % 9 != 3 ||
      document.header.rating != 3U || document.post_elevator.b923 != 0U ||
      document.post_elevator.b928 != 0U || owner.type != 5 ||
      load_u16(person.exact_bytes, 2U, document.header.byte_swapped) != 1U ||
      !original_person_has_parking(person)) {
    return;
  }
  document.post_elevator.b928 = 1U;
  document.post_elevator.b924 = static_cast<std::int32_t>(person_index);
  result.periodic_visitor_changed = true;
  result.changed = true;
  append_original_hotel_process_request(
      result, 3000U,
      static_cast<std::int32_t>(owner_floor - 9) * 10000);
}

OriginalTdtTenant* original_retail_service_tenant(
    OriginalTdtDocument& document,
    const OriginalTdtRetailRecord& service) noexcept {
  const auto floor = signed_byte(service.exact_bytes[0]);
  const auto key = signed_byte(service.exact_bytes[1]);
  if (floor < 0 || floor >= 120 || key < 0 ||
      key >= static_cast<std::int16_t>(OriginalTdtFloor::kIndexCapacity)) {
    return nullptr;
  }
  return find_original_tenant(document, static_cast<std::uint8_t>(floor),
                              static_cast<std::uint8_t>(key));
}

void update_original_retail_service_status(
    OriginalTdtDocument& document,
    OriginalTdtRetailRecord& service,
    bool entering,
    bool& tenant_marked_dirty) noexcept {
  // Exact 11a8:0bd5 threshold table. Only the 0/1/9/10 population edges
  // change the service state or dirty the linked tenant.
  const auto population =
      std::to_integer<std::uint8_t>(service.exact_bytes[9]);
  std::optional<std::uint8_t> status;
  if (entering) {
    if (population == 1U) {
      status = 1U;
    } else if (population == 10U) {
      status = 2U;
    }
  } else {
    if (population == 0U) {
      status = 0U;
    } else if (population == 9U) {
      status = 1U;
    }
  }
  if (!status) return;
  service.exact_bytes[2] = static_cast<std::byte>(*status);
  if (auto* tenant = original_retail_service_tenant(document, service)) {
    mark_original_tenant_changed(*tenant);
    tenant_marked_dirty = true;
  }
}

std::int16_t enter_original_metro_service(
    OriginalTdtDocument& document,
    OriginalTdtPersonRecord& person,
    std::int16_t service_index,
    const OriginalPartTable& part,
    bool& population_changed,
    bool& tenant_marked_dirty) noexcept {
  auto person_exact = std::span<std::byte>(person.exact_bytes);
  if (service_index < 0 ||
      service_index >= static_cast<std::int16_t>(document.retail.size())) {
    store_u16(person_exact, 10U, document.header.frame_time,
              document.header.byte_swapped);
    return 3;
  }
  auto& service = document.retail[static_cast<std::size_t>(service_index)];
  auto service_exact = std::span<std::byte>(service.exact_bytes);
  const auto floor = signed_byte(service_exact[0]);
  const auto key = signed_byte(service_exact[1]);
  const auto service_status = signed_byte(service_exact[2]);

  if (key == -1 || service_status == -1 || service_status == 3) {
    add_original_person_waiting_delay(
        person, part.words_00_to_40[4U], document.header.byte_swapped);
    apply_original_person_metric_finalizer(
        person, document.header.byte_swapped);
    person_exact[7] = static_cast<std::byte>(floor);
    person_exact[8] = std::byte{0xfe};
    store_u16(person_exact, 10U, document.header.frame_time,
              document.header.byte_swapped);
    return -1;
  }

  if (signed_byte(service_exact[9]) < 0x28) {
    service_exact[9] = static_cast<std::byte>(
        std::to_integer<std::uint8_t>(service_exact[9]) + 1U);
    population_changed = true;
    if (signed_byte(person_exact[0]) != floor ||
        signed_byte(person_exact[1]) != key) {
      store_u16(service_exact, 16U,
                static_cast<std::uint16_t>(
                    load_u16(service_exact, 16U,
                             document.header.byte_swapped) +
                    1U),
                document.header.byte_swapped);
    }
    update_original_retail_service_status(
        document, service, true, tenant_marked_dirty);
    if (auto* tenant = original_retail_service_tenant(document, service)) {
      mark_original_tenant_changed(*tenant);
      tenant_marked_dirty = true;
    }
    store_u16(person_exact, 10U, document.header.frame_time,
              document.header.byte_swapped);
    return 3;
  }

  person_exact[7] = static_cast<std::byte>(floor);
  person_exact[8] = std::byte{0xfe};
  store_u16(person_exact, 10U, document.header.frame_time,
            document.header.byte_swapped);
  return 2;
}

std::uint16_t original_retail_service_dwell(
    OriginalTdtDocument& document,
    const OriginalTdtRetailRecord& service,
    const OriginalPartTable& part) noexcept {
  // Exact 11a8:12a4 selector: Restaurant, Retail and Fast Food use PART head
  // words 21, 27 and 14 respectively; every other tenant type returns zero.
  const auto* tenant = original_retail_service_tenant(document, service);
  if (!tenant) return 0U;
  switch (signed_byte(tenant->exact_bytes[4])) {
    case 6:
      return part.words_00_to_40[21U];  // DS:dda4
    case 10:
      return part.words_00_to_40[27U];  // DS:ddb0
    case 12:
      return part.words_00_to_40[14U];  // DS:dd96
    default:
      return 0U;
  }
}

bool leave_original_metro_service(
    OriginalTdtDocument& document,
    OriginalTdtPersonRecord& person,
    std::int16_t service_index,
    const OriginalPartTable& part,
    bool& population_changed,
    bool& tenant_marked_dirty) noexcept {
  auto person_exact = std::span<std::byte>(person.exact_bytes);
  std::int16_t floor = 10;
  if (service_index >= 0 &&
      service_index < static_cast<std::int16_t>(document.retail.size())) {
    auto& service = document.retail[static_cast<std::size_t>(service_index)];
    auto service_exact = std::span<std::byte>(service.exact_bytes);
    floor = signed_byte(service_exact[0]);
    const auto key = signed_byte(service_exact[1]);
    const auto status = signed_byte(service_exact[2]);
    if (key != -1 && status != -1 && status != 3) {
      const auto elapsed = std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(
              document.header.frame_time -
              load_u16(person_exact, 10U, document.header.byte_swapped)));
      const auto dwell = std::bit_cast<std::int16_t>(
          original_retail_service_dwell(document, service, part));
      if (elapsed < dwell) return false;
      service_exact[9] = static_cast<std::byte>(
          std::to_integer<std::uint8_t>(service_exact[9]) - 1U);
      population_changed = true;
      update_original_retail_service_status(
          document, service, false, tenant_marked_dirty);
    }
  }
  person_exact[7] = static_cast<std::byte>(floor);
  person_exact[8] = std::byte{0xfe};
  return true;
}

bool reserve_original_food_service(
    OriginalTdtDocument& document,
    const OriginalTdtPersonRecord& person,
    std::int16_t service_index) noexcept {
  if (service_index < 0 ||
      service_index >= static_cast<std::int16_t>(document.retail.size())) {
    return false;
  }
  auto exact = std::span<std::byte>(
      document.retail[static_cast<std::size_t>(service_index)].exact_bytes);
  if (exact[6] == std::byte{0}) return false;

  const auto reservation_word = std::bit_cast<std::int16_t>(
      load_u16(exact, 12U, document.header.byte_swapped));
  if (reservation_word < 0) {
    // 11a8:10b3 performs 16-bit NEG/INC, then a signed comparison against
    // the owning facility's person ordinal.
    const auto limit_bits = static_cast<std::uint16_t>(
        0U - static_cast<std::uint16_t>(reservation_word) + 1U);
    const auto limit = std::bit_cast<std::int16_t>(limit_bits);
    const auto ordinal = std::bit_cast<std::int16_t>(
        load_u16(person.exact_bytes, 2U, document.header.byte_swapped));
    if (ordinal > limit) return false;
  }

  exact[6] = static_cast<std::byte>(
      std::to_integer<std::uint8_t>(exact[6]) - 1U);
  exact[7] = static_cast<std::byte>(
      std::to_integer<std::uint8_t>(exact[7]) + 1U);
  store_u16(exact, 16U,
            static_cast<std::uint16_t>(
                load_u16(exact, 16U, document.header.byte_swapped) + 1U),
            document.header.byte_swapped);
  return true;
}

void undo_original_food_service_reservation(
    OriginalTdtDocument& document,
    std::int16_t service_index) noexcept {
  if (service_index < 0 ||
      service_index >= static_cast<std::int16_t>(document.retail.size())) {
    return;
  }
  auto exact = std::span<std::byte>(
      document.retail[static_cast<std::size_t>(service_index)].exact_bytes);
  exact[6] = static_cast<std::byte>(
      std::to_integer<std::uint8_t>(exact[6]) + 1U);
  exact[7] = static_cast<std::byte>(
      std::to_integer<std::uint8_t>(exact[7]) - 1U);
  store_u16(exact, 16U,
            static_cast<std::uint16_t>(
                load_u16(exact, 16U, document.header.byte_swapped) - 1U),
            document.header.byte_swapped);
}

std::int16_t original_food_service_person_performance(
    const OriginalTdtDocument& document,
    const OriginalTdtPersonRecord& person) noexcept {
  const auto divisor = signed_byte(person.exact_bytes[9]);
  if (divisor == 0) return 0;
  const auto numerator = std::bit_cast<std::int16_t>(
      load_u16(person.exact_bytes, 14U, document.header.byte_swapped));
  const auto quotient = static_cast<std::int32_t>(numerator) /
                        static_cast<std::int32_t>(divisor);
  return std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(quotient));
}

std::size_t original_food_service_history_lane(
    const OriginalTdtDocument& document) noexcept {
  if (document.header.version_20_word != 0U) return 5U;
  return original_calendar_phase(document.header.current_day) == 0U ? 3U
                                                                    : 4U;
}

std::int16_t original_food_service_history_capacity(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::int8_t facility_type) noexcept {
  const auto lane = original_food_service_history_lane(document) - 3U;
  std::size_t base{};
  switch (facility_type) {
    case 6:
      base = 22U;  // DS:dda6/dda8/ddaa
      break;
    case 10:
      base = 28U;  // DS:ddb2/ddb4/ddb6
      break;
    case 12:
      base = 15U;  // DS:dd98/dd9a/dd9c
      break;
    default:
      return 0;
  }
  return std::bit_cast<std::int16_t>(part.words_00_to_40[base + lane]);
}

std::int16_t original_food_service_performance_threshold(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    bool upper) noexcept {
  std::size_t rating_band{};
  if (document.header.rating == 1U || document.header.rating == 2U) {
    rating_band = 0U;
  } else if (document.header.rating == 3U) {
    rating_band = 1U;
  } else {
    rating_band = 2U;
  }
  return std::bit_cast<std::int16_t>(
      part.words_00_to_40[(upper ? 8U : 5U) + rating_band]);
}

bool update_original_food_service_history(
    OriginalTdtDocument& document,
    const OriginalTdtPersonRecord& person,
    std::int8_t facility_type,
    std::int16_t service_index,
    const OriginalPartTable& part) noexcept {
  if (service_index < 0 ||
      service_index >= static_cast<std::int16_t>(document.retail.size())) {
    return false;
  }
  const auto performance =
      original_food_service_person_performance(document, person);
  if (performance < 0) return false;

  auto& exact =
      document.retail[static_cast<std::size_t>(service_index)].exact_bytes;
  const auto lane = original_food_service_history_lane(document);
  auto replacement = static_cast<std::int16_t>(signed_byte(exact[lane]));
  if (performance <
      original_food_service_performance_threshold(document, part, false)) {
    replacement = static_cast<std::int16_t>(replacement + 2);
  } else if (performance <
             original_food_service_performance_threshold(document, part,
                                                         true)) {
    replacement = static_cast<std::int16_t>(replacement + 1);
  }
  replacement = std::min(
      replacement, original_food_service_history_capacity(
                       document, part, facility_type));
  const auto stored = static_cast<std::byte>(replacement);
  if (exact[lane] == stored) return false;
  exact[lane] = stored;
  return true;
}

void activate_original_retail_store(
    OriginalTdtDocument& document,
    OriginalTdtTenant& owner,
    std::int16_t service_index,
    const OriginalYenTable& rent_income,
    bool& activation_visual_requested) noexcept {
  // Exact persisted portion of 1178:1140 with its literal third argument
  // zero. 1118:0a49(6) is process/UI state and is returned to the host.
  add_original_rent_income(
      document, rent_income, 10U,
      std::to_integer<std::uint8_t>(owner.exact_bytes[16]));
  activation_visual_requested = true;

  if (service_index >= 0 &&
      service_index < static_cast<std::int16_t>(document.retail.size())) {
    document.retail[static_cast<std::size_t>(service_index)].exact_bytes[2] =
        std::byte{0};
  }
  mark_original_tenant_changed(owner);
  add_original_population_for_type(document, 10U, 10);

  const auto start = original_tenant_people_start(
      owner, document.header.byte_swapped);
  const auto limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  for (std::size_t ordinal = 0U; ordinal < 48U; ++ordinal) {
    const auto index = static_cast<std::uint64_t>(start) + ordinal;
    if (index >= limit) break;
    auto& person = document.people[static_cast<std::size_t>(index)].exact_bytes;
    person[9] = std::byte{0};
    store_u16(person, 14U, 0U, document.header.byte_swapped);
  }
}

}  // namespace

void enter_original_office(OriginalTdtTenant& owner) noexcept {
  // Exact 1220:6bef status entry: 8 wraps to 1, every other value increments,
  // and byte 19 is marked dirty.
  set_original_tenant_status(
      owner, owner.status == 8U
                 ? 1U
                 : static_cast<std::uint8_t>(owner.status + 1U));
  mark_original_tenant_changed(owner);
}

void leave_original_office(OriginalTdtDocument& document,
                           OriginalTdtTenant& owner) noexcept {
  // Exact 1220:6cb6 status exit: decrement, change zero to 8 at day phase four
  // or later, and mark the tenant dirty.
  set_original_tenant_status(
      owner, static_cast<std::uint8_t>(owner.status - 1U));
  if (owner.status == 0U &&
      original_day_phase(document.header.frame_time) >= 4) {
    set_original_tenant_status(owner, 8U);
  }
  mark_original_tenant_changed(owner);
}

namespace {

void reset_original_office_metrics(OriginalTdtDocument& document,
                                   const OriginalTdtTenant& owner) noexcept {
  const auto start = original_tenant_people_start(
      owner, document.header.byte_swapped);
  const auto limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  for (std::size_t ordinal = 0U; ordinal < 6U; ++ordinal) {
    const auto index = static_cast<std::uint64_t>(start) + ordinal;
    if (index >= limit) break;
    auto& exact = document.people[static_cast<std::size_t>(index)].exact_bytes;
    exact[9] = std::byte{0};
    store_u16(exact, 14U, 0U, document.header.byte_swapped);
  }
}

void activate_original_office(
    OriginalTdtDocument& document,
    OriginalTdtTenant& owner,
    const OriginalYenTable& rent_income) noexcept {
  // Persisted portion of 1178:0cb4 with literal third argument zero.
  add_original_rent_income(
      document, rent_income, 7U,
      std::to_integer<std::uint8_t>(owner.exact_bytes[16]));
  set_original_tenant_status(owner, 0U);
  mark_original_tenant_changed(owner);
  add_original_population_for_type(document, 7U, 6);
  reset_original_office_metrics(document, owner);
}

}  // namespace

std::int16_t select_original_medical_service(
    OriginalTdtDocument& document,
    std::int16_t owner_floor) noexcept {
  // Exact 1170:056f seven-band, 22-byte Medical Center route table. Empty
  // bands fall back to band zero and a random live word selects the service.
  auto group = static_cast<std::int16_t>((owner_floor - 9) / 15);
  if (group < 0) group = 0;
  if (group >= 7) return -1;

  constexpr std::size_t kGroupSize = 0x16U;
  const auto table = std::span<const std::byte>(document.medical_route_index);
  const auto count_for_group = [&](std::size_t selected) noexcept {
    return load_u16(table, selected * kGroupSize,
                    document.header.byte_swapped);
  };
  auto selected_group = static_cast<std::size_t>(group);
  if (count_for_group(selected_group) == 0U) selected_group = 0U;
  const auto count = count_for_group(selected_group);
  if (count == 0U || count > 10U) return -1;
  const auto ordinal = static_cast<std::size_t>(
      next_original_people_random(document) % count);
  const auto offset = selected_group * kGroupSize + 2U + ordinal * 2U;
  const auto selected = std::bit_cast<std::int16_t>(
      load_u16(table, offset, document.header.byte_swapped));
  return selected >= 0 &&
                 selected < static_cast<std::int16_t>(
                                document.post_elevator.dbfc_dwords.size())
             ? selected
             : -1;
}

namespace {

std::int8_t original_medical_floor(std::uint32_t record) noexcept {
  return std::bit_cast<std::int8_t>(
      static_cast<std::uint8_t>(record & 0xffU));
}

std::int8_t original_medical_key(std::uint32_t record) noexcept {
  return std::bit_cast<std::int8_t>(
      static_cast<std::uint8_t>((record >> 8U) & 0xffU));
}

std::uint8_t original_medical_population(std::uint32_t record) noexcept {
  return static_cast<std::uint8_t>((record >> 16U) & 0xffU);
}

void set_original_medical_population(std::uint32_t& record,
                                     std::uint8_t population) noexcept {
  record = (record & 0xff00ffffU) |
           (static_cast<std::uint32_t>(population) << 16U);
}

enum class OriginalOfficeMedicalEnterStatus : std::int8_t {
  malformed = -2,
  unavailable = -1,
  full = 2,
  entered = 3,
};

OriginalOfficeMedicalEnterStatus enter_original_office_medical_service(
    OriginalTdtDocument& document,
    OriginalTdtPersonRecord& person,
    std::int16_t service_index,
    const OriginalPartTable& part,
    bool& population_changed,
    bool& tenant_marked_dirty) noexcept {
  auto exact = std::span<std::byte>(person.exact_bytes);
  if (service_index < 0) {
    store_u16(exact, 10U, document.header.frame_time,
              document.header.byte_swapped);
    return OriginalOfficeMedicalEnterStatus::entered;
  }
  if (service_index >= static_cast<std::int16_t>(
                           document.post_elevator.dbfc_dwords.size())) {
    return OriginalOfficeMedicalEnterStatus::malformed;
  }

  auto& record = document.post_elevator.dbfc_dwords[
      static_cast<std::size_t>(service_index)];
  const auto floor = original_medical_floor(record);
  const auto key = original_medical_key(record);
  if (key < 0) {
    // Exact 1170:02cd-0339 unavailable-service branch: after the route
    // resolver has finalized the same-floor leg, apply DS:dd82 (PART/1000
    // word four) through 11d8:02f7 and immediately finalize that penalty
    // through 11d8:0000 before publishing floor/FD. Unlike the full-service
    // branch below, this early return deliberately does not restart word 10.
    add_original_person_waiting_delay(
        person, part.words_00_to_40[4U], document.header.byte_swapped);
    apply_original_person_metric_finalizer(
        person, document.header.byte_swapped);
    exact[7] = static_cast<std::byte>(floor);
    exact[8] = std::byte{0xfd};
    return OriginalOfficeMedicalEnterStatus::unavailable;
  }

  auto population = original_medical_population(record);
  if (std::bit_cast<std::int8_t>(population) < 0x28) {
    population = static_cast<std::uint8_t>(population + 1U);
    set_original_medical_population(record, population);
    population_changed = true;
    if (population == 1U) {
      auto* tenant = find_original_tenant(
          document, static_cast<std::uint8_t>(floor),
          static_cast<std::uint8_t>(key));
      if (!tenant) return OriginalOfficeMedicalEnterStatus::malformed;
      mark_original_tenant_changed(*tenant);
      tenant_marked_dirty = true;
    }
    store_u16(exact, 10U, document.header.frame_time,
              document.header.byte_swapped);
    return OriginalOfficeMedicalEnterStatus::entered;
  }

  exact[7] = static_cast<std::byte>(floor);
  exact[8] = std::byte{0xfd};
  store_u16(exact, 10U, document.header.frame_time,
            document.header.byte_swapped);
  return OriginalOfficeMedicalEnterStatus::full;
}

enum class OriginalOfficeMedicalLeaveStatus : std::uint8_t {
  waiting,
  left,
  malformed,
};

OriginalOfficeMedicalLeaveStatus leave_original_office_medical_service(
    OriginalTdtDocument& document,
    OriginalTdtPersonRecord& person,
    std::int16_t service_index,
    bool& population_changed,
    bool& tenant_marked_dirty) noexcept {
  // Exact 1170:0414: negative service means floor ten; a live service holds
  // the person for 16 ticks, then decrements population and dirties the tenant
  // when the last patient leaves before publishing floor/FD.
  auto exact = std::span<std::byte>(person.exact_bytes);
  std::int8_t floor = 10;
  if (service_index >= 0) {
    if (service_index >= static_cast<std::int16_t>(
                             document.post_elevator.dbfc_dwords.size())) {
      return OriginalOfficeMedicalLeaveStatus::malformed;
    }
    auto& record = document.post_elevator.dbfc_dwords[
        static_cast<std::size_t>(service_index)];
    floor = original_medical_floor(record);
    const auto key = original_medical_key(record);
    if (key >= 0) {
      const auto elapsed = std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(
              document.header.frame_time -
              load_u16(exact, 10U, document.header.byte_swapped)));
      if (elapsed < 0x10) {
        return OriginalOfficeMedicalLeaveStatus::waiting;
      }
      auto population = static_cast<std::uint8_t>(
          original_medical_population(record) - 1U);
      set_original_medical_population(record, population);
      population_changed = true;
      if (population == 0U) {
        auto* tenant = find_original_tenant(
            document, static_cast<std::uint8_t>(floor),
            static_cast<std::uint8_t>(key));
        if (!tenant) return OriginalOfficeMedicalLeaveStatus::malformed;
        mark_original_tenant_changed(*tenant);
        tenant_marked_dirty = true;
      }
    }
  }
  exact[7] = static_cast<std::byte>(floor);
  exact[8] = std::byte{0xfd};
  return OriginalOfficeMedicalLeaveStatus::left;
}

}  // namespace

OriginalHotelCheckoutPresentation
consume_original_hotel_checkout_presentation(
    OriginalTdtDocument& document) noexcept {
  if (!document.hotel_checkout_effect_active) return {};
  OriginalHotelCheckoutPresentation result{
      true, document.hotel_checkout_effect_cadence};
  document.hotel_checkout_effect_cadence = true;
  document.hotel_checkout_effect_active = false;
  return result;
}

OriginalPersonTransportSelection
select_original_person_transport_for_information(
    const OriginalTdtDocument& document,
    std::int16_t source_floor,
    std::int16_t destination_floor,
    std::uint16_t person_x,
    bool tracked_route) noexcept {
  const auto selection = select_original_person_transport(
      document, source_floor, destination_floor, person_x, tracked_route);
  return {selection.transport_index, selection.direction_up};
}

OriginalElevatorWaitingPersonHit
original_elevator_waiting_person_hit_from_client(
    const OriginalTdtDocument& document,
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept {
  const int world_x = client_x + view_x;
  const int world_y = client_y + view_y;
  const int floor_number = 120 - world_y / 36 - 1;
  if (floor_number < 0 ||
      floor_number >= static_cast<int>(document.floors.size())) {
    return {};
  }
  const auto floor = static_cast<std::int16_t>(floor_number);
  const int x = world_x / 8;

  struct FloorElevator {
    std::size_t index{};
    std::int16_t x{};
  };
  std::array<FloorElevator, 24> shafts{};
  std::size_t shaft_count = 0U;
  for (std::size_t index = 0U; index < document.elevators.size(); ++index) {
    const auto& elevator = document.elevators[index];
    if (elevator.used == 0U ||
        floor < static_cast<std::int16_t>(elevator.bottom_floor - 1) ||
        floor > static_cast<std::int16_t>(elevator.top_floor + 1)) {
      continue;
    }
    shafts[shaft_count++] = {
        index, static_cast<std::int16_t>(elevator.x)};
  }

  // Direct translation of 10a8:00a8-019d. The original uses Shell's
  // diminishing gap and swaps only when the left x is strictly greater.
  for (std::int16_t gap = static_cast<std::int16_t>(shaft_count);;) {
    gap = static_cast<std::int16_t>(gap / 2);
    if (gap <= 0) break;
    for (std::int16_t end = gap;
         end < static_cast<std::int16_t>(shaft_count); ++end) {
      for (std::int16_t left = static_cast<std::int16_t>(end - gap);
           left >= 0; left = static_cast<std::int16_t>(left - gap)) {
        const auto right = static_cast<std::size_t>(left + gap);
        if (shafts[static_cast<std::size_t>(left)].x <= shafts[right].x) {
          break;
        }
        std::swap(shafts[static_cast<std::size_t>(left)], shafts[right]);
      }
    }
  }

  const auto person_width = [&](std::uint32_t person_index) noexcept {
    if (person_index >= document.people.size() ||
        person_index >= document.people_count) {
      return 0;
    }
    const auto& exact = document.people[person_index].exact_bytes;
    const auto type = signed_byte(exact[4]);
    switch (type) {
      case 3:
      case 4:
      case 5:
      case 7:
      case 14:
        return 1;
      case 9:
        return load_u16(exact, 2U, document.header.byte_swapped) == 1U
                   ? 2
                   : 1;
      case 15:
        return 2;
      default: {
        const auto flags = static_cast<std::uint16_t>(
            load_u16(exact, 2U, document.header.byte_swapped) & 7U);
        return flags == 5U || flags == 7U ? 2 : 1;
      }
    }
  };

  const auto& floor_record = document.floors[static_cast<std::size_t>(floor)];
  const auto lane_hit = [&](const OriginalTdtElevatorFloorRecord& record,
                            std::size_t elevator_index,
                            std::size_t sequence,
                            OriginalElevatorWaitingLane lane,
                            int begin,
                            int end) noexcept
      -> OriginalElevatorWaitingPersonHit {
    if (begin > x || end <= x) return {};
    const bool first_lane = lane == OriginalElevatorWaitingLane::first;
    const std::size_t count_offset = first_lane ? 0U : 2U;
    const std::size_t cursor_offset = first_lane ? 1U : 3U;
    const std::size_t table_offset = first_lane ? 4U : 164U;
    const auto count = signed_byte(record.exact_bytes[count_offset]);
    const auto ring_cursor =
        std::to_integer<std::uint8_t>(record.exact_bytes[cursor_offset]);
    if (count <= 0 || ring_cursor >= 40U) return {};

    std::size_t ordinal = 0U;
    int cursor = first_lane ? end - 1 : begin;
    const auto advance = [&]() noexcept {
      if (ordinal >= static_cast<std::size_t>(count)) return false;
      const auto slot = (ring_cursor + ordinal) % 40U;
      const auto person_index = load_u32(
          record.exact_bytes, table_offset + slot * 4U,
          document.header.byte_swapped);
      const int width = person_width(person_index);
      if (width == 0) return false;
      cursor += first_lane ? -width : width;
      ++ordinal;
      return true;
    };
    while (first_lane ? x < cursor : cursor < x) {
      if (!advance()) return {};
    }
    if (ordinal >= static_cast<std::size_t>(count)) return {};
    const auto slot = (ring_cursor + ordinal) % 40U;
    const auto person_index = load_u32(
        record.exact_bytes, table_offset + slot * 4U,
        document.header.byte_swapped);
    if (person_width(person_index) == 0) return {};
    return {true, elevator_index, person_index, floor, lane, ordinal};
  };

  for (std::size_t sequence = 0U; sequence < shaft_count; ++sequence) {
    const auto elevator_index = shafts[sequence].index;
    const auto& elevator = document.elevators[elevator_index];
    const auto mapped = original_elevator_floor_record_index(
        elevator.type, elevator.bottom_floor, elevator.top_floor, floor);
    if (mapped < 0) continue;
    const auto record = std::find_if(
        elevator.floor_records.begin(), elevator.floor_records.end(),
        [&](const OriginalTdtElevatorFloorRecord& candidate) {
          return candidate.mapped_index == mapped;
        });
    if (record == elevator.floor_records.end()) continue;

    int first_begin = floor_record.left_edge;
    if (sequence != 0U && shafts[sequence - 1U].x >= first_begin) {
      first_begin = shafts[sequence].x -
          ((shafts[sequence].x - shafts[sequence - 1U].x - 4) >> 1);
    }
    const int first_end = shafts[sequence].x - 2;
    if (auto hit = lane_hit(*record, elevator_index, sequence,
                            OriginalElevatorWaitingLane::first,
                            first_begin, first_end);
        hit.hit) {
      return hit;
    }

    const int shaft_width = elevator.type == 0U ? 6 : 4;
    const int second_begin = shafts[sequence].x + shaft_width + 2;
    int second_end = floor_record.right_edge;
    if (sequence + 1U != shaft_count &&
        shafts[sequence + 1U].x <= second_end) {
      second_end = shafts[sequence + 1U].x -
          ((shafts[sequence + 1U].x - shafts[sequence].x - 4) >> 1);
    }
    if (auto hit = lane_hit(*record, elevator_index, sequence,
                            OriginalElevatorWaitingLane::second,
                            second_begin, second_end);
        hit.hit) {
      return hit;
    }
  }
  return {};
}

OriginalPersonRouteResult route_original_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPersonRouteRequest& request) noexcept {
  OriginalPersonRouteResult result{};
  const auto person_x = original_person_route_x(document, person_index);
  if (!person_x) return result;

  auto source_floor = request.source_floor;
  auto destination_floor = request.destination_floor;
  if (source_floor < 0) source_floor = 10;
  if (destination_floor < 0) destination_floor = 10;
  if (!original_route_floor_valid(source_floor) ||
      !original_route_floor_valid(destination_floor)) {
    result.status = OriginalPersonRouteStatus::malformed_transport;
    return result;
  }
  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);

  if (source_floor == destination_floor) {
    if (request.tracked_route) {
      apply_original_person_metric_finalizer(
          person, document.header.byte_swapped);
    }
    result.status = OriginalPersonRouteStatus::already_on_floor;
    return result;
  }

  const auto selection = select_original_person_transport(
      document, source_floor, destination_floor, *person_x,
      request.tracked_route);
  result.transport_index = selection.transport_index;
  result.direction_up = selection.direction_up;
  if (selection.transport_index < 0) {
    if (request.tracked_route) {
      add_original_person_waiting_delay(
          person, request.no_route_delay, document.header.byte_swapped);
      apply_original_person_metric_finalizer(
          person, document.header.byte_swapped);
      result.failure_visualization_requested = request.visualize_failure;
    }
    result.status = OriginalPersonRouteStatus::no_route;
    return result;
  }

  if (selection.transport_index < 0x40) {
    const auto stair_index =
        static_cast<std::size_t>(selection.transport_index);
    if (stair_index >= document.post_elevator.stairs_bd70.size()) {
      result.status = OriginalPersonRouteStatus::malformed_transport;
      return result;
    }
    auto& stair = document.post_elevator.stairs_bd70[stair_index];
    // Exact 1210:114f Stair/Escalator occupancy update: direction selects
    // word 6 versus word 8 and marks the shared simulation state dirty.
    if (selection.direction_up) {
      stair.word_6 = static_cast<std::uint16_t>(stair.word_6 + 1U);
      store_u16(stair.exact_bytes, 6U, stair.word_6,
                document.header.byte_swapped);
    } else {
      stair.word_8 = static_cast<std::uint16_t>(stair.word_8 + 1U);
      store_u16(stair.exact_bytes, 8U, stair.word_8,
                document.header.byte_swapped);
    }
    const auto span = static_cast<std::int16_t>((stair.shape >> 1U) + 1U);
    if (request.tracked_route) {
      const auto per_floor = (stair.shape & 1U) != 0U
                                 ? request.stair_odd_delay
                                 : request.stair_even_delay;
      add_original_person_waiting_delay(
          person, static_cast<std::uint16_t>(per_floor * span),
          document.header.byte_swapped);
    }
    if (request.add_distance_penalty) {
      const auto distance = original_wrapped_absolute_difference(
          stair.x, *person_x);
      if (distance >= 80) {
        add_original_person_waiting_delay(
            person, distance < 125 ? 30U : 60U,
            document.header.byte_swapped);
      }
    }
    exact[7] = static_cast<std::byte>(static_cast<std::uint8_t>(
        source_floor + (selection.direction_up ? span : -span)));
    exact[8] = static_cast<std::byte>(selection.transport_index);
    result.status = OriginalPersonRouteStatus::stair;
    return result;
  }

  const auto elevator_index = static_cast<std::size_t>(
      selection.transport_index - 0x40);
  if (elevator_index >= document.elevators.size()) {
    result.status = OriginalPersonRouteStatus::malformed_transport;
    return result;
  }
  auto& elevator = document.elevators[elevator_index];
  const auto mapped = original_elevator_floor_record_index(
      elevator.type, elevator.bottom_floor, elevator.top_floor, source_floor);
  auto* floor_record = find_original_elevator_floor_record(elevator, mapped);
  if (mapped < 0 || floor_record == nullptr) {
    result.status = OriginalPersonRouteStatus::malformed_transport;
    return result;
  }
  const auto count_offset = selection.direction_up ? 0U : 2U;
  const auto count = signed_byte(floor_record->exact_bytes[count_offset]);
  exact[7] = static_cast<std::byte>(source_floor);
  if (count == 40) {
    if (request.tracked_route) {
      add_original_person_waiting_delay(
          person, request.queue_full_delay, document.header.byte_swapped);
    }
    exact[8] = std::byte{0xff};
    result.status = OriginalPersonRouteStatus::elevator_queue_full;
    return result;
  }
  if (!enqueue_original_elevator_waiting_person(
          document, person_index, elevator_index, source_floor,
          selection.direction_up, request.calendar_phase, request.day_phase,
          result.queue_assignment_created)) {
    result.status = OriginalPersonRouteStatus::malformed_transport;
    return result;
  }
  exact[8] = static_cast<std::byte>(
      elevator_index + (selection.direction_up ? 0x40U : 0x58U));
  if (request.add_distance_penalty && elevator.type != 0U) {
    const auto distance = original_wrapped_absolute_difference(
        elevator.x, *person_x);
    if (distance >= 80) {
      add_original_person_waiting_delay(
          person, distance < 125 ? 30U : 60U,
          document.header.byte_swapped);
    }
  }
  store_u16(exact, 10U, request.frame_time,
            document.header.byte_swapped);
  result.status = OriginalPersonRouteStatus::elevator;
  return result;
}

OriginalPersonRouteRequest original_person_route_context(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept {
  OriginalPersonRouteRequest result{};
  result.frame_time = document.header.frame_time;
  result.queue_full_delay = part.words_00_to_40[1U];
  result.no_route_delay = part.words_00_to_40[3U];
  result.stair_even_delay = part.words_00_to_40[31U];
  result.stair_odd_delay = part.words_00_to_40[32U];
  result.calendar_phase = original_calendar_phase(document.header.current_day);
  result.day_phase = static_cast<std::int8_t>(
      original_day_phase(document.header.frame_time));
  return result;
}

OriginalSecurityPersonStepResult step_original_security_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPartTable& part) noexcept {
  OriginalSecurityPersonStepResult result{};
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return result;

  auto& exact = document.people[person_index].exact_bytes;
  if (signed_byte(exact[4]) != 14) {
    result.status = OriginalSecurityPersonStepStatus::not_security;
    return result;
  }
  if (signed_byte(exact[5]) != 0) {
    result.status = OriginalSecurityPersonStepStatus::inactive;
    return result;
  }

  const auto event_flags = static_cast<std::uint8_t>(
      load_original_header_word(document, 60U));
  if ((event_flags & 1U) != 0U) {
    result = step_original_bomb_security_person(document, person_index, part);
    if (result.status ==
        OriginalSecurityPersonStepStatus::search_exhausted) {
      exact[5] = std::byte{1};
      result.changed = true;
    }
    return result;
  }
  if ((event_flags & 8U) != 0U) {
    return step_original_fire_security_person(document, person_index, part);
  }
  result.status = OriginalSecurityPersonStepStatus::no_event;
  return result;
}

OriginalSecurityPeopleStepResult step_original_security_people(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) {
  // Exact all-tenant realization of 1220:6764: resolve each Security-owned
  // person, and dispatch 1220:67cf only for state zero while event flags & 9.
  OriginalSecurityPeopleStepResult result{};
  if ((static_cast<std::uint8_t>(
           load_original_header_word(document, 60U)) &
       9U) == 0U) {
    return result;
  }

  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  for (const auto& floor : document.floors) {
    for (const auto& tenant : floor.tenants) {
      if (tenant.type != 14) continue;
      const auto people_start = original_tenant_people_start(
          tenant, document.header.byte_swapped);
      for (std::size_t ordinal = 0U; ordinal < 6U; ++ordinal) {
        const auto person_index =
            static_cast<std::uint64_t>(people_start) + ordinal;
        if (person_index >= person_limit) break;
        auto& person =
            document.people[static_cast<std::size_t>(person_index)];
        if (signed_byte(person.exact_bytes[5]) != 0) continue;
        ++result.responders;
        const auto step = step_original_security_person(
            document, static_cast<std::size_t>(person_index), part);
        if (step.changed) ++result.changed;
        if (step.bomb_found) ++result.bombs_found;
        if (step.fire_extinguished) ++result.fire_bands_extinguished;
        if (step.effect.valid()) result.effects.push_back(step.effect);
      }
    }
  }
  return result;
}

OriginalHousekeepingPersonStepResult step_original_housekeeping_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    const OriginalPersonRouteRequest& route_context) noexcept {
  OriginalHousekeepingPersonStepResult result{};
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return result;

  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);
  if (signed_byte(exact[4]) != 15) {
    result.status = OriginalHousekeepingPersonStepStatus::not_housekeeping;
    return result;
  }

  result.released_stair_counter =
      release_original_completed_stair(document, person);
  result.changed = result.released_stair_counter;
  const auto state = signed_byte(exact[5]);

  auto service_route = [&](std::int16_t source,
                           std::int16_t destination) noexcept {
    auto request = route_context;
    request.source_floor = source;
    request.destination_floor = destination;
    request.tracked_route = false;
    request.add_distance_penalty = false;
    request.visualize_failure = false;
    return route_original_person(document, person_index, request);
  };

  if (state == 0 || state == 3) {
    if (state == 0 && signed_byte(exact[7]) < 0) {
      exact[7] = static_cast<std::byte>(
          static_cast<std::uint8_t>(owner_floor));
      result.changed = true;
    }

    const auto room = find_original_housekeeping_room(document, person);
    if (!room) {
      exact[6] = std::byte{0xff};
      exact[5] = std::byte{1};
      result.status = OriginalHousekeepingPersonStepStatus::no_dirty_room;
      result.changed = true;
      return result;
    }
    exact[6] = static_cast<std::byte>(
        static_cast<std::uint8_t>(room->floor));
    result.selected_room_floor = room->floor;
    result.selected_room_key = room->key;
    result.route = service_route(signed_byte(exact[7]), room->floor);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        exact[5] = std::byte{0};
        result.status = OriginalHousekeepingPersonStepStatus::route_failed;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{3};
        result.status =
            OriginalHousekeepingPersonStepStatus::routed_to_room;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::already_on_floor: {
        if (static_cast<std::int8_t>(original_day_phase(
                document.header.frame_time)) < 0 ||
            document.header.frame_time >= 0x05dcU) {
          exact[5] = std::byte{0};
          result.status = OriginalHousekeepingPersonStepStatus::route_failed;
          result.changed = true;
          return result;
        }
        bool guest_changed = false;
        result.room_status_changed = clean_original_housekeeping_room(
            document, person, guest_changed);
        result.hotel_guest_state_changed = guest_changed;
        exact[5] = std::byte{2};
        store_u16(exact, 10U, 3U, document.header.byte_swapped);
        result.status = result.room_status_changed
                            ? OriginalHousekeepingPersonStepStatus::arrived_room
                            : OriginalHousekeepingPersonStepStatus::malformed_room;
        result.changed = true;
        return result;
      }
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalHousekeepingPersonStepStatus::malformed_route;
        return result;
    }
  }

  if (state == 1 || state == 4) {
    result.route = service_route(signed_byte(exact[7]), owner_floor);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        exact[5] = std::byte{0};
        result.status = OriginalHousekeepingPersonStepStatus::route_failed;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{4};
        result.status = OriginalHousekeepingPersonStepStatus::routed_home;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::already_on_floor:
        exact[5] = std::byte{0};
        result.status = OriginalHousekeepingPersonStepStatus::returned_home;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalHousekeepingPersonStepStatus::malformed_route;
        return result;
    }
  }

  if (state == 2) {
    const auto countdown =
        load_u16(exact, 10U, document.header.byte_swapped);
    if (countdown != 0U) {
      store_u16(exact, 10U,
                static_cast<std::uint16_t>(countdown - 1U),
                document.header.byte_swapped);
      result.status =
          OriginalHousekeepingPersonStepStatus::cleaning_countdown;
      result.changed = true;
      return result;
    }
    bool guest_changed = false;
    const bool valid_room = reopen_original_housekeeping_room(
        document, person, guest_changed);
    result.hotel_guest_state_changed = guest_changed;
    exact[5] = std::byte{0};
    result.status = valid_room
                        ? OriginalHousekeepingPersonStepStatus::room_reopened
                        : OriginalHousekeepingPersonStepStatus::malformed_room;
    result.changed = true;
    return result;
  }

  result.status = OriginalHousekeepingPersonStepStatus::unhandled_state;
  return result;
}

OriginalHousekeepingPeopleStepResult step_original_housekeeping_people(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept {
  OriginalHousekeepingPeopleStepResult result{};
  if ((load_original_header_word(document, 60U) & 9U) != 0U) return result;

  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  const auto route_context = original_person_route_context(document, part);
  for (std::size_t scanned_index = document.header.frame_time % 16U;
       scanned_index < person_limit; scanned_index += 16U) {
    ++result.scanned;
    const auto& scanned = document.people[scanned_index].exact_bytes;
    if (signed_byte(scanned[4]) != 15) continue;

    const auto owner_floor = signed_byte(scanned[0]);
    const auto owner_key = signed_byte(scanned[1]);
    const auto ordinal = load_u16(
        scanned, 2U, document.header.byte_swapped);
    const auto resolved = resolve_original_owned_person(
        document, owner_floor, owner_key, ordinal);
    if (!resolved) continue;

    const auto& target = document.people[*resolved].exact_bytes;
    const auto state = signed_byte(target[5]);
    bool dispatch = false;
    if (state >= 3) {
      dispatch = signed_byte(target[8]) < 0x40;
    } else if (state == 0) {
      dispatch =
          static_cast<std::int8_t>(original_day_phase(
              document.header.frame_time)) >= 0 &&
          document.header.frame_time < 0x05dcU;
    } else if (state == 1 || state == 2) {
      dispatch = true;
    }
    if (!dispatch) continue;

    ++result.dispatched;
    const auto step = step_original_housekeeping_person(
        document, *resolved, owner_floor, route_context);
    if (step.changed) ++result.changed;
    if (step.room_status_changed) ++result.rooms_cleaned;
    if (step.status == OriginalHousekeepingPersonStepStatus::room_reopened) {
      ++result.rooms_reopened;
    }
  }
  return result;
}

OriginalMetroPersonStepResult step_original_metro_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    const OriginalPartTable& part) noexcept {
  OriginalMetroPersonStepResult result{};
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return result;

  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);
  if (signed_byte(exact[4]) != 33) {
    result.status = OriginalMetroPersonStepStatus::not_metro;
    return result;
  }

  const auto state = signed_byte(exact[5]);
  const auto current_floor = signed_byte(exact[7]);
  result.released_stair_counter =
      state >= 0x40 && release_original_completed_stair(document, person);

  const bool inbound = state == 1 || state == 0x41;
  const bool outbound = state == 0x22 || state == 0x62;
  if (!inbound && !outbound) {
    result.status = OriginalMetroPersonStepStatus::unhandled_state;
    result.changed = result.released_stair_counter;
    return result;
  }

  auto route_context = original_person_route_context(document, part);
  route_context.tracked_route = true;
  if (inbound) {
    if (state == 1) {
      result.service_index = select_original_metro_service(document);
      exact[6] = static_cast<std::byte>(result.service_index);
      result.changed = true;
    } else {
      result.service_index = signed_byte(exact[6]);
    }
    if (result.service_index < 0) {
      exact[5] = std::byte{0x27};
      result.status = OriginalMetroPersonStepStatus::no_destination;
      result.changed = true;
      return result;
    }
    if (result.service_index >=
        static_cast<std::int16_t>(document.retail.size())) {
      result.status = OriginalMetroPersonStepStatus::malformed_service;
      return result;
    }

    const auto destination = signed_byte(
        document.retail[static_cast<std::size_t>(result.service_index)]
            .exact_bytes[0]);
    route_context.source_floor =
        state == 1 ? static_cast<std::int16_t>(owner_floor + 2)
                   : current_floor;
    route_context.destination_floor = destination;
    route_context.add_distance_penalty = state == 1;
    route_context.visualize_failure = false;
    result.route = route_original_person(document, person_index, route_context);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        exact[5] = std::byte{0x27};
        result.status = OriginalMetroPersonStepStatus::route_failed;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x41};
        result.status = OriginalMetroPersonStepStatus::routed_to_service;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::already_on_floor: {
        const auto entered = enter_original_metro_service(
            document, person, result.service_index, part,
            result.service_population_changed,
            result.service_tenant_marked_dirty);
        if (entered == 2) {
          exact[5] = std::byte{0x41};
          result.status = OriginalMetroPersonStepStatus::service_full;
        } else if (entered == -1 || entered == 3) {
          exact[5] = std::byte{0x22};
          result.status = OriginalMetroPersonStepStatus::arrived_service;
        } else {
          result.status = OriginalMetroPersonStepStatus::malformed_service;
        }
        result.changed = true;
        return result;
      }
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalMetroPersonStepStatus::malformed_route;
        return result;
    }
  }

  result.service_index = signed_byte(exact[6]);
  if (state == 0x22 &&
      !leave_original_metro_service(
          document, person, result.service_index, part,
          result.service_population_changed,
          result.service_tenant_marked_dirty)) {
    result.status = OriginalMetroPersonStepStatus::waiting_at_service;
    result.changed = result.released_stair_counter;
    return result;
  }

  route_context.source_floor = signed_byte(exact[7]);
  route_context.destination_floor =
      static_cast<std::int16_t>(owner_floor + 2);
  route_context.add_distance_penalty = state == 0x22;
  route_context.visualize_failure = true;
  result.route = route_original_person(document, person_index, route_context);
  switch (result.route.status) {
    case OriginalPersonRouteStatus::no_route:
      exact[5] = std::byte{0x27};
      result.status = OriginalMetroPersonStepStatus::route_failed;
      result.changed = true;
      return result;
    case OriginalPersonRouteStatus::elevator_queue_full:
    case OriginalPersonRouteStatus::stair:
    case OriginalPersonRouteStatus::elevator:
      exact[5] = std::byte{0x62};
      result.status = OriginalMetroPersonStepStatus::routed_home;
      result.changed = true;
      return result;
    case OriginalPersonRouteStatus::already_on_floor:
      exact[5] = std::byte{1};
      result.status = OriginalMetroPersonStepStatus::returned_home;
      result.changed = true;
      return result;
    case OriginalPersonRouteStatus::invalid_person:
    case OriginalPersonRouteStatus::malformed_transport:
      result.status = OriginalMetroPersonStepStatus::malformed_route;
      return result;
  }
  result.status = OriginalMetroPersonStepStatus::malformed_route;
  return result;
}

OriginalFoodServicePersonStepResult step_original_food_service_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    std::int16_t owner_key,
    const OriginalPartTable& part) noexcept {
  OriginalFoodServicePersonStepResult result{};
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return result;

  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);
  const auto person_type = signed_byte(exact[4]);
  if (person_type != 6 && person_type != 12) {
    result.status = OriginalFoodServicePersonStepStatus::not_food_service;
    return result;
  }

  const auto state = signed_byte(exact[5]);
  const auto current_floor = signed_byte(exact[7]);
  result.released_stair_counter =
      state >= 0x40 && release_original_completed_stair(document, person);
  const bool inbound = state == 0x20 || state == 0x60;
  const bool outbound = state == 0x05 || state == 0x45;
  if (!inbound && !outbound) {
    result.status = OriginalFoodServicePersonStepStatus::unhandled_state;
    result.changed = result.released_stair_counter;
    return result;
  }

  if (owner_floor < 0 || owner_floor >= 120 || owner_key < 0 ||
      owner_key >= static_cast<std::int16_t>(
                       OriginalTdtFloor::kIndexCapacity)) {
    result.status = OriginalFoodServicePersonStepStatus::malformed_owner;
    result.changed = result.released_stair_counter;
    return result;
  }
  auto* owner = find_original_tenant(
      document, static_cast<std::uint8_t>(owner_floor),
      static_cast<std::uint8_t>(owner_key));
  if (!owner) {
    result.status = OriginalFoodServicePersonStepStatus::malformed_owner;
    result.changed = result.released_stair_counter;
    return result;
  }
  result.service_index = std::bit_cast<std::int16_t>(
      load_u16(owner->exact_bytes, 6U, document.header.byte_swapped));
  if (result.service_index < 0 ||
      result.service_index >=
          static_cast<std::int16_t>(document.retail.size())) {
    result.status = OriginalFoodServicePersonStepStatus::malformed_service;
    result.changed = result.released_stair_counter;
    return result;
  }

  auto route_context = original_person_route_context(document, part);
  route_context.tracked_route = true;
  if (inbound) {
    auto& service = document.retail[
        static_cast<std::size_t>(result.service_index)].exact_bytes;
    if (state == 0x20) {
      if (service[2] == std::byte{3}) {
        exact[5] = std::byte{0x27};
        result.status = OriginalFoodServicePersonStepStatus::service_closed;
        result.changed = true;
        return result;
      }
      if (!reserve_original_food_service(
              document, person, result.service_index)) {
        result.status =
            OriginalFoodServicePersonStepStatus::reservation_blocked;
        result.changed = result.released_stair_counter;
        return result;
      }
      result.reservation_changed = true;
      result.changed = true;
    }

    route_context.source_floor = state == 0x20 ? 10 : current_floor;
    route_context.destination_floor = owner_floor;
    route_context.add_distance_penalty = state == 0x20;
    route_context.visualize_failure = state != 0x20;
    result.route = route_original_person(document, person_index, route_context);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        if (state == 0x20) {
          exact[5] = std::byte{0x20};
          exact[9] = std::byte{0};
          store_u16(exact, 12U, 0U, document.header.byte_swapped);
          store_u16(exact, 14U, 0U, document.header.byte_swapped);
          undo_original_food_service_reservation(
              document, result.service_index);
          result.reservation_changed = true;
          result.status = OriginalFoodServicePersonStepStatus::route_failed;
          result.changed = true;
          return result;
        }
        break;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x60};
        result.status =
            OriginalFoodServicePersonStepStatus::routed_to_service;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::already_on_floor: {
        const auto entered = enter_original_metro_service(
            document, person, result.service_index, part,
            result.service_population_changed,
            result.service_tenant_marked_dirty);
        if (entered == 2) {
          exact[5] = std::byte{0x60};
          result.status = OriginalFoodServicePersonStepStatus::service_full;
        } else if (entered == -1 || entered == 3) {
          exact[5] = std::byte{0x05};
          result.status =
              OriginalFoodServicePersonStepStatus::arrived_service;
        } else {
          result.status =
              OriginalFoodServicePersonStepStatus::malformed_service;
        }
        result.changed = true;
        return result;
      }
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalFoodServicePersonStepStatus::malformed_route;
        result.changed = result.released_stair_counter ||
                         result.reservation_changed;
        return result;
    }
    // State 60 maps a route failure to the same terminal cleanup as an
    // already-on-floor outbound leg.
  } else {
    if (state == 0x05 &&
        !leave_original_metro_service(
            document, person, result.service_index, part,
            result.service_population_changed,
            result.service_tenant_marked_dirty)) {
      result.status =
          OriginalFoodServicePersonStepStatus::waiting_at_service;
      result.changed = result.released_stair_counter;
      return result;
    }

    route_context.source_floor = signed_byte(exact[7]);
    route_context.destination_floor = 10;
    route_context.add_distance_penalty = state == 0x05;
    route_context.visualize_failure = true;
    result.route = route_original_person(document, person_index, route_context);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x45};
        result.status = OriginalFoodServicePersonStepStatus::routed_home;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalFoodServicePersonStepStatus::malformed_route;
        result.changed = result.released_stair_counter ||
                         result.service_population_changed;
        return result;
      case OriginalPersonRouteStatus::no_route:
      case OriginalPersonRouteStatus::already_on_floor:
        break;
    }
  }

  exact[5] = std::byte{0x27};
  result.service_history_changed = update_original_food_service_history(
      document, person, signed_byte(owner->exact_bytes[4]),
      result.service_index, part);
  result.status = result.route.status == OriginalPersonRouteStatus::no_route
                      ? OriginalFoodServicePersonStepStatus::route_failed
                      : OriginalFoodServicePersonStepStatus::returned_home;
  result.changed = true;
  return result;
}

OriginalRetailPersonStepResult step_original_retail_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    std::int16_t owner_key,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) noexcept {
  OriginalRetailPersonStepResult result{};
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return result;

  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);
  if (signed_byte(exact[4]) != 10) {
    result.status = OriginalRetailPersonStepStatus::not_retail;
    return result;
  }

  const auto state = signed_byte(exact[5]);
  const auto current_floor = signed_byte(exact[7]);
  result.released_stair_counter =
      state >= 0x40 && release_original_completed_stair(document, person);
  const bool inbound = state == 0x20 || state == 0x60;
  const bool outbound = state == 0x05 || state == 0x45;
  if (!inbound && !outbound) {
    result.status = OriginalRetailPersonStepStatus::unhandled_state;
    result.changed = result.released_stair_counter;
    return result;
  }

  if (owner_floor < 0 || owner_floor >= 120 || owner_key < 0 ||
      owner_key >= static_cast<std::int16_t>(
                       OriginalTdtFloor::kIndexCapacity)) {
    result.status = OriginalRetailPersonStepStatus::malformed_owner;
    result.changed = result.released_stair_counter;
    return result;
  }
  auto* owner = find_original_tenant(
      document, static_cast<std::uint8_t>(owner_floor),
      static_cast<std::uint8_t>(owner_key));
  if (!owner) {
    result.status = OriginalRetailPersonStepStatus::malformed_owner;
    result.changed = result.released_stair_counter;
    return result;
  }
  result.service_index = std::bit_cast<std::int16_t>(
      load_u16(owner->exact_bytes, 6U, document.header.byte_swapped));
  if (result.service_index < 0 ||
      result.service_index >=
          static_cast<std::int16_t>(document.retail.size())) {
    result.status = OriginalRetailPersonStepStatus::malformed_service;
    result.changed = result.released_stair_counter;
    return result;
  }

  auto& service = document.retail[
      static_cast<std::size_t>(result.service_index)].exact_bytes;
  auto route_context = original_person_route_context(document, part);
  route_context.tracked_route = true;
  if (inbound) {
    if (state == 0x20) {
      if (!reserve_original_food_service(
              document, person, result.service_index)) {
        result.status = OriginalRetailPersonStepStatus::reservation_blocked;
        result.changed = result.released_stair_counter;
        return result;
      }
      result.reservation_changed = true;
      result.changed = true;
    }

    const bool store_inactive = service[2] == std::byte{0xff};
    route_context.source_floor = state == 0x20 ? 10 : current_floor;
    route_context.destination_floor = owner_floor;
    route_context.add_distance_penalty = state == 0x20;
    route_context.visualize_failure = !store_inactive;
    result.route = route_original_person(document, person_index, route_context);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        if (store_inactive) {
          exact[5] = std::byte{0x20};
          exact[9] = std::byte{0};
          store_u16(exact, 12U, 0U, document.header.byte_swapped);
          store_u16(exact, 14U, 0U, document.header.byte_swapped);
          undo_original_food_service_reservation(
              document, result.service_index);
          result.reservation_changed = true;
          result.status = OriginalRetailPersonStepStatus::route_failed;
          result.changed = true;
          return result;
        }
        break;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        if (store_inactive) {
          activate_original_retail_store(
              document, *owner, result.service_index, rent_income,
              result.activation_visual_requested);
          result.store_activated = true;
        }
        exact[5] = std::byte{0x60};
        result.status = OriginalRetailPersonStepStatus::routed_to_store;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::already_on_floor: {
        if (store_inactive) {
          activate_original_retail_store(
              document, *owner, result.service_index, rent_income,
              result.activation_visual_requested);
          result.store_activated = true;
        }
        const auto entered = enter_original_metro_service(
            document, person, result.service_index, part,
            result.service_population_changed,
            result.service_tenant_marked_dirty);
        if (entered == 2) {
          exact[5] = std::byte{0x60};
          result.status = OriginalRetailPersonStepStatus::store_full;
        } else if (entered == -1 || entered == 3) {
          exact[5] = std::byte{0x05};
          result.status = OriginalRetailPersonStepStatus::arrived_store;
        } else {
          result.status = OriginalRetailPersonStepStatus::malformed_service;
        }
        result.changed = true;
        return result;
      }
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalRetailPersonStepStatus::malformed_route;
        result.changed = result.released_stair_counter ||
                         result.reservation_changed;
        return result;
    }
  } else {
    if (state == 0x05 &&
        !leave_original_metro_service(
            document, person, result.service_index, part,
            result.service_population_changed,
            result.service_tenant_marked_dirty)) {
      result.status = OriginalRetailPersonStepStatus::waiting_at_store;
      result.changed = result.released_stair_counter;
      return result;
    }

    route_context.source_floor = signed_byte(exact[7]);
    route_context.destination_floor = 10;
    route_context.add_distance_penalty = state == 0x05;
    route_context.visualize_failure = true;
    result.route = route_original_person(document, person_index, route_context);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x45};
        result.status = OriginalRetailPersonStepStatus::routed_home;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalRetailPersonStepStatus::malformed_route;
        result.changed = result.released_stair_counter ||
                         result.service_population_changed;
        return result;
      case OriginalPersonRouteStatus::no_route:
      case OriginalPersonRouteStatus::already_on_floor:
        break;
    }
  }

  exact[5] = std::byte{0x27};
  result.service_history_changed = update_original_food_service_history(
      document, person, 10, result.service_index, part);
  result.status = result.route.status == OriginalPersonRouteStatus::no_route
                      ? OriginalRetailPersonStepStatus::route_failed
                      : OriginalRetailPersonStepStatus::returned_home;
  result.changed = true;
  return result;
}

OriginalEntertainmentPersonStepResult step_original_entertainment_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    std::int16_t owner_key,
    const OriginalPartTable& part) noexcept {
  OriginalEntertainmentPersonStepResult result{};
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return result;

  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);
  const auto person_type = signed_byte(exact[4]);
  if (person_type != 18 && person_type != 29) {
    result.status = OriginalEntertainmentPersonStepStatus::not_entertainment;
    return result;
  }

  const auto state = signed_byte(exact[5]);
  const auto current_floor = signed_byte(exact[7]);
  result.released_stair_counter =
      state >= 0x40 && release_original_completed_stair(document, person);
  result.changed = result.released_stair_counter;

  const bool inbound_entertainment = state == 0x20 || state == 0x60;
  const bool outbound_home = state == 0x05 || state == 0x45;
  const bool inbound_service = state == 0x01 || state == 0x41;
  const bool outbound_service = state == 0x22 || state == 0x62;
  if (!inbound_entertainment && !outbound_home && !inbound_service &&
      !outbound_service) {
    result.status = OriginalEntertainmentPersonStepStatus::unhandled_state;
    return result;
  }

  if (owner_floor < 0 || owner_floor >= 120 || owner_key < 0 ||
      owner_key >= static_cast<std::int16_t>(
                       OriginalTdtFloor::kIndexCapacity)) {
    result.status = OriginalEntertainmentPersonStepStatus::malformed_owner;
    return result;
  }
  auto* owner = find_original_tenant(
      document, static_cast<std::uint8_t>(owner_floor),
      static_cast<std::uint8_t>(owner_key));
  if (!owner) {
    result.status = OriginalEntertainmentPersonStepStatus::malformed_owner;
    return result;
  }
  const auto entertainment_index =
      original_entertainment_record_index(document, *owner);
  if (!entertainment_index) {
    result.status = OriginalEntertainmentPersonStepStatus::malformed_owner;
    return result;
  }
  result.entertainment_index =
      static_cast<std::int16_t>(*entertainment_index);
  auto& entertainment =
      document.post_elevator.dc24_records[*entertainment_index];
  const auto owner_type = signed_byte(owner->exact_bytes[4]);

  auto request = original_person_route_context(document, part);
  request.tracked_route = true;

  if (inbound_entertainment) {
    if (state == 0x20) {
      if (!reserve_original_entertainment_capacity(
              entertainment, owner_type)) {
        result.status =
            OriginalEntertainmentPersonStepStatus::capacity_unavailable;
        return result;
      }
      result.entertainment_capacity_changed = true;
      result.changed = true;
    }

    request.source_floor = state == 0x20 ? 10 : current_floor;
    // Exact 1180:0d96 destination side: byte 0 for a paired record and byte 1
    // for the negative single-facility sentinel.
    request.destination_floor = signed_byte(
        entertainment[signed_byte(entertainment[7]) >= 0 ? 0U : 1U]);
    request.add_distance_penalty = state == 0x20;
    request.visualize_failure = state != 0x20;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        if (state == 0x20) {
          exact[9] = std::byte{0};
          store_u16(exact, 12U, 0U, document.header.byte_swapped);
          store_u16(exact, 14U, 0U, document.header.byte_swapped);
          restore_original_entertainment_capacity(
              entertainment, owner_type);
        } else {
          exact[5] = std::byte{0x27};
        }
        result.status = OriginalEntertainmentPersonStepStatus::route_failed;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x60};
        result.status =
            OriginalEntertainmentPersonStepStatus::routed_to_entertainment;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::already_on_floor:
        enter_original_entertainment_record(document, entertainment);
        exact[5] = std::byte{3};
        result.entertainment_record_changed = true;
        result.status =
            OriginalEntertainmentPersonStepStatus::arrived_entertainment;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status =
            OriginalEntertainmentPersonStepStatus::malformed_route;
        return result;
    }
  }

  if (outbound_home) {
    request.source_floor =
        state == 0x05 ? signed_byte(entertainment[1]) : current_floor;
    request.destination_floor = 10;
    request.add_distance_penalty = state == 0x05;
    request.visualize_failure = true;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        exact[5] = std::byte{0x27};
        result.status = OriginalEntertainmentPersonStepStatus::route_failed;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x45};
        result.status = OriginalEntertainmentPersonStepStatus::routed_home;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::already_on_floor:
        exact[5] = std::byte{0x27};
        result.status = OriginalEntertainmentPersonStepStatus::returned_home;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status =
            OriginalEntertainmentPersonStepStatus::malformed_route;
        return result;
    }
  }

  if (inbound_service) {
    if (state == 0x01) {
      const auto family = static_cast<std::size_t>(
          next_original_people_random(document) % 3U);
      // Exact 11a8:1498 band: max(0, (owner_floor - 9) / 15), passed to
      // 11a8:12dc's family-specific commercial-service selector.
      auto group = static_cast<std::int16_t>((owner_floor - 9) / 15);
      if (group < 0) group = 0;
      result.service_index = select_original_commercial_service(
          document, family, static_cast<std::size_t>(group));
      exact[6] = static_cast<std::byte>(result.service_index);
      result.changed = true;
    } else {
      result.service_index = signed_byte(exact[6]);
    }

    std::int16_t destination = 10;
    if (result.service_index >= 0) {
      if (result.service_index >=
          static_cast<std::int16_t>(document.retail.size())) {
        result.status =
            OriginalEntertainmentPersonStepStatus::malformed_service;
        return result;
      }
      destination = signed_byte(
          document.retail[static_cast<std::size_t>(result.service_index)]
              .exact_bytes[0]);
    }
    request.source_floor =
        state == 0x01 ? signed_byte(entertainment[1]) : current_floor;
    request.destination_floor = destination;
    request.add_distance_penalty = state == 0x01;
    request.visualize_failure = state != 0x01;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        if (state == 0x01) {
          exact[5] = std::byte{0x41};
          exact[6] = std::byte{0xff};
          exact[7] = entertainment[1];
          exact[8] = std::byte{0xff};
        } else {
          exact[5] = std::byte{0x27};
        }
        result.status = OriginalEntertainmentPersonStepStatus::route_failed;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x41};
        result.status =
            OriginalEntertainmentPersonStepStatus::routed_to_service;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::already_on_floor: {
        const auto entered = enter_original_metro_service(
            document, person, result.service_index, part,
            result.service_population_changed,
            result.service_tenant_marked_dirty);
        if (entered == 2) {
          exact[5] = std::byte{0x41};
          result.status =
              OriginalEntertainmentPersonStepStatus::service_full;
        } else if (entered == -1 || entered == 3) {
          exact[5] = std::byte{0x22};
          result.status = entered == -1
                              ? OriginalEntertainmentPersonStepStatus::
                                    service_unavailable
                              : OriginalEntertainmentPersonStepStatus::
                                    arrived_service;
        } else {
          result.status =
              OriginalEntertainmentPersonStepStatus::malformed_service;
        }
        result.changed = true;
        return result;
      }
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status =
            OriginalEntertainmentPersonStepStatus::malformed_route;
        return result;
    }
  }

  result.service_index = signed_byte(exact[6]);
  if (state == 0x22 &&
      !leave_original_metro_service(
          document, person, result.service_index, part,
          result.service_population_changed,
          result.service_tenant_marked_dirty)) {
    result.status =
        OriginalEntertainmentPersonStepStatus::waiting_at_service;
    result.changed = result.changed || result.service_population_changed ||
                     result.service_tenant_marked_dirty;
    return result;
  }

  request.source_floor = signed_byte(exact[7]);
  request.destination_floor = signed_byte(entertainment[1]);
  request.add_distance_penalty = state == 0x22;
  request.visualize_failure = true;
  result.route = route_original_person(document, person_index, request);
  switch (result.route.status) {
    case OriginalPersonRouteStatus::no_route:
      exact[5] = std::byte{0x27};
      result.status = OriginalEntertainmentPersonStepStatus::route_failed;
      result.changed = true;
      return result;
    case OriginalPersonRouteStatus::elevator_queue_full:
    case OriginalPersonRouteStatus::stair:
    case OriginalPersonRouteStatus::elevator:
      exact[5] = std::byte{0x62};
      result.status =
          OriginalEntertainmentPersonStepStatus::routed_from_service;
      result.changed = true;
      return result;
    case OriginalPersonRouteStatus::already_on_floor:
      exact[5] = std::byte{0x27};
      result.status =
          OriginalEntertainmentPersonStepStatus::returned_from_service;
      result.changed = true;
      return result;
    case OriginalPersonRouteStatus::invalid_person:
    case OriginalPersonRouteStatus::malformed_transport:
      result.status =
          OriginalEntertainmentPersonStepStatus::malformed_route;
      return result;
  }
  result.status = OriginalEntertainmentPersonStepStatus::malformed_route;
  return result;
}

OriginalCondoPersonStepResult step_original_condo_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    std::int16_t owner_key,
    std::uint16_t owner_ordinal,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) noexcept {
  OriginalCondoPersonStepResult result{};
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return result;

  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);
  if (signed_byte(exact[4]) != 9) {
    result.status = OriginalCondoPersonStepStatus::not_condo;
    return result;
  }

  const auto state = signed_byte(exact[5]);
  const auto current_floor = signed_byte(exact[7]);
  result.released_stair_counter =
      state >= 0x40 && release_original_completed_stair(document, person);
  result.changed = result.released_stair_counter;
  constexpr std::array<std::int8_t, 12> kStates{
      0x00, 0x01, 0x04, 0x10, 0x20, 0x21,
      0x22, 0x40, 0x41, 0x60, 0x61, 0x62};
  if (std::find(kStates.begin(), kStates.end(), state) == kStates.end()) {
    result.status = OriginalCondoPersonStepStatus::unhandled_state;
    return result;
  }

  if (owner_floor < 0 || owner_floor >= 120 || owner_key < 0 ||
      owner_key >= static_cast<std::int16_t>(
                       OriginalTdtFloor::kIndexCapacity)) {
    result.status = OriginalCondoPersonStepStatus::malformed_owner;
    return result;
  }
  auto* owner = find_original_tenant(
      document, static_cast<std::uint8_t>(owner_floor),
      static_cast<std::uint8_t>(owner_key));
  if (!owner || signed_byte(owner->exact_bytes[4]) != 9) {
    result.status = OriginalCondoPersonStepStatus::malformed_owner;
    return result;
  }

  const auto finish_resident =
      [&](OriginalCondoPersonStepStatus status) noexcept {
        finish_original_condo_resident(document, *owner);
        exact[5] = std::byte{4};
        result.owner_status_changed = true;
        result.changed = true;
        result.status = status;
      };

  auto request = original_person_route_context(document, part);
  request.tracked_route = true;

  const auto step_commercial_inbound = [&]() noexcept -> std::int16_t {
    if (state == 1) {
      std::size_t family{};
      if (original_calendar_phase(document.header.current_day) != 0U) {
        family = owner_key % 4 == 0 ? 1U : 2U;
      }
      auto group = static_cast<std::int16_t>((owner_floor - 9) / 15);
      if (group < 0) group = 0;
      result.service_index = select_original_commercial_service(
          document, family, static_cast<std::size_t>(group));
      exact[6] = static_cast<std::byte>(result.service_index);
      result.changed = true;
    } else {
      result.service_index = signed_byte(exact[6]);
    }

    std::int16_t destination = 10;
    if (result.service_index >= 0) {
      if (result.service_index >=
          static_cast<std::int16_t>(document.retail.size())) {
        result.status = OriginalCondoPersonStepStatus::malformed_service;
        return 0x40;
      }
      destination = signed_byte(
          document.retail[static_cast<std::size_t>(result.service_index)]
              .exact_bytes[0]);
    }
    request.source_floor = state == 1 ? owner_floor : current_floor;
    request.destination_floor = destination;
    request.add_distance_penalty = state == 1;
    request.visualize_failure = state != 1;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        result.status = OriginalCondoPersonStepStatus::route_failed;
        result.changed = true;
        if (state != 1) return -1;
        exact[5] = std::byte{0x41};
        exact[6] = std::byte{0xff};
        exact[7] = static_cast<std::byte>(owner_floor);
        exact[8] = std::byte{0xff};
        return 0x40;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x41};
        result.status = OriginalCondoPersonStepStatus::routed_to_service;
        result.changed = true;
        return 0x40;
      case OriginalPersonRouteStatus::already_on_floor: {
        const auto entered = enter_original_metro_service(
            document, person, result.service_index, part,
            result.service_population_changed,
            result.service_tenant_marked_dirty);
        if (entered == 2) {
          exact[5] = std::byte{0x41};
          result.status = OriginalCondoPersonStepStatus::service_full;
        } else if (entered == -1 || entered == 3) {
          exact[5] = std::byte{0x22};
          result.status = entered == -1
                              ? OriginalCondoPersonStepStatus::
                                    service_unavailable
                              : OriginalCondoPersonStepStatus::arrived_service;
        } else {
          result.status = OriginalCondoPersonStepStatus::malformed_service;
        }
        result.changed = true;
        return 0x40;
      }
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalCondoPersonStepStatus::malformed_route;
        return 0x40;
    }
    return 0x40;
  };

  const auto step_commercial_outbound = [&]() noexcept -> std::int16_t {
    // Exact shared 1230:0244 commercial-service outbound route helper.
    result.service_index = signed_byte(exact[6]);
    if (state == 0x22 &&
        !leave_original_metro_service(
            document, person, result.service_index, part,
            result.service_population_changed,
            result.service_tenant_marked_dirty)) {
      result.status = OriginalCondoPersonStepStatus::waiting_at_service;
      result.changed = result.changed || result.service_population_changed ||
                       result.service_tenant_marked_dirty;
      return 0x40;
    }
    request.source_floor = signed_byte(exact[7]);
    request.destination_floor = owner_floor;
    request.add_distance_penalty = state == 0x22;
    request.visualize_failure = true;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        result.status = OriginalCondoPersonStepStatus::route_failed;
        result.changed = true;
        return -1;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x62};
        result.status = OriginalCondoPersonStepStatus::routed_from_service;
        result.changed = true;
        return 0x40;
      case OriginalPersonRouteStatus::already_on_floor:
        result.status = OriginalCondoPersonStepStatus::arrived_home;
        result.changed = true;
        return 3;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalCondoPersonStepStatus::malformed_route;
        return 0x40;
    }
    return 0x40;
  };

  if (state == 0 || state == 0x40) {
    if (state == 0) {
      decrement_original_condo_status(*owner);
      result.owner_status_changed = true;
      result.changed = true;
    }
    request.source_floor = state == 0 ? owner_floor : current_floor;
    request.destination_floor = 10;
    request.add_distance_penalty = state == 0;
    request.visualize_failure = true;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        finish_resident(OriginalCondoPersonStepStatus::route_failed);
        return result;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x40};
        result.status = OriginalCondoPersonStepStatus::routed_to_lobby;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::already_on_floor:
        exact[5] = std::byte{0x21};
        result.status = OriginalCondoPersonStepStatus::arrived_lobby;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalCondoPersonStepStatus::malformed_route;
        return result;
    }
  }

  if (state == 1 || state == 0x41) {
    if (state == 1) {
      decrement_original_condo_status(*owner);
      result.owner_status_changed = true;
      result.changed = true;
    }
    if (step_commercial_inbound() == -1) {
      finish_resident(OriginalCondoPersonStepStatus::route_failed);
    }
    return result;
  }

  if (state == 4) {
    exact[5] = std::byte{0x10};
    if (synchronize_original_condo_residents(
            document, *owner, owner_ordinal)) {
      result.owner_status_changed = true;
    }
    result.status = OriginalCondoPersonStepStatus::resident_synchronized;
    result.changed = true;
    return result;
  }

  if (state == 0x10) {
    if (owner->status == 0x10U) {
      set_original_tenant_status(*owner, 3U);
      mark_original_tenant_changed(*owner);
      result.owner_status_changed = true;
    }
    if (original_calendar_phase(document.header.current_day) == 1U) {
      exact[5] = owner_key % 2 == 0 ? std::byte{1} : std::byte{4};
    } else {
      exact[5] = owner_ordinal == 1U ? std::byte{1} : std::byte{0};
    }
    result.status = OriginalCondoPersonStepStatus::resident_synchronized;
    result.changed = true;
    return result;
  }

  if (state == 0x20 || state == 0x60) {
    const bool inactive = signed_byte(owner->exact_bytes[5]) >= 0x18;
    request.source_floor = state == 0x20 ? 10 : current_floor;
    request.destination_floor = owner_floor;
    request.add_distance_penalty = state == 0x20;
    request.visualize_failure = !inactive;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        if (!inactive) {
          finish_resident(OriginalCondoPersonStepStatus::route_failed);
        } else {
          exact[5] = std::byte{0x20};
          exact[9] = std::byte{0};
          store_u16(exact, 12U, 0U, document.header.byte_swapped);
          store_u16(exact, 14U, 0U, document.header.byte_swapped);
          result.status = OriginalCondoPersonStepStatus::route_failed;
          result.changed = true;
        }
        return result;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        if (inactive) {
          activate_original_condo(document, *owner, rent_income);
          result.condo_activated = true;
          result.activation_visual_requested = true;
          result.owner_status_changed = true;
        }
        exact[5] = std::byte{0x60};
        result.status = OriginalCondoPersonStepStatus::routed_home;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::already_on_floor:
        if (inactive) {
          activate_original_condo(document, *owner, rent_income);
          result.condo_activated = true;
          result.activation_visual_requested = true;
          result.owner_status_changed = true;
        }
        finish_resident(OriginalCondoPersonStepStatus::arrived_home);
        return result;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalCondoPersonStepStatus::malformed_route;
        return result;
    }
  }

  if (state == 0x21 || state == 0x61) {
    request.source_floor = state == 0x21 ? 10 : current_floor;
    request.destination_floor = owner_floor;
    request.add_distance_penalty = state == 0x21;
    request.visualize_failure = true;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        finish_resident(OriginalCondoPersonStepStatus::route_failed);
        return result;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x61};
        result.status = OriginalCondoPersonStepStatus::routed_home;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::already_on_floor:
        finish_resident(OriginalCondoPersonStepStatus::arrived_home);
        return result;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalCondoPersonStepStatus::malformed_route;
        return result;
    }
  }

  const auto commercial = step_commercial_outbound();
  if (commercial == -1 || commercial == 3) {
    finish_resident(commercial == -1
                        ? OriginalCondoPersonStepStatus::route_failed
                        : OriginalCondoPersonStepStatus::arrived_home);
  }
  return result;
}

OriginalHotelPersonStepResult step_original_hotel_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    std::int16_t owner_key,
    std::uint16_t owner_ordinal,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) {
  OriginalHotelPersonStepResult result{};
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return result;

  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);
  const auto person_type = signed_byte(exact[4]);
  if (!is_original_hotel_type(person_type)) {
    result.status = OriginalHotelPersonStepStatus::not_hotel;
    return result;
  }

  const auto state = signed_byte(exact[5]);
  const auto current_floor = signed_byte(exact[7]);
  result.released_stair_counter =
      state >= 0x40 && release_original_completed_stair(document, person);
  result.changed = result.released_stair_counter;
  constexpr std::array<std::int8_t, 10> kStates{
      0x01, 0x04, 0x05, 0x10, 0x20,
      0x22, 0x41, 0x45, 0x60, 0x62};
  if (std::find(kStates.begin(), kStates.end(), state) == kStates.end()) {
    result.status = OriginalHotelPersonStepStatus::unhandled_state;
    return result;
  }

  if (owner_floor < 0 || owner_floor >= 120 || owner_key < 0 ||
      owner_key >=
          static_cast<std::int16_t>(OriginalTdtFloor::kIndexCapacity)) {
    result.status = OriginalHotelPersonStepStatus::malformed_owner;
    return result;
  }
  auto* owner = find_original_tenant(
      document, static_cast<std::uint8_t>(owner_floor),
      static_cast<std::uint8_t>(owner_key));
  if (!owner || !is_original_hotel_type(signed_byte(owner->exact_bytes[4]))) {
    result.status = OriginalHotelPersonStepStatus::malformed_owner;
    return result;
  }

  const auto finish_guest =
      [&](OriginalHotelPersonStepStatus status) noexcept {
        finish_original_hotel_guest(document, *owner);
        exact[5] = std::byte{4};
        result.owner_status_changed = true;
        result.changed = true;
        result.status = status;
      };
  const auto remove_parking = [&]() noexcept {
    if (!original_person_has_parking(person)) return;
    remove_original_hotel_person_parking(document, person_index);
    result.parking_changed = true;
    result.changed = true;
  };

  auto request = original_person_route_context(document, part);
  request.tracked_route = true;

  const auto step_commercial_inbound = [&]() -> std::int16_t {
    if (state == 1) {
      auto group = static_cast<std::int16_t>((owner_floor - 9) / 15);
      if (group < 0) group = 0;
      // 1230:0000 receives literal selector one from 3154.
      result.service_index = select_original_commercial_service(
          document, 1U, static_cast<std::size_t>(group));
      exact[6] = static_cast<std::byte>(result.service_index);
      result.changed = true;
    } else {
      result.service_index = signed_byte(exact[6]);
    }

    std::int16_t destination = 10;
    if (result.service_index >= 0) {
      if (result.service_index >=
          static_cast<std::int16_t>(document.retail.size())) {
        result.status = OriginalHotelPersonStepStatus::malformed_service;
        return 0x40;
      }
      destination = signed_byte(
          document.retail[static_cast<std::size_t>(result.service_index)]
              .exact_bytes[0]);
    }
    request.source_floor = state == 1 ? owner_floor : current_floor;
    request.destination_floor = destination;
    request.add_distance_penalty = state == 1;
    request.visualize_failure = state != 1;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        result.status = OriginalHotelPersonStepStatus::route_failed;
        if (state != 1) return -1;
        exact[5] = std::byte{0x41};
        exact[6] = std::byte{0xff};
        exact[7] = static_cast<std::byte>(owner_floor);
        exact[8] = std::byte{0xff};
        result.changed = true;
        return 0x40;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x41};
        result.status = OriginalHotelPersonStepStatus::routed_to_service;
        result.changed = true;
        return 0x40;
      case OriginalPersonRouteStatus::already_on_floor: {
        const auto entered = enter_original_metro_service(
            document, person, result.service_index, part,
            result.service_population_changed,
            result.service_tenant_marked_dirty);
        if (entered == 2) {
          exact[5] = std::byte{0x41};
          result.status = OriginalHotelPersonStepStatus::service_full;
        } else if (entered == -1 || entered == 3) {
          exact[5] = std::byte{0x22};
          result.status = entered == -1
                              ? OriginalHotelPersonStepStatus::
                                    service_unavailable
                              : OriginalHotelPersonStepStatus::arrived_service;
        } else {
          result.status = OriginalHotelPersonStepStatus::malformed_service;
        }
        result.changed = true;
        return 0x40;
      }
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalHotelPersonStepStatus::malformed_route;
        return 0x40;
    }
    return 0x40;
  };

  const auto step_commercial_outbound = [&]() -> std::int16_t {
    // Exact shared 1230:0244 commercial-service outbound route helper.
    result.service_index = signed_byte(exact[6]);
    if (state == 0x22 &&
        !leave_original_metro_service(
            document, person, result.service_index, part,
            result.service_population_changed,
            result.service_tenant_marked_dirty)) {
      result.status = OriginalHotelPersonStepStatus::waiting_at_service;
      result.changed = result.changed || result.service_population_changed ||
                       result.service_tenant_marked_dirty;
      return 0x40;
    }
    request.source_floor = signed_byte(exact[7]);
    request.destination_floor = owner_floor;
    request.add_distance_penalty = state == 0x22;
    request.visualize_failure = true;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        result.status = OriginalHotelPersonStepStatus::route_failed;
        return -1;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x62};
        result.status = OriginalHotelPersonStepStatus::routed_from_service;
        result.changed = true;
        return 0x40;
      case OriginalPersonRouteStatus::already_on_floor:
        result.status = OriginalHotelPersonStepStatus::arrived_hotel;
        result.changed = true;
        return 3;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalHotelPersonStepStatus::malformed_route;
        return 0x40;
    }
    return 0x40;
  };

  if (state == 0x10) {
    if (owner->status == 0x10U) {
      set_original_tenant_status(*owner, owner->type == 3 ? 1U : 2U);
      mark_original_tenant_changed(*owner);
      result.owner_status_changed = true;
    }
    exact[5] = std::byte{5};
    result.status = OriginalHotelPersonStepStatus::departure_prepared;
    result.changed = true;
    return result;
  }

  if (state == 4) {
    exact[5] = std::byte{0x10};
    if (synchronize_original_hotel_guests(
            document, *owner, owner_ordinal)) {
      result.owner_status_changed = true;
    }
    result.status = OriginalHotelPersonStepStatus::guest_synchronized;
    result.changed = true;
    return result;
  }

  if (state == 1 || state == 0x41) {
    if (state == 1) {
      decrement_original_hotel_status(*owner);
      result.owner_status_changed = true;
      result.changed = true;
    }
    if (step_commercial_inbound() == -1) {
      finish_guest(OriginalHotelPersonStepStatus::route_failed);
    }
    return result;
  }

  if (state == 0x22 || state == 0x62) {
    const auto commercial = step_commercial_outbound();
    if (commercial == -1 || commercial == 3) {
      finish_guest(commercial == -1
                       ? OriginalHotelPersonStepStatus::route_failed
                       : OriginalHotelPersonStepStatus::arrived_hotel);
    }
    return result;
  }

  if (state == 5 || state == 0x45) {
    request.source_floor =
        state == 5 ? owner_floor : current_floor;
    request.destination_floor = original_person_parking_floor(document, person);
    request.add_distance_penalty = state == 5;
    request.visualize_failure = true;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        exact[5] = std::byte{0x20};
        remove_parking();
        clear_original_hotel_periodic_visitor(
            document, person_index, result);
        result.status = OriginalHotelPersonStepStatus::route_failed;
        result.changed = true;
        break;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x45};
        result.status = OriginalHotelPersonStepStatus::routed_from_hotel;
        result.changed = true;
        break;
      case OriginalPersonRouteStatus::already_on_floor:
        exact[5] = std::byte{0x20};
        finish_original_hotel_periodic_visit(
            document, person_index, part, result);
        start_original_hotel_periodic_visit(
            document, person_index, *owner, owner_floor, result);
        remove_parking();
        result.status = OriginalHotelPersonStepStatus::departed_hotel;
        result.changed = true;
        break;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalHotelPersonStepStatus::malformed_route;
        break;
    }

    // Only the initial state-five call performs occupancy decrement and the
    // last-guest checkout test, after the route branch completes.
    if (state == 5) {
      decrement_original_hotel_status(*owner);
      result.owner_status_changed = true;
      result.changed = true;
      if ((owner->status & 7U) == 0U) {
        checkout_original_hotel_room(document, *owner, rent_income);
        result.room_checked_out = true;
        result.checkout_visual_requested = true;
        append_original_income_status_request(result.host_requests, 2U);
        result.owner_status_changed = true;
      }
    }
    return result;
  }

  // States 20/60 are the parking-or-lobby return path.
  if (state == 0x20) {
    const auto allocation = allocate_original_hotel_parking(
        document, person_index, *owner, owner_floor, owner_key);
    result.parking_changed = allocation.changed;
    result.changed = result.changed || allocation.changed;
    result.notification_code = allocation.notification_code;
    append_original_notification_status_request(
        result.host_requests, result.notification_code);
    if (owner->type == 5 && !original_person_has_parking(person)) {
      exact[5] = std::byte{0x26};
      clear_original_hotel_periodic_visitor(
          document, person_index, result);
      result.status = OriginalHotelPersonStepStatus::parking_unavailable;
      result.changed = true;
      return result;
    }
    notify_original_hotel_periodic_arrival(
        document, person_index, result);
  }

  const bool inactive = signed_byte(owner->exact_bytes[5]) >= 0x18;
  request.source_floor = state == 0x20
                             ? original_person_parking_floor(document, person)
                             : current_floor;
  request.destination_floor = owner_floor;
  request.add_distance_penalty = state == 0x20;
  request.visualize_failure = !inactive;
  result.route = route_original_person(document, person_index, request);
  switch (result.route.status) {
    case OriginalPersonRouteStatus::no_route:
      if (!inactive) {
        finish_guest(OriginalHotelPersonStepStatus::route_failed);
      } else {
        exact[5] = std::byte{0x20};
        remove_parking();
        clear_original_hotel_periodic_visitor(
            document, person_index, result);
        exact[9] = std::byte{0};
        store_u16(exact, 12U, 0U, document.header.byte_swapped);
        store_u16(exact, 14U, 0U, document.header.byte_swapped);
        result.status = OriginalHotelPersonStepStatus::route_failed;
        result.changed = true;
      }
      return result;
    case OriginalPersonRouteStatus::elevator_queue_full:
    case OriginalPersonRouteStatus::stair:
    case OriginalPersonRouteStatus::elevator:
      if (inactive) {
        activate_original_hotel_room(document, *owner);
        result.room_activated = true;
        result.owner_status_changed = true;
      }
      exact[5] = std::byte{0x60};
      result.status = OriginalHotelPersonStepStatus::routed_to_hotel;
      result.changed = true;
      return result;
    case OriginalPersonRouteStatus::already_on_floor:
      if (inactive) {
        activate_original_hotel_room(document, *owner);
        result.room_activated = true;
        result.owner_status_changed = true;
      }
      finish_original_hotel_guest(document, *owner);
      result.owner_status_changed = true;
      // Exact 1220:382c-3861: the Hotel tenant key (bp+0c), not this
      // person's room ordinal (bp+0e), determines the post-arrival schedule.
      exact[5] = owner_key % 2 == 0 ? std::byte{1} : std::byte{4};
      result.status = OriginalHotelPersonStepStatus::arrived_hotel;
      result.changed = true;
      return result;
    case OriginalPersonRouteStatus::invalid_person:
    case OriginalPersonRouteStatus::malformed_transport:
      result.status = OriginalHotelPersonStepStatus::malformed_route;
      return result;
  }
  result.status = OriginalHotelPersonStepStatus::malformed_route;
  return result;
}

OriginalOfficeNormalPersonStepResult step_original_office_normal_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    std::int16_t owner_key,
    std::uint16_t owner_ordinal,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) {
  OriginalOfficeNormalPersonStepResult result{};
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return result;

  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);
  if (signed_byte(exact[4]) != 7) {
    result.status = OriginalOfficeNormalPersonStepStatus::not_office;
    return result;
  }

  const auto state = signed_byte(exact[5]);
  const auto current_floor = signed_byte(exact[7]);
  result.released_stair_counter =
      state >= 0x40 && release_original_completed_stair(document, person);
  result.changed = result.released_stair_counter;
  constexpr std::array<std::int8_t, 16> kStates{
      0x00, 0x01, 0x02, 0x05, 0x20, 0x21, 0x22, 0x23,
      0x40, 0x41, 0x42, 0x45, 0x60, 0x61, 0x62, 0x63};
  if (std::find(kStates.begin(), kStates.end(), state) == kStates.end()) {
    result.status = OriginalOfficeNormalPersonStepStatus::unhandled_state;
    return result;
  }

  if (owner_floor < 0 || owner_floor >= 120 || owner_key < 0 ||
      owner_key >= static_cast<std::int16_t>(
                       OriginalTdtFloor::kIndexCapacity)) {
    result.status = OriginalOfficeNormalPersonStepStatus::malformed_owner;
    return result;
  }
  auto* owner = find_original_tenant(
      document, static_cast<std::uint8_t>(owner_floor),
      static_cast<std::uint8_t>(owner_key));
  if (!owner || signed_byte(owner->exact_bytes[4]) != 7) {
    result.status = OriginalOfficeNormalPersonStepStatus::malformed_owner;
    return result;
  }

  const auto remove_parking = [&]() noexcept {
    if (!original_person_has_parking(person)) return;
    remove_original_hotel_person_parking(document, person_index);
    result.parking_changed = true;
    result.changed = true;
  };
  const auto medical_unavailable = [&]() noexcept {
    document.post_elevator.b92d = 0U;
    result.notification_code = 6U;
    result.changed = true;
  };
  const auto arrive_office = [&]() noexcept {
    enter_original_office(*owner);
    result.owner_status_changed = true;
    result.changed = true;
  };
  const auto depart_office = [&]() noexcept {
    leave_original_office(document, *owner);
    result.owner_status_changed = true;
    result.changed = true;
  };
  const auto activate_office = [&]() noexcept {
    activate_original_office(document, *owner, rent_income);
    result.office_activated = true;
    result.activation_visual_requested = true;
    result.owner_status_changed = true;
    result.changed = true;
  };

  auto request = original_person_route_context(document, part);
  request.tracked_route = true;

  const auto step_commercial_inbound = [&]() -> std::int16_t {
    if (state == 1) {
      auto group = static_cast<std::int16_t>((owner_floor - 9) / 15);
      if (group < 0) group = 0;
      result.service_index = select_original_commercial_service(
          document, 2U, static_cast<std::size_t>(group));
      exact[6] = static_cast<std::byte>(result.service_index);
      result.changed = true;
    } else {
      result.service_index = signed_byte(exact[6]);
    }

    std::int16_t destination = 10;
    if (result.service_index >= 0) {
      if (result.service_index >=
          static_cast<std::int16_t>(document.retail.size())) {
        result.status =
            OriginalOfficeNormalPersonStepStatus::malformed_service;
        return 0x40;
      }
      destination = signed_byte(
          document.retail[static_cast<std::size_t>(result.service_index)]
              .exact_bytes[0]);
    }
    request.source_floor = state == 1 ? owner_floor : current_floor;
    request.destination_floor = destination;
    request.add_distance_penalty = state == 1;
    request.visualize_failure = state != 1;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        result.status = OriginalOfficeNormalPersonStepStatus::route_failed;
        if (state != 1) return -1;
        exact[5] = std::byte{0x41};
        exact[6] = std::byte{0xff};
        exact[7] = static_cast<std::byte>(owner_floor);
        exact[8] = std::byte{0xff};
        result.changed = true;
        return 0x40;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x41};
        result.status =
            OriginalOfficeNormalPersonStepStatus::routed_to_service;
        result.changed = true;
        return 0x40;
      case OriginalPersonRouteStatus::already_on_floor: {
        const auto entered = enter_original_metro_service(
            document, person, result.service_index, part,
            result.service_population_changed,
            result.service_tenant_marked_dirty);
        if (entered == 2) {
          exact[5] = std::byte{0x41};
          result.status = OriginalOfficeNormalPersonStepStatus::service_full;
        } else if (entered == -1 || entered == 3) {
          exact[5] = std::byte{0x22};
          result.status =
              entered == -1
                  ? OriginalOfficeNormalPersonStepStatus::service_unavailable
                  : OriginalOfficeNormalPersonStepStatus::arrived_service;
        } else {
          result.status =
              OriginalOfficeNormalPersonStepStatus::malformed_service;
        }
        result.changed = true;
        return 0x40;
      }
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalOfficeNormalPersonStepStatus::malformed_route;
        return 0x40;
    }
    return 0x40;
  };

  const auto step_commercial_outbound = [&]() -> std::int16_t {
    // Exact shared 1230:0244 commercial-service outbound route helper.
    result.service_index = signed_byte(exact[6]);
    if (state == 0x22 &&
        !leave_original_metro_service(
            document, person, result.service_index, part,
            result.service_population_changed,
            result.service_tenant_marked_dirty)) {
      result.status =
          OriginalOfficeNormalPersonStepStatus::waiting_at_service;
      result.changed = result.changed || result.service_population_changed ||
                       result.service_tenant_marked_dirty;
      return 0x40;
    }
    request.source_floor = signed_byte(exact[7]);
    request.destination_floor = owner_floor;
    request.add_distance_penalty = state == 0x22;
    request.visualize_failure = true;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        result.status = OriginalOfficeNormalPersonStepStatus::route_failed;
        return -1;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x62};
        result.status =
            OriginalOfficeNormalPersonStepStatus::routed_to_office;
        result.changed = true;
        return 0x40;
      case OriginalPersonRouteStatus::already_on_floor:
        arrive_office();
        exact[5] = owner_ordinal == 1U ? std::byte{0} : std::byte{5};
        result.status = OriginalOfficeNormalPersonStepStatus::arrived_office;
        result.changed = true;
        return 3;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalOfficeNormalPersonStepStatus::malformed_route;
        return 0x40;
    }
    return 0x40;
  };

  if (state == 0x20 || state == 0x60) {
    if (state == 0x20) {
      const auto allocation = allocate_original_hotel_parking(
          document, person_index, *owner, owner_floor, owner_key);
      result.parking_changed = allocation.changed;
      result.changed = result.changed || allocation.changed;
      result.notification_code = allocation.notification_code;
    }
    const bool inactive = signed_byte(owner->exact_bytes[5]) >= 0x10;
    request.source_floor = state == 0x20
                               ? original_person_parking_floor(document, person)
                               : current_floor;
    request.destination_floor = owner_floor;
    request.add_distance_penalty = state == 0x20;
    request.visualize_failure = !inactive;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        if (inactive) {
          remove_parking();
          exact[5] = std::byte{0x20};
          exact[9] = std::byte{0};
          store_u16(exact, 12U, 0U, document.header.byte_swapped);
          store_u16(exact, 14U, 0U, document.header.byte_swapped);
        } else {
          exact[5] = std::byte{0x25};
          remove_parking();
        }
        result.status = OriginalOfficeNormalPersonStepStatus::route_failed;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        if (inactive) activate_office();
        exact[5] = std::byte{0x60};
        result.status =
            OriginalOfficeNormalPersonStepStatus::routed_to_office;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::already_on_floor:
        if (inactive) activate_office();
        arrive_office();
        if (owner_ordinal == 0U) {
          exact[5] = std::byte{0};
        } else {
          // Exact 1170:0635 gate: rating three or above and abs(rand)%10 == 0.
          const bool medical_trip =
              document.header.rating >= 3U &&
              next_original_people_random(document) % 10U == 0U;
          exact[5] = medical_trip ? std::byte{2} : std::byte{1};
        }
        result.status = OriginalOfficeNormalPersonStepStatus::arrived_office;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalOfficeNormalPersonStepStatus::malformed_route;
        return result;
    }
  }

  if (state == 0x21 || state == 0x61) {
    request.source_floor = state == 0x21 ? 10 : current_floor;
    request.destination_floor = owner_floor;
    request.add_distance_penalty = state == 0x21;
    request.visualize_failure = true;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        exact[5] = std::byte{0x26};
        remove_parking();
        result.status = OriginalOfficeNormalPersonStepStatus::route_failed;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x61};
        result.status =
            OriginalOfficeNormalPersonStepStatus::routed_to_office;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::already_on_floor:
        arrive_office();
        exact[5] = std::byte{5};
        result.status = OriginalOfficeNormalPersonStepStatus::arrived_office;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalOfficeNormalPersonStepStatus::malformed_route;
        return result;
    }
  }

  if (state == 0x22 || state == 0x62) {
    if (step_commercial_outbound() == -1) {
      exact[5] = std::byte{0x26};
      remove_parking();
      result.changed = true;
    }
    return result;
  }

  if (state == 0x23 || state == 0x63) {
    result.medical_service_index = signed_byte(exact[6]);
    if (state == 0x23) {
      const auto left = leave_original_office_medical_service(
          document, person, result.medical_service_index,
          result.medical_population_changed,
          result.medical_tenant_marked_dirty);
      if (left == OriginalOfficeMedicalLeaveStatus::waiting) {
        result.status =
            OriginalOfficeNormalPersonStepStatus::waiting_at_medical;
        return result;
      }
      if (left == OriginalOfficeMedicalLeaveStatus::malformed) {
        result.status = OriginalOfficeNormalPersonStepStatus::
            malformed_medical_service;
        return result;
      }
      result.changed = true;
    }
    request.source_floor = signed_byte(exact[7]);
    request.destination_floor = owner_floor;
    request.add_distance_penalty = state == 0x23;
    request.visualize_failure = true;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        exact[5] = std::byte{0x26};
        remove_parking();
        result.status = OriginalOfficeNormalPersonStepStatus::route_failed;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x63};
        result.status =
            OriginalOfficeNormalPersonStepStatus::routed_to_office;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::already_on_floor:
        arrive_office();
        exact[5] = owner_ordinal == 1U ? std::byte{0} : std::byte{5};
        result.status = OriginalOfficeNormalPersonStepStatus::arrived_office;
        result.changed = true;
        return result;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalOfficeNormalPersonStepStatus::malformed_route;
        return result;
    }
  }

  if (state == 0 || state == 0x40) {
    request.source_floor = state == 0 ? owner_floor : current_floor;
    request.destination_floor = 10;
    request.add_distance_penalty = state == 0;
    request.visualize_failure = true;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        exact[5] = std::byte{0x26};
        remove_parking();
        result.status = OriginalOfficeNormalPersonStepStatus::route_failed;
        result.changed = true;
        break;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x40};
        result.status =
            OriginalOfficeNormalPersonStepStatus::routed_to_lobby;
        result.changed = true;
        break;
      case OriginalPersonRouteStatus::already_on_floor:
        exact[5] = std::byte{0x21};
        result.status = OriginalOfficeNormalPersonStepStatus::arrived_lobby;
        result.changed = true;
        break;
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalOfficeNormalPersonStepStatus::malformed_route;
        break;
    }
    if (state == 0) depart_office();
    return result;
  }

  if (state == 1 || state == 0x41) {
    if (step_commercial_inbound() == -1) {
      exact[5] = std::byte{0x26};
      remove_parking();
      result.changed = true;
    }
    if (state == 1) depart_office();
    return result;
  }

  if (state == 2 || state == 0x42) {
    if (state == 2) {
      result.medical_service_index =
          select_original_medical_service(document, owner_floor);
      exact[6] = static_cast<std::byte>(result.medical_service_index);
      result.changed = true;
      if (result.medical_service_index < 0) medical_unavailable();
    } else {
      result.medical_service_index = signed_byte(exact[6]);
    }

    std::int16_t destination = 10;
    if (result.medical_service_index >= 0) {
      if (result.medical_service_index >= static_cast<std::int16_t>(
              document.post_elevator.dbfc_dwords.size())) {
        result.status = OriginalOfficeNormalPersonStepStatus::
            malformed_medical_service;
        if (state == 2) depart_office();
        return result;
      }
      destination = original_medical_floor(
          document.post_elevator.dbfc_dwords[
              static_cast<std::size_t>(result.medical_service_index)]);
    }
    request.source_floor = state == 2 ? owner_floor : current_floor;
    request.destination_floor = destination;
    request.add_distance_penalty = state == 2;
    request.visualize_failure = state != 2;
    result.route = route_original_person(document, person_index, request);
    switch (result.route.status) {
      case OriginalPersonRouteStatus::no_route:
        if (state == 2) {
          exact[5] = std::byte{0x41};
          exact[6] = std::byte{0xff};
          exact[7] = static_cast<std::byte>(owner_floor);
          exact[8] = std::byte{0xff};
        } else {
          exact[5] = std::byte{0x26};
          remove_parking();
        }
        result.status = OriginalOfficeNormalPersonStepStatus::route_failed;
        result.changed = true;
        break;
      case OriginalPersonRouteStatus::elevator_queue_full:
      case OriginalPersonRouteStatus::stair:
      case OriginalPersonRouteStatus::elevator:
        exact[5] = std::byte{0x42};
        result.status =
            OriginalOfficeNormalPersonStepStatus::routed_to_medical;
        result.changed = true;
        break;
      case OriginalPersonRouteStatus::already_on_floor: {
        const auto entered = enter_original_office_medical_service(
            document, person, result.medical_service_index,
            part,
            result.medical_population_changed,
            result.medical_tenant_marked_dirty);
        if (entered == OriginalOfficeMedicalEnterStatus::malformed) {
          result.status = OriginalOfficeNormalPersonStepStatus::
              malformed_medical_service;
        } else if (entered == OriginalOfficeMedicalEnterStatus::full) {
          exact[5] = std::byte{0x42};
          result.status = OriginalOfficeNormalPersonStepStatus::medical_full;
          result.changed = true;
        } else {
          if (entered == OriginalOfficeMedicalEnterStatus::unavailable) {
            medical_unavailable();
          }
          exact[5] = std::byte{0x23};
          result.status =
              OriginalOfficeNormalPersonStepStatus::arrived_medical;
          result.changed = true;
        }
        break;
      }
      case OriginalPersonRouteStatus::invalid_person:
      case OriginalPersonRouteStatus::malformed_transport:
        result.status = OriginalOfficeNormalPersonStepStatus::malformed_route;
        break;
    }
    if (state == 2) depart_office();
    return result;
  }

  // States 05/45 route to the assigned parking floor (or lobby floor ten),
  // then 2068 holds state 27 until frame 08fc before the next commute.
  request.source_floor = state == 5 ? owner_floor : current_floor;
  request.destination_floor = original_person_parking_floor(document, person);
  request.add_distance_penalty = state == 5;
  request.visualize_failure = true;
  result.route = route_original_person(document, person_index, request);
  switch (result.route.status) {
    case OriginalPersonRouteStatus::no_route:
      exact[5] = std::byte{0x26};
      remove_parking();
      result.status = OriginalOfficeNormalPersonStepStatus::route_failed;
      result.changed = true;
      break;
    case OriginalPersonRouteStatus::elevator_queue_full:
    case OriginalPersonRouteStatus::stair:
    case OriginalPersonRouteStatus::elevator:
      exact[5] = std::byte{0x45};
      result.status =
          OriginalOfficeNormalPersonStepStatus::routed_from_office;
      result.changed = true;
      break;
    case OriginalPersonRouteStatus::already_on_floor:
      exact[5] = std::byte{0x27};
      remove_parking();
      result.status = OriginalOfficeNormalPersonStepStatus::departed_office;
      result.changed = true;
      break;
    case OriginalPersonRouteStatus::invalid_person:
    case OriginalPersonRouteStatus::malformed_transport:
      result.status = OriginalOfficeNormalPersonStepStatus::malformed_route;
      break;
  }
  if (state == 5) depart_office();
  return result;
}

OriginalPersonFamilyDispatchResult dispatch_original_person_family(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income,
    OriginalPersonFamilyDispatchSource source) {
  OriginalPersonFamilyDispatchResult result{};
  result.source = source;
  result.person_index = person_index;
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return result;

  const auto& exact = document.people[person_index].exact_bytes;
  result.person_type = static_cast<std::int8_t>(signed_byte(exact[4]));
  const auto owner_floor = signed_byte(exact[0]);
  const auto owner_key = signed_byte(exact[1]);
  const auto owner_ordinal =
      load_u16(exact, 2U, document.header.byte_swapped);

  switch (result.person_type) {
    case 3:
    case 4:
    case 5: {
      const auto step = step_original_hotel_person(
          document, person_index, owner_floor, owner_key, owner_ordinal, part,
          rent_income);
      result.status = OriginalPersonFamilyDispatchStatus::hotel;
      result.changed = step.changed;
      result.checkout_visual_requested = step.checkout_visual_requested;
      result.notification_code = step.notification_code;
      result.hotel_process_requests = step.process_requests;
      result.host_requests = step.host_requests;
      return result;
    }
    case 6:
    case 12: {
      const auto step = step_original_food_service_person(
          document, person_index, owner_floor, owner_key, part);
      result.status = OriginalPersonFamilyDispatchStatus::food_service;
      result.changed = step.changed;
      return result;
    }
    case 7: {
      const auto step = step_original_office_normal_person(
          document, person_index, owner_floor, owner_key, owner_ordinal, part,
          rent_income);
      result.status = OriginalPersonFamilyDispatchStatus::office;
      result.changed = step.changed;
      result.activation_visual_requested =
          step.activation_visual_requested;
      result.notification_code = step.notification_code;
      if (step.activation_visual_requested) {
        append_original_income_status_request(result.host_requests, 1U);
      }
      append_original_notification_status_request(
          result.host_requests, step.notification_code);
      return result;
    }
    case 9: {
      const auto step = step_original_condo_person(
          document, person_index, owner_floor, owner_key, owner_ordinal, part,
          rent_income);
      result.status = OriginalPersonFamilyDispatchStatus::condo;
      result.changed = step.changed;
      result.activation_visual_requested =
          step.activation_visual_requested;
      if (step.activation_visual_requested) {
        append_original_income_status_request(result.host_requests, 3U);
      }
      return result;
    }
    case 10: {
      const auto step = step_original_retail_person(
          document, person_index, owner_floor, owner_key, part, rent_income);
      result.status = OriginalPersonFamilyDispatchStatus::retail;
      result.changed = step.changed;
      result.activation_visual_requested =
          step.activation_visual_requested;
      if (step.activation_visual_requested) {
        append_original_income_status_request(result.host_requests, 6U);
      }
      return result;
    }
    case 15: {
      const auto step = step_original_housekeeping_person(
          document, person_index, owner_floor,
          original_person_route_context(document, part));
      result.status = OriginalPersonFamilyDispatchStatus::housekeeping;
      result.changed = step.changed;
      return result;
    }
    case 18:
    case 29: {
      const auto step = step_original_entertainment_person(
          document, person_index, owner_floor, owner_key, part);
      result.status = OriginalPersonFamilyDispatchStatus::entertainment;
      result.changed = step.changed;
      return result;
    }
    case 33: {
      const auto step = step_original_metro_person(
          document, person_index, owner_floor, part);
      result.status = OriginalPersonFamilyDispatchStatus::metro;
      result.changed = step.changed;
      return result;
    }
    case 36: {
      const auto step = dispatch_original_cathedral_person(
          document, person_index, part);
      result.status = OriginalPersonFamilyDispatchStatus::cathedral;
      result.changed = step.step.changed;
      result.cathedral_arrival = step.arrival;
      return result;
    }
    case 14:
      // The CS:1aa9 table used by 16ab deliberately has no type-14 branch.
      // 1210:0883 has a separate live call to 1220:67cf for car arrivals.
      if (source ==
          OriginalPersonFamilyDispatchSource::elevator_car_0883) {
        const auto step =
            step_original_security_person(document, person_index, part);
        result.status = OriginalPersonFamilyDispatchStatus::security;
        result.changed = step.changed;
        result.security_effect = step.effect;
      } else {
        result.status = OriginalPersonFamilyDispatchStatus::no_handler;
      }
      return result;
    default:
      result.status = OriginalPersonFamilyDispatchStatus::no_handler;
      return result;
  }
}

OriginalTranslatedPeopleStepResult step_original_translated_people(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) {
  OriginalTranslatedPeopleStepResult result{};
  if ((load_original_header_word(document, 60U) & 9U) != 0U) return result;

  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  const auto route_context = original_person_route_context(document, part);
  const auto run_elevator_timeout = [&](std::size_t person_index) {
    ++result.elevator_timeout_checks;
    const auto timeout = step_original_elevator_wait_timeout(
        document, person_index, part);
    if (timeout.status == OriginalElevatorWaitTimeoutStatus::malformed_queue) {
      ++result.elevator_timeout_malformed_queues;
      return;
    }
    if (timeout.status != OriginalElevatorWaitTimeoutStatus::dispatched) {
      return;
    }
    ++result.elevator_timeouts_triggered;
    result.elevator_transit_people_dispatched +=
        timeout.dispatch.dispatches.size();
    result.dispatched += timeout.dispatch.dispatches.size();
    if (timeout.dispatch.view_slot_restore_requested) {
      ++result.elevator_timeout_view_requests;
    }
    for (const auto& dispatch : timeout.dispatch.dispatches) {
      if (dispatch.changed) ++result.changed;
      result.hotel_process_requests.insert(
          result.hotel_process_requests.end(),
          dispatch.process_requests.begin(), dispatch.process_requests.end());
      result.host_requests.insert(
          result.host_requests.end(), dispatch.host_requests.begin(),
          dispatch.host_requests.end());
    }
  };
  for (std::size_t scanned_index = document.header.frame_time % 16U;
       scanned_index < person_limit; scanned_index += 16U) {
    ++result.scanned;
    const auto& scanned = document.people[scanned_index].exact_bytes;
    const auto type = signed_byte(scanned[4]);
    if (type != 3 && type != 4 && type != 5 && type != 6 && type != 7 &&
        type != 9 && type != 10 && type != 12 && type != 15 && type != 18 &&
        type != 29 && type != 33 && type != 36) {
      continue;
    }

    const auto owner_floor = signed_byte(scanned[0]);
    const auto owner_key = signed_byte(scanned[1]);
    const auto ordinal =
        load_u16(scanned, 2U, document.header.byte_swapped);
    const auto resolved = resolve_original_owned_person(
        document, owner_floor, owner_key, ordinal);
    if (!resolved) continue;

    auto& target = document.people[*resolved].exact_bytes;
    const auto resolved_type = signed_byte(target[4]);
    const auto state = signed_byte(target[5]);
    if (is_original_hotel_type(resolved_type)) {
      // 0daf deliberately excludes Hotel ordinal zero from this normal pass;
      // that record is still dispatched by the transit/sweep call sites.
      if (ordinal < 1U) continue;
      const auto* owner = find_original_tenant(
          document, static_cast<std::uint8_t>(owner_floor),
          static_cast<std::uint8_t>(owner_key));
      if (!owner || !is_original_hotel_type(
                        signed_byte(owner->exact_bytes[4]))) {
        continue;
      }

      bool dispatch = false;
      const auto day_phase = original_day_phase(document.header.frame_time);
      if (state >= 0x40) {
        if (signed_byte(target[8]) >= 0x40) {
          run_elevator_timeout(*resolved);
        } else {
          dispatch = true;
        }
      } else if (state == 1) {
        if (day_phase == 4) {
          dispatch = next_original_people_random(document) % 6U == 0U;
        } else if (day_phase > 4) {
          target[5] = std::byte{4};
          ++result.changed;
        }
      } else if (state == 4 && day_phase >= 5) {
        dispatch = document.header.frame_time > 0x0960U ||
                   next_original_people_random(document) % 12U == 0U;
      } else if (state == 5) {
        if (day_phase == 0) {
          dispatch = next_original_people_random(document) % 12U == 0U;
        } else if (day_phase != 6) {
          dispatch = true;
        }
      } else if (state == 0x10) {
        // Exact 1220:2fa9 branch selected by 2e92's CS:3138 table: phases
        // zero through four dispatch immediately. The twelve-way cadence
        // resumes only late in phase six, strictly after frame 0x0a06.
        if (day_phase < 5) {
          dispatch = true;
        } else if (document.header.frame_time > 0x0a06U) {
          dispatch = next_original_people_random(document) % 12U == 0U;
        }
      } else if (state == 0x20 && owner->exact_bytes[14] != std::byte{0}) {
        if (day_phase == 4) {
          dispatch = next_original_people_random(document) % 12U == 0U;
        } else if (day_phase > 4 &&
                   document.header.frame_time < 0x08fcU) {
          dispatch = true;
        }
      } else if (state == 0x22) {
        dispatch = day_phase >= 4;
      } else if (state == 0x26 &&
                 document.header.frame_time > 0x08fcU) {
        target[5] = ordinal == 0U ? std::byte{0x24}
                                  : std::byte{0x20};
        ++result.changed;
      }

      if (dispatch) {
        ++result.dispatched;
        ++result.hotel_dispatched;
        auto step = step_original_hotel_person(
            document, *resolved, owner_floor, owner_key, ordinal, part,
            rent_income);
        if (step.changed) ++result.changed;
        if (step.checkout_visual_requested) {
          ++result.hotel_checkout_visual_requests;
        }
        if (step.notification_code == 5U) {
          ++result.hotel_parking_notifications;
        }
        if (step.notification_code != 0U) {
          result.notification_codes.push_back(step.notification_code);
        }
        result.hotel_process_requests.insert(
            result.hotel_process_requests.end(),
            step.process_requests.begin(), step.process_requests.end());
        result.host_requests.insert(
            result.host_requests.end(), step.host_requests.begin(),
            step.host_requests.end());
      }
      continue;
    }

    if (resolved_type == 7) {
      const auto* owner = find_original_tenant(
          document, static_cast<std::uint8_t>(owner_floor),
          static_cast<std::uint8_t>(owner_key));
      if (!owner || signed_byte(owner->exact_bytes[4]) != 7) continue;

      bool dispatch = false;
      const auto day_phase = original_day_phase(document.header.frame_time);
      const auto calendar_phase =
          original_calendar_phase(document.header.current_day);
      if (state >= 0x40) {
        if (signed_byte(target[8]) >= 0x40) {
          run_elevator_timeout(*resolved);
        } else {
          dispatch = true;
        }
      } else if (state == 0) {
        if (day_phase >= 4) {
          target[5] = std::byte{5};
          ++result.changed;
        } else if (ordinal == 0U) {
          if (day_phase == 0) {
            dispatch = next_original_people_random(document) % 12U == 0U;
          } else {
            dispatch = true;
          }
        } else if (day_phase == 3) {
          dispatch = next_original_people_random(document) % 12U == 0U;
        }
      } else if (state == 1 || state == 2) {
        if (day_phase >= 4) {
          target[5] = std::byte{5};
          ++result.changed;
        } else if (day_phase == 1) {
          dispatch = next_original_people_random(document) % 12U == 0U;
        } else if (day_phase > 1) {
          dispatch = true;
        }
      } else if (state == 5) {
        if (day_phase == 4) {
          dispatch = next_original_people_random(document) % 6U == 0U;
        } else if (day_phase > 4) {
          dispatch = true;
        }
      } else if (state == 0x20 && calendar_phase == 0U &&
                 owner->exact_bytes[14] != std::byte{0}) {
        if (day_phase == 0) {
          dispatch = next_original_people_random(document) % 12U == 0U;
        } else if (day_phase < 3) {
          dispatch = true;
        }
      } else if (state == 0x21) {
        if (day_phase >= 4) {
          target[5] = std::byte{0x27};
          remove_original_hotel_person_parking(document, *resolved);
          ++result.changed;
        } else if (day_phase == 3) {
          dispatch = next_original_people_random(document) % 12U == 0U;
        }
      } else if (state == 0x22 || state == 0x23) {
        if (day_phase >= 4) {
          target[5] = std::byte{0x27};
          remove_original_hotel_person_parking(document, *resolved);
          ++result.changed;
        } else if (day_phase >= 2) {
          dispatch = true;
        }
      } else if ((state == 0x25 || state == 0x26 || state == 0x27) &&
                 document.header.frame_time > 0x08fcU) {
        target[5] = std::byte{0x20};
        ++result.changed;
      }

      if (dispatch) {
        ++result.dispatched;
        ++result.office_normal_dispatched;
        const auto step = step_original_office_normal_person(
            document, *resolved, owner_floor, owner_key, ordinal, part,
            rent_income);
        if (step.changed) ++result.changed;
        if (step.activation_visual_requested) {
          ++result.office_activation_visual_requests;
          append_original_income_status_request(result.host_requests, 1U);
        }
        if (step.notification_code == 5U) {
          ++result.office_parking_notifications;
        } else if (step.notification_code == 6U) {
          ++result.office_medical_notifications;
        }
        if (step.notification_code != 0U) {
          result.notification_codes.push_back(step.notification_code);
          append_original_notification_status_request(
              result.host_requests, step.notification_code);
        }
      }
      continue;
    }

    if (resolved_type == 9) {
      const auto* owner = find_original_tenant(
          document, static_cast<std::uint8_t>(owner_floor),
          static_cast<std::uint8_t>(owner_key));
      if (!owner || signed_byte(owner->exact_bytes[4]) != 9) continue;

      bool dispatch = false;
      const auto day_phase = original_day_phase(document.header.frame_time);
      const auto calendar_phase =
          original_calendar_phase(document.header.current_day);
      if (state >= 0x40) {
        if (signed_byte(target[8]) >= 0x40) {
          run_elevator_timeout(*resolved);
        } else {
          dispatch = true;
        }
      } else if (state == 0) {
        if (day_phase == 0) {
          dispatch = next_original_people_random(document) % 12U == 0U;
        } else if (day_phase != 6) {
          dispatch = true;
        }
      } else if (state == 1) {
        if (calendar_phase == 1U && owner_key % 4 == 0) {
          if (day_phase == 4) {
            dispatch = next_original_people_random(document) % 6U == 0U;
          } else if (day_phase > 4) {
            target[5] = std::byte{4};
            ++result.changed;
          }
        } else if (day_phase == 0) {
          if (document.header.frame_time > 0x00f0U) {
            dispatch = next_original_people_random(document) % 12U == 0U;
          }
        } else if (day_phase != 6) {
          dispatch = true;
        }
      } else if (state == 4) {
        // Exact 1220:3b97 branch selected by 38e1's CS:3bed state table.
        // Resident ordinal two leaves immediately in phase five; the other
        // two residents use the twelve-way random gate until phase six.
        if (day_phase >= 5) {
          if (ordinal == 2U || document.header.frame_time > 0x0960U) {
            dispatch = true;
          } else {
            dispatch = next_original_people_random(document) % 12U == 0U;
          }
        }
      } else if (state == 0x10) {
        // 1220:39ba dispatches throughout phases zero through four. Late in
        // phase six it resumes on the twelve-way random cadence.
        if (day_phase < 5) {
          dispatch = true;
        } else if (document.header.frame_time > 0x0a06U) {
          dispatch = next_original_people_random(document) % 12U == 0U;
        }
      } else if (state == 0x20) {
        // 1220:3ace tests the owning tenant's runtime byte 14, not a person
        // state byte, and admits every phase below five when it is nonzero.
        dispatch = owner->exact_bytes[14] != std::byte{0} && day_phase < 5;
      } else if (state == 0x21) {
        // 1220:3b0c uses the resident ordinal to choose phase three for the
        // third resident and phase four for the other two.
        const auto departure_phase = ordinal == 2U ? 3 : 4;
        if (day_phase == departure_phase) {
          dispatch = next_original_people_random(document) % 12U == 0U;
        } else if (day_phase > departure_phase) {
          dispatch = true;
        }
      } else if (state == 0x22) {
        dispatch = day_phase >= 3;
      }

      if (dispatch) {
        ++result.dispatched;
        ++result.condo_dispatched;
        const auto step = step_original_condo_person(
            document, *resolved, owner_floor, owner_key, ordinal, part,
            rent_income);
        if (step.changed) ++result.changed;
        if (step.activation_visual_requested) {
          ++result.condo_activation_visual_requests;
          append_original_income_status_request(result.host_requests, 3U);
        }
      }
      continue;
    }

    if (resolved_type == 18 || resolved_type == 29) {
      bool dispatch = false;
      if (state >= 0x40) {
        if (signed_byte(target[8]) >= 0x40) {
          run_elevator_timeout(*resolved);
        } else {
          dispatch = true;
        }
      } else if (state == 0x01 || state == 0x05 || state == 0x22) {
        dispatch = true;
      } else if (state == 0x20) {
        const auto day_phase = static_cast<std::int8_t>(
            original_day_phase(document.header.frame_time));
        if (day_phase >= 0 && day_phase < 4 &&
            document.header.frame_time > 0x00f0U) {
          dispatch = next_original_people_random(document) % 6U == 0U;
        }
        if (day_phase >= 4) {
          target[5] = std::byte{0x27};
          ++result.changed;
        }
      }
      if (dispatch) {
        ++result.dispatched;
        ++result.entertainment_dispatched;
        if (step_original_entertainment_person(
                document, *resolved, owner_floor, owner_key, part).changed) {
          ++result.changed;
        }
      }
      continue;
    }

    if (resolved_type == 36) {
      const auto dispatch_cathedral = [&]() {
        ++result.dispatched;
        ++result.cathedral_dispatched;
        const auto dispatch = dispatch_original_cathedral_person(
            document, *resolved, part);
        if (dispatch.step.changed) ++result.changed;
        if (!dispatch.arrival) return;
        ++result.cathedral_arrival_checks;
        if (dispatch.arrival->status ==
            OriginalCathedralArrivalStatus::ceremony_started) {
          result.cathedral_ceremony = *dispatch.arrival;
        }
      };

      if (state >= 0x40) {
        if (signed_byte(target[8]) >= 0x40) {
          run_elevator_timeout(*resolved);
        } else {
          dispatch_cathedral();
        }
      } else if (state == 0x05) {
        dispatch_cathedral();
      } else if (state == 0x20 &&
                 original_calendar_phase(document.header.current_day) == 1U) {
        const auto day_phase = static_cast<std::int8_t>(
            original_day_phase(document.header.frame_time));
        if (day_phase == 0) {
          if (document.header.frame_time > 0x0050U &&
              next_original_people_random(document) % 12U == 0U) {
            dispatch_cathedral();
          }
          // 5edd deliberately falls through after the random callback. At
          // this boundary the same person can therefore be called twice.
          if (document.header.frame_time > 0x00f0U) {
            dispatch_cathedral();
          }
        }
        if (day_phase > 0) {
          target[5] = std::byte{0x27};
          ++result.changed;
        }
      }
      continue;
    }

    if (resolved_type == 10) {
      bool dispatch = false;
      if (state >= 0x40) {
        if (signed_byte(target[8]) >= 0x40) {
          run_elevator_timeout(*resolved);
        } else {
          dispatch = true;
        }
      } else if (state == 0x05) {
        dispatch = static_cast<std::int8_t>(
                       original_day_phase(document.header.frame_time)) >= 0;
      } else if (state == 0x27) {
        if (document.header.frame_time > 0x08fcU) {
          target[5] = std::byte{0x20};
          ++result.changed;
        }
      } else if (state == 0x20) {
        const auto* owner = find_original_tenant(
            document, static_cast<std::uint8_t>(owner_floor),
            static_cast<std::uint8_t>(owner_key));
        if (!owner) continue;
        const auto service_index = std::bit_cast<std::int16_t>(
            load_u16(owner->exact_bytes, 6U,
                     document.header.byte_swapped));
        if (service_index < 0 ||
            service_index >=
                static_cast<std::int16_t>(document.retail.size())) {
          continue;
        }
        const auto service_status = signed_byte(
            document.retail[static_cast<std::size_t>(service_index)]
                .exact_bytes[2]);
        if (service_status > -1 || owner->exact_bytes[14] != std::byte{0}) {
          const auto day_phase = original_day_phase(
              document.header.frame_time);
          if (day_phase < 4 && document.header.frame_time > 0x00f0U) {
            dispatch = next_original_people_random(document) % 36U == 0U;
          } else if (day_phase == 4) {
            dispatch = next_original_people_random(document) % 6U == 0U;
          }
        }
      }
      if (!dispatch) continue;
      ++result.dispatched;
      ++result.retail_dispatched;
      const auto step = step_original_retail_person(
          document, *resolved, owner_floor, owner_key, part, rent_income);
      if (step.changed) ++result.changed;
      if (step.activation_visual_requested) {
        ++result.retail_activation_visual_requests;
        append_original_income_status_request(result.host_requests, 6U);
      }
      continue;
    }

    if (resolved_type == 6 || resolved_type == 12) {
      bool dispatch = false;
      if (state >= 0x40) {
        if (signed_byte(target[8]) >= 0x40) {
          run_elevator_timeout(*resolved);
        } else {
          dispatch = true;
        }
      } else if (state == 0x05) {
        dispatch = true;
      } else if (state == 0x27) {
        if (document.header.frame_time > 0x08fcU) {
          target[5] = std::byte{0x20};
          ++result.changed;
        }
      } else if (state == 0x20) {
        const auto* owner = find_original_tenant(
            document, static_cast<std::uint8_t>(owner_floor),
            static_cast<std::uint8_t>(owner_key));
        if (!owner) continue;
        const auto owner_type = signed_byte(owner->exact_bytes[4]);
        const auto day_phase = original_day_phase(document.header.frame_time);
        if (owner_type == 6) {
          if (day_phase == 4) {
            dispatch = next_original_people_random(document) % 12U == 0U;
          } else if (day_phase > 4 &&
                     document.header.frame_time < 0x0898U) {
            dispatch = true;
          }
        } else if (day_phase >= 0 && day_phase < 4 &&
                   document.header.frame_time > 0x00f0U) {
          dispatch = next_original_people_random(document) % 36U == 0U;
        } else if (day_phase == 4) {
          dispatch = next_original_people_random(document) % 6U == 0U;
        }
      }
      if (!dispatch) continue;
      ++result.dispatched;
      ++result.food_service_dispatched;
      if (step_original_food_service_person(
              document, *resolved, owner_floor, owner_key, part).changed) {
        ++result.changed;
      }
      continue;
    }

    if (resolved_type == 15) {
      bool dispatch = false;
      if (state >= 3) {
        if (signed_byte(target[8]) >= 0x40) {
          run_elevator_timeout(*resolved);
        } else {
          dispatch = true;
        }
      } else if (state == 0) {
        dispatch =
            static_cast<std::int8_t>(original_day_phase(
                document.header.frame_time)) >= 0 &&
            document.header.frame_time < 0x05dcU;
      } else if (state == 1 || state == 2) {
        dispatch = true;
      }
      if (!dispatch) continue;
      ++result.dispatched;
      ++result.housekeeping_dispatched;
      if (step_original_housekeeping_person(
              document, *resolved, owner_floor, route_context).changed) {
        ++result.changed;
      }
      continue;
    }

    if (resolved_type != 33) continue;
    bool dispatch = false;
    // 1220:51ea and 51ac compare DS:b3de with signed JLE branches. The live
    // clock normally wraps far below the sign bit, but imported/malformed
    // high-bit words must not pass either time threshold.
    const auto signed_frame =
        std::bit_cast<std::int16_t>(document.header.frame_time);
    if (state >= 0x40) {
      if (signed_byte(target[8]) >= 0x40) {
        run_elevator_timeout(*resolved);
      } else {
        dispatch = true;
      }
    } else if (state == 1) {
      const auto day_phase = static_cast<std::int8_t>(
          original_day_phase(document.header.frame_time));
      if (day_phase >= 0 && day_phase < 4 &&
          signed_frame > 0x00f0) {
        dispatch = next_original_people_random(document) % 36U == 0U;
      }
    } else if (state == 0x22) {
      dispatch = true;
    } else if (state == 0x27 &&
               signed_frame > 0x08fc) {
      target[5] = std::byte{1};
      ++result.changed;
    }
    if (!dispatch) continue;
    ++result.dispatched;
    ++result.metro_dispatched;
    if (step_original_metro_person(
            document, *resolved, owner_floor, part).changed) {
      ++result.changed;
    }
  }
  return result;
}

OriginalCathedralPersonStepResult step_original_cathedral_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPersonRouteRequest& route_context) noexcept {
  OriginalCathedralPersonStepResult result{};
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return result;
  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);
  if (signed_byte(exact[4]) != 36) {
    result.status = OriginalCathedralPersonStepStatus::not_cathedral;
    return result;
  }

  const auto state = signed_byte(exact[5]);
  const auto current_floor = signed_byte(exact[7]);
  const auto transit = signed_byte(exact[8]);
  // 6037's leading 1210:1184 call applies only to a completed Stair leg.
  // Elevator encodings begin at 0x40 and are released by the car/queue code.
  if (state >= 0x40 && transit >= 0 && transit < 0x40) {
    const auto stair_index = static_cast<std::size_t>(transit);
    if (stair_index < document.post_elevator.stairs_bd70.size()) {
      auto& stair = document.post_elevator.stairs_bd70[stair_index];
      if (static_cast<std::int16_t>(stair.floor) == current_floor) {
        stair.word_8 = static_cast<std::uint16_t>(stair.word_8 - 1U);
        store_u16(stair.exact_bytes, 8U, stair.word_8,
                  document.header.byte_swapped);
      } else {
        stair.word_6 = static_cast<std::uint16_t>(stair.word_6 - 1U);
        store_u16(stair.exact_bytes, 6U, stair.word_6,
                  document.header.byte_swapped);
      }
      result.released_stair_counter = true;
      result.changed = true;
    }
  }

  const bool inbound = state == 0x20 || state == 0x60;
  const bool outbound = state == 0x05 || state == 0x45;
  if (!inbound && !outbound) {
    result.status = OriginalCathedralPersonStepStatus::unhandled_state;
    return result;
  }

  auto request = route_context;
  request.tracked_route = true;
  request.visualize_failure = true;
  if (inbound) {
    request.source_floor = state == 0x20 ? 10 : current_floor;
    request.destination_floor = 109;
    request.add_distance_penalty = state == 0x20;
  } else {
    request.source_floor = state == 0x05 ? 109 : current_floor;
    request.destination_floor = 10;
    request.add_distance_penalty = state == 0x05;
  }
  result.route = route_original_person(document, person_index, request);

  switch (result.route.status) {
    case OriginalPersonRouteStatus::no_route:
      exact[5] = std::byte{0x27};
      result.changed = true;
      result.status = OriginalCathedralPersonStepStatus::route_failed;
      return result;
    case OriginalPersonRouteStatus::elevator_queue_full:
    case OriginalPersonRouteStatus::stair:
    case OriginalPersonRouteStatus::elevator:
      exact[5] = static_cast<std::byte>(inbound ? 0x60U : 0x45U);
      result.changed = true;
      result.status = OriginalCathedralPersonStepStatus::routed;
      return result;
    case OriginalPersonRouteStatus::already_on_floor:
      if (inbound) {
        exact[5] = std::byte{3};
        result.changed = true;
        result.cathedral_arrival_check_requested = true;
        result.status =
            OriginalCathedralPersonStepStatus::arrived_cathedral;
      } else {
        exact[5] = std::byte{0x27};
        result.changed = true;
        result.status =
            OriginalCathedralPersonStepStatus::returned_to_lobby;
      }
      return result;
    case OriginalPersonRouteStatus::invalid_person:
    case OriginalPersonRouteStatus::malformed_transport:
      result.status = OriginalCathedralPersonStepStatus::malformed_route;
      return result;
  }
  result.status = OriginalCathedralPersonStepStatus::malformed_route;
  return result;
}

OriginalCathedralArrivalResult apply_original_cathedral_arrival_check(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept {
  OriginalCathedralArrivalResult result{};
  const auto cathedral_key = static_cast<std::int16_t>(
      load_original_header_word(document, 34U));
  if (cathedral_key < 0) return result;
  if (document.header.frame_time >= 0x0320U) {
    result.status =
        OriginalCathedralArrivalStatus::outside_arrival_window;
    return result;
  }

  auto& bottom_floor = document.floors[109U];
  if (static_cast<std::size_t>(cathedral_key) >=
      bottom_floor.tenant_index.size()) {
    result.status = OriginalCathedralArrivalStatus::malformed_cathedral;
    return result;
  }
  const auto bottom_index =
      bottom_floor.tenant_index[static_cast<std::size_t>(cathedral_key)];
  if (bottom_index >= bottom_floor.tenants.size()) {
    result.status = OriginalCathedralArrivalStatus::malformed_cathedral;
    return result;
  }
  auto& bottom = bottom_floor.tenants[bottom_index];
  const auto set_variant_word = [&](OriginalTdtTenant& tenant,
                                    std::uint16_t value) {
    store_u16(tenant.exact_bytes, 6U, value,
              document.header.byte_swapped);
    tenant.variant = std::to_integer<std::uint8_t>(tenant.exact_bytes[6]);
    tenant.preserved_07_to_0f[0] = tenant.exact_bytes[7];
    tenant.exact_bytes[13] = std::byte{1};
    tenant.preserved_07_to_0f[6] = std::byte{1};
  };

  // Exact signed thresholds loaded by 1190:0005 from PART/1000 +42..+4e.
  const auto population = document.post_elevator.finance.total_population;
  std::int16_t population_rating = 6;
  if (population < std::bit_cast<std::int32_t>(part.dwords_42_to_4e[0])) {
    population_rating = 1;
  } else if (population <
             std::bit_cast<std::int32_t>(part.dwords_42_to_4e[1])) {
    population_rating = 2;
  } else if (population <
             std::bit_cast<std::int32_t>(part.dwords_42_to_4e[2])) {
    population_rating = 3;
  } else if (population <
             std::bit_cast<std::int32_t>(part.dwords_42_to_4e[3])) {
    population_rating = 4;
  } else if (population < 15000) {
    population_rating = 5;
  }
  if (population_rating > static_cast<std::int16_t>(document.header.rating)) {
    set_variant_word(bottom, 3U);
    result.status =
        OriginalCathedralArrivalStatus::population_rating_gate;
    return result;
  }

  // Exact 1040:03bb Cathedral readiness count: inspect eight people in each
  // of the five type-36..40 records and continue only at exactly forty.
  for (const auto& floor : document.floors) {
    for (const auto& tenant : floor.tenants) {
      if (tenant.type < 36 || tenant.type > 40) continue;
      const auto people_start = load_u32(
          tenant.exact_bytes, 8U, document.header.byte_swapped);
      if (people_start > document.people.size() ||
          document.people.size() - people_start < 8U) {
        result.status = OriginalCathedralArrivalStatus::malformed_cathedral;
        return result;
      }
      for (std::size_t ordinal = 0U; ordinal < 8U; ++ordinal) {
        if (document.people[people_start + ordinal].exact_bytes[5] ==
            std::byte{3}) {
          ++result.arrived_people;
        }
      }
    }
  }
  if (result.arrived_people != 40U) {
    set_variant_word(bottom, 3U);
    result.status = OriginalCathedralArrivalStatus::waiting_for_people;
    return result;
  }

  // 1040:02b5 returns immediately at rating six, before b406 or any visual,
  // audio, frame, or focus mutation.
  if (document.header.rating == 6U) {
    result.status =
        OriginalCathedralArrivalStatus::already_maximum_rating;
    return result;
  }

  store_original_header_word(
      document, 60U,
      static_cast<std::uint16_t>(
          load_original_header_word(document, 60U) + 4U));
  store_original_header_dword(document, 66U, 0U);
  result.effect_floor = 111;
  result.effect_x = static_cast<std::uint16_t>(
      load_u16(bottom.exact_bytes, 6U, document.header.byte_swapped) + 14U);

  document.header.rating = 6U;
  if (document.header.exact_bytes.size() >= 4U) {
    store_u16(document.header.exact_bytes, 2U, 6U,
              document.header.byte_swapped);
  }
  for (auto& floor : document.floors) {
    for (auto& tenant : floor.tenants) {
      if (tenant.type >= 36 && tenant.type <= 40) {
        set_variant_word(tenant, 2U);
      }
    }
  }

  result.rating_changed = true;
  result.repaint_requested = true;
  result.stop_both_audio_channels = true;
  result.wave_resource = 10008;
  result.wave_repeat = 5U;
  result.wave_priority = 4U;
  result.status = OriginalCathedralArrivalStatus::ceremony_started;
  return result;
}

OriginalCathedralPersonDispatchResult dispatch_original_cathedral_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPartTable& part) noexcept {
  OriginalCathedralPersonDispatchResult result{};
  result.step = step_original_cathedral_person(
      document, person_index, original_person_route_context(document, part));
  if (result.step.cathedral_arrival_check_requested) {
    result.arrival = apply_original_cathedral_arrival_check(document, part);
  }
  return result;
}

OriginalOfficePersonStepStatus step_original_office_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::uint16_t movement_delta) {
  if (person_index >= document.people.size() ||
      person_index >= document.people_count) {
    return OriginalOfficePersonStepStatus::invalid_person;
  }
  auto& person = document.people[person_index];
  auto exact = std::span<std::byte>(person.exact_bytes);
  if (exact[4] != std::byte{7}) {
    return OriginalOfficePersonStepStatus::not_office;
  }

  const std::uint8_t state = std::to_integer<std::uint8_t>(exact[5]);
  apply_original_person_common_update(
      person, movement_delta, document.header.byte_swapped);

  // CS:2004 contains 40,41,42,45,60,61,62,63. CS:2014 maps the first three
  // to 1c3d and the remaining five to 1bbe.
  if (state == 0x40U || state == 0x41U || state == 0x42U) {
    auto* tenant = find_original_tenant(
        document, std::to_integer<std::uint8_t>(exact[0]),
        std::to_integer<std::uint8_t>(exact[1]));
    if (!tenant) {
      return OriginalOfficePersonStepStatus::malformed_tenant_link;
    }
    set_original_tenant_status(
        *tenant, tenant->status == 8U
                     ? 1U
                     : static_cast<std::uint8_t>(tenant->status + 1U));
    mark_original_tenant_changed(*tenant);
    exact[5] = std::byte{5};
    return OriginalOfficePersonStepStatus::entered_office;
  }

  if (state == 0x45U || (state >= 0x60U && state <= 0x63U)) {
    exact[5] = std::byte{0x26};
    return remove_original_office_presence(document, person_index, person);
  }
  return OriginalOfficePersonStepStatus::common_update_only;
}

OriginalTransitPersonDispatchResult dispatch_original_transit_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::uint16_t movement_delta,
    const OriginalPartTable& part) {
  OriginalTransitPersonDispatchResult result{};
  result.person_index = person_index;
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return result;

  auto& person = document.people[person_index];
  const auto before = person.exact_bytes;
  auto exact = std::span<std::byte>(person.exact_bytes);
  result.person_type = static_cast<std::int8_t>(signed_byte(exact[4]));
  result.state_before = std::to_integer<std::uint8_t>(exact[5]);

  // Office already has a public exact translation of its 1aed subtable.
  // Let it own the common metric pair so those calls occur exactly once.
  if (result.person_type == 7) {
    auto* owner = find_original_tenant(
        document, std::to_integer<std::uint8_t>(exact[0]),
        std::to_integer<std::uint8_t>(exact[1]));
    const auto owner_status = owner ? std::optional<std::uint8_t>(owner->status)
                                    : std::nullopt;
    const bool had_parking = original_person_has_parking(person);
    const auto status =
        step_original_office_person(document, person_index, movement_delta);
    result.common_update_applied = true;
    switch (status) {
      case OriginalOfficePersonStepStatus::entered_office:
        result.status = OriginalTransitPersonDispatchStatus::office_arrived;
        break;
      case OriginalOfficePersonStepStatus::left_office:
        result.status = OriginalTransitPersonDispatchStatus::office_departed;
        break;
      case OriginalOfficePersonStepStatus::malformed_tenant_link:
      case OriginalOfficePersonStepStatus::malformed_route_table:
        result.status = OriginalTransitPersonDispatchStatus::malformed_owner;
        break;
      case OriginalOfficePersonStepStatus::invalid_person:
      case OriginalOfficePersonStepStatus::not_office:
      case OriginalOfficePersonStepStatus::common_update_only:
        result.status =
            OriginalTransitPersonDispatchStatus::common_update_only;
        break;
    }
    result.owner_status_changed =
        owner && owner_status && owner->status != *owner_status;
    result.parking_changed =
        had_parking && !original_person_has_parking(person);
    result.changed = before != person.exact_bytes ||
                     result.owner_status_changed || result.parking_changed;
    return result;
  }

  // 1aed skips both 11d8 calls only for Housekeeping type 15. This happens
  // before its family switch, so even unlisted/unknown types receive the
  // common metric mutation.
  if (result.person_type != 15) {
    apply_original_person_common_update(
        person, movement_delta, document.header.byte_swapped);
    result.common_update_applied = true;
  }

  switch (result.person_type) {
    case 3:
    case 4:
    case 5:
      if (result.state_before == 0x45U) {
        const bool had_parking = original_person_has_parking(person);
        exact[5] = std::byte{0x26};
        remove_original_hotel_person_parking(document, person_index);
        result.parking_changed =
            had_parking && !original_person_has_parking(person);
        if (original_hotel_periodic_visitor_matches(document, person_index)) {
          document.post_elevator.b923 = 0U;
          document.post_elevator.b928 = 0U;
          document.post_elevator.b924 = -1;
          result.periodic_visitor_changed = true;
          result.process_requests.push_back({3003U, 0});
          result.host_requests.push_back(
              {OriginalPersonHostRequestKind::hotel_dialog, 3003U, 0});
        }
        result.status = OriginalTransitPersonDispatchStatus::hotel_departed;
      } else if (result.state_before == 0x41U ||
                 result.state_before == 0x60U ||
                 result.state_before == 0x62U) {
        auto* owner = find_original_tenant(
            document, std::to_integer<std::uint8_t>(exact[0]),
            std::to_integer<std::uint8_t>(exact[1]));
        if (!owner) {
          result.status = OriginalTransitPersonDispatchStatus::malformed_owner;
          break;
        }
        finish_original_hotel_guest(document, *owner);
        exact[5] = std::byte{4};
        result.owner_status_changed = true;
        result.status = OriginalTransitPersonDispatchStatus::hotel_arrived;
      } else {
        result.status =
            OriginalTransitPersonDispatchStatus::common_update_only;
      }
      break;

    case 9:
      if (result.state_before == 0x40U ||
          result.state_before == 0x41U ||
          result.state_before == 0x60U ||
          result.state_before == 0x61U ||
          result.state_before == 0x62U) {
        auto* owner = find_original_tenant(
            document, std::to_integer<std::uint8_t>(exact[0]),
            std::to_integer<std::uint8_t>(exact[1]));
        if (!owner) {
          result.status = OriginalTransitPersonDispatchStatus::malformed_owner;
          break;
        }
        finish_original_condo_resident(document, *owner);
        exact[5] = std::byte{4};
        result.owner_status_changed = true;
        result.status = OriginalTransitPersonDispatchStatus::condo_arrived;
      } else {
        result.status =
            OriginalTransitPersonDispatchStatus::common_update_only;
      }
      break;

    case 6:
    case 10:
    case 12: {
      // 1e85 stores state 27 before resolving the owner's runtime +0a/+0c
      // fields. In serialized tenant bytes those are type at +4 and the
      // service word at +6 because the floor allocation has a six-byte head.
      exact[5] = std::byte{0x27};
      auto* owner = find_original_tenant(
          document, std::to_integer<std::uint8_t>(exact[0]),
          std::to_integer<std::uint8_t>(exact[1]));
      if (!owner) {
        result.status = OriginalTransitPersonDispatchStatus::malformed_owner;
        break;
      }
      result.service_index = std::bit_cast<std::int16_t>(
          load_u16(owner->exact_bytes, 6U, document.header.byte_swapped));
      if (result.service_index < 0 ||
          result.service_index >=
              static_cast<std::int16_t>(document.retail.size())) {
        result.status =
            OriginalTransitPersonDispatchStatus::malformed_service;
        break;
      }
      result.service_history_changed = update_original_food_service_history(
          document, person, signed_byte(owner->exact_bytes[4]),
          result.service_index, part);
      result.status =
          OriginalTransitPersonDispatchStatus::commercial_completed;
      break;
    }

    case 18:
    case 29:
    case 33:
    case 36:
      exact[5] = std::byte{0x27};
      result.status = OriginalTransitPersonDispatchStatus::terminal_state;
      break;

    case 15:
      exact[5] = std::byte{0};
      exact[7] = std::byte{0xff};
      result.status = OriginalTransitPersonDispatchStatus::housekeeping_reset;
      break;

    default:
      result.status =
          OriginalTransitPersonDispatchStatus::common_update_only;
      break;
  }

  result.changed = before != person.exact_bytes ||
                   result.owner_status_changed || result.parking_changed ||
                   result.periodic_visitor_changed ||
                   result.service_history_changed;
  return result;
}

OriginalElevatorWaitingDispatchResult
dispatch_original_elevator_waiting_people(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::uint16_t movement_delta,
    const OriginalPartTable& part) {
  OriginalElevatorWaitingDispatchResult result{};
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return result;

  const auto& target = document.people[person_index].exact_bytes;
  const auto original_floor = signed_byte(target[7]);
  auto elevator_code = signed_byte(target[8]);
  if (elevator_code < 0x40) {
    result.person_indices.push_back(person_index);
    result.dispatches.push_back(dispatch_original_transit_person(
        document, person_index, movement_delta, part));
    result.status =
        OriginalElevatorWaitingDispatchStatus::direct_dispatch;
    return result;
  }

  // Exact 1210:1d56 decoder: byte 8 minus 0x40 selects an Elevator; values
  // 24..47 subtract 24 again and select the second waiting lane.
  elevator_code = static_cast<std::int16_t>(elevator_code - 0x40);
  bool first_lane = true;
  if (elevator_code >= 24) {
    elevator_code = static_cast<std::int16_t>(elevator_code - 24);
    first_lane = false;
  }
  if (elevator_code < 0 ||
      elevator_code >= static_cast<std::int16_t>(document.elevators.size()) ||
      original_floor < 0 || original_floor >= 120) {
    result.status =
        OriginalElevatorWaitingDispatchStatus::malformed_queue;
    return result;
  }

  auto& elevator = document.elevators[static_cast<std::size_t>(elevator_code)];
  const auto mapped = original_elevator_floor_record_index(
      elevator.type, elevator.bottom_floor, elevator.top_floor,
      original_floor);
  auto* record = find_original_elevator_floor_record(elevator, mapped);
  if (mapped < 0 || record == nullptr) {
    result.status =
        OriginalElevatorWaitingDispatchStatus::malformed_queue;
    return result;
  }

  auto& queue = record->exact_bytes;
  const std::size_t count_offset = first_lane ? 0U : 2U;
  const std::size_t cursor_offset = first_lane ? 1U : 3U;
  const std::size_t table_offset = first_lane ? 4U : 164U;
  const auto count = signed_byte(queue[count_offset]);
  const auto cursor = signed_byte(queue[cursor_offset]);
  if (count <= 0 || count > 40 || cursor < 0 || cursor >= 40) {
    result.status =
        OriginalElevatorWaitingDispatchStatus::malformed_queue;
    return result;
  }

  // Preflight the exact active span through the target. The executable
  // assumes valid far pointers and loops until it sees the original person;
  // rejecting malformed loaded data here prevents a native infinite loop or
  // partial queue mutation while leaving every valid TDT byte-exact.
  std::size_t target_ordinal = static_cast<std::size_t>(count);
  for (std::size_t ordinal = 0U;
       ordinal < static_cast<std::size_t>(count); ++ordinal) {
    const auto slot =
        (static_cast<std::size_t>(cursor) + ordinal) % 40U;
    const auto queued = load_u32(
        queue, table_offset + slot * 4U, document.header.byte_swapped);
    if (queued >= person_limit) {
      result.status =
          OriginalElevatorWaitingDispatchStatus::malformed_queue;
      return result;
    }
    if (queued == person_index) {
      target_ordinal = ordinal;
      break;
    }
  }
  if (target_ordinal == static_cast<std::size_t>(count)) {
    result.status =
        OriginalElevatorWaitingDispatchStatus::malformed_queue;
    return result;
  }

  for (std::size_t ordinal = 0U; ordinal <= target_ordinal; ++ordinal) {
    const auto active_cursor =
        std::to_integer<std::uint8_t>(queue[cursor_offset]);
    const auto queued = load_u32(
        queue, table_offset + active_cursor * 4U,
        document.header.byte_swapped);
    // 1210:1332 advances the ring before 1220:1aed is called. It deliberately
    // leaves the stale person dword in the retired slot.
    queue[cursor_offset] =
        static_cast<std::byte>((active_cursor + 1U) % 40U);
    queue[count_offset] = static_cast<std::byte>(
        std::to_integer<std::uint8_t>(queue[count_offset]) - 1U);
    result.person_indices.push_back(static_cast<std::size_t>(queued));
    result.dispatches.push_back(dispatch_original_transit_person(
        document, static_cast<std::size_t>(queued), movement_delta, part));
  }

  result.view_slot_restore_requested = true;
  result.view_floor = original_floor;
  result.status = OriginalElevatorWaitingDispatchStatus::ring_dispatched;
  return result;
}

OriginalElevatorWaitTimeoutResult step_original_elevator_wait_timeout(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPartTable& part) {
  OriginalElevatorWaitTimeoutResult result{};
  const auto person_limit =
      std::min<std::size_t>(document.people_count, document.people.size());
  if (person_index >= person_limit) return result;

  const auto timestamp = load_u16(
      document.people[person_index].exact_bytes, 10U,
      document.header.byte_swapped);
  if (timestamp == 0U) {
    result.status = OriginalElevatorWaitTimeoutStatus::not_armed;
    return result;
  }
  const auto elapsed = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(document.header.frame_time - timestamp));
  const auto threshold =
      std::bit_cast<std::int16_t>(part.words_00_to_40[0U]);
  if (elapsed < threshold) {
    result.status = OriginalElevatorWaitTimeoutStatus::pending;
    return result;
  }

  result.dispatch = dispatch_original_elevator_waiting_people(
      document, person_index, document.header.frame_time, part);
  if (result.dispatch.status ==
      OriginalElevatorWaitingDispatchStatus::malformed_queue) {
    result.status = OriginalElevatorWaitTimeoutStatus::malformed_queue;
  } else if (result.dispatch.status ==
                 OriginalElevatorWaitingDispatchStatus::invalid_person) {
    result.status = OriginalElevatorWaitTimeoutStatus::invalid_person;
  } else {
    result.status = OriginalElevatorWaitTimeoutStatus::dispatched;
  }
  return result;
}

std::size_t reset_original_office_people_for_day(
    OriginalTdtDocument& document) noexcept {
  std::size_t reset = 0;
  const std::size_t count =
      document.people_count < document.people.size()
          ? static_cast<std::size_t>(document.people_count)
          : document.people.size();
  for (std::size_t index = 0; index < count; ++index) {
    auto& exact = document.people[index].exact_bytes;
    if (exact[4] != std::byte{7}) {
      continue;
    }
    exact[5] = std::byte{0x20};
    exact[7] = std::byte{0};
    exact[8] = std::byte{0};
    store_u16(exact, 12, 0, document.header.byte_swapped);
    ++reset;
  }
  return reset;
}

std::size_t reset_original_people_for_day(
    OriginalTdtDocument& document) noexcept {
  std::size_t reset = 0U;
  const std::size_t count =
      document.people_count < document.people.size()
          ? static_cast<std::size_t>(document.people_count)
          : document.people.size();

  const auto clear_short_tail = [&](std::span<std::byte> exact) {
    exact[7] = std::byte{0};
    exact[8] = std::byte{0};
  };
  const auto clear_full_tail = [&](std::span<std::byte> exact) {
    exact[7] = std::byte{0};
    exact[8] = std::byte{0};
    exact[9] = std::byte{0};
    store_u16(exact, 10U, 0U, document.header.byte_swapped);
    store_u16(exact, 12U, 0U, document.header.byte_swapped);
    store_u16(exact, 14U, 0U, document.header.byte_swapped);
  };

  for (std::size_t index = 0; index < count; ++index) {
    auto exact = std::span<std::byte>(document.people[index].exact_bytes);
    const auto type = std::to_integer<std::uint8_t>(exact[4]);
    switch (type) {
      case 3U:
      case 4U:
      case 5U: {
        if (load_u16(exact, 2U, document.header.byte_swapped) == 0U) {
          exact[5] = std::byte{0x24};
        } else {
          const auto* tenant = find_original_tenant(
              document, std::to_integer<std::uint8_t>(exact[0]),
              std::to_integer<std::uint8_t>(exact[1]));
          if (tenant == nullptr) {
            continue;
          }
          // 1220:01ba-021b uses signed JGE on tenant byte +0b. High-bit
          // transient statuses therefore take the same 0x10 branch as values
          // below 0x18, rather than comparing as an unsigned byte.
          exact[5] = std::bit_cast<std::int8_t>(tenant->status) < 0x18
                         ? std::byte{0x10}
                         : std::byte{0x20};
        }
        clear_short_tail(exact);
        ++reset;
        break;
      }
      case 7U:
        // Type 7 has the deliberately shorter loc_00a4 reset: byte 9,
        // word 10, and word 14 survive.
        exact[5] = std::byte{0x20};
        clear_short_tail(exact);
        store_u16(exact, 12U, 0U, document.header.byte_swapped);
        ++reset;
        break;
      case 9U: {
        const auto* tenant = find_original_tenant(
            document, std::to_integer<std::uint8_t>(exact[0]),
            std::to_integer<std::uint8_t>(exact[1]));
        if (tenant == nullptr) {
          continue;
        }
        // 1220:01ec-021b shares the signed status comparison with Hotel.
        exact[5] = std::bit_cast<std::int8_t>(tenant->status) < 0x18
                       ? std::byte{0x10}
                       : std::byte{0x20};
        clear_short_tail(exact);
        ++reset;
        break;
      }
      case 6U:
      case 10U:
      case 12U:
        exact[5] = std::byte{0x20};
        clear_full_tail(exact);
        ++reset;
        break;
      case 14U:
      case 33U:
        exact[5] = std::byte{1};
        clear_full_tail(exact);
        ++reset;
        break;
      case 15U:
        exact[5] = std::byte{0};
        clear_full_tail(exact);
        exact[7] = std::byte{0xff};
        ++reset;
        break;
      case 18U:
      case 29U:
      case 36U:
        exact[5] = std::byte{0x27};
        clear_full_tail(exact);
        ++reset;
        break;
      default:
        break;
    }
  }
  return reset;
}

std::size_t initialize_original_people_runtime_state(
    OriginalTdtDocument& document) noexcept {
  const auto count = std::min<std::size_t>(
      document.people_count, document.people.size());
  std::size_t initialized{};
  for (std::size_t index = 0U; index < count; ++index) {
    auto& bytes = document.people[index].exact_bytes;
    const auto type = signed_byte(bytes[4]);
    switch (type) {
      case 3:
      case 4:
      case 5:
        bytes[5] = std::byte{0x26};
        bytes[7] = std::byte{0};
        bytes[8] = std::byte{0};
        std::fill(bytes.begin() + 10U, bytes.begin() + 14U,
                  std::byte{0});
        ++initialized;
        break;
      case 7:
        bytes[5] = std::byte{0x27};
        bytes[7] = std::byte{0};
        bytes[8] = std::byte{0};
        std::fill(bytes.begin() + 10U, bytes.begin() + 14U,
                  std::byte{0});
        ++initialized;
        break;
      case 9:
        bytes[5] = std::byte{0x21};
        bytes[7] = std::byte{0};
        bytes[8] = std::byte{0};
        std::fill(bytes.begin() + 10U, bytes.begin() + 14U,
                  std::byte{0});
        ++initialized;
        break;
      case 6:
      case 10:
      case 12:
      case 18:
      case 29:
      case 33:
      case 36:
        bytes[5] = std::byte{0x27};
        bytes[7] = std::byte{0};
        std::fill(bytes.begin() + 8U, bytes.end(), std::byte{0});
        ++initialized;
        break;
      case 15:
        bytes[5] = std::byte{0};
        bytes[7] = std::byte{0xff};
        std::fill(bytes.begin() + 8U, bytes.end(), std::byte{0});
        ++initialized;
        break;
      default:
        break;
    }
  }
  return initialized;
}

std::size_t initialize_original_tenant_runtime_state(
    OriginalTdtDocument& document) noexcept {
  // 10b0:0072 walks the 120 six-byte-headed floor blocks and dispatches on
  // tenant byte +4. Runtime offsets +0a/+0b/+0c/+13 therefore correspond to
  // serialized tenant bytes +4/+5/+6/+13.
  std::size_t initialized{};
  const auto mark_initialized = [&](OriginalTdtTenant& tenant) {
    mark_original_tenant_changed(tenant);
    ++initialized;
  };
  const auto reset_retail = [&](OriginalTdtTenant& tenant,
                                bool preserve_unused) {
    const auto linked = load_u16(
        tenant.exact_bytes, 6U, document.header.byte_swapped);
    if (linked >= document.retail.size()) return;
    auto& record = document.retail[linked].exact_bytes;
    if (preserve_unused && record[2] == std::byte{0xff}) return;
    record[2] = std::byte{3};
    record[9] = std::byte{0};
    mark_initialized(tenant);
  };
  const auto reset_dc24 = [&](OriginalTdtTenant& tenant) {
    const auto linked = load_u16(
        tenant.exact_bytes, 6U, document.header.byte_swapped);
    if (linked >= document.post_elevator.dc24_records.size()) return;
    auto& record = document.post_elevator.dc24_records[linked];
    // The executable writes DS:dc2a, dc28, dc29, and dc2e in that order.
    record[6] = std::byte{0};
    record[4] = std::byte{0};
    record[5] = std::byte{0};
    record[10] = std::byte{0};
    mark_initialized(tenant);
  };
  const auto clear_tenant_link = [&](OriginalTdtTenant& tenant) {
    store_u16(tenant.exact_bytes, 6U, 0U,
              document.header.byte_swapped);
    tenant.variant = 0U;
    tenant.preserved_07_to_0f[0] = std::byte{0};
    mark_initialized(tenant);
  };

  for (auto& floor : document.floors) {
    for (auto& tenant : floor.tenants) {
      switch (tenant.type) {
        case 3:
        case 4:
        case 5:
          if (signed_byte(tenant.exact_bytes[5]) < 0x18) {
            set_original_tenant_status(tenant, 0x28U);
            mark_initialized(tenant);
          }
          break;
        case 6:
        case 12:
          reset_retail(tenant, false);
          break;
        case 7:
          if (signed_byte(tenant.exact_bytes[5]) < 0x10) {
            set_original_tenant_status(tenant, 0U);
            mark_initialized(tenant);
          }
          break;
        case 9:
          if (signed_byte(tenant.exact_bytes[5]) < 0x18) {
            set_original_tenant_status(tenant, 0U);
            mark_initialized(tenant);
          }
          break;
        case 10:
          reset_retail(tenant, true);
          break;
        case 11:
          set_original_tenant_status(tenant, 0U);
          mark_initialized(tenant);
          break;
        case 18:
        case 19:
        case 29:
        case 30:
        case 34:
        case 35:
          reset_dc24(tenant);
          break;
        case 31:
        case 32:
        case 33:
        case 36:
        case 37:
        case 38:
        case 39:
        case 40:
          clear_tenant_link(tenant);
          break;
        default:
          break;
      }
    }
  }
  return initialized;
}

std::size_t sweep_original_people_transit(
    OriginalTdtDocument& document,
    std::uint16_t frame_time) noexcept {
  std::size_t finalized = 0U;
  for (auto& floor : document.floors) {
    for (const auto& source_tenant : floor.tenants) {
      finalized += finalize_original_facility_people(
          document, floor, source_tenant, frame_time);
    }
  }
  return finalized;
}

std::size_t count_original_vertical_transport_cleanup_people(
    const OriginalTdtDocument& document,
    std::size_t transport_index) noexcept {
  // byte 8 is sign-extended before the word comparison at each 1218:0000
  // family branch. Therefore no record can match a bd70 index above 63.
  if (transport_index >= 64U) return 0U;

  const auto bounded_count = std::min<std::size_t>(
      document.people_count, document.people.size());
  std::size_t matches = 0U;
  for (std::size_t index = 0; index < bounded_count; ++index) {
    const auto& exact = document.people[index].exact_bytes;
    const auto type = signed_byte(exact[4]);
    std::int16_t threshold = 0x40;
    switch (type) {
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 9:
      case 10:
      case 12:
      case 18:
      case 29:
      case 33:
      case 36:
        break;
      case 15:
        threshold = 3;
        break;
      default:
        continue;
    }
    if (signed_byte(exact[5]) < threshold ||
        signed_byte(exact[8]) !=
            static_cast<std::int16_t>(transport_index)) {
      continue;
    }
    ++matches;
  }
  return matches;
}

OriginalVerticalTransportPeopleCleanupResult
cleanup_original_vertical_transport_people(
    OriginalTdtDocument& document,
    std::size_t transport_index,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) {
  OriginalVerticalTransportPeopleCleanupResult result{};
  // The caller's hit-test can only produce 0..63. Preserve a checked native
  // boundary for malformed callers before reproducing 1218's direct scan.
  if (transport_index >= 64U) return result;
  result.valid_transport_index = true;

  const auto person_limit = std::min<std::size_t>(
      document.people_count, document.people.size());
  for (std::size_t person_index = 0U;
       person_index < person_limit; ++person_index) {
    const auto& exact = document.people[person_index].exact_bytes;
    const auto type = signed_byte(exact[4]);
    std::int16_t threshold = 0x40;
    switch (type) {
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 9:
      case 10:
      case 12:
      case 18:
      case 29:
      case 33:
      case 36:
        break;
      case 15:
        threshold = 3;
        break;
      default:
        continue;
    }
    if (signed_byte(exact[5]) < threshold ||
        signed_byte(exact[8]) !=
            static_cast<std::int16_t>(transport_index)) {
      continue;
    }
    result.family_dispatches.push_back(dispatch_original_person_family(
        document, person_index, part, rent_income,
        OriginalPersonFamilyDispatchSource::vertical_transport_1218));
  }
  return result;
}

std::optional<std::uint32_t> pop_original_elevator_car_passenger_slot(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index,
    std::size_t slot_index) noexcept {
  constexpr std::size_t kOriginalCarSlotCount = 42U;
  if (elevator_index >= document.elevators.size() ||
      car_index >= document.elevators[elevator_index].car_records.size() ||
      slot_index >= kOriginalCarSlotCount) {
    return std::nullopt;
  }

  auto& exact = document.elevators[elevator_index]
                    .car_records[car_index]
                    .exact_bytes;
  const auto person_index =
      load_u32(exact, 16U + slot_index * 4U,
               document.header.byte_swapped);
  exact[184U + slot_index] = std::byte{0xff};
  store_u32(exact, 16U + slot_index * 4U, 0xffffffffU,
            document.header.byte_swapped);
  return person_index;
}

OriginalElevatorBoardingDestination
select_original_elevator_boarding_destination(
    const OriginalTdtDocument& document,
    std::size_t person_index,
    std::size_t elevator_index,
    std::int16_t current_floor,
    bool direction_up) noexcept {
  OriginalElevatorBoardingDestination result{};
  const auto person_limit = std::min<std::size_t>(
      document.people_count, document.people.size());
  if (person_index >= person_limit) return result;
  if (elevator_index >= document.elevators.size()) {
    result.status =
        OriginalElevatorBoardingDestinationStatus::invalid_elevator;
    return result;
  }

  const auto& person = document.people[person_index].exact_bytes;
  const auto type = signed_byte(person[4]);
  const auto state = signed_byte(person[5]);
  const auto owner_floor = signed_byte(person[0]);
  const auto linked_service_floor = [&]() -> std::int16_t {
    const auto index = signed_byte(person[6]);
    if (index < 0) return 10;
    if (index >= static_cast<std::int16_t>(document.retail.size())) return -1;
    return signed_byte(
        document.retail[static_cast<std::size_t>(index)].exact_bytes[0]);
  };
  const auto medical_floor = [&]() -> std::int16_t {
    // Exact 1170:0522 destination helper: a negative service uses floor ten;
    // otherwise return that Medical Center record's floor byte.
    const auto index = signed_byte(person[6]);
    if (index < 0) return 10;
    if (index >= static_cast<std::int16_t>(
                     document.post_elevator.dbfc_dwords.size())) {
      return -1;
    }
    return original_medical_floor(
        document.post_elevator.dbfc_dwords[static_cast<std::size_t>(index)]);
  };
  const auto entertainment_floor = [&]() -> std::int16_t {
    const auto floor = signed_byte(person[0]);
    const auto key = signed_byte(person[1]);
    if (floor < 0 || key < 0) return -1;
    const auto* owner = find_original_tenant(
        document, static_cast<std::uint8_t>(floor),
        static_cast<std::uint8_t>(key));
    if (!owner) return -1;
    const auto linked = std::bit_cast<std::int16_t>(load_u16(
        owner->exact_bytes, 6U, document.header.byte_swapped));
    if (linked < 0 || linked >= static_cast<std::int16_t>(
                                  document.post_elevator.dc24_records.size())) {
      return -1;
    }
    const auto& record =
        document.post_elevator.dc24_records[static_cast<std::size_t>(linked)];
    // Exact 1180:0dcc is the signed byte-one accessor used for the lower or
    // single-facility floor; paired records instead select byte zero here.
    return signed_byte(record[signed_byte(record[7]) >= 0 ? 0U : 1U]);
  };

  std::optional<std::int16_t> final_destination{};
  switch (type) {
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 9:
    case 10:
    case 12:
      switch (state) {
        case 0x40:
          final_destination = 10;
          break;
        case 0x41:
          final_destination = linked_service_floor();
          break;
        case 0x42:
          final_destination = medical_floor();
          break;
        case 0x45:
          final_destination = original_person_parking_floor(
              document, document.people[person_index]);
          break;
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
          final_destination = owner_floor;
          break;
      }
      break;
    case 15:
      if (state == 3) final_destination = signed_byte(person[6]);
      if (state == 4) final_destination = owner_floor;
      break;
    case 18:
    case 29:
      if (state == 0x41) final_destination = linked_service_floor();
      if (state == 0x45 || state == 0x62) final_destination = 10;
      if (state == 0x60) final_destination = entertainment_floor();
      break;
    case 33:
      if (state == 0x45) final_destination = 10;
      if (state == 0x60) final_destination = 109;
      break;
    case 36:
      if (state == 0x41) final_destination = linked_service_floor();
      if (state == 0x62) {
        final_destination = static_cast<std::int16_t>(owner_floor + 2);
      }
      break;
    default:
      break;
  }
  if (!final_destination) {
    result.status = OriginalElevatorBoardingDestinationStatus::
        unsupported_family_state;
    return result;
  }
  result.final_destination = *final_destination;
  if (!original_route_floor_valid(*final_destination) ||
      !original_route_floor_valid(current_floor)) {
    result.status = OriginalElevatorBoardingDestinationStatus::no_route;
    return result;
  }

  const auto& elevator = document.elevators[elevator_index];
  if (elevator.used == 0U) {
    result.status = OriginalElevatorBoardingDestinationStatus::no_route;
    return result;
  }
  if (elevator.serviced_floors[
          static_cast<std::size_t>(*final_destination)] != std::byte{0}) {
    result.car_destination = *final_destination;
    result.status = OriginalElevatorBoardingDestinationStatus::selected;
    return result;
  }

  const auto destination_graph = load_u32(
      elevator.block_c2,
      static_cast<std::size_t>(*final_destination) * 4U,
      document.header.byte_swapped);
  const auto elevator_bit = original_route_bit(elevator_index);
  if (destination_graph == 0U || elevator_bit == 0U) {
    result.status = OriginalElevatorBoardingDestinationStatus::no_route;
    return result;
  }
  for (const auto& transfer : document.post_elevator.db9c_records) {
    auto mask = load_u32(transfer, 0U, document.header.byte_swapped);
    if ((mask & elevator_bit) == 0U) continue;
    const auto transfer_floor = signed_byte(transfer[4]);
    if (transfer_floor == current_floor) continue;
    mask &= ~elevator_bit;
    if ((destination_graph & mask) == 0U ||
        ((transfer_floor > current_floor) != direction_up)) {
      continue;
    }
    result.car_destination = transfer_floor;
    result.status = OriginalElevatorBoardingDestinationStatus::selected;
    return result;
  }
  result.status = OriginalElevatorBoardingDestinationStatus::no_route;
  return result;
}

OriginalElevatorPassengerStepResult
step_original_elevator_car_passengers(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income,
    bool isolation_active,
    std::function<void(const OriginalPersonFamilyDispatchResult&)>
        family_dispatch_callback) {
  OriginalElevatorPassengerStepResult result{};
  if (elevator_index >= document.elevators.size() ||
      car_index >= document.elevators[elevator_index].car_records.size()) {
    return result;
  }
  auto& elevator = document.elevators[elevator_index];
  auto& car = elevator.car_records[car_index].exact_bytes;
  if (elevator.used == 0U || car[15] == std::byte{0}) {
    result.status = OriginalElevatorPassengerStepStatus::inactive_car;
    return result;
  }
  const auto floor = signed_byte(car[0]);
  const auto person_limit = std::min<std::size_t>(
      document.people_count, document.people.size());
  const auto capacity = std::min<std::size_t>(elevator.capacity, 42U);
  const auto passenger_count = signed_byte(car[3]);
  const auto distinct_destination_count = signed_byte(car[12]);
  if (!original_route_floor_valid(floor) || passenger_count < 0 ||
      passenger_count > static_cast<std::int16_t>(capacity) ||
      distinct_destination_count < 0 ||
      distinct_destination_count > passenger_count) {
    result.status = OriginalElevatorPassengerStepStatus::malformed_state;
    return result;
  }

  struct AlightingPassenger {
    std::size_t slot{};
    std::size_t person{};
  };
  std::vector<AlightingPassenger> alighting{};
  if (car[2] == std::byte{5} &&
      car[226U + static_cast<std::size_t>(floor)] != std::byte{0}) {
    const auto floor_occupancy =
        signed_byte(car[226U + static_cast<std::size_t>(floor)]);
    if (passenger_count == 0 || distinct_destination_count == 0 ||
        floor_occupancy < 1 || floor_occupancy > passenger_count) {
      result.status = OriginalElevatorPassengerStepStatus::malformed_state;
      return result;
    }
    for (std::size_t slot = 0U; slot < capacity; ++slot) {
      if (signed_byte(car[184U + slot]) != floor) continue;
      const auto person_index = load_u32(
          car, 16U + slot * 4U, document.header.byte_swapped);
      if (person_index >= person_limit) {
        result.status = OriginalElevatorPassengerStepStatus::malformed_state;
        return result;
      }
      alighting.push_back(
          AlightingPassenger{slot, static_cast<std::size_t>(person_index)});
    }
    if (alighting.empty() ||
        alighting.size() > static_cast<std::size_t>(passenger_count) ||
        alighting.size() > static_cast<std::size_t>(floor_occupancy)) {
      result.status = OriginalElevatorPassengerStepStatus::malformed_state;
      return result;
    }
  }

  const auto mapped = original_elevator_floor_record_index(
      elevator.type, elevator.bottom_floor, elevator.top_floor, floor);
  auto* floor_record = find_original_elevator_floor_record(elevator, mapped);
  struct BoardingPlan {
    bool direction_up{};
    std::vector<std::size_t> people{};
    std::vector<OriginalElevatorBoardingDestination> destinations{};
  };
  std::array<BoardingPlan, 2> plans{};
  std::size_t plan_count = 0U;
  bool direction_up = signed_byte(car[4]) != 0;
  bool direction_changed = false;
  const auto distinct_after_alighting = static_cast<std::int16_t>(
      signed_byte(car[12]) -
      (!alighting.empty() &&
               static_cast<std::size_t>(std::to_integer<std::uint8_t>(
                   car[226U + static_cast<std::size_t>(floor)])) ==
                   alighting.size()
           ? 1
           : 0));
  std::int16_t projected_passengers = static_cast<std::int16_t>(
      passenger_count - static_cast<std::int16_t>(alighting.size()));

  const auto build_plan = [&](bool lane_direction,
                              std::size_t maximum) -> bool {
    if (!floor_record || maximum == 0U) return true;
    const std::size_t count_offset = lane_direction ? 0U : 2U;
    const std::size_t cursor_offset = lane_direction ? 1U : 3U;
    const std::size_t table_offset = lane_direction ? 4U : 164U;
    const auto count = signed_byte(floor_record->exact_bytes[count_offset]);
    const auto cursor = signed_byte(floor_record->exact_bytes[cursor_offset]);
    if (count < 0 || count > 40 || cursor < 0 || cursor >= 40) return false;
    const auto amount = std::min<std::size_t>(
        static_cast<std::size_t>(count), maximum);
    if (amount == 0U) return true;
    auto& plan = plans[plan_count++];
    plan.direction_up = lane_direction;
    for (std::size_t ordinal = 0U; ordinal < amount; ++ordinal) {
      const auto slot =
          (static_cast<std::size_t>(cursor) + ordinal) % 40U;
      const auto person_index = load_u32(
          floor_record->exact_bytes, table_offset + slot * 4U,
          document.header.byte_swapped);
      if (person_index >= person_limit) return false;
      plan.people.push_back(static_cast<std::size_t>(person_index));
      plan.destinations.push_back(
          select_original_elevator_boarding_destination(
              document, static_cast<std::size_t>(person_index),
              elevator_index, floor, lane_direction));
    }
    return true;
  };

  if ((std::to_integer<std::uint8_t>(car[2]) & 1U) != 0U && floor_record) {
    auto lane_count = signed_byte(
        floor_record->exact_bytes[direction_up ? 0U : 2U]);
    const auto opposite_count = signed_byte(
        floor_record->exact_bytes[direction_up ? 2U : 0U]);
    if (lane_count < 0 || lane_count > 40 || opposite_count < 0 ||
        opposite_count > 40) {
      result.status = OriginalElevatorPassengerStepStatus::malformed_state;
      return result;
    }
    if (lane_count == 0 &&
        load_u16(car, 10U, document.header.byte_swapped) == 0U &&
        distinct_after_alighting == 0 && opposite_count != 0) {
      direction_up = !direction_up;
      direction_changed = true;
      lane_count = opposite_count;
    }
    const auto free = static_cast<std::size_t>(
        static_cast<std::int16_t>(capacity) - projected_passengers);
    const auto first_limit = car[2] == std::byte{1}
                                 ? free
                                 : std::min<std::size_t>(free, 1U);
    if (!build_plan(direction_up, first_limit)) {
      result.status = OriginalElevatorPassengerStepStatus::malformed_state;
      return result;
    }
    if (plan_count != 0U) {
      const auto successes = static_cast<std::int16_t>(std::count_if(
          plans[0].destinations.begin(), plans[0].destinations.end(),
          [](const auto& destination) {
            return destination.status ==
                   OriginalElevatorBoardingDestinationStatus::selected;
          }));
      projected_passengers =
          static_cast<std::int16_t>(projected_passengers + successes);
    }

    // Nonzero car byte 14 is the original special-service mode which can
    // consume the opposite waiting lane in the same door cycle.
    if (car[14] != std::byte{0}) {
      const auto free_second = static_cast<std::size_t>(
          static_cast<std::int16_t>(capacity) - projected_passengers);
      const auto second_limit = car[2] == std::byte{1}
                                    ? free_second
                                    : std::min<std::size_t>(free_second, 1U);
      if (!build_plan(!direction_up, second_limit)) {
        result.status = OriginalElevatorPassengerStepStatus::malformed_state;
        return result;
      }
    }
  }

  std::size_t successful_boardings = 0U;
  for (std::size_t index = 0U; index < plan_count; ++index) {
    successful_boardings += static_cast<std::size_t>(std::count_if(
        plans[index].destinations.begin(), plans[index].destinations.end(),
        [](const auto& destination) {
          return destination.status ==
                 OriginalElevatorBoardingDestinationStatus::selected;
        }));
  }
  std::size_t free_slots = 0U;
  for (std::size_t slot = 0U; slot < capacity; ++slot) {
    if (signed_byte(car[184U + slot]) < 0) ++free_slots;
  }
  free_slots += alighting.size();
  if (successful_boardings > free_slots) {
    result.status = OriginalElevatorPassengerStepStatus::malformed_state;
    return result;
  }

  const bool alighting_direction = signed_byte(car[4]) != 0;
  std::optional<std::size_t> last_alighting_person{};
  for (const auto& passenger : alighting) {
    const auto popped = pop_original_elevator_car_passenger_slot(
        document, elevator_index, car_index, passenger.slot);
    if (!popped || *popped != passenger.person) {
      result.status = OriginalElevatorPassengerStepStatus::malformed_state;
      return result;
    }
    if (!isolation_active) {
      auto& person = document.people[passenger.person];
      if (original_car_arrival_dispatches_family(person.exact_bytes[4])) {
        if (original_car_arrival_sets_person_floor(person.exact_bytes[4])) {
          person.exact_bytes[7] = static_cast<std::byte>(floor);
        }
        auto dispatch = dispatch_original_person_family(
            document, passenger.person, part, rent_income,
            OriginalPersonFamilyDispatchSource::elevator_car_0883);
        // 1210:0883 invokes the family far call before decrementing this
        // passenger's car/floor aggregates or advancing to the next slot.
        if (family_dispatch_callback) family_dispatch_callback(dispatch);
        result.family_dispatches.push_back(std::move(dispatch));
      }
    }
    car[3] = static_cast<std::byte>(
        std::to_integer<std::uint8_t>(car[3]) - 1U);
    auto& occupancy = car[226U + static_cast<std::size_t>(floor)];
    occupancy = static_cast<std::byte>(
        std::to_integer<std::uint8_t>(occupancy) - 1U);
    last_alighting_person = passenger.person;
    ++result.alighted;
  }
  if (!alighting.empty() &&
      car[226U + static_cast<std::size_t>(floor)] == std::byte{0}) {
    car[12] = static_cast<std::byte>(
        std::to_integer<std::uint8_t>(car[12]) - 1U);
  }
  if (!isolation_active && last_alighting_person) {
    // 1210:0883 returns the final popped dword, so 10a8:022b retains only the
    // last right-side transfer visual even when the whole floor group exits.
    result.alighting_visual = OriginalElevatorPassengerVisualEvent{
        elevator_index, floor, false, alighting_direction,
        *last_alighting_person};
  }

  const auto apply_lobby_wait_discount = [&](std::uint16_t elapsed) noexcept {
    // Exact signed comparisons in 11d8:0423. A wrapped negative word is at
    // or below either threshold and therefore becomes zero; treating it as
    // an unsigned 0xffff-style duration creates a spurious huge wait.
    const auto signed_elapsed = std::bit_cast<std::int16_t>(elapsed);
    if (floor == 10 && document.header.lobby_height == 2U) {
      return signed_elapsed > 25
                 ? static_cast<std::uint16_t>(elapsed - 25U)
                 : static_cast<std::uint16_t>(0U);
    }
    if (floor == 10 && document.header.lobby_height == 3U) {
      return signed_elapsed > 50
                 ? static_cast<std::uint16_t>(elapsed - 50U)
                 : static_cast<std::uint16_t>(0U);
    }
    return elapsed;
  };

  if (direction_changed) {
    car[4] = static_cast<std::byte>(direction_up ? 1U : 0U);
  }
  const auto apply_boarding_metric = [&](std::size_t person_index) {
    // Exact 11d8:01f1 packed wait metric: preserve the upper six bits, add
    // frame_time-start to the low ten, apply the two/three-story Lobby
    // 25/50-tick discount through 11d8:0423, clamp to 300, and clear start.
    if (elevator.type == 2U) return;
    auto& exact = document.people[person_index].exact_bytes;
    const auto old = load_u16(exact, 12U, document.header.byte_swapped);
    const std::uint16_t elapsed = apply_lobby_wait_discount(
        static_cast<std::uint16_t>(
            (old & 0x03ffU) + document.header.frame_time -
            load_u16(exact, 10U, document.header.byte_swapped)));
    const auto bounded = std::bit_cast<std::int16_t>(elapsed) >= 300
                             ? 300U
                             : elapsed;
    store_u16(exact, 12U,
              static_cast<std::uint16_t>((old & 0xfc00U) + bounded),
              document.header.byte_swapped);
    store_u16(exact, 10U, 0U, document.header.byte_swapped);
  };
  const auto pop_waiting_head = [&](bool lane_direction)
      -> std::optional<std::size_t> {
    const std::size_t count_offset = lane_direction ? 0U : 2U;
    const std::size_t cursor_offset = lane_direction ? 1U : 3U;
    const std::size_t table_offset = lane_direction ? 4U : 164U;
    const auto count = std::to_integer<std::uint8_t>(
        floor_record->exact_bytes[count_offset]);
    const auto cursor = std::to_integer<std::uint8_t>(
        floor_record->exact_bytes[cursor_offset]);
    if (count == 0U || count > 40U || cursor >= 40U) return std::nullopt;
    const auto person_index = load_u32(
        floor_record->exact_bytes,
        table_offset + static_cast<std::size_t>(cursor) * 4U,
        document.header.byte_swapped);
    if (isolation_active && person_index < person_limit) {
      // With DS:b3ae set, 1210:1332 leaves the person record untouched and
      // replaces the consumed queue dword with its projected wait metric.
      // 10f0:0318 later restores only the queue counters/cursors, deliberately
      // retaining these synthetic future-user selectors during Simulate.
      const auto& person = document.people[person_index].exact_bytes;
      const auto old = static_cast<std::uint16_t>(
          load_u16(person, 12U, document.header.byte_swapped) & 0x03ffU);
      const auto elapsed = apply_lobby_wait_discount(
          static_cast<std::uint16_t>(
              document.header.frame_time -
              load_u16(person, 10U, document.header.byte_swapped)));
      const auto projected = static_cast<std::int16_t>(
          static_cast<std::uint16_t>(old + elapsed));
      store_u32(floor_record->exact_bytes,
                table_offset + static_cast<std::size_t>(cursor) * 4U,
                static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(projected)),
                document.header.byte_swapped);
    }
    floor_record->exact_bytes[cursor_offset] =
        static_cast<std::byte>((cursor + 1U) % 40U);
    floor_record->exact_bytes[count_offset] =
        static_cast<std::byte>(count - 1U);
    return static_cast<std::size_t>(person_index);
  };
  const auto insert_passenger = [&](std::size_t person_index,
                                    std::int16_t destination) {
    // Exact 1210:1a3b first-free car-slot insertion: destination byte at
    // +184 and the passenger dword at +16 for the same capacity-bounded slot.
    for (std::size_t slot = 0U; slot < capacity; ++slot) {
      if (signed_byte(car[184U + slot]) >= 0) continue;
      car[184U + slot] = static_cast<std::byte>(destination);
      store_u32(car, 16U + slot * 4U,
                static_cast<std::uint32_t>(person_index),
                document.header.byte_swapped);
      auto& occupancy = car[226U + static_cast<std::size_t>(destination)];
      if (occupancy == std::byte{0}) {
        car[12] = static_cast<std::byte>(
            std::to_integer<std::uint8_t>(car[12]) + 1U);
      }
      occupancy = static_cast<std::byte>(
          std::to_integer<std::uint8_t>(occupancy) + 1U);
      return;
    }
  };

  for (std::size_t plan_index = 0U; plan_index < plan_count; ++plan_index) {
    auto& plan = plans[plan_index];
    std::size_t boarded_in_plan = 0U;
    for (std::size_t ordinal = 0U; ordinal < plan.people.size(); ++ordinal) {
      const auto popped = pop_waiting_head(plan.direction_up);
      if (!popped || *popped != plan.people[ordinal]) {
        result.status = OriginalElevatorPassengerStepStatus::malformed_state;
        return result;
      }
      if (!isolation_active) apply_boarding_metric(*popped);
      const auto& destination = plan.destinations[ordinal];
      if (destination.status ==
          OriginalElevatorBoardingDestinationStatus::selected) {
        insert_passenger(*popped, destination.car_destination);
        ++boarded_in_plan;
        ++result.boarded;
      } else {
        if (!isolation_active) {
          if (elevator.type != 2U) {
            add_original_person_waiting_delay(
                document.people[*popped], part.words_00_to_40[2U],
                document.header.byte_swapped);
          }
          auto dispatch = dispatch_original_person_family(
              document, *popped, part, rent_income,
              OriginalPersonFamilyDispatchSource::dispatcher_16ab);
          // 1210:0351 -> 1332 dispatches a rejected waiting person at this
          // exact ring position before the remaining boarding loop continues.
          if (family_dispatch_callback) family_dispatch_callback(dispatch);
          result.family_dispatches.push_back(std::move(dispatch));
        }
        ++result.rejected;
      }
      if (!isolation_active) {
        result.boarding_visual = OriginalElevatorPassengerVisualEvent{
            elevator_index, floor, true, plan.direction_up, *popped};
      }
    }
    car[3] = static_cast<std::byte>(
        std::to_integer<std::uint8_t>(car[3]) + boarded_in_plan);
  }

  if (result.boarded != 0U || result.rejected != 0U ||
      result.alighted != 0U) {
    result.status = OriginalElevatorPassengerStepStatus::transferred;
  } else {
    result.status = OriginalElevatorPassengerStepStatus::no_transfer;
  }
  return result;
}

OriginalElevatorCarStepResult step_original_elevator_car_state(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index,
    std::function<void()> movement_sound_callback) {
  OriginalElevatorCarStepResult result{};
  if (elevator_index >= document.elevators.size() ||
      car_index >= document.elevators[elevator_index].car_records.size()) {
    return result;
  }
  auto& elevator = document.elevators[elevator_index];
  auto& car = elevator.car_records[car_index].exact_bytes;
  if (elevator.used == 0U || car[15] == std::byte{0}) {
    result.status = OriginalElevatorCarStepStatus::inactive_car;
    return result;
  }

  const auto current = signed_byte(car[0]);
  const auto target = signed_byte(car[5]);
  const auto direction = signed_byte(car[4]);
  const auto passengers = signed_byte(car[3]);
  const auto door_state = signed_byte(car[2]);
  const auto capacity = static_cast<std::int16_t>(
      std::min<std::size_t>(elevator.capacity, 42U));
  result.floor_before = current;
  result.floor_after = current;
  if (!original_route_floor_valid(current) ||
      current < elevator.bottom_floor || current > elevator.top_floor ||
      !original_route_floor_valid(target) ||
      target < elevator.bottom_floor || target > elevator.top_floor ||
      (direction != 0 && direction != 1) || passengers < 0 ||
      passengers > capacity || door_state < 0 || door_state > 5) {
    result.status = OriginalElevatorCarStepStatus::malformed_state;
    return result;
  }

  const auto motion_class = [&]() -> std::uint8_t {
    // Exact 1090:209f motion classification over the absolute target and
    // previous-floor distances, with distinct Express versus other cutoffs.
    const auto previous = signed_byte(car[6]);
    const auto target_distance = static_cast<std::int16_t>(
        std::abs(static_cast<int>(current) - static_cast<int>(target)));
    const auto previous_distance = static_cast<std::int16_t>(
        std::abs(static_cast<int>(current) - static_cast<int>(previous)));
    if (target_distance <= 1 || previous_distance <= 1) return 0U;
    if (elevator.type == 0U) {
      return target_distance > 4 && previous_distance > 4 ? 3U : 2U;
    }
    return target_distance > 3 && previous_distance > 3 ? 2U : 1U;
  };

  // 1090:0740-078d: while the near-floor settle counter is live, only class
  // zero decrements it. Faster geometry cancels it immediately.
  if (car[1] != std::byte{0}) {
    result.motion_class = motion_class();
    if (result.motion_class == 0U) {
      car[1] = static_cast<std::byte>(
          std::to_integer<std::uint8_t>(car[1]) - 1U);
    } else {
      car[1] = std::byte{0};
    }
    result.status = OriginalElevatorCarStepStatus::countdown_advanced;
    result.changed = true;
    return result;
  }

  const auto calendar_phase =
      original_calendar_phase(document.header.current_day);
  const auto day_phase = original_day_phase(document.header.frame_time);
  if (calendar_phase >= 2U || day_phase < 0 || day_phase >= 7) {
    result.status = OriginalElevatorCarStepStatus::malformed_state;
    return result;
  }
  const auto schedule_offset =
      static_cast<std::size_t>(calendar_phase) * 7U +
      static_cast<std::size_t>(day_phase);

  const auto should_depart = [&]() noexcept {
    // Exact 1090:23a5 departure predicate: full car, zero dwell, ordinary
    // non-endpoint floor, or an elapsed endpoint dwell greater than dwell*30.
    if (passengers == capacity) return true;
    const auto dwell = signed_byte(elevator.schedule[42U + schedule_offset]);
    if (dwell == 0) return true;
    const auto home = signed_byte(elevator.car_home_floors[car_index]);
    const bool lobby_floor =
        current >= 10 &&
        current < static_cast<std::int16_t>(10 + document.header.lobby_height);
    const bool express_floor =
        current > 10 && ((static_cast<int>(current) - 9) % 15) == 0;
    if (current != home && !lobby_floor && !express_floor) return true;
    // 2461-2494 keeps the CWD/XOR/SUB absolute value in AX and performs a
    // signed JLE. In particular, abs(0x8000) wraps back to signed -32768.
    const auto elapsed = original_wrapped_absolute_difference(
        load_u16(car, 8U, document.header.byte_swapped),
        document.header.frame_time);
    return elapsed > static_cast<std::int16_t>(dwell * 30);
  };

  // 1090:07a9-083b: door state is itself the five-frame countdown. At zero,
  // remember this stop, recompute work, then either depart or reopen at one.
  if (car[2] != std::byte{0}) {
    car[2] = static_cast<std::byte>(
        std::to_integer<std::uint8_t>(car[2]) - 1U);
    if (car[2] == std::byte{0}) {
      car[6] = car[0];
      recompute_original_elevator_car(
          elevator, car_index, document.header.byte_swapped);
      if (!should_depart()) car[2] = std::byte{1};
    }
    result.status = OriginalElevatorCarStepStatus::door_advanced;
    result.changed = true;
    return result;
  }

  const auto occupancy = std::to_integer<std::uint8_t>(
      car[226U + static_cast<std::size_t>(current)]);
  // A full car with nobody alighting skips the stop. Every other at-target
  // case opens to state five, even when no waiting queue is currently owned.
  if (target == current && (occupancy != 0U || passengers != capacity)) {
    if (current == elevator.bottom_floor || current == elevator.top_floor) {
      car[14] = elevator.schedule[28U + schedule_offset];
    }
    clear_original_elevator_floor_assignments(
        elevator, car_index, current, document.header.byte_swapped, 0U);
    car[2] = std::byte{5};
    if (car[7] == std::byte{0}) {
      store_u16(car, 8U, document.header.frame_time,
                document.header.byte_swapped);
    }
    car[7] = std::byte{1};
    result.status = OriginalElevatorCarStepStatus::doors_opened;
    result.changed = true;
    return result;
  }

  const auto mapped = original_elevator_floor_record_index(
      elevator.type, elevator.bottom_floor, elevator.top_floor, current);
  auto* floor_record = find_original_elevator_floor_record(elevator, mapped);
  bool unassigned_up = false;
  bool unassigned_down = false;
  if (floor_record) {
    const auto up_count = signed_byte(floor_record->exact_bytes[0]);
    const auto down_count = signed_byte(floor_record->exact_bytes[2]);
    if (up_count < 0 || up_count > 40 || down_count < 0 ||
        down_count > 40) {
      result.status = OriginalElevatorCarStepStatus::malformed_state;
      return result;
    }
    unassigned_up =
        up_count != 0 &&
        elevator.block_2a2[static_cast<std::size_t>(current)] ==
            std::byte{0};
    unassigned_down =
        down_count != 0 &&
        elevator.block_31a[static_cast<std::size_t>(current)] ==
            std::byte{0};
  }

  // 1090:12c9 differs from 13cc: departure releases only this car's own
  // one-based owner entry and decrements its word-10 count directly.
  const auto release_owned = [&](std::array<std::byte, 120>& owners) {
    auto& owner = owners[static_cast<std::size_t>(current)];
    if (owner != static_cast<std::byte>(car_index + 1U)) return;
    owner = std::byte{0};
    store_u16(car, 10U,
              static_cast<std::uint16_t>(
                  load_u16(car, 10U, document.header.byte_swapped) - 1U),
              document.header.byte_swapped);
  };
  if (car[14] != std::byte{0} || direction != 0) {
    release_owned(elevator.block_2a2);
  }
  if (car[14] != std::byte{0} || direction == 0) {
    release_owned(elevator.block_31a);
  }

  // Exact 1090:10e4 motion helper.
  if (signed_byte(car[0]) == signed_byte(car[5])) {
    car[6] = car[0];
    recompute_original_elevator_car(
        elevator, car_index, document.header.byte_swapped);
  }
  const auto motion_current = signed_byte(car[0]);
  const auto motion_target = signed_byte(car[5]);
  const auto motion_previous = signed_byte(car[6]);
  const auto target_distance = static_cast<std::int16_t>(
      std::abs(static_cast<int>(motion_current) -
               static_cast<int>(motion_target)));
  const auto previous_distance = static_cast<std::int16_t>(
      std::abs(static_cast<int>(motion_current) -
               static_cast<int>(motion_previous)));
  if (target_distance <= 1 || previous_distance <= 1) {
    result.motion_class = 0U;
  } else if (elevator.type == 0U) {
    result.motion_class =
        target_distance > 4 && previous_distance > 4 ? 3U : 2U;
  } else {
    result.motion_class =
        target_distance > 3 && previous_distance > 3 ? 2U : 1U;
  }
  if (result.motion_class == 0U) car[1] = std::byte{5};
  if (result.motion_class == 1U) car[1] = std::byte{2};
  const auto delta = static_cast<std::int16_t>(
      result.motion_class == 3U ? 3 : 1);
  car[0] = static_cast<std::byte>(static_cast<std::int16_t>(
      motion_current + (signed_byte(car[4]) != 0 ? delta : -delta)));
  if (car[7] != std::byte{0}) {
    result.movement_sound_requested = true;
    // 1090:10e4 calls 11c8:0167 while byte 7 is still nonzero, before
    // clearing it and before 06fb performs either waiting-lane assignment.
    if (movement_sound_callback) movement_sound_callback();
    car[7] = std::byte{0};
  }
  result.floor_after = signed_byte(car[0]);

  if (unassigned_up && assign_original_elevator_waiting_floor(
                               document, elevator_index, current, true,
                               calendar_phase, day_phase)) {
    ++result.assignments_created;
  }
  if (unassigned_down && assign_original_elevator_waiting_floor(
                                 document, elevator_index, current, false,
                                 calendar_phase, day_phase)) {
    ++result.assignments_created;
  }
  result.status = OriginalElevatorCarStepStatus::moved;
  result.changed = true;
  return result;
}

OriginalElevatorFrameStepResult step_original_elevator_frame(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income,
    bool isolation_active,
    OriginalElevatorFrameHostHooks host_hooks) {
  OriginalElevatorFrameStepResult result{};
  const auto retain_transfer_visual = [&result](const auto& event) {
    if (!event) return;
    const auto same_cache_slot = [&](const auto& retained) {
      return retained.elevator_index == event->elevator_index &&
             retained.floor == event->floor &&
             retained.boarding == event->boarding;
    };
    const auto retained = std::find_if(result.transfer_visuals.begin(),
                                       result.transfer_visuals.end(),
                                       same_cache_slot);
    if (retained == result.transfer_visuals.end()) {
      result.transfer_visuals.push_back(*event);
    } else {
      *retained = *event;
    }
  };
  // 1090:04c0 owns the outer Elevator loop. Within each used shaft,
  // 04cb-04fb advances all eight cars before 04fd-053b performs that same
  // shaft's 07a6/0351 passenger pairs. Only then does 053d pump and 0542 move
  // to the next Elevator. The earlier native all-shafts-motion followed by
  // all-shafts-passengers split changed cross-shaft observable state.
  for (std::size_t elevator_index = 0U;
       elevator_index < document.elevators.size(); ++elevator_index) {
    auto& elevator = document.elevators[elevator_index];
    if (elevator.used == 0U) continue;
    ++result.elevators_scanned;
    for (std::size_t car_index = 0U;
         car_index < elevator.car_records.size(); ++car_index) {
      if (elevator.car_records[car_index].exact_bytes[15] == std::byte{0}) {
        continue;
      }
      ++result.cars_scanned;
      const auto step = step_original_elevator_car_state(
          document, elevator_index, car_index, host_hooks.movement_sound);
      if (step.changed) {
        ++result.cars_changed;
        result.changed = true;
      }
      if (step.movement_sound_requested) {
        ++result.movement_sound_requests;
      }
    }
    for (std::size_t car_index = 0U;
         car_index < elevator.car_records.size(); ++car_index) {
      if (elevator.car_records[car_index].exact_bytes[15] == std::byte{0}) {
        continue;
      }
      auto passenger = step_original_elevator_car_passengers(
          document, elevator_index, car_index, part, rent_income,
          isolation_active, host_hooks.family_dispatch);
      result.boarded += passenger.boarded;
      result.rejected += passenger.rejected;
      result.alighted += passenger.alighted;
      // 10a8:022b maps the event through 10a8:0000's per-floor reverse
      // Elevator index. Its two dword banks are separate for boarding and
      // alighting, so unrelated floors/Elevators coexist until 02aa paints.
      retain_transfer_visual(passenger.boarding_visual);
      retain_transfer_visual(passenger.alighting_visual);
      if (passenger.boarded != 0U || passenger.rejected != 0U ||
          passenger.alighted != 0U) {
        result.changed = true;
      }
      result.family_dispatches.insert(
          result.family_dispatches.end(),
          std::make_move_iterator(passenger.family_dispatches.begin()),
          std::make_move_iterator(passenger.family_dispatches.end()));
    }
    // 1090:053d invokes 11e0:0e84 once after both inner loops, but only for a
    // used Elevator. Keep the callback after all model-visible mutations for
    // this shaft and before the next shaft's first car-state pass.
    if (host_hooks.elevator_checkpoint) host_hooks.elevator_checkpoint();
  }
  // 10a8:02aa walks rows top-to-bottom, each row's complete 10a8:0000
  // diminishing-gap Shell-sorted Elevator list, and its boarding bank before
  // its alighting bank. Sort the process-only result through those exact
  // complete row lists: non-event shafts can affect equal-x Shell ordering.
  auto retained_visuals = std::move(result.transfer_visuals);
  result.transfer_visuals.clear();
  result.transfer_visuals.reserve(retained_visuals.size());
  for (int floor = 119; floor >= 0; --floor) {
    std::array<std::size_t, 24> shafts{};
    std::size_t shaft_count = 0U;
    for (std::size_t elevator_index = 0U;
         elevator_index < document.elevators.size(); ++elevator_index) {
      const auto& elevator = document.elevators[elevator_index];
      if (elevator.used == 0U ||
          floor < static_cast<int>(elevator.bottom_floor) - 1 ||
          floor > static_cast<int>(elevator.top_floor) + 1) {
        continue;
      }
      shafts[shaft_count++] = elevator_index;
    }
    for (int gap = static_cast<int>(shaft_count);;) {
      gap /= 2;
      if (gap <= 0) break;
      for (int end = gap; end < static_cast<int>(shaft_count); ++end) {
        for (int left = end - gap; left >= 0; left -= gap) {
          const auto right = static_cast<std::size_t>(left + gap);
          if (document.elevators[shafts[static_cast<std::size_t>(left)]].x <=
              document.elevators[shafts[right]].x) {
            break;
          }
          std::swap(shafts[static_cast<std::size_t>(left)], shafts[right]);
        }
      }
    }
    for (std::size_t slot = 0U; slot < shaft_count; ++slot) {
      for (const bool boarding : {true, false}) {
        const auto visual = std::find_if(
            retained_visuals.begin(), retained_visuals.end(),
            [&](const auto& candidate) {
              return candidate.floor == floor &&
                     candidate.elevator_index == shafts[slot] &&
                     candidate.boarding == boarding;
            });
        if (visual != retained_visuals.end()) {
          result.transfer_visuals.push_back(*visual);
        }
      }
    }
  }
  return result;
}

OriginalElevatorAssignmentSelection select_original_elevator_assignment_car(
    const OriginalTdtElevator& elevator,
    std::int16_t floor,
    bool direction_up,
    std::uint8_t calendar_phase,
    std::int8_t day_phase) noexcept {
  if (calendar_phase >= 2U || day_phase < 0 || day_phase >= 7) {
    return {};
  }

  constexpr std::int16_t kNoDistance = 9999;
  std::int16_t best_same_direction = kNoDistance;
  std::int16_t best_idle = kNoDistance;
  std::int16_t best_wrap = kNoDistance;
  std::uint8_t same_direction_car = 0U;
  std::uint8_t idle_car = 0U;
  std::uint8_t wrap_car = 0U;
  const std::int16_t requested_direction = direction_up ? 1 : 0;

  for (std::size_t car_index = 0;
       car_index < elevator.car_records.size(); ++car_index) {
    const auto& exact = elevator.car_records[car_index].exact_bytes;
    if (exact[15] == std::byte{0}) continue;

    const auto current = signed_byte(exact[0]);
    const auto direction = signed_byte(exact[4]);
    if (current == floor && exact[1] == std::byte{0} &&
        (exact[14] != std::byte{0} || direction == requested_direction)) {
      return {OriginalElevatorAssignmentSelectionStatus::immediate_service,
              static_cast<std::uint8_t>(car_index)};
    }

    if (exact[10] == std::byte{0} && exact[11] == std::byte{0} &&
        exact[12] == std::byte{0} &&
        signed_byte(elevator.car_home_floors[car_index]) == current &&
        exact[1] == std::byte{0}) {
      const auto distance = static_cast<std::int16_t>(
          floor >= current ? floor - current : current - floor);
      if (distance == 0) {
        return {OriginalElevatorAssignmentSelectionStatus::immediate_service,
                static_cast<std::uint8_t>(car_index)};
      }
      if (distance < best_idle) {
        best_idle = distance;
        idle_car = static_cast<std::uint8_t>(car_index);
      }
      continue;
    }

    if (direction == requested_direction) {
      const auto distance = static_cast<std::int16_t>(
          direction_up ? floor - current : current - floor);
      if (distance == 0 && exact[1] == std::byte{0}) {
        return {OriginalElevatorAssignmentSelectionStatus::immediate_service,
                static_cast<std::uint8_t>(car_index)};
      }
      if (distance >= 0) {
        if (distance < best_same_direction) {
          best_same_direction = distance;
          same_direction_car = static_cast<std::uint8_t>(car_index);
        }
      } else {
        const auto pivot = signed_byte(exact[13]);
        const auto wrap_distance = static_cast<std::int16_t>(
            direction_up ? pivot - current + pivot - floor
                         : current - pivot + floor - pivot);
        if (wrap_distance < best_wrap) {
          best_wrap = wrap_distance;
          wrap_car = static_cast<std::uint8_t>(car_index);
        }
      }
      continue;
    }

    const auto pivot = signed_byte(exact[13]);
    std::int16_t wrap_distance{};
    if (direction_up) {
      wrap_distance = static_cast<std::int16_t>(
          floor > pivot ? current - pivot + floor - pivot
                        : current - floor);
    } else {
      wrap_distance = static_cast<std::int16_t>(
          floor < pivot ? pivot - current + pivot - floor
                        : floor - current);
    }
    if (wrap_distance < best_wrap) {
      best_wrap = wrap_distance;
      wrap_car = static_cast<std::uint8_t>(car_index);
    }
  }

  const auto schedule_index =
      14U + static_cast<std::size_t>(calendar_phase) * 7U +
      static_cast<std::size_t>(day_phase);
  const auto threshold = signed_byte(elevator.schedule[schedule_index]);
  if (best_same_direction != kNoDistance) {
    const auto car = threshold >
                             static_cast<std::int16_t>(best_same_direction -
                                                       best_idle)
                         ? same_direction_car
                         : idle_car;
    return {OriginalElevatorAssignmentSelectionStatus::assign_car, car};
  }
  if (best_wrap != kNoDistance) {
    const auto car = threshold >
                             static_cast<std::int16_t>(best_wrap - best_idle)
                         ? wrap_car
                         : idle_car;
    return {OriginalElevatorAssignmentSelectionStatus::assign_car, car};
  }
  // 1090:10d0 writes literal zero when the valid-state search found only
  // idle/no active candidates; preserve that unusual original fallback.
  return {OriginalElevatorAssignmentSelectionStatus::assign_car, 0U};
}

bool assign_original_elevator_waiting_floor(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    bool direction_up,
    std::uint8_t calendar_phase,
    std::int8_t day_phase) noexcept {
  if (elevator_index >= document.elevators.size() ||
      !original_floor_in_elevator_range(floor)) {
    return false;
  }
  auto& elevator = document.elevators[elevator_index];
  auto& assignments = direction_up ? elevator.block_2a2
                                   : elevator.block_31a;
  auto& owner = assignments[static_cast<std::size_t>(floor)];
  if (owner != std::byte{0}) return false;

  const auto selection = select_original_elevator_assignment_car(
      elevator, floor, direction_up, calendar_phase, day_phase);
  if (selection.status !=
          OriginalElevatorAssignmentSelectionStatus::assign_car ||
      selection.car_index >= elevator.car_records.size()) {
    return false;
  }

  owner = static_cast<std::byte>(selection.car_index + 1U);
  auto& car = elevator.car_records[selection.car_index].exact_bytes;
  store_u16(car, 10U,
            static_cast<std::uint16_t>(
                load_u16(car, 10U, document.header.byte_swapped) + 1U),
            document.header.byte_swapped);
  recompute_original_elevator_car(
      elevator, selection.car_index, document.header.byte_swapped);
  return true;
}

void recompute_original_elevator_car_state(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index) noexcept {
  if (elevator_index >= document.elevators.size()) return;
  recompute_original_elevator_car(
      document.elevators[elevator_index], car_index,
      document.header.byte_swapped);
}

OriginalElevatorFloorPeopleCleanupResult
cleanup_original_elevator_waiting_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    std::uint16_t waiting_delay,
    OriginalPersonFamilyDispatch dispatch,
    void* context) noexcept {
  OriginalElevatorFloorPeopleCleanupResult result{};
  if (elevator_index >= document.elevators.size() || floor < 0 ||
      floor >= 120) {
    return result;
  }
  auto& elevator = document.elevators[elevator_index];
  if (elevator.used == 0U) return result;
  const auto person_limit = std::min<std::size_t>(
      document.people_count, document.people.size());
  const auto mapped = original_elevator_floor_record_index(
      elevator.type, elevator.bottom_floor, elevator.top_floor, floor);
  auto floor_record = elevator.floor_records.end();
  if (mapped >= 0) {
    floor_record = std::find_if(
        elevator.floor_records.begin(), elevator.floor_records.end(),
        [&](const OriginalTdtElevatorFloorRecord& record) {
          return record.mapped_index == mapped;
        });
  }
  if (floor_record == elevator.floor_records.end()) {
    result.status = OriginalElevatorFloorPeopleCleanupStatus::cleaned;
    return result;
  }

  // Preflight both rings before changing either one. The executable assumes
  // loaded indices are valid; the native port rejects malformed saves while
  // retaining atomic mutation at this public boundary.
  const auto& source = floor_record->exact_bytes;
  bool has_people = false;
  for (const auto [count_offset, cursor_offset, table_offset] :
       {std::array<std::size_t, 3>{0U, 1U, 4U},
        std::array<std::size_t, 3>{2U, 3U, 164U}}) {
    const auto count = signed_byte(source[count_offset]);
    if (count <= 0) continue;
    has_people = true;
    const auto cursor = std::to_integer<std::uint8_t>(source[cursor_offset]);
    if (cursor >= 40U) return result;
    for (std::size_t ordinal = 0U;
         ordinal < static_cast<std::size_t>(count); ++ordinal) {
      const auto slot = (cursor + ordinal) % 40U;
      if (load_u32(source, table_offset + slot * 4U,
                   document.header.byte_swapped) >= person_limit) {
        return result;
      }
    }
  }
  if (has_people && dispatch == nullptr) {
    result.status =
        OriginalElevatorFloorPeopleCleanupStatus::dispatch_required;
    return result;
  }

  // 10a0:1625 drains the complete up ring before the complete down ring.
  // Slots remain stale exactly as in the Win16 data; only cursor/count move.
  auto& exact = floor_record->exact_bytes;
  for (const auto [count_offset, cursor_offset, table_offset] :
       {std::array<std::size_t, 3>{0U, 1U, 4U},
        std::array<std::size_t, 3>{2U, 3U, 164U}}) {
    while (signed_byte(exact[count_offset]) > 0) {
      const auto cursor = std::to_integer<std::uint8_t>(exact[cursor_offset]);
      const auto person_index = load_u32(
          exact, table_offset + cursor * 4U,
          document.header.byte_swapped);
      exact[cursor_offset] =
          static_cast<std::byte>((cursor + 1U) % 40U);
      if (elevator.type != 2U) {
        add_original_person_waiting_delay(
            document.people[static_cast<std::size_t>(person_index)],
            waiting_delay, document.header.byte_swapped);
      }
      dispatch(document, static_cast<std::size_t>(person_index),
               OriginalPersonFamilyDispatchSource::dispatcher_16ab,
               context);
      exact[count_offset] = static_cast<std::byte>(
          std::to_integer<std::uint8_t>(exact[count_offset]) - 1U);
      ++result.waiting_passengers;
    }
  }
  result.status = OriginalElevatorFloorPeopleCleanupStatus::cleaned;
  return result;
}

OriginalElevatorFloorPeopleCleanupResult
cleanup_original_elevator_car_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    OriginalPersonFamilyDispatch dispatch,
    void* context,
    bool suppress_car_family_dispatch) noexcept {
  OriginalElevatorFloorPeopleCleanupResult result{};
  if (elevator_index >= document.elevators.size() || floor < 0 ||
      floor >= 120) {
    return result;
  }
  auto& elevator = document.elevators[elevator_index];
  if (elevator.used == 0U) return result;
  const auto floor_index = static_cast<std::size_t>(floor);
  const auto capacity = std::min<std::size_t>(elevator.capacity, 42U);
  const auto person_limit = std::min<std::size_t>(
      document.people_count, document.people.size());
  bool has_people = false;
  for (const auto& car : elevator.car_records) {
    if (car.exact_bytes[15] == std::byte{0} ||
        car.exact_bytes[226U + floor_index] == std::byte{0}) {
      continue;
    }
    for (std::size_t slot = 0U; slot < capacity; ++slot) {
      if (signed_byte(car.exact_bytes[184U + slot]) != floor) continue;
      has_people = true;
      if (load_u32(car.exact_bytes, 16U + slot * 4U,
                   document.header.byte_swapped) >= person_limit) {
        return result;
      }
    }
  }
  if (has_people && dispatch == nullptr) {
    result.status =
        OriginalElevatorFloorPeopleCleanupStatus::dispatch_required;
    return result;
  }

  // 10a0:14fa walks all eight 154a car records, dispatching arrivals before
  // decrementing their car/occupancy counts, then releases both owners.
  for (std::size_t car_index = 0U;
       car_index < elevator.car_records.size(); ++car_index) {
    auto& car = elevator.car_records[car_index].exact_bytes;
    if (car[15] == std::byte{0}) continue;
    auto& occupancy = car[226U + floor_index];
    if (occupancy != std::byte{0}) {
      for (std::size_t slot = 0U; slot < capacity; ++slot) {
        if (signed_byte(car[184U + slot]) != floor) continue;
        const auto popped = pop_original_elevator_car_passenger_slot(
            document, elevator_index, car_index, slot);
        if (!popped || *popped >= person_limit) return result;
        auto& person = document.people[static_cast<std::size_t>(*popped)];
        if (!suppress_car_family_dispatch &&
            original_car_arrival_dispatches_family(person.exact_bytes[4])) {
          if (original_car_arrival_sets_person_floor(person.exact_bytes[4])) {
            person.exact_bytes[7] = static_cast<std::byte>(floor);
          }
          dispatch(document, static_cast<std::size_t>(*popped),
                   OriginalPersonFamilyDispatchSource::elevator_car_0883,
                   context);
        }
        car[3] = static_cast<std::byte>(
            std::to_integer<std::uint8_t>(car[3]) - 1U);
        occupancy = static_cast<std::byte>(
            std::to_integer<std::uint8_t>(occupancy) - 1U);
        ++result.car_passengers;
      }
      if (occupancy == std::byte{0}) {
        car[12] = static_cast<std::byte>(
            std::to_integer<std::uint8_t>(car[12]) - 1U);
      }
    }

    const auto owner = static_cast<std::byte>(car_index + 1U);
    if (elevator.block_2a2[floor_index] == owner) {
      elevator.block_2a2[floor_index] = std::byte{0};
      store_u16(car, 10U,
                static_cast<std::uint16_t>(
                    load_u16(car, 10U, document.header.byte_swapped) - 1U),
                document.header.byte_swapped);
    }
    if (elevator.block_31a[floor_index] == owner) {
      elevator.block_31a[floor_index] = std::byte{0};
      store_u16(car, 10U,
                static_cast<std::uint16_t>(
                    load_u16(car, 10U, document.header.byte_swapped) - 1U),
                document.header.byte_swapped);
    }
    recompute_original_elevator_car(
        elevator, car_index, document.header.byte_swapped);
  }
  result.status = OriginalElevatorFloorPeopleCleanupStatus::cleaned;
  return result;
}

OriginalElevatorFloorPeopleCleanupResult
cleanup_original_elevator_selected_car_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index,
    std::int16_t floor,
    OriginalPersonFamilyDispatch dispatch,
    void* context,
    bool suppress_car_family_dispatch) noexcept {
  OriginalElevatorFloorPeopleCleanupResult result{};
  if (elevator_index >= document.elevators.size() || floor < 0 ||
      floor >= 120) {
    return result;
  }
  auto& elevator = document.elevators[elevator_index];
  if (elevator.used == 0U || car_index >= elevator.car_records.size()) {
    return result;
  }
  auto& car = elevator.car_records[car_index].exact_bytes;
  if (car[15] == std::byte{0}) return result;
  const auto floor_index = static_cast<std::size_t>(floor);
  const auto capacity = std::min<std::size_t>(elevator.capacity, 42U);
  const auto person_limit = std::min<std::size_t>(
      document.people_count, document.people.size());
  bool has_people = false;
  if (car[226U + floor_index] != std::byte{0}) {
    for (std::size_t slot = 0U; slot < capacity; ++slot) {
      if (signed_byte(car[184U + slot]) != floor) continue;
      has_people = true;
      if (load_u32(car, 16U + slot * 4U,
                   document.header.byte_swapped) >= person_limit) {
        return result;
      }
    }
  }
  if (has_people && dispatch == nullptr) {
    result.status =
        OriginalElevatorFloorPeopleCleanupStatus::dispatch_required;
    return result;
  }

  auto& occupancy = car[226U + floor_index];
  if (occupancy != std::byte{0}) {
    for (std::size_t slot = 0U; slot < capacity; ++slot) {
      if (signed_byte(car[184U + slot]) != floor) continue;
      const auto popped = pop_original_elevator_car_passenger_slot(
          document, elevator_index, car_index, slot);
      if (!popped || *popped >= person_limit) return result;
      auto& person = document.people[static_cast<std::size_t>(*popped)];
      if (!suppress_car_family_dispatch &&
          original_car_arrival_dispatches_family(person.exact_bytes[4])) {
        if (original_car_arrival_sets_person_floor(person.exact_bytes[4])) {
          person.exact_bytes[7] = static_cast<std::byte>(floor);
        }
        dispatch(document, static_cast<std::size_t>(*popped),
                 OriginalPersonFamilyDispatchSource::elevator_car_0883,
                 context);
      }
      car[3] = static_cast<std::byte>(
          std::to_integer<std::uint8_t>(car[3]) - 1U);
      occupancy = static_cast<std::byte>(
          std::to_integer<std::uint8_t>(occupancy) - 1U);
      ++result.car_passengers;
    }
    if (occupancy == std::byte{0}) {
      car[12] = static_cast<std::byte>(
          std::to_integer<std::uint8_t>(car[12]) - 1U);
    }
  }

  const auto owner = static_cast<std::byte>(car_index + 1U);
  if (elevator.block_2a2[floor_index] == owner) {
    elevator.block_2a2[floor_index] = std::byte{0};
    store_u16(car, 10U,
              static_cast<std::uint16_t>(
                  load_u16(car, 10U, document.header.byte_swapped) - 1U),
              document.header.byte_swapped);
  }
  if (elevator.block_31a[floor_index] == owner) {
    elevator.block_31a[floor_index] = std::byte{0};
    store_u16(car, 10U,
              static_cast<std::uint16_t>(
                  load_u16(car, 10U, document.header.byte_swapped) - 1U),
              document.header.byte_swapped);
  }
  recompute_original_elevator_car(
      elevator, car_index, document.header.byte_swapped);
  result.status = OriginalElevatorFloorPeopleCleanupStatus::cleaned;
  return result;
}

OriginalElevatorFloorPeopleCleanupResult
cleanup_original_elevator_service_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    std::uint16_t waiting_delay,
    OriginalPersonFamilyDispatch dispatch,
    void* context,
    bool suppress_car_family_dispatch) noexcept {
  OriginalElevatorFloorPeopleCleanupResult result{};
  if (elevator_index >= document.elevators.size() || floor < 0 ||
      floor >= 120) {
    return result;
  }
  auto& elevator = document.elevators[elevator_index];
  if (elevator.used == 0U) return result;
  const auto floor_index = static_cast<std::size_t>(floor);
  const auto capacity = std::min<std::size_t>(elevator.capacity, 42U);
  const auto person_limit = std::min<std::size_t>(
      document.people_count, document.people.size());

  const auto valid_person = [&](std::uint32_t person_index) noexcept {
    return person_index < person_limit;
  };
  bool has_people = false;
  for (const auto& car : elevator.car_records) {
    if (car.exact_bytes[15] == std::byte{0} ||
        car.exact_bytes[226U + floor_index] == std::byte{0}) {
      continue;
    }
    for (std::size_t slot = 0U; slot < capacity; ++slot) {
      if (signed_byte(car.exact_bytes[184U + slot]) != floor) continue;
      has_people = true;
      if (!valid_person(load_u32(car.exact_bytes, 16U + slot * 4U,
                                 document.header.byte_swapped))) {
        return result;
      }
    }
  }

  const auto mapped = original_elevator_floor_record_index(
      elevator.type, elevator.bottom_floor, elevator.top_floor, floor);
  auto floor_record = elevator.floor_records.end();
  if (mapped >= 0) {
    floor_record = std::find_if(
        elevator.floor_records.begin(), elevator.floor_records.end(),
        [&](const OriginalTdtElevatorFloorRecord& record) {
          return record.mapped_index == mapped;
        });
  }
  if (floor_record != elevator.floor_records.end()) {
    const auto& exact = floor_record->exact_bytes;
    for (const auto [count_offset, cursor_offset, table_offset] :
         {std::array<std::size_t, 3>{0U, 1U, 4U},
          std::array<std::size_t, 3>{2U, 3U, 164U}}) {
      const auto signed_count = signed_byte(exact[count_offset]);
      if (signed_count <= 0) continue;
      has_people = true;
      const auto cursor = std::to_integer<std::uint8_t>(exact[cursor_offset]);
      if (cursor >= 40U) return result;
      for (std::size_t ordinal = 0U;
           ordinal < static_cast<std::size_t>(signed_count); ++ordinal) {
        const auto slot = (cursor + ordinal) % 40U;
        if (!valid_person(load_u32(exact, table_offset + slot * 4U,
                                   document.header.byte_swapped))) {
          return result;
        }
      }
    }
  }
  if (has_people && dispatch == nullptr) {
    result.status =
        OriginalElevatorFloorPeopleCleanupStatus::dispatch_required;
    return result;
  }

  // 14fa walks all eight car records. 154a calls 1210:0883 only when this
  // car's per-floor occupancy byte is nonzero, then releases both owners.
  for (std::size_t car_index = 0U;
       car_index < elevator.car_records.size(); ++car_index) {
    auto& car = elevator.car_records[car_index].exact_bytes;
    if (car[15] == std::byte{0}) continue;
    auto& occupancy = car[226U + floor_index];
    if (occupancy != std::byte{0}) {
      for (std::size_t slot = 0U; slot < capacity; ++slot) {
        if (signed_byte(car[184U + slot]) != floor) continue;
        const auto popped = pop_original_elevator_car_passenger_slot(
            document, elevator_index, car_index, slot);
        if (!popped || !valid_person(*popped)) return result;
        auto& person = document.people[static_cast<std::size_t>(*popped)];
        if (!suppress_car_family_dispatch &&
            original_car_arrival_dispatches_family(person.exact_bytes[4])) {
          if (original_car_arrival_sets_person_floor(person.exact_bytes[4])) {
            person.exact_bytes[7] = static_cast<std::byte>(floor);
          }
          dispatch(document, static_cast<std::size_t>(*popped),
                   OriginalPersonFamilyDispatchSource::elevator_car_0883,
                   context);
        }
        car[3] = static_cast<std::byte>(
            std::to_integer<std::uint8_t>(car[3]) - 1U);
        occupancy = static_cast<std::byte>(
            std::to_integer<std::uint8_t>(occupancy) - 1U);
        ++result.car_passengers;
      }
      if (occupancy == std::byte{0}) {
        car[12] = static_cast<std::byte>(
            std::to_integer<std::uint8_t>(car[12]) - 1U);
      }
    }

    const auto owner = static_cast<std::byte>(car_index + 1U);
    if (elevator.block_2a2[floor_index] == owner) {
      elevator.block_2a2[floor_index] = std::byte{0};
      store_u16(car, 10U,
                static_cast<std::uint16_t>(
                    load_u16(car, 10U, document.header.byte_swapped) - 1U),
                document.header.byte_swapped);
    }
    if (elevator.block_31a[floor_index] == owner) {
      elevator.block_31a[floor_index] = std::byte{0};
      store_u16(car, 10U,
                static_cast<std::uint16_t>(
                    load_u16(car, 10U, document.header.byte_swapped) - 1U),
                document.header.byte_swapped);
    }
    recompute_original_elevator_car(
        elevator, car_index, document.header.byte_swapped);
  }

  // 1625 drains the up ring completely before the down ring. The person
  // dword stays stale in its slot; only cursor and count advance/change.
  if (floor_record != elevator.floor_records.end()) {
    auto& exact = floor_record->exact_bytes;
    for (const auto [count_offset, cursor_offset, table_offset] :
         {std::array<std::size_t, 3>{0U, 1U, 4U},
          std::array<std::size_t, 3>{2U, 3U, 164U}}) {
      while (signed_byte(exact[count_offset]) > 0) {
        const auto cursor = std::to_integer<std::uint8_t>(exact[cursor_offset]);
        const auto person_index = load_u32(
            exact, table_offset + cursor * 4U,
            document.header.byte_swapped);
        exact[cursor_offset] =
            static_cast<std::byte>((cursor + 1U) % 40U);
        if (elevator.type != 2U) {
          add_original_person_waiting_delay(
              document.people[static_cast<std::size_t>(person_index)],
              waiting_delay, document.header.byte_swapped);
        }
        dispatch(document, static_cast<std::size_t>(person_index),
                 OriginalPersonFamilyDispatchSource::dispatcher_16ab,
                 context);
        exact[count_offset] = static_cast<std::byte>(
            std::to_integer<std::uint8_t>(exact[count_offset]) - 1U);
        ++result.waiting_passengers;
      }
    }
  }
  result.status = OriginalElevatorFloorPeopleCleanupStatus::cleaned;
  return result;
}

namespace {

struct OriginalNativeElevatorCleanupContext {
  const OriginalPartTable* part{};
  const OriginalYenTable* rent_income{};
  std::vector<OriginalPersonFamilyDispatchResult>* dispatches{};
};

void dispatch_original_native_elevator_cleanup_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    OriginalPersonFamilyDispatchSource source,
    void* raw_context) noexcept {
  auto& context =
      *static_cast<OriginalNativeElevatorCleanupContext*>(raw_context);
  context.dispatches->push_back(dispatch_original_person_family(
      document, person_index, *context.part, *context.rent_income, source));
}

}  // namespace

OriginalNativeElevatorFloorPeopleCleanupResult
cleanup_original_elevator_service_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    std::uint16_t waiting_delay,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income,
    bool suppress_car_family_dispatch) noexcept {
  OriginalNativeElevatorFloorPeopleCleanupResult result{};
  OriginalNativeElevatorCleanupContext context{
      &part, &rent_income, &result.family_dispatches};
  result.cleanup = cleanup_original_elevator_service_floor_people(
      document, elevator_index, floor, waiting_delay,
      dispatch_original_native_elevator_cleanup_person, &context,
      suppress_car_family_dispatch);
  return result;
}

OriginalNativeElevatorFloorPeopleCleanupResult
cleanup_original_elevator_waiting_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    std::uint16_t waiting_delay,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) noexcept {
  OriginalNativeElevatorFloorPeopleCleanupResult result{};
  OriginalNativeElevatorCleanupContext context{
      &part, &rent_income, &result.family_dispatches};
  result.cleanup = cleanup_original_elevator_waiting_floor_people(
      document, elevator_index, floor, waiting_delay,
      dispatch_original_native_elevator_cleanup_person, &context);
  return result;
}

OriginalNativeElevatorFloorPeopleCleanupResult
cleanup_original_elevator_car_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income,
    bool suppress_car_family_dispatch) noexcept {
  OriginalNativeElevatorFloorPeopleCleanupResult result{};
  OriginalNativeElevatorCleanupContext context{
      &part, &rent_income, &result.family_dispatches};
  result.cleanup = cleanup_original_elevator_car_floor_people(
      document, elevator_index, floor,
      dispatch_original_native_elevator_cleanup_person, &context,
      suppress_car_family_dispatch);
  return result;
}

OriginalNativeElevatorFloorPeopleCleanupResult
cleanup_original_elevator_selected_car_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index,
    std::int16_t floor,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income,
    bool suppress_car_family_dispatch) noexcept {
  OriginalNativeElevatorFloorPeopleCleanupResult result{};
  OriginalNativeElevatorCleanupContext context{
      &part, &rent_income, &result.family_dispatches};
  result.cleanup = cleanup_original_elevator_selected_car_floor_people(
      document, elevator_index, car_index, floor,
      dispatch_original_native_elevator_cleanup_person, &context,
      suppress_car_family_dispatch);
  return result;
}

OriginalFacilityPeopleCleanupResult cleanup_original_facility_people(
    OriginalTdtDocument& document,
    std::int16_t floor_number,
    std::size_t tenant_index,
    std::uint16_t frame_time) noexcept {
  OriginalFacilityPeopleCleanupResult result{};
  if (floor_number < 0 ||
      static_cast<std::size_t>(floor_number) >= document.floors.size()) {
    return result;
  }
  auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  if (tenant_index >= floor.tenants.size()) return result;
  const auto source = floor.tenants[tenant_index];

  // These five entries jump directly out of 1220:10af before its nonzero-
  // argument retirement tail. Types 24..26 are also protected by 11f8:3383.
  if (source.type == 11 || source.type == 13 || source.type == 24 ||
      source.type == 25 || source.type == 26) {
    return result;
  }

  result.finalized = finalize_original_facility_people(
      document, floor, source, frame_time);

  const auto key = signed_byte(source.exact_bytes[12]);
  if (key < 0 ||
      static_cast<std::size_t>(key) >= floor.tenant_index.size()) {
    return result;
  }
  const auto target_index = floor.tenant_index[static_cast<std::size_t>(key)];
  if (target_index >= floor.tenants.size()) return result;
  const auto& target = floor.tenants[target_index];
  const auto start = original_tenant_people_start(
      target, document.header.byte_swapped);
  const auto count = original_person_span(target.type);

  for (std::size_t ordinal = 0; ordinal < count; ++ordinal) {
    const auto wide_index = static_cast<std::uint64_t>(start) + ordinal;
    if (wide_index >= document.people.size() ||
        wide_index >= document.people_count) {
      break;
    }
    const auto person_index = static_cast<std::size_t>(wide_index);
    if (source.type >= 3 && source.type <= 5) {
      remove_original_hotel_person_parking(document, person_index);
    }
    (void)remove_original_person_link(document, person_index);
    auto& person = document.people[person_index].exact_bytes;
    person[4] = std::byte{0};
    person[6] = std::byte{0};
    ++result.retired;

    if (source.type >= 3 && source.type <= 5 &&
        document.post_elevator.b928 != 0U &&
        document.post_elevator.b924 ==
            static_cast<std::int32_t>(person_index)) {
      document.post_elevator.b923 = 0U;
      document.post_elevator.b928 = 0U;
      document.post_elevator.b924 = -1;
      result.cleared_periodic_visitor = true;
      result.notification_code = 3003U;
    }
  }
  return result;
}

std::size_t prepare_original_facilities_for_night(
    OriginalTdtDocument& document) noexcept {
  std::size_t touched = 0U;
  for (auto& floor : document.floors) {
    for (auto& tenant : floor.tenants) {
      std::uint8_t replacement = 0U;
      std::uint8_t threshold = 0U;
      switch (tenant.type) {
        case 3:
        case 4:
        case 5:
        case 9:
          replacement = 0x10U;
          threshold = 0x18U;
          break;
        case 7:
          replacement = 0x08U;
          threshold = 0x10U;
          break;
        default:
          continue;
      }
      if (tenant.status >= threshold) {
        continue;
      }
      set_original_tenant_status(tenant, replacement);
      mark_original_tenant_changed(tenant);
      ++touched;
    }
  }
  return touched;
}

std::size_t advance_original_facilities_for_day(
    OriginalTdtDocument& document,
    std::uint8_t calendar_phase) noexcept {
  std::size_t changed = 0U;
  for (auto& floor : document.floors) {
    for (auto& tenant : floor.tenants) {
      bool touched = false;
      switch (tenant.type) {
        case 3:
        case 4:
        case 5:
          // 1228:0a03 rolls the three occupied Hotel bands down by eight.
          if (tenant.status == 0x20U || tenant.status == 0x30U ||
              tenant.status == 0x40U) {
            set_original_tenant_status(
                tenant, static_cast<std::uint8_t>(tenant.status - 8U));
            touched = true;
          }
          break;
        case 7:
          // An Office in status 0x18 first becomes 0x10. Every other status
          // is replaced with zero/eight according to DS:b3a0.
          set_original_tenant_status(
              tenant, tenant.status == 0x18U
                          ? 0x10U
                          : static_cast<std::uint8_t>(calendar_phase == 0U
                                                          ? 0U
                                                          : 8U));
          touched = true;
          break;
        case 9:
          if (tenant.status == 0x20U) {
            set_original_tenant_status(tenant, 0x18U);
            touched = true;
          }
          break;
        case 13:
          // 1228:0ada marks Medical Center dirty without changing +0x0c.
          touched = true;
          break;
        case 31:
        case 32:
        case 33:
        case 36:
        case 37:
        case 38:
        case 39:
        case 40:
          // loc_0abd clears the complete word at tenant+0x0c.
          tenant.variant = 0U;
          tenant.exact_bytes[6] = std::byte{0};
          tenant.exact_bytes[7] = std::byte{0};
          tenant.preserved_07_to_0f[0] = std::byte{0};
          touched = true;
          break;
        default:
          break;
      }
      if (touched) {
        mark_original_tenant_changed(tenant);
        ++changed;
      }
    }
  }
  return changed;
}

std::size_t advance_original_facilities_for_evening(
    OriginalTdtDocument& document) noexcept {
  std::size_t changed = 0U;
  for (auto& floor : document.floors) {
    for (auto& tenant : floor.tenants) {
      bool touched = false;
      switch (tenant.type) {
        case 3:
        case 4:
        case 5:
          // 1228:0bf8 advances the three occupied Hotel bands by eight.
          if (tenant.status == 0x18U || tenant.status == 0x28U ||
              tenant.status == 0x38U) {
            set_original_tenant_status(
                tenant, static_cast<std::uint8_t>(tenant.status + 8U));
            touched = true;
          }
          break;
        case 7:
          if (tenant.status == 0x10U) {
            set_original_tenant_status(tenant, 0x18U);
            touched = true;
          } else if (tenant.status == 0U) {
            set_original_tenant_status(tenant, 8U);
            touched = true;
          }
          break;
        case 9:
          if (tenant.status == 0x18U) {
            set_original_tenant_status(tenant, 0x20U);
            touched = true;
          } else if (tenant.status <= 7U) {
            set_original_tenant_status(
                tenant, static_cast<std::uint8_t>((tenant.status & 7U) + 8U));
            touched = true;
          }
          break;
        case 13:
          // loc_0d1b only marks the Medical Center tenant dirty.
          touched = true;
          break;
        case 31:
        case 32:
        case 33:
        case 36:
        case 37:
        case 38:
        case 39:
        case 40:
          tenant.variant = 1U;
          tenant.exact_bytes[6] = std::byte{1};
          tenant.exact_bytes[7] = std::byte{0};
          tenant.preserved_07_to_0f[0] = std::byte{0};
          touched = true;
          break;
        default:
          break;
      }
      if (touched) {
        mark_original_tenant_changed(tenant);
        ++changed;
      }
    }
  }
  return changed;
}

std::size_t repair_original_hotel_pair_states(
    OriginalTdtDocument& document,
    std::int8_t day_phase) noexcept {
  std::size_t touched = 0;
  const std::uint8_t new_status = day_phase < 4 ? 0x38U : 0x40U;
  for (auto& floor : document.floors) {
    std::size_t index = 0;
    while (index < floor.tenants.size()) {
      const auto& current = floor.tenants[index];
      const auto signed_status = std::bit_cast<std::int8_t>(current.status);
      if (!is_original_hotel_type(current.type) || signed_status < 0x38) {
        ++index;
        continue;
      }

      if (index > 0U &&
          is_original_hotel_type(floor.tenants[index - 1U].type)) {
        set_original_hotel_pair_state(floor.tenants[index - 1U], new_status);
        ++touched;
      }

      const std::size_t next = index + 1U;
      if (next < floor.tenants.size() &&
          is_original_hotel_type(floor.tenants[next].type)) {
        const auto signed_next_status =
            std::bit_cast<std::int8_t>(floor.tenants[next].status);
        if (signed_next_status >= 0x38) {
          // 1130:02eb branches directly to the loop test and revisits this
          // already-high adjacent record on the next iteration.
          index = next;
          continue;
        }
        set_original_hotel_pair_state(floor.tenants[next], new_status);
        ++touched;
      }

      // Other paths increment after the routine has already advanced its
      // index from current-1 to current+1, skipping the adjacent slot.
      index += 2U;
    }
  }
  return touched;
}

std::size_t remove_original_nightly_person_links(
    OriginalTdtDocument& document) noexcept {
  // Executable segment 50, CS:0a04 parallel-lookup keys for 1188:0977.
  constexpr std::array<std::int8_t, 7> kTypes = {
      6, 10, 12, 18, 29, 33, 36};
  return remove_original_person_links_by_type(document, kTypes);
}

std::size_t remove_original_hotel_person_links(
    OriginalTdtDocument& document) noexcept {
  constexpr std::array<std::int8_t, 3> kTypes = {3, 4, 5};
  return remove_original_person_links_by_type(document, kTypes);
}

OriginalRecyclingStepResult advance_original_recycling_phase(
    OriginalTdtDocument& document,
    std::uint8_t requested_phase) noexcept {
  OriginalRecyclingStepResult result{};
  // Both 1088:0000 and 1088:00de are unavailable below a three-star rating.
  if (document.header.rating < 3U) {
    return result;
  }

  const auto center_count = static_cast<std::int16_t>(
      load_original_header_word(document, 42U));  // DS:b3f4
  if (center_count == 0) {
    result.notification_code = 3U;
    document.post_elevator.b92c = 0U;
    return result;
  }

  std::uint8_t phase =
      original_recycling_population_phase(document, center_count);
  if (phase > requested_phase) {
    phase = requested_phase;
    if (requested_phase == 5U) {
      result.notification_code = 4U;
    }
    document.post_elevator.b92c = 0U;
  } else {
    document.post_elevator.b92c = 1U;
  }

  for (auto& floor : document.floors) {
    for (auto& tenant : floor.tenants) {
      if (tenant.type != 20 && tenant.type != 21) {
        continue;
      }
      // loc_0085 deliberately leaves frame five untouched after the phase
      // gate has fallen to zero.
      if (document.post_elevator.b92c == 0U && tenant.status == 5U) {
        continue;
      }
      set_original_tenant_status(tenant, phase);
      mark_original_tenant_changed(tenant);
      ++result.touched;
    }
  }
  return result;
}

OriginalRecyclingStepResult reset_original_recycling_for_day(
    OriginalTdtDocument& document) noexcept {
  OriginalRecyclingStepResult result{};
  if (document.header.rating < 3U) {
    return result;
  }
  const auto center_count = static_cast<std::int16_t>(
      load_original_header_word(document, 42U));  // DS:b3f4
  if (center_count == 0) {
    result.notification_code = 3U;
    document.post_elevator.b92c = 0U;
    return result;
  }

  for (auto& floor : document.floors) {
    for (auto& tenant : floor.tenants) {
      if ((tenant.type != 20 && tenant.type != 21) || tenant.status == 0U ||
          document.post_elevator.b92c == 0U) {
        continue;
      }
      set_original_tenant_status(tenant, tenant.type == 20 ? 0U : 6U);
      mark_original_tenant_changed(tenant);
      ++result.touched;
    }
  }
  result.play_transition_sound = result.touched != 0U;
  return result;
}

std::size_t finish_original_recycling_day_start(
    OriginalTdtDocument& document) noexcept {
  std::size_t touched = 0U;
  for (auto& floor : document.floors) {
    for (auto& tenant : floor.tenants) {
      if (tenant.type != 21 || tenant.status != 6U) {
        continue;
      }
      set_original_tenant_status(tenant, 0U);
      mark_original_tenant_changed(tenant);
      ++touched;
    }
  }
  return touched;
}

}  // namespace simtower
