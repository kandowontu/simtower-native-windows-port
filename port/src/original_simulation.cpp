#include "original_simulation.hpp"

#include "original_audio.hpp"
#include "original_construction.hpp"
#include "original_people.hpp"
#include "original_tdt.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <span>

namespace simtower {
namespace {

void call(std::vector<OriginalSimulationCall>& calls,
          std::uint16_t selector,
          std::uint16_t offset,
          std::initializer_list<std::int32_t> arguments = {}) {
  calls.push_back({selector, offset, arguments});
}

void play(std::vector<OriginalSimulationCall>& calls,
          std::int32_t resource,
          std::int32_t repeat,
          std::int32_t priority) {
  call(calls, 0x11c8, 0x0167, {resource, repeat, priority});
}

constexpr std::uint16_t byte_swap(std::uint16_t value) noexcept {
  return static_cast<std::uint16_t>((value << 8U) | (value >> 8U));
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
  const auto bytes = std::span<const std::byte>(document.header.exact_bytes);
  std::uint16_t value =
      static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
      static_cast<std::uint16_t>(
          std::to_integer<std::uint8_t>(bytes[offset + 1U]) << 8U);
  return document.header.byte_swapped ? byte_swap(value) : value;
}

void store_original_header_word(OriginalTdtDocument& document,
                                std::size_t version_20_offset,
                                std::uint16_t value) noexcept {
  const auto offset =
      original_header_runtime_offset(document, version_20_offset);
  if (offset + 2U > document.header.exact_bytes.size()) {
    return;
  }
  if (document.header.byte_swapped) {
    value = byte_swap(value);
  }
  document.header.exact_bytes[offset] = static_cast<std::byte>(value);
  document.header.exact_bytes[offset + 1U] =
      static_cast<std::byte>(value >> 8U);
}

std::uint32_t load_original_header_dword(
    const OriginalTdtDocument& document,
    std::size_t version_20_offset) noexcept {
  const auto low = load_original_header_word(document, version_20_offset);
  const auto high = load_original_header_word(document,
                                               version_20_offset + 2U);
  return document.header.byte_swapped
             ? (static_cast<std::uint32_t>(low) << 16U) | high
             : static_cast<std::uint32_t>(low) |
                   (static_cast<std::uint32_t>(high) << 16U);
}

void store_original_header_dword(OriginalTdtDocument& document,
                                 std::size_t version_20_offset,
                                 std::uint32_t value) noexcept {
  if (document.header.byte_swapped) {
    store_original_header_word(document, version_20_offset,
                               static_cast<std::uint16_t>(value >> 16U));
    store_original_header_word(document, version_20_offset + 2U,
                               static_cast<std::uint16_t>(value));
  } else {
    store_original_header_word(document, version_20_offset,
                               static_cast<std::uint16_t>(value));
    store_original_header_word(document, version_20_offset + 2U,
                               static_cast<std::uint16_t>(value >> 16U));
  }
}

std::int32_t wrapping_add(std::int32_t left, std::int32_t right) noexcept {
  return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(left) +
                                     std::bit_cast<std::uint32_t>(right));
}

std::int32_t wrapping_subtract(std::int32_t left,
                               std::int32_t right) noexcept {
  return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(left) -
                                     std::bit_cast<std::uint32_t>(right));
}

std::int32_t wrapping_multiply(std::int32_t left,
                               std::int32_t right) noexcept {
  return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(left) *
                                     std::bit_cast<std::uint32_t>(right));
}

std::int32_t cap_original_positive_delta(
    std::int32_t balance,
    std::int32_t amount) noexcept {
  // 1178:1377 compares the result of a wrapping ADD as a signed dword.
  constexpr std::int32_t kMaximumBalance = 99'999'999;
  if (wrapping_add(balance, amount) > kMaximumBalance) {
    return wrapping_subtract(kMaximumBalance, balance);
  }
  return amount;
}

std::int32_t original_rent_amount(
    const OriginalYenTable& rent_income,
    std::uint16_t facility_type,
    std::uint16_t rent_tier) noexcept {
  if (rent_tier == 4U) {
    return 0;
  }
  // All executable callers pass tiers 0..4 and types whose four-entry band
  // fits YEN/1001. Treat corrupt native state as a no-op instead of reading
  // beyond the original 180-byte resource.
  if (rent_tier > 4U) {
    return 0;
  }
  const auto index = static_cast<std::size_t>(facility_type) * 4U + rent_tier;
  if (index >= rent_income.size()) {
    return 0;
  }
  return std::bit_cast<std::int32_t>(rent_income[index]);
}

std::uint16_t next_original_random(OriginalTdtDocument& document) noexcept {
  // Microsoft C 7.0/Visual C++ 1.x rand() at 1000:3a2f.
  document.random_state = document.random_state * 0x015a4e35U + 1U;
  return static_cast<std::uint16_t>(
      (document.random_state >> 16U) & 0x7fffU);
}

std::int16_t original_random_between(OriginalTdtDocument& document,
                                     std::int16_t low,
                                     std::int16_t high) noexcept {
  // 10c8:03b0 returns before rand() when the signed interval is empty.
  if (low > high) {
    return -1;
  }
  const auto span = static_cast<std::int32_t>(high) - low + 1;
  return static_cast<std::int16_t>(
      static_cast<std::int32_t>(next_original_random(document)) % span +
      low);
}

}  // namespace

std::int16_t select_original_event_floor(
    OriginalTdtDocument& document,
    std::int16_t requested) noexcept {
  // Literal 10c8:033e. In particular, the original tests `first > 0`, not
  // `first >= 0`, and returns -1 when a constructed run reaches floor 119
  // without a following empty allocation.
  std::int16_t last = -1;
  std::int16_t first = -1;
  std::int16_t floor = 0;
  while (floor < static_cast<std::int16_t>(document.floors.size()) &&
         last < 0) {
    if (document.floors[static_cast<std::size_t>(floor)].tenants.empty()) {
      if (first > 0) {
        last = static_cast<std::int16_t>(floor - 1);
      }
    } else if (first < 0) {
      first = floor;
    }
    ++floor;
  }
  if (last < requested || last == -1) {
    return -1;
  }
  return original_random_between(document, requested, last);
}

namespace {

std::uint32_t load_original_tenant_people_start(
    const OriginalTdtTenant& tenant,
    bool byte_swapped) noexcept {
  const auto byte = [&](std::size_t index) {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(tenant.exact_bytes[8U + index]));
  };
  if (byte_swapped) {
    return (byte(0) << 24U) | (byte(1) << 16U) | (byte(2) << 8U) | byte(3);
  }
  return byte(0) | (byte(1) << 8U) | (byte(2) << 16U) | (byte(3) << 24U);
}

void store_original_person_word(OriginalTdtPersonRecord& person,
                                std::size_t offset,
                                std::uint16_t value,
                                bool byte_swapped) noexcept {
  if (byte_swapped) {
    person.exact_bytes[offset] = static_cast<std::byte>(value >> 8U);
    person.exact_bytes[offset + 1U] = static_cast<std::byte>(value);
  } else {
    person.exact_bytes[offset] = static_cast<std::byte>(value);
    person.exact_bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
  }
}

void dispatch_original_security_people(OriginalTdtDocument& document,
                                       std::uint16_t flags) noexcept {
  // Complete persisted portion of 10f8:033d. DS:cf88 contains ten native
  // floor/key words registered by Security and Housekeeping activation.
  const bool secom_available =
      std::bit_cast<std::int16_t>(load_original_header_word(document, 32U)) >=
      0;
  const auto event_floor = load_original_header_word(document, 64U);

  for (const auto slot : document.post_elevator.cf88_words) {
    const auto floor_number = static_cast<std::uint8_t>(slot);
    const auto key = static_cast<std::uint8_t>(slot >> 8U);
    if (floor_number == 0xffU ||
        floor_number >= document.floors.size() ||
        key >= OriginalTdtFloor::kIndexCapacity) {
      continue;
    }
    auto& floor = document.floors[floor_number];
    const auto tenant_index = floor.tenant_index[key];
    if (tenant_index >= floor.tenants.size()) {
      continue;
    }
    auto& tenant = floor.tenants[tenant_index];
    const auto people_start = load_original_tenant_people_start(
        tenant, document.header.byte_swapped);
    for (std::size_t ordinal = 0; ordinal < 6U; ++ordinal) {
      const auto person_index =
          static_cast<std::uint64_t>(people_start) + ordinal;
      if (person_index >= document.people.size() ||
          person_index >= document.people_count) {
        break;
      }
      auto& person = document.people[static_cast<std::size_t>(person_index)];
      // 10f8:0396-0450 gives Bomb bit zero priority over Fire bit three.
      // With SECOM and both bits present, only ordinal zero responds; the Fire
      // branch is reached only when Bomb is clear.
      if ((flags & 1U) != 0U) {
        if (!secom_available) {
          person.exact_bytes[5] = std::byte{0};
          person.exact_bytes[7] = static_cast<std::byte>(
              ordinal >= 3U ? static_cast<std::uint8_t>(floor_number - 1U)
                            : floor_number);
        } else if (ordinal == 0U) {
          person.exact_bytes[5] = std::byte{0};
          person.exact_bytes[7] = static_cast<std::byte>(floor_number);
        } else {
          person.exact_bytes[5] = std::byte{1};
          person.exact_bytes[7] = std::byte{0};
        }
      } else if ((flags & 8U) != 0U) {
        person.exact_bytes[5] = std::byte{0};
        person.exact_bytes[7] = static_cast<std::byte>(floor_number);
      } else {
        person.exact_bytes[5] = std::byte{1};
        person.exact_bytes[7] = std::byte{0};
      }
      person.exact_bytes[8] = std::byte{0};
      store_original_person_word(person, 10U, 0U,
                                 document.header.byte_swapped);
      store_original_person_word(person, 12U, 0U,
                                 document.header.byte_swapped);
      store_original_person_word(person, 14U, 0U,
                                 document.header.byte_swapped);
    }

    if ((flags & 1U) != 0U) {
      const auto target = secom_available
                              ? event_floor
                              : static_cast<std::uint16_t>(floor_number);
      tenant.status = static_cast<std::uint8_t>(target - 1U);
      tenant.exact_bytes[5] = static_cast<std::byte>(tenant.status);
      tenant.variant = static_cast<std::uint8_t>(target);
      tenant.exact_bytes[6] = static_cast<std::byte>(target);
      tenant.exact_bytes[7] = static_cast<std::byte>(target >> 8U);
      tenant.preserved_07_to_0f[0] =
          static_cast<std::byte>(target >> 8U);
    }
  }
}

std::uint16_t original_tenant_variant_word(
    const OriginalTdtTenant& tenant) noexcept {
  return static_cast<std::uint16_t>(tenant.variant) |
         static_cast<std::uint16_t>(
             std::to_integer<std::uint8_t>(tenant.preserved_07_to_0f[0]))
             << 8U;
}

void store_original_tenant_variant_word(OriginalTdtTenant& tenant,
                                        std::uint16_t value) noexcept {
  tenant.variant = static_cast<std::uint8_t>(value);
  tenant.preserved_07_to_0f[0] = static_cast<std::byte>(value >> 8U);
  tenant.exact_bytes[6] = static_cast<std::byte>(value);
  tenant.exact_bytes[7] = static_cast<std::byte>(value >> 8U);
  tenant.exact_bytes[13] = std::byte{1};
  tenant.preserved_07_to_0f[6] = std::byte{1};
}

std::uint16_t load_runtime_word(std::span<const std::byte> bytes,
                                std::size_t offset) noexcept {
  return static_cast<std::uint16_t>(
             std::to_integer<std::uint8_t>(bytes[offset])) |
         static_cast<std::uint16_t>(
             std::to_integer<std::uint8_t>(bytes[offset + 1U]))
             << 8U;
}

void store_runtime_word(std::span<std::byte> bytes,
                        std::size_t offset,
                        std::uint16_t value) noexcept {
  bytes[offset] = static_cast<std::byte>(value);
  bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

void charge_original_maintenance(OriginalTdtDocument& document,
                                 std::uint16_t facility_type,
                                 std::int32_t amount) noexcept {
  // 1178:097c/09ee/0a6a debit the wrapping signed dword directly and then
  // publish the same amount through 1060:0880. Unlike rent and other positive
  // income, none of these maintenance paths calls 1178:1377's balance cap.
  // Zero charges leave every field untouched.
  if (amount == 0) {
    return;
  }

  document.header.balance =
      wrapping_subtract(document.header.balance, amount);
  add_original_maintenance_for_type(document, facility_type, amount);
}

void charge_original_scaled_maintenance(
    OriginalTdtDocument& document,
    const OriginalYenTable& maintenance_costs,
    std::uint16_t facility_type,
    std::int16_t multiplier) noexcept {
  // 1178:09ee is the signed-count form used for Elevator cars and each
  // Stair/Escalator span. It multiplies before the shared debit/accounting.
  const auto base = std::bit_cast<std::int32_t>(
      maintenance_costs[static_cast<std::size_t>(facility_type)]);
  charge_original_maintenance(
      document, facility_type,
      wrapping_multiply(base, static_cast<std::int32_t>(multiplier)));
}

struct OriginalTenantLocation {
  OriginalTdtFloor* floor{};
  std::size_t index{};
  OriginalTdtTenant* tenant{};
};

std::optional<OriginalTenantLocation> original_tenant_location(
    OriginalTdtDocument& document,
    std::byte floor_byte,
    std::byte key_byte) noexcept {
  const auto floor_number = std::bit_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(floor_byte));
  const auto key = std::to_integer<std::uint8_t>(key_byte);
  if (floor_number < 0 ||
      static_cast<std::size_t>(floor_number) >= document.floors.size() ||
      key >= OriginalTdtFloor::kIndexCapacity) {
    return std::nullopt;
  }
  auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  const auto tenant_index = floor.tenant_index[key];
  if (tenant_index >= floor.tenants.size() ||
      floor.tenants[tenant_index].exact_bytes[12] != key_byte) {
    return std::nullopt;
  }
  return OriginalTenantLocation{
      &floor, tenant_index, &floor.tenants[tenant_index]};
}

std::uint32_t original_tenant_people_start(
    const OriginalTdtTenant& tenant,
    bool byte_swapped) noexcept {
  const auto byte = [&](std::size_t index) {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(tenant.exact_bytes[index]));
  };
  if (byte_swapped) {
    return (byte(8) << 24U) | (byte(9) << 16U) | (byte(10) << 8U) |
           byte(11);
  }
  return byte(8) | (byte(9) << 8U) | (byte(10) << 16U) |
         (byte(11) << 24U);
}

std::size_t original_entertainment_person_count(
    std::int8_t facility_type) noexcept {
  switch (facility_type) {
    case 18:
    case 19:
    case 34:
    case 35:
      return 56U;
    case 29:
    case 30:
      return 40U;
    default:
      // The active dc24 records should only refer to the cases above. Six is
      // the 1228:07c5 default retained for a corrupt but bounded native save.
      return 6U;
  }
}

bool original_entertainment_record_matches(
    const std::array<std::byte, 0x0c>& record,
    std::uint16_t service_group) noexcept {
  const std::uint16_t actual_group =
      std::bit_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(record[7])) >= 0
          ? 1U
          : 0U;
  return actual_group == service_group;
}

void mark_original_entertainment_tenants(
    OriginalTdtDocument& document,
    const std::array<std::byte, 0x0c>& record) noexcept {
  // 1180:0f87 returns a two-tenant span while byte 7 is nonnegative and a
  // one-tenant span for the negative sentinel.
  const std::size_t count =
      std::bit_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(record[7])) >= 0
          ? 2U
          : 1U;
  for (std::size_t side = 0; side < 2U; ++side) {
    const auto location = original_tenant_location(
        document, record[side], record[2U + side]);
    if (!location) {
      continue;
    }
    for (std::size_t offset = 0;
         offset < count && location->index + offset < location->floor->tenants.size();
         ++offset) {
      auto& tenant = location->floor->tenants[location->index + offset];
      tenant.exact_bytes[13] = std::byte{1};
      tenant.preserved_07_to_0f[6] = std::byte{1};
    }
  }
}

template <typename Mutation>
void mutate_original_entertainment_people(
    OriginalTdtDocument& document,
    const std::array<std::byte, 0x0c>& record,
    std::uint16_t side,
    Mutation mutation) noexcept {
  if (side > 1U) {
    return;
  }
  const auto location = original_tenant_location(
      document, record[side], record[2U + side]);
  if (!location) {
    return;
  }
  const auto first = original_tenant_people_start(
      *location->tenant, document.header.byte_swapped);
  const auto count = original_entertainment_person_count(
      location->tenant->type);
  if (first >= document.people.size()) {
    return;
  }
  const auto available = std::min<std::size_t>(
      count, document.people.size() - static_cast<std::size_t>(first));
  for (std::size_t index = 0; index < available; ++index) {
    mutation(document.people[static_cast<std::size_t>(first) + index]);
  }
}

