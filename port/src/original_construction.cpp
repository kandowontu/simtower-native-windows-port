#include "original_construction.hpp"

#include "original_people.hpp"
#include "original_simulation.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace simtower {
namespace {

void store_u16(std::span<std::byte> destination, std::size_t offset,
               std::uint16_t value, bool byte_swapped) {
  if (byte_swapped) {
    destination[offset] = static_cast<std::byte>(value >> 8U);
    destination[offset + 1U] = static_cast<std::byte>(value);
  } else {
    destination[offset] = static_cast<std::byte>(value);
    destination[offset + 1U] = static_cast<std::byte>(value >> 8U);
  }
}

void store_u32(std::span<std::byte> destination, std::size_t offset,
               std::uint32_t value, bool byte_swapped) {
  if (byte_swapped) {
    destination[offset] = static_cast<std::byte>(value >> 24U);
    destination[offset + 1U] = static_cast<std::byte>(value >> 16U);
    destination[offset + 2U] = static_cast<std::byte>(value >> 8U);
    destination[offset + 3U] = static_cast<std::byte>(value);
  } else {
    destination[offset] = static_cast<std::byte>(value);
    destination[offset + 1U] = static_cast<std::byte>(value >> 8U);
    destination[offset + 2U] = static_cast<std::byte>(value >> 16U);
    destination[offset + 3U] = static_cast<std::byte>(value >> 24U);
  }
}

std::uint16_t load_u16(std::span<const std::byte> source,
                       std::size_t offset,
                       bool byte_swapped) {
  const auto first = std::to_integer<std::uint8_t>(source[offset]);
  const auto second = std::to_integer<std::uint8_t>(source[offset + 1U]);
  return byte_swapped
             ? static_cast<std::uint16_t>((first << 8U) | second)
             : static_cast<std::uint16_t>(first | (second << 8U));
}

std::uint32_t load_u32(std::span<const std::byte> source,
                       std::size_t offset,
                       bool byte_swapped) {
  const auto byte = [&](std::size_t index) {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(source[offset + index]));
  };
  if (byte_swapped) {
    return (byte(0) << 24U) | (byte(1) << 16U) | (byte(2) << 8U) | byte(3);
  }
  return byte(0) | (byte(1) << 8U) | (byte(2) << 16U) | (byte(3) << 24U);
}

std::size_t original_header_runtime_offset(const OriginalTdtDocument& document,
                                           std::size_t version_20_offset) {
  std::size_t offset = version_20_offset -
                       (document.header.format_version >= 0x20U ? 0U : 2U);
  // Revision 0x23 inserted runtime word b404 immediately before b406. The
  // in-memory addresses stay fixed, but earlier serialized headers omit it.
  if (version_20_offset >= 60U && document.header.format_version < 0x23U) {
    offset -= 2U;
  }
  return offset;
}

std::uint16_t load_original_header_word(const OriginalTdtDocument& document,
                                        std::size_t version_20_offset) {
  const auto offset = original_header_runtime_offset(document,
                                                     version_20_offset);
  if (offset + 2U > document.header.exact_bytes.size()) {
    return 0U;
  }
  return load_u16(std::span<const std::byte>(document.header.exact_bytes),
                  offset, document.header.byte_swapped);
}

void store_original_header_word(OriginalTdtDocument& document,
                                std::size_t version_20_offset,
                                std::uint16_t value) {
  const auto offset = original_header_runtime_offset(document,
                                                     version_20_offset);
  if (offset + 2U <= document.header.exact_bytes.size()) {
    store_u16(document.header.exact_bytes, offset, value,
              document.header.byte_swapped);
  }
}

std::int32_t wrapping_subtract(std::int32_t value,
                               std::uint32_t amount) noexcept {
  // 1178:004e aggregates 035b's facility charges with 0583's newly exposed
  // floor cells; 1178:01db/027c then subtract that result from balance and
  // construction costs. Exact 1178:0697 is the shared direct-amount form used
  // by the demolition/structure path. Native construction helpers inline
  // those transactions at each call site, preserving 32-bit wrapping debit
  // semantics.
  return std::bit_cast<std::int32_t>(
      std::bit_cast<std::uint32_t>(value) - amount);
}

constexpr std::uint16_t original_status_code(
    bool condition,
    std::uint16_t when_true,
    std::uint16_t when_false) noexcept {
  return condition ? when_true : when_false;
}

constexpr int original_signed_shape_half(std::uint8_t shape) noexcept {
  // Win16 emits CBW followed by SAR AX,1 for persisted Stair shape bytes.
  // Division would differ for negative odd values because IDIV truncates
  // toward zero while SAR rounds toward negative infinity.
  const int value = static_cast<std::int8_t>(shape);
  return value >= 0 ? value / 2 : -((-value + 1) / 2);
}

std::uint16_t original_funds_status_code(
    std::int32_t balance,
    std::uint64_t facility_cost) noexcept {
  // 1178:009e/011d first compares the facility charge and emits 7. Only
  // the later exposed-floor comparison emits 8.
  return facility_cost > 0x7fffffffULL ||
          balance < static_cast<std::int32_t>(facility_cost)
      ? 7U
      : 8U;
}

OriginalTdtTenant make_lobby_tenant(std::uint16_t left,
                                    std::uint16_t right,
                                    bool byte_swapped) {
  OriginalTdtTenant tenant{};
  tenant.left = left;
  tenant.right = right;
  tenant.type = 0x18;
  tenant.status = 0;
  tenant.variant = 0;

  // 11f8:1e88-1f14 creates the common record, then the type-0x18 branch in
  // 1228:0103 allocates lookup key zero on an otherwise empty floor and
  // clears byte +0x0b. The remaining bytes came from GMEM_ZEROINIT.
  tenant.preserved_07_to_0f[5] = std::byte{0};     // exact byte 12: key 0
  tenant.preserved_07_to_0f[6] = std::byte{1};     // exact byte 13
  tenant.preserved_07_to_0f[7] = std::byte{1};     // exact byte 14
  tenant.preserved_07_to_0f[8] = std::byte{0xff};  // exact byte 15
  tenant.rent_rate = 4;
  tenant.subtype = 0;

  auto exact = std::span<std::byte>(tenant.exact_bytes);
  store_u16(exact, 0, left, byte_swapped);
  store_u16(exact, 2, right, byte_swapped);
  exact[4] = std::byte{0x18};
  exact[5] = std::byte{0};
  exact[6] = std::byte{0};
  std::copy(tenant.preserved_07_to_0f.begin(),
            tenant.preserved_07_to_0f.end(), exact.begin() + 7);
  exact[16] = std::byte{4};
  exact[17] = std::byte{0};
  return tenant;
}

OriginalTdtTenant make_original_empty_floor_tenant(std::uint16_t left,
                                                    std::uint16_t right,
                                                    bool byte_swapped) {
  OriginalTdtTenant tenant{};
  tenant.left = left;
  tenant.right = right;
  tenant.type = 0;
  tenant.status = 2;
  tenant.variant = 0;
  tenant.preserved_07_to_0f[5] = std::byte{0xff};
  tenant.preserved_07_to_0f[6] = std::byte{1};
  tenant.preserved_07_to_0f[7] = std::byte{1};
  tenant.preserved_07_to_0f[8] = std::byte{0xff};
  tenant.rent_rate = 4;
  tenant.subtype = 0;

  auto exact = std::span<std::byte>(tenant.exact_bytes);
  store_u16(exact, 0, left, byte_swapped);
  store_u16(exact, 2, right, byte_swapped);
  exact[5] = std::byte{2};
  exact[12] = std::byte{0xff};
  exact[13] = std::byte{1};
  exact[14] = std::byte{1};
  exact[15] = std::byte{0xff};
  exact[16] = std::byte{4};
  return tenant;
}

OriginalTdtTenant make_original_automatic_floor_tenant(
    const OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    std::uint16_t right) {
  // Literal 10a0:1310 and 11f8:30ef. Gaps made while a construction request
  // extends a floor use a Lobby record on the automatic ground-Lobby stories,
  // but the requested type-0 interval itself remains an ordinary Floor.
  const bool lobby_story = floor >= 10 &&
      floor < static_cast<std::int16_t>(
                  10U + document.header.lobby_height);
  return lobby_story
      ? make_lobby_tenant(left, right, document.header.byte_swapped)
      : make_original_empty_floor_tenant(
            left, right, document.header.byte_swapped);
}

OriginalTdtTenant make_original_pending_facility(
    std::uint16_t left,
    std::uint16_t right,
    std::uint8_t type,
    std::uint8_t variant,
    std::uint8_t key,
    std::uint32_t people_start,
    bool byte_swapped) {
  OriginalTdtTenant tenant{};
  tenant.left = left;
  tenant.right = right;
  // 1228:0000 negates byte +4 while the ten-slot 11f0 activation queue owns
  // the new unit. The positive type is restored by 11f0:00a0.
  tenant.type = static_cast<std::int8_t>(-static_cast<std::int16_t>(type));
  tenant.status = 0;
  tenant.variant = variant;
  tenant.preserved_07_to_0f[5] = static_cast<std::byte>(key);
  tenant.preserved_07_to_0f[6] = std::byte{1};
  tenant.preserved_07_to_0f[7] = std::byte{1};
  tenant.preserved_07_to_0f[8] = std::byte{0xff};
  const bool unit_rent_rate =
      (type >= 3U && type <= 5U) || type == 7U || type == 9U || type == 10U;
  tenant.rent_rate = unit_rent_rate ? 1U : 4U;
  tenant.subtype = 0x0c;

  auto exact = std::span<std::byte>(tenant.exact_bytes);
  store_u16(exact, 0, left, byte_swapped);
  store_u16(exact, 2, right, byte_swapped);
  exact[4] = static_cast<std::byte>(
      static_cast<std::uint8_t>(-static_cast<std::int16_t>(type)));
  exact[6] = static_cast<std::byte>(variant);
  store_u32(exact, 8, people_start, byte_swapped);
  exact[12] = static_cast<std::byte>(key);
  exact[13] = std::byte{1};
  exact[14] = std::byte{1};
  exact[15] = std::byte{0xff};
  // 11f8:1ec0's type table assigns one to 3/4/5/7/9/10 and four to the
  // remaining generic facilities, including Restaurant/Fast Food/Security.
  exact[16] = static_cast<std::byte>(tenant.rent_rate);
  exact[17] = std::byte{0x0c};
  std::copy(exact.begin() + 7, exact.begin() + 16,
            tenant.preserved_07_to_0f.begin());
  return tenant;
}

OriginalTdtTenant make_original_immediate_facility(
    std::uint16_t left,
    std::uint16_t right,
    std::uint8_t type,
    std::uint8_t status,
    bool byte_swapped) {
  OriginalTdtTenant tenant{};
  tenant.left = left;
  tenant.right = right;
  tenant.type = static_cast<std::int8_t>(type);
  tenant.status = status;
  tenant.variant = 0;
  tenant.preserved_07_to_0f[5] = std::byte{0xff};
  tenant.preserved_07_to_0f[6] = std::byte{1};
  tenant.preserved_07_to_0f[7] = std::byte{1};
  tenant.preserved_07_to_0f[8] = std::byte{0xff};
  tenant.rent_rate = 4;
  tenant.subtype = 0;

  auto exact = std::span<std::byte>(tenant.exact_bytes);
  store_u16(exact, 0, left, byte_swapped);
  store_u16(exact, 2, right, byte_swapped);
  exact[4] = static_cast<std::byte>(type);
  exact[5] = static_cast<std::byte>(status);
  exact[12] = std::byte{0xff};
  exact[13] = std::byte{1};
  exact[14] = std::byte{1};
  exact[15] = std::byte{0xff};
  exact[16] = std::byte{4};
  return tenant;
}

void set_tenant_left(OriginalTdtTenant& tenant, std::uint16_t left,
                     bool byte_swapped) {
  tenant.left = left;
  store_u16(tenant.exact_bytes, 0, left, byte_swapped);
}

void set_tenant_right(OriginalTdtTenant& tenant, std::uint16_t right,
                      bool byte_swapped) {
  tenant.right = right;
  store_u16(tenant.exact_bytes, 2, right, byte_swapped);
}

std::optional<std::uint8_t> first_original_floor_key(
    const OriginalTdtFloor& floor) {
  std::array<bool, OriginalTdtFloor::kIndexCapacity> used{};
  for (const auto& tenant : floor.tenants) {
    const auto key = std::to_integer<std::uint8_t>(tenant.exact_bytes[12]);
    if (key < used.size()) {
      used[key] = true;
    }
  }
  for (std::size_t key = 0; key < used.size(); ++key) {
    if (!used[key]) {
      return static_cast<std::uint8_t>(key);
    }
  }
  return std::nullopt;
}

enum class OriginalFloorRunReplacementStatus : std::uint8_t {
  replaced,
  occupied,
  tenant_limit,
};

struct OriginalFloorRunReplacement {
  OriginalFloorRunReplacementStatus status{
      OriginalFloorRunReplacementStatus::occupied};
  std::vector<OriginalTdtTenant> tenants{};
};

OriginalFloorRunReplacement replace_original_floor_or_lobby_run(
    const OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    std::uint16_t right,
    std::uint8_t requested_type) {
  // Exact record surgery shared by 11f8:17fd and 11f8:284d for type 0 Floor
  // and type 24 Lobby. The overlap path accepts only Floor or the requested
  // type, preserves at most two byte-identical edge remainders, and creates a
  // fresh common record in place of the whole crossed run.
  OriginalFloorRunReplacement result{};
  const auto& old_floor = document.floors[static_cast<std::size_t>(floor)];
  const bool byte_swapped = document.header.byte_swapped;
  auto requested = requested_type == 0x18U
      ? make_lobby_tenant(left, right, byte_swapped)
      : make_original_empty_floor_tenant(left, right, byte_swapped);
  if (requested_type == 0x18U) {
    // 284d initializes byte 18 to FF before 1228:0103 asks 0d9a for the first
    // key unused by the already-rewritten record list.
    requested.exact_bytes[12] = std::byte{0xff};
    requested.preserved_07_to_0f[5] = std::byte{0xff};
  }

  std::optional<std::size_t> requested_index{};
  const auto append_requested = [&]() {
    requested_index = result.tenants.size();
    result.tenants.push_back(requested);
  };
  const bool overlaps_floor = !old_floor.tenants.empty() &&
      old_floor.left_edge < right && old_floor.right_edge > left;
  if (old_floor.tenants.empty()) {
    append_requested();
  } else if (!overlaps_floor && right <= old_floor.left_edge) {
    append_requested();
    if (right != old_floor.left_edge) {
      result.tenants.push_back(make_original_automatic_floor_tenant(
          document, floor, right, old_floor.left_edge));
    }
    result.tenants.insert(result.tenants.end(), old_floor.tenants.begin(),
                          old_floor.tenants.end());
  } else if (!overlaps_floor && left >= old_floor.right_edge) {
    result.tenants = old_floor.tenants;
    if (left != old_floor.right_edge) {
      result.tenants.push_back(make_original_automatic_floor_tenant(
          document, floor, old_floor.right_edge, left));
    }
    append_requested();
  } else {
    std::size_t first_replaced = 0U;
    while (first_replaced < old_floor.tenants.size() &&
           old_floor.tenants[first_replaced].right <= left) {
      ++first_replaced;
    }
    if (first_replaced >= old_floor.tenants.size()) return result;
    std::size_t last_replaced = first_replaced;
    while (last_replaced + 1U < old_floor.tenants.size() &&
           old_floor.tenants[last_replaced].right < right) {
      ++last_replaced;
    }
    for (std::size_t index = first_replaced; index <= last_replaced; ++index) {
      const auto type = old_floor.tenants[index].type;
      if (type != 0 && type != static_cast<std::int8_t>(requested_type)) {
        return result;
      }
    }

    result.tenants.insert(
        result.tenants.end(), old_floor.tenants.begin(),
        old_floor.tenants.begin() + static_cast<std::ptrdiff_t>(first_replaced));
    if (old_floor.tenants[first_replaced].left < left) {
      auto remainder = old_floor.tenants[first_replaced];
      set_tenant_right(remainder, left, byte_swapped);
      result.tenants.push_back(std::move(remainder));
    }
    append_requested();
    if (old_floor.tenants[last_replaced].right > right) {
      auto remainder = old_floor.tenants[last_replaced];
      set_tenant_left(remainder, right, byte_swapped);
      result.tenants.push_back(std::move(remainder));
    }
    result.tenants.insert(
        result.tenants.end(),
        old_floor.tenants.begin() +
            static_cast<std::ptrdiff_t>(last_replaced + 1U),
        old_floor.tenants.end());
  }

  if (result.tenants.size() > OriginalTdtFloor::kTenantCapacity) {
    result.status = OriginalFloorRunReplacementStatus::tenant_limit;
    result.tenants.clear();
    return result;
  }
  if (requested_type == 0x18U && requested_index) {
    OriginalTdtFloor rewritten = old_floor;
    rewritten.tenants = result.tenants;
    const auto key = first_original_floor_key(rewritten).value_or(0xffU);
    auto& lobby = result.tenants[*requested_index];
    lobby.exact_bytes[12] = static_cast<std::byte>(key);
    lobby.preserved_07_to_0f[5] = static_cast<std::byte>(key);
  }
  result.status = OriginalFloorRunReplacementStatus::replaced;
  return result;
}

std::uint32_t original_sky_lobby_chargeable_cells(
    const OriginalTdtFloor& floor,
    std::uint16_t left,
    std::uint16_t right) noexcept {
  // Exact type-24 half of 1178:035b. Only an existing Lobby containing the
  // request's left or right edge trims the charge; internal Lobby records are
  // deliberately still included by the original two boundary scans.
  auto charge_left = left;
  for (const auto& tenant : floor.tenants) {
    if (tenant.type != 0x18 || tenant.left > charge_left ||
        tenant.right <= charge_left) {
      continue;
    }
    charge_left = std::min(tenant.right, right);
  }
  auto charge_right = right;
  for (const auto& tenant : floor.tenants) {
    if (tenant.type != 0x18 || tenant.left >= charge_right ||
        tenant.right < charge_right) {
      continue;
    }
    charge_right = std::max(tenant.left, charge_left);
  }
  return charge_left < charge_right
      ? static_cast<std::uint32_t>(charge_right - charge_left)
      : 0U;
}

std::optional<std::uint32_t> first_original_people_run(
    const OriginalTdtDocument& document, std::size_t length) {
  if (document.people_count != document.people.size()) {
    return std::nullopt;
  }
  for (std::size_t start = 0; start + length <= document.people.size();
       ++start) {
    bool free = true;
    for (std::size_t index = 0; index < length; ++index) {
      if (document.people[start + index].exact_bytes[4] != std::byte{0}) {
        free = false;
        start += index;
        break;
      }
    }
    if (free) {
      return static_cast<std::uint32_t>(start);
    }
  }
  return std::nullopt;
}

void rebuild_original_floor_lookup(OriginalTdtFloor& floor) {
  // 1228:0e30 intentionally does not clear entries for keys no longer in
  // use. It only overwrites the keys present in the current record list.
  for (std::size_t index = 0; index < floor.tenants.size(); ++index) {
    const auto key =
        std::to_integer<std::uint8_t>(floor.tenants[index].exact_bytes[12]);
    if (key < floor.tenant_index.size()) {
      floor.tenant_index[key] = static_cast<std::uint16_t>(index);
    }
  }
}

enum class OfficeInsertionResult : std::uint8_t {
  inserted,
  occupied,
  tenant_limit,
};

OfficeInsertionResult insert_original_office_record(
    std::vector<OriginalTdtTenant>& tenants,
    std::uint16_t& floor_left,
    std::uint16_t& floor_right,
    OriginalTdtTenant office,
    bool byte_swapped) {
  // 11f8:321e is the matching placement preflight: a span inside represented
  // bounds is admissible only when one type-zero record contains it in full.
  const auto left = office.left;
  const auto right = office.right;
  if (tenants.empty()) {
    floor_left = left;
    floor_right = right;
    tenants.push_back(std::move(office));
    return OfficeInsertionResult::inserted;
  }

  if (right <= floor_left) {
    const bool adjacent = right == floor_left;
    const std::size_t added = adjacent ? 1U : 2U;
    if (tenants.size() + added > OriginalTdtFloor::kTenantCapacity) {
      return OfficeInsertionResult::tenant_limit;
    }
    std::vector<OriginalTdtTenant> prefix;
    prefix.reserve(added);
    prefix.push_back(std::move(office));
    if (!adjacent) {
      prefix.push_back(make_original_empty_floor_tenant(
          right, floor_left, byte_swapped));
    }
    tenants.insert(tenants.begin(), prefix.begin(), prefix.end());
    floor_left = left;
    return OfficeInsertionResult::inserted;
  }

  if (left >= floor_right) {
    const bool adjacent = left == floor_right;
    const std::size_t added = adjacent ? 1U : 2U;
    if (tenants.size() + added > OriginalTdtFloor::kTenantCapacity) {
      return OfficeInsertionResult::tenant_limit;
    }
    if (!adjacent) {
      tenants.push_back(make_original_empty_floor_tenant(
          floor_right, left, byte_swapped));
    }
    tenants.push_back(std::move(office));
    floor_right = right;
    return OfficeInsertionResult::inserted;
  }

  for (std::size_t index = 0; index < tenants.size(); ++index) {
    const auto& candidate = tenants[index];
    if (candidate.type != 0 || candidate.left > left ||
        candidate.right < right) {
      continue;
    }
    const bool left_remainder = candidate.left < left;
    const bool right_remainder = candidate.right > right;
    const std::size_t added = static_cast<std::size_t>(left_remainder) +
                              static_cast<std::size_t>(right_remainder);
    if (tenants.size() + added > OriginalTdtFloor::kTenantCapacity) {
      return OfficeInsertionResult::tenant_limit;
    }
    std::vector<OriginalTdtTenant> replacement;
    replacement.reserve(added + 1U);
    if (left_remainder) {
      auto remainder = candidate;
      set_tenant_right(remainder, left, byte_swapped);
      replacement.push_back(std::move(remainder));
    }
    replacement.push_back(std::move(office));
    if (right_remainder) {
      auto remainder = candidate;
      set_tenant_left(remainder, right, byte_swapped);
      replacement.push_back(std::move(remainder));
    }
    tenants.erase(tenants.begin() + static_cast<std::ptrdiff_t>(index));
    tenants.insert(tenants.begin() + static_cast<std::ptrdiff_t>(index),
                   replacement.begin(), replacement.end());
    return OfficeInsertionResult::inserted;
  }
  return OfficeInsertionResult::occupied;
}

void enqueue_original_pending_tenant(OriginalTdtDocument& document,
                                     std::uint8_t floor,
                                     std::uint8_t key) {
  auto& tail = document.post_elevator;
  const std::uint8_t first = std::to_integer<std::uint8_t>(tail.b92e[1]);
  const std::uint8_t slot = static_cast<std::uint8_t>(
      (first + tail.b92e_counter) % 10U);
  tail.b92e[2U + slot] = static_cast<std::byte>(floor);
  tail.b92e[12U + slot] = static_cast<std::byte>(key);
  tail.b944_words[slot] = document.header.frame_time;
  ++tail.b92e_counter;
  tail.b92e[0] = static_cast<std::byte>(tail.b92e_counter);
}

struct PendingFacilityLocation {
  std::uint8_t floor{};
  std::uint8_t key{};
  std::size_t tenant_index{};
};

std::optional<PendingFacilityLocation> original_pending_location(
    const OriginalTdtDocument& document, std::uint8_t slot) {
  const auto& tail = document.post_elevator;
  const auto floor = std::to_integer<std::uint8_t>(tail.b92e[2U + slot]);
  const auto key = std::to_integer<std::uint8_t>(tail.b92e[12U + slot]);
  if (floor >= document.floors.size() ||
      key >= OriginalTdtFloor::kIndexCapacity) {
    return std::nullopt;
  }
  const auto& target_floor = document.floors[floor];
  const std::size_t tenant_index = target_floor.tenant_index[key];
  if (tenant_index >= target_floor.tenants.size() ||
      target_floor.tenants[tenant_index].exact_bytes[12] !=
          static_cast<std::byte>(key)) {
    return std::nullopt;
  }
  return PendingFacilityLocation{floor, key, tenant_index};
}

std::optional<std::size_t> original_pending_people_count(
    std::int8_t pending_type) {
  switch (pending_type) {
    case -3:
      return 2U;
    case -4:
    case -5:
      return 3U;
    case -7:
      return 6U;
    case -9:
      return 3U;
    case -10:
      return 48U;
    case -6:
    case -12:
      return 48U;
    case -14:
      return 6U;
    case -15:
      return 6U;
    case -17:
      return 6U;
    case -18:
    case -19:
      return 56U;
    case -20:
    case -21:
      return 6U;
    case -29:
    case -30:
      return 40U;
    case -31:
    case -32:
      return 6U;
    case -33:
      return 240U;
    case -36:
    case -37:
    case -38:
    case -39:
    case -40:
      return 8U;
    case -13:
      return 6U;
    default:
      return std::nullopt;
  }
}

std::uint16_t next_original_random(OriginalTdtDocument& document) noexcept {
  // Microsoft C 7.0/Visual C++ 1.x rand() at 1000:3a2f. DS:0bd4 starts at
  // one in the executable's initialized data segment.
  document.random_state = document.random_state * 0x015a4e35U + 1U;
  return static_cast<std::uint16_t>(
      (document.random_state >> 16U) & 0x7fffU);
}

const OriginalTdtTenant* original_tenant_by_key(
    const OriginalTdtDocument& document,
    std::int8_t floor_number,
    std::uint8_t key) {
  if (floor_number < 0 ||
      static_cast<std::size_t>(floor_number) >= document.floors.size() ||
      key >= OriginalTdtFloor::kIndexCapacity) {
    return nullptr;
  }
  const auto& floor =
      document.floors[static_cast<std::size_t>(floor_number)];
  const auto tenant_index = floor.tenant_index[key];
  if (tenant_index >= floor.tenants.size()) {
    return nullptr;
  }
  const auto& tenant = floor.tenants[tenant_index];
  return tenant.exact_bytes[12] == static_cast<std::byte>(key)
             ? &tenant
             : nullptr;
}

std::uint16_t allocate_original_movie_service(
    OriginalTdtDocument& document,
    std::int8_t first_floor,
    std::uint8_t first_key,
    std::int8_t second_floor,
    std::uint8_t second_key) {
  auto& records = document.post_elevator.dc24_records;
  std::size_t service_index = records.size();
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (records[index][0] == std::byte{0xfe}) {
      service_index = index;
      break;
    }
  }
  if (service_index == records.size()) {
    return 0xffffU;
  }

  auto& record = records[service_index];
  record.fill(std::byte{0});
  record[0] = static_cast<std::byte>(
      static_cast<std::uint8_t>(first_floor));
  record[1] = static_cast<std::byte>(
      static_cast<std::uint8_t>(second_floor));
  record[2] = static_cast<std::byte>(first_key);
  record[3] = static_cast<std::byte>(second_key);

  // 1180:0073 resolves whichever half is present and randomizes byte seven
  // only when that linked tenant is a transformed type 34/35 entrance.
  const bool first_present = first_floor >= 0;
  const auto* linked = original_tenant_by_key(
      document, first_present ? first_floor : second_floor,
      first_present ? first_key : second_key);
  if (linked != nullptr && (linked->type == 34 || linked->type == 35)) {
    record[7] = static_cast<std::byte>(next_original_random(document) % 14U);
  } else {
    record[7] = std::byte{0xff};
  }
  return static_cast<std::uint16_t>(service_index);
}

std::uint16_t link_original_movie_service(
    OriginalTdtDocument& document,
    std::int8_t floor_number,
    std::uint8_t key,
    std::int8_t transformed_type) {
  const auto* current = original_tenant_by_key(document, floor_number, key);
  if (current == nullptr) {
    return 0xffffU;
  }

  auto& records = document.post_elevator.dc24_records;
  if (transformed_type == 34 || transformed_type == 29) {
    // 1180:01ad joins an already-created lower half at the same x position.
    for (std::size_t index = 0; index < records.size(); ++index) {
      auto& record = records[index];
      const auto lower_floor = static_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(record[1]));
      if (lower_floor < 0 || lower_floor + 1 != floor_number) {
        continue;
      }
      const auto* lower = original_tenant_by_key(
          document, lower_floor,
          std::to_integer<std::uint8_t>(record[3]));
      if (lower != nullptr && lower->left == current->left) {
        record[0] = static_cast<std::byte>(
            static_cast<std::uint8_t>(floor_number));
        record[2] = static_cast<std::byte>(key);
        return static_cast<std::uint16_t>(index);
      }
    }
    return allocate_original_movie_service(
        document, floor_number, key, -1, 0U);
  }

  // 1180:0282 is the symmetric lower-half path for transformed type 35.
  for (std::size_t index = 0; index < records.size(); ++index) {
    auto& record = records[index];
    const auto upper_floor = static_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(record[0]));
    if (upper_floor < 0 || floor_number + 1 != upper_floor) {
      continue;
    }
    const auto* upper = original_tenant_by_key(
        document, upper_floor,
        std::to_integer<std::uint8_t>(record[2]));
    if (upper != nullptr && upper->left == current->left) {
      record[1] = static_cast<std::byte>(
          static_cast<std::uint8_t>(floor_number));
      record[3] = static_cast<std::byte>(key);
      return static_cast<std::uint16_t>(index);
    }
  }
  return allocate_original_movie_service(
      document, -1, 0U, floor_number, key);
}

bool activate_original_pending_movie(
    OriginalTdtDocument& document,
    const PendingFacilityLocation& location,
    std::uint8_t positive_type,
    std::uint32_t people_start,
    std::size_t people_count) {
  if ((positive_type != 18U && positive_type != 19U) ||
      people_count != 56U) {
    return false;
  }

  auto working = document;
  auto& floor = working.floors[location.floor];
  if (location.tenant_index >= floor.tenants.size() ||
      floor.tenants.size() >= OriginalTdtFloor::kTenantCapacity) {
    return false;
  }

  auto& pending = floor.tenants[location.tenant_index];
  pending.type = static_cast<std::int8_t>(positive_type);
  pending.exact_bytes[4] = static_cast<std::byte>(positive_type);
  pending.exact_bytes[12] = std::byte{0xff};
  pending.preserved_07_to_0f[5] = std::byte{0xff};
  pending.exact_bytes[13] = std::byte{1};
  pending.preserved_07_to_0f[6] = std::byte{1};
  pending.subtype = 0U;
  pending.exact_bytes[17] = std::byte{0};

  // 1180:0352 turns the pending body into a seven-cell entrance first.
  pending.type = static_cast<std::int8_t>(positive_type + 16U);
  pending.exact_bytes[4] = static_cast<std::byte>(positive_type + 16U);
  pending.status = 0U;
  pending.exact_bytes[5] = std::byte{0};
  pending.exact_bytes[12] = std::byte{0xff};
  pending.preserved_07_to_0f[5] = std::byte{0xff};
  pending.exact_bytes[13] = std::byte{1};
  pending.preserved_07_to_0f[6] = std::byte{1};
  pending.exact_bytes[14] = std::byte{1};
  pending.preserved_07_to_0f[7] = std::byte{1};
  pending.exact_bytes[15] = std::byte{2};
  pending.preserved_07_to_0f[8] = std::byte{2};
  pending.rent_rate = 4U;
  pending.exact_bytes[16] = std::byte{4};
  pending.subtype = 0U;
  pending.exact_bytes[17] = std::byte{0};

  const auto entrance_key = first_original_floor_key(floor);
  if (!entrance_key) {
    return false;
  }
  pending.exact_bytes[12] = static_cast<std::byte>(*entrance_key);
  pending.preserved_07_to_0f[5] = static_cast<std::byte>(*entrance_key);
  rebuild_original_floor_lookup(floor);

  for (std::size_t index = 0; index < people_count; ++index) {
    auto& exact = working.people[people_start + index].exact_bytes;
    exact.fill(std::byte{0});
    exact[0] = static_cast<std::byte>(location.floor);
    exact[1] = static_cast<std::byte>(*entrance_key);
    store_u16(exact, 2, static_cast<std::uint16_t>(index),
              working.header.byte_swapped);
    exact[4] = std::byte{18};
    exact[5] = std::byte{0x27};
  }

  const auto service_index = link_original_movie_service(
      working, static_cast<std::int8_t>(location.floor), *entrance_key,
      pending.type);
  pending.variant = static_cast<std::uint8_t>(service_index);
  pending.exact_bytes[6] = static_cast<std::byte>(service_index);
  pending.exact_bytes[7] = static_cast<std::byte>(service_index >> 8U);
  pending.preserved_07_to_0f[0] =
      static_cast<std::byte>(service_index >> 8U);
  set_tenant_right(pending, static_cast<std::uint16_t>(pending.left + 7U),
                   working.header.byte_swapped);

  // The original shifts later records upward without clearing the vacated
  // slot. Preserve those stale bytes before overwriting the fields that
  // 1180:0352 explicitly initializes.
  OriginalTdtTenant body =
      location.tenant_index + 1U < floor.tenants.size()
          ? floor.tenants[location.tenant_index + 1U]
          : OriginalTdtTenant{};
  set_tenant_left(body, pending.right, working.header.byte_swapped);
  set_tenant_right(body, static_cast<std::uint16_t>(pending.right + 24U),
                   working.header.byte_swapped);
  body.type = static_cast<std::int8_t>(positive_type);
  body.exact_bytes[4] = static_cast<std::byte>(positive_type);
  body.status = 0U;
  body.exact_bytes[5] = std::byte{0};
  body.exact_bytes[12] = std::byte{0xff};
  body.preserved_07_to_0f[5] = std::byte{0xff};
  body.exact_bytes[13] = std::byte{1};
  body.preserved_07_to_0f[6] = std::byte{1};
  body.exact_bytes[14] = std::byte{1};
  body.preserved_07_to_0f[7] = std::byte{1};
  body.exact_bytes[15] = std::byte{2};
  body.preserved_07_to_0f[8] = std::byte{2};
  body.rent_rate = 4U;
  body.exact_bytes[16] = std::byte{4};
  body.subtype = 0U;
  body.exact_bytes[17] = std::byte{0};

  floor.tenants.insert(
      floor.tenants.begin() +
          static_cast<std::ptrdiff_t>(location.tenant_index + 1U),
      std::move(body));
  const auto body_key = first_original_floor_key(floor);
  if (!body_key) {
    return false;
  }
  auto& inserted = floor.tenants[location.tenant_index + 1U];
  inserted.exact_bytes[12] = static_cast<std::byte>(*body_key);
  inserted.preserved_07_to_0f[5] = static_cast<std::byte>(*body_key);
  const auto& transformed = floor.tenants[location.tenant_index];
  inserted.variant = transformed.variant;
  inserted.exact_bytes[6] = transformed.exact_bytes[6];
  inserted.exact_bytes[7] = transformed.exact_bytes[7];
  inserted.preserved_07_to_0f[0] = transformed.preserved_07_to_0f[0];
  rebuild_original_floor_lookup(floor);

  auto& tail = working.post_elevator;
  const auto first = std::to_integer<std::uint8_t>(tail.b92e[1]);
  tail.b92e[1] = static_cast<std::byte>((first + 1U) % 10U);
  --tail.b92e_counter;
  tail.b92e[0] = static_cast<std::byte>(tail.b92e_counter);
  document = std::move(working);
  return true;
}

std::uint16_t allocate_original_commercial_service(
    OriginalTdtDocument& document,
    std::uint8_t floor,
    std::uint8_t key,
    std::uint8_t type) {
  std::size_t service_index = document.retail.size();
  for (std::size_t index = 0; index < document.retail.size(); ++index) {
    if (document.retail[index].exact_bytes[0] == std::byte{0xff}) {
      service_index = index;
      break;
    }
  }
  if (service_index == document.retail.size()) {
    return 0xffffU;
  }

  auto& exact = document.retail[service_index].exact_bytes;
  exact[0] = static_cast<std::byte>(floor);
  exact[1] = static_cast<std::byte>(key);

  // 11a8:1812 is false for Restaurant. For Fast Food it is true only during
  // day phase zero after frame 0xf0. Type 10 is allocated later by its rental
  // path, not by deferred construction activation.
  const bool initially_open =
      type == 12U && original_day_phase(document.header.frame_time) == 0 &&
      document.header.frame_time > 0x00f0U;
  exact[2] = initially_open ? std::byte{0} : std::byte{3};
  exact[3] = std::byte{0x0a};
  exact[4] = std::byte{0x0a};
  exact[5] = std::byte{0x0a};
  if (initially_open) {
    const std::size_t lane = document.header.version_20_word != 0U
                                 ? 5U
                                 : (original_calendar_phase(
                                            document.header.current_day) == 0U
                                        ? 3U
                                        : 4U);
    exact[lane] = std::byte{0};
    exact[6] = std::byte{0x0a};
    exact[7] = std::byte{0};
    exact[8] = std::byte{0x0a};
    store_u16(exact, 12, 0xfff5U, document.header.byte_swapped);
    // 11a8:09b4-09f4 forwards the initialized byte-eight population to
    // 1060:07f7. That immediately raises both Fast Food's category total and
    // the overall population by ten when a newly activated service starts
    // open; it is not deferred until the first visitor callback.
    add_original_population_for_type(
        document, type,
        static_cast<std::int16_t>(
            std::to_integer<std::int8_t>(exact[8])));
  } else {
    exact[6] = std::byte{0};
    exact[7] = std::byte{0x0a};
    exact[8] = std::byte{0};
    store_u16(exact, 12, 0xffffU, document.header.byte_swapped);
  }
  exact[9] = std::byte{0};
  exact[10] = std::byte{0};
  auto& selector = type == 6U ? document.restaurant_service_variant
                              : document.fast_food_service_variant;
  exact[11] = static_cast<std::byte>(selector);
  selector = static_cast<std::uint16_t>((selector + 1U) % 5U);
  store_u16(exact, 16, 0U, document.header.byte_swapped);

  // 11a8:1596 groups floors by the signed IDIV of (floor - 5) / 15 and
  // appends the allocated service index to the type-specific persisted block.
  const int shifted_floor = static_cast<int>(floor) - 5;
  const int group = shifted_floor / 15;
  const int remainder = shifted_floor % 15;
  if (group >= 0 && group < 7 && remainder <= 9) {
    auto append_index = [&](std::span<std::byte> block,
                            std::size_t group_size) {
      const std::size_t base = static_cast<std::size_t>(group) * group_size;
      const auto count = load_u16(block, base, document.header.byte_swapped);
      const std::size_t destination = base + 2U + count * 2U;
      if (destination + 2U <= base + group_size) {
        store_u16(block, destination,
                  static_cast<std::uint16_t>(service_index),
                  document.header.byte_swapped);
        store_u16(block, base, static_cast<std::uint16_t>(count + 1U),
                  document.header.byte_swapped);
      }
    };
    if (type == 6U) {
      append_index(document.post_elevator.dynamic_dd60, 0x12eU);
    } else {
      append_index(document.post_elevator.dynamic_dd64, 0x1ceU);
    }
  }
  return static_cast<std::uint16_t>(service_index);
}

std::uint16_t allocate_original_medical_service(
    OriginalTdtDocument& document,
    std::uint8_t floor,
    std::uint8_t key) {
  // Exact 1170:01bf Medical-service allocator: claim the first free DBFC
  // record, use 1170:0681 to append its index to BD5C/BD5A, and add it to the
  // floor-group route.
  std::size_t service_index = document.post_elevator.dbfc_dwords.size();
  for (std::size_t index = 0;
       index < document.post_elevator.dbfc_dwords.size(); ++index) {
    if ((document.post_elevator.dbfc_dwords[index] & 0xffU) == 0xffU) {
      service_index = index;
      break;
    }
  }
  if (service_index == document.post_elevator.dbfc_dwords.size()) {
    return 0xffffU;
  }
  document.post_elevator.dbfc_dwords[service_index] =
      static_cast<std::uint32_t>(floor) |
      (static_cast<std::uint32_t>(key) << 8U);

  auto& tail = document.post_elevator;
  if (tail.bd5a_count < tail.bd5c_entries.size()) {
    tail.bd5c_entries[tail.bd5a_count] =
        static_cast<std::uint16_t>(service_index);
    ++tail.bd5a_count;
  }

  const int shifted_floor = static_cast<int>(floor) - 5;
  const int group = shifted_floor / 15;
  const int remainder = shifted_floor % 15;
  if (group >= 0 && group < 7 && remainder <= 9) {
    constexpr std::size_t kGroupSize = 0x16U;
    const std::size_t base = static_cast<std::size_t>(group) * kGroupSize;
    auto route = std::span<std::byte>(document.medical_route_index);
    const auto count = load_u16(route, base, document.header.byte_swapped);
    const std::size_t destination = base + 2U + count * 2U;
    if (destination + 2U <= base + kGroupSize) {
      store_u16(route, destination,
                static_cast<std::uint16_t>(service_index),
                document.header.byte_swapped);
      store_u16(route, base, static_cast<std::uint16_t>(count + 1U),
                document.header.byte_swapped);
    }
  }
  return static_cast<std::uint16_t>(service_index);
}

bool activate_original_pending_facility(OriginalTdtDocument& document) {
  auto& tail = document.post_elevator;
  if (tail.b92e_counter == 0U) {
    return false;
  }
  const auto first = std::to_integer<std::uint8_t>(tail.b92e[1]);
  if (first >= 10U) {
    return false;
  }
  const auto location = original_pending_location(document, first);
  if (!location) {
    return false;
  }
  auto& floor = document.floors[location->floor];
  auto& tenant = floor.tenants[location->tenant_index];
  const auto people_count = original_pending_people_count(tenant.type);
  if (!people_count) {
    return false;
  }

  const auto positive_type = static_cast<std::uint8_t>(-tenant.type);
  const auto people_start = load_u32(
      std::span<const std::byte>(tenant.exact_bytes), 8,
      document.header.byte_swapped);
  if (people_start > document.people.size() ||
      document.people.size() - people_start < *people_count) {
    return false;
  }
  // 11e8:0000 is the type-33 specialization reached from 11f0:00a0. Its
  // two single-record copies assume the normal Metro bottom is the sole
  // floor-zero tenant and needs two free slots in the fixed 150-record block.
  if (positive_type == 33U &&
      (location->tenant_index != 0U || floor.tenants.size() != 1U ||
       floor.tenants.size() + 2U > OriginalTdtFloor::kTenantCapacity)) {
    return false;
  }

  if (positive_type == 18U || positive_type == 19U) {
    return activate_original_pending_movie(
        document, *location, positive_type, people_start, *people_count);
  }

  // 11f0:00a0 restores the positive type and resets these common bytes
  // before calling the type-specific 1228:0103 branch.
  tenant.type = static_cast<std::int8_t>(positive_type);
  tenant.exact_bytes[4] = static_cast<std::byte>(positive_type);
  tenant.exact_bytes[12] = std::byte{0xff};
  tenant.preserved_07_to_0f[5] = std::byte{0xff};
  tenant.exact_bytes[13] = std::byte{1};
  tenant.preserved_07_to_0f[6] = std::byte{1};
  tenant.subtype = 0;
  tenant.exact_bytes[17] = std::byte{0};

  if (positive_type == 17U || positive_type == 20U ||
      positive_type == 21U || positive_type == 31U ||
      positive_type == 32U) {
    // Types 17, 20, 21, 31, and 32 dispatch to 1228:075b. Unlike every
    // translated ordinary family, that branch does not call 1228:0d9a and
    // therefore retains the 11f0 reset value 0xff in the lookup-key byte. It
    // also never calls a 1220 people initializer, so their reservation
    // records remain byte-exact pending records with the original negative
    // type.
    rebuild_original_floor_lookup(floor);
    tail.b92e[1] = static_cast<std::byte>((first + 1U) % 10U);
    --tail.b92e_counter;
    tail.b92e[0] = static_cast<std::byte>(tail.b92e_counter);
    return true;
  }

  const auto new_key = first_original_floor_key(floor);
  if (!new_key) {
    return false;
  }
  tenant.exact_bytes[12] = static_cast<std::byte>(*new_key);
  tenant.preserved_07_to_0f[5] = static_cast<std::byte>(*new_key);
  // 1228:01e6 selects 0x18/0x20 for Hotel and Condo while 1228:0164
  // selects 0x10/0x18 for Office. Both read the DS:b3a1 day phase produced
  // by 1200:0543, rather than the tower rating at DS:b3cc.
  const bool before_phase_four =
      original_day_phase(document.header.frame_time) < 4;
  if (positive_type == 7U) {
    tenant.status = before_phase_four ? 0x10U : 0x18U;
    tenant.exact_bytes[5] = static_cast<std::byte>(tenant.status);
  } else if ((positive_type >= 3U && positive_type <= 5U) ||
             positive_type == 9U) {
    tenant.status = before_phase_four ? 0x18U : 0x20U;
    tenant.exact_bytes[5] = static_cast<std::byte>(tenant.status);
  }
  // Type 10's 1228:02c2 branch deliberately does not write tenant status.
  // The zero supplied by 11f8:0c9c therefore survives activation.

  // 1220:049a first clears dynamic bytes 4..15 for the tenant-owned span.
  // Exact 1220:067c then performs the common activated-facility person-record
  // initializer; family wrappers supply the initial type/state bytes below.
  if (positive_type != 13U) {
    for (std::size_t index = 0; index < *people_count; ++index) {
      auto& exact = document.people[people_start + index].exact_bytes;
      exact.fill(std::byte{0});
      exact[0] = static_cast<std::byte>(location->floor);
      exact[1] = static_cast<std::byte>(*new_key);
      store_u16(exact, 2, static_cast<std::uint16_t>(index),
                document.header.byte_swapped);
      // Exact 1220:0c72 supplies Restaurant/Fast Food's tenant type with
      // state/byte6/byte7 20/FE/00. 1220:0cec supplies Security family/state
      // 14/01, 1220:0d13 supplies
      // Movie family/state 18/27, and 1220:0d88 supplies Metro family/state
      // 33/01 to the common 08fb initializer. 1220:0d3a supplies family 29
      // to both Party Hall halves; the lower type-30 tenant therefore owns
      // type-29 people records, just like both Movie Theater halves own
      // type-18 people records. 1220:0d61 similarly supplies family 36 for
      // every part of the five-story Cathedral.
      const bool cathedral_part =
          positive_type >= 36U && positive_type <= 40U;
      exact[4] = static_cast<std::byte>(
          (positive_type == 29U || positive_type == 30U) ? 29U
          : (cathedral_part ? 36U : positive_type));
      exact[5] = positive_type == 14U || positive_type == 33U
                     ? std::byte{1}
                     : (positive_type == 15U
                            ? std::byte{0}
                            : ((positive_type == 29U || positive_type == 30U ||
                                cathedral_part)
                                   ? std::byte{0x27}
                                   : std::byte{0x20}));
      // 1220:0be3 initializes Condo people through 1220:08fb with 0x0a.
      // Hotel, Office, Retail, and exact Metro wrapper 1220:0d88 pass 0xfe
      // to that common initializer; exact Security 1220:0cec and Movie
      // 1220:0d13 pass zero.
      exact[6] = (positive_type == 14U || positive_type == 29U ||
                  positive_type == 30U || cathedral_part)
                     ? std::byte{0}
                     : (positive_type == 9U ? std::byte{0x0a}
                                            : std::byte{0xfe});
      // 1220:0cc5 is the only translated wrapper that supplies -1 for the
      // final initializer byte. All other currently constructible families
      // leave byte 7 at zero.
      exact[7] = positive_type == 15U ? std::byte{0xff} : std::byte{0};
    }
  }
  // 1220:0b4e overwrites the first guest's state after the common 08fb
  // initializer. Office uses 1220:0b27 and leaves all six at 0x20.
  if (positive_type >= 3U && positive_type <= 5U) {
    document.people[people_start].exact_bytes[5] = std::byte{0x24};
  } else if (positive_type == 9U) {
    // 1220:0be3 then overwrites only the second of the three Condo people.
    document.people[people_start + 1U].exact_bytes[6] = std::byte{0xfe};
  }
  if (positive_type == 6U || positive_type == 12U) {
    const auto service_index = allocate_original_commercial_service(
        document, location->floor, *new_key, positive_type);
    tenant.variant = static_cast<std::uint8_t>(service_index);
    tenant.exact_bytes[6] = static_cast<std::byte>(service_index);
    tenant.exact_bytes[7] = static_cast<std::byte>(service_index >> 8U);
    tenant.preserved_07_to_0f[0] =
        static_cast<std::byte>(service_index >> 8U);
  }
  if (positive_type == 29U || positive_type == 30U) {
    const auto service_index = link_original_movie_service(
        document, static_cast<std::int8_t>(location->floor), *new_key,
        static_cast<std::int8_t>(positive_type));
    tenant.variant = static_cast<std::uint8_t>(service_index);
    tenant.exact_bytes[6] = static_cast<std::byte>(service_index);
    tenant.exact_bytes[7] = static_cast<std::byte>(service_index >> 8U);
    tenant.preserved_07_to_0f[0] =
        static_cast<std::byte>(service_index >> 8U);
  }
  if (positive_type == 14U || positive_type == 15U) {
    // 10f8:002d stores the floor/key pair in the first free ten-word slot.
    for (auto& slot : document.post_elevator.cf88_words) {
      if ((slot & 0x00ffU) == 0x00ffU) {
        slot = static_cast<std::uint16_t>(location->floor) |
               (static_cast<std::uint16_t>(*new_key) << 8U);
        break;
      }
    }
  }
  if (positive_type == 13U) {
    const auto service_index = allocate_original_medical_service(
        document, location->floor, *new_key);
    tenant.variant = static_cast<std::uint8_t>(service_index);
    tenant.exact_bytes[6] = static_cast<std::byte>(service_index);
    tenant.exact_bytes[7] = static_cast<std::byte>(service_index >> 8U);
    tenant.preserved_07_to_0f[0] =
        static_cast<std::byte>(service_index >> 8U);
  }
  if (positive_type == 40U) {
    // 1228:06b2 replaces the construction-time floor marker at DS:b3ec
    // with the activated bottom Cathedral tenant's lookup key. Cathedral
    // simulation then uses this key against hard-coded floor 109.
    store_original_header_word(document, 34U, *new_key);
  }
  if (positive_type == 33U) {
    // Exact 11e8:0000 Metro-bottom expansion. After 1228:0103 initializes
    // the real type-33 tenant, the original duplicates it to slot one,
    // overwrites slot zero with an invisible type-45 span, creates a second
    // type-45 span in slot two, and publishes floor edges 0..0x177. The left
    // sentinel deliberately inherits bytes 8..11 from the Metro tenant;
    // the right sentinel comes from the floor block's zero-filled free slot.
    const OriginalTdtTenant metro = tenant;
    const auto make_boundary = [&](std::uint16_t boundary_left,
                                   std::uint16_t boundary_right,
                                   OriginalTdtTenant boundary) {
      boundary.left = boundary_left;
      boundary.right = boundary_right;
      boundary.type = 45;
      boundary.status = 0U;
      boundary.variant = 0U;
      boundary.rent_rate = 4U;
      boundary.subtype = 0U;
      store_u16(boundary.exact_bytes, 0U, boundary_left,
                document.header.byte_swapped);
      store_u16(boundary.exact_bytes, 2U, boundary_right,
                document.header.byte_swapped);
      boundary.exact_bytes[4] = std::byte{45};
      boundary.exact_bytes[5] = std::byte{0};
      store_u16(boundary.exact_bytes, 6U, 0U,
                document.header.byte_swapped);
      boundary.exact_bytes[12] = std::byte{0xff};
      boundary.exact_bytes[13] = std::byte{1};
      boundary.exact_bytes[14] = std::byte{1};
      boundary.exact_bytes[15] = std::byte{2};
      boundary.exact_bytes[16] = std::byte{4};
      boundary.exact_bytes[17] = std::byte{0};
      std::copy(boundary.exact_bytes.begin() + 7U,
                boundary.exact_bytes.begin() + 16U,
                boundary.preserved_07_to_0f.begin());
      return boundary;
    };
    OriginalTdtTenant right_boundary{};
    right_boundary.exact_bytes.fill(std::byte{0});
    floor.tenants = {
        make_boundary(0U, metro.left, metro), metro,
        make_boundary(metro.right, 0x0177U, right_boundary)};
    // Final 11e8:0240-0267 writes the six-byte floor-block header directly.
    floor.left_edge = 0U;
    floor.right_edge = 0x0177U;
  }
  rebuild_original_floor_lookup(floor);

  tail.b92e[1] = static_cast<std::byte>((first + 1U) % 10U);
  --tail.b92e_counter;
  tail.b92e[0] = static_cast<std::byte>(tail.b92e_counter);
  return true;
}

void set_original_tenant_status(OriginalTdtTenant& tenant,
                                std::uint8_t status) {
  tenant.status = status;
  tenant.exact_bytes[5] = static_cast<std::byte>(status);
}

void mark_original_tenant_changed(OriginalTdtTenant& tenant) {
  tenant.exact_bytes[13] = std::byte{1};
  tenant.preserved_07_to_0f[6] = std::byte{1};
}

void update_original_parking_side(OriginalTdtFloor& floor,
                                  std::size_t begin,
                                  std::size_t end,
                                  int direction,
                                  bool connected) {
  // Exact two-direction 1198:09ce Parking connectivity scan. A non-Parking
  // tenant or an empty span at least four cells wide cuts connectivity for
  // every farther Parking unit; changed status bytes also set dirty byte 19.
  for (std::size_t cursor = begin; cursor != end;
       cursor = static_cast<std::size_t>(
           static_cast<std::ptrdiff_t>(cursor) + direction)) {
    auto& tenant = floor.tenants[cursor];
    if (tenant.type == 11) {
      if (connected && tenant.status == 1U) {
        set_original_tenant_status(tenant, 0U);
        mark_original_tenant_changed(tenant);
      } else if (!connected && tenant.status != 1U) {
        set_original_tenant_status(tenant, 1U);
        mark_original_tenant_changed(tenant);
      }
    } else if (tenant.type == 0 && tenant.right - tenant.left >= 4U) {
      connected = false;
    }
  }
}

void update_original_parking_around(OriginalTdtFloor& floor,
                                    std::size_t ramp_index,
                                    bool connected) {
  if (ramp_index > 0U) {
    update_original_parking_side(floor, ramp_index - 1U,
                                 static_cast<std::size_t>(-1), -1,
                                 connected);
  }
  if (ramp_index < floor.tenants.size()) {
    update_original_parking_side(floor, ramp_index + 1U,
                                 floor.tenants.size(), 1, connected);
  }
}

void activate_all_original_pending_facilities(OriginalTdtDocument& document) {
  // 1198:01ab starts with 11f0:0016, which activates the queue count captured
  // on entry. Every currently constructible family has an exact native
  // activation path; stop only on a malformed or unsupported record.
  const auto pending = document.post_elevator.b92e_counter;
  for (std::uint8_t index = 0; index < pending; ++index) {
    if (!activate_original_pending_facility(document)) {
      break;
    }
  }
}