std::int32_t original_movie_income(
    const std::array<std::byte, 0x0c>& record,
    const OriginalPartTable& part) noexcept {
  // Exact 1180:0bcb signed attendance threshold and four-value income ladder.
  const auto attendance = static_cast<std::int16_t>(
      std::bit_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(record[11])));
  const auto signed_part = [&](std::size_t index) {
    return static_cast<std::int32_t>(
        std::bit_cast<std::int16_t>(part.words_52_to_ac[index]));
  };
  if (attendance < signed_part(16U)) {
    return signed_part(19U);
  }
  if (attendance < signed_part(17U)) {
    return signed_part(20U);
  }
  if (attendance < signed_part(18U)) {
    return signed_part(21U);
  }
  return signed_part(22U);
}

void add_original_entertainment_income(
    OriginalTdtDocument& document,
    std::uint16_t facility_type,
    std::int32_t amount) noexcept {
  amount = cap_original_positive_delta(document.header.balance, amount);
  if (amount == 0) {
    return;
  }
  document.header.balance = wrapping_add(document.header.balance, amount);
  add_original_income_for_type(document, facility_type, amount);
}

std::uint16_t load_exact_word(std::span<const std::byte> bytes,
                              std::size_t offset,
                              bool byte_swapped) noexcept {
  auto value = static_cast<std::uint16_t>(
                   std::to_integer<std::uint8_t>(bytes[offset])) |
               static_cast<std::uint16_t>(
                   std::to_integer<std::uint8_t>(bytes[offset + 1U]))
                   << 8U;
  return byte_swapped ? byte_swap(value) : value;
}

void store_exact_word(std::span<std::byte> bytes,
                      std::size_t offset,
                      std::uint16_t value,
                      bool byte_swapped) noexcept {
  if (byte_swapped) {
    value = byte_swap(value);
  }
  bytes[offset] = static_cast<std::byte>(value);
  bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

std::int16_t signed_part_head(const OriginalPartTable& part,
                              std::size_t index) noexcept {
  return std::bit_cast<std::int16_t>(part.words_00_to_40[index]);
}

std::int32_t signed_part_tail(const OriginalPartTable& part,
                              std::size_t index) noexcept {
  return static_cast<std::int32_t>(
      std::bit_cast<std::int16_t>(part.words_52_to_ac[index]));
}

}  // namespace

std::size_t original_commercial_lane(
    const OriginalTdtDocument& document) noexcept {
  // 11a8:17eb returns the byte offset inside the 18-byte retail record.
  if (document.header.version_20_word != 0U) {
    return 5U;
  }
  return original_calendar_phase(document.header.current_day) == 0U ? 3U
                                                                     : 4U;
}

std::int16_t original_commercial_capacity(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::uint16_t facility_type) noexcept {
  // Exact 11a8:174e type/lane table. All PART words remain signed.
  const auto lane = original_commercial_lane(document) - 3U;
  switch (facility_type) {
    case 6U:
      return signed_part_head(part, 22U + lane);
    case 10U:
      return signed_part_head(part, 28U + lane);
    case 12U:
      return signed_part_head(part, 15U + lane);
    default:
      return 0;
  }
}

std::int32_t original_commercial_revenue(
    const OriginalPartTable& part,
    std::uint16_t facility_type,
    std::int16_t attendance) noexcept {
  // Exact 11a8:16ac signed threshold ladder and signed dword result.
  if (facility_type == 12U) {
    if (attendance < signed_part_head(part, 13U)) {
      return signed_part_tail(part, 15U);
    }
    if (attendance < signed_part_head(part, 12U)) {
      return signed_part_tail(part, 14U);
    }
    if (attendance < signed_part_head(part, 11U)) {
      return signed_part_tail(part, 13U);
    }
    return signed_part_tail(part, 12U);
  }
  if (facility_type == 6U) {
    if (attendance < signed_part_head(part, 20U)) {
      return signed_part_tail(part, 11U);
    }
    if (attendance < signed_part_head(part, 19U)) {
      return signed_part_tail(part, 10U);
    }
    if (attendance < signed_part_head(part, 18U)) {
      return signed_part_tail(part, 9U);
    }
    return signed_part_tail(part, 8U);
  }
  // 11a8:16ac explicitly returns zero for Retail Shop.
  return 0;
}

namespace {

void mark_original_tenant_dirty(OriginalTdtTenant& tenant) noexcept {
  tenant.exact_bytes[13] = std::byte{1};
  tenant.preserved_07_to_0f[6] = std::byte{1};
}

void append_original_commercial_route(OriginalTdtDocument& document,
                                      std::uint16_t facility_type,
                                      std::int16_t floor,
                                      std::uint16_t retail_index) noexcept {
  // 11a8:166b uses signed IDIV, whose quotient truncates toward zero. Thus
  // floors 0..4 also land in group zero with a negative accepted remainder.
  const auto shifted_floor = static_cast<int>(floor) - 5;
  const auto group = shifted_floor / 15;
  const auto remainder = shifted_floor % 15;
  if (group < 0 || group >= 7 || remainder > 9) {
    return;
  }

  auto append = [&](std::span<std::byte> block,
                    std::size_t group_size) noexcept {
    const auto base = static_cast<std::size_t>(group) * group_size;
    const auto count = load_exact_word(
        std::span<const std::byte>(block), base,
        document.header.byte_swapped);
    const auto destination = base + 2U + static_cast<std::size_t>(count) * 2U;
    // Valid original state cannot overflow a group. Keep malformed imported
    // state bounded instead of reproducing an out-of-allocation write.
    if (destination + 2U > base + group_size) {
      return;
    }
    store_exact_word(block, destination, retail_index,
                     document.header.byte_swapped);
    store_exact_word(block, base, static_cast<std::uint16_t>(count + 1U),
                     document.header.byte_swapped);
  };

  switch (facility_type) {
    case 6U:
      append(document.post_elevator.dynamic_dd60, 0x12eU);
      break;
    case 10U:
      append(document.post_elevator.dynamic_dd5c, 0x26eU);
      break;
    case 12U:
      append(document.post_elevator.dynamic_dd64, 0x1ceU);
      break;
    default:
      break;
  }
}

void initialize_original_commercial_record(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::size_t retail_index,
    OriginalTdtTenant& tenant) noexcept {
  // Exact 11a8:02f2 commercial-record reset, shared by daily and evening
  // Restaurant/Retail scheduling paths.
  auto& record = document.retail[retail_index].exact_bytes;
  if (record[2] != std::byte{0xff}) {
    record[2] = std::byte{0};
  }

  const auto lane = original_commercial_lane(document);
  auto initial = std::bit_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(record[lane]));
  if (lane == 4U) {
    const auto lane_three = std::bit_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(record[3]));
    initial = std::max(initial, lane_three);
  }

  auto population = static_cast<std::int16_t>(initial);
  const auto facility_type = static_cast<std::uint16_t>(
      static_cast<std::uint8_t>(tenant.type));
  const auto capacity =
      original_commercial_capacity(document, part, facility_type);
  if (population > capacity) {
    population = capacity;
  }
  if (population < 10) {
    population = 10;
  }

  record[6] = static_cast<std::byte>(population);
  store_exact_word(
      record, 12U,
      static_cast<std::uint16_t>(-(static_cast<std::int32_t>(population) + 1)),
      document.header.byte_swapped);
  record[8] = record[7];
  add_original_population_for_type(
      document, facility_type,
      static_cast<std::int16_t>(std::bit_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(record[8]))));
  record[lane] = std::byte{0};
  record[7] = std::byte{0};
  record[9] = std::byte{0};
  store_exact_word(record, 16U, 0U, document.header.byte_swapped);
  mark_original_tenant_dirty(tenant);
  append_original_commercial_route(
      document, facility_type,
      std::bit_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(record[0])),
      static_cast<std::uint16_t>(retail_index));
}

std::optional<std::uint8_t> close_original_commercial_record(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::size_t retail_index,
    OriginalTdtTenant& tenant) noexcept {
  auto& record = document.retail[retail_index].exact_bytes;
  record[2] = std::byte{3};
  const auto facility_type = static_cast<std::uint16_t>(
      static_cast<std::uint8_t>(tenant.type));
  const auto attendance = std::bit_cast<std::int16_t>(load_exact_word(
      record, 16U, document.header.byte_swapped));
  const auto revenue =
      original_commercial_revenue(part, facility_type, attendance);
  std::optional<std::uint8_t> income_status_code{};

  // 11a8:06b2 skips 1178:126c for Retail Shop. That common income helper
  // also suppresses Restaurant/Fast Food revenue on both event-cycle days.
  if (facility_type != 10U &&
      document.header.current_day % 60 != 59 &&
      document.header.current_day % 84 != 83) {
    add_original_entertainment_income(document, facility_type, revenue);
    if (facility_type == 6U) income_status_code = 4U;
    if (facility_type == 12U) income_status_code = 5U;
  }
  record[10] = static_cast<std::byte>(revenue);
  mark_original_tenant_dirty(tenant);
  return income_status_code;
}

template <typename Predicate, typename Mutation>
void visit_original_commercial_records(OriginalTdtDocument& document,
                                       Predicate predicate,
                                       Mutation mutation) {
  for (std::size_t index = 0; index < document.retail.size(); ++index) {
    auto& record = document.retail[index].exact_bytes;
    const auto floor_number = std::bit_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(record[0]));
    if (floor_number < 0 || record[1] == std::byte{0xff}) {
      continue;
    }
    const auto location =
        original_tenant_location(document, record[0], record[1]);
    if (!location || !predicate(record, *location->tenant)) {
      continue;
    }
    mutation(index, *location->tenant);
  }
}

void store_original_tenant_exact_byte(OriginalTdtTenant& tenant,
                                      std::size_t offset,
                                      std::uint8_t value) noexcept {
  tenant.exact_bytes[offset] = static_cast<std::byte>(value);
  if (offset >= 7U && offset <= 15U) {
    tenant.preserved_07_to_0f[offset - 7U] = static_cast<std::byte>(value);
  } else if (offset == 16U) {
    tenant.rent_rate = value;
  } else if (offset == 17U) {
    tenant.subtype = value;
  }
}

void store_original_tenant_status(OriginalTdtTenant& tenant,
                                  std::uint8_t value) noexcept {
  tenant.status = value;
  tenant.exact_bytes[5] = static_cast<std::byte>(value);
}

std::uint16_t original_tenant_logical_word_6(
    const OriginalTdtDocument& document,
    const OriginalTdtTenant& tenant) noexcept {
  return load_exact_word(tenant.exact_bytes, 6U,
                         document.header.byte_swapped);
}

std::int16_t original_person_performance(
    const OriginalTdtDocument& document,
    std::uint32_t person_index) noexcept {
  // Exact 1130:0360 signed word-14 / signed byte-9 performance quotient.
  if (person_index >= document.people.size()) {
    return 0;
  }
  const auto& exact = document.people[person_index].exact_bytes;
  const auto divisor = std::bit_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(exact[9]));
  if (divisor == 0) {
    return 0;
  }
  const auto numerator = std::bit_cast<std::int16_t>(load_exact_word(
      exact, 14U, document.header.byte_swapped));
  // Valid simulation counters never form the x86 IDIV overflow pair.
  if (numerator == std::numeric_limits<std::int16_t>::min() &&
      divisor == -1) {
    return numerator;
  }
  return static_cast<std::int16_t>(numerator / divisor);
}

std::uint16_t original_amenity_spacing(std::int8_t type) noexcept {
  switch (type) {
    case 3:
    case 4:
    case 5:
      return 20U;
    case 7:
      return 10U;
    case 9:
      return 30U;
    default:
      return 0U;
  }
}

std::int16_t original_amenity_type(std::int8_t candidate,
                                   std::int8_t desired) noexcept {
  switch (candidate) {
    case 3:
    case 4:
    case 5:
      return desired == 9 ? candidate : 0;
    case 6:
    case 10:
    case 12:
      return candidate;
    case 7:
      return desired == 7 ? 0 : 7;
    case 18:
    case 19:
    case 34:
    case 35:
      return 18;
    case 29:
    case 30:
      return 29;
    default:
      return 0;
  }
}

bool original_tenant_is_isolated(const OriginalTdtFloor& floor,
                                 std::size_t tenant_index) noexcept {
  if (tenant_index >= floor.tenants.size()) {
    return true;
  }
  const auto& tenant = floor.tenants[tenant_index];
  const auto spacing = original_amenity_spacing(tenant.type);
  if (spacing == 0U) {
    return true;
  }

  const auto left_limit = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(tenant.left - spacing));
  auto cursor = tenant_index;
  while (cursor != 0U &&
         std::bit_cast<std::int16_t>(floor.tenants[cursor].left) >=
             left_limit) {
    --cursor;
    if (original_amenity_type(floor.tenants[cursor].type, tenant.type) != 0) {
      return false;
    }
  }

  const auto right_limit = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(tenant.right + spacing));
  cursor = tenant_index;
  while (cursor + 1U < floor.tenants.size() &&
         std::bit_cast<std::int16_t>(floor.tenants[cursor].right) <=
             right_limit) {
    ++cursor;
    if (original_amenity_type(floor.tenants[cursor].type, tenant.type) != 0) {
      return false;
    }
  }
  return true;
}

std::int16_t original_tenant_performance(
    const OriginalTdtDocument& document,
    const OriginalTdtFloor& floor,
    std::size_t tenant_index) noexcept {
  if (tenant_index >= floor.tenants.size()) {
    return -1;
  }
  const auto& tenant = floor.tenants[tenant_index];
  const auto status = std::bit_cast<std::int8_t>(tenant.status);
  std::size_t offset{};
  std::size_t count{};
  switch (tenant.type) {
    case 3:
    case 4:
    case 5:
      if (status >= 0x38) {
        return -1;
      }
      offset = 1U;
      count = tenant.type == 3 ? 1U : 2U;
      break;
    case 7:
      if (status >= 0x10 && tenant.exact_bytes[14] != std::byte{0}) {
        return -1;
      }
      count = 6U;
      break;
    case 9:
      if (status >= 0x18 && tenant.exact_bytes[14] != std::byte{0}) {
        return -1;
      }
      count = 3U;
      break;
    default:
      return -1;
  }

  const auto start = original_tenant_people_start(
      tenant, document.header.byte_swapped);
  std::int32_t sum{};
  for (std::size_t ordinal = 0; ordinal < count; ++ordinal) {
    sum = wrapping_add(
        sum, original_person_performance(
                 document,
                 start + static_cast<std::uint32_t>(offset + ordinal)));
  }
  auto result = static_cast<std::int16_t>(sum / static_cast<std::int32_t>(count));
  // Exact 1130:0630 rent adjustment, amenity bonus, and zero clamp applied
  // to the average calculated by 1130:03f4.
  switch (std::bit_cast<std::int8_t>(tenant.rent_rate)) {
    case 0:
      result = std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(
              static_cast<std::uint16_t>(result) + 30U));
      break;
    case 2:
      result = std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(
              static_cast<std::uint16_t>(result) - 30U));
      break;
    case 3:
      result = 0;
      break;
    default:
      break;
  }
  if (!original_tenant_is_isolated(floor, tenant_index)) {
    result = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(result) + 60U));
  }
  return std::max<std::int16_t>(result, 0);
}

std::size_t original_tenant_metric_span(std::int8_t type) noexcept {
  switch (type) {
    case 3:
      return 1U;
    case 4:
    case 5:
      return 2U;
    case 7:
      return 6U;
    case 9:
      return 3U;
    case 10:
      return 48U;
    default:
      return 0U;
  }
}

void reset_original_tenant_person_metrics(
    OriginalTdtDocument& document,
    const OriginalTdtTenant& tenant) noexcept {
  // Exact 1130:0cec type-specific metric reset. Hotels skip owner ordinal
  // zero, Office resets six records, Condo three, and Retail the PART-sized
  // 48-record span; 11d8:03c4 clears byte 9 and word 14 in each record.
  const auto count = original_tenant_metric_span(tenant.type);
  const auto offset = tenant.type >= 3 && tenant.type <= 5 ? 1U : 0U;
  const auto start = original_tenant_people_start(
      tenant, document.header.byte_swapped);
  for (std::size_t ordinal = 0; ordinal < count; ++ordinal) {
    const auto index = static_cast<std::uint64_t>(start) + offset + ordinal;
    if (index >= document.people.size()) {
      break;
    }
    auto& exact = document.people[static_cast<std::size_t>(index)].exact_bytes;
    exact[9] = std::byte{0};
    store_exact_word(exact, 14U, 0U, document.header.byte_swapped);
  }
}

std::int16_t original_part_satisfaction_threshold(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    bool upper) noexcept {
  std::size_t band{};
  if (document.header.rating == 1U || document.header.rating == 2U) {
    band = 0U;
  } else if (document.header.rating == 3U) {
    band = 1U;
  } else {
    band = 2U;
  }
  return signed_part_head(part, (upper ? 8U : 5U) + band);
}