void rebuild_original_parking_population_limits(
    OriginalTdtDocument& document) noexcept {
  // Exact 1198:00d9 writes into the second b846 series after b958 has been
  // rebuilt. Categories 0 and 3 receive twice the connected-space count;
  // categories 1, 2 and 4..8 receive the count. The original accidentally
  // writes category 4 twice and never writes category 9, then includes that
  // preserved category-9 value in the wrapping ten-category total.
  auto& limits = document.post_elevator.b846_series[1];
  const auto count = static_cast<std::int32_t>(
      document.post_elevator.parking_connected);
  const auto count_word = static_cast<std::uint16_t>(
      document.post_elevator.parking_connected);
  const auto doubled_word = static_cast<std::uint16_t>(
      count_word + count_word);
  const auto doubled = static_cast<std::int32_t>(
      std::bit_cast<std::int16_t>(doubled_word));

  limits[0] = doubled;
  limits[1] = count;
  limits[2] = count;
  limits[3] = doubled;
  limits[4] = count;
  limits[5] = count;
  limits[4] = count;  // 1198:0124-012b repeats b882, byte-exact quirk.
  limits[6] = count;
  limits[7] = count;
  limits[8] = count;

  std::uint32_t total = 0U;
  for (std::size_t index = 0; index < 10U; ++index) {
    total += std::bit_cast<std::uint32_t>(limits[index]);
  }
  limits[10] = std::bit_cast<std::int32_t>(total);
}

void rebuild_original_parking_index(OriginalTdtDocument& document) {
  activate_all_original_pending_facilities(document);

  auto& tail = document.post_elevator;
  // Exact 1198:07a5 clears DS:b958 and all 512 following word entries before
  // the live-ramp scan repopulates the derived index.
  tail.parking_connected = 0;
  tail.parking_entries.fill(0U);
  auto parking_count = load_original_header_word(document, 50U);  // DS:b3fc

  for (std::size_t index = 0; index < tail.cf9c_records.size(); ++index) {
    auto& record = tail.cf9c_records[index];
    const auto floor_number = static_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(record[0]));
    if (floor_number < 0) {
      continue;
    }
    const auto key = static_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(record[1]));
    if (key == -1) {
      record[0] = std::byte{0xff};
      if (parking_count != 0U) {
        --parking_count;
      }
      continue;
    }
    if (floor_number >= static_cast<std::int8_t>(document.floors.size()) ||
        key < 0 || key >= static_cast<std::int8_t>(
                             OriginalTdtFloor::kIndexCapacity)) {
      continue;
    }
    const auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
    const auto tenant_index = floor.tenant_index[static_cast<std::size_t>(key)];
    if (tenant_index < floor.tenants.size() &&
        floor.tenants[tenant_index].status != 1U &&
        tail.parking_connected <
            static_cast<std::int16_t>(tail.parking_entries.size())) {
      // Exact 1198:07c3 b958 append primitive.
      tail.parking_entries[static_cast<std::size_t>(tail.parking_connected)] =
          static_cast<std::uint16_t>(index);
      ++tail.parking_connected;
    }
  }
  store_original_header_word(document, 50U, parking_count);
  rebuild_original_parking_population_limits(document);
}

void rebuild_original_parking_connectivity(OriginalTdtDocument& document) {
  bool connected = false;
  std::int16_t last_ramp_x = -1;

  for (int floor_number = 9; floor_number >= 0; --floor_number) {
    auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
    bool found_ramp = false;
    for (std::size_t index = 0; index < floor.tenants.size(); ++index) {
      auto& ramp = floor.tenants[index];
      if (ramp.type != 0x2c) {
        continue;
      }
      found_ramp = true;
      last_ramp_x = static_cast<std::int16_t>(ramp.left);
      if (floor_number == 9) {
        connected = true;
      }
      set_original_tenant_status(ramp, 0U);

      if (floor_number > 0) {
        const auto& below =
            document.floors[static_cast<std::size_t>(floor_number - 1)];
        const bool continues = std::any_of(
            below.tenants.begin(), below.tenants.end(),
            [&](const OriginalTdtTenant& candidate) {
              return candidate.type == 0x2c && candidate.left == ramp.left;
            });
        if (continues) {
          set_original_tenant_status(ramp, floor_number == 9 ? 2U : 1U);
        }
      }

      mark_original_tenant_changed(ramp);
      update_original_parking_around(floor, index, connected);
      if (ramp.status == 0U) {
        connected = false;
      }
    }
    if (!found_ramp) {
      update_original_parking_around(floor, floor.tenants.size(), false);
    }
  }

  // DS:b3ee is persisted in the header and is the x-coordinate gate used by
  // 11f8:0aa0 before another ramp drag may begin.
  store_original_header_word(document, 36U,
                             static_cast<std::uint16_t>(last_ramp_x));
  rebuild_original_parking_index(document);
}

void set_lobby_tenant_bounds(OriginalTdtTenant& tenant,
                             std::uint16_t left,
                             std::uint16_t right,
                             bool byte_swapped) {
  tenant.left = left;
  tenant.right = right;
  auto exact = std::span<std::byte>(tenant.exact_bytes);
  store_u16(exact, 0, left, byte_swapped);
  store_u16(exact, 2, right, byte_swapped);
}

bool overlaps(std::uint16_t left_a, std::uint16_t right_a,
              std::uint16_t left_b, std::uint16_t right_b) {
  return left_a < right_b && left_b < right_a;
}

bool overlaps_signed(int left_a, int right_a,
                     int left_b, int right_b) noexcept {
  return left_a < right_b && left_b < right_a;
}

bool rectangles_overlap(int left_a, int top_a, int right_a, int bottom_a,
                         int left_b, int top_b, int right_b,
                         int bottom_b) noexcept {
  return overlaps_signed(left_a, right_a, left_b, right_b) &&
         overlaps_signed(top_a, bottom_a, top_b, bottom_b);
}

void update_floor_bounds(OriginalTdtFloor& floor);

bool original_elevator_floor_service_allowed(
    const OriginalTdtDocument& document,
    const OriginalTdtElevator& elevator,
    std::int16_t floor) noexcept {
  // Literal 10a0:1296. A type-zero shaft above floor ten may stop only at
  // 12e0's 24,39,54,... sequence. Every shaft rejects automatic upper Lobby
  // stories [11,10+lobby_height).
  if (elevator.type == 0U && floor > 10 && (floor - 9) % 15 != 0) {
    return false;
  }
  return floor < 11 ||
         floor >= static_cast<std::int16_t>(
                      10U + document.header.lobby_height);
}

std::uint16_t original_elevator_width(const OriginalTdtElevator& elevator) {
  return elevator.type == 0U ? 6U : 4U;
}

bool original_elevator_extension_collision(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t bottom,
    std::int16_t top) noexcept {
  const auto& source = document.elevators[elevator_index];
  const int width = original_elevator_width(source);
  const int candidate_left = static_cast<int>(source.x) - 8;
  const int candidate_right = static_cast<int>(source.x) + width + 8;
  const int candidate_top = static_cast<int>(bottom) - 2;
  const int candidate_bottom = static_cast<int>(top) + 1;

  // Literal 10a0:10e8: the edited shaft is skipped, the candidate rectangle
  // is expanded by eight cells horizontally, and existing shaft/stair
  // rectangles remain unexpanded.
  for (std::size_t index = 0; index < document.elevators.size(); ++index) {
    if (index == elevator_index) continue;
    const auto& candidate = document.elevators[index];
    if (candidate.used == 0U) continue;
    const int candidate_width = original_elevator_width(candidate);
    if (rectangles_overlap(
            candidate_left, candidate_top, candidate_right, candidate_bottom,
            candidate.x, static_cast<int>(candidate.bottom_floor) - 2,
            static_cast<int>(candidate.x) + candidate_width,
            static_cast<int>(candidate.top_floor) + 1)) {
      return true;
    }
  }
  for (const auto& stair : document.post_elevator.stairs_bd70) {
    if (stair.used == 0U) continue;
    const int height = original_signed_shape_half(stair.shape);
    if (rectangles_overlap(
            candidate_left, candidate_top, candidate_right, candidate_bottom,
            stair.x, static_cast<int>(stair.floor) - 1,
            static_cast<int>(stair.x) + 8,
            static_cast<int>(stair.floor) + height + 1)) {
      return true;
    }
  }
  return false;
}

std::uint64_t original_floor_extension_cost(
    const OriginalTdtDocument& document,
    std::int16_t first_floor,
    std::int16_t last_floor,
    std::uint16_t left,
    std::uint16_t right,
    const OriginalYenTable& construction_costs) {
  std::uint64_t total = 0U;
  for (int floor = first_floor; floor <= last_floor; ++floor) {
    const auto& record = document.floors[static_cast<std::size_t>(floor)];
    const std::uint32_t added_cells = record.tenants.empty()
        ? static_cast<std::uint32_t>(right - left)
        : static_cast<std::uint32_t>(
              record.left_edge - std::min(record.left_edge, left)) +
              static_cast<std::uint32_t>(
                  std::max(record.right_edge, right) - record.right_edge);
    std::uint64_t cell_cost = construction_costs[0];
    if (floor >= 10 &&
        floor < 10 + static_cast<int>(document.header.lobby_height)) {
      cell_cost = static_cast<std::uint64_t>(construction_costs[0x18]) *
                  document.header.lobby_height;
    }
    total += static_cast<std::uint64_t>(added_cells) * cell_cost;
  }
  return total;
}

bool ensure_original_elevator_floor_coverage(
    OriginalTdtDocument& document,
    const OriginalTdtElevator& elevator) {
  const auto width = original_elevator_width(elevator);
  const auto left = elevator.x;
  const auto right = static_cast<std::uint16_t>(left + width);
  for (int floor_number = elevator.bottom_floor;
       floor_number <= elevator.top_floor; ++floor_number) {
    auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
    const bool lobby_story = document.header.lobby_height != 0U &&
        floor_number >= 10 &&
        floor_number < 10 + static_cast<int>(document.header.lobby_height);
    const std::int8_t inserted_type = lobby_story ? 0x18 : 0;

    if (floor.tenants.empty()) {
      floor.tenants.push_back(
          lobby_story
              ? make_lobby_tenant(left, right, document.header.byte_swapped)
              : make_original_empty_floor_tenant(
                    left, right, document.header.byte_swapped));
    } else {
      if (left < floor.left_edge) {
        if (floor.tenants.front().type == inserted_type) {
          set_tenant_left(floor.tenants.front(), left,
                          document.header.byte_swapped);
        } else {
          if (floor.tenants.size() >= OriginalTdtFloor::kTenantCapacity) {
            return false;
          }
          auto tenant = lobby_story
              ? make_lobby_tenant(left, floor.left_edge,
                                  document.header.byte_swapped)
              : make_original_empty_floor_tenant(
                    left, floor.left_edge, document.header.byte_swapped);
          if (lobby_story) {
            const auto key = first_original_floor_key(floor);
            if (!key) return false;
            tenant.exact_bytes[12] = static_cast<std::byte>(*key);
            tenant.preserved_07_to_0f[5] = static_cast<std::byte>(*key);
          }
          floor.tenants.insert(floor.tenants.begin(), std::move(tenant));
        }
      }
      if (right > floor.right_edge) {
        if (floor.tenants.back().type == inserted_type) {
          set_tenant_right(floor.tenants.back(), right,
                           document.header.byte_swapped);
        } else {
          if (floor.tenants.size() >= OriginalTdtFloor::kTenantCapacity) {
            return false;
          }
          auto tenant = lobby_story
              ? make_lobby_tenant(floor.right_edge, right,
                                  document.header.byte_swapped)
              : make_original_empty_floor_tenant(
                    floor.right_edge, right, document.header.byte_swapped);
          if (lobby_story) {
            const auto key = first_original_floor_key(floor);
            if (!key) return false;
            tenant.exact_bytes[12] = static_cast<std::byte>(*key);
            tenant.preserved_07_to_0f[5] = static_cast<std::byte>(*key);
          }
          floor.tenants.push_back(std::move(tenant));
        }
      }
    }
    update_floor_bounds(floor);
    rebuild_original_floor_lookup(floor);
  }
  return true;
}

void rebuild_original_elevator_floor_records(
    OriginalTdtElevator& elevator,
    std::int8_t new_bottom,
    std::int8_t new_top) {
  auto old_records = std::move(elevator.floor_records);
  std::vector<OriginalTdtElevatorFloorRecord> records;
  for (int floor = new_bottom; floor <= new_top; ++floor) {
    const auto mapped = original_elevator_floor_record_index(
        elevator.type, new_bottom, new_top, floor);
    if (mapped < 0) continue;
    const auto found = std::find_if(
        old_records.begin(), old_records.end(),
        [&](const OriginalTdtElevatorFloorRecord& record) {
          return record.floor == floor;
        });
    if (found != old_records.end()) {
      records.push_back(std::move(*found));
    } else {
      records.emplace_back();
    }
    records.back().mapped_index = mapped;
    records.back().floor = static_cast<std::int8_t>(floor);
  }
  elevator.floor_records = std::move(records);
}

bool original_vertical_landing_type(std::int8_t type) noexcept {
  // Literal word tables at 10c0:0855 and 10c0:095b. Both support tests use
  // the same ten tenant types before accepting an Escalator landing.
  constexpr std::array<std::int8_t, 10> kLandingTypes = {
      0, 6, 10, 12, 18, 19, 24, 29, 30, 31,
  };
  return std::find(kLandingTypes.begin(), kLandingTypes.end(), type) !=
         kLandingTypes.end();
}

bool original_vertical_upper_landing(const OriginalTdtFloor& floor,
                                     std::uint16_t x,
                                     std::uint8_t shape) {
  const int right = static_cast<int>(x) + 8;
  // 10c0:0775 deliberately requires space strictly beyond the right edge.
  if (floor.tenants.empty() || floor.left_edge > x ||
      floor.right_edge <= right) {
    return false;
  }
  if ((shape & 1U) != 0U) {
    return true;
  }
  for (const auto& tenant : floor.tenants) {
    if (tenant.right >= right) {
      return original_vertical_landing_type(tenant.type);
    }
  }
  return false;
}

bool original_vertical_lower_landing(const OriginalTdtFloor& floor,
                                     std::uint16_t x,
                                     std::uint8_t shape) {
  const int right = static_cast<int>(x) + 8;
  // Exact 10c0:087d lower-landing test. Unlike 10c0:0775's upper landing,
  // equality at the floor's right edge is accepted; Escalators then use the
  // first tenant whose right edge reaches x+1 and the shared ten-type table.
  if (floor.tenants.empty() || floor.left_edge > x ||
      floor.right_edge < right) {
    return false;
  }
  if ((shape & 1U) != 0U) {
    return true;
  }
  const int first_required_right = static_cast<int>(x) + 1;
  for (const auto& tenant : floor.tenants) {
    if (tenant.right >= first_required_right) {
      return original_vertical_landing_type(tenant.type);
    }
  }
  return false;
}

bool original_vertical_hits_elevator(const OriginalTdtDocument& document,
                                     int left, int top, int right,
                                     int bottom) {
  for (const auto& elevator : document.elevators) {
    if (elevator.used == 0U) {
      continue;
    }
    const int width = elevator.type == 0U ? 6 : 4;
    if (rectangles_overlap(
            left, top, right, bottom, elevator.x,
            static_cast<int>(elevator.bottom_floor) - 2,
            static_cast<int>(elevator.x) + width,
            static_cast<int>(elevator.top_floor) + 1)) {
      return true;
    }
  }
  return false;
}

bool original_normal_vertical_transport_collision(
    const OriginalTdtDocument& document, int lower, int x,
    std::size_t free_index) {
  // Exact 10c0:0983 normal Stair/Escalator collision scan, including the
  // split diagonal rectangles used when both records have zero height.
  if (original_vertical_hits_elevator(document, x, lower - 1, x + 8,
                                      lower + 1)) {
    return true;
  }

  for (std::size_t index = 0;
       index < document.post_elevator.stairs_bd70.size(); ++index) {
    const auto& stair = document.post_elevator.stairs_bd70[index];
    if (stair.used == 0U || index == free_index) {
      continue;
    }
    const int existing_height = static_cast<int>(stair.shape) / 2;
    if (existing_height != 0) {
      if (rectangles_overlap(x, lower - 1, x + 8, lower + 1,
                             stair.x, static_cast<int>(stair.floor) - 1,
                             static_cast<int>(stair.x) + 8,
                             static_cast<int>(stair.floor) +
                                 existing_height)) {
        return true;
      }
      continue;
    }

    const int existing_x = stair.x;
    const int existing_floor = stair.floor;
    const bool first_half = rectangles_overlap(
        x, lower - 1, x + 4, lower,
        existing_x, existing_floor - 1, existing_x + 4, existing_floor) ||
        rectangles_overlap(x, lower - 1, x + 4, lower,
                           existing_x + 4, existing_floor,
                           existing_x + 8, existing_floor + 1);
    const bool second_half = rectangles_overlap(
        x + 4, lower, x + 8, lower + 1,
        existing_x, existing_floor - 1, existing_x + 4, existing_floor) ||
        rectangles_overlap(x + 4, lower, x + 8, lower + 1,
                           existing_x + 4, existing_floor,
                           existing_x + 8, existing_floor + 1);
    if (first_half || second_half) {
      return true;
    }
  }
  return false;
}

bool original_tall_vertical_transport_collision(
    const OriginalTdtDocument& document, int lower, int x,
    std::uint8_t shape, std::size_t free_index) {
  // Exact 10c0:0d06 tall/lobby-spanning Stair collision scan. The candidate
  // bottom is lower+shape/2, while existing Stair rectangles deliberately
  // extend through existing.floor+existing.shape/2+1.
  const int height = static_cast<int>(shape) / 2;
  if (original_vertical_hits_elevator(document, x, lower - 1, x + 8,
                                      lower + height)) {
    return true;
  }
  for (std::size_t index = 0;
       index < document.post_elevator.stairs_bd70.size(); ++index) {
    const auto& stair = document.post_elevator.stairs_bd70[index];
    if (stair.used == 0U || index == free_index) {
      continue;
    }
    const int existing_height = static_cast<int>(stair.shape) / 2;
    if (rectangles_overlap(x, lower - 1, x + 8, lower + height,
                           stair.x, static_cast<int>(stair.floor) - 1,
                           static_cast<int>(stair.x) + 8,
                           static_cast<int>(stair.floor) +
                               existing_height + 1)) {
      return true;
    }
  }
  return false;
}

}  // namespace

int original_route_boundary(const std::array<std::byte, 0x78>& links,
                            int floor, bool upward) noexcept {
  // Exact 11b0:0763 six-floor boundary scan used by 11b0:06a4's bff0 route
  // summary rebuild, including the three-floor cutoff after crossing bit 0.
  bool crossed_non_stair_link = false;
  if (upward) {
    for (int candidate = floor; candidate < floor + 6; ++candidate) {
      const auto flags = std::to_integer<std::uint8_t>(links[candidate]);
      if (flags == 0U) {
        return candidate;
      }
      if ((flags & 1U) == 0U) {
        crossed_non_stair_link = true;
      }
      if (crossed_non_stair_link && floor + 3 <= candidate) {
        return candidate;
      }
    }
    return floor + 6;
  }

  for (int candidate = floor - 1; candidate >= floor - 6; --candidate) {
    const auto flags = std::to_integer<std::uint8_t>(links[candidate]);
    if (flags == 0U) {
      return candidate + 1;
    }
    if ((flags & 1U) == 0U) {
      crossed_non_stair_link = true;
    }
    if (crossed_non_stair_link && floor - 3 > candidate) {
      return candidate + 1;
    }
  }
  return floor - 6;
}

namespace {

void rebuild_original_vertical_route_summaries(OriginalTdtDocument& document) {
  // Literal 11b0:00a4/06a4. Only the leading four bytes of each 0x1e4-byte
  // route record are reset; the remainder is deliberately preserved.
  for (auto& route : document.post_elevator.routes_bff0) {
    route[0] = std::byte{0};
    route[1] = std::byte{0};
    route[2] = std::byte{0xff};
    route[3] = std::byte{0xff};
  }

  std::size_t route_index = 0;
  auto add_floor = [&](int floor) {
    const int lower = original_route_boundary(
        document.post_elevator.cf10, floor, false);
    const int upper = original_route_boundary(
        document.post_elevator.cf10, floor, true);
    if (lower < upper && route_index <
                             document.post_elevator.routes_bff0.size()) {
      auto& route = document.post_elevator.routes_bff0[route_index++];
      route[1] = std::byte{1};
      route[2] = static_cast<std::byte>(upper);
      route[3] = static_cast<std::byte>(lower);
    }
  };
  add_floor(10);
  for (int floor = 24; floor <= 109; floor += 15) {
    add_floor(floor);
  }
}

void initialize_original_elevator_car(OriginalTdtElevatorCarRecord& car,
                                      std::uint8_t floor,
                                      std::uint8_t floor_mode,
                                      std::optional<bool> active) {
  // 1090:0192 writes this exact 0x15a-byte car record. The allocation is
  // GMEM_ZEROINIT; offsets 16..183 are 42 -1 dwords, offsets 184..225 are
  // 42 -1 bytes, and offsets 226..345 are explicitly cleared. A negative
  // third argument deliberately leaves byte 15 untouched; 1090:00d9 uses
  // that form while rebuilding loaded and demolished shafts.
  const auto preserved_active = car.exact_bytes[15];
  car.exact_bytes.fill(std::byte{0});
  car.exact_bytes[0] = static_cast<std::byte>(floor);
  car.exact_bytes[4] = std::byte{1};
  car.exact_bytes[5] = static_cast<std::byte>(floor);
  car.exact_bytes[6] = static_cast<std::byte>(floor);
  car.exact_bytes[13] = static_cast<std::byte>(floor);
  car.exact_bytes[14] = static_cast<std::byte>(floor_mode);
  car.exact_bytes[15] = active
                            ? static_cast<std::byte>(*active ? 1 : 0)
                            : preserved_active;
  std::fill(car.exact_bytes.begin() + 16,
            car.exact_bytes.begin() + 226, std::byte{0xff});
}

void initialize_original_elevator(OriginalTdtElevator& elevator,
                                  std::uint8_t type,
                                  std::uint8_t capacity,
                                  std::uint16_t x,
                                  std::uint8_t floor) {
  elevator = {};
  elevator.used = 1;
  elevator.type = type;          // bp+0c at 11f8:12d6
  elevator.capacity = capacity;  // bp+0e at 11f8:12e7
  elevator.cars = 1;
  // 11f8:12fd-136e initializes two seven-entry schedule bands.
  std::fill(elevator.schedule.begin(), elevator.schedule.begin() + 14,
            std::byte{1});
  std::fill(elevator.schedule.begin() + 14, elevator.schedule.begin() + 28,
            std::byte{5});
  elevator.word_3c = 1;
  elevator.x = x;
  elevator.top_floor = static_cast<std::int8_t>(floor);
  elevator.bottom_floor = static_cast<std::int8_t>(floor);
  elevator.serviced_floors[floor] = std::byte{1};
  elevator.car_home_floors.fill(static_cast<std::byte>(floor));

  OriginalTdtElevatorFloorRecord floor_record{};
  floor_record.mapped_index = original_elevator_floor_record_index(
      type, static_cast<std::int8_t>(floor),
      static_cast<std::int8_t>(floor), floor);
  floor_record.floor = static_cast<std::int8_t>(floor);
  elevator.floor_records.push_back(floor_record);
  for (std::size_t index = 0; index < elevator.car_records.size(); ++index) {
    initialize_original_elevator_car(
        elevator.car_records[index], floor, 0U, index == 0U);
  }

  // The 0x24 writer transfers the complete 194-byte header. Keep the exact
  // copy synchronized for lossless inspection as well as the modeled fields
  // used by serialize_original_tdt.
  elevator.reconstructed_header.fill(std::byte{0});
  elevator.reconstructed_header[0] = std::byte{1};
  elevator.reconstructed_header[1] = static_cast<std::byte>(type);
  elevator.reconstructed_header[2] = static_cast<std::byte>(capacity);
  elevator.reconstructed_header[3] = std::byte{1};
  std::copy(elevator.schedule.begin(), elevator.schedule.end(),
            elevator.reconstructed_header.begin() + 4);
  elevator.exact_file_header.assign(elevator.reconstructed_header.begin(),
                                    elevator.reconstructed_header.end());
  elevator.file_header_size = elevator.reconstructed_header.size();
}

void update_floor_bounds(OriginalTdtFloor& floor) {
  if (floor.tenants.empty()) {
    floor.left_edge = 0;
    floor.right_edge = 0;
    return;
  }
  auto left = floor.tenants.front().left;
  auto right = floor.tenants.front().right;
  for (const auto& tenant : floor.tenants) {
    left = std::min(left, tenant.left);
    right = std::max(right, tenant.right);
  }
  floor.left_edge = left;
  floor.right_edge = right;
}

OriginalConstructionResult build_original_immediate_parking_unit(
    OriginalTdtDocument& document,
    std::uint8_t type,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs) {
  const std::uint16_t width = type == 0x2cU ? 16U : 4U;
  const auto right = static_cast<std::uint16_t>(left + width);
  if (type != 0x2cU && type != 11U) {
    return {OriginalConstructionStatus::invalid_floor, 0};
  }
  if (floor < 0) {
    return {OriginalConstructionStatus::invalid_floor, 0, 20U};
  }
  if (floor >= 10) {
    return {OriginalConstructionStatus::invalid_floor, 0,
            original_status_code(floor == 10, 12U, 11U)};
  }
  if (left > kOriginalWorldGridWidth - width) {
    return {OriginalConstructionStatus::invalid_span, 0, 20U};
  }

  if (type == 0x2cU) {
    const auto ramp_x = static_cast<std::int16_t>(
        load_original_header_word(document, 36U));  // DS:b3ee
    if (ramp_x < 0 && floor != 9) {
      return {OriginalConstructionStatus::invalid_floor, 0, 31U};
    }
    if (ramp_x >= 0 && ramp_x != static_cast<std::int16_t>(left)) {
      return {OriginalConstructionStatus::invalid_span, 0, 32U};
    }
  } else {
    const auto& target = document.floors[static_cast<std::size_t>(floor)];
    // Exact 1198:0beb floor-local tenant scan: Parking is legal when any
    // record has raw type 0x2c, independent of that Ramp's runtime status.
    const bool has_ramp = std::any_of(
        target.tenants.begin(), target.tenants.end(),
        [](const OriginalTdtTenant& candidate) {
          return candidate.type == 0x2c;
        });
    if (!has_ramp) {
      return {OriginalConstructionStatus::parking_ramp_required, 0, 34U};
    }
    // 11f8:240d compares the persisted runtime counter at DS:b3fc with
    // exactly 0x200 before each four-cell segment.
    if (load_original_header_word(document, 50U) == 0x0200U) {
      return {OriginalConstructionStatus::tenant_limit, 0, 30U};
    }
  }

  // Both immediate types take 11f8:2e64's basement support branch and
  // 11f8:2f5a's basement-only legality branch.
  const std::size_t b3e8_offset =
      document.header.format_version >= 0x20U ? 30U : 28U;
  if (document.header.exact_bytes.size() < b3e8_offset + 2U) {
    return {OriginalConstructionStatus::invalid_floor, 0, 14U};
  }
  const auto deepest_floor = static_cast<std::int16_t>(load_u16(
      std::span<const std::byte>(document.header.exact_bytes), b3e8_offset,
      document.header.byte_swapped));
  if (static_cast<int>(deepest_floor) - 1 > floor) {
    return {OriginalConstructionStatus::invalid_floor, 0, 14U};
  }

  const auto& old_floor = document.floors[static_cast<std::size_t>(floor)];
  const auto& supporting =
      document.floors[static_cast<std::size_t>(floor + 1)];
  const auto& lobby = document.floors[10];
  if (supporting.tenants.empty()) {
    return {OriginalConstructionStatus::invalid_span, 0, 4U};
  }
  if (lobby.tenants.empty() || lobby.left_edge > left ||
      lobby.right_edge < right) {
    return {OriginalConstructionStatus::invalid_span, 0, 2U};
  }
  if (old_floor.tenants.empty() &&
      (supporting.left_edge >= right || supporting.right_edge <= left)) {
    return {OriginalConstructionStatus::invalid_span, 0, 4U};
  }

  std::optional<std::uint8_t> key;
  if (type == 11U) {
    key = first_original_floor_key(old_floor);
    if (!key) {
      return {OriginalConstructionStatus::tenant_limit, 0, 9U};
    }
  }

  auto new_tenants = old_floor.tenants;
  auto new_left = old_floor.left_edge;
  auto new_right = old_floor.right_edge;
  auto unit = make_original_immediate_facility(
      left, right, type, type == 11U ? 1U : 0U,
      document.header.byte_swapped);
  const auto insertion = insert_original_office_record(
      new_tenants, new_left, new_right, std::move(unit),
      document.header.byte_swapped);
  if (insertion != OfficeInsertionResult::inserted) {
    return {insertion == OfficeInsertionResult::tenant_limit
                ? OriginalConstructionStatus::tenant_limit
                : OriginalConstructionStatus::occupied,
            0, 9U};
  }

  const std::uint32_t added_cells = old_floor.tenants.empty()
      ? width
      : static_cast<std::uint32_t>(old_floor.left_edge -
                                   std::min(old_floor.left_edge, left)) +
            static_cast<std::uint32_t>(
                std::max(old_floor.right_edge, right) - old_floor.right_edge);
  const std::uint64_t cost64 =
      static_cast<std::uint64_t>(construction_costs[type]) +
      static_cast<std::uint64_t>(added_cells) * construction_costs[0];
  const auto cost = static_cast<std::uint32_t>(cost64);
  if (cost64 > 0x7fffffffULL ||
      (cost != 0U && document.header.balance < static_cast<std::int32_t>(cost))) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(cost),
            original_funds_status_code(document.header.balance,
                                       construction_costs[type])};
  }

  auto& target = document.floors[static_cast<std::size_t>(floor)];
  target.tenants = std::move(new_tenants);
  target.left_edge = new_left;
  target.right_edge = new_right;

  auto inserted = std::find_if(
      target.tenants.begin(), target.tenants.end(),
      [&](const OriginalTdtTenant& candidate) {
        return candidate.type == static_cast<std::int8_t>(type) &&
               candidate.left == left && candidate.right == right;
      });
  if (inserted == target.tenants.end()) {
    return {OriginalConstructionStatus::occupied, 0, 9U};
  }

  if (type == 11U) {
    // 1228:0393 first assigns the floor lookup key and rebuilds +0xa92.
    inserted->exact_bytes[12] = static_cast<std::byte>(*key);
    inserted->preserved_07_to_0f[5] = static_cast<std::byte>(*key);
    rebuild_original_floor_lookup(target);

    // 1198:0252 reserves the first cf9c record, writes floor/key and a zero
    // dword, then returns its index. Failure returns 0xffff but does not undo
    // the already-created tenant.
    std::uint16_t parking_index = 0xffffU;
    for (std::size_t index = 0;
         index < document.post_elevator.cf9c_records.size(); ++index) {
      auto& record = document.post_elevator.cf9c_records[index];
      if (record[0] != std::byte{0xff}) {
        continue;
      }
      record.fill(std::byte{0});
      record[0] = static_cast<std::byte>(static_cast<std::uint8_t>(floor));
      record[1] = static_cast<std::byte>(*key);
      parking_index = static_cast<std::uint16_t>(index);
      break;
    }
    inserted->subtype = 0;
    inserted->exact_bytes[17] = std::byte{0};
    store_u16(inserted->exact_bytes, 12, parking_index,
              document.header.byte_swapped);
    inserted->preserved_07_to_0f[5] = inserted->exact_bytes[12];
    inserted->preserved_07_to_0f[6] = inserted->exact_bytes[13];

    const auto parking_count = load_original_header_word(document, 50U);
    store_original_header_word(
        document, 50U, static_cast<std::uint16_t>(parking_count + 1U));
  } else {
    rebuild_original_floor_lookup(target);
  }

  document.header.balance = wrapping_subtract(document.header.balance, cost);
  document.header.construction_costs =
      wrapping_subtract(document.header.construction_costs, cost);
  rebuild_original_parking_connectivity(document);
  return {OriginalConstructionStatus::ok, static_cast<std::int32_t>(cost)};
}

}  // namespace