std::int16_t original_adjusted_retail_threshold(
    std::int16_t threshold,
    const OriginalTdtTenant& tenant) noexcept {
  // Exact 1130:069e signed +5/-5/-12 adjustment selected by rent byte 16.
  switch (std::bit_cast<std::int8_t>(tenant.rent_rate)) {
    case 0:
      return std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(
              static_cast<std::uint16_t>(threshold) + 5U));
    case 2:
      return std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(
              static_cast<std::uint16_t>(threshold) - 5U));
    case 3:
      return std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(
              static_cast<std::uint16_t>(threshold) - 12U));
    default:
      return threshold;
  }
}

std::optional<std::int16_t> original_movie_capacity(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    const OriginalTdtTenant& tenant) noexcept {
  const auto index = original_tenant_logical_word_6(document, tenant);
  if (index >= document.post_elevator.dc24_records.size()) {
    return std::nullopt;
  }
  const auto& record = document.post_elevator.dc24_records[index];
  const auto age = std::bit_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(record[9]));
  const auto band = static_cast<std::int16_t>(age) / 3;
  const auto base = std::bit_cast<std::int8_t>(
                        std::to_integer<std::uint8_t>(record[7])) < 7
                        ? 27U
                        : 23U;
  const auto selected = base +
                        (band == 0 ? 0U : band == 1 ? 1U
                                            : band == 2 ? 2U : 3U);
  return std::bit_cast<std::int16_t>(part.words_52_to_ac[selected]);
}

void update_original_tenant_satisfaction(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    OriginalTdtFloor& floor,
    std::size_t tenant_index) noexcept {
  auto& tenant = floor.tenants[tenant_index];
  std::optional<std::int8_t> replacement;
  if (tenant.type == 6 || tenant.type == 12) {
    const auto service_index = original_tenant_logical_word_6(document, tenant);
    if (service_index < document.retail.size()) {
      const auto attendance = std::bit_cast<std::int16_t>(load_exact_word(
          document.retail[service_index].exact_bytes, 16U,
          document.header.byte_swapped));
      const std::array<std::size_t, 3> thresholds =
          tenant.type == 6 ? std::array<std::size_t, 3>{20U, 19U, 18U}
                           : std::array<std::size_t, 3>{13U, 12U, 11U};
      replacement = attendance < signed_part_head(part, thresholds[0])
                        ? 0
                        : attendance < signed_part_head(part, thresholds[1])
                              ? 1
                              : attendance < signed_part_head(part,
                                                              thresholds[2])
                                    ? 2
                                    : 3;
    }
  } else if (tenant.type == 10) {
    const auto service_index = original_tenant_logical_word_6(document, tenant);
    if (service_index < document.retail.size()) {
      const auto& record = document.retail[service_index].exact_bytes;
      const auto total = std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(
              static_cast<std::int16_t>(std::bit_cast<std::int8_t>(
                  std::to_integer<std::uint8_t>(record[6]))) +
              std::bit_cast<std::int16_t>(load_exact_word(
                  record, 16U, document.header.byte_swapped))));
      const auto high = original_adjusted_retail_threshold(
          signed_part_head(part, 26U), tenant);
      const auto low = original_adjusted_retail_threshold(
          signed_part_head(part, 25U), tenant);
      replacement = high <= total ? 0 : low <= total ? 2 : 1;
    }
  } else if (tenant.type == 18 || tenant.type == 19 || tenant.type == 34 ||
             tenant.type == 35) {
    const auto capacity = original_movie_capacity(document, part, tenant);
    if (capacity == 20) {
      replacement = 0;
    } else if (capacity == 40) {
      replacement = 1;
    } else if (capacity == 60) {
      replacement = 2;
    }
  } else {
    const auto performance =
        original_tenant_performance(document, floor, tenant_index);
    if (performance < 0) {
      replacement = -1;
    } else if (performance <
               original_part_satisfaction_threshold(document, part, false)) {
      replacement = 2;
    } else if (performance <
               original_part_satisfaction_threshold(document, part, true)) {
      replacement = 1;
    } else {
      replacement = 0;
    }
  }

  if (replacement) {
    store_original_tenant_exact_byte(
        tenant, 15U, std::bit_cast<std::uint8_t>(*replacement));
  }
  if (tenant.exact_bytes[14] != std::byte{0}) {
    return;
  }
  const auto satisfaction = std::bit_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(tenant.exact_bytes[15]));
  const auto status = std::bit_cast<std::int8_t>(tenant.status);
  if (tenant.type >= 3 && tenant.type <= 5) {
    if (status < 0x28 && satisfaction != 0) {
      store_original_tenant_exact_byte(tenant, 14U, 1U);
    }
  } else if (satisfaction != 0) {
    store_original_tenant_exact_byte(tenant, 14U, 1U);
  }
}

bool original_retail_service_active(
    const OriginalTdtDocument& document,
    const OriginalTdtTenant& tenant) noexcept {
  const auto index = original_tenant_logical_word_6(document, tenant);
  return index < document.retail.size() &&
         std::bit_cast<std::int8_t>(
             std::to_integer<std::uint8_t>(
                 document.retail[index].exact_bytes[2])) > -1;
}

void run_original_three_day_tenant_departure(
    OriginalTdtDocument& document,
    const OriginalYenTable& rent_income,
    OriginalTdtFloor& floor,
    std::size_t tenant_index) noexcept {
  auto& tenant = floor.tenants[tenant_index];
  if (tenant.exact_bytes[15] != std::byte{0}) {
    return;
  }
  if (tenant.type != 7 && tenant.type != 9 && tenant.type != 10) {
    return;
  }
  // 1130:09e5's Retail branch returns before the same-floor pairing scan
  // when this exact age byte is zero.
  if (tenant.type == 10 && tenant.subtype == 0U) {
    return;
  }
  const auto status = std::bit_cast<std::int8_t>(tenant.status);
  if (tenant.type == 7 && status < 0x10) {
    // Exact 1178:0d3f three-day Office departure/reset transaction.
    store_original_tenant_status(
        tenant, original_day_phase(document.header.frame_time) < 4 ? 0x10U
                                                                    : 0x18U);
    mark_original_tenant_dirty(tenant);
    store_original_tenant_exact_byte(tenant, 14U, 0U);
    store_original_tenant_exact_byte(tenant, 17U, 0U);
    add_original_population_for_type(document, 7U, -6);
  } else if (tenant.type == 9 && status < 0x18) {
    // Exact 1178:1086 three-day Condo departure/reset transaction.
    store_original_tenant_status(
        tenant, original_day_phase(document.header.frame_time) < 4 ? 0x18U
                                                                    : 0x20U);
    mark_original_tenant_dirty(tenant);
    store_original_tenant_exact_byte(tenant, 14U, 0U);
    store_original_tenant_exact_byte(tenant, 17U, 0U);
    remove_original_rent_income(document, rent_income, 9U, tenant.rent_rate);
    add_original_population_for_type(document, 9U, -3);
  } else if (tenant.type == 10 && tenant.subtype != 0U &&
             original_retail_service_active(document, tenant)) {
    // Exact 1178:11da three-day Retail service deactivation/reset transaction.
    const auto service_index = original_tenant_logical_word_6(document, tenant);
    document.retail[service_index].exact_bytes[2] = std::byte{0xff};
    mark_original_tenant_dirty(tenant);
    store_original_tenant_exact_byte(tenant, 14U, 0U);
    store_original_tenant_exact_byte(tenant, 17U, 0U);
  }

  for (std::size_t candidate_index = 0;
       candidate_index < floor.tenants.size(); ++candidate_index) {
    auto& candidate = floor.tenants[candidate_index];
    if (candidate.type != tenant.type ||
        candidate.exact_bytes[15] != std::byte{2}) {
      continue;
    }
    store_original_tenant_exact_byte(tenant, 15U, 1U);
    store_original_tenant_exact_byte(candidate, 15U, 1U);
    store_original_tenant_exact_byte(tenant, 14U, 1U);
    reset_original_tenant_person_metrics(document, tenant);
    break;
  }
}

void run_original_three_day_tenant_income(
    OriginalTdtDocument& document,
    const OriginalYenTable& rent_income,
    OriginalTdtTenant& tenant) noexcept {
  // Exact 1130:0b92 Office/Condo/Retail age, rent, and metric-reset pass.
  const auto status = std::bit_cast<std::int8_t>(tenant.status);
  if (tenant.type == 7 && status < 0x10) {
    if (tenant.subtype < 0x78U) {
      store_original_tenant_exact_byte(
          tenant, 17U, static_cast<std::uint8_t>(tenant.subtype + 1U));
    }
    add_original_rent_income(document, rent_income, 7U, tenant.rent_rate);
  } else if (tenant.type == 9 && status < 0x18) {
    if (tenant.subtype < 0x78U) {
      store_original_tenant_exact_byte(
          tenant, 17U, static_cast<std::uint8_t>(tenant.subtype + 1U));
    }
  } else if (tenant.type == 10 &&
             original_retail_service_active(document, tenant)) {
    if (tenant.subtype < 0x78U) {
      store_original_tenant_exact_byte(
          tenant, 17U, static_cast<std::uint8_t>(tenant.subtype + 1U));
    }
    add_original_rent_income(document, rent_income, 10U, tenant.rent_rate);
  } else {
    return;
  }
  reset_original_tenant_person_metrics(document, tenant);
}

void advance_original_hotel_checkout_state(
    OriginalTdtDocument& document,
    OriginalTdtTenant& tenant) noexcept {
  // Exact 1130:0e5c checkout flag/age transition and phase-selected status.
  if (std::bit_cast<std::int8_t>(tenant.status) < 0x28) {
    return;
  }
  if (tenant.exact_bytes[14] != std::byte{0}) {
    store_original_tenant_exact_byte(tenant, 15U, 0U);
    store_original_tenant_exact_byte(tenant, 17U, 0U);
    store_original_tenant_exact_byte(tenant, 14U, 0U);
  } else {
    store_original_tenant_exact_byte(
        tenant, 17U, static_cast<std::uint8_t>(tenant.subtype + 1U));
  }
  if (tenant.subtype == 3U) {
    store_original_tenant_status(
        tenant, original_day_phase(document.header.frame_time) < 4 ? 0x38U
                                                                    : 0x40U);
    mark_original_tenant_dirty(tenant);
  }
}

void advance_original_hotel_pairing(
    OriginalTdtDocument& document,
    OriginalTdtFloor& floor,
    std::size_t tenant_index) noexcept {
  // Exact 1130:0f57 same-type grade-zero/grade-two pairing scan.
  auto& tenant = floor.tenants[tenant_index];
  if (std::bit_cast<std::int8_t>(tenant.status) >= 0x28) {
    return;
  }
  const auto satisfaction = std::bit_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(tenant.exact_bytes[15]));
  if (satisfaction >= 1) {
    store_original_tenant_exact_byte(tenant, 14U, 1U);
    reset_original_tenant_person_metrics(document, tenant);
    return;
  }
  if (satisfaction != 0) {
    return;
  }
  for (std::size_t candidate_index = 0;
       candidate_index < floor.tenants.size(); ++candidate_index) {
    auto& candidate = floor.tenants[candidate_index];
    if (candidate.type != tenant.type ||
        candidate.exact_bytes[15] != std::byte{2}) {
      continue;
    }
    store_original_tenant_exact_byte(tenant, 15U, 1U);
    store_original_tenant_exact_byte(candidate, 15U, 1U);
    store_original_tenant_exact_byte(tenant, 14U, 1U);
    reset_original_tenant_person_metrics(document, tenant);
    return;
  }
  store_original_tenant_exact_byte(tenant, 14U, 0U);
}

}  // namespace

std::uint8_t original_calendar_phase(std::int32_t current_day) noexcept {
  // 1200:0558 uses signed IDIV twice. Save data and normal simulation keep
  // days non-negative; preserve the observed result for that domain.
  const std::int32_t month_remainder = current_day % 12;
  const std::int32_t third_remainder = month_remainder % 3;
  return third_remainder >= 2 ? 1U : 0U;
}

bool original_special_event_audio_active(
    const OriginalTdtDocument& document) noexcept {
  return (load_original_header_word(document, 60U) & 0x10U) != 0U;
}

bool original_emergency_people_pass_active(
    const OriginalTdtDocument& document) noexcept {
  return (static_cast<std::uint8_t>(
              load_original_header_word(document, 60U)) &
          0x09U) != 0U;
}

bool raise_original_periodic_b406_flag(
    OriginalTdtDocument& document) noexcept {
  // 1020:0dd3 uses signed IDIV by eight and compares the remainder with four.
  if (document.header.current_day % 8 != 4 || document.header.rating >= 5U) {
    return false;
  }
  const auto flags = load_original_header_word(document, 60U);  // DS:b406
  if ((flags & 0x10U) != 0U) {
    return false;
  }
  store_original_header_word(
      document, 60U, static_cast<std::uint16_t>(flags + 0x10U));
  document.header.version_20_word = 1U;  // DS:b3e4
  return true;
}

bool clear_original_periodic_b406_flag(
    OriginalTdtDocument& document) noexcept {
  const auto flags = load_original_header_word(document, 60U);  // DS:b406
  if ((flags & 0x10U) == 0U) {
    return false;
  }
  store_original_header_word(
      document, 60U, static_cast<std::uint16_t>(flags - 0x10U));
  return true;
}

bool reset_original_periodic_b924_state(
    OriginalTdtDocument& document) noexcept {
  // 1240:01e6 uses signed IDIV by nine.
  if (document.header.current_day % 9 == 3) {
    return false;
  }
  document.post_elevator.b928 = 0U;
  document.post_elevator.b924 = -1;
  return true;
}

namespace {

std::uint16_t original_population_rating(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept {
  // 1140:0411 uses signed dword comparisons for all four PART thresholds and
  // the literal 15000-person Tower threshold.
  const auto population = document.post_elevator.finance.total_population;
  for (std::size_t index = 0U; index < part.dwords_42_to_4e.size(); ++index) {
    const auto threshold =
        std::bit_cast<std::int32_t>(part.dwords_42_to_4e[index]);
    if (population < threshold) {
      return static_cast<std::uint16_t>(index + 1U);
    }
  }
  return population < 15'000 ? 5U : 6U;
}

bool original_rating_treasure_condition(
    const OriginalTdtDocument& document) noexcept {
  // 1148:01a8 addresses floor (8-rating), checks the floor-record count, and
  // compares its signed right-minus-left span with rating*25. Ratings above
  // three return false before touching a floor record.
  const auto rating = document.header.rating;
  if (rating < 1U || rating > 3U) {
    return false;
  }
  const std::size_t floor_index = static_cast<std::size_t>(8U - rating);
  const auto& floor = document.floors[floor_index];
  if (floor.tenants.empty()) {
    return false;
  }
  const auto unsigned_span = static_cast<std::uint16_t>(
      floor.right_edge - floor.left_edge);
  const auto signed_span = std::bit_cast<std::int16_t>(unsigned_span);
  return signed_span > static_cast<std::int16_t>(rating * 25U);
}

}  // namespace

OriginalRatingProgressResult step_original_rating_progress(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::uint8_t calendar_phase,
    std::int8_t day_phase) noexcept {
  OriginalRatingProgressResult result{};
  result.desired_rating = original_population_rating(document, part);
  if (result.desired_rating <= document.header.rating) {
    return result;
  }

  auto& tail = document.post_elevator;
  bool allowed = false;
  const auto report_missing = [&](std::uint16_t code) {
    if (tail.b929 == 0U) {
      result.notification_code = code;
    }
    tail.b929 = 1U;
  };

  switch (document.header.rating) {
    case 2U:
      if (tail.b92a == 0U) {
        report_missing(1U);  // Security
        return result;
      }
      allowed = true;
      break;
    case 3U:
      if (tail.b92b == 0U) {
        report_missing(2U);  // Hotel Suites
        return result;
      }
      if (tail.b92c == 0U || tail.b923 == 0U || day_phase < 4 ||
          calendar_phase == 1U) {
        return result;
      }
      if (tail.b92d == 0U) {
        report_missing(6U);  // Medical Center
        return result;
      }
      allowed = true;
      break;
    case 4U: {
      const auto metro_floor = static_cast<std::int16_t>(
          load_original_header_word(document, 30U));  // DS:b3e8
      if (metro_floor < 0 || tail.b92c == 0U || day_phase < 4 ||
          calendar_phase == 1U) {
        return result;
      }
      if (tail.b92d == 0U) {
        report_missing(6U);  // Medical Center
        return result;
      }
      allowed = true;
      break;
    }
    case 5U:
      // 1148:007e deliberately blocks the ordinary five-to-Tower step. The
      // Cathedral arrival family owns the only normal promotion to six.
      return result;
    default:
      allowed = true;
      break;
  }

  if (!allowed) {
    return result;
  }

  // 1148:007e clears its prerequisite message immediately before returning
  // true, even when there was no message currently visible.
  result.notification_code = 0U;
  ++document.header.rating;
  result.promoted = true;

  // 1148:003d snapshots whether this rating's treasure condition was already
  // met, clears all VIP/transient prerequisite state, then shows the rating
  // modal. b92a/b92b/b92c/b92d intentionally survive.
  tail.b922_flag = original_rating_treasure_condition(document) ? 1U : 0U;
  tail.b923 = 0U;
  tail.b928 = 0U;
  tail.b924 = -1;
  tail.b929 = 0U;
  result.dialog = {
      static_cast<std::uint16_t>(0x0bd4U + document.header.rating),
      0,
      0x2710};
  return result;
}

OriginalRatingConstructionResult complete_original_rating_construction(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::uint16_t facility_type) noexcept {
  OriginalRatingConstructionResult result{};
  auto& tail = document.post_elevator;

  if (facility_type == 14U && tail.b92a == 0U) {
    tail.b92a = 1U;
    result.changed = true;
  } else if (facility_type == 5U && tail.b92b == 0U) {
    tail.b92b = 1U;
    result.changed = true;
  }

  if (tail.b929 != 0U) {
    tail.b929 = 0U;
    result.changed = true;
  }

  if (tail.b922_flag != 0U ||
      !original_rating_treasure_condition(document)) {
    return result;
  }

  const auto treasure = award_original_rating_treasure(document, part);
  result.changed = result.changed || treasure.changed;
  result.treasure_awarded = treasure.treasure_awarded;
  result.treasure_value = treasure.treasure_value;
  result.dialog = treasure.dialog;
  return result;
}

OriginalRatingConstructionResult award_original_rating_treasure(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept {
  OriginalRatingConstructionResult result{};
  // 1148:020f selects signed PART words at +0xa8/+0xaa/+0xac for ratings
  // one/two/three, displays DIALOG/3040 with WAVE/10001, and then feeds the
  // amount through 1178:076f's shared 99,999,999 balance cap before adding it
  // to both balance and the header's other-income accumulator.
  document.post_elevator.b922_flag = 1U;
  result.changed = true;
  if (document.header.rating < 1U || document.header.rating > 3U) {
    return result;
  }
  result.treasure_awarded = true;
  const std::size_t value_index = 42U + document.header.rating;
  if (value_index < part.words_52_to_ac.size()) {
    result.treasure_value = static_cast<std::int16_t>(
        part.words_52_to_ac[value_index]);
  }
  result.treasure_value = cap_original_positive_delta(
      document.header.balance, result.treasure_value);
  if (result.treasure_value != 0) {
    document.header.balance = wrapping_add(
        document.header.balance, result.treasure_value);
    document.header.other_income = wrapping_add(
        document.header.other_income, result.treasure_value);
  }
  result.dialog = {3040U, result.treasure_value, 10001};
  return result;
}

void reset_original_quarter_finance(
    OriginalTdtDocument& document) noexcept {
  // Exact 1060:003a intentionally differs from the fresh-state 1060:0000
  // reset: it preserves the population band while clearing only the eleven
  // income and eleven maintenance dwords.
  document.header.last_quarter_money = document.header.balance;
  document.post_elevator.finance.income_by_category.fill(0);
  document.post_elevator.finance.total_income = 0;
  document.post_elevator.finance.maintenance_by_category.fill(0);
  document.post_elevator.finance.total_maintenance = 0;
  document.header.other_income = 0;
  document.header.construction_costs = 0;
}

std::int16_t original_finance_category_for_type(
    std::uint16_t facility_type) noexcept {
  // 1060:08be indexes a 33-entry switch covering facility types 3..35.
  switch (facility_type) {
    case 7:
      return 0;
    case 3:
      return 1;
    case 4:
      return 2;
    case 5:
      return 3;
    case 10:
      return 4;
    case 12:
      return 5;
    case 6:
      return 6;
    case 29:
    case 30:
      return 7;
    case 18:
    case 19:
    case 34:
    case 35:
      return 8;
    case 9:
      return 9;
    default:
      return -1;
  }
}

std::int16_t original_maintenance_category_for_type(
    std::uint16_t facility_type) noexcept {
  // 1060:0958 performs a ten-key parallel lookup. The key order at CS:09c1
  // is 1,14,15,20,24,27,31,42,43,44; the adjacent jump table supplies these
  // deliberately non-sequential results.
  switch (facility_type) {
    case 24:
      return 0;
    case 1:
      return 1;
    case 42:
      return 2;
    case 43:
      return 3;
    case 27:
      return 4;
    case 44:
      return 5;
    case 20:
      return 6;
    case 31:
      return 7;
    case 15:
      return 8;
    case 14:
      return 9;
    default:
      return -1;
  }
}

bool clear_original_population_for_type(
    OriginalTdtDocument& document,
    std::uint16_t facility_type) noexcept {
  const auto category = original_finance_category_for_type(facility_type);
  if (category < 0) {
    return false;
  }
  auto& finance = document.post_elevator.finance;
  auto& category_population =
      finance.population_by_category[static_cast<std::size_t>(category)];
  finance.total_population =
      wrapping_subtract(finance.total_population, category_population);
  category_population = 0;
  return true;
}

void add_original_population_for_type(OriginalTdtDocument& document,
                                      std::uint16_t facility_type,
                                      std::int16_t amount) noexcept {
  auto& finance = document.post_elevator.finance;
  const auto signed_amount = static_cast<std::int32_t>(amount);
  const auto category = original_finance_category_for_type(facility_type);
  if (category >= 0) {
    auto& category_population =
        finance.population_by_category[static_cast<std::size_t>(category)];
    category_population = wrapping_add(category_population, signed_amount);
  }
  finance.total_population =
      wrapping_add(finance.total_population, signed_amount);
}

void add_original_income_for_type(OriginalTdtDocument& document,
                                  std::uint16_t facility_type,
                                  std::int32_t amount) noexcept {
  const auto category = original_finance_category_for_type(facility_type);
  if (category < 0) {
    document.header.other_income =
        wrapping_add(document.header.other_income, amount);
    return;
  }
  auto& finance = document.post_elevator.finance;
  auto& category_income =
      finance.income_by_category[static_cast<std::size_t>(category)];
  category_income = wrapping_add(category_income, amount);
  finance.total_income = wrapping_add(finance.total_income, amount);
}

void add_original_maintenance_for_type(OriginalTdtDocument& document,
                                       std::uint16_t facility_type,
                                       std::int32_t amount) noexcept {
  auto& finance = document.post_elevator.finance;
  const auto category = original_maintenance_category_for_type(facility_type);
  if (category >= 0) {
    auto& category_maintenance =
        finance.maintenance_by_category[static_cast<std::size_t>(category)];
    category_maintenance = wrapping_add(category_maintenance, amount);
  }
  finance.total_maintenance =
      wrapping_add(finance.total_maintenance, amount);
}

void add_original_rent_income(OriginalTdtDocument& document,
                              const OriginalYenTable& rent_income,
                              std::uint16_t facility_type,
                              std::uint16_t rent_tier) noexcept {
  auto amount = original_rent_amount(rent_income, facility_type, rent_tier);
  amount = cap_original_positive_delta(document.header.balance, amount);
  if (amount == 0) {
    return;
  }
  document.header.balance = wrapping_add(document.header.balance, amount);
  add_original_income_for_type(document, facility_type, amount);
}

void remove_original_rent_income(OriginalTdtDocument& document,
                                 const OriginalYenTable& rent_income,
                                 std::uint16_t facility_type,
                                 std::uint16_t rent_tier) noexcept {
  const auto amount =
      original_rent_amount(rent_income, facility_type, rent_tier);
  if (amount == 0) {
    return;
  }
  document.header.balance = wrapping_subtract(document.header.balance, amount);
  add_original_income_for_type(document, facility_type,
                               wrapping_subtract(0, amount));
}

OriginalMetroPulseResult pulse_original_metro_effects(
    OriginalTdtDocument& document) noexcept {
  OriginalMetroPulseResult result{};
  const auto event_flags = load_original_header_word(document, 60U); // b406
  const auto metro_key = std::bit_cast<std::int16_t>(
      load_original_header_word(document, 30U)); // b3e8
  if ((event_flags & 9U) != 0U || metro_key < 0) {
    return result;
  }

  // rand() is non-negative, so the original CDQ/XOR/SUB absolute-value
  // sequence is an identity before signed IDIV by 100.
  if (next_original_random(document) % 100U != 0U) {
    return result;
  }

  for (auto& floor : document.floors) {
    for (auto& tenant : floor.tenants) {
      if (tenant.type < 31 || tenant.type > 33) {
        continue;
      }
      const auto replacement =
          original_tenant_variant_word(tenant) == 0U ? 2U : 0U;
      store_original_tenant_variant_word(
          tenant, static_cast<std::uint16_t>(replacement));
      result.play_transition_sound =
          result.play_transition_sound || replacement == 2U;
      ++result.touched;
    }
  }
  return result;
}

OriginalAnnualEffectStartResult start_original_annual_effect(
    OriginalTdtDocument& document) noexcept {
  auto& state = document.post_elevator.version_18_dd6c;
  if (state.size() < 8U) {
    // Pre-0x18 saves do not persist this process-local state, but the Win16
    // runtime still owns the same zero-initialized eight-byte object.
    state.resize(8U, std::byte{0});
  }
  if (state[0] != std::byte{0}) {
    return {};
  }

  state[0] = std::byte{1};
  state[1] = std::byte{0};
  // 1048:00ad initializes DS:7756..775c from BITMAP/904's 140x48 DIB,
  // making DS:775a the source rectangle's right edge (140). 11b8:0028
  // starts the effect with that full width just inside the 3000-pixel world.
  store_runtime_word(state, 4U, 3000U - 140U);
  store_runtime_word(state, 6U, static_cast<std::uint16_t>(4320U - 2124U));
  return {true, 7U};
}

bool advance_original_annual_effect(OriginalTdtDocument& document) noexcept {
  auto& state = document.post_elevator.version_18_dd6c;
  if (state.size() < 8U || state[0] == std::byte{0}) {
    return false;
  }

  const auto x = static_cast<std::uint16_t>(
      load_runtime_word(state, 4U) - 10U);
  const auto y = static_cast<std::uint16_t>(
      load_runtime_word(state, 6U) + 1U);
  store_runtime_word(state, 4U, x);
  store_runtime_word(state, 6U, y);

  // 11b8:0060 adds BITMAP/904's 140-pixel right edge before signed JG, so
  // the state is cleared only once the complete sprite has moved off-screen.
  const auto right = static_cast<std::uint16_t>(x + 140U);
  if (std::bit_cast<std::int16_t>(right) <= 0) {
    std::fill(state.begin(), state.begin() + 8U, std::byte{0});
  }
  return true;
}

std::optional<std::int32_t> select_original_ambient_sound(
    OriginalTdtDocument& document,
    bool sound_enabled,
    std::int32_t view_x,
    std::int32_t view_y,
    std::int32_t client_width,
    std::int32_t client_height) noexcept {
  const auto world_mode_flags = static_cast<std::uint8_t>(
      load_original_header_word(document, 60U));  // DS:b406 low byte
  if (!sound_enabled || (world_mode_flags & 0x09U) != 0U) {
    return std::nullopt;
  }

  // 11c8:03ab consumes no random number until both static gates pass. Its
  // one-in-sixteen branch consumes exactly this first value.
  if (!original_should_attempt_ambient_sound(
          true, 0U,
          std::bit_cast<std::int16_t>(next_original_random(document)))) {
    return std::nullopt;
  }

  const auto probe_index =
      static_cast<std::uint16_t>(next_original_random(document) % 6U);
  // 11c8:0671 creates the visible cell cache one cell past both client edges.
  const auto tower_width = static_cast<std::int16_t>(
      (client_width + 7) / 8 + 1);
  const auto tower_height = static_cast<std::int16_t>(
      (client_height + 35) / 36 + 1);
  const auto aligned_view_x = view_x - view_x % 8;
  const auto aligned_view_y = view_y - view_y % 36;
  const auto ground_coordinate = static_cast<std::int16_t>(
      119 - aligned_view_y / 36);
  const auto probe = original_ambient_probe(
      probe_index, tower_width, tower_height, ground_coordinate);
  if (!probe) {
    return std::nullopt;
  }

  const auto global_x = aligned_view_x / 8 + probe->column;
  const OriginalTdtTenant* tenant = nullptr;
  if (probe->coordinate >= 0 &&
      static_cast<std::size_t>(probe->coordinate) < document.floors.size()) {
    for (const auto& candidate :
         document.floors[static_cast<std::size_t>(probe->coordinate)].tenants) {
      if (global_x >= static_cast<std::int32_t>(candidate.left) &&
          global_x < static_cast<std::int32_t>(candidate.right)) {
        tenant = &candidate;
        break;
      }
    }
  }

  OriginalFacilitySoundRecord facility{};
  const OriginalFacilitySoundRecord* facility_pointer = nullptr;
  if (tenant) {
    facility.type = tenant->type;
    facility.phase = std::bit_cast<std::int8_t>(tenant->status);
    facility.linked_index = original_tenant_variant_word(*tenant);
    facility_pointer = &facility;
  }

  // 11c8:07d2 multiplies the commercial link by 18 and indexes DS:b7e2,
  // the fixed Retail-record table. It is not the separate 16-byte people
  // table used by the live-person simulation.
  std::array<OriginalLinkedSoundRecord, 512> linked_records{};
  for (std::size_t index = 0; index < linked_records.size(); ++index) {
    const auto& bytes = document.retail[index].exact_bytes;
    linked_records[index] = {
        std::to_integer<std::uint8_t>(bytes[2]),
        std::to_integer<std::uint8_t>(bytes[9])};
  }

  std::array<OriginalServiceSoundRecord, 16> service_records{};
  for (std::size_t index = 0; index < service_records.size(); ++index) {
    const auto& bytes = document.post_elevator.dc24_records[index];
    service_records[index] = {
        std::bit_cast<std::int8_t>(
            std::to_integer<std::uint8_t>(bytes[0])),
        std::bit_cast<std::int8_t>(
            std::to_integer<std::uint8_t>(bytes[1]))};
  }

  const auto event = static_cast<std::int16_t>(
      original_sound_event_for_facility(
          probe->coordinate, facility_pointer, linked_records,
          service_records));
  if (event >= 0) {
    // 11c8:0426 only calls rand() for these five switch branches. Fixed and
    // pass-through events must leave the PRNG state untouched.
    std::int16_t random_value{};
    switch (event) {
      case 6:
      case 9:
      case 10:
      case 11:
      case 12:
        random_value = std::bit_cast<std::int16_t>(
            next_original_random(document));
        break;
      default:
        break;
    }
    return original_wave_resource_for_sound_event(event, random_value);
  }

  const bool annual_effect_active =
      !document.post_elevator.version_18_dd6c.empty() &&
      document.post_elevator.version_18_dd6c[0] != std::byte{0};
  return original_contextual_wave_resource(
      event, true, annual_effect_active, document.header.current_day,
      static_cast<std::int8_t>(
          original_day_phase(document.header.frame_time)));
}

std::optional<std::int32_t> select_original_facility_sound(
    OriginalTdtDocument& document,
    bool sound_enabled,
    std::int16_t floor_number,
    std::size_t tenant_index) noexcept {
  // Exact 11c8:03fb -> 06b6 -> 0426 path used by 1100:03ac after its
  // temporary world snapshot and before DIALOGBOXPARAM. The zero second
  // argument to 0426 suppresses contextual -1 sounds for an empty location.
  if (floor_number < 0 ||
      floor_number >= static_cast<std::int16_t>(document.floors.size())) {
    return std::nullopt;
  }
  const auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  if (tenant_index >= floor.tenants.size()) return std::nullopt;
  const auto& tenant = floor.tenants[tenant_index];

  const OriginalFacilitySoundRecord facility{
      tenant.type, std::bit_cast<std::int8_t>(tenant.status),
      original_tenant_variant_word(tenant)};
  std::array<OriginalLinkedSoundRecord, 512> linked_records{};
  for (std::size_t index = 0; index < linked_records.size(); ++index) {
    const auto& bytes = document.retail[index].exact_bytes;
    linked_records[index] = {
        std::to_integer<std::uint8_t>(bytes[2]),
        std::to_integer<std::uint8_t>(bytes[9])};
  }
  std::array<OriginalServiceSoundRecord, 16> service_records{};
  for (std::size_t index = 0; index < service_records.size(); ++index) {
    const auto& bytes = document.post_elevator.dc24_records[index];
    service_records[index] = {
        std::bit_cast<std::int8_t>(
            std::to_integer<std::uint8_t>(bytes[0])),
        std::bit_cast<std::int8_t>(
            std::to_integer<std::uint8_t>(bytes[1]))};
  }

  const auto event = static_cast<std::int16_t>(
      original_sound_event_for_facility(
          floor_number, &facility, linked_records, service_records));
  // 11c8:0426 checks DS:02a8 before any resource-variant rand() call.
  if (!sound_enabled || event < 0) return std::nullopt;
  std::int16_t random_value{};
  switch (event) {
    case 6:
    case 9:
    case 10:
    case 11:
    case 12:
      random_value = std::bit_cast<std::int16_t>(
          next_original_random(document));
      break;
    default:
      break;
  }
  return original_wave_resource_for_sound_event(event, random_value);
}

void reset_original_entertainment_for_day(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) {
  // Exact scheduled entertainment root 1180:05af.
  // 1180:05be performs 11f0:0016 before touching population or service state.
  activate_all_original_pending_facilities_for_schedule(document);
  (void)clear_original_population_for_type(document, 18U);
  (void)clear_original_population_for_type(document, 29U);

  for (auto& record : document.post_elevator.dc24_records) {
    if (std::bit_cast<std::int8_t>(
            std::to_integer<std::uint8_t>(record[0])) < 0) {
      continue;
    }

    if (std::bit_cast<std::int8_t>(
            std::to_integer<std::uint8_t>(record[7])) >= 0) {
      // 1180:0b3c selects one of four PART words from the signed quotient of
      // dc2d / 3. Movie selectors below seven use offsets 0x88..0x8e;
      // selectors seven and above use 0x80..0x86.
      const auto age = std::bit_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(record[9]));
      const auto band = static_cast<std::int16_t>(age) / 3;
      const std::size_t base =
          std::to_integer<std::uint8_t>(record[7]) < 7U ? 27U : 23U;
      const std::size_t capacity_index =
          base + (band == 0 ? 0U : band == 1 ? 1U : band == 2 ? 2U : 3U);
      const auto capacity = static_cast<std::uint8_t>(
          part.words_52_to_ac[capacity_index]);
      record[4] = static_cast<std::byte>(capacity);
      record[5] = static_cast<std::byte>(capacity);
      add_original_population_for_type(
          document, 18U,
          static_cast<std::int16_t>(static_cast<std::int8_t>(capacity)) * 2);
    } else {
      record[4] = std::byte{0};
      record[5] = std::byte{0x32};
      add_original_population_for_type(document, 29U, 0x32);
    }

    auto age = std::to_integer<std::uint8_t>(record[9]);
    if (std::bit_cast<std::int8_t>(age) < 0x7f) {
      age = static_cast<std::uint8_t>(age + 1U);
      record[9] = static_cast<std::byte>(age);
    }
    record[8] = std::byte{0};
    record[10] = std::byte{0};
    record[11] = std::byte{0};
  }
}

void begin_original_entertainment_arrivals(
    OriginalTdtDocument& document,
    std::uint16_t side,
    std::uint16_t service_group) noexcept {
  // Exact 1180:06a8 arrival-side transition.
  for (auto& record : document.post_elevator.dc24_records) {
    if (std::bit_cast<std::int8_t>(
            std::to_integer<std::uint8_t>(record[0])) < 0 ||
        !original_entertainment_record_matches(record, service_group)) {
      continue;
    }
    if (record[6] == std::byte{0}) {
      record[6] = std::byte{1};
    }
    mutate_original_entertainment_people(
        document, record, side, [](OriginalTdtPersonRecord& person) {
          person.exact_bytes[5] = std::byte{0x20};
        });
    mark_original_entertainment_tenants(document, record);
  }
}

void advance_original_entertainment_show(
    OriginalTdtDocument& document,
    std::uint16_t /*unused_side*/,
    std::uint16_t service_group) noexcept {
  // Exact 1180:0826 show-state transition.
  for (auto& record : document.post_elevator.dc24_records) {
    if (std::bit_cast<std::int8_t>(
            std::to_integer<std::uint8_t>(record[0])) < 0 ||
        !original_entertainment_record_matches(record, service_group) ||
        std::bit_cast<std::int8_t>(
            std::to_integer<std::uint8_t>(record[6])) < 2) {
      continue;
    }
    record[6] = std::byte{3};
    mark_original_entertainment_tenants(document, record);
  }
}

OriginalIncomeStatusResult finish_original_entertainment_phase(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::uint16_t side,
    std::uint16_t service_group) {
  // Exact 1180:090a departure, attendance, dirty-state, and income pass.
  OriginalIncomeStatusResult result{};
  for (std::size_t service_index = 0;
       service_index < document.post_elevator.dc24_records.size();
       ++service_index) {
    auto& record = document.post_elevator.dc24_records[service_index];
    if (std::bit_cast<std::int8_t>(
            std::to_integer<std::uint8_t>(record[0])) < 0 ||
        !original_entertainment_record_matches(record, service_group)) {
      continue;
    }

    if (side == 1U) {
      record[6] = std::byte{0};
      // 1178:126c suppresses all four service-income families on the last
      // day of the 60- and 84-day event cycles.
      if (document.header.current_day % 60 != 59 &&
          document.header.current_day % 84 != 83) {
        if (service_group == 1U) {
          add_original_entertainment_income(
              document, 18U, original_movie_income(record, part));
          result.codes.push_back(7U);
        } else if (service_group == 0U && record[11] != std::byte{0}) {
          add_original_entertainment_income(document, 29U, 200);
          result.codes.push_back(8U);
        }
      }
    }

    mutate_original_entertainment_people(
        document, record, side,
        [&](OriginalTdtPersonRecord& person) {
          if (person.exact_bytes[5] != std::byte{3}) {
            return;
          }
          person.exact_bytes[5] =
              service_group != 0U &&
                      original_day_phase(document.header.frame_time) < 4
                  ? std::byte{1}
                  : std::byte{5};
          record[10] = static_cast<std::byte>(
              std::to_integer<std::uint8_t>(record[10]) - 1U);
        });
    mark_original_entertainment_tenants(document, record);

    if (side == 0U) {
      record[6] = record[10] != std::byte{0} ? std::byte{2}
                                             : std::byte{1};
    }
  }
  return result;
}

void reset_original_commercial_for_day(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) {
  // Exact leading 11f0:0016 and 11a8:14c9 calls.
  activate_all_original_pending_facilities_for_schedule(document);
  document.post_elevator.dynamic_dd5c.fill(std::byte{0});
  document.post_elevator.dynamic_dd60.fill(std::byte{0});
  document.post_elevator.dynamic_dd64.fill(std::byte{0});
  (void)clear_original_population_for_type(document, 12U);
  (void)clear_original_population_for_type(document, 10U);

  for (std::size_t index = 0; index < document.retail.size(); ++index) {
    auto& record = document.retail[index].exact_bytes;
    if (std::bit_cast<std::int8_t>(
            std::to_integer<std::uint8_t>(record[0])) < 0) {
      continue;
    }
    if (record[1] == std::byte{0xff}) {
      record[0] = std::byte{0xff};
      store_original_header_word(
          document, 46U,
          static_cast<std::uint16_t>(
              load_original_header_word(document, 46U) - 1U));
      continue;
    }

    const auto location =
        original_tenant_location(document, record[0], record[1]);
    if (!location || location->tenant->type == 6) {
      continue;
    }
    initialize_original_commercial_record(
        document, part, index, *location->tenant);
  }
}

void reset_original_restaurants_for_evening(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept {
  (void)clear_original_population_for_type(document, 6U);
  visit_original_commercial_records(
      document,
      [](const auto&, const OriginalTdtTenant& tenant) {
        return tenant.type == 6;
      },
      [&](std::size_t index, OriginalTdtTenant& tenant) {
        initialize_original_commercial_record(document, part, index, tenant);
      });
}

OriginalIncomeStatusResult close_original_nonrestaurant_commercial(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) {
  // Exact 11a8:0554 scans all 512 commercial records, rejects negative owner
  // floors, missing tenant links, inactive records, and Restaurant (type 6),
  // then delegates each surviving record to 11a8:06b2.
  OriginalIncomeStatusResult result{};
  visit_original_commercial_records(
      document,
      [](const auto& record, const OriginalTdtTenant& tenant) {
        return record[2] != std::byte{0xff} && tenant.type != 6;
      },
      [&](std::size_t index, OriginalTdtTenant& tenant) {
        const auto code =
            close_original_commercial_record(document, part, index, tenant);
        if (code) result.codes.push_back(*code);
      });
  return result;
}

OriginalIncomeStatusResult close_original_restaurants_for_night(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) {
  // Exact 11a8:0603 is the complementary 512-record pass: it keeps only live
  // Restaurant (type 6) links before invoking the same 11a8:06b2 close path.
  OriginalIncomeStatusResult result{};
  visit_original_commercial_records(
      document,
      [](const auto& record, const OriginalTdtTenant& tenant) {
        return record[2] != std::byte{0xff} && tenant.type == 6;
      },
      [&](std::size_t index, OriginalTdtTenant& tenant) {
        const auto code =
            close_original_commercial_record(document, part, index, tenant);
        if (code) result.codes.push_back(*code);
      });
  return result;
}

void advance_original_tenants_at_midnight(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) noexcept {
  const bool three_day_pass = document.header.current_day % 3 == 0;
  // 1130:0000 completes all three passes for one floor before advancing to
  // the next floor pointer. Preserve that ordering because rent capping is
  // balance-sensitive.
  for (auto& floor : document.floors) {
    for (std::size_t index = 0; index < floor.tenants.size(); ++index) {
      update_original_tenant_satisfaction(document, part, floor, index);
    }
    if (!three_day_pass) {
      continue;
    }
    for (std::size_t index = 0; index < floor.tenants.size(); ++index) {
      run_original_three_day_tenant_departure(
          document, rent_income, floor, index);
    }
    for (auto& tenant : floor.tenants) {
      run_original_three_day_tenant_income(
          document, rent_income, tenant);
    }
  }
}

void advance_original_hotels_for_evening(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept {
  // 1130:0109 deliberately performs two ordered loops per floor. The first
  // calculates satisfaction and advances checkout age; the second pairs
  // zero/two satisfaction records and resets the selected guest metrics.
  for (auto& floor : document.floors) {
    for (std::size_t index = 0; index < floor.tenants.size(); ++index) {
      const auto type = floor.tenants[index].type;
      if (type < 3 || type > 5) {
        continue;
      }
      update_original_tenant_satisfaction(document, part, floor, index);
      advance_original_hotel_checkout_state(
          document, floor.tenants[index]);
    }
    for (std::size_t index = 0; index < floor.tenants.size(); ++index) {
      const auto type = floor.tenants[index].type;
      if (type >= 3 && type <= 5) {
        advance_original_hotel_pairing(document, floor, index);
      }
    }
  }
}

void refresh_original_map_tenant_satisfaction(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept {
  // Exact 1130:00b5 pass called by 11d0:0000 when Map overlay one is
  // selected. It recalculates every tenant in floor/record order, but does
  // not perform the three-day departure, income, or Hotel pairing passes.
  for (auto& floor : document.floors) {
    for (std::size_t index = 0; index < floor.tenants.size(); ++index) {
      update_original_tenant_satisfaction(document, part, floor, index);
    }
  }
}

std::int16_t original_tenant_information_performance(
    const OriginalTdtDocument& document,
    std::int16_t floor,
    std::size_t tenant_index) noexcept {
  if (floor < 0 ||
      floor >= static_cast<std::int16_t>(document.floors.size())) {
    return -1;
  }
  return original_tenant_performance(
      document, document.floors[static_cast<std::size_t>(floor)],
      tenant_index);
}

void refresh_original_tenant_information_satisfaction(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::int16_t floor,
    std::size_t tenant_index) noexcept {
  if (floor < 0 ||
      floor >= static_cast<std::int16_t>(document.floors.size())) {
    return;
  }
  auto& record = document.floors[static_cast<std::size_t>(floor)];
  if (tenant_index >= record.tenants.size()) return;
  update_original_tenant_satisfaction(
      document, part, record, tenant_index);
}

void charge_original_three_day_maintenance(
    OriginalTdtDocument& document,
    const OriginalYenTable& maintenance_costs,
    const OriginalPartTable& part) noexcept {
  // 1178:0b44 visits all 120 floor allocations and every active record.
  for (std::size_t floor_index = 0; floor_index < document.floors.size();
       ++floor_index) {
    for (const auto& tenant : document.floors[floor_index].tenants) {
      if (tenant.type < 0) {
        continue;
      }
      const auto type = static_cast<std::uint16_t>(tenant.type);
      if (type != 24U && type != 25U && type != 26U) {
        charge_original_scaled_maintenance(document, maintenance_costs, type,
                                           1);
        continue;
      }

      // 10a0:133b excludes Lobby pieces on the additional Lobby stories.
      if (floor_index >= 11U &&
          floor_index < 10U + document.header.lobby_height) {
        continue;
      }
      const std::uint16_t raw_rate =
          document.header.rating < 3U
              ? part.words_52_to_ac[37]
              : (document.header.rating < 4U
                     ? part.words_52_to_ac[38]
                     : part.words_52_to_ac[39]);
      const auto rate = static_cast<std::int32_t>(
          std::bit_cast<std::int16_t>(raw_rate));
      if (rate == 0) {
        continue;
      }

      // 1178:0a6a subtracts the 16-bit endpoints before sign extension, then
      // uses signed IMUL and IDIV. All three Lobby-part types are accounted
      // under maintenance type 24.
      const auto width_word = static_cast<std::uint16_t>(
          tenant.right - tenant.left);
      const auto width = static_cast<std::int32_t>(
          std::bit_cast<std::int16_t>(width_word));
      const auto amount = wrapping_multiply(width, rate) / 10;
      charge_original_maintenance(document, 24U, amount);
    }
  }

  for (const auto& elevator : document.elevators) {
    if (elevator.used == 0U) {
      continue;
    }
    const auto cars = static_cast<std::int16_t>(
        std::bit_cast<std::int8_t>(elevator.cars));
    switch (elevator.type) {
      case 0:
        charge_original_scaled_maintenance(document, maintenance_costs, 42U,
                                           cars);
        break;
      case 1:
        charge_original_scaled_maintenance(document, maintenance_costs, 1U,
                                           cars);
        break;
      case 2:
        charge_original_scaled_maintenance(document, maintenance_costs, 43U,
                                           cars);
        break;
      default:
        break;
    }
  }

  for (const auto& stair : document.post_elevator.stairs_bd70) {
    if (stair.used == 0U) {
      continue;
    }
    const auto signed_shape = static_cast<std::int16_t>(
        std::bit_cast<std::int8_t>(stair.shape));
    const auto multiplier = static_cast<std::int16_t>(
        (signed_shape >> 1) + 1);
    const auto type = (stair.shape & 1U) == 0U ? 27U : 22U;
    charge_original_scaled_maintenance(document, maintenance_costs, type,
                                       multiplier);
  }
}

namespace {

bool original_facility_damage_protected(std::int8_t type) noexcept {
  switch (type) {
    case 14:
    case 15:
    case 24:
    case 25:
    case 26:
    case 31:
    case 32:
    case 33:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
    case 45:
      return true;
    default:
      return false;
  }
}

void convert_original_tenant_after_damage(OriginalTdtTenant& tenant,
                                          bool byte_swapped,
                                          std::uint16_t flags) noexcept {
  // Literal 11f8:3959. A zero flag produces the damaged type 47 record;
  // replacement demolition instead restores a type-zero/status-two span.
  tenant.type = flags == 0U ? 47 : 0;
  tenant.exact_bytes[4] = static_cast<std::byte>(tenant.type);
  tenant.status = flags == 0U ? 0U : 2U;
  tenant.exact_bytes[5] = static_cast<std::byte>(tenant.status);
  tenant.variant = 0U;
  tenant.preserved_07_to_0f[0] = std::byte{0};
  tenant.exact_bytes[6] = std::byte{0};
  tenant.exact_bytes[7] = std::byte{0};
  store_original_tenant_exact_byte(tenant, 12U, 0xffU);
  store_original_tenant_exact_byte(tenant, 13U, 1U);
  store_original_tenant_exact_byte(tenant, 14U, 1U);
  store_original_tenant_exact_byte(tenant, 15U, 0xffU);
  store_original_tenant_exact_byte(tenant, 16U, 4U);
  store_original_tenant_exact_byte(tenant, 17U, 0U);
  (void)byte_swapped;
}

void merge_original_floor_type(OriginalTdtFloor& floor,
                               std::int8_t type,
                               bool byte_swapped) {
  // Exact persisted translation of 11f8:3a87: merge each adjacent pair of
  // the requested type, copy the second right edge to the first, shift every
  // following 18-byte record left, decrement the live count, and retry the
  // same index. std::vector::erase performs that identical record shift.
  std::size_t index = 0U;
  while (index + 1U < floor.tenants.size()) {
    auto& current = floor.tenants[index];
    const auto& next = floor.tenants[index + 1U];
    if (current.type != type || next.type != type) {
      ++index;
      continue;
    }
    current.right = next.right;
    store_exact_word(current.exact_bytes, 2U, current.right, byte_swapped);
    floor.tenants.erase(floor.tenants.begin() +
                        static_cast<std::ptrdiff_t>(index + 1U));
  }
}

void rebuild_original_floor_after_damage(OriginalTdtDocument& document,
                                         std::int16_t floor_number) {
  // Exact persisted portion of 11f8:3a31: coalesce adjacent type-0 records,
  // then type-24 records, and rebuild the live-key lookup. The native host
  // performs the original conditional redraw after this model mutation.
  if (floor_number < 0 ||
      static_cast<std::size_t>(floor_number) >= document.floors.size()) {
    return;
  }
  auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  merge_original_floor_type(floor, 0, document.header.byte_swapped);
  merge_original_floor_type(floor, 24, document.header.byte_swapped);
  // 1228:0e30 overwrites only keys represented by live records.
  for (std::size_t index = 0; index < floor.tenants.size(); ++index) {
    const auto key = std::to_integer<std::uint8_t>(
        floor.tenants[index].exact_bytes[12]);
    if (key < floor.tenant_index.size()) {
      floor.tenant_index[key] = static_cast<std::uint16_t>(index);
    }
  }
}

std::optional<std::size_t> original_tenant_at_damage_x(
    const OriginalTdtFloor& floor,
    std::int16_t x) noexcept {
  for (std::size_t index = 0; index < floor.tenants.size(); ++index) {
    const auto left = std::bit_cast<std::int16_t>(floor.tenants[index].left);
    const auto right = std::bit_cast<std::int16_t>(floor.tenants[index].right);
    if (left <= x && right > x) return index;
  }
  return std::nullopt;
}

std::optional<std::pair<std::int16_t, std::size_t>>
original_tenant_from_floor_key(OriginalTdtDocument& document,
                               std::int16_t floor_number,
                               std::int16_t key) noexcept {
  if (floor_number < 0 || key < 0 ||
      static_cast<std::size_t>(floor_number) >= document.floors.size() ||
      static_cast<std::size_t>(key) >= OriginalTdtFloor::kIndexCapacity) {
    return std::nullopt;
  }
  auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  const auto tenant_index = floor.tenant_index[static_cast<std::size_t>(key)];
  if (tenant_index >= floor.tenants.size()) return std::nullopt;
  return std::pair<std::int16_t, std::size_t>{floor_number, tenant_index};
}

std::uint16_t original_damage_link_id(const OriginalTdtDocument& document,
                                      std::int16_t floor_number,
                                      const OriginalTdtTenant& tenant) noexcept {
  auto link_floor = floor_number;
  auto key = std::bit_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(tenant.exact_bytes[12]));
  switch (tenant.type) {
    case 18:
    case 19:
    case 29:
    case 30:
    case 34:
    case 35: {
      const auto linked = original_tenant_variant_word(tenant);
      if (linked >= document.post_elevator.dc24_records.size()) {
        return 0xffffU;
      }
      const auto& record = document.post_elevator.dc24_records[linked];
      link_floor = std::bit_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(record[0]));
      key = std::bit_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(record[2]));
      break;
    }
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
      link_floor = 109;
      key = std::bit_cast<std::int16_t>(
          load_original_header_word(document, 34U));
      break;
    default:
      break;
  }
  if (key == -1) return 0xffffU;
  return static_cast<std::uint16_t>(
      static_cast<std::int32_t>(link_floor) * 94 + key);
}

void remove_original_dd34_damage_link(OriginalTdtDocument& document,
                                      std::uint16_t link) noexcept {
  if (document.header.format_version < 0x23U) return;
  auto count = std::min<std::uint16_t>(
      document.header.tenant_link_count, 20U);
  auto bytes = std::span<std::byte>(document.post_elevator.dce4_or_dd34);
  for (std::size_t index = 0; index < count; ++index) {
    if (load_exact_word(bytes, index * 2U,
                        document.header.byte_swapped) != link) {
      continue;
    }
    for (std::size_t source = index + 1U; source < count; ++source) {
      store_exact_word(bytes, (source - 1U) * 2U,
                       load_exact_word(bytes, source * 2U,
                                       document.header.byte_swapped),
                       document.header.byte_swapped);
    }
    if (index < document.tenant_link_names.size()) {
      document.tenant_link_names.erase(
          document.tenant_link_names.begin() +
          static_cast<std::ptrdiff_t>(index));
    }
    --count;
    store_exact_word(bytes, static_cast<std::size_t>(count) * 2U, 0U,
                     document.header.byte_swapped);
    document.header.tenant_link_count = count;
    store_original_header_word(document, 58U, count);
    return;
  }
}

void append_original_damage_cleanup_result(
    OriginalFacilityDamageResult& result,
    const OriginalFacilityPeopleCleanupResult& cleanup) {
  if (cleanup.notification_code != 0U) {
    result.notification_codes.push_back(cleanup.notification_code);
  }
}

std::size_t convert_original_entertainment_damage(
    OriginalTdtDocument& document,
    std::uint16_t linked,
    std::uint16_t flags,
    OriginalFacilityDamageResult& result) {
  // 1180:0e79 first sweeps both linked facility owners through 1220:10af;
  // 1180:0ee3 then applies 11f8:3959 to every record in both Movie/Party
  // halves, retires their shared dc24 entry, and decrements b400 exactly once.
  if (linked >= document.post_elevator.dc24_records.size()) return 0U;
  auto& record = document.post_elevator.dc24_records[linked];

  for (std::size_t slot = 0; slot < 2U; ++slot) {
    const auto floor_number = std::bit_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(record[slot]));
    const auto key = std::bit_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(record[2U + slot]));
    const auto location = original_tenant_from_floor_key(
        document, floor_number, key);
    if (!location) continue;
    append_original_damage_cleanup_result(
        result, cleanup_original_facility_people(
                    document, location->first, location->second,
                    document.header.frame_time));
  }