void activate_all_original_pending_facilities_for_schedule(
    OriginalTdtDocument& document) {
  activate_all_original_pending_facilities(document);
}

void reset_original_loaded_simulation_state(
    OriginalTdtDocument& document) {
  // Exact 10b0:0000 non-visual call order. 11f0:0016 consumes the complete
  // pending queue before 10b0:0072 normalizes the linked tenant records.
  activate_all_original_pending_facilities_for_schedule(document);
  (void)initialize_original_tenant_runtime_state(document);

  // Exact 1198:0000 clears both eleven-dword b846 series. The original does
  // not rebuild the parking ceilings here; 1198:01ab does that at day start.
  for (auto& series : document.post_elevator.b846_series) {
    series.fill(0);
  }

  // Exact 11a8:14c9 seven-bank reset. Its word-by-word loops clear the full
  // runtime allocations, including each bank's leading count word.
  document.post_elevator.dynamic_dd5c.fill(std::byte{0});
  document.post_elevator.dynamic_dd60.fill(std::byte{0});
  document.post_elevator.dynamic_dd64.fill(std::byte{0});

  const int floor_mode_index =
      28 + static_cast<int>(
               original_calendar_phase(document.header.current_day)) * 7 +
      original_day_phase(document.header.frame_time);
  // Exact 24-call 1090:00d9(elevator, -1) loop: clear per-floor ownership and
  // waiting-ring headers, then recreate all eight inactive car records at
  // their stored home floors with the current schedule byte.
  for (auto& elevator : document.elevators) {
    elevator.block_2a2.fill(std::byte{0});
    elevator.block_31a.fill(std::byte{0});
    for (auto& floor_record : elevator.floor_records) {
      std::fill_n(floor_record.exact_bytes.begin(), 4U, std::byte{0});
    }
    for (std::size_t car_index = 0U;
         car_index < elevator.car_records.size(); ++car_index) {
      const auto home = std::to_integer<std::uint8_t>(
          elevator.car_home_floors[car_index]);
      const auto floor_mode = floor_mode_index >= 0 &&
                                      static_cast<std::size_t>(floor_mode_index) <
                                          elevator.schedule.size()
                                  ? std::to_integer<std::uint8_t>(
                                        elevator.schedule[static_cast<std::size_t>(
                                            floor_mode_index)])
                                  : 0U;
      initialize_original_elevator_car(
          elevator.car_records[car_index], home, floor_mode, std::nullopt);
    }
  }

  // Exact 64-call 10c0:0000 loop. Shape, position and used state survive.
  for (auto& transport : document.post_elevator.stairs_bd70) {
    transport.word_6 = 0U;
    transport.word_8 = 0U;
    store_u16(transport.exact_bytes, 6U, 0U,
              document.header.byte_swapped);
    store_u16(transport.exact_bytes, 8U, 0U,
              document.header.byte_swapped);
  }

  (void)initialize_original_people_runtime_state(document);
}

OriginalParkingDragRunPlan original_parking_drag_run_plan(
    OriginalParkingDragRunState state,
    std::int32_t initial_left,
    std::int32_t initial_right,
    std::int32_t current_left,
    std::int32_t current_right) {
  constexpr std::int32_t kParkingUnitWidth = 4;
  if (!state.initialized) {
    state = {
        true, initial_left, initial_left, initial_left, initial_right};
  }

  OriginalParkingDragRunPlan plan{};
  if (state.retained_right <= state.built_left) {
    std::int32_t left = state.built_left - kParkingUnitWidth;
    while (state.retained_left <= left) {
      plan.unit_lefts.push_back(left);
      left -= kParkingUnitWidth;
      state.built_left -= kParkingUnitWidth;
    }
  } else if (state.built_right <= state.retained_left) {
    std::int32_t left = state.built_right;
    while (left + kParkingUnitWidth <= state.retained_right) {
      plan.unit_lefts.push_back(left);
      left += kParkingUnitWidth;
      state.built_right += kParkingUnitWidth;
    }
  }

  state.retained_left = current_left;
  state.retained_right = current_right;
  plan.next_state = state;
  return plan;
}

OriginalParkingRampDragRunPlan original_parking_ramp_drag_run_plan(
    OriginalParkingRampDragRunState state,
    std::int16_t initial_floor,
    std::int16_t current_floor,
    std::int32_t construction_left) {
  if (!state.initialized) {
    state = {
        true,
        initial_floor,
        initial_floor,
        static_cast<std::int16_t>(initial_floor + 1),
        initial_floor,
    };
  }

  OriginalParkingRampDragRunPlan plan{};
  if (state.built_upper_exclusive <= state.retained_floor) {
    std::int16_t candidate = static_cast<std::int16_t>(
        state.built_upper_exclusive + 1);
    while (candidate <= state.retained_upper_exclusive) {
      plan.attempts.push_back({
          static_cast<std::int16_t>(candidate - 1), construction_left});
      candidate = static_cast<std::int16_t>(candidate + 1);
      state.built_upper_exclusive = static_cast<std::int16_t>(
          state.built_upper_exclusive + 1);
    }
  } else if (state.retained_upper_exclusive <= state.built_lower) {
    std::int16_t candidate = static_cast<std::int16_t>(
        state.built_lower - 1);
    while (state.retained_floor <= candidate) {
      plan.attempts.push_back({candidate, construction_left});
      candidate = static_cast<std::int16_t>(candidate - 1);
      state.built_lower = static_cast<std::int16_t>(state.built_lower - 1);
    }
  }

  state.retained_floor = current_floor;
  state.retained_upper_exclusive = static_cast<std::int16_t>(current_floor + 1);
  plan.next_state = state;
  return plan;
}

OriginalLobbyPlacement original_lobby_placement_from_client(
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept {
  // 11f8:3da4 subtracts half of RECT.right (32/2) before truncating to an
  // eight-pixel boundary. IDIV remainder subtraction is truncation toward
  // zero, which differs from mathematical floor for negative coordinates.
  int world_x = client_x + view_x - 16;
  world_x -= world_x % 8;
  int world_y = client_y + view_y;
  world_y -= world_y % 36;
  const int left = world_x / 8;
  return {
      static_cast<std::int16_t>(119 - world_y / 36),
      left,
      left + 4,
  };
}

OriginalLobbyPlacement original_floor_placement_from_client(
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept {
  // The type-zero width table at 11f8:0254 dispatches to 11f8:00db, which
  // stores one cell. 11f8:3dc4 subtracts half of that eight-pixel width.
  int world_x = client_x + view_x - 4;
  world_x -= world_x % 8;
  int world_y = client_y + view_y;
  world_y -= world_y % 36;
  // 11f8:3da4 adds twelve after the vertical snap for every selected type
  // except Lobby (0x18). The quotient is unchanged for ordinary nonnegative
  // points, but the distinction is observable for captured points above zero
  // because signed IDIV truncates toward zero.
  world_y += 12;
  const int left = world_x / 8;
  return {
      static_cast<std::int16_t>(119 - world_y / 36),
      left,
      left + 1,
  };
}

bool apply_original_initial_balance_bonus(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::int32_t left) noexcept {
  // 11f8:0955-097d performs these tests in this order before passing the
  // current balance as 1178:076f's capped other-income amount.
  if (document.header.lobby_height != 0U || floor != 0 || left != 0 ||
      document.header.balance != 20'000 ||
      !document.floors[0].tenants.empty()) {
    return false;
  }
  constexpr std::uint32_t kInitialBonus = 20'000U;
  document.header.balance = std::bit_cast<std::int32_t>(
      std::bit_cast<std::uint32_t>(document.header.balance) + kInitialBonus);
  document.header.other_income = std::bit_cast<std::int32_t>(
      std::bit_cast<std::uint32_t>(document.header.other_income) +
      kInitialBonus);
  return true;
}

OriginalLobbyPlacement original_office_placement_from_client(
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept {
  int world_x = client_x + view_x - 36;
  world_x -= world_x % 8;
  int world_y = client_y + view_y;
  world_y -= world_y % 36;
  world_y += 12;
  const int left = world_x / 8;
  return {
      static_cast<std::int16_t>(119 - world_y / 36),
      left,
      left + 9,
  };
}

std::uint16_t original_facility_width_cells(std::uint16_t type) noexcept {
  switch (type) {
    case 1:
    case 43:
      return 4;
    case 42:
      return 6;
    case 3:
      return 4;
    case 4:
      return 6;
    case 5:
      return 10;
    case 6:
      return 24;
    case 7:
      return 9;
    case 8:
      return 2;
    case 9:
      return 16;
    case 10:
      return 12;
    case 11:
      return 4;
    case 12:
      return 16;
    case 13:
      return 26;
    case 14:
      return 16;
    case 15:
      return 15;
    case 17:
      return 2;
    case 18:
    case 19:
      return 24;
    case 20:
    case 21:
      return 25;
    case 29:
    case 30:
      return 24;
    case 31:
    case 32:
    case 33:
      return 30;
    case 34:
    case 35:
      return 7;
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
      return 28;
    case 22:
    case 27:
      return 8;
    case 0x2c:
      // 11f8:0000 loads BITMAP/3816 for type 0x2c and divides its exact
      // 128-pixel DIB width by the eight-pixel world cell width.
      return 16;
    default:
      return 0;
  }
}

OriginalLobbyPlacement original_facility_placement_from_client(
    std::uint16_t type,
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept {
  const int width = original_facility_width_cells(type);
  if (width == 0) {
    return {};
  }
  constexpr int kCellWidth = 8;
  constexpr int kFloorHeight = 36;
  int world_x = client_x + view_x - (width * kCellWidth) / 2;
  world_x -= world_x % kCellWidth;
  int world_y = client_y + view_y;
  world_y -= world_y % kFloorHeight;
  world_y += 12;
  const int left = world_x / kCellWidth;
  return {
      static_cast<std::int16_t>(119 - world_y / kFloorHeight),
      left,
      left + width,
  };
}

OriginalVerticalTransportHit original_vertical_transport_hit_from_client(
    const OriginalTdtDocument& document,
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept {
  // 11f8:3d5d/3d2d add the shared view point before 10c0:0606 converts the
  // resulting world pixel to the 120-story/eight-pixel construction grid.
  const int world_x = client_x + view_x;
  const int world_y = client_y + view_y;
  const int floor = 120 - world_y / 36 - 1;
  const int x = world_x / 8;

  for (std::size_t index = 0;
       index < document.post_elevator.stairs_bd70.size(); ++index) {
    const auto& transport = document.post_elevator.stairs_bd70[index];
    if (transport.used == 0U) continue;

    const int left = transport.x;
    const int base = transport.floor;
    const int height = static_cast<std::int8_t>(transport.shape) / 2;
    if (x < left || x >= left + 8 || floor < base) continue;

    if (height != 0) {
      if (floor <= base + height) {
        return {true, index, static_cast<std::int16_t>(floor),
                static_cast<std::int16_t>(x)};
      }
      continue;
    }

    if (floor > base + 1) continue;
    const int local_y = world_y - (120 - base - 2) * 36;
    const int local_x = x - left;
    const int diagonal = (8 - local_x) * 6;
    if (local_y >= diagonal + 12 && local_y < diagonal + 36) {
      return {true, index, static_cast<std::int16_t>(floor),
              static_cast<std::int16_t>(x)};
    }
  }
  return {};
}

OriginalElevatorHit original_elevator_hit_from_client(
    const OriginalTdtDocument& document,
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept {
  const int world_x = client_x + view_x;
  const int world_y = client_y + view_y;
  const int floor = 120 - world_y / 36 - 1;
  const int x = world_x / 8;

  for (std::size_t elevator_index = 0;
       elevator_index < document.elevators.size(); ++elevator_index) {
    const auto& elevator = document.elevators[elevator_index];
    if (elevator.used == 0U) continue;
    const int width = elevator.type == 0U ? 6 : 4;
    if (x < elevator.x || x >= static_cast<int>(elevator.x) + width ||
        floor < static_cast<int>(elevator.bottom_floor) - 1 ||
        floor > static_cast<int>(elevator.top_floor) + 1) {
      continue;
    }

    OriginalElevatorHit hit{
        true, elevator_index, static_cast<std::int16_t>(floor), -1};
    const int shaft_left = static_cast<int>(elevator.x) * 8 - view_x;
    for (std::size_t car_index = 0;
         car_index < elevator.car_records.size(); ++car_index) {
      const auto& exact = elevator.car_records[car_index].exact_bytes;
      if (exact[15] == std::byte{0}) continue;
      const int car_floor = static_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(exact[0]));
      const int motion = static_cast<std::int8_t>(
          std::to_integer<std::uint8_t>(exact[1])) * 6;
      const bool upward = exact[4] != std::byte{0};
      const int car_top = (119 - car_floor) * 36 - view_y +
                          (upward ? motion : -motion) + 5;
      if (client_x >= shaft_left + 2 &&
          client_x < shaft_left + width * 8 - 2 &&
          client_y >= car_top && client_y < car_top + 31) {
        hit.car_index = static_cast<std::int16_t>(car_index);
      }
    }
    return hit;
  }
  return {};
}

OriginalElevatorServiceFloorGate original_elevator_service_floor_gate(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept {
  if (elevator_index >= document.elevators.size()) {
    return OriginalElevatorServiceFloorGate::invalid_elevator;
  }
  if (floor < 0 || floor >= 120) {
    return OriginalElevatorServiceFloorGate::invalid_floor;
  }
  const auto& elevator = document.elevators[elevator_index];
  if (elevator.used == 0U || elevator.word_3c == 0U) {
    return OriginalElevatorServiceFloorGate::inactive_shaft;
  }
  if (floor < elevator.bottom_floor || floor > elevator.top_floor) {
    return OriginalElevatorServiceFloorGate::outside_shaft;
  }

  // 10a0:102d scans all eight car records, not merely the header car count.
  // A nonzero +15 active byte protects that car's signed header home floor.
  for (std::size_t index = 0; index < elevator.car_records.size(); ++index) {
    if (elevator.car_records[index].exact_bytes[15] != std::byte{0} &&
        static_cast<std::int8_t>(std::to_integer<std::uint8_t>(
            elevator.car_home_floors[index])) == floor) {
      return OriginalElevatorServiceFloorGate::active_car_home;
    }
  }

  // Existing stops skip 1296 entirely and may be removed even if that floor
  // would be forbidden as a new stop. The first condition is 12e0's signed
  // (floor - 9) % 15 test for a type-zero shaft above floor ten; the second
  // is 133b's automatic multi-story Lobby interval [11, 10+height).
  if (elevator.serviced_floors[static_cast<std::size_t>(floor)] ==
      std::byte{0}) {
    const bool type_zero_non_stop =
        elevator.type == 0U && floor > 10 && (floor - 9) % 15 != 0;
    const bool lobby_story =
        floor >= 11 &&
        floor < static_cast<std::int16_t>(10U +
                                          document.header.lobby_height);
    if (type_zero_non_stop || lobby_story) {
      return OriginalElevatorServiceFloorGate::forbidden_new_stop;
    }
  }
  return OriginalElevatorServiceFloorGate::eligible;
}

bool original_elevator_floor_connected_for_shaft_removal(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept {
  if (elevator_index >= document.elevators.size() || floor < 0 ||
      floor >= 120) {
    return false;
  }
  const auto& elevator = document.elevators[elevator_index];
  if (elevator.used == 0U) return false;

  // Non-express-group shafts immediately pass when neither vertical-
  // transport direction bit reaches this floor. Type 2 skips this gate.
  if (elevator.type != 2U) {
    const auto at_floor = std::to_integer<std::uint8_t>(
        document.post_elevator.cf10[static_cast<std::size_t>(floor)]);
    const auto below =
        floor < 1
            ? 0U
            : std::to_integer<std::uint8_t>(
                  document.post_elevator.cf10[
                      static_cast<std::size_t>(floor - 1)]);
    if (at_floor == 0U && (floor < 1 || below == 0U)) return true;
  }
  if (elevator.serviced_floors[static_cast<std::size_t>(floor)] ==
      std::byte{0}) {
    return true;
  }

  // 11b0:0cfe treats type 2 as one class and all other types as the other;
  // another used shaft in the same class serving this floor is sufficient.
  const bool source_non_type_two = elevator.type != 2U;
  for (std::size_t index = 0; index < document.elevators.size(); ++index) {
    if (index == elevator_index) continue;
    const auto& candidate = document.elevators[index];
    if (candidate.used == 0U ||
        (candidate.type != 2U) != source_non_type_two) {
      continue;
    }
    if (candidate.serviced_floors[static_cast<std::size_t>(floor)] !=
        std::byte{0}) {
      return true;
    }
  }
  return false;
}

bool original_elevator_shaft_demolition_requires_confirmation(
    const OriginalTdtDocument& document,
    std::size_t elevator_index) noexcept {
  if (elevator_index >= document.elevators.size() ||
      document.elevators[elevator_index].used == 0U) {
    return false;
  }
  const auto& elevator = document.elevators[elevator_index];
  const int bottom = elevator.bottom_floor;
  const int top = elevator.top_floor;
  if (bottom < 0 || top >= 120 || bottom > top) return false;
  for (int floor = bottom; floor <= top; ++floor) {
    if (elevator.serviced_floors[static_cast<std::size_t>(floor)] ==
        std::byte{0}) {
      continue;
    }
    if (!original_elevator_floor_connected_for_shaft_removal(
            document, elevator_index, static_cast<std::int16_t>(floor))) {
      return true;
    }
  }
  return false;
}

void rebuild_original_transport_route_graphs(
    OriginalTdtDocument& document) noexcept {
  // 11b0:00da clears the disposable 120-byte Win16 transfer/route scratch at
  // the start of each rebuild. This direct graph pass uses local/value state,
  // so no persistent scratch bank remains to clear.
  auto& elevators = document.elevators;
  auto& tail = document.post_elevator;
  const bool byte_swapped = document.header.byte_swapped;
  constexpr std::size_t kTransferCount = 16U;
  constexpr std::size_t kElevatorCount = 24U;
  constexpr std::size_t kRouteCount = 8U;
  constexpr std::size_t kFloorCount = 120U;

  const auto bit = [](std::size_t index) noexcept {
    return static_cast<std::uint32_t>(0x80000000U >> index);
  };
  const auto has_bit = [&](std::uint32_t value, std::size_t index) noexcept {
    return (value & bit(index)) != 0U;
  };
  const auto route_contains_floor = [](const auto& route,
                                       std::int16_t floor) noexcept {
    if (route[1] == std::byte{0}) return false;
    const auto upper = std::bit_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(route[2]));
    const auto lower = std::bit_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(route[3]));
    return lower <= floor && upper >= floor;
  };
  const auto transfer_mask = [&](std::size_t index) noexcept {
    return load_u32(tail.db9c_records[index], 0U, byte_swapped);
  };
  const auto store_transfer_mask = [&](std::size_t index,
                                       std::uint32_t value) noexcept {
    store_u32(tail.db9c_records[index], 0U, value, byte_swapped);
  };
  const auto transfer_floor = [&](std::size_t index) noexcept {
    return static_cast<std::int16_t>(std::bit_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(tail.db9c_records[index][4])));
  };
  const auto store_graph = [&](auto& graph, std::size_t offset,
                               std::uint32_t value) noexcept {
    store_u32(graph, offset, value, byte_swapped);
  };

  // 11b0:0000 clears only the 120 graph dwords in each elevator and route;
  // exact 11b0:006d clears db9c's dword and floor byte but preserves byte five.
  for (auto& elevator : elevators) elevator.block_c2.fill(std::byte{0});
  for (auto& route : tail.routes_bff0) {
    std::fill(route.begin() + 4U, route.end(), std::byte{0});
  }
  for (auto& transfer : tail.db9c_records) {
    store_u32(transfer, 0U, 0U, byte_swapped);
    transfer[4] = std::byte{0xff};
  }

  // 11b0:049f creates a Lobby transfer record for every type-24 span touched
  // by at least one serviced elevator. Internal floors 11 and 12 are skipped
  // literally. Adjacent same-floor records merge only when their masks
  // overlap; the just-written slot is intentionally not cleared on a merge.
  std::size_t transfer_count = 0U;
  bool transfer_table_full = false;
  for (std::size_t floor = 0U;
       floor < document.floors.size() && !transfer_table_full; ++floor) {
    if (floor == 11U || floor == 12U) continue;
    for (const auto& tenant : document.floors[floor].tenants) {
      if (tenant.type != 0x18) continue;
      std::uint32_t mask = 0U;
      std::size_t connections = 0U;
      for (std::size_t elevator_index = 0U;
           elevator_index < elevators.size(); ++elevator_index) {
        const auto& elevator = elevators[elevator_index];
        if (elevator.used == 0U ||
            elevator.serviced_floors[floor] == std::byte{0}) {
          continue;
        }
        const auto car_offset = elevator.type == 0U ? 6U : 4U;
        const auto contact = static_cast<std::uint16_t>(
            elevator.x + car_offset);
        if (tenant.left > contact || tenant.right < elevator.x) continue;
        mask |= bit(elevator_index);
        ++connections;
      }
      if (connections == 0U) continue;
      if (transfer_count == kTransferCount) {
        transfer_table_full = true;
        break;
      }

      store_transfer_mask(transfer_count, mask);
      tail.db9c_records[transfer_count][4] =
          static_cast<std::byte>(floor);
      if (transfer_count > 0U &&
          transfer_floor(transfer_count - 1U) ==
              static_cast<std::int16_t>(floor) &&
          (transfer_mask(transfer_count - 1U) & mask) != 0U) {
        --transfer_count;
        store_transfer_mask(
            transfer_count, transfer_mask(transfer_count) | mask);
      }
      ++transfer_count;
    }
  }

  // A full sixteen-record table jumps out of 049f before this route-bit
  // augmentation, but the caller still invokes 00f2 afterward.
  if (!transfer_table_full) {
    for (std::size_t route_index = 0U;
         route_index < tail.routes_bff0.size(); ++route_index) {
      const auto& route = tail.routes_bff0[route_index];
      if (route[1] == std::byte{0}) continue;
      for (std::size_t transfer_index = 0U;
           transfer_index < tail.db9c_records.size(); ++transfer_index) {
        if (!route_contains_floor(route, transfer_floor(transfer_index))) {
          continue;
        }
        store_transfer_mask(
            transfer_index,
            transfer_mask(transfer_index) |
                bit(kElevatorCount + route_index));
      }
    }
  }

  const auto aggregate_transfer_mask = [&](std::size_t source_bit) noexcept {
    std::uint32_t mask = 0U;
    for (std::size_t index = 0U; index < kTransferCount; ++index) {
      const auto candidate = transfer_mask(index);
      if (has_bit(candidate, source_bit)) mask |= candidate;
    }
    return mask & ~bit(source_bit);
  };
  const auto direct_transfer = [&](std::size_t source_bit,
                                   std::int16_t floor) noexcept {
    std::uint32_t value = 0U;
    for (std::size_t index = 0U; index < kTransferCount; ++index) {
      if (has_bit(transfer_mask(index), source_bit) &&
          transfer_floor(index) == floor) {
        value = static_cast<std::uint32_t>(index + 1U);
      }
    }
    return value;
  };

  // Elevator half of 11b0:00f2. At a serviced floor the graph contains only
  // the last matching one-based db9c record. At every other floor it contains
  // an MSB-first mask of reachable non-type-2 elevators/vertical routes.
  for (std::size_t elevator_index = 0U;
       elevator_index < elevators.size(); ++elevator_index) {
    auto& source = elevators[elevator_index];
    if (source.used == 0U) continue;
    const auto connected = aggregate_transfer_mask(elevator_index);
    for (std::size_t floor = 0U; floor < kFloorCount; ++floor) {
      const auto offset = floor * 4U;
      store_graph(source.block_c2, offset, 0U);
      if (source.serviced_floors[floor] != std::byte{0}) {
        store_graph(source.block_c2, offset,
                    direct_transfer(elevator_index,
                                    static_cast<std::int16_t>(floor)));
        continue;
      }
      if (source.type == 2U) continue;

      std::uint32_t graph = 0U;
      for (std::size_t candidate_index = 0U;
           candidate_index < elevators.size(); ++candidate_index) {
        const auto& candidate = elevators[candidate_index];
        if (has_bit(connected, candidate_index) && candidate.type != 2U &&
            candidate.serviced_floors[floor] != std::byte{0}) {
          graph |= bit(candidate_index);
        }
      }
      for (std::size_t route_index = 0U; route_index < kRouteCount;
           ++route_index) {
        if (has_bit(connected, kElevatorCount + route_index) &&
            route_contains_floor(tail.routes_bff0[route_index],
                                 static_cast<std::int16_t>(floor))) {
          graph |= bit(kElevatorCount + route_index);
        }
      }
      store_graph(source.block_c2, offset, graph);
    }
  }

  // Vertical-route half of 00f2 mirrors the elevator graph, excluding all
  // type-2 elevator links but permitting transfers to other active routes.
  for (std::size_t route_index = 0U; route_index < kRouteCount;
       ++route_index) {
    auto& source = tail.routes_bff0[route_index];
    if (source[1] == std::byte{0}) continue;
    const auto source_bit = kElevatorCount + route_index;
    const auto connected = aggregate_transfer_mask(source_bit);
    for (std::size_t floor = 0U; floor < kFloorCount; ++floor) {
      const auto offset = 4U + floor * 4U;
      store_graph(source, offset, 0U);
      if (route_contains_floor(source, static_cast<std::int16_t>(floor))) {
        store_graph(source, offset,
                    direct_transfer(source_bit,
                                    static_cast<std::int16_t>(floor)));
        continue;
      }

      std::uint32_t graph = 0U;
      for (std::size_t elevator_index = 0U;
           elevator_index < elevators.size(); ++elevator_index) {
        const auto& elevator = elevators[elevator_index];
        if (has_bit(connected, elevator_index) && elevator.type != 2U &&
            elevator.serviced_floors[floor] != std::byte{0}) {
          graph |= bit(elevator_index);
        }
      }
      for (std::size_t candidate_index = 0U;
           candidate_index < kRouteCount; ++candidate_index) {
        if (has_bit(connected, kElevatorCount + candidate_index) &&
            route_contains_floor(tail.routes_bff0[candidate_index],
                                 static_cast<std::int16_t>(floor))) {
          graph |= bit(kElevatorCount + candidate_index);
        }
      }
      store_graph(source, offset, graph);
    }
  }
}

std::uint16_t original_elevator_service_floor_warning_code(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept {
  if (elevator_index >= document.elevators.size() || floor < 0 ||
      floor >= 120) {
    return 0U;
  }
  const auto& source = document.elevators[elevator_index];
  if (source.used == 0U) return 0U;

  // 11b0:0bab-c5: ordinary elevators need no warning when either vertical-
  // transport direction reaches the selected floor (or the floor below).
  if (source.type != 2U) {
    if (document.post_elevator.cf10[static_cast<std::size_t>(floor)] !=
        std::byte{0}) {
      return 0U;
    }
    if (floor >= 1 &&
        document.post_elevator.cf10[static_cast<std::size_t>(floor - 1)] !=
            std::byte{0}) {
      return 0U;
    }
  }
  if (source.serviced_floors[static_cast<std::size_t>(floor)] ==
      std::byte{0}) {
    return 0U;
  }

  constexpr auto bit = [](std::size_t index) noexcept {
    return static_cast<std::uint32_t>(0x80000000U >> index);
  };
  const auto graph = load_u32(
      source.block_c2, static_cast<std::size_t>(floor) * 4U,
      document.header.byte_swapped);
  if (graph != 0U) {
    // At a serviced floor 00f2 stores a one-based db9c record, then 0b8b
    // removes the source bit and searches for another used exact same type.
    const auto direct = static_cast<std::uint16_t>(graph);
    if (direct >= 1U &&
        direct <= document.post_elevator.db9c_records.size()) {
      auto mask = load_u32(
          document.post_elevator.db9c_records[direct - 1U], 0U,
          document.header.byte_swapped);
      mask &= ~bit(elevator_index);
      for (std::size_t index = 0U; index < document.elevators.size(); ++index) {
        const auto& candidate = document.elevators[index];
        if (index != elevator_index && (mask & bit(index)) != 0U &&
            candidate.used != 0U && candidate.type == source.type) {
          return 3U;
        }
      }
    }
  }

  // A same-type used shaft that also serves this floor suppresses the loss
  // warning even when it was not represented by the direct Lobby transfer.
  for (std::size_t index = 0U; index < document.elevators.size(); ++index) {
    if (index == elevator_index) continue;
    const auto& candidate = document.elevators[index];
    if (candidate.used != 0U && candidate.type == source.type &&
        candidate.serviced_floors[static_cast<std::size_t>(floor)] !=
            std::byte{0}) {
      return 0U;
    }
  }
  return source.type == 2U ? 2U : 1U;
}

bool add_original_elevator_service_floor(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept {
  if (original_elevator_service_floor_gate(
          document, elevator_index, floor) !=
      OriginalElevatorServiceFloorGate::eligible) {
    return false;
  }
  auto& elevator = document.elevators[elevator_index];
  auto& serviced = elevator.serviced_floors[static_cast<std::size_t>(floor)];
  if (serviced != std::byte{0}) return false;

  // `neg; sbb; inc` at 10a0:011d-0126 normalizes zero to the byte value one.
  serviced = std::byte{1};
  rebuild_original_transport_route_graphs(document);
  return true;
}

OriginalElevatorShaftExtensionResult extend_original_elevator_shaft(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t target_floor,
    const OriginalYenTable& construction_costs) {
  if (elevator_index >= document.elevators.size()) {
    return {OriginalConstructionStatus::invalid_floor, 0, target_floor,
            false};
  }
  const auto& source = document.elevators[elevator_index];
  if (source.used == 0U || source.word_3c == 0U ||
      source.bottom_floor > source.top_floor) {
    return {OriginalConstructionStatus::invalid_span, 0, target_floor,
            false};
  }
  if (target_floor == source.top_floor ||
      target_floor == source.bottom_floor) {
    return {OriginalConstructionStatus::ok, 0, target_floor, false};
  }

  const bool upper = target_floor > source.top_floor;
  const bool lower = target_floor < source.bottom_floor;
  if (!upper && !lower) {
    // This is the shrink half of 0819/0b87. It is intentionally kept out of
    // the extension transaction until 1625/14fa can dispatch every person
    // family without losing passengers or waiting-ring entries.
    return {OriginalConstructionStatus::invalid_span, 0, target_floor,
            false};
  }
  if ((upper && target_floor > 109) ||
      (lower && target_floor <= 0)) {
    return {OriginalConstructionStatus::invalid_floor, 0, target_floor,
            false};
  }
  if (lower) {
    const auto metro_top = static_cast<std::int16_t>(
        load_original_header_word(document, 30U));  // DS:b3e8
    if (target_floor < static_cast<int>(metro_top) - 1) {
      return {OriginalConstructionStatus::invalid_floor, 0, target_floor,
              false};
    }
  }

  const auto requested_bottom = lower ? target_floor : source.bottom_floor;
  const auto requested_top = upper ? target_floor : source.top_floor;
  if (original_elevator_extension_collision(
          document, elevator_index, requested_bottom, requested_top)) {
    return {OriginalConstructionStatus::occupied, 0, target_floor, false};
  }

  const auto width = original_elevator_width(source);
  const auto right = static_cast<std::uint16_t>(source.x + width);
  const auto preview_first = upper
      ? static_cast<std::int16_t>(source.top_floor + 1)
      : target_floor;
  const auto preview_last = upper
      ? target_floor
      : static_cast<std::int16_t>(source.bottom_floor - 1);
  const auto preview64 = original_floor_extension_cost(
      document, preview_first, preview_last, source.x, right,
      construction_costs);
  const auto preview = static_cast<std::uint32_t>(preview64);
  if (preview64 > 0x7fffffffULL ||
      (preview != 0U &&
       document.header.balance < static_cast<std::int32_t>(preview))) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(preview), target_floor, false};
  }

  std::int16_t actual_target = target_floor;
  bool clamped = false;
  if (source.type != 0U) {
    if (upper && actual_target - source.bottom_floor > 29) {
      actual_target = static_cast<std::int16_t>(source.bottom_floor + 29);
      clamped = true;
    } else if (lower && source.top_floor - actual_target > 29) {
      actual_target = static_cast<std::int16_t>(source.top_floor - 29);
      clamped = true;
    }
  }

  const auto actual_first = upper
      ? static_cast<std::int16_t>(source.top_floor + 1)
      : actual_target;
  const auto actual_last = upper
      ? actual_target
      : static_cast<std::int16_t>(source.bottom_floor - 1);
  const auto actual64 = actual_first <= actual_last
      ? original_floor_extension_cost(
            document, actual_first, actual_last, source.x, right,
            construction_costs)
      : 0U;
  if (actual64 > 0x7fffffffULL) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(actual64), actual_target, clamped};
  }
  const auto actual_cost = static_cast<std::uint32_t>(actual64);

  auto working = document;
  auto& elevator = working.elevators[elevator_index];
  if (upper) {
    for (int floor = source.top_floor + 1; floor <= actual_target; ++floor) {
      elevator.serviced_floors[static_cast<std::size_t>(floor)] =
          static_cast<std::byte>(
              original_elevator_floor_service_allowed(
                  working, elevator, static_cast<std::int16_t>(floor))
                  ? 1
                  : 0);
    }
  } else {
    for (int floor = actual_target; floor < source.bottom_floor; ++floor) {
      elevator.serviced_floors[static_cast<std::size_t>(floor)] =
          static_cast<std::byte>(
              original_elevator_floor_service_allowed(
                  working, elevator, static_cast<std::int16_t>(floor))
                  ? 1
                  : 0);
    }
  }

  const auto new_bottom = static_cast<std::int8_t>(
      lower ? actual_target : source.bottom_floor);
  const auto new_top = static_cast<std::int8_t>(
      upper ? actual_target : source.top_floor);
  rebuild_original_elevator_floor_records(elevator, new_bottom, new_top);

  // 0819/0b87 rebuild both graphs before committing the charge and endpoint.
  rebuild_original_transport_route_graphs(working);
  working.header.balance =
      wrapping_subtract(working.header.balance, actual_cost);
  working.header.construction_costs =
      wrapping_subtract(working.header.construction_costs, actual_cost);
  elevator.bottom_floor = new_bottom;
  elevator.top_floor = new_top;

  if (!ensure_original_elevator_floor_coverage(working, elevator)) {
    return {OriginalConstructionStatus::tenant_limit, 0, actual_target,
            clamped};
  }
  document = std::move(working);
  return {OriginalConstructionStatus::ok,
          static_cast<std::int32_t>(actual_cost), actual_target, clamped};
}

OriginalElevatorShaftExtensionResult
shrink_original_elevator_shaft_without_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    OriginalElevatorShaftEnd shaft_end,
    std::int16_t target_floor) {
  if (elevator_index >= document.elevators.size()) {
    return {OriginalConstructionStatus::invalid_floor, 0, target_floor,
            false};
  }
  const auto& source = document.elevators[elevator_index];
  if (source.used == 0U || source.word_3c == 0U ||
      source.bottom_floor > source.top_floor) {
    return {OriginalConstructionStatus::invalid_span, 0, target_floor,
            false};
  }
  const bool shrink_upper = shaft_end == OriginalElevatorShaftEnd::upper;
  if ((shrink_upper && target_floor == source.top_floor) ||
      (!shrink_upper && target_floor == source.bottom_floor)) {
    return {OriginalConstructionStatus::ok, 0, target_floor, false};
  }

  const bool valid_target = shrink_upper
      ? target_floor >= source.bottom_floor && target_floor < source.top_floor
      : target_floor > source.bottom_floor && target_floor <= source.top_floor;
  if (!valid_target) {
    return {OriginalConstructionStatus::invalid_span, 0, target_floor,
            false};
  }
  if (target_floor <= 0 || target_floor > 109) {
    return {OriginalConstructionStatus::invalid_floor, 0, target_floor,
            false};
  }
  if (!shrink_upper) {
    const auto metro_top = static_cast<std::int16_t>(
        load_original_header_word(document, 30U));
    if (target_floor < static_cast<int>(metro_top) - 1) {
      return {OriginalConstructionStatus::invalid_floor, 0, target_floor,
              false};
    }
  }

  const auto retained_bottom = static_cast<std::int16_t>(
      shrink_upper ? source.bottom_floor : target_floor);
  const auto retained_top = static_cast<std::int16_t>(
      shrink_upper ? target_floor : source.top_floor);
  if (original_elevator_extension_collision(
          document, elevator_index, retained_bottom, retained_top)) {
    return {OriginalConstructionStatus::occupied, 0, target_floor, false};
  }

  const auto removed_first = static_cast<std::int16_t>(
      shrink_upper ? target_floor + 1 : source.bottom_floor);
  const auto removed_last = static_cast<std::int16_t>(
      shrink_upper ? source.top_floor : target_floor - 1);
  for (int floor = removed_first; floor <= removed_last; ++floor) {
    if (original_elevator_service_floor_has_people(
            document, elevator_index, static_cast<std::int16_t>(floor))) {
      return {OriginalConstructionStatus::person_cleanup_required, 0,
              target_floor, false};
    }
  }

  auto working = document;
  auto& elevator = working.elevators[elevator_index];
  for (int floor = removed_first; floor <= removed_last; ++floor) {
    elevator.serviced_floors[static_cast<std::size_t>(floor)] = std::byte{0};
  }

  for (std::size_t car_index = 0U;
       car_index < elevator.car_records.size(); ++car_index) {
    auto& car = elevator.car_records[car_index].exact_bytes;
    if (car[15] == std::byte{0}) continue;
    const auto home = static_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(elevator.car_home_floors[car_index]));
    if ((shrink_upper && home > target_floor) ||
        (!shrink_upper && home < target_floor)) {
      elevator.car_home_floors[car_index] =
          static_cast<std::byte>(target_floor);
    }
    const auto current = static_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(car[0]));
    if ((shrink_upper && current > target_floor) ||
        (!shrink_upper && current < target_floor)) {
      car[0] = static_cast<std::byte>(target_floor);
      car[1] = std::byte{0};
      car[2] = std::byte{0};
      car[6] = static_cast<std::byte>(target_floor);
    }
  }

  // Both original shrink paths rebuild routing while the old endpoint is
  // still installed, then compact records and publish the new endpoint.
  rebuild_original_transport_route_graphs(working);
  const auto new_bottom = static_cast<std::int8_t>(retained_bottom);
  const auto new_top = static_cast<std::int8_t>(retained_top);
  rebuild_original_elevator_floor_records(elevator, new_bottom, new_top);
  elevator.bottom_floor = new_bottom;
  elevator.top_floor = new_top;

  // With the preflight above, 154a executes only its structural owner release
  // and 1090:0bcf recomputation; 1625 has no ring entries to dispatch.
  const auto cleanup_floor = [&](std::int16_t floor) {
    return cleanup_original_elevator_service_floor_people(
               working, elevator_index, floor, 0U, nullptr)
               .status == OriginalElevatorFloorPeopleCleanupStatus::cleaned;
  };
  if (shrink_upper) {
    for (int floor = removed_last; floor >= removed_first; --floor) {
      if (!cleanup_floor(static_cast<std::int16_t>(floor))) {
        return {OriginalConstructionStatus::person_cleanup_required, 0,
                target_floor, false};
      }
    }
  } else {
    for (int floor = removed_first; floor <= removed_last; ++floor) {
      if (!cleanup_floor(static_cast<std::int16_t>(floor))) {
        return {OriginalConstructionStatus::person_cleanup_required, 0,
                target_floor, false};
      }
    }
  }

  document = std::move(working);
  return {OriginalConstructionStatus::ok, 0, target_floor, false};
}