  const auto records_per_half =
      std::bit_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(record[7])) >= 0
          ? 2U
          : 1U;
  std::size_t converted = 0U;
  for (std::size_t slot = 0; slot < 2U; ++slot) {
    const auto floor_number = std::bit_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(record[slot]));
    const auto key = std::bit_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(record[2U + slot]));
    const auto location = original_tenant_from_floor_key(
        document, floor_number, key);
    if (!location) continue;
    auto& floor = document.floors[static_cast<std::size_t>(location->first)];
    for (std::size_t part = 0; part < records_per_half; ++part) {
      const auto index = location->second + part;
      if (index >= floor.tenants.size()) break;
      convert_original_tenant_after_damage(
          floor.tenants[index], document.header.byte_swapped, flags);
      ++converted;
    }
  }
  record[0] = std::byte{0xfe};
  record[1] = std::byte{0xfe};
  store_original_header_word(
      document, 54U,
      static_cast<std::uint16_t>(
          load_original_header_word(document, 54U) - 1U));
  return converted;
}

std::size_t convert_original_recycling_damage(
    OriginalTdtDocument& document,
    std::int16_t floor_number,
    std::size_t tenant_index,
    std::int8_t original_type,
    std::uint16_t flags) {
  // 1088:038d converts the selected Recycling half and its vertically paired
  // type-20/type-21 record through 11f8:3959, then decrements b3f4 once.
  auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  const auto left = floor.tenants[tenant_index].left;
  convert_original_tenant_after_damage(
      floor.tenants[tenant_index], document.header.byte_swapped, flags);
  std::size_t converted = 1U;

  const auto adjacent = static_cast<std::int16_t>(
      floor_number + (original_type == 20 ? -1 : 1));
  if (adjacent < 0 ||
      static_cast<std::size_t>(adjacent) >= document.floors.size()) {
    return converted;
  }
  auto& paired_floor = document.floors[static_cast<std::size_t>(adjacent)];
  for (auto& tenant : paired_floor.tenants) {
    if (tenant.left != left || (tenant.type != 20 && tenant.type != 21)) {
      continue;
    }
    convert_original_tenant_after_damage(
        tenant, document.header.byte_swapped, flags);
    store_original_header_word(
        document, 42U,
        static_cast<std::uint16_t>(
            load_original_header_word(document, 42U) - 1U));
    ++converted;
    break;
  }
  return converted;
}