OriginalElevatorShaftShrinkResult shrink_original_elevator_shaft(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    OriginalElevatorShaftEnd shaft_end,
    std::int16_t target_floor,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) {
  OriginalElevatorShaftShrinkResult result{};
  const auto finish = [&](OriginalConstructionStatus status) {
    if (status != OriginalConstructionStatus::ok) {
      result.family_dispatches.clear();
      result.waiting_passengers = 0U;
      result.car_passengers = 0U;
    }
    result.shaft = {status, 0, target_floor, false};
    return result;
  };
  if (elevator_index >= document.elevators.size()) {
    return finish(OriginalConstructionStatus::invalid_floor);
  }
  const auto& source = document.elevators[elevator_index];
  if (source.used == 0U || source.word_3c == 0U ||
      source.bottom_floor > source.top_floor) {
    return finish(OriginalConstructionStatus::invalid_span);
  }
  const bool shrink_upper = shaft_end == OriginalElevatorShaftEnd::upper;
  if ((shrink_upper && target_floor == source.top_floor) ||
      (!shrink_upper && target_floor == source.bottom_floor)) {
    return finish(OriginalConstructionStatus::ok);
  }
  const bool valid_target = shrink_upper
      ? target_floor >= source.bottom_floor && target_floor < source.top_floor
      : target_floor > source.bottom_floor && target_floor <= source.top_floor;
  if (!valid_target) {
    return finish(OriginalConstructionStatus::invalid_span);
  }
  if (target_floor <= 0 || target_floor > 109) {
    return finish(OriginalConstructionStatus::invalid_floor);
  }
  if (!shrink_upper) {
    const auto metro_top = static_cast<std::int16_t>(
        load_original_header_word(document, 30U));
    if (target_floor < static_cast<int>(metro_top) - 1) {
      return finish(OriginalConstructionStatus::invalid_floor);
    }
  }

  const auto retained_bottom = static_cast<std::int16_t>(
      shrink_upper ? source.bottom_floor : target_floor);
  const auto retained_top = static_cast<std::int16_t>(
      shrink_upper ? target_floor : source.top_floor);
  if (original_elevator_extension_collision(
          document, elevator_index, retained_bottom, retained_top)) {
    return finish(OriginalConstructionStatus::occupied);
  }
  const auto removed_first = static_cast<std::int16_t>(
      shrink_upper ? target_floor + 1 : source.bottom_floor);
  const auto removed_last = static_cast<std::int16_t>(
      shrink_upper ? source.top_floor : target_floor - 1);

  auto working = document;
  auto& elevator = working.elevators[elevator_index];
  for (int floor = removed_first; floor <= removed_last; ++floor) {
    elevator.serviced_floors[static_cast<std::size_t>(floor)] = std::byte{0};
  }
  for (std::size_t car_index = 0U;
       car_index < elevator.car_records.size(); ++car_index) {
    auto& car = elevator.car_records[car_index].exact_bytes;
    if (car[15] == std::byte{0}) continue;
    const auto home = static_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(elevator.car_home_floors[car_index]));
    if ((shrink_upper && home > target_floor) ||
        (!shrink_upper && home < target_floor)) {
      elevator.car_home_floors[car_index] =
          static_cast<std::byte>(target_floor);
    }
    const auto current = static_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(car[0]));
    if ((shrink_upper && current > target_floor) ||
        (!shrink_upper && current < target_floor)) {
      car[0] = static_cast<std::byte>(target_floor);
      car[1] = std::byte{0};
      car[2] = std::byte{0};
      car[6] = static_cast<std::byte>(target_floor);
    }
  }

  // Both cap paths clear service and clamp cars before rebuilding 049f/00f2.
  // Their shrink branches then call 1625 for the entire removed range while
  // the old endpoint/floor-record mapping is still present.
  rebuild_original_transport_route_graphs(working);
  const auto append_dispatches = [&](auto& cleanup) {
    for (auto& dispatch : cleanup.family_dispatches) {
      result.family_dispatches.push_back(std::move(dispatch));
    }
  };
  const auto waiting_delay = part.words_00_to_40[2U];  // DS:dd7e
  if (shrink_upper) {
    for (int floor = removed_last; floor >= removed_first; --floor) {
      auto cleanup = cleanup_original_elevator_waiting_floor_people(
          working, elevator_index, static_cast<std::int16_t>(floor),
          waiting_delay, part, rent_income);
      if (cleanup.cleanup.status !=
          OriginalElevatorFloorPeopleCleanupStatus::cleaned) {
        return finish(OriginalConstructionStatus::person_cleanup_required);
      }
      result.waiting_passengers += cleanup.cleanup.waiting_passengers;
      append_dispatches(cleanup);
    }
  } else {
    for (int floor = removed_first; floor <= removed_last; ++floor) {
      auto cleanup = cleanup_original_elevator_waiting_floor_people(
          working, elevator_index, static_cast<std::int16_t>(floor),
          waiting_delay, part, rent_income);
      if (cleanup.cleanup.status !=
          OriginalElevatorFloorPeopleCleanupStatus::cleaned) {
        return finish(OriginalConstructionStatus::person_cleanup_required);
      }
      result.waiting_passengers += cleanup.cleanup.waiting_passengers;
      append_dispatches(cleanup);
    }
  }

  // Upper shrinking discards the suffix; lower shrinking performs the
  // executable's 324-byte record move. Rebuilding by floor preserves the
  // same retained records and remaps their indices before publishing the new
  // endpoint used by the following 14fa pass.
  const auto new_bottom = static_cast<std::int8_t>(retained_bottom);
  const auto new_top = static_cast<std::int8_t>(retained_top);
  rebuild_original_elevator_floor_records(elevator, new_bottom, new_top);
  elevator.bottom_floor = new_bottom;
  elevator.top_floor = new_top;

  if (shrink_upper) {
    for (int floor = removed_last; floor >= removed_first; --floor) {
      auto cleanup = cleanup_original_elevator_car_floor_people(
          working, elevator_index, static_cast<std::int16_t>(floor), part,
          rent_income);
      if (cleanup.cleanup.status !=
          OriginalElevatorFloorPeopleCleanupStatus::cleaned) {
        return finish(OriginalConstructionStatus::person_cleanup_required);
      }
      result.car_passengers += cleanup.cleanup.car_passengers;
      append_dispatches(cleanup);
    }
  } else {
    for (int floor = removed_first; floor <= removed_last; ++floor) {
      auto cleanup = cleanup_original_elevator_car_floor_people(
          working, elevator_index, static_cast<std::int16_t>(floor), part,
          rent_income);
      if (cleanup.cleanup.status !=
          OriginalElevatorFloorPeopleCleanupStatus::cleaned) {
        return finish(OriginalConstructionStatus::person_cleanup_required);
      }
      result.car_passengers += cleanup.cleanup.car_passengers;
      append_dispatches(cleanup);
    }
  }

  if (!ensure_original_elevator_floor_coverage(working, elevator)) {
    return finish(OriginalConstructionStatus::tenant_limit);
  }
  document = std::move(working);
  result.shaft = {OriginalConstructionStatus::ok, 0, target_floor, false};
  return result;
}