void append_original_rebuilt_floor(OriginalFacilityDamageResult& result,
                                   std::int16_t floor) {
  if (floor < 0) return;
  if (std::find(result.rebuilt_floors.begin(), result.rebuilt_floors.end(),
                floor) == result.rebuilt_floors.end()) {
    result.rebuilt_floors.push_back(floor);
  }
}

}  // namespace

OriginalFacilityDamageResult apply_original_facility_damage(
    OriginalTdtDocument& document,
    const OriginalYenTable& rent_income,
    std::int16_t floor_number,
    std::int16_t x,
    std::uint16_t flags) noexcept {
  OriginalFacilityDamageResult result{};
  if (floor_number < 0 ||
      static_cast<std::size_t>(floor_number) >= document.floors.size()) {
    result.allowed = true;
    return result;
  }
  auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  const auto found = original_tenant_at_damage_x(floor, x);
  if (!found) {
    result.allowed = true;
    return result;
  }
  const auto tenant_index = *found;
  const auto original = floor.tenants[tenant_index];
  result.original_type = original.type;

  if (original.type < 0) {
    result.alert_code = flags == 0U ? 0 : 33;
    return result;
  }
  if (original_facility_damage_protected(original.type)) {
    result.alert_code = flags == 0U ? 0 : 21;
    return result;
  }
  result.allowed = true;
  if (original.type == 0) return result;

  remove_original_dd34_damage_link(
      document, original_damage_link_id(document, floor_number, original));

  const bool entertainment =
      original.type == 18 || original.type == 19 || original.type == 29 ||
      original.type == 30 || original.type == 34 || original.type == 35;
  if (!entertainment) {
    append_original_damage_cleanup_result(
        result, cleanup_original_facility_people(
                    document, floor_number, tenant_index,
                    document.header.frame_time));
  }

  switch (original.type) {
    case 3:
    case 4:
    case 5:
      if (original.status < 0x18U) {
        add_original_population_for_type(
            document, static_cast<std::uint16_t>(original.type),
            original.type == 3 ? -1 : -2);
      }
      break;
    case 6:
    case 10:
    case 12: {
      const auto linked = original_tenant_variant_word(original);
      if (linked < document.retail.size()) {
        document.retail[linked].exact_bytes[1] = std::byte{0xff};
      }
      break;
    }
    case 7:
      if (original.status < 0x10U) {
        add_original_population_for_type(document, 7U, -6);
      }
      break;
    case 9:
      if (original.status < 0x18U) {
        add_original_population_for_type(document, 9U, -3);
        remove_original_rent_income(
            document, rent_income, 9U, original.rent_rate);
      }
      break;
    case 11: {
      const auto linked = original_tenant_variant_word(original);
      if (linked < document.post_elevator.cf9c_records.size()) {
        document.post_elevator.cf9c_records[linked][0] = std::byte{0xff};
      }
      break;
    }
    case 13: {
      const auto linked = original_tenant_variant_word(original);
      if (linked < document.post_elevator.dbfc_dwords.size()) {
        auto& record = document.post_elevator.dbfc_dwords[linked];
        record = (record & 0xffffff00U) | 0xffU;
      }
      break;
    }
    default:
      break;
  }

  if (entertainment) {
    result.converted_records = convert_original_entertainment_damage(
        document, original_tenant_variant_word(original), flags, result);
  } else if (original.type == 20 || original.type == 21) {
    result.converted_records = convert_original_recycling_damage(
        document, floor_number, tenant_index, original.type, flags);
  } else {
    convert_original_tenant_after_damage(
        floor.tenants[tenant_index], document.header.byte_swapped, flags);
    result.converted_records = 1U;
  }

  rebuild_original_floor_after_damage(document, floor_number);
  append_original_rebuilt_floor(result, floor_number);
  std::int16_t adjacent = -1;
  switch (original.type) {
    case 18:
    case 20:
    case 29:
    case 34:
      adjacent = static_cast<std::int16_t>(floor_number - 1);
      break;
    case 19:
    case 21:
    case 30:
    case 35:
      adjacent = static_cast<std::int16_t>(floor_number + 1);
      break;
    default:
      break;
  }
  if (adjacent >= 0 &&
      static_cast<std::size_t>(adjacent) < document.floors.size()) {
    rebuild_original_floor_after_damage(document, adjacent);
    append_original_rebuilt_floor(result, adjacent);
  }
  if (original.type == 11 || original.type == 44) {
    rebuild_original_parking_after_facility_change(document);
  }

  result.sound_requests.push_back({
      flags == 0U
          ? static_cast<std::int32_t>(10004U + next_original_random(document) % 2U)
          : 7003,
      0U,
      static_cast<std::uint16_t>(flags == 0U ? 2U : 4U),
      false,
  });
  result.changed = true;
  return result;
}

OriginalFacilityDamageSequenceResult apply_original_facility_damage_sequence(
    OriginalTdtDocument& document,
    const OriginalYenTable& rent_income,
    std::span<const OriginalFacilityDamageRequest> requests) noexcept {
  OriginalFacilityDamageSequenceResult sequence{};
  for (const auto& request : requests) {
    ++sequence.attempts;
    auto result = apply_original_facility_damage(
        document, rent_income, request.floor, request.x, request.flags);
    if (result.allowed) ++sequence.allowed;
    if (result.changed) ++sequence.changed;
    sequence.converted_records += result.converted_records;
    sequence.notification_codes.insert(sequence.notification_codes.end(),
                                       result.notification_codes.begin(),
                                       result.notification_codes.end());
    sequence.sound_requests.insert(sequence.sound_requests.end(),
                                   result.sound_requests.begin(),
                                   result.sound_requests.end());
  }
  return sequence;
}

OriginalEventHostPlan original_bomb_event_host_plan(
    const OriginalEventActionResult& action) noexcept {
  OriginalEventHostPlan plan{};
  const auto append = [&plan](OriginalEventHostOperation operation) {
    plan.operations[plan.operation_count++] = operation;
  };
  append(OriginalEventHostOperation::play_sounds);
  append(OriginalEventHostOperation::apply_damage);
  if (action.security_dispatch_pending) {
    append(OriginalEventHostOperation::dispatch_security);
  }
  if (action.focus_requested) {
    append(OriginalEventHostOperation::focus_coordinate);
  }
  if (action.dialog.valid()) {
    append(OriginalEventHostOperation::show_dialog);
  }
  if (action.deferred_completion != OriginalEventDeferredCompletion::none) {
    append(OriginalEventHostOperation::complete_deferred_action);
  }
  return plan;
}

OriginalEventHostPlan original_fire_event_host_plan(
    const OriginalEventActionResult& action) noexcept {
  OriginalEventHostPlan plan{};
  const auto append = [&plan](OriginalEventHostOperation operation) {
    plan.operations[plan.operation_count++] = operation;
  };
  append(OriginalEventHostOperation::apply_damage);
  append(OriginalEventHostOperation::play_sounds);
  if (action.fire_menu_enabled) {
    append(OriginalEventHostOperation::update_fire_menu);
  }
  if (action.dialog.valid()) {
    append(OriginalEventHostOperation::show_dialog);
  }
  if (action.deferred_completion != OriginalEventDeferredCompletion::none) {
    append(OriginalEventHostOperation::complete_deferred_action);
  }
  return plan;
}

OriginalFacilityHit original_facility_hit_from_client(
    const OriginalTdtDocument& document,
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept {
  // 11f8:3d2d adds the shared view POINT before 3e3e divides by 36/8.
  const int world_x = client_x + view_x;
  const int world_y = client_y + view_y;
  const int floor_number = 120 - world_y / 36 - 1;
  if (floor_number < 0 ||
      floor_number >= static_cast<int>(document.floors.size())) {
    return {};
  }
  const auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  if (floor.tenants.empty()) return {};

  const int x = world_x / 8;
  if (static_cast<int>(floor.left_edge) > x ||
      static_cast<int>(floor.right_edge) < x) {
    return {};
  }
  for (std::size_t index = 0; index < floor.tenants.size(); ++index) {
    if (static_cast<int>(floor.tenants[index].right) > x) {
      return {true, static_cast<std::int16_t>(floor_number),
              static_cast<std::int16_t>(x), index};
    }
  }
  return {};
}

OriginalFacilityClickDamageResult apply_original_facility_click_damage(
    OriginalTdtDocument& document,
    const OriginalYenTable& rent_income,
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept {
  OriginalFacilityClickDamageResult result{};
  result.hit = original_facility_hit_from_client(
      document, client_x, client_y, view_x, view_y);
  if (result.hit.hit) {
    result.damage = apply_original_facility_damage(
        document, rent_income, result.hit.floor, result.hit.x, 1U);
  }
  return result;
}

OriginalReplacementDemolitionResult apply_original_replacement_demolition(
    OriginalTdtDocument& document,
    const OriginalYenTable& rent_income,
    std::uint16_t selected_type,
    std::int16_t floor_number,
    std::int16_t left,
    std::int16_t right) noexcept {
  OriginalReplacementDemolitionResult sequence{};

  // Parallel key/target tables at CS:34f8/3510. These construction shapes
  // never run a facility demolition prepass when Shift is held.
  switch (selected_type) {
    case 0:
    case 1:
    case 22:
    case 24:
    case 27:
    case 42:
    case 43:
      return sequence;
    default:
      break;
  }

  int first_floor = floor_number;
  switch (selected_type) {
    case 18:
    case 20:
    case 29:
      --first_floor;
      break;
    case 31:
      first_floor -= 2;
      break;
    case 36:
      first_floor -= 4;
      break;
    default:
      break;
  }

  for (int candidate_floor = first_floor;
       candidate_floor <= floor_number; ++candidate_floor) {
    if (candidate_floor < 0 ||
        candidate_floor >= static_cast<int>(document.floors.size()) ||
        document.floors[static_cast<std::size_t>(candidate_floor)]
            .tenants.empty()) {
      continue;
    }
    for (int x = left; x < right; ++x) {
      ++sequence.attempts;
      const auto result = apply_original_facility_damage(
          document, rent_income, static_cast<std::int16_t>(candidate_floor),
          static_cast<std::int16_t>(x), 1U);
      if (result.alert_code != 0) {
        sequence.alert_codes.push_back(result.alert_code);
      }
      sequence.notification_codes.insert(sequence.notification_codes.end(),
                                         result.notification_codes.begin(),
                                         result.notification_codes.end());
      sequence.sound_requests.insert(sequence.sound_requests.end(),
                                     result.sound_requests.begin(),
                                     result.sound_requests.end());
      if (result.changed) ++sequence.changed;
      if (!result.allowed) {
        sequence.completed = false;
        return sequence;
      }
    }
  }
  return sequence;
}

OriginalBombEventOffer prepare_original_bomb_event(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept {
  OriginalBombEventOffer result{};

  // Literal 10c8:006e gate ordering. Failed leading gates consume no random
  // values and do not touch the persisted bomb coordinates.
  const auto event_flags = load_original_header_word(document, 60U);
  if ((static_cast<std::uint8_t>(event_flags) & 9U) != 0U ||
      load_original_header_word(document, 48U) == 0U ||
      document.header.frame_time > 0x04b0U) {
    return result;
  }

  const auto requested_word = static_cast<std::uint16_t>(
      document.header.lobby_height + 10U);
  const auto floor = select_original_event_floor(
      document, std::bit_cast<std::int16_t>(requested_word));
  store_original_header_word(document, 64U,
                             std::bit_cast<std::uint16_t>(floor));
  result.floor = floor;
  if (floor < 0 ||
      static_cast<std::size_t>(floor) >= document.floors.size()) {
    return result;
  }

  const auto& floor_record =
      document.floors[static_cast<std::size_t>(floor)];
  const auto width = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(floor_record.right_edge -
                                 floor_record.left_edge));
  if (width < 4) {
    return result;
  }

  const auto high_word = static_cast<std::uint16_t>(
      floor_record.right_edge - 4U);
  const auto x = original_random_between(
      document, std::bit_cast<std::int16_t>(floor_record.left_edge),
      std::bit_cast<std::int16_t>(high_word));
  store_original_header_word(document, 62U,
                             std::bit_cast<std::uint16_t>(x));
  result.x = x;

  std::uint16_t ransom_word{};
  switch (document.header.rating) {
    case 2U:
      ransom_word = part.words_52_to_ac[40];
      break;
    case 3U:
      ransom_word = part.words_52_to_ac[41];
      break;
    case 4U:
      ransom_word = part.words_52_to_ac[42];
      break;
    default:
      // The executable selects and stores both coordinates before its rating
      // switch. Preserve those writes and both RNG advances on this path.
      return result;
  }

  result.offered = true;
  result.ransom = std::bit_cast<std::int16_t>(ransom_word);
  result.dialog = {
      3020U,
      -static_cast<std::int32_t>(result.ransom),
      10003,
  };
  return result;
}

OriginalBombEventResolution resolve_original_bomb_event(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::uint16_t dialog_result) noexcept {
  OriginalBombEventResolution result{};

  std::uint16_t ransom_word{};
  switch (document.header.rating) {
    case 2U:
      ransom_word = part.words_52_to_ac[40];
      break;
    case 3U:
      ransom_word = part.words_52_to_ac[41];
      break;
    case 4U:
      ransom_word = part.words_52_to_ac[42];
      break;
    default:
      return result;
  }
  const auto ransom = static_cast<std::int32_t>(
      std::bit_cast<std::int16_t>(ransom_word));

  if (dialog_result == 2U) {
    // 10c8:0187 pays immediately and never raises b406's active-bomb bit.
    result.paid = true;
    result.direct_wave_resource = 10015;
    document.header.balance = wrapping_subtract(document.header.balance,
                                                ransom);
    // 1178:07e8 records non-construction spending in DS:b3d2. Construction
    // purchases instead use 1178:0703 and DS:b3d6.
    document.header.other_income = wrapping_subtract(
        document.header.other_income, ransom);
    return result;
  }

  const auto event_floor = load_original_header_word(document, 64U);
  const bool secom_available =
      std::bit_cast<std::int16_t>(
          load_original_header_word(document, 32U)) >= 0;
  result.started = true;
  result.followup_dialog = {
      static_cast<std::uint16_t>(secom_available ? 3021U : 3022U),
      secom_available
          ? static_cast<std::int32_t>(
                std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
                    event_floor - 9U)))
          : 0,
      10000,
  };
  return result;
}

void commit_original_bomb_event(OriginalTdtDocument& document) noexcept {
  // 10c8:0199-01b3 is reached only after DIALOG/3021-or-3022 returns.
  store_original_header_word(
      document, 60U,
      static_cast<std::uint16_t>(load_original_header_word(document, 60U) +
                                 1U));
  store_original_header_dword(document, 66U, 1200U);
  document.security_event_accelerated = false;
  dispatch_original_security_people(document, 1U);
}

OriginalFireEventOffer prepare_original_fire_event(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::int8_t day_phase) noexcept {
  OriginalFireEventOffer result{};

  // Literal 10e8:0029 gate order. Like the bomb starter, only the floor/span
  // work below consumes the Microsoft-runtime random stream.
  const auto event_flags = load_original_header_word(document, 60U);
  if ((static_cast<std::uint8_t>(event_flags) & 9U) != 0U ||
      load_original_header_word(document, 48U) == 0U || day_phase >= 4 ||
      document.header.rating < 3U ||
      std::bit_cast<std::int16_t>(
          load_original_header_word(document, 34U)) >= 0) {
    return result;
  }

  const auto requested_word = static_cast<std::uint16_t>(
      document.header.lobby_height + 10U);
  const auto floor = select_original_event_floor(
      document, std::bit_cast<std::int16_t>(requested_word));
  store_original_header_word(document, 76U,
                             std::bit_cast<std::uint16_t>(floor));
  result.floor = floor;
  if (floor < 0 ||
      static_cast<std::size_t>(floor) >= document.floors.size()) {
    return result;
  }

  const auto& floor_record =
      document.floors[static_cast<std::size_t>(floor)];
  const auto width = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(floor_record.right_edge -
                                 floor_record.left_edge));
  if (width < 32) {
    return result;
  }

  const auto x_word = static_cast<std::uint16_t>(
      floor_record.right_edge - 32U);
  const auto x = std::bit_cast<std::int16_t>(x_word);
  store_original_header_word(document, 74U, x_word);
  result.x = x;

  const bool secom_available =
      std::bit_cast<std::int16_t>(
          load_original_header_word(document, 32U)) >= 0;
  result.dialog = {
      static_cast<std::uint16_t>(secom_available ? 3010U : 3011U),
      static_cast<std::int32_t>(
          std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
              static_cast<std::uint16_t>(floor) - 9U))),
      10006,
  };
  result.offered = true;
  return result;
}

void commit_original_fire_event(OriginalTdtDocument& document,
                                const OriginalPartTable& part) noexcept {
  const auto floor = std::bit_cast<std::int16_t>(
      load_original_header_word(document, 76U));
  const auto x_word = load_original_header_word(document, 74U);
  if (floor < 0 ||
      static_cast<std::size_t>(floor) >= document.floors.size()) {
    return;
  }

  // 10e8:00d0 onward executes only after DIALOG/3010-or-3011 returns. Reload
  // the live words here just as the executable does at that boundary.
  const bool secom_available =
      std::bit_cast<std::int16_t>(
          load_original_header_word(document, 32U)) >= 0;
  store_original_header_word(document, 72U,
                              secom_available ? part.words_52_to_ac[6] : 0U);
  store_original_header_word(
      document, 60U,
      static_cast<std::uint16_t>(load_original_header_word(document, 60U) +
                                 8U));
  // DS:b410 is the event-start frame word; DS:b412 immediately follows it
  // and holds the independent SECOM countdown initialized above.
  store_original_header_word(document, 70U, document.header.frame_time);

  // Exact 10e8:0000 initializes both 120-word fire-front arrays to -1.
  for (std::size_t index = 0; index < document.floors.size(); ++index) {
    store_original_header_word(document, 80U + index * 2U, 0xffffU);
    store_original_header_word(document, 320U + index * 2U, 0xffffU);
  }
  const auto floor_index = static_cast<std::size_t>(floor);
  store_original_header_word(document, 80U + floor_index * 2U, x_word);
  store_original_header_word(document, 320U + floor_index * 2U, x_word);
  store_original_header_word(document, 78U, 0U);
  document.security_event_accelerated = false;

}

void dispatch_original_security_response(OriginalTdtDocument& document,
                                         std::uint16_t flags) noexcept {
  dispatch_original_security_people(document, flags);
}

void complete_original_event_action(
    OriginalTdtDocument& document,
    OriginalEventDeferredCompletion completion) noexcept {
  switch (completion) {
    case OriginalEventDeferredCompletion::none:
      return;
    case OriginalEventDeferredCompletion::bomb:
      // 10c8:02a3-02b6 follows DIALOG/3023-or-3024.
      dispatch_original_security_people(document, 0U);
      document.security_event_accelerated = false;
      document.header.frame_time = 1500U;
      return;
    case OriginalEventDeferredCompletion::fire:
      // 10e8:02d8-0300 follows DIALOG/3013.
      for (std::size_t index = 0; index < document.floors.size(); ++index) {
        store_original_header_word(document, 80U + index * 2U, 0xffffU);
        store_original_header_word(document, 320U + index * 2U, 0xffffU);
      }
      dispatch_original_security_people(document, 0U);
      document.security_event_accelerated = false;
      if (document.header.frame_time < 1500U) {
        document.header.frame_time = 1500U;
      }
      return;
  }
}

OriginalEventActionResult trigger_original_bomb_outcome(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    bool found) noexcept {
  OriginalEventActionResult result{};
  result.changed = true;

  auto flags = load_original_header_word(document, 60U);
  if (found) {
    store_original_header_word(
        document, 60U, static_cast<std::uint16_t>(flags + 0x20U));
  } else {
    store_original_header_word(
        document, 60U, static_cast<std::uint16_t>(flags + 0x40U));
    result.sound_requests.push_back({10004, 0U, 3U, false});

    // Literal 10c8:02bd call sequence. The floor allocation is tested once
    // before its forty x-coordinate calls, so retain all forty requests even
    // if applying an early one later empties that floor.
    const auto event_floor = std::bit_cast<std::int16_t>(
        load_original_header_word(document, 64U));
    const auto event_x = std::bit_cast<std::int16_t>(
        load_original_header_word(document, 62U));
    for (std::int32_t floor = static_cast<std::int32_t>(event_floor) - 2;
         floor <= static_cast<std::int32_t>(event_floor) + 3; ++floor) {
      if (floor < 0 || floor >= static_cast<std::int32_t>(document.floors.size()) ||
          document.floors[static_cast<std::size_t>(floor)].tenants.empty()) {
        continue;
      }
      for (std::int32_t ordinal = 0; ordinal < 40; ++ordinal) {
        const auto x_word = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(event_x) - 20U +
            static_cast<std::uint16_t>(ordinal));
        result.damage_requests.push_back({
            static_cast<std::int16_t>(floor),
            std::bit_cast<std::int16_t>(x_word), 0U});
      }
    }
    result.security_dispatch_pending = true;
    result.security_dispatch_flags = 0U;
    result.focus_requested = true;
    result.focus_floor = event_floor;
    result.focus_x = event_x;
  }

  const auto deadline_word = static_cast<std::uint16_t>(
      document.header.frame_time + part.words_52_to_ac[5]);
  store_original_header_dword(
      document, 66U,
      std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(
          std::bit_cast<std::int16_t>(deadline_word))));
  return result;
}

OriginalEventActionResult check_original_bomb_coordinate(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::int16_t floor,
    std::int16_t x) noexcept {
  if (std::bit_cast<std::uint16_t>(floor) !=
          load_original_header_word(document, 64U) ||
      std::bit_cast<std::uint16_t>(x) !=
          load_original_header_word(document, 62U)) {
    return {};
  }
  return trigger_original_bomb_outcome(document, part, true);
}

OriginalEventActionResult advance_original_bomb_event(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept {
  OriginalEventActionResult result{};
  auto flags = load_original_header_word(document, 60U);
  const auto signed_frame = static_cast<std::int32_t>(
      std::bit_cast<std::int16_t>(document.header.frame_time));
  const auto deadline = std::bit_cast<std::int32_t>(
      load_original_header_dword(document, 66U));

  if ((static_cast<std::uint8_t>(flags) & 0x60U) != 0U &&
      signed_frame == deadline) {
    flags = static_cast<std::uint16_t>(flags - 1U);
    store_original_header_word(document, 60U, flags);
    const auto floor_word = load_original_header_word(document, 64U);
    const auto floor_argument = static_cast<std::int32_t>(
        std::bit_cast<std::int16_t>(
            static_cast<std::uint16_t>(floor_word - 9U)));
    if ((static_cast<std::uint8_t>(flags) & 0x20U) != 0U) {
      result.dialog = {3023U, floor_argument, 10000};
      flags = static_cast<std::uint16_t>(flags - 0x20U);
    } else {
      result.dialog = {3024U, floor_argument, 10000};
      flags = static_cast<std::uint16_t>(flags - 0x40U);
    }
    store_original_header_word(document, 60U, flags);
    result.deferred_completion = OriginalEventDeferredCompletion::bomb;
    result.changed = true;
    result.completed = true;
  }

  // 10c8:0000 continues to this independent test even after 0254 returns.
  flags = load_original_header_word(document, 60U);
  if ((static_cast<std::uint8_t>(flags) & 1U) != 0U &&
      std::bit_cast<std::int32_t>(static_cast<std::int32_t>(
          std::bit_cast<std::int16_t>(document.header.frame_time))) ==
          std::bit_cast<std::int32_t>(
              load_original_header_dword(document, 66U))) {
    auto outcome = trigger_original_bomb_outcome(document, part, false);
    result.changed = result.changed || outcome.changed;
    result.focus_requested = outcome.focus_requested;
    result.focus_floor = outcome.focus_floor;
    result.focus_x = outcome.focus_x;
    result.security_dispatch_pending = outcome.security_dispatch_pending;
    result.security_dispatch_flags = outcome.security_dispatch_flags;
    result.damage_requests.insert(result.damage_requests.end(),
                                  outcome.damage_requests.begin(),
                                  outcome.damage_requests.end());
    result.sound_requests.insert(result.sound_requests.end(),
                                 outcome.sound_requests.begin(),
                                 outcome.sound_requests.end());
  }
  return result;
}

bool original_fire_crew_offer_due(const OriginalTdtDocument& document,
                                  const OriginalPartTable& part) noexcept {
  if ((static_cast<std::uint8_t>(
           load_original_header_word(document, 60U)) &
       8U) == 0U) {
    return false;
  }
  return static_cast<std::uint16_t>(
             load_original_header_word(document, 70U) +
             part.words_52_to_ac[5]) == document.header.frame_time;
}

bool original_fire_event_active(
    const OriginalTdtDocument& document) noexcept {
  return (static_cast<std::uint8_t>(
              load_original_header_word(document, 60U)) &
          8U) != 0U;
}

OriginalEventDialogRequest original_fire_crew_offer(
    const OriginalPartTable& part) noexcept {
  const auto cost = static_cast<std::int32_t>(
      std::bit_cast<std::int16_t>(part.words_52_to_ac[36]));
  return {3012U, -cost, 10000};
}

bool original_fire_crew_menu_offer_available(
    const OriginalTdtDocument& document) noexcept {
  // Leading 10e8:01ea gate. The menu itself is enabled only on the scheduled
  // decline path, but the function's sole runtime guard is the crew x word.
  return load_original_header_word(document, 78U) == 0U;
}

bool original_fire_crew_menu_enabled_after_rebuild(
    const OriginalTdtDocument& document) noexcept {
  // 10d0:0b03-0b31 enables command 0x9c48 only for b406 bit three with a
  // zero b418 crew coordinate. Every other reconstructed state is grayed.
  return original_fire_event_active(document) &&
         original_fire_crew_menu_offer_available(document);
}

OriginalFireCrewResolution resolve_original_fire_crew_offer(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::uint16_t dialog_result,
    bool scheduled_offer) noexcept {
  OriginalFireCrewResolution result{};
  if (!scheduled_offer && load_original_header_word(document, 78U) != 0U) {
    return result;
  }
  result.handled = true;

  const bool hire = scheduled_offer ? dialog_result != 1U
                                    : dialog_result == 2U;
  if (!hire) {
    if (scheduled_offer) {
      result.followup_dialog = {3014U, 0, 10000};
      result.security_dispatch_pending = true;
      result.security_dispatch_flags = 8U;
      // Win16 MF_ENABLED is zero. 10e8:01d9 passes flag zero after the
      // scheduled offer is declined, leaving the menu command available for
      // the later 10e8:01e2 reconsideration path.
      result.fire_menu_enabled = true;
    }
    return result;
  }

  const auto floor_word = load_original_header_word(document, 76U);
  const auto floor = std::bit_cast<std::int16_t>(floor_word);
  if (floor < 0 ||
      static_cast<std::size_t>(floor) >= document.floors.size()) {
    return result;
  }
  const auto crew_x_word = static_cast<std::uint16_t>(
      document.floors[static_cast<std::size_t>(floor)].right_edge - 12U);
  store_original_header_word(document, 78U, crew_x_word);
  const auto cost = static_cast<std::int32_t>(
      std::bit_cast<std::int16_t>(part.words_52_to_ac[36]));
  if (cost != 0) {
    document.header.balance = wrapping_subtract(document.header.balance, cost);
    document.header.other_income = wrapping_subtract(
        document.header.other_income, cost);
  }
  result.hired = true;
  result.focus_requested = true;
  result.focus_floor = floor;
  result.focus_x = std::bit_cast<std::int16_t>(crew_x_word);
  // 10e8:0251 passes MF_GRAYED (one) once the crew has been hired.
  result.fire_menu_enabled = false;
  return result;
}

bool original_fire_covers_coordinate(const OriginalTdtDocument& document,
                                     std::int16_t floor,
                                     std::int16_t x) noexcept {
  if (floor < 0 ||
      static_cast<std::size_t>(floor) >= document.floors.size()) {
    return false;
  }
  const auto index = static_cast<std::size_t>(floor);
  const auto left = std::bit_cast<std::int16_t>(
      load_original_header_word(document, 320U + index * 2U));
  if (left >= 0 && left <= x &&
      std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
          static_cast<std::uint16_t>(left) + 6U)) > x) {
    return true;
  }
  const auto right = std::bit_cast<std::int16_t>(
      load_original_header_word(document, 80U + index * 2U));
  return right >= 0 &&
         std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
             static_cast<std::uint16_t>(right) + 6U)) <= x &&
         std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
             static_cast<std::uint16_t>(right) + 12U)) > x;
}