bool original_elevator_service_floor_has_people(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept {
  if (elevator_index >= document.elevators.size() || floor < 0 ||
      floor >= 120) {
    return false;
  }
  const auto& elevator = document.elevators[elevator_index];
  if (elevator.used == 0U) return false;
  const auto floor_index = static_cast<std::size_t>(floor);
  for (const auto& car : elevator.car_records) {
    if (car.exact_bytes[15] != std::byte{0} &&
        car.exact_bytes[226U + floor_index] != std::byte{0}) {
      return true;
    }
  }

  const auto mapped = original_elevator_floor_record_index(
      elevator.type, elevator.bottom_floor, elevator.top_floor, floor);
  if (mapped < 0) return false;
  const auto found = std::find_if(
      elevator.floor_records.begin(), elevator.floor_records.end(),
      [&](const OriginalTdtElevatorFloorRecord& record) {
        return record.mapped_index == mapped;
      });
  return found != elevator.floor_records.end() &&
         (found->exact_bytes[0] != std::byte{0} ||
          found->exact_bytes[2] != std::byte{0});
}

bool remove_original_elevator_service_floor_without_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept {
  if (original_elevator_service_floor_gate(
          document, elevator_index, floor) !=
      OriginalElevatorServiceFloorGate::eligible) {
    return false;
  }
  auto& elevator = document.elevators[elevator_index];
  const auto floor_index = static_cast<std::size_t>(floor);
  if (elevator.serviced_floors[floor_index] == std::byte{0} ||
      original_elevator_service_floor_has_people(
          document, elevator_index, floor)) {
    return false;
  }

  // This preserves 0085's order: the route graph sees the new serviced byte
  // before 14cc releases per-car direction ownership.
  elevator.serviced_floors[floor_index] = std::byte{0};
  rebuild_original_transport_route_graphs(document);

  const bool byte_swapped = document.header.byte_swapped;
  for (std::size_t car_index = 0U;
       car_index < elevator.car_records.size(); ++car_index) {
    auto& car = elevator.car_records[car_index].exact_bytes;
    if (car[15] == std::byte{0}) continue;
    const auto owner = static_cast<std::byte>(car_index + 1U);
    if (elevator.block_2a2[floor_index] == owner) {
      elevator.block_2a2[floor_index] = std::byte{0};
      store_u16(car, 10U,
                static_cast<std::uint16_t>(
                    load_u16(car, 10U, byte_swapped) - 1U),
                byte_swapped);
    }
    if (elevator.block_31a[floor_index] == owner) {
      elevator.block_31a[floor_index] = std::byte{0};
      store_u16(car, 10U,
                static_cast<std::uint16_t>(
                    load_u16(car, 10U, byte_swapped) - 1U),
                byte_swapped);
    }
    recompute_original_elevator_car_state(
        document, elevator_index, car_index);
  }
  return true;
}

OriginalNativeElevatorFloorPeopleCleanupResult
remove_original_elevator_service_floor(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) noexcept {
  OriginalNativeElevatorFloorPeopleCleanupResult result{};
  if (original_elevator_service_floor_gate(
          document, elevator_index, floor) !=
      OriginalElevatorServiceFloorGate::eligible) {
    return result;
  }
  if (floor < 0 || floor >= 120 ||
      document.elevators[elevator_index]
              .serviced_floors[static_cast<std::size_t>(floor)] ==
          std::byte{0}) {
    return result;
  }

  auto working = document;
  working.elevators[elevator_index]
      .serviced_floors[static_cast<std::size_t>(floor)] = std::byte{0};
  // 10a0:0085 publishes the cleared service byte to both route graphs before
  // entering 14cc's car-first/waiting-second cleanup.
  rebuild_original_transport_route_graphs(working);
  result = cleanup_original_elevator_service_floor_people(
      working, elevator_index, floor, part.words_00_to_40[2U], part,
      rent_income);
  if (result.cleanup.status !=
      OriginalElevatorFloorPeopleCleanupStatus::cleaned) {
    return result;
  }
  document = std::move(working);
  return result;
}

bool commit_original_elevator_car_demolition(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index) noexcept {
  if (elevator_index >= document.elevators.size()) return false;
  auto& elevator = document.elevators[elevator_index];
  if (elevator.used == 0U || elevator.cars == 1U ||
      car_index >= elevator.car_records.size() ||
      elevator.car_records[car_index].exact_bytes[15] == std::byte{0}) {
    return false;
  }
  elevator.car_records[car_index].exact_bytes[15] = std::byte{0};
  --elevator.cars;
  return true;
}

OriginalElevatorDemolitionResult remove_original_elevator_car(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) {
  OriginalElevatorDemolitionResult result{};
  if (elevator_index >= document.elevators.size()) return result;
  const auto& source = document.elevators[elevator_index];
  if (source.used == 0U || source.cars == 1U ||
      car_index >= source.car_records.size() ||
      source.car_records[car_index].exact_bytes[15] == std::byte{0} ||
      source.bottom_floor > source.top_floor) {
    return result;
  }

  auto working = document;
  const auto append = [&](auto& cleanup) {
    result.car_passengers += cleanup.cleanup.car_passengers;
    for (auto& dispatch : cleanup.family_dispatches) {
      result.family_dispatches.push_back(std::move(dispatch));
    }
  };
  for (int floor = source.bottom_floor; floor <= source.top_floor; ++floor) {
    auto cleanup = cleanup_original_elevator_selected_car_floor_people(
        working, elevator_index, car_index,
        static_cast<std::int16_t>(floor), part, rent_income);
    if (cleanup.cleanup.status !=
        OriginalElevatorFloorPeopleCleanupStatus::cleaned) {
      return {};
    }
    append(cleanup);
  }
  if (!commit_original_elevator_car_demolition(
          working, elevator_index, car_index)) {
    return {};
  }

  // 036e revisits every floor after retiring the car. A nonempty ring with
  // no owner is immediately assigned through 1090:0a4c, up before down.
  auto& elevator = working.elevators[elevator_index];
  const auto route_context = original_person_route_context(working, part);
  for (int floor = source.bottom_floor; floor <= source.top_floor; ++floor) {
    const auto mapped = original_elevator_floor_record_index(
        elevator.type, elevator.bottom_floor, elevator.top_floor,
        static_cast<std::int16_t>(floor));
    if (mapped < 0) continue;
    const auto record = std::find_if(
        elevator.floor_records.begin(), elevator.floor_records.end(),
        [&](const OriginalTdtElevatorFloorRecord& candidate) {
          return candidate.mapped_index == mapped;
        });
    if (record == elevator.floor_records.end()) continue;
    const auto floor_index = static_cast<std::size_t>(floor);
    if (record->exact_bytes[0] != std::byte{0} &&
        elevator.block_2a2[floor_index] == std::byte{0}) {
      (void)assign_original_elevator_waiting_floor(
          working, elevator_index, static_cast<std::int16_t>(floor), true,
          route_context.calendar_phase, route_context.day_phase);
    }
    if (record->exact_bytes[2] != std::byte{0} &&
        elevator.block_31a[floor_index] == std::byte{0}) {
      (void)assign_original_elevator_waiting_floor(
          working, elevator_index, static_cast<std::int16_t>(floor), false,
          route_context.calendar_phase, route_context.day_phase);
    }
  }

  document = std::move(working);
  result.removed = true;
  return result;
}

OriginalElevatorDemolitionResult remove_original_elevator_shaft(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) {
  OriginalElevatorDemolitionResult result{};
  if (elevator_index >= document.elevators.size()) return result;
  const auto& source = document.elevators[elevator_index];
  if (source.used == 0U || source.bottom_floor > source.top_floor) {
    return result;
  }

  auto working = document;
  auto& elevator = working.elevators[elevator_index];
  for (int floor = source.bottom_floor; floor <= source.top_floor; ++floor) {
    elevator.serviced_floors[static_cast<std::size_t>(floor)] = std::byte{0};
  }
  rebuild_original_transport_route_graphs(working);
  for (int floor = source.bottom_floor; floor <= source.top_floor; ++floor) {
    auto cleanup = cleanup_original_elevator_service_floor_people(
        working, elevator_index, static_cast<std::int16_t>(floor),
        part.words_00_to_40[2U], part, rent_income);
    if (cleanup.cleanup.status !=
        OriginalElevatorFloorPeopleCleanupStatus::cleaned) {
      return {};
    }
    result.car_passengers += cleanup.cleanup.car_passengers;
    result.waiting_passengers += cleanup.cleanup.waiting_passengers;
    for (auto& dispatch : cleanup.family_dispatches) {
      result.family_dispatches.push_back(std::move(dispatch));
    }
  }

  // Exact persisted half of 1090:00d9(elevator, 0): release all owners,
  // clear each serialized ring header, and initialize every car inactive at
  // its stored home floor using the current schedule byte.
  elevator.block_2a2.fill(std::byte{0});
  elevator.block_31a.fill(std::byte{0});
  for (auto& record : elevator.floor_records) {
    std::fill(record.exact_bytes.begin(), record.exact_bytes.begin() + 4U,
              std::byte{0});
  }
  const auto route_context = original_person_route_context(working, part);
  const int floor_mode_index =
      28 + static_cast<int>(route_context.calendar_phase) * 7 +
      route_context.day_phase;
  for (std::size_t car_index = 0U;
       car_index < elevator.car_records.size(); ++car_index) {
    const auto home = std::to_integer<std::uint8_t>(
        elevator.car_home_floors[car_index]);
    const auto floor_mode = floor_mode_index >= 0 &&
                                    static_cast<std::size_t>(floor_mode_index) <
                                        elevator.schedule.size()
                                ? std::to_integer<std::uint8_t>(
                                      elevator.schedule[static_cast<std::size_t>(
                                          floor_mode_index)])
                                : 0U;
    initialize_original_elevator_car(
        elevator.car_records[car_index], home, floor_mode, std::nullopt);
  }
  elevator.used = 0U;

  document = std::move(working);
  result.removed = true;
  result.removed_entire_shaft = true;
  return result;
}

bool commit_original_vertical_transport_demolition(
    OriginalTdtDocument& document,
    std::size_t transport_index) noexcept {
  auto& records = document.post_elevator.stairs_bd70;
  if (transport_index >= records.size() ||
      records[transport_index].used == 0U) {
    return false;
  }

  auto& removed = records[transport_index];
  removed.used = 0U;
  removed.exact_bytes[0] = std::byte{0};
  // 10c0:0000 is invoked immediately after clearing bd70.used.
  removed.word_6 = 0U;
  removed.word_8 = 0U;
  store_u16(removed.exact_bytes, 6U, 0U, document.header.byte_swapped);
  store_u16(removed.exact_bytes, 8U, 0U, document.header.byte_swapped);

  auto& links = document.post_elevator.cf10;
  links.fill(std::byte{0});
  for (const auto& transport : records) {
    if (transport.used == 0U) continue;
    const int base = transport.floor;
    const int height = static_cast<std::int8_t>(transport.shape) / 2;
    const auto bit = static_cast<std::uint8_t>(
        (transport.shape & 1U) == 0U ? 1U : 2U);
    for (int floor = base; floor <= base + height; ++floor) {
      if (floor < 0 || floor >= static_cast<int>(links.size())) continue;
      auto flags = std::to_integer<std::uint8_t>(
          links[static_cast<std::size_t>(floor)]);
      links[static_cast<std::size_t>(floor)] =
          static_cast<std::byte>(flags | bit);
    }
  }
  rebuild_original_vertical_route_summaries(document);
  return true;
}

OriginalVerticalTransportDemolitionResult
remove_original_vertical_transport(
    OriginalTdtDocument& document,
    std::size_t transport_index,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) {
  OriginalVerticalTransportDemolitionResult result{};
  if (transport_index >= document.post_elevator.stairs_bd70.size() ||
      document.post_elevator.stairs_bd70[transport_index].used == 0U) {
    return result;
  }

  auto working = document;
  auto cleanup = cleanup_original_vertical_transport_people(
      working, transport_index, part, rent_income);
  if (!cleanup.valid_transport_index ||
      !commit_original_vertical_transport_demolition(
          working, transport_index)) {
    return result;
  }
  document = std::move(working);
  result.removed = true;
  result.family_dispatches = std::move(cleanup.family_dispatches);
  return result;
}

OriginalConstructionResult build_original_floor(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    std::uint16_t right,
    const OriginalYenTable& construction_costs) {
  if (floor < 0 || floor > 109) {
    return {OriginalConstructionStatus::invalid_floor, 0,
            original_status_code(floor < 0, 20U, 5U)};
  }
  if (left >= right || right > kOriginalWorldGridWidth) {
    return {OriginalConstructionStatus::invalid_span, 0, 20U};
  }

  const auto& old_floor = document.floors[static_cast<std::size_t>(floor)];
  const std::uint32_t added_cells = old_floor.tenants.empty()
      ? static_cast<std::uint32_t>(right - left)
      : static_cast<std::uint32_t>(
            old_floor.left_edge - std::min(old_floor.left_edge, left)) +
            static_cast<std::uint32_t>(
                std::max(old_floor.right_edge, right) - old_floor.right_edge);

  // 1178:035b returns zero for type 0. 1178:0583 supplies the entire cost:
  // ordinary cells use YEN[0], while stories inside the selected Lobby
  // height use YEN[0x18] multiplied by that height. 11f8:284d performs this
  // funds check before its support and type-legality calls.
  std::uint64_t cell_cost = construction_costs[0];
  if (floor >= 10 &&
      floor < 10 + static_cast<int>(document.header.lobby_height)) {
    cell_cost = static_cast<std::uint64_t>(construction_costs[0x18]) *
                document.header.lobby_height;
  }
  const std::uint64_t cost64 =
      static_cast<std::uint64_t>(added_cells) * cell_cost;
  const auto cost = static_cast<std::uint32_t>(cost64);
  if (cost64 > 0x7fffffffULL ||
      (cost != 0U && document.header.balance < static_cast<std::int32_t>(cost))) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(cost), 8U};
  }

  // Literal 11f8:2e64 support cases. Ground floor 10 is unconditional.
  // Above ground, the immediately lower floor must cover the whole request.
  // Below ground, the ground lobby/floor must cover it; opening an empty
  // basement story additionally requires overlap with the story above.
  if (floor > 10) {
    const auto& supporting =
        document.floors[static_cast<std::size_t>(floor - 1)];
    if (supporting.tenants.empty()) {
      return {OriginalConstructionStatus::invalid_span, 0, 3U};
    }
    if (supporting.left_edge > left || supporting.right_edge < right) {
      return {OriginalConstructionStatus::invalid_span, 0, 6U};
    }
  } else if (floor < 10) {
    const auto& supporting =
        document.floors[static_cast<std::size_t>(floor + 1)];
    const auto& ground = document.floors[10];
    if (supporting.tenants.empty()) {
      return {OriginalConstructionStatus::invalid_span, 0, 4U};
    }
    if (ground.tenants.empty() || ground.left_edge > left ||
        ground.right_edge < right) {
      return {OriginalConstructionStatus::invalid_span, 0, 2U};
    }
    if (old_floor.tenants.empty() &&
        (supporting.left_edge >= right || supporting.right_edge <= left)) {
      return {OriginalConstructionStatus::invalid_span, 0, 4U};
    }
  }

  // 11f8:2fab applies this common legality guard before its type tables.
  // DS:b3e8 starts at -1 and later stores the Metro Station's top floor;
  // construction may extend at most one story beneath the Metro stack.
  const auto b3e8 = static_cast<std::int16_t>(
      load_original_header_word(document, 30U));
  if (static_cast<int>(b3e8) - 1 > floor) {
    return {OriginalConstructionStatus::invalid_floor, 0, 14U};
  }

  // 11f8:284d takes the in-place replacement path whenever the requested
  // span overlaps the represented floor. Every record crossed by a type-zero
  // request must itself be type zero. The specialized path preserves edge
  // remainders instead of coalescing the whole connected empty interval.
  const bool overlaps_floor = !old_floor.tenants.empty() &&
      old_floor.left_edge < right && old_floor.right_edge > left;
  std::size_t first_replaced = 0U;
  std::size_t last_replaced = 0U;
  if (overlaps_floor) {
    while (first_replaced < old_floor.tenants.size() &&
           old_floor.tenants[first_replaced].right <= left) {
      ++first_replaced;
    }
    if (first_replaced >= old_floor.tenants.size()) {
      return {OriginalConstructionStatus::occupied, 0, 9U};
    }
    last_replaced = first_replaced;
    while (last_replaced + 1U < old_floor.tenants.size() &&
           old_floor.tenants[last_replaced].right < right) {
      ++last_replaced;
    }
    for (std::size_t index = first_replaced;
         index <= last_replaced; ++index) {
      if (old_floor.tenants[index].type != 0) {
        return {OriginalConstructionStatus::occupied, 0, 9U};
      }
    }
  }

  std::vector<OriginalTdtTenant> tenants;
  tenants.reserve(old_floor.tenants.size() + 2U);
  const auto requested = make_original_empty_floor_tenant(
      left, right, document.header.byte_swapped);
  if (old_floor.tenants.empty()) {
    // 11f8:17fd 1889-18e5.
    tenants.push_back(requested);
  } else if (!overlaps_floor && right <= old_floor.left_edge) {
    // 11f8:17fd 18e8-1a3d. A disjoint request creates two records: the
    // requested Floor, followed by 30ef's automatic Floor/Lobby gap.
    tenants.push_back(requested);
    if (right != old_floor.left_edge) {
      tenants.push_back(make_original_automatic_floor_tenant(
          document, floor, right, old_floor.left_edge));
    }
    tenants.insert(tenants.end(), old_floor.tenants.begin(),
                   old_floor.tenants.end());
  } else if (!overlaps_floor && left >= old_floor.right_edge) {
    // 11f8:17fd 1a40-1b1c. The gap precedes the requested record on a
    // right-side extension; adjacent placements omit only the gap.
    tenants = old_floor.tenants;
    if (left != old_floor.right_edge) {
      tenants.push_back(make_original_automatic_floor_tenant(
          document, floor, old_floor.right_edge, left));
    }
    tenants.push_back(requested);
  } else {
    // 11f8:284d 2ac6-2dd4. Replace the covered run with one fresh record and
    // retain byte-identical copies of the two possible edge remainders.
    tenants.insert(tenants.end(), old_floor.tenants.begin(),
                   old_floor.tenants.begin() +
                       static_cast<std::ptrdiff_t>(first_replaced));
    if (old_floor.tenants[first_replaced].left < left) {
      auto remainder = old_floor.tenants[first_replaced];
      set_tenant_right(remainder, left, document.header.byte_swapped);
      tenants.push_back(std::move(remainder));
    }
    tenants.push_back(requested);
    if (old_floor.tenants[last_replaced].right > right) {
      auto remainder = old_floor.tenants[last_replaced];
      set_tenant_left(remainder, right, document.header.byte_swapped);
      tenants.push_back(std::move(remainder));
    }
    tenants.insert(
        tenants.end(),
        old_floor.tenants.begin() +
            static_cast<std::ptrdiff_t>(last_replaced + 1U),
        old_floor.tenants.end());
  }
  if (tenants.size() > OriginalTdtFloor::kTenantCapacity) {
    return {OriginalConstructionStatus::tenant_limit, 0, 9U};
  }

  auto& target = document.floors[static_cast<std::size_t>(floor)];
  target.tenants = std::move(tenants);
  update_floor_bounds(target);
  rebuild_original_floor_lookup(target);
  document.header.balance = wrapping_subtract(document.header.balance, cost);
  document.header.construction_costs =
      wrapping_subtract(document.header.construction_costs, cost);
  return {OriginalConstructionStatus::ok,
          static_cast<std::int32_t>(cost),
          0U,
          !old_floor.tenants.empty() && old_floor.left_edge < right &&
              old_floor.right_edge > left && cost != 0U,
          true};
}

namespace {

OriginalConstructionResult construct_original_ground_lobby_story(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    std::uint16_t right,
    bool preview,
    std::uint16_t lobby_height,
    const OriginalYenTable& construction_costs) {
  // One literal 26dd -> 284d story call. 1178:035b contributes zero on the
  // automatic Lobby floors while 1178:0583 charges newly exposed floor-edge
  // cells at YEN[24] * b3e6. Preview stories still compute that amount (for
  // 284d's sound test) but skip 009e/0697's affordability and debit paths.
  if (left >= right || right > kOriginalWorldGridWidth || floor < 10 ||
      floor >= static_cast<std::int16_t>(10 + lobby_height)) {
    return {OriginalConstructionStatus::invalid_span, 0, 20U};
  }

  const auto& old_floor = document.floors[static_cast<std::size_t>(floor)];
  const std::uint32_t added_cells = old_floor.tenants.empty()
      ? static_cast<std::uint32_t>(right - left)
      : static_cast<std::uint32_t>(
            old_floor.left_edge - std::min(old_floor.left_edge, left)) +
            static_cast<std::uint32_t>(
                std::max(old_floor.right_edge, right) - old_floor.right_edge);
  const std::uint64_t computed_cost64 =
      static_cast<std::uint64_t>(added_cells) *
      static_cast<std::uint64_t>(construction_costs[0x18]) * lobby_height;
  const auto computed_cost = static_cast<std::uint32_t>(computed_cost64);
  if (!preview &&
      (computed_cost64 > 0x7fffffffULL ||
       (computed_cost != 0U &&
        document.header.balance < static_cast<std::int32_t>(computed_cost)))) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(computed_cost), 8U};
  }

  // Exact 2e64 support ordering follows the funds test. Ground is
  // unconditional; every automatic upper Lobby must fit on the story below.
  if (floor > 10) {
    const auto& supporting =
        document.floors[static_cast<std::size_t>(floor - 1)];
    if (supporting.tenants.empty()) {
      return {OriginalConstructionStatus::invalid_span, 0, 3U};
    }
    if (supporting.left_edge > left || supporting.right_edge < right) {
      return {OriginalConstructionStatus::invalid_span, 0, 6U};
    }
  }

  // 284d delegates an empty or disjoint floor to 17fd. Only an overlap stays
  // in 284d and can request WAVE/7001 after a nonzero computed cost.
  const bool overlapping_284d_path = !old_floor.tenants.empty() &&
      old_floor.left_edge < right && old_floor.right_edge > left;
  auto replacement = replace_original_floor_or_lobby_run(
      document, floor, left, right, 0x18U);
  if (replacement.status != OriginalFloorRunReplacementStatus::replaced) {
    return {replacement.status == OriginalFloorRunReplacementStatus::tenant_limit
                ? OriginalConstructionStatus::tenant_limit
                : OriginalConstructionStatus::occupied,
            0, 9U};
  }

  auto& target = document.floors[static_cast<std::size_t>(floor)];
  target.tenants = std::move(replacement.tenants);
  update_floor_bounds(target);
  rebuild_original_floor_lookup(target);
  if (!preview) {
    document.header.balance =
        wrapping_subtract(document.header.balance, computed_cost);
    document.header.construction_costs =
        wrapping_subtract(document.header.construction_costs, computed_cost);
  }
  return {OriginalConstructionStatus::ok,
          preview ? 0 : static_cast<std::int32_t>(computed_cost),
          0U,
          overlapping_284d_path && computed_cost != 0U,
          true};
}

}  // namespace

OriginalConstructionResult build_original_initial_lobby(
    OriginalTdtDocument& document,
    std::uint16_t left,
    std::uint16_t right,
    std::uint16_t lobby_height,
    const OriginalYenTable& construction_costs) {
  if (lobby_height < 1U || lobby_height > 3U) {
    return {OriginalConstructionStatus::invalid_lobby_height, 0};
  }
  if (document.header.lobby_height != 0U) {
    return {OriginalConstructionStatus::lobby_already_initialized, 0};
  }
  // 11f8:098f-09b3 publishes b3e6 before 26dd performs any geometry, funds,
  // support, or occupancy check. A rejected construction therefore retains
  // the selected Lobby height.
  document.header.lobby_height = lobby_height;

  OriginalConstructionResult final{};
  std::int32_t charged{};
  bool sound_requested{};
  bool document_changed = true;  // b3e6 publication precedes every story.
  for (std::uint16_t story = 0; story < lobby_height; ++story) {
    const auto floor = static_cast<std::int16_t>(10 + story);
    const bool preview = story != 0U;
    final = construct_original_ground_lobby_story(
        document, floor, left, right, preview, lobby_height,
        construction_costs);
    if (!preview && final.succeeded()) charged = final.cost;
    sound_requested = sound_requested || final.construction_sound_requested;
    document_changed = document_changed || final.document_changed;
  }
  if (charged != 0 || final.succeeded()) final.cost = charged;
  final.construction_sound_requested = sound_requested;
  final.document_changed = document_changed;
  return final;
}

OriginalConstructionResult extend_original_lobby(
    OriginalTdtDocument& document,
    std::uint16_t desired_left,
    std::uint16_t desired_right,
    const OriginalYenTable& construction_costs) {
  const std::uint16_t height = document.header.lobby_height;
  if (height < 1U || height > 3U) {
    return {OriginalConstructionStatus::lobby_not_initialized, 0};
  }

  // 26dd does not preflight or roll back the selected stories. It invokes
  // 284d for ground first and then each automatic upper story with preview=1,
  // overwriting its retained return after every call.
  OriginalConstructionResult final{};
  std::int32_t charged{};
  bool sound_requested{};
  bool document_changed{};
  for (std::uint16_t story = 0; story < height; ++story) {
    const auto floor = static_cast<std::int16_t>(10 + story);
    const bool preview = story != 0U;
    final = construct_original_ground_lobby_story(
        document, floor, desired_left, desired_right, preview, height,
        construction_costs);
    if (!preview && final.succeeded()) charged = final.cost;
    sound_requested = sound_requested || final.construction_sound_requested;
    document_changed = document_changed || final.document_changed;
  }
  if (charged != 0 || final.succeeded()) final.cost = charged;
  final.construction_sound_requested = sound_requested;
  final.document_changed = document_changed;
  return final;
}

OriginalConstructionResult build_original_sky_lobby(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    std::uint16_t right,
    const OriginalYenTable& construction_costs) {
  // 10a0:1366 accepts the automatic ground-Lobby stories or 12e0's exact
  // (floor-9) % 15 cadence. This entry point is the non-ground branch;
  // 11f8:2f5a applies its ordinary floor-109 ceiling first.
  if (floor <= 10 || floor > 109) {
    return {OriginalConstructionStatus::invalid_floor, 0,
            original_status_code(floor > 109, 5U, 13U)};
  }
  if (left >= right || right > kOriginalWorldGridWidth) {
    return {OriginalConstructionStatus::invalid_span, 0, 20U};
  }

  const auto& old_floor = document.floors[static_cast<std::size_t>(floor)];
  const std::uint32_t lobby_cells =
      original_sky_lobby_chargeable_cells(old_floor, left, right);
  const std::uint32_t floor_cells = old_floor.tenants.empty()
      ? static_cast<std::uint32_t>(right - left)
      : static_cast<std::uint32_t>(
            old_floor.left_edge - std::min(old_floor.left_edge, left)) +
            static_cast<std::uint32_t>(
                std::max(old_floor.right_edge, right) -
                old_floor.right_edge);
  const std::uint64_t facility_cost64 =
      static_cast<std::uint64_t>(lobby_cells) * construction_costs[0x18];
  const std::uint64_t floor_cost64 =
      static_cast<std::uint64_t>(floor_cells) * construction_costs[0];
  const std::uint64_t cost64 = facility_cost64 + floor_cost64;
  const auto cost = static_cast<std::uint32_t>(cost64);
  if (facility_cost64 > 0x7fffffffULL || cost64 > 0x7fffffffULL ||
      (cost != 0U &&
       document.header.balance < static_cast<std::int32_t>(cost))) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(cost),
            original_funds_status_code(document.header.balance,
                                        facility_cost64)};
  }

  // 11f8:284d performs its funds check before 2e64 support and 2f5a cadence.
  const auto& supporting =
      document.floors[static_cast<std::size_t>(floor - 1)];
  if (supporting.tenants.empty()) {
    return {OriginalConstructionStatus::invalid_span, 0, 3U};
  }
  if (supporting.left_edge > left || supporting.right_edge < right) {
    return {OriginalConstructionStatus::invalid_span, 0, 6U};
  }
  if ((floor - 9) % 15 != 0) {
    return {OriginalConstructionStatus::invalid_floor, 0, 13U};
  }

  auto replacement = replace_original_floor_or_lobby_run(
      document, floor, left, right, 0x18U);
  if (replacement.status != OriginalFloorRunReplacementStatus::replaced) {
    return {replacement.status ==
                    OriginalFloorRunReplacementStatus::tenant_limit
                ? OriginalConstructionStatus::tenant_limit
                : OriginalConstructionStatus::occupied,
            0, 9U};
  }

  auto& target = document.floors[static_cast<std::size_t>(floor)];
  target.tenants = std::move(replacement.tenants);
  update_floor_bounds(target);
  rebuild_original_floor_lookup(target);
  document.header.balance = wrapping_subtract(document.header.balance, cost);
  document.header.construction_costs =
      wrapping_subtract(document.header.construction_costs, cost);
  return {OriginalConstructionStatus::ok,
          static_cast<std::int32_t>(cost),
          0U,
          !old_floor.tenants.empty() && old_floor.left_edge < right &&
              old_floor.right_edge > left && cost != 0U,
          true};
}

OriginalConstructionResult build_original_elevator(
    OriginalTdtDocument& document,
    std::uint16_t command_type,
    std::int16_t floor,
    std::uint16_t x,
    const OriginalYenTable& construction_costs,
    const OriginalPartTable& part) {
  // Complete native transaction rooted at 11f8:0fea: legality, existing-car
  // insertion, funds checks, shaft collision/allocation, initialization, and
  // the balance/construction-cost commit.
  std::uint8_t elevator_type{};
  std::uint8_t capacity{};
  std::uint16_t width{};
  std::size_t car_part_offset{};
  switch (command_type) {
    case 1:
      elevator_type = 1U;
      capacity = 0x15U;
      width = 4U;
      car_part_offset = 0x90U;
      break;
    case 42:
      elevator_type = 0U;
      capacity = 0x2aU;
      width = 6U;
      car_part_offset = 0x92U;
      break;
    case 43:
      elevator_type = 2U;
      capacity = 0x15U;
      width = 4U;
      car_part_offset = 0x94U;
      break;
    default:
      return {OriginalConstructionStatus::invalid_span, 0};
  }

  if (floor < 1 || floor >= static_cast<std::int16_t>(document.floors.size())) {
    return {OriginalConstructionStatus::invalid_floor, 0};
  }
  // 11f8:0fff-1010 permits an express shaft above floor ten only on the
  // exact 10a0:12e0 sequence 24, 39, 54, 69, 84, 99.
  if (elevator_type == 0U && floor > 10 && (floor - 9) % 15 != 0) {
    return {OriginalConstructionStatus::invalid_floor, 0};
  }
  if (x > kOriginalWorldGridWidth - width) {
    return {OriginalConstructionStatus::invalid_span, 0, 20U};
  }

  // 10a0:133b rejects a new elevator inside the automatic upper stories of
  // a multi-story lobby. Ground floor 10 remains the legal starting floor.
  if (floor >= 11 &&
      floor < static_cast<std::int16_t>(10U + document.header.lobby_height)) {
    return {OriginalConstructionStatus::invalid_floor, 0};
  }

  const auto& supporting_floor =
      document.floors[static_cast<std::size_t>(floor)];
  if (supporting_floor.tenants.empty()) {
    // 11f8:2e64 requires the complete four- or six-cell shaft footprint to
    // be inside already constructed floor/lobby bounds.
    return {OriginalConstructionStatus::invalid_span, 0,
            original_status_code(
                floor > 10, 3U,
                original_status_code(floor < 10, 4U, 2U))};
  }
  if (supporting_floor.left_edge > x ||
      supporting_floor.right_edge < x + width) {
    return {OriginalConstructionStatus::invalid_span, 0,
            original_status_code(floor > 10, 6U, 2U)};
  }

  // 10a0:1080 searches before the new-shaft collision/allocation path.
  for (auto& elevator : document.elevators) {
    if (elevator.used == 0U || elevator.type != elevator_type ||
        elevator.x != x ||
        floor < elevator.bottom_floor || floor > elevator.top_floor) {
      continue;
    }
    // 1148:02c8 checks the persisted +3 car-count byte against literal eight
    // and reports STRL/1003 entry 24 before 11f8:113f scans the raw records.
    if (elevator.cars >= 8U) {
      return {OriginalConstructionStatus::elevator_car_limit, 0, 24U};
    }
    std::size_t car_index = elevator.car_records.size();
    for (std::size_t index = 0; index < elevator.car_records.size(); ++index) {
      if (elevator.car_records[index].exact_bytes[15] == std::byte{0}) {
        car_index = index;
        break;
      }
    }
    if (car_index == elevator.car_records.size()) {
      // If an imported record disagrees with the count byte, 11f8:1163 fails
      // silently after 02c8 already succeeded; it does not emit entry 24.
      return {OriginalConstructionStatus::elevator_car_limit, 0, 0U};
    }

    // 1190:0005 copies PART/1000 offsets 0x90/0x92/0x94 to de0a/de0c/de0e;
    // 11f8:10f7-11ed selects the word by stored elevator type.
    const std::size_t car_part_index =
        (car_part_offset - 0x52U) / 2U;
    const std::uint32_t cost =
        part.words_52_to_ac[car_part_index];
    if (cost != 0U && document.header.balance < static_cast<std::int32_t>(cost)) {
      return {OriginalConstructionStatus::insufficient_funds,
              static_cast<std::int32_t>(cost), 7U};
    }

    const auto floor_byte = static_cast<std::uint8_t>(floor);
    elevator.car_home_floors[car_index] = static_cast<std::byte>(floor_byte);
    const int floor_mode_index =
        28 + static_cast<int>(
                 original_calendar_phase(document.header.current_day)) * 7 +
        original_day_phase(document.header.frame_time);
    const auto floor_mode = floor_mode_index >= 0 &&
                                    static_cast<std::size_t>(floor_mode_index) <
                                        elevator.schedule.size()
                                ? std::to_integer<std::uint8_t>(
                                      elevator.schedule[static_cast<std::size_t>(
                                          floor_mode_index)])
                                : 0U;
    initialize_original_elevator_car(
        elevator.car_records[car_index], floor_byte, floor_mode, true);
    ++elevator.cars;
    document.header.balance = wrapping_subtract(document.header.balance, cost);
    document.header.construction_costs =
        wrapping_subtract(document.header.construction_costs, cost);
    return {OriginalConstructionStatus::ok,
            static_cast<std::int32_t>(cost),
            0U,
            false,
            true,
            false};
  }

  // 1148:0277 counts all 24 used elevator pointers and reports entry 25 only
  // when every slot is occupied. Selecting the first free slot below is the
  // following 11f8:127b allocation scan.
  std::size_t free_index = document.elevators.size();
  for (std::size_t index = 0; index < document.elevators.size(); ++index) {
    if (document.elevators[index].used == 0U) {
      free_index = index;
      break;
    }
  }
  if (free_index == document.elevators.size()) {
    return {OriginalConstructionStatus::elevator_limit, 0, 25U};
  }

  // Exact 10a0:10e8 candidate rectangle: [x-8,x+width+8) x
  // [floor-2,floor+1). Existing elevator/stair rectangles are unexpanded;
  // this is the original minimum-spacing collision test.
  for (const auto& elevator : document.elevators) {
    if (elevator.used == 0U) {
      continue;
    }
    const int existing_width = elevator.type == 0U ? 6 : 4;
    if (overlaps_signed(static_cast<int>(x) - 8,
                        static_cast<int>(x) + static_cast<int>(width) + 8,
                        elevator.x, elevator.x + existing_width) &&
        overlaps_signed(floor - 2, floor + 1,
                        elevator.bottom_floor - 2,
                        elevator.top_floor + 1)) {
      return {OriginalConstructionStatus::occupied, 0, 22U};
    }
  }
  for (const auto& stair : document.post_elevator.stairs_bd70) {
    if (stair.used == 0U) {
      continue;
    }
    // 10a0:1247-124c sign-extends the persisted byte and performs SAR AX,1.
    // Keep that distinction for high-bit/malformed saves.
    const int stair_height = original_signed_shape_half(stair.shape);
    if (overlaps_signed(static_cast<int>(x) - 8,
                        static_cast<int>(x) + static_cast<int>(width) + 8,
                        stair.x, stair.x + 8) &&
        overlaps_signed(floor - 2, floor + 1,
                        static_cast<int>(stair.floor) - 1,
                        static_cast<int>(stair.floor) + stair_height + 1)) {
      return {OriginalConstructionStatus::occupied, 0, 22U};
    }
  }

  const std::uint32_t cost = construction_costs[command_type];
  if (cost != 0U && document.header.balance < static_cast<std::int32_t>(cost)) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(cost), 7U};
  }

  initialize_original_elevator(document.elevators[free_index], elevator_type,
                               capacity, x,
                               static_cast<std::uint8_t>(floor));
  document.header.balance = wrapping_subtract(document.header.balance, cost);
  document.header.construction_costs =
      wrapping_subtract(document.header.construction_costs, cost);
  return {OriginalConstructionStatus::ok,
          static_cast<std::int32_t>(cost),
          0U,
          false,
          true,
          true};
}