bool extinguish_original_fire_at(OriginalTdtDocument& document,
                                 std::int16_t floor,
                                 std::int16_t x) noexcept {
  // Exact 10e8:07d6 two-front hit test: each live 12-cell half-open band is
  // cleared independently. The Security-person caller raises DS:77aa when
  // either front changes, preserving the original responder acceleration.
  if (floor < 0 ||
      static_cast<std::size_t>(floor) >= document.floors.size()) {
    return false;
  }
  bool changed = false;
  const auto index = static_cast<std::size_t>(floor);
  for (const auto base : {320U, 80U}) {
    const auto offset = base + index * 2U;
    const auto fire_x = std::bit_cast<std::int16_t>(
        load_original_header_word(document, offset));
    if (fire_x >= 0 && fire_x <= x &&
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(fire_x) + 12U)) > x) {
      store_original_header_word(document, offset, 0xffffU);
      changed = true;
    }
  }
  return changed;
}

OriginalEventActionResult finish_original_fire_event(
    OriginalTdtDocument& document) noexcept {
  OriginalEventActionResult result{};
  const auto flags = load_original_header_word(document, 60U);
  if ((static_cast<std::uint8_t>(flags) & 8U) == 0U) {
    return result;
  }
  store_original_header_word(document, 60U,
                             static_cast<std::uint16_t>(flags - 8U));
  // 10e8:02c2 also passes MF_GRAYED (one) when the fire ends.
  result.fire_menu_enabled = false;
  result.dialog = {3013U, 0, 10000};
  result.deferred_completion = OriginalEventDeferredCompletion::fire;
  result.changed = true;
  result.completed = true;
  return result;
}

OriginalEventActionResult advance_original_fire_event(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept {
  bool active = false;
  for (std::size_t index = 0; index < document.floors.size(); ++index) {
    if (std::bit_cast<std::int16_t>(
            load_original_header_word(document, 320U + index * 2U)) >= 0 ||
        std::bit_cast<std::int16_t>(
            load_original_header_word(document, 80U + index * 2U)) >= 0) {
      active = true;
    }
  }
  if (!active) {
    return finish_original_fire_event(document);
  }

  OriginalEventActionResult result{};
  const auto secom_timer = load_original_header_word(document, 72U);
  if (secom_timer != 0U) {
    store_original_header_word(document, 72U,
                               static_cast<std::uint16_t>(secom_timer - 1U));
    result.changed = true;
  } else {
    const auto frame = std::bit_cast<std::int16_t>(document.header.frame_time);
    const auto event_floor = std::bit_cast<std::int16_t>(
        load_original_header_word(document, 76U));
    const auto event_x = std::bit_cast<std::int16_t>(
        load_original_header_word(document, 74U));
    const auto spread_period = std::bit_cast<std::int16_t>(
        part.words_52_to_ac[2]);
    const auto floor_delay = std::bit_cast<std::int16_t>(
        part.words_52_to_ac[3]);
    const auto start_frame = load_original_header_word(document, 70U);

    for (std::size_t index = 0; index < document.floors.size(); ++index) {
      const auto& floor = document.floors[index];
      if (floor.tenants.empty()) {
        continue;
      }
      const auto index_word = static_cast<std::uint16_t>(index);
      const auto delta_word = static_cast<std::uint16_t>(
          index_word - static_cast<std::uint16_t>(event_floor));
      const auto delayed = static_cast<std::uint16_t>(
          static_cast<std::int32_t>(std::bit_cast<std::int16_t>(delta_word)) *
          floor_delay);
      const auto due_frame = static_cast<std::uint16_t>(delayed + start_frame);

      const auto left_offset = 320U + index * 2U;
      auto left = std::bit_cast<std::int16_t>(
          load_original_header_word(document, left_offset));
      if (left >= 0) {
        result.damage_requests.push_back(
            {static_cast<std::int16_t>(index), left, 0U});
        result.changed = true;
        if (spread_period != 0 && frame % spread_period == 0) {
          left = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
              static_cast<std::uint16_t>(left) - 1U));
          store_original_header_word(document, left_offset,
                                     std::bit_cast<std::uint16_t>(left));
        }
        if (left < std::bit_cast<std::int16_t>(floor.left_edge)) {
          store_original_header_word(document, left_offset, 0xffffU);
        }
      } else if (due_frame == document.header.frame_time) {
        if (std::bit_cast<std::int16_t>(floor.left_edge) > event_x) {
          store_original_header_word(document, left_offset, 0xffffU);
        } else {
          store_original_header_word(document, left_offset,
                                     std::bit_cast<std::uint16_t>(event_x));
        }
        result.changed = true;
      }

      const auto right_offset = 80U + index * 2U;
      auto right = std::bit_cast<std::int16_t>(
          load_original_header_word(document, right_offset));
      if (right > 0) {
        const auto damage_x = std::bit_cast<std::int16_t>(
            static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(right) + 12U));
        result.damage_requests.push_back(
            {static_cast<std::int16_t>(index), damage_x, 0U});
        result.changed = true;
        if (spread_period != 0 && frame % spread_period == 0) {
          right = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
              static_cast<std::uint16_t>(right) + 1U));
          store_original_header_word(document, right_offset,
                                     std::bit_cast<std::uint16_t>(right));
        }
        const auto edge = std::bit_cast<std::int16_t>(floor.right_edge);
        const auto limit = std::bit_cast<std::int16_t>(
            static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(right) + 12U));
        if (edge < limit) {
          store_original_header_word(document, right_offset, 0xffffU);
        }
      } else if (due_frame == document.header.frame_time) {
        const auto limit = std::bit_cast<std::int16_t>(
            static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(event_x) + 12U));
        if (std::bit_cast<std::int16_t>(floor.right_edge) < limit) {
          store_original_header_word(document, right_offset, 0xffffU);
        } else {
          store_original_header_word(document, right_offset,
                                     std::bit_cast<std::uint16_t>(event_x));
        }
        result.changed = true;
      }
    }
  }

  auto crew_x = load_original_header_word(document, 78U);
  if (crew_x != 0U) {
    const auto crew_period = std::bit_cast<std::int16_t>(
        part.words_52_to_ac[4]);
    const auto frame = std::bit_cast<std::int16_t>(document.header.frame_time);
    if (crew_period != 0 && frame % crew_period == 0) {
      crew_x = static_cast<std::uint16_t>(crew_x - 1U);
      store_original_header_word(document, 78U, crew_x);
      result.changed = true;
    }
    const auto floor = std::bit_cast<std::int16_t>(
        load_original_header_word(document, 76U));
    if (floor >= 0 &&
        static_cast<std::size_t>(floor) < document.floors.size() &&
        document.floors[static_cast<std::size_t>(floor)].left_edge == crew_x) {
      crew_x = 0U;
      store_original_header_word(document, 78U, 0U);
      result.changed = true;
    }
    result.sound_requests.push_back({10009, 10U, 5U, true});

    // 10e8:0856 is called after the possible zeroing above and is an exact
    // no-op once the crew reaches the floor's left edge.
    if (crew_x != 0U) {
      const auto signed_crew = std::bit_cast<std::int16_t>(crew_x);
      for (std::size_t index = 0; index < document.floors.size(); ++index) {
        for (const auto base : {320U, 80U}) {
          const auto offset = base + index * 2U;
          if (std::bit_cast<std::int16_t>(
                  load_original_header_word(document, offset)) > signed_crew) {
            store_original_header_word(document, offset, 0xffffU);
            result.changed = true;
          }
        }
      }
    }
  }
  return result;
}

OriginalSimulationStep step_original_simulation(
    OriginalSimulationState& state,
    std::uint32_t now_tick,
    bool fast_mode,
    bool special_tower_flag) {
  OriginalSimulationStep result{};
  // 1200:01ac-01c1 calls 1208:05e6, then performs a signed comparison
  // against last_tick + 6. Those are coarse 16-ms ticks, making the nominal
  // host interval 96 ms. Fast Mode (DS:de34) bypasses the gate but not calls.
  const auto signed_now = std::bit_cast<std::int32_t>(now_tick);
  const auto signed_deadline = std::bit_cast<std::int32_t>(
      state.last_tick + kOriginalSimulationGateTicks);
  if (!fast_mode && signed_now < signed_deadline) {
    return result;
  }

  result.advanced = true;
  ++state.frame_time;
  if (state.frame_time == 0x0a28U) {
    state.frame_time = 0;
  }
  state.day_phase = static_cast<std::int8_t>(
      original_day_phase(state.frame_time));

  // Per-frame gates at 1200:01e7-020a precede the discrete schedule.
  if (state.frame_time > 0x00f0U && state.day_phase < 6) {
    call(result.calls, 0x11c8, 0x03ab);
  }
  if (state.frame_time > 0x00f0U && state.day_phase < 4) {
    call(result.calls, 0x11e8, 0x0273);
  }

  switch (state.frame_time) {
    case 0x09c4:
      call(result.calls, 0x1220, 0x1059);
      call(result.calls, 0x1220, 0x0000);
      call(result.calls, 0x1188, 0x0977);
      call(result.calls, 0x1228, 0x086b);
      break;
    case 0x09e5:
      if (state.current_day % 3 == 0) {
        call(result.calls, 0x1060, 0x003a);
      }
      call(result.calls, 0x1130, 0x0000);
      if (state.current_day % 3 == 0) {
        call(result.calls, 0x1178, 0x0b44);
      }
      call(result.calls, 0x1220, 0x1059);
      call(result.calls, 0x1220, 0x0000);
      break;
    case 0x09f6:
      if (state.current_day % 5 != 4) {
        play(result.calls, 0x1388, 4, 3);
      } else {
        play(result.calls, 0x1389, 1, 3);
      }
      break;
    case 0x0000:
      call(result.calls, 0x1228, 0x0968);
      call(result.calls, 0x1198, 0x01ab);
      call(result.calls, 0x1170, 0x011f);
      call(result.calls, 0x1088, 0x00de);
      call(result.calls, 0x1040, 0x0000);
      call(result.calls, 0x1020, 0x0dcb);
      break;
    case 0x0020:
      call(result.calls, 0x1088, 0x01d1);
      break;
    case 0x0050:
      if (special_tower_flag) play(result.calls, 0x138d, 2, 3);
      break;
    case 0x0078:
      if (special_tower_flag) play(result.calls, 0x138b, 4, 3);
      break;
    case 0x00a0:
      play(result.calls, 0x138c, 0, 3);
      break;
    case 0x00f0:
      call(result.calls, 0x11a8, 0x0184);
      call(result.calls, 0x1180, 0x05af);
      if (state.current_day % 84 == 83) {
        call(result.calls, 0x10e8, 0x0029);
      }
      if (state.current_day % 60 == 59) {
        call(result.calls, 0x10c8, 0x006e);
      }
      break;
    case 0x03e8:
      call(result.calls, 0x1180, 0x06a8, {0, 1});
      break;
    case 0x04b0:
      call(result.calls, 0x1180, 0x0826, {0, 1});
      call(result.calls, 0x1180, 0x06a8, {1, 0});
      call(result.calls, 0x1040, 0x0179);
      break;
    case 0x0578:
      call(result.calls, 0x1180, 0x06a8, {1, 1});
      break;
    case 0x05dc:
      call(result.calls, 0x1180, 0x090a, {0, 1});
      break;
    case 0x0640:
      call(result.calls, 0x11a8, 0x0250);
      call(result.calls, 0x1130, 0x01e2);
      call(result.calls, 0x1130, 0x0109);
      call(result.calls, 0x1240, 0x01de);
      call(result.calls, 0x1188, 0x0a20);
      call(result.calls, 0x1228, 0x0b59);
      call(result.calls, 0x1180, 0x0826, {1, 1});
      call(result.calls, 0x1180, 0x090a, {1, 0});
      call(result.calls, 0x1088, 0x0000, {0});
      call(result.calls, 0x1020, 0x0e0b);
      break;
    case 0x06a4:
      play(result.calls, 0x138a, 0, 3);
      break;
    case 0x0708:
      call(result.calls, 0x1088, 0x0000, {1});
      break;
    case 0x076c:
      call(result.calls, 0x1180, 0x090a, {1, 1});
      break;
    case 0x07d0:
      call(result.calls, 0x11a8, 0x0554);
      call(result.calls, 0x1088, 0x0000, {2});
      if (state.current_day % 12 == 11) {
        call(result.calls, 0x11b8, 0x0028);
      }
      break;
    case 0x0898:
      call(result.calls, 0x11a8, 0x0603);
      call(result.calls, 0x1088, 0x0000, {3});
      break;
    case 0x08fc:
      // 1200:04b3 is a wrapping 386 INC DWORD. Preserve its INT32_MAX edge
      // for malformed/foreign save state instead of invoking signed C++
      // overflow before the literal 0x2ed4 reset comparison.
      state.current_day = wrapping_add(state.current_day, 1);
      if (state.current_day == 0x2ed4) {
        state.current_day = 0;
      }
      state.calendar_phase = original_calendar_phase(state.current_day);
      result.day_changed = true;
      break;
    case 0x0960:
      call(result.calls, 0x1088, 0x0000, {4});
      break;
    case 0x0a06:
      call(result.calls, 0x1088, 0x0000, {5});
      break;
    default:
      break;
  }

  return result;
}

OriginalSimulationStep step_original_simulation(
    OriginalSimulationState& state,
    OriginalTdtDocument& document,
    std::uint32_t now_tick,
    bool fast_mode) {
  auto result = step_original_simulation(
      state, now_tick, fast_mode,
      original_special_event_audio_active(document));
  if (!result.advanced) return result;

  document.header.frame_time = state.frame_time;
  document.header.current_day = state.current_day;
  if (state.frame_time == 0U) {
    // 1200:02b4 precedes every frame-zero far call.
    document.header.version_20_word = 0U;
  }
  if (state.frame_time == 0x04b0U) {
    // 1200:0391 precedes the entertainment and Cathedral calls.
    document.hotel_checkout_count = 0U;
  }
  return result;
}

void finish_original_simulation_step(
    OriginalSimulationState& state,
    std::uint32_t completion_tick) noexcept {
  // 1200:0529 calls 1208:05e6 again after the selected schedule branch has
  // completed. This is deliberately not the entry sample used by 01ac's
  // gate: scheduled dialogs and other expensive callbacks postpone the next
  // eligible frame from their completion time.
  state.last_tick = completion_tick;
}

}  // namespace simtower