OriginalConstructionResult build_original_standard_elevator(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t x,
    const OriginalYenTable& construction_costs,
    const OriginalPartTable& part) {
  return build_original_elevator(
      document, 1U, floor, x, construction_costs, part);
}

OriginalConstructionResult build_original_vertical_transport(
    OriginalTdtDocument& document,
    std::uint8_t type,
    std::int16_t floor,
    std::uint16_t x,
    const OriginalYenTable& construction_costs) {
  if (type != 22U && type != 27U) {
    return {OriginalConstructionStatus::invalid_span, 0};
  }
  if (x > kOriginalWorldGridWidth - 8U) {
    return {OriginalConstructionStatus::invalid_span, 0, 20U};
  }

  // 11f8:0a21 supplies one for Stairs; 0a1d supplies zero for Escalator.
  std::uint8_t shape = type == 22U ? 1U : 0U;
  int upper = floor;
  const int lobby_height = document.header.lobby_height;
  if (lobby_height >= 2 && upper > 10 && upper <= 10 + lobby_height) {
    shape = static_cast<std::uint8_t>(
        shape + 2 * (lobby_height - 1));
    upper = 10 + lobby_height;
  }
  const int height = static_cast<int>(shape) / 2;
  const int lower = upper - height - 1;
  if (lower < 0 || upper < 0 ||
      upper >= static_cast<int>(document.floors.size())) {
    return {OriginalConstructionStatus::invalid_floor, 0,
            original_status_code(type == 22U, 29U, 28U)};
  }

  // 1178:011d first checks the facility charge, then adds 1178:0583's
  // exposed-floor preview for every crossed story. A legal landing below
  // necessarily makes the preview contribution zero, but preserving this
  // ordering also matches malformed imported documents.
  std::uint64_t preview_cost = construction_costs[type];
  for (int crossed = lower; crossed <= upper; ++crossed) {
    const auto& crossed_floor =
        document.floors[static_cast<std::size_t>(crossed)];
    std::uint32_t added_cells = 0U;
    if (crossed_floor.tenants.empty()) {
      added_cells = 8U;
    } else {
      if (crossed_floor.left_edge > x) {
        added_cells += crossed_floor.left_edge - x;
      }
      const int right = static_cast<int>(x) + 8;
      if (crossed_floor.right_edge < right) {
        added_cells += static_cast<std::uint32_t>(
            right - crossed_floor.right_edge);
      }
    }
    std::uint64_t cell_cost = construction_costs[0];
    if (crossed >= 10 && crossed < 10 + lobby_height) {
      cell_cost = static_cast<std::uint64_t>(construction_costs[0x18]) *
                  document.header.lobby_height;
    }
    preview_cost += static_cast<std::uint64_t>(added_cells) * cell_cost;
  }
  if (preview_cost > 0x7fffffffULL ||
      (preview_cost != 0U &&
       document.header.balance < static_cast<std::int32_t>(preview_cost))) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(preview_cost),
            original_funds_status_code(document.header.balance,
                                       construction_costs[type])};
  }

  if (!original_vertical_upper_landing(
          document.floors[static_cast<std::size_t>(upper)], x, shape) ||
      !original_vertical_lower_landing(
          document.floors[static_cast<std::size_t>(lower)], x, shape)) {
    return {OriginalConstructionStatus::invalid_span, 0, 28U};
  }

  std::size_t free_index = document.post_elevator.stairs_bd70.size();
  for (std::size_t index = 0;
       index < document.post_elevator.stairs_bd70.size(); ++index) {
    if (document.post_elevator.stairs_bd70[index].used == 0U) {
      free_index = index;
      break;
    }
  }
  if (free_index == document.post_elevator.stairs_bd70.size()) {
    return {OriginalConstructionStatus::vertical_transport_limit, 0,
            original_status_code(type == 22U, 27U, 26U)};
  }

  const bool collides = height == 0
      ? original_normal_vertical_transport_collision(
            document, lower, x, free_index)
      : original_tall_vertical_transport_collision(
            document, lower, x, shape, free_index);
  if (collides) {
    return {OriginalConstructionStatus::occupied, 0, 23U};
  }

  auto& record = document.post_elevator.stairs_bd70[free_index];
  record.used = 1U;
  record.shape = shape;
  record.x = x;
  record.floor = static_cast<std::int8_t>(lower);
  record.byte_5 = 0U;
  // 10c0:0000 clears the two animation/state words after construction.
  record.word_6 = 0U;
  record.word_8 = 0U;
  record.exact_bytes.fill(std::byte{0});
  record.exact_bytes[0] = std::byte{1};
  record.exact_bytes[1] = static_cast<std::byte>(shape);
  store_u16(record.exact_bytes, 2U, x, document.header.byte_swapped);
  record.exact_bytes[4] = static_cast<std::byte>(lower);

  for (int link = upper - 1; link >= lower; --link) {
    auto flags = std::to_integer<std::uint8_t>(
        document.post_elevator.cf10[static_cast<std::size_t>(link)]);
    flags = static_cast<std::uint8_t>(
        flags | ((shape & 1U) == 0U ? 1U : 2U));
    document.post_elevator.cf10[static_cast<std::size_t>(link)] =
        static_cast<std::byte>(flags);
  }
  rebuild_original_vertical_route_summaries(document);

  // 1178:0703 debits only YEN[type], not the already-zero floor preview.
  const std::uint32_t cost = construction_costs[type];
  document.header.balance = wrapping_subtract(document.header.balance, cost);
  document.header.construction_costs =
      wrapping_subtract(document.header.construction_costs, cost);
  return {OriginalConstructionStatus::ok, static_cast<std::int32_t>(cost)};
}

OriginalConstructionResult build_original_deferred_facility(
    OriginalTdtDocument& document,
    std::uint8_t type,
    std::int16_t floor,
    std::uint16_t left,
    std::uint8_t variant,
    const OriginalYenTable& construction_costs,
    bool enforce_balance = true) {
  std::uint16_t width = 0;
  std::uint8_t variant_count = 0;
  std::size_t people_per_facility = 0;
  switch (type) {
    case 3:
      width = 4;
      variant_count = 2;
      people_per_facility = 2;
      break;
    case 4:
      width = 6;
      variant_count = 4;
      people_per_facility = 3;
      break;
    case 5:
      width = 10;
      variant_count = 2;
      people_per_facility = 3;
      break;
    case 9:
      width = 16;
      variant_count = 3;
      people_per_facility = 3;
      break;
    case 10:
      width = 12;
      variant_count = 1;
      people_per_facility = 48;
      break;
    case 6:
      width = 24;
      variant_count = 1;
      people_per_facility = 48;
      break;
    case 12:
      width = 16;
      variant_count = 1;
      people_per_facility = 48;
      break;
    case 14:
      width = 16;
      variant_count = 1;
      people_per_facility = 6;
      break;
    case 15:
      width = 15;
      variant_count = 1;
      people_per_facility = 6;
      break;
    case 17:
      width = 2;
      variant_count = 1;
      people_per_facility = 6;
      break;
    case 18:
    case 19:
      width = 24;
      variant_count = 1;
      people_per_facility = 56;
      break;
    case 20:
    case 21:
      width = 25;
      variant_count = 1;
      people_per_facility = 6;
      break;
    case 29:
    case 30:
      width = 24;
      variant_count = 1;
      people_per_facility = 40;
      break;
    case 31:
    case 32:
      width = 30;
      variant_count = 2;
      people_per_facility = 6;
      break;
    case 33:
      width = 30;
      variant_count = 2;
      people_per_facility = 240;
      break;
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
      width = 28;
      variant_count = 2;
      people_per_facility = 8;
      break;
    case 13:
      width = 26;
      variant_count = 3;
      people_per_facility = 6;
      break;
    default:
      return {OriginalConstructionStatus::invalid_span, 0};
  }
  const auto right = static_cast<std::uint16_t>(left + width);

  // Types 3/4/5/9 are above-ground only. Type 10 takes the common branch of
  // 11f8:2f5a: floors 0..9 and 11..109 are legal, but ground floor 10 is not.
  // The routine's first four-key table exempts only types 1, 24, 42, and 43
  // from that ground-floor rejection; type 19 therefore does not receive a
  // special ground-floor path even though it is the lower Movie half.
  const bool commercial = type == 6U || type == 10U || type == 12U;
  const bool cathedral_part = type >= 36U && type <= 40U;
  const bool common_all_floors =
      commercial || type == 13U || type == 14U || type == 15U ||
      type == 17U || type == 18U || type == 19U || type == 29U ||
      type == 30U || type == 31U || type == 32U || type == 33U ||
      cathedral_part;
  const bool recycling_half = type == 20U || type == 21U;
  const bool reversed_support =
      (common_all_floors || recycling_half) && floor >= 0 && floor < 10;
  const std::int16_t maximum_floor = cathedral_part ? 119 : 109;
  if ((common_all_floors &&
        (floor < 0 || floor > maximum_floor ||
         floor == 10)) ||
      (recycling_half && (floor < 0 || floor >= 10)) ||
      (!common_all_floors && !recycling_half &&
       (floor < 11 || floor > 109))) {
    const std::uint16_t code = floor < 0
        ? 20U
        : (floor == 10
               ? 12U
               : (floor > maximum_floor
                      ? 5U
                      : (recycling_half ? 11U : 10U)));
    return {OriginalConstructionStatus::invalid_floor, 0, code};
  }
  if (cathedral_part && floor != static_cast<std::int16_t>(149U - type)) {
    // 11f8:2291 is the only construction caller for types 36..40. Its
    // selected type-36 floor is fixed at 113 and it emits the remaining
    // four parts on 112..109 respectively.
    return {OriginalConstructionStatus::invalid_floor, 0, 16U};
  }
  if ((type == 18U || type == 29U) && floor >= 11 &&
      floor <= static_cast<std::int16_t>(10U + document.header.lobby_height)) {
    return {OriginalConstructionStatus::invalid_floor, 0, 12U};
  }
  if (variant >= variant_count) {
    return {OriginalConstructionStatus::invalid_span, 0};
  }
  if (left > kOriginalWorldGridWidth - width) {
    return {OriginalConstructionStatus::invalid_span, 0, 20U};
  }

  const auto& old_floor = document.floors[static_cast<std::size_t>(floor)];
  if (reversed_support) {
    // 11f8:2fab applies the persisted signed DS:b3e8 Metro-floor guard. It
    // is at byte 30 in 0x20+ headers and byte 28 in earlier revisions.
    const std::size_t b3e8_offset =
        document.header.format_version >= 0x20U ? 30U : 28U;
    if (document.header.exact_bytes.size() < b3e8_offset + 2U) {
      return {OriginalConstructionStatus::invalid_floor, 0, 14U};
    }
    const auto b3e8 = static_cast<std::int16_t>(load_u16(
        std::span<const std::byte>(document.header.exact_bytes), b3e8_offset,
        document.header.byte_swapped));
    if (static_cast<int>(b3e8) - 1 > floor) {
      return {OriginalConstructionStatus::invalid_floor, 0, 14U};
    }
    // After the common b3e8-1 extension gate, 11f8:2fb5 applies a stricter
    // boundary to the selected upper halves of Movie Theater, Recycling
    // Center, and Party Hall. Unlike ordinary facilities, types 18/20/29 may
    // not occupy the one new story immediately below b3e8; they must be at
    // b3e8 or above. Their floor-zero rejection is the preceding 2fc9 branch.
    if ((type == 18U || type == 20U || type == 29U) && floor < b3e8) {
      return {OriginalConstructionStatus::invalid_floor, 0, 14U};
    }

    const auto& supporting =
        document.floors[static_cast<std::size_t>(floor + 1)];
    const auto& lobby = document.floors[10];
    if (supporting.tenants.empty()) {
      return {OriginalConstructionStatus::invalid_span, 0, 4U};
    }
    if (lobby.tenants.empty() || lobby.left_edge > left ||
        lobby.right_edge < right) {
      return {OriginalConstructionStatus::invalid_span, 0, 2U};
    }
    // 11f8:2f15 allows an already-open basement floor directly. Opening a
    // new one additionally requires the new span to overlap its support.
    if (old_floor.tenants.empty() &&
        (supporting.left_edge >= right || supporting.right_edge <= left)) {
      return {OriginalConstructionStatus::invalid_span, 0, 4U};
    }
  } else {
    const auto& supporting =
        document.floors[static_cast<std::size_t>(floor - 1)];
    if (supporting.tenants.empty()) {
      return {OriginalConstructionStatus::invalid_span, 0, 3U};
    }
    if (supporting.left_edge > left || supporting.right_edge < right) {
      return {OriginalConstructionStatus::invalid_span, 0, 6U};
    }
  }

  const auto key = first_original_floor_key(old_floor);
  if (!key) {
    return {OriginalConstructionStatus::tenant_limit, 0, 9U};
  }
  // The 11f8:07d8 dispatcher, not 17fd's generic constructor, gates the
  // persisted commercial/Security/Medical counters. Their public dispatch
  // wrappers preserve the original exact-equality sentinels and priority over
  // floor/span validation.
  const auto people_start =
      first_original_people_run(document, people_per_facility);
  if (!people_start) {
    return {OriginalConstructionStatus::tenant_limit, 0, 9U};
  }
  if (document.post_elevator.b92e_counter > 10U) {
    return {OriginalConstructionStatus::pending_queue_full, 0, 9U};
  }
  if (document.post_elevator.b92e_counter == 10U) {
    auto working = document;
    if (!activate_original_pending_facility(working)) {
      return {OriginalConstructionStatus::pending_queue_full, 0, 9U};
    }
    const auto result = build_original_deferred_facility(
        working, type, floor, left, variant, construction_costs,
        enforce_balance);
    if (result.succeeded()) {
      document = std::move(working);
    }
    return result;
  }

  auto new_tenants = old_floor.tenants;
  auto new_left = old_floor.left_edge;
  auto new_right = old_floor.right_edge;
  auto room = make_original_pending_facility(
      left, right, type, variant, *key, *people_start,
      document.header.byte_swapped);
  const auto insertion = insert_original_office_record(
      new_tenants, new_left, new_right, std::move(room),
      document.header.byte_swapped);
  if (insertion != OfficeInsertionResult::inserted) {
    return {insertion == OfficeInsertionResult::tenant_limit
                ? OriginalConstructionStatus::tenant_limit
                : OriginalConstructionStatus::occupied,
            0, 9U};
  }

  const std::uint32_t added_cells = old_floor.tenants.empty()
      ? width
      : static_cast<std::uint32_t>(old_floor.left_edge -
                                   std::min(old_floor.left_edge, left)) +
            static_cast<std::uint32_t>(
                std::max(old_floor.right_edge, right) - old_floor.right_edge);
  const std::uint64_t cost64 =
      static_cast<std::uint64_t>(construction_costs[type]) +
      static_cast<std::uint64_t>(added_cells) * construction_costs[0];
  const auto cost = static_cast<std::uint32_t>(cost64);
  if (cost64 > 0x7fffffffULL ||
      (enforce_balance && cost != 0U &&
       document.header.balance < static_cast<std::int32_t>(cost))) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(cost),
            original_funds_status_code(document.header.balance,
                                       construction_costs[type])};
  }

  auto& target = document.floors[static_cast<std::size_t>(floor)];
  target.tenants = std::move(new_tenants);
  target.left_edge = new_left;
  target.right_edge = new_right;
  rebuild_original_floor_lookup(target);

  const std::size_t required_end =
      static_cast<std::size_t>(*people_start) + people_per_facility;
  if (document.people.size() - required_end < 256U) {
    document.people.resize(document.people.size() + 256U);
    document.people_count = static_cast<std::uint32_t>(document.people.size());
  }
  const auto pending_type = static_cast<std::byte>(
      static_cast<std::uint8_t>(-static_cast<std::int16_t>(type)));
  for (std::size_t index = 0; index < people_per_facility; ++index) {
    auto& exact = document.people[*people_start + index].exact_bytes;
    exact.fill(std::byte{0});
    exact[0] = static_cast<std::byte>(static_cast<std::uint8_t>(floor));
    exact[1] = static_cast<std::byte>(*key);
    store_u16(exact, 2, static_cast<std::uint16_t>(index),
              document.header.byte_swapped);
    exact[4] = pending_type;
  }
  enqueue_original_pending_tenant(
      document, static_cast<std::uint8_t>(floor), *key);

  document.header.balance = wrapping_subtract(document.header.balance, cost);
  document.header.construction_costs =
      wrapping_subtract(document.header.construction_costs, cost);
  return {OriginalConstructionStatus::ok, static_cast<std::int32_t>(cost)};
}

OriginalConstructionResult build_original_hotel_room(
    OriginalTdtDocument& document,
    std::uint8_t type,
    std::int16_t floor,
    std::uint16_t left,
    std::uint8_t variant,
    const OriginalYenTable& construction_costs) {
  if (type < 3U || type > 5U) {
    return {OriginalConstructionStatus::invalid_span, 0};
  }
  return build_original_deferred_facility(
      document, type, floor, left, variant, construction_costs);
}

OriginalConstructionResult build_original_condo(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    std::uint8_t variant,
    const OriginalYenTable& construction_costs) {
  return build_original_deferred_facility(
      document, 9U, floor, left, variant, construction_costs);
}

OriginalConstructionResult build_original_retail_shop(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs) {
  // 11f8:0c92-0cd4 rejects exactly 0x0200 members of the shared
  // Restaurant/Retail/Fast Food pool before entering the generic facility
  // constructor. Counts above the sentinel retain the original's permissive
  // equality-test behavior.
  if (load_original_header_word(document, 46U) == 0x0200U) {
    return {OriginalConstructionStatus::tenant_limit, 0, 30U};
  }
  const auto result = build_original_deferred_facility(
      document, 10U, floor, left, 0U, construction_costs);
  if (result.succeeded()) {
    // 11f8:07d8 increments the shared Restaurant/Retail/Fast Food count
    // immediately after the type-specific constructor returns success.
    store_original_header_word(
        document, 46U,
        static_cast<std::uint16_t>(
            load_original_header_word(document, 46U) + 1U));
  }
  return result;
}

OriginalConstructionResult build_original_restaurant(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs) {
  if (load_original_header_word(document, 46U) == 0x0200U) {
    return {OriginalConstructionStatus::tenant_limit, 0, 30U};
  }
  const auto result = build_original_deferred_facility(
      document, 6U, floor, left, 0U, construction_costs);
  if (result.succeeded()) {
    store_original_header_word(
        document, 46U,
        static_cast<std::uint16_t>(
            load_original_header_word(document, 46U) + 1U));
  }
  return result;
}

OriginalConstructionResult build_original_fast_food(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs) {
  if (load_original_header_word(document, 46U) == 0x0200U) {
    return {OriginalConstructionStatus::tenant_limit, 0, 30U};
  }
  const auto result = build_original_deferred_facility(
      document, 12U, floor, left, 0U, construction_costs);
  if (result.succeeded()) {
    store_original_header_word(
        document, 46U,
        static_cast<std::uint16_t>(
            load_original_header_word(document, 46U) + 1U));
  }
  return result;
}

OriginalConstructionResult build_original_security(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs) {
  // 11f8:0c0d-0c43 performs the same exact-sentinel gate for ten Security
  // Offices before attempting placement.
  if (load_original_header_word(document, 48U) == 10U) {
    return {OriginalConstructionStatus::tenant_limit, 0, 30U};
  }
  const auto result = build_original_deferred_facility(
      document, 14U, floor, left, 0U, construction_costs);
  if (result.succeeded()) {
    store_original_header_word(
        document, 48U,
        static_cast<std::uint16_t>(
            load_original_header_word(document, 48U) + 1U));
  }
  return result;
}

OriginalConstructionResult build_original_housekeeping(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs) {
  return build_original_deferred_facility(
      document, 15U, floor, left, 0U, construction_costs);
}

OriginalConstructionResult build_original_secom_center(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs) {
  return build_original_deferred_facility(
      document, 17U, floor, left, 0U, construction_costs);
}

OriginalConstructionResult build_original_movie_theater(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs) {
  // 11f8:0ccb compares DS:b400 to exactly sixteen before calling the paired
  // constructor, then increments it once only when both halves succeed.
  if (load_original_header_word(document, 54U) == 0x10U) {
    return {OriginalConstructionStatus::tenant_limit, 0, 30U};
  }
  if (floor < 0 || floor > 109 || floor == 10 ||
      (floor >= 11 &&
       floor <= static_cast<std::int16_t>(10U +
                                          document.header.lobby_height))) {
    return {OriginalConstructionStatus::invalid_floor, 0,
            original_status_code(
                floor < 0, 20U,
                original_status_code(floor > 109, 5U, 12U))};
  }
  if (left > kOriginalWorldGridWidth - 24U) {
    return {OriginalConstructionStatus::invalid_span, 0, 20U};
  }

  const auto added_cells = [&](std::int16_t target_floor) {
    const auto& old =
        document.floors[static_cast<std::size_t>(target_floor)];
    const auto right = static_cast<std::uint16_t>(left + 24U);
    return old.tenants.empty()
               ? 24U
               : static_cast<std::uint32_t>(
                     old.left_edge - std::min(old.left_edge, left)) +
                     static_cast<std::uint32_t>(
                         std::max(old.right_edge, right) - old.right_edge);
  };
  const auto lower_floor = static_cast<std::int16_t>(floor - 1);
  if (lower_floor < 0) {
    return {OriginalConstructionStatus::invalid_floor, 0, 20U};
  }
  const std::uint64_t cost64 =
      static_cast<std::uint64_t>(construction_costs[18]) +
      static_cast<std::uint64_t>(added_cells(floor) +
                                 added_cells(lower_floor)) *
          construction_costs[0];
  const auto total_cost = static_cast<std::uint32_t>(cost64);
  if (cost64 > 0x7fffffffULL ||
      (total_cost != 0U &&
       document.header.balance < static_cast<std::int32_t>(total_cost))) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(total_cost),
            original_funds_status_code(document.header.balance,
                                       construction_costs[18])};
  }

  auto working = document;
  OriginalConstructionResult first{};
  OriginalConstructionResult second{};
  if (floor >= 11) {
    first = build_original_deferred_facility(
        working, 19U, lower_floor, left, 0U, construction_costs);
    if (first.succeeded()) {
      second = build_original_deferred_facility(
          working, 18U, floor, left, 0U, construction_costs);
    }
  } else {
    first = build_original_deferred_facility(
        working, 18U, floor, left, 0U, construction_costs);
    if (first.succeeded()) {
      second = build_original_deferred_facility(
          working, 19U, lower_floor, left, 0U, construction_costs);
    }
  }
  if (!first.succeeded()) {
    return first;
  }
  if (!second.succeeded()) {
    return second;
  }

  store_original_header_word(
      working, 54U,
      static_cast<std::uint16_t>(load_original_header_word(working, 54U) +
                                 1U));
  const auto actual_cost = static_cast<std::int32_t>(first.cost + second.cost);
  document = std::move(working);
  return {OriginalConstructionStatus::ok, actual_cost};
}

OriginalConstructionResult build_original_recycling_center(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs) {
  // Type 20 maps to 11f8:3007: the selected upper half must be below ground.
  // Floor zero cannot hold a pair because the lower type-21 floor is floor-1.
  if (floor < 1 || floor >= 10) {
    return {OriginalConstructionStatus::invalid_floor, 0,
            original_status_code(
                floor < 1, 20U,
                original_status_code(floor == 10, 12U, 11U))};
  }
  constexpr std::uint16_t kWidth = 25U;
  if (left > kOriginalWorldGridWidth - kWidth) {
    return {OriginalConstructionStatus::invalid_span, 0, 20U};
  }
  const auto right = static_cast<std::uint16_t>(left + kWidth);

  // DS:75aa is zero in the executable's initialized data and has no direct
  // writer. Once persisted DS:b3f4 is nonzero, 1088:02c8 scans floor-2
  // through floor+1 and accepts only a positive type-20/21 record wholly
  // contained by the proposed span. Equal 25-cell widths make this exact
  // x-alignment with an already activated, vertically adjacent center.
  if (load_original_header_word(document, 42U) != 0U) {
    bool connected = false;
    const int first_floor = std::max<int>(floor - 2, 0);
    const int last_floor = std::min<int>(floor + 1, 119);
    for (int candidate_floor = first_floor;
         candidate_floor <= last_floor && !connected; ++candidate_floor) {
      for (const auto& candidate :
           document.floors[static_cast<std::size_t>(candidate_floor)].tenants) {
        if (candidate.left >= left && candidate.right <= right &&
            (candidate.type == 20 || candidate.type == 21)) {
          connected = true;
          break;
        }
      }
    }
    if (!connected) {
      return {OriginalConstructionStatus::invalid_span, 0};
    }
  }

  const auto lower_floor = static_cast<std::int16_t>(floor - 1);
  const auto added_cells = [&](std::int16_t target_floor) {
    const auto& old =
        document.floors[static_cast<std::size_t>(target_floor)];
    return old.tenants.empty()
               ? static_cast<std::uint32_t>(kWidth)
               : static_cast<std::uint32_t>(
                     old.left_edge - std::min(old.left_edge, left)) +
                     static_cast<std::uint32_t>(
                         std::max(old.right_edge, right) - old.right_edge);
  };
  const std::uint64_t cost64 =
      static_cast<std::uint64_t>(construction_costs[20]) +
      static_cast<std::uint64_t>(construction_costs[21]) +
      static_cast<std::uint64_t>(added_cells(floor) +
                                 added_cells(lower_floor)) *
          construction_costs[0];
  const auto total_cost = static_cast<std::uint32_t>(cost64);
  if (cost64 > 0x7fffffffULL ||
      (total_cost != 0U &&
       document.header.balance < static_cast<std::int32_t>(total_cost))) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(total_cost),
            original_funds_status_code(document.header.balance,
                                       construction_costs[20])};
  }

  // For the basement branch at 11f8:2070, 11f8:1fa5 constructs the selected
  // upper type-20 record first and then the lower type-21 record.
  auto working = document;
  const auto upper = build_original_deferred_facility(
      working, 20U, floor, left, 0U, construction_costs);
  if (!upper.succeeded()) {
    return upper;
  }
  const auto lower = build_original_deferred_facility(
      working, 21U, lower_floor, left, 0U, construction_costs);
  if (!lower.succeeded()) {
    return lower;
  }

  store_original_header_word(
      working, 42U,
      static_cast<std::uint16_t>(load_original_header_word(working, 42U) +
                                 1U));
  const auto actual_cost = static_cast<std::int32_t>(lower.cost + upper.cost);
  document = std::move(working);
  return {OriginalConstructionStatus::ok, actual_cost};
}

OriginalConstructionResult build_original_party_hall(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs) {
  // 11f8:0ccb uses the same persisted DS:b400 count as Movie Theater and
  // compares it to exactly sixteen before entering the shared paired
  // constructor. Successful construction increments it once for the pair.
  if (load_original_header_word(document, 54U) == 0x10U) {
    return {OriginalConstructionStatus::tenant_limit, 0, 30U};
  }

  // Type 29 maps to the same 11f8:306b floor-class branch as type 18:
  // basements are legal, as are stories strictly above every Lobby story.
  // Ground floor, floor zero (which has no lower half), and Lobby stories
  // are rejected.
  if (floor < 1 || floor > 109 || floor == 10 ||
      (floor >= 11 &&
       floor <= static_cast<std::int16_t>(10U +
                                          document.header.lobby_height))) {
    return {OriginalConstructionStatus::invalid_floor, 0,
            original_status_code(
                floor < 1, 20U,
                original_status_code(floor > 109, 5U, 12U))};
  }
  constexpr std::uint16_t kWidth = 24U;
  if (left > kOriginalWorldGridWidth - kWidth) {
    return {OriginalConstructionStatus::invalid_span, 0, 20U};
  }

  const auto lower_floor = static_cast<std::int16_t>(floor - 1);
  const auto right = static_cast<std::uint16_t>(left + kWidth);
  const auto added_cells = [&](std::int16_t target_floor) {
    const auto& old =
        document.floors[static_cast<std::size_t>(target_floor)];
    return old.tenants.empty()
               ? static_cast<std::uint32_t>(kWidth)
               : static_cast<std::uint32_t>(
                     old.left_edge - std::min(old.left_edge, left)) +
                     static_cast<std::uint32_t>(
                         std::max(old.right_edge, right) - old.right_edge);
  };
  const std::uint64_t cost64 =
      static_cast<std::uint64_t>(construction_costs[29]) +
      static_cast<std::uint64_t>(construction_costs[30]) +
      static_cast<std::uint64_t>(added_cells(floor) +
                                 added_cells(lower_floor)) *
          construction_costs[0];
  const auto total_cost = static_cast<std::uint32_t>(cost64);
  if (cost64 > 0x7fffffffULL ||
      (total_cost != 0U &&
       document.header.balance < static_cast<std::int32_t>(total_cost))) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(total_cost),
            original_funds_status_code(document.header.balance,
                                       construction_costs[29])};
  }

  // 11f8:1fa5 constructs the lower type-30 half first above ground, but
  // constructs the selected upper type-29 half first in a basement.
  auto working = document;
  OriginalConstructionResult first{};
  OriginalConstructionResult second{};
  if (floor >= 11) {
    first = build_original_deferred_facility(
        working, 30U, lower_floor, left, 0U, construction_costs);
    if (first.succeeded()) {
      second = build_original_deferred_facility(
          working, 29U, floor, left, 0U, construction_costs);
    }
  } else {
    first = build_original_deferred_facility(
        working, 29U, floor, left, 0U, construction_costs);
    if (first.succeeded()) {
      second = build_original_deferred_facility(
          working, 30U, lower_floor, left, 0U, construction_costs);
    }
  }
  if (!first.succeeded()) {
    return first;
  }
  if (!second.succeeded()) {
    return second;
  }

  store_original_header_word(
      working, 54U,
      static_cast<std::uint16_t>(load_original_header_word(working, 54U) +
                                 1U));
  const auto actual_cost = static_cast<std::int32_t>(first.cost + second.cost);
  document = std::move(working);
  return {OriginalConstructionStatus::ok, actual_cost};
}

OriginalConstructionResult build_original_metro_station(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs) {
  // The type-31 dispatch at 11f8:0d04 permits exactly one Metro. DS:b3e8 is
  // initialized to -1 and receives the selected top floor only after the
  // three-part constructor succeeds.
  if (static_cast<std::int16_t>(load_original_header_word(document, 30U)) >=
      0) {
    return {OriginalConstructionStatus::tenant_limit, 0, 17U};
  }
  constexpr std::uint16_t kWidth = 30U;
  if (left > kOriginalWorldGridWidth - kWidth) {
    return {OriginalConstructionStatus::invalid_span, 0, 20U};
  }

  // Type 31's 11f8:3010 branch accepts only runtime floor two. It requires
  // floor zero to contain exactly one ordinary type-zero floor record, then
  // clears that record count before any of the later cost/collision checks.
  // Preserve that observable side effect even when a subsequent check fails.
  if (floor != 2) {
    return {OriginalConstructionStatus::invalid_floor, 0, 15U};
  }
  auto& bottom = document.floors[0];
  if (bottom.tenants.size() != 1U || bottom.tenants[0].type != 0) {
    return {OriginalConstructionStatus::invalid_floor, 0, 15U};
  }
  bottom.tenants.clear();

  const auto right = static_cast<std::uint16_t>(left + kWidth);
  const auto added_cells = [&](std::int16_t target_floor) {
    const auto& old =
        document.floors[static_cast<std::size_t>(target_floor)];
    return old.tenants.empty()
               ? static_cast<std::uint32_t>(kWidth)
               : static_cast<std::uint32_t>(
                     old.left_edge - std::min(old.left_edge, left)) +
                     static_cast<std::uint32_t>(
                         std::max(old.right_edge, right) - old.right_edge);
  };

  // 1178:011d performs two independent balance checks: the type-31 YEN
  // charge and the exposed-floor charge for floors 0..2. It does not compare
  // their sum, so an exactly type-31-sized balance can legitimately finish
  // negative by the newly exposed floor-cell amount.
  const std::uint64_t facility_preview = construction_costs[31];
  const std::uint64_t floor_preview =
      static_cast<std::uint64_t>(added_cells(0) + added_cells(1) +
                                 added_cells(2)) *
      construction_costs[0];
  const std::uint64_t combined_preview = facility_preview + floor_preview;
  if (facility_preview > 0x7fffffffULL || floor_preview > 0x7fffffffULL ||
      (facility_preview != 0U &&
       document.header.balance <
           static_cast<std::int32_t>(facility_preview)) ||
      (floor_preview != 0U &&
       document.header.balance < static_cast<std::int32_t>(floor_preview))) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(combined_preview),
            original_funds_status_code(document.header.balance,
                                       facility_preview)};
  }

  // Below ground, 11f8:20e7 constructs top-to-bottom: type 31 at floor 2,
  // type 32 at floor 1, then type 33 at floor 0. DS:b3a1 chooses persisted
  // frame selector zero before phase four and one afterward. The preflight
  // above is the original funds gate; each internal 17fd call then debits
  // without repeating a combined-balance test.
  const std::uint8_t variant =
      original_day_phase(document.header.frame_time) < 4 ? 0U : 1U;
  auto working = document;
  const auto upper = build_original_deferred_facility(
      working, 31U, 2, left, variant, construction_costs, false);
  if (!upper.succeeded()) {
    return upper;
  }
  const auto middle = build_original_deferred_facility(
      working, 32U, 1, left, variant, construction_costs, false);
  if (!middle.succeeded()) {
    return middle;
  }
  const auto lower = build_original_deferred_facility(
      working, 33U, 0, left, variant, construction_costs, false);
  if (!lower.succeeded()) {
    return lower;
  }

  store_original_header_word(working, 30U, 2U);
  const auto actual_cost = static_cast<std::int32_t>(
      upper.cost + middle.cost + lower.cost);
  document = std::move(working);
  return {OriginalConstructionStatus::ok, actual_cost};
}

OriginalConstructionResult build_original_cathedral(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs) {
  // The type-36 dispatch at 11f8:0d4b permits exactly one Cathedral.
  // DS:b3ec is initialized to -1, receives selected floor 113 after the
  // five-part constructor succeeds, and is replaced with the type-40 lookup
  // key when deferred construction activates its bottom part.
  if (static_cast<std::int16_t>(load_original_header_word(document, 34U)) >=
      0) {
    return {OriginalConstructionStatus::tenant_limit, 0, 19U};
  }
  if (floor != 113) {
    return {OriginalConstructionStatus::invalid_floor, 0, 16U};
  }
  constexpr std::uint16_t kWidth = 28U;
  if (left > kOriginalWorldGridWidth - kWidth) {
    return {OriginalConstructionStatus::invalid_span, 0, 20U};
  }

  const auto right = static_cast<std::uint16_t>(left + kWidth);
  const auto added_cells = [&](std::int16_t target_floor) {
    const auto& old =
        document.floors[static_cast<std::size_t>(target_floor)];
    return old.tenants.empty()
               ? static_cast<std::uint32_t>(kWidth)
               : static_cast<std::uint32_t>(
                     old.left_edge - std::min(old.left_edge, left)) +
                     static_cast<std::uint32_t>(
                         std::max(old.right_edge, right) - old.right_edge);
  };

  // 1178:011d previews the type-36 YEN charge and the exposed floor cells
  // for floors 109..113 independently against the same starting balance.
  // Like Metro Station, their eventual combined debit may therefore leave
  // a negative balance even though both preview gates succeeded.
  const std::uint64_t facility_preview = construction_costs[36];
  std::uint64_t new_floor_cells = 0U;
  for (std::int16_t target_floor = 109; target_floor <= 113;
       ++target_floor) {
    new_floor_cells += added_cells(target_floor);
  }
  const std::uint64_t floor_preview =
      new_floor_cells * construction_costs[0];
  const std::uint64_t combined_preview = facility_preview + floor_preview;
  if (facility_preview > 0x7fffffffULL || floor_preview > 0x7fffffffULL ||
      combined_preview > 0x7fffffffULL ||
      (facility_preview != 0U &&
       document.header.balance <
           static_cast<std::int32_t>(facility_preview)) ||
      (floor_preview != 0U &&
       document.header.balance < static_cast<std::int32_t>(floor_preview))) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(combined_preview),
            original_funds_status_code(document.header.balance,
                                       facility_preview)};
  }

  // 11f8:2291 emits the fixed stack bottom-to-top. DS:b3a1 supplies one
  // persisted frame selector to every part: zero before phase four and one
  // afterward. The exact split preflight above is the only funds gate.
  const std::uint8_t variant =
      original_day_phase(document.header.frame_time) < 4 ? 0U : 1U;
  auto working = document;
  std::int32_t actual_cost = 0;
  for (int type = 40; type >= 36; --type) {
    const auto target_floor = static_cast<std::int16_t>(149 - type);
    const auto part = build_original_deferred_facility(
        working, static_cast<std::uint8_t>(type), target_floor, left, variant,
        construction_costs, false);
    if (!part.succeeded()) {
      return part;
    }
    actual_cost += part.cost;
  }

  store_original_header_word(working, 34U, 113U);
  document = std::move(working);
  return {OriginalConstructionStatus::ok, actual_cost};
}

OriginalConstructionResult build_original_medical_center(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    std::uint8_t variant,
    const OriginalYenTable& construction_costs) {
  // 11f8:0c46-0c8f rejects exactly ten Medical Centers before the
  // constructor and, consequently, before DS:795c's variant advance.
  if (load_original_header_word(document, 52U) == 10U) {
    return {OriginalConstructionStatus::tenant_limit, 0, 30U};
  }
  const auto result = build_original_deferred_facility(
      document, 13U, floor, left, variant, construction_costs);
  if (result.succeeded()) {
    store_original_header_word(
        document, 52U,
        static_cast<std::uint16_t>(
            load_original_header_word(document, 52U) + 1U));
  }
  return result;
}

OriginalConstructionResult build_original_parking_ramp(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs) {
  return build_original_immediate_parking_unit(
      document, 0x2cU, floor, left, construction_costs);
}

OriginalConstructionResult build_original_parking(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    const OriginalYenTable& construction_costs) {
  return build_original_immediate_parking_unit(
      document, 11U, floor, left, construction_costs);
}

OriginalConstructionResult build_original_office(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::uint16_t left,
    std::uint8_t variant,
    const OriginalYenTable& construction_costs) {
  constexpr std::uint16_t kWidth = 9;
  constexpr std::size_t kPeoplePerOffice = 6;
  const std::uint16_t right = static_cast<std::uint16_t>(left + kWidth);

  // 11f8:2f5a routes type 7 to the above-ground-only branch. Its common
  // ground-floor rejection makes floor 10 illegal and the general ceiling
  // rejects floors above 109.
  if (floor < 11 || floor > 109) {
    return {OriginalConstructionStatus::invalid_floor, 0,
            original_status_code(
                floor < 0, 20U,
                original_status_code(
                    floor < 10, 10U,
                    original_status_code(floor == 10, 12U, 5U)))};
  }
  if (variant >= 6U) {
    return {OriginalConstructionStatus::invalid_span, 0};
  }
  if (left > kOriginalWorldGridWidth - kWidth) {
    return {OriginalConstructionStatus::invalid_span, 0, 20U};
  }

  // For an above-ground facility 11f8:2e64 checks the immediately lower
  // floor (the b5f6 pointer expression is b5fa[floor-1]), not the target
  // floor. This is what allows the first Office to create a new story.
  const auto& supporting =
      document.floors[static_cast<std::size_t>(floor - 1)];
  if (supporting.tenants.empty()) {
    return {OriginalConstructionStatus::invalid_span, 0, 3U};
  }
  if (supporting.left_edge > left || supporting.right_edge < right) {
    return {OriginalConstructionStatus::invalid_span, 0, 6U};
  }

  const auto& old_floor = document.floors[static_cast<std::size_t>(floor)];
  const auto key = first_original_floor_key(old_floor);
  if (!key) {
    return {OriginalConstructionStatus::tenant_limit, 0, 9U};
  }
  const auto people_start =
      first_original_people_run(document, kPeoplePerOffice);
  if (!people_start) {
    return {OriginalConstructionStatus::tenant_limit, 0, 9U};
  }
  if (document.post_elevator.b92e_counter > 10U) {
    return {OriginalConstructionStatus::pending_queue_full, 0, 9U};
  }
  if (document.post_elevator.b92e_counter == 10U) {
    // 11f0:004b activates the circular queue head before calculating the new
    // tail slot. Run the translated Office activation on a working copy, so
    // an unsupported loaded family or a subsequently rejected placement
    // remains byte-exact rather than receiving a speculative activation.
    auto working = document;
    if (!activate_original_pending_facility(working)) {
      return {OriginalConstructionStatus::pending_queue_full, 0, 9U};
    }
    const auto result = build_original_office(
        working, floor, left, variant, construction_costs);
    if (result.succeeded()) {
      document = std::move(working);
    }
    return result;
  }

  auto new_tenants = old_floor.tenants;
  auto new_left = old_floor.left_edge;
  auto new_right = old_floor.right_edge;
  auto office = make_original_pending_facility(
      left, right, 7U, variant, *key, *people_start,
      document.header.byte_swapped);
  const auto insertion = insert_original_office_record(
      new_tenants, new_left, new_right, std::move(office),
      document.header.byte_swapped);
  if (insertion != OfficeInsertionResult::inserted) {
    return {insertion == OfficeInsertionResult::tenant_limit
                ? OriginalConstructionStatus::tenant_limit
                : OriginalConstructionStatus::occupied,
            0, 9U};
  }

  const std::uint32_t added_cells = old_floor.tenants.empty()
      ? kWidth
      : static_cast<std::uint32_t>(old_floor.left_edge -
                                   std::min(old_floor.left_edge, left)) +
            static_cast<std::uint32_t>(
                std::max(old_floor.right_edge, right) - old_floor.right_edge);
  // 1178:035b reads YEN/1000[7]. For a floor outside the automatic lobby
  // stories, 1178:0583 charges YEN/1000[0] for each newly covered cell.
  const std::uint64_t cost64 =
      static_cast<std::uint64_t>(construction_costs[7]) +
      static_cast<std::uint64_t>(added_cells) * construction_costs[0];
  const std::uint32_t cost = static_cast<std::uint32_t>(cost64);
  if (cost64 > 0x7fffffffULL ||
      (cost != 0U && document.header.balance < static_cast<std::int32_t>(cost))) {
    return {OriginalConstructionStatus::insufficient_funds,
            static_cast<std::int32_t>(cost),
            original_funds_status_code(document.header.balance,
                                       construction_costs[7])};
  }

  auto& target = document.floors[static_cast<std::size_t>(floor)];
  target.tenants = std::move(new_tenants);
  target.left_edge = new_left;
  target.right_edge = new_right;
  rebuild_original_floor_lookup(target);

  const std::size_t required_end =
      static_cast<std::size_t>(*people_start) + kPeoplePerOffice;
  // 1238:013a grows the zero-initialized pool by 256 records whenever fewer
  // than 256 records would remain after this allocation.
  if (document.people.size() - required_end < 256U) {
    document.people.resize(document.people.size() + 256U);
    document.people_count = static_cast<std::uint32_t>(document.people.size());
  }
  for (std::size_t index = 0; index < kPeoplePerOffice; ++index) {
    auto& exact = document.people[*people_start + index].exact_bytes;
    exact.fill(std::byte{0});
    exact[0] = static_cast<std::byte>(static_cast<std::uint8_t>(floor));
    exact[1] = static_cast<std::byte>(*key);
    store_u16(exact, 2, static_cast<std::uint16_t>(index),
              document.header.byte_swapped);
    exact[4] = std::byte{0xf9};
  }
  enqueue_original_pending_tenant(
      document, static_cast<std::uint8_t>(floor), *key);

  document.header.balance = wrapping_subtract(document.header.balance, cost);
  document.header.construction_costs =
      wrapping_subtract(document.header.construction_costs, cost);
  return {OriginalConstructionStatus::ok, static_cast<std::int32_t>(cost)};
}

OriginalPendingStepStatus step_original_pending_construction(
    OriginalTdtDocument& document) {
  auto& tail = document.post_elevator;
  if (tail.b92e_counter == 0U) {
    return OriginalPendingStepStatus::no_pending;
  }
  if (tail.b92e_counter > 10U) {
    return OriginalPendingStepStatus::malformed_queue;
  }
  const auto first = std::to_integer<std::uint8_t>(tail.b92e[1]);
  if (first >= 10U) {
    return OriginalPendingStepStatus::malformed_queue;
  }

  // Validate the complete queue before mutating it. This keeps a loaded save
  // with an invalid pending-family code byte-exact instead of partially
  // advancing some earlier valid entries.
  for (std::uint8_t index = 0; index < tail.b92e_counter; ++index) {
    const auto slot = static_cast<std::uint8_t>((first + index) % 10U);
    const auto location = original_pending_location(document, slot);
    if (!location) {
      return OriginalPendingStepStatus::malformed_queue;
    }
    if (!original_pending_people_count(
            document.floors[location->floor]
                .tenants[location->tenant_index]
                .type)) {
      return OriginalPendingStepStatus::unsupported_pending_type;
    }
  }

  for (std::uint8_t index = 0; index < tail.b92e_counter; ++index) {
    const auto slot = static_cast<std::uint8_t>((first + index) % 10U);
    const auto location = *original_pending_location(document, slot);
    auto& tenant =
        document.floors[location.floor].tenants[location.tenant_index];
    tenant.subtype = static_cast<std::uint8_t>(tenant.subtype - 1U);
    tenant.exact_bytes[17] = static_cast<std::byte>(tenant.subtype);
    tenant.exact_bytes[13] = std::byte{1};
    tenant.preserved_07_to_0f[6] = std::byte{1};
    if (tenant.subtype == 0U) {
      return activate_original_pending_facility(document)
                 ? OriginalPendingStepStatus::activated
                 : OriginalPendingStepStatus::malformed_queue;
    }
  }
  return OriginalPendingStepStatus::advanced;
}

void refresh_original_parking_for_day(OriginalTdtDocument& document) {
  rebuild_original_parking_index(document);
}

void rebuild_original_parking_after_facility_change(
    OriginalTdtDocument& document) {
  rebuild_original_parking_connectivity(document);
}

void refresh_original_medical_for_day(OriginalTdtDocument& document) {
  // 1170:011f begins with 11f0:0016 before rebuilding every derived table.
  activate_all_original_pending_facilities(document);

  auto& tail = document.post_elevator;
  // Exact 1170:0663 clears BD5A and all ten BD5C words. The subsequent scan
  // is the 1170:008a rebuild root and uses 1170:0681 for each live append.
  tail.bd5a_count = 0U;
  tail.bd5c_entries.fill(0U);
  // Exact 1170:06a4 seven-by-0x16 medical route-bank reset.
  document.medical_route_index.fill(std::byte{0});

  auto medical_count = load_original_header_word(document, 52U);  // DS:b3fe
  for (std::size_t index = 0; index < tail.dbfc_dwords.size(); ++index) {
    auto& record = tail.dbfc_dwords[index];
    const auto floor = static_cast<std::int8_t>(record & 0xffU);
    if (floor < 0) {
      continue;
    }
    const auto key = static_cast<std::int8_t>((record >> 8U) & 0xffU);
    if (key == -1) {
      // loc_016e invalidates only the floor byte and decrements the persisted
      // count with ordinary 16-bit wraparound.
      record = (record & 0xffffff00U) | 0xffU;
      medical_count = static_cast<std::uint16_t>(medical_count - 1U);
      continue;
    }

    // Byte 2 is the per-service timer used by 1170:0291.
    record &= 0xff00ffffU;
    if (tail.bd5a_count < tail.bd5c_entries.size()) {
      tail.bd5c_entries[tail.bd5a_count] =
          static_cast<std::uint16_t>(index);
      ++tail.bd5a_count;
    }

    // 11a8:166b uses signed IDIV of (floor-5)/15 and accepts remainders <=9.
    // C++ integer division has the same truncation-toward-zero behavior.
    const int shifted_floor = static_cast<int>(floor) - 5;
    const int group = shifted_floor / 15;
    const int remainder = shifted_floor % 15;
    if (group >= 0 && group < 7 && remainder <= 9) {
      constexpr std::size_t kGroupSize = 0x16U;
      const std::size_t base = static_cast<std::size_t>(group) * kGroupSize;
      auto route = std::span<std::byte>(document.medical_route_index);
      const auto count = load_u16(route, base, document.header.byte_swapped);
      store_u16(route, base + 2U + count * 2U,
                static_cast<std::uint16_t>(index),
                document.header.byte_swapped);
      store_u16(route, base, static_cast<std::uint16_t>(count + 1U),
                document.header.byte_swapped);
    }
  }
  store_original_header_word(document, 52U, medical_count);
  if (document.header.rating >= 3U) {
    tail.b92d = 1U;
  }
}

std::size_t reset_original_cathedral_for_day(
    OriginalTdtDocument& document) {
  // 1040:000f returns before touching the queue when Cathedral does not
  // exist. While construction is pending b3ec holds floor 113, so the
  // original first calls 11f0:0016 to complete the captured queue.
  if (static_cast<std::int16_t>(load_original_header_word(document, 34U)) <
      0) {
    return 0U;
  }
  activate_all_original_pending_facilities(document);

  std::size_t reset = 0U;
  for (std::size_t floor_number = 109U; floor_number < 120U;
       ++floor_number) {
    for (const auto& tenant : document.floors[floor_number].tenants) {
      if (tenant.type < 36 || tenant.type > 40) {
        continue;
      }
      const auto people_start = load_u32(
          std::span<const std::byte>(tenant.exact_bytes), 8U,
          document.header.byte_swapped);
      if (people_start > document.people.size() ||
          document.people.size() - people_start < 8U) {
        continue;
      }
      for (std::size_t ordinal = 0; ordinal < 8U; ++ordinal) {
        document.people[people_start + ordinal].exact_bytes[5] =
            std::byte{0x20};
        ++reset;
      }
    }
  }
  return reset;
}

std::size_t close_original_cathedral_for_day(
    OriginalTdtDocument& document) {
  if (static_cast<std::int16_t>(load_original_header_word(document, 34U)) <
      0) {
    return 0U;
  }

  std::size_t transitioned = 0U;
  for (std::size_t floor_number = 109U; floor_number < 120U;
       ++floor_number) {
    for (auto& tenant : document.floors[floor_number].tenants) {
      if (tenant.type < 36 || tenant.type > 40) {
        continue;
      }
      // 1040:01cc clears the word at tenant+0x0c, which is also the exact
      // frame selector consumed by 1038:0946, then marks the record dirty.
      tenant.variant = 0U;
      tenant.exact_bytes[6] = std::byte{0};
      tenant.exact_bytes[7] = std::byte{0};
      tenant.preserved_07_to_0f[0] = std::byte{0};
      tenant.exact_bytes[13] = std::byte{1};
      tenant.preserved_07_to_0f[6] = std::byte{1};

      const auto people_start = load_u32(
          std::span<const std::byte>(tenant.exact_bytes), 8U,
          document.header.byte_swapped);
      if (people_start > document.people.size() ||
          document.people.size() - people_start < 8U) {
        continue;
      }
      for (std::size_t ordinal = 0; ordinal < 8U; ++ordinal) {
        auto& state = document.people[people_start + ordinal].exact_bytes[5];
        if (state == std::byte{3}) {
          state = std::byte{5};
          ++transitioned;
        }
      }
    }
  }

  // The ceremony path adds bit 2 to DS:b406. 1040:0296 removes exactly
  // four when that bit is set, preserving every other header flag.
  const auto flags = load_original_header_word(document, 60U);
  if ((flags & 4U) != 0U) {
    store_original_header_word(
        document, 60U, static_cast<std::uint16_t>(flags - 4U));
  }
  return transitioned;
}

}  // namespace simtower
