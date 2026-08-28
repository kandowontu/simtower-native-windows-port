#include "original_information.hpp"

#include "original_construction.hpp"
#include "original_dialog.hpp"
#include "original_dtmp.hpp"
#include "original_find.hpp"
#include "original_people.hpp"
#include "original_simulation.hpp"
#include "original_tables.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace simtower {
namespace {

std::int16_t signed_byte(std::byte value) noexcept {
  return std::bit_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(value));
}

std::uint16_t load_u16(std::span<const std::byte> bytes,
                       std::size_t offset,
                       bool byte_swapped) noexcept {
  if (offset + 2U > bytes.size()) return 0U;
  const auto first = std::to_integer<std::uint8_t>(bytes[offset]);
  const auto second = std::to_integer<std::uint8_t>(bytes[offset + 1U]);
  return byte_swapped
      ? static_cast<std::uint16_t>((first << 8U) | second)
      : static_cast<std::uint16_t>(first | (second << 8U));
}

std::uint32_t load_u32(std::span<const std::byte> bytes,
                       std::size_t offset,
                       bool byte_swapped) noexcept {
  if (offset + 4U > bytes.size()) return 0U;
  const auto first = load_u16(bytes, offset, byte_swapped);
  const auto second = load_u16(bytes, offset + 2U, byte_swapped);
  return byte_swapped
      ? (static_cast<std::uint32_t>(first) << 16U) | second
      : first | (static_cast<std::uint32_t>(second) << 16U);
}

void store_u16(std::span<std::byte> bytes,
               std::size_t offset,
               std::uint16_t value,
               bool byte_swapped) noexcept {
  if (offset + 2U > bytes.size()) return;
  if (byte_swapped) {
    bytes[offset] = static_cast<std::byte>(value >> 8U);
    bytes[offset + 1U] = static_cast<std::byte>(value);
  } else {
    bytes[offset] = static_cast<std::byte>(value);
    bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
  }
}

std::int32_t wrapping_subtract(std::int32_t value,
                               std::int32_t delta) noexcept {
  return std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(value) -
      static_cast<std::uint32_t>(delta));
}

std::int16_t original_person_portrait_frame(
    const OriginalTdtDocument& document,
    const OriginalTdtPersonRecord& record) noexcept {
  // Literal 1100:3856 portrait-frame selector, including the type-specific
  // states and the default low-three-bit animation mapping.
  const auto& person = record.exact_bytes;
  const auto type = signed_byte(person[4]);
  const auto word_2 = load_u16(person, 2U, document.header.byte_swapped);
  const auto signed_word_2 = std::bit_cast<std::int16_t>(word_2);
  switch (type) {
    case 3:
    case 4:
    case 5:
      return word_2 == 2U ? 4 : 0;
    case 7:
      if (word_2 == 4U) return 2;
      if (word_2 == 5U) return 4;
      // 1100:38d8-38df uses signed JG after the exact 4/5 checks.
      return signed_word_2 <= 1 ? 1 : 0;
    case 9:
      if (word_2 == 1U) return 8;
      if (word_2 == 2U) return 3;
      return word_2 == 0U ? 0 : -1;
    case 14:
      return 5;
    case 15:
      return 10;
    default:
      switch (word_2 & 7U) {
        case 1U:
          return 2;
        case 3U:
          return 4;
        case 5U:
          return 6;
        case 7U:
          return 8;
        default:
          return 0;
      }
  }
}

std::string original_floor_suffix(const OriginalResources& resources,
                                  std::int16_t floor) {
  // Literal 1100:27a7 label renderer: STRL/712 item 1 plus a one-based
  // above-ground number, or item 2 plus the basement depth.
  std::string text = original_strl_entry(resources.find("STRL", 712), 1U);
  if (floor >= 10) {
    text += std::to_string(static_cast<std::int16_t>(floor - 9));
  } else {
    text += original_strl_entry(resources.find("STRL", 712), 2U);
    text += std::to_string(static_cast<std::int16_t>(10 - floor));
  }
  return text;
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

std::uint16_t original_header_word(
    const OriginalTdtDocument& document,
    std::size_t version_20_offset) noexcept {
  const auto offset = original_header_runtime_offset(
      document, version_20_offset);
  return load_u16(document.header.exact_bytes, offset,
                  document.header.byte_swapped);
}

std::optional<std::uint16_t> original_tenant_name_link(
    const OriginalTdtDocument& document,
    std::int16_t floor_number,
    std::size_t tenant_index) noexcept {
  if (floor_number < 0 ||
      floor_number >= static_cast<std::int16_t>(document.floors.size())) {
    return std::nullopt;
  }
  const auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  if (tenant_index >= floor.tenants.size()) return std::nullopt;
  const auto& tenant = floor.tenants[tenant_index];
  auto linked_floor = floor_number;
  auto linked_key = signed_byte(tenant.exact_bytes[12]);
  const auto type = signed_byte(tenant.exact_bytes[4]);
  switch (type) {
    case 18:
    case 19:
    case 29:
    case 30:
    case 34:
    case 35: {
      const auto linked = load_u16(tenant.exact_bytes, 6U,
                                   document.header.byte_swapped);
      if (linked >= document.post_elevator.dc24_records.size()) {
        return std::nullopt;
      }
      const auto& record = document.post_elevator.dc24_records[linked];
      linked_floor = signed_byte(record[0]);
      linked_key = signed_byte(record[2]);
      break;
    }
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
      // Exact 1188:0aa0 b25-b28 branch: all Cathedral parts use hard-coded
      // floor 109 and the process/save-backed DS:b3ec word, not the clicked
      // part's byte-12 key.
      linked_floor = 109;
      linked_key = std::bit_cast<std::int16_t>(
          original_header_word(document, 34U));
      break;
    default:
      break;
  }
  if (linked_floor < 0 || linked_key < 0) return std::nullopt;
  return static_cast<std::uint16_t>(linked_floor * 94 + linked_key);
}

std::optional<std::string> original_saved_tenant_name(
    const OriginalTdtDocument& document,
    std::int16_t floor,
    std::size_t tenant_index) {
  const auto link = original_tenant_name_link(document, floor, tenant_index);
  if (!link) return std::nullopt;
  const auto count = std::min<std::size_t>(
      document.header.tenant_link_count, 20U);
  const auto bytes = std::span<const std::byte>(
      document.post_elevator.dce4_or_dd34);
  for (std::size_t index = 0U; index < count; ++index) {
    if (load_u16(bytes, index * 2U, document.header.byte_swapped) != *link) {
      continue;
    }
    if (index >= document.tenant_link_names.size()) return std::string{};
    return original_find_name_text(document.tenant_link_names[index]);
  }
  return std::nullopt;
}

std::string original_service_subtype_text(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    std::int16_t floor_number,
    std::size_t tenant_index) {
  // Exact 1100:50e4 service-name dispatcher: Restaurant/Fast Food/Retail
  // select STRL/714/715/716 and index it with linked retail byte 11 plus one.
  if (floor_number < 0 ||
      floor_number >= static_cast<std::int16_t>(document.floors.size())) {
    return {};
  }
  const auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  if (tenant_index >= floor.tenants.size()) return {};
  const auto& tenant = floor.tenants[tenant_index];
  const auto type = signed_byte(tenant.exact_bytes[4]);
  int resource_id{};
  if (type == 6) resource_id = 714;
  if (type == 12) resource_id = 715;
  if (type == 10) resource_id = 716;
  if (resource_id == 0) return {};
  const auto retail_index = load_u16(tenant.exact_bytes, 6U,
                                     document.header.byte_swapped);
  if (retail_index >= document.retail.size()) return {};
  const auto subtype = std::to_integer<std::uint8_t>(
      document.retail[retail_index].exact_bytes[11]);
  return original_strl_entry(resources.find("STRL", resource_id),
                             static_cast<std::uint16_t>(subtype + 1U));
}

std::string original_facility_text(const OriginalResources& resources,
                                   const OriginalTdtDocument& document,
                                   std::int16_t floor_number,
                                   std::size_t tenant_index) {
  // 1100:22d5 paints this exact service/facility name followed by the shared
  // 1100:27a7 floor suffix; the native dialog consumes the combined string.
  if (floor_number < 0 ||
      floor_number >= static_cast<std::int16_t>(document.floors.size())) {
    return {};
  }
  if (tenant_index >= document.floors[static_cast<std::size_t>(floor_number)]
                          .tenants.size()) {
    return {};
  }
  auto normalized_floor = floor_number;
  auto normalized_type = signed_byte(
      document.floors[static_cast<std::size_t>(floor_number)]
          .tenants[tenant_index].exact_bytes[4]);
  switch (normalized_type) {
    case 18:
    case 20:
    case 29:
    case 34:
      --normalized_floor;
      break;
    case 19:
    case 21:
    case 30:
    case 35:
      --normalized_type;
      break;
    case 31:
    case 32:
    case 33:
      normalized_floor = static_cast<std::int16_t>(
          normalized_floor + normalized_type - 32);
      normalized_type = 31;
      break;
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
      normalized_floor = static_cast<std::int16_t>(
          normalized_floor + normalized_type - 40);
      normalized_type = 36;
      break;
    default:
      break;
  }
  if (normalized_floor < 0 ||
      normalized_floor >= static_cast<std::int16_t>(document.floors.size())) {
    return {};
  }

  std::string name;
  if (const auto saved = original_saved_tenant_name(
          document, normalized_floor, tenant_index)) {
    name = *saved;
  } else if (normalized_type == 6 || normalized_type == 10 ||
             normalized_type == 12) {
    name = original_service_subtype_text(
        resources, document, normalized_floor, tenant_index);
  } else if (normalized_type >= 0) {
    name = original_strl_entry(
        resources.find("STRL", 710),
        static_cast<std::uint16_t>(normalized_type + 1));
  }
  if (name.empty()) return {};
  return name + original_floor_suffix(resources, normalized_floor);
}

std::string original_person_origin_text(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    std::int16_t owner_floor,
    std::size_t owner_tenant_index,
    std::int8_t owner_type) {
  switch (owner_type) {
    case 3:
    case 4:
    case 5:
    case 7:
    case 9:
    case 14:
    case 15:
      return original_facility_text(
          resources, document, owner_floor, owner_tenant_index);
    default:
      return original_strl_entry(resources.find("STRL", 700), 1U);
  }
}

std::int16_t original_person_final_destination(
    const OriginalTdtDocument& document,
    std::size_t person_index) noexcept {
  if (person_index >= document.people_count ||
      person_index >= document.people.size()) {
    return -1;
  }
  const auto current_floor = signed_byte(
      document.people[person_index].exact_bytes[7]);
  const auto selection = select_original_elevator_boarding_destination(
      document, person_index, 0U, current_floor, true);
  return selection.final_destination;
}

std::string original_activity_service_text(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalTdtPersonRecord& record) {
  const auto service_index = signed_byte(record.exact_bytes[6]);
  if (service_index < 0 ||
      service_index >= static_cast<std::int16_t>(document.retail.size())) {
    return {};
  }
  const auto& service = document.retail[static_cast<std::size_t>(service_index)]
                            .exact_bytes;
  const auto floor_number = signed_byte(service[0]);
  const auto key = signed_byte(service[1]);
  if (floor_number < 0 ||
      floor_number >= static_cast<std::int16_t>(document.floors.size()) ||
      key < 0 || key >= static_cast<std::int16_t>(
                            OriginalTdtFloor::kIndexCapacity)) {
    return {};
  }
  const auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  const auto tenant_index = floor.tenant_index[static_cast<std::size_t>(key)];
  return original_service_subtype_text(
      resources, document, floor_number, tenant_index);
}

std::string original_person_activity_text(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    std::size_t person_index) {
  // Exact 1110:0000 state/type dispatcher. Its drawing destination is modeled
  // separately by the Person Information dialog host.
  const auto& record = document.people[person_index];
  const auto& person = record.exact_bytes;
  const auto type = signed_byte(person[4]);
  const auto state = signed_byte(person[5]);
  const auto destination = original_person_final_destination(
      document, person_index);
  const auto facility_name = [&](std::uint16_t entry) {
    return original_strl_entry(resources.find("STRL", 710), entry);
  };
  const auto with_floor = [&](std::string text) {
    if (text.empty() || destination < 0) return text;
    return text + original_floor_suffix(resources, destination);
  };
  const auto lobby_with_suffix = [&](std::uint16_t suffix) {
    return facility_name(25U) +
           original_strl_entry(resources.find("STRL", 701), suffix);
  };

  const bool common =
      type == 3 || type == 4 || type == 5 || type == 6 || type == 7 ||
      type == 9 || type == 10 || type == 12 || type == 18 || type == 29 ||
      type == 33 || type == 36;
  if (common) {
    switch (state) {
      case 0x40:
        return lobby_with_suffix(type == 9 ? 2U : 1U);
      case 0x41:
        if (signed_byte(person[6]) < 0) return lobby_with_suffix(3U);
        return with_floor(original_activity_service_text(
            resources, document, record));
      case 0x42:
        return with_floor(facility_name(14U));
      case 0x45:
        return (destination == 10 ? facility_name(25U)
                                  : facility_name(12U)) +
               original_strl_entry(resources.find("STRL", 701), 4U);
      case 0x60:
      case 0x61:
      case 0x63:
        return type >= 0
            ? with_floor(facility_name(
                  static_cast<std::uint16_t>(type + 1)))
            : std::string{};
      case 0x62:
        if (type == 18) return lobby_with_suffix(4U);
        return type >= 0
            ? with_floor(facility_name(
                  static_cast<std::uint16_t>(type + 1)))
            : std::string{};
      default:
        return {};
    }
  }
  if (type == 15) {
    if (state == 3) {
      return destination < 0 ? std::string{}
                             : original_floor_suffix(resources, destination);
    }
    if (state == 4) return with_floor(facility_name(16U));
  }
  return {};
}

std::pair<std::int16_t, std::int16_t> original_information_thresholds_impl(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept {
  // Exact 1140:019d rating selector: ratings 1/2, 3, and 4+ choose the three
  // consecutive lower/upper PART pairs at head-word indices 5..7 and 8..10.
  const std::size_t band = document.header.rating <= 2U
                               ? 0U
                               : document.header.rating == 3U ? 1U : 2U;
  return {
      std::bit_cast<std::int16_t>(part.words_00_to_40[5U + band]),
      std::bit_cast<std::int16_t>(part.words_00_to_40[8U + band]),
  };
}

OriginalInformationMeter original_information_meter(
    std::int16_t value,
    std::int16_t lower,
    std::int16_t upper,
    bool visible) noexcept {
  OriginalInformationMeter meter{};
  meter.visible = visible;
  meter.value = value;
  meter.lower = lower;
  meter.upper = upper;
  meter.maximum = 300;
  meter.band = value < lower ? 0U : value < upper ? 1U : 2U;
  return meter;
}

std::optional<std::pair<std::int16_t, std::size_t>>
original_tenant_location_from_key(const OriginalTdtDocument& document,
                                  std::int16_t floor_number,
                                  std::int16_t key) noexcept {
  if (floor_number < 0 ||
      floor_number >= static_cast<std::int16_t>(document.floors.size()) ||
      key < 0 || key >= static_cast<std::int16_t>(
                            OriginalTdtFloor::kIndexCapacity)) {
    return std::nullopt;
  }
  const auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  const auto tenant_index = floor.tenant_index[static_cast<std::size_t>(key)];
  if (tenant_index >= floor.tenants.size()) return std::nullopt;
  return std::pair{floor_number, static_cast<std::size_t>(tenant_index)};
}

std::uint8_t original_facility_dialog_group(std::int8_t type) noexcept {
  const auto dialog_id = original_facility_information_dialog_id(type);
  return dialog_id ? static_cast<std::uint8_t>(*dialog_id - 748U) : 0xffU;
}

std::uint16_t original_facility_occupancy_code(
    const OriginalTdtDocument& document,
    const OriginalTdtTenant& tenant) noexcept {
  // Exact 1100:1a5a status dispatcher: tenant types 3/4/5/6/7/9/10 select
  // STRL/712 entries 3..7 from byte 11 thresholds or linked retail byte 2.
  const auto status = signed_byte(tenant.exact_bytes[5]);
  switch (signed_byte(tenant.exact_bytes[4])) {
    case 3:
    case 4:
    case 5:
      return status < 24 ? 3U : status < 40 ? 6U : 7U;
    case 7:
      return status < 16 ? 3U : 4U;
    case 9:
      return status < 24 ? 3U : 5U;
    case 10: {
      const auto index = load_u16(tenant.exact_bytes, 6U,
                                  document.header.byte_swapped);
      return index < document.retail.size() &&
                     signed_byte(document.retail[index].exact_bytes[2]) > -1
                 ? 3U
                 : 4U;
    }
    default:
      return 0U;
  }
}

std::string original_facility_age_text(
    const OriginalResources& resources,
    const OriginalTdtTenant& tenant) {
  // Exact 1100:248d tenure renderer: byte 17 is quarters, 120 selects STRL/
  // 713 item 5, otherwise nonzero whole years precede the one-based quarter.
  const auto age = std::to_integer<std::uint8_t>(tenant.exact_bytes[17]);
  if (age >= 120U) {
    return original_strl_entry(resources.find("STRL", 713), 5U);
  }
  std::string result;
  const auto years = age / 4U;
  if (years != 0U) {
    result = std::to_string(years);
    result += original_strl_entry(resources.find("STRL", 713), 1U);
  }
  result += std::to_string(age % 4U + 1U);
  result += original_strl_entry(resources.find("STRL", 713), 4U);
  return result;
}

}  // namespace

std::pair<std::int16_t, std::int16_t> original_information_thresholds(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept {
  return original_information_thresholds_impl(document, part);
}

std::string original_movie_length_text(
    const OriginalResources& resources,
    const std::array<std::byte, 0x0c>& record) {
  // Exact text assembled and painted by 1100:25d9: four three-quarter age
  // bands, then STRL/713 item 6 once signed program age reaches twelve.
  const auto age = signed_byte(record[9]);
  if (age >= 12) {
    return original_strl_entry(resources.find("STRL", 713), 6U);
  }
  return std::to_string(age / 3 + 1) +
         original_strl_entry(resources.find("STRL", 713), 4U);
}

std::int32_t original_movie_information_income(
    const std::array<std::byte, 0x0c>& record,
    const OriginalPartTable& part) noexcept {
  // 1100:268e calls 1180:0bcb for this PART-threshold income value before
  // appending STRL/713 item 7 and painting it into the Cinema dialog.
  const auto attendance = signed_byte(record[11]);
  const auto signed_part = [&](std::size_t index) {
    return static_cast<std::int32_t>(
        std::bit_cast<std::int16_t>(part.words_52_to_ac[index]));
  };
  if (attendance < signed_part(16U)) return signed_part(19U);
  if (attendance < signed_part(17U)) return signed_part(20U);
  if (attendance < signed_part(18U)) return signed_part(21U);
  return signed_part(22U);
}

namespace {

std::int16_t original_signed_part_head(const OriginalPartTable& part,
                                       std::size_t index) noexcept {
  return std::bit_cast<std::int16_t>(part.words_00_to_40[index]);
}

std::int16_t original_adjusted_facility_threshold(
    std::int16_t threshold,
    const OriginalTdtTenant& tenant) noexcept {
  switch (signed_byte(tenant.exact_bytes[16])) {
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

OriginalInformationMeter original_commercial_information_meter(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    const OriginalTdtTenant& tenant) noexcept {
  // Exact 1100:2031 attendance meter model. The linked record's signed word
  // 16 is compared against Restaurant/Retail/Fast Food PART thresholds; the
  // native painter consumes the returned value, band, and maximum in item 6.
  OriginalInformationMeter result{};
  const auto linked = load_u16(tenant.exact_bytes, 6U,
                               document.header.byte_swapped);
  if (linked >= document.retail.size()) return result;
  result.visible = true;
  result.value = std::bit_cast<std::int16_t>(load_u16(
      document.retail[linked].exact_bytes, 16U,
      document.header.byte_swapped));
  const auto type = signed_byte(tenant.exact_bytes[4]);
  if (type == 12) {
    result.lower = original_signed_part_head(part, 13U);
    result.upper = original_signed_part_head(part, 12U);
    result.maximum = original_signed_part_head(part, 16U);
  } else if (type == 6) {
    result.lower = original_signed_part_head(part, 20U);
    result.upper = original_signed_part_head(part, 19U);
    result.maximum = original_signed_part_head(part, 23U);
  } else if (type == 10) {
    result.lower = original_adjusted_facility_threshold(
        original_signed_part_head(part, 26U), tenant);
    result.upper = original_adjusted_facility_threshold(
        original_signed_part_head(part, 25U), tenant);
    result.maximum = original_signed_part_head(part, 29U);
  } else {
    result.visible = false;
    return result;
  }
  result.band = result.value < result.lower
                    ? 2U
                    : result.value < result.upper ? 1U : 0U;
  return result;
}

std::array<std::string, 4> original_rent_choices_impl(
    std::uint8_t group) {
  // Exact 1100:0644 six-group combo-box string table. The native dialog adds
  // the selected group's four strings to control 13 in the same order.
  static constexpr std::array<std::array<std::string_view, 4>, 6> choices{{
      {"$15000", "$10000", "$5000", "$2000"},
      {"$3000", "$2000", "$1500", "$500"},
      {"$4500", "$3000", "$2000", "$800"},
      {"$9000", "$6000", "$4000", "$1500"},
      {"$200000", "$150000", "$100000", "$40000"},
      {"$20000", "$15000", "$10000", "$4000"},
  }};
  std::array<std::string, 4> result{};
  if (group >= choices.size()) return result;
  for (std::size_t index = 0U; index < result.size(); ++index) {
    result[index] = choices[group][index];
  }
  return result;
}

OriginalFacilityPreview original_facility_preview(
    const OriginalTdtDocument& document,
    std::int16_t floor_number,
    std::size_t tenant_index) noexcept {
  // Exact 1100:465a source tenant/origin selection and 1100:4869
  // source-world RECT calculation, represented as the crop coordinates
  // consumed by the native renderer. The separate 1100:4514 minimum applies
  // to the temporary backing surface, not to this crop rectangle.
  OriginalFacilityPreview preview{};
  if (floor_number < 0 ||
      floor_number >= static_cast<std::int16_t>(document.floors.size())) {
    return preview;
  }
  const auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  if (tenant_index >= floor.tenants.size()) return preview;
  const auto& clicked = floor.tenants[tenant_index];
  const auto type = signed_byte(clicked.exact_bytes[4]);
  const OriginalTdtTenant* source = &clicked;
  auto source_floor = floor_number;

  const auto select_location = [&](std::int16_t linked_floor,
                                   std::int16_t linked_key) {
    const auto location = original_tenant_location_from_key(
        document, linked_floor, linked_key);
    if (!location) return;
    source_floor = location->first;
    source = &document.floors[static_cast<std::size_t>(location->first)]
                  .tenants[location->second];
  };

  if (type == 6 || type == 10 || type == 12) {
    const auto linked = load_u16(clicked.exact_bytes, 6U,
                                 document.header.byte_swapped);
    if (linked < document.retail.size()) {
      select_location(signed_byte(document.retail[linked].exact_bytes[0]),
                      signed_byte(document.retail[linked].exact_bytes[1]));
    }
  }
  if (type == 18 || type == 19 || type == 29 || type == 30 ||
      type == 34 || type == 35) {
    const auto linked = load_u16(clicked.exact_bytes, 6U,
                                 document.header.byte_swapped);
    if (linked < document.post_elevator.dc24_records.size()) {
      const auto& record = document.post_elevator.dc24_records[linked];
      select_location(signed_byte(record[0]), signed_byte(record[2]));
    }
  }

  preview.view_x = static_cast<int>(source->left) * 8;
  preview.width = static_cast<int>(static_cast<std::uint16_t>(
                      source->right - source->left)) * 8;
  int top_floor = source_floor;
  preview.height = 24;
  if (type == 6 || type == 10 || type == 12) {
    // 1100:4a17-4b0c gets the linked source tenant's type and indexes the
    // original type-width table at DS:74ba. It deliberately does not trust
    // the serialized source record's right-minus-left span.
    const auto source_type = static_cast<std::uint8_t>(
        signed_byte(source->exact_bytes[4]));
    const auto width_cells = original_facility_width_cells(source_type);
    preview.width = static_cast<int>(width_cells) * 8;
  } else if (type == 18 || type == 19 || type == 34 || type == 35) {
    preview.width = 248;
    preview.height = 60;
  } else if (type == 29 || type == 30) {
    preview.width = 192;
    preview.height = 60;
  } else if (type == 20 || type == 21) {
    top_floor = floor_number + type - 20;
    preview.height = 60;
  } else if (type >= 31 && type <= 33) {
    // 1100:4b82-4bf3 keeps the universal twelve-pixel top inset and ends at
    // the second floor boundary: 2*36-12 = 60 visible pixels.
    top_floor = floor_number + type - 31;
    preview.height = 60;
  } else if (type >= 36 && type <= 40) {
    // 1100:4bfb-4c72 applies the same inset through the fifth boundary:
    // 5*36-12 = 168 pixels, not a complete 180-pixel five-floor band.
    top_floor = floor_number + type - 36;
    preview.height = 168;
  }
  preview.view_y = (119 - top_floor) * 36 + 12;
  return preview;
}

OriginalFacilityPreviewDestination original_facility_preview_destination_impl(
    const OriginalFacilityPreview& preview,
    int container_left,
    int container_top,
    int container_right,
    int container_bottom) noexcept {
  // 1100:4d39 uses shared 1208:00b5 to carry paired scale/centering values;
  // the native destination rectangle retains those two signed components.
  OriginalFacilityPreviewDestination destination{
      0, 0, preview.width, preview.height};
  const int container_width = container_right - container_left;
  const int container_height = container_bottom - container_top;
  if (!preview.valid() || container_width <= 0 || container_height <= 0) {
    return {container_left, container_top, container_left, container_top};
  }

  std::int32_t width_ratio =
      static_cast<std::int32_t>(container_width) * 100 / preview.width;
  std::int32_t height_ratio =
      static_cast<std::int32_t>(container_height) * 100 / preview.height;
  const auto scale = [&](std::int32_t percentage) {
    destination.right = static_cast<int>(
        static_cast<std::int32_t>(destination.right) * percentage / 100);
    destination.bottom = static_cast<int>(
        static_cast<std::int32_t>(destination.bottom) * percentage / 100);
  };

  if (width_ratio > height_ratio) {
    if (width_ratio < 100) {
      scale(width_ratio);
    } else {
      height_ratio = std::min<std::int32_t>(height_ratio, 200);
      scale(height_ratio);
    }
  } else if (width_ratio < height_ratio) {
    if (height_ratio < 100) {
      scale(height_ratio);
    } else {
      width_ratio = std::min<std::int32_t>(width_ratio, 200);
      scale(width_ratio);
    }
  } else if (width_ratio != 100) {
    destination.right = container_width;
    destination.bottom = container_height;
  }

  // 1100:4f86-4fac uses signed IDIV-by-two semantics; C++ integer division
  // has the same truncation toward zero for a negative crop offset.
  const int offset_x =
      container_left + (container_width - destination.width()) / 2;
  const int offset_y =
      container_top + (container_height - destination.height()) / 2;
  destination.left += offset_x;
  destination.right += offset_x;
  destination.top += offset_y;
  destination.bottom += offset_y;
  return destination;
}

void append_original_facility_advisory(
    OriginalFacilityInformation& information,
    const OriginalResources& resources,
    std::uint16_t entry) {
  if (information.advisory_line_count >=
      information.advisory_lines.size()) {
    return;
  }
  information.advisory_lines[information.advisory_line_count++] =
      original_strl_entry(resources.find("STRL", 711), entry);
}

void append_original_facility_advisory(
    OriginalFacilityInformation& information,
    std::string text) {
  if (information.advisory_line_count >=
      information.advisory_lines.size()) {
    return;
  }
  information.advisory_lines[information.advisory_line_count++] =
      std::move(text);
}

const OriginalTdtTenant* original_tenant_for_key(
    const OriginalTdtDocument& document,
    std::int16_t floor_number,
    std::int16_t key) noexcept {
  const auto location = original_tenant_location_from_key(
      document, floor_number, key);
  if (!location) return nullptr;
  return &document.floors[static_cast<std::size_t>(location->first)]
              .tenants[location->second];
}

bool original_medical_route_available(
    const OriginalTdtDocument& document,
    std::int16_t floor_number) noexcept {
  // 1170:05f0 maps (floor-9)/15, clamps negative quotients to group zero,
  // and 056f falls back to group zero when the selected group is empty.
  int group = (static_cast<int>(floor_number) - 9) / 15;
  if (group < 0) group = 0;
  if (group >= 7) group = 6;
  constexpr std::size_t kGroupBytes = 0x16U;
  const auto route = std::span<const std::byte>(
      document.medical_route_index);
  auto count = load_u16(route,
                        static_cast<std::size_t>(group) * kGroupBytes,
                        document.header.byte_swapped);
  if (count == 0U && group != 0) {
    count = load_u16(route, 0U, document.header.byte_swapped);
  }
  return count != 0U;
}

bool original_lobby_route_band_available(std::int16_t floor_number) noexcept {
  // Literal signed-IDIV acceptance test at 11a8:166b.
  const int shifted = static_cast<int>(floor_number) - 5;
  const int group = shifted / 15;
  const int remainder = shifted % 15;
  return group >= 0 && group < 7 && remainder <= 9;
}

void append_original_transport_advisory(
    OriginalFacilityInformation& information,
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalTdtTenant& tenant) {
  // Exact 1108:014b transport-distance advisory, including the dialog-group
  // exclusion, three-line cap, 80/125-cell thresholds, and transport kind.
  if (information.dialog_group >= 6U && information.dialog_group <= 9U) {
    return;
  }
  // 1108:016e reads floor_base + tenant_index*18 + 6. The first six bytes
  // belong to the floor header, so this is serialized tenant word +0 (left),
  // not the type-specific link/state word at serialized tenant +6.
  const auto person_x = load_u16(
      tenant.exact_bytes, 0U, document.header.byte_swapped);
  const auto selection = select_original_person_transport_for_information(
      document, information.floor, 10, person_x, true);
  if (selection.transport_index < 0) {
    append_original_facility_advisory(information, resources, 1U);
    return;
  }

  std::uint16_t x{};
  bool stair{};
  bool stair_odd{};
  if (selection.transport_index < 0x40) {
    const auto index = static_cast<std::size_t>(selection.transport_index);
    if (index >= document.post_elevator.stairs_bd70.size()) return;
    const auto& route = document.post_elevator.stairs_bd70[index];
    x = route.x;
    stair = true;
    stair_odd = (route.shape & 1U) != 0U;
  } else {
    const auto index = static_cast<std::size_t>(
        selection.transport_index - 0x40);
    if (index >= document.elevators.size()) return;
    x = document.elevators[index].x;
  }
  // 1108:01bc/0239 performs the subtraction and branchless absolute value in
  // a 16-bit AX register, then compares the result with signed JL. Preserve
  // both the wrapping delta and the -32768 fixed point for imported words.
  const auto difference = static_cast<std::uint16_t>(x - person_x);
  const auto signed_difference = std::bit_cast<std::int16_t>(difference);
  const auto absolute_word = signed_difference < 0
      ? static_cast<std::uint16_t>(0U - difference)
      : difference;
  const auto distance = std::bit_cast<std::int16_t>(absolute_word);
  if (distance < 80) return;
  if (!stair) {
    append_original_facility_advisory(
        information, resources, distance >= 125 ? 13U : 12U);
  } else if (stair_odd) {
    append_original_facility_advisory(
        information, resources, distance >= 125 ? 17U : 16U);
  } else {
    append_original_facility_advisory(
        information, resources, distance >= 125 ? 15U : 14U);
  }
}

std::int16_t original_movie_advisory_capacity(
    const std::array<std::byte, 0x0c>& record,
    const OriginalPartTable& part) noexcept {
  const auto age = signed_byte(record[9]);
  const auto band = static_cast<std::int16_t>(age) / 3;
  const std::size_t base =
      std::to_integer<std::uint8_t>(record[7]) < 7U ? 27U : 23U;
  const std::size_t offset = band == 0 ? 0U : band == 1 ? 1U
                                      : band == 2 ? 2U : 3U;
  return std::bit_cast<std::int16_t>(part.words_52_to_ac[base + offset]);
}

void append_original_movie_advisory(
    OriginalFacilityInformation& information,
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    const OriginalTdtTenant& tenant) {
  // Exact 1108:0285 Movie advisory: dialog group 10, shared three-line cap,
  // empty-program item 7, or capacity-class items 4/5/6.
  if (information.dialog_group != 10U ||
      information.advisory_line_count >= 3U) {
    return;
  }
  const auto linked = load_u16(
      tenant.exact_bytes, 6U, document.header.byte_swapped);
  if (linked >= document.post_elevator.dc24_records.size()) return;
  const auto& record = document.post_elevator.dc24_records[linked];
  std::uint16_t entry{};
  if (record[9] == std::byte{0}) {
    entry = 7U;
  } else {
    switch (original_movie_advisory_capacity(record, part)) {
      case 60:
        entry = 4U;
        break;
      case 40:
        entry = 5U;
        break;
      case 20:
        entry = 6U;
        break;
      default:
        return;
    }
  }
  append_original_facility_advisory(information, resources, entry);
}

void append_original_commercial_advisory(
    OriginalFacilityInformation& information,
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    const OriginalTdtTenant& tenant) {
  // Exact 1108:030d commercial advisory selector and its 1108:0565/05a4
  // calendar/version follow-up branches.
  if ((information.dialog_group != 5U &&
       information.dialog_group != 12U) ||
      information.advisory_line_count >= 3U) {
    return;
  }
  const auto linked = load_u16(
      tenant.exact_bytes, 6U, document.header.byte_swapped);
  if (linked >= document.retail.size()) return;
  const auto& record = document.retail[linked].exact_bytes;
  const auto metric = std::bit_cast<std::int16_t>(load_u16(
      record, 16U, document.header.byte_swapped));
  const auto part_word = [&](std::size_t index) {
    return std::bit_cast<std::int16_t>(part.words_00_to_40[index]);
  };

  std::uint16_t entry{};
  if (information.type == 12) {
    entry = metric < part_word(13U) ? 11U
            : metric < part_word(12U) ? 10U
            : metric < part_word(11U) ? 9U : 8U;
  } else if (information.type == 6) {
    entry = metric < part_word(20U) ? 11U
            : metric < part_word(19U) ? 10U
            : metric < part_word(18U) ? 9U : 8U;
  } else if (information.type == 10) {
    if (signed_byte(record[2]) == -1) {
      entry = 33U;
    } else if (metric < original_adjusted_facility_threshold(
                            part_word(26U), tenant)) {
      entry = 11U;
    } else if (metric < original_adjusted_facility_threshold(
                            part_word(25U), tenant)) {
      entry = 10U;
    } else {
      entry = 9U;
    }
  } else {
    return;
  }
  append_original_facility_advisory(information, resources, entry);

  if (entry <= 9U) {
    if (original_calendar_phase(document.header.current_day) == 1U) {
      append_original_facility_advisory(information, resources, 22U);
    }
  } else if (document.header.version_20_word != 0U) {
    append_original_facility_advisory(information, resources, 23U);
  }
}

int original_noise_distance(std::int8_t current_type) noexcept {
  // Exact 1138:0269 distance selector used by both directional scans.
  switch (current_type) {
    case 3:
    case 4:
    case 5:
      return 20;
    case 7:
      return 10;
    case 9:
      return 30;
    default:
      return current_type;
  }
}

std::int8_t original_noise_kind(std::int8_t candidate_type,
                                std::int8_t current_type) noexcept {
  // Exact 1138:01b8 facility-family normalization and compatibility gate.
  switch (candidate_type) {
    case 3:
    case 4:
    case 5:
      return current_type == 9 ? candidate_type : 0;
    case 6:
    case 10:
    case 12:
      return candidate_type;
    case 7:
      return current_type == 7 ? 0 : candidate_type;
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

std::int8_t original_noisy_neighbor(
    const OriginalTdtDocument& document,
    std::int16_t floor_number,
    std::size_t tenant_index,
    std::int8_t current_type) noexcept {
  if (floor_number < 0 ||
      floor_number >= static_cast<std::int16_t>(document.floors.size())) {
    return 0;
  }
  const auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  if (tenant_index >= floor.tenants.size()) return 0;
  const auto& current = floor.tenants[tenant_index];
  const int distance = original_noise_distance(current_type);
  const auto left_boundary = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(current.left - distance));
  // 1138:0000 gates this family to raw types 3/4/5/7/9, then requires both
  // 00a5 and 0128 to find no disqualifying neighbor. Exact 1138:00a5 left
  // scan: the range test is deliberately applied to
  // the current record before stepping left, so the first neighbor beyond
  // the nominal boundary is still examined by the original.
  for (std::size_t candidate = tenant_index;
       candidate != 0U &&
       std::bit_cast<std::int16_t>(floor.tenants[candidate].left) >=
           left_boundary;) {
    --candidate;
    const auto kind = original_noise_kind(
        signed_byte(floor.tenants[candidate].exact_bytes[4]), current_type);
    if (kind != 0) return kind;
  }

  const auto right_boundary = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(current.right + distance));
  // Exact 1138:0128 mirror scan, including the same current-before-step
  // boundary ordering on the right side.
  for (std::size_t candidate = tenant_index;
       candidate + 1U < floor.tenants.size() &&
       std::bit_cast<std::int16_t>(floor.tenants[candidate].right) <=
           right_boundary;) {
    ++candidate;
    const auto kind = original_noise_kind(
        signed_byte(floor.tenants[candidate].exact_bytes[4]), current_type);
    if (kind != 0) return kind;
  }
  return 0;
}

void build_original_facility_advisories(
    OriginalFacilityInformation& information,
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    const OriginalTdtTenant& tenant) {
  // Exact helper order at 1108:0000. Every append observes the shared
  // three-line cap at DS:b3a7.
  // 1108:0893 appends item 27 for dialog groups 0/4 while serialized tenant
  // byte +14 is zero.
  if ((information.dialog_group == 0U || information.dialog_group == 4U) &&
      tenant.exact_bytes[14] == std::byte{0}) {
    append_original_facility_advisory(information, resources, 27U);
  }
  // 1108:0698 maps runtime tenant bytes +0a/+0b to serialized +4/+5 and
  // appends item 26 for late-state Hotel rooms.
  if (information.type >= 3 && information.type <= 5 &&
      signed_byte(tenant.exact_bytes[5]) >= 0x38) {
    append_original_facility_advisory(information, resources, 26U);
  }
  append_original_transport_advisory(
      information, resources, document, tenant);
  append_original_movie_advisory(
      information, resources, document, part, tenant);
  append_original_commercial_advisory(
      information, resources, document, part, tenant);

  // 1108:0476 gates the Office/Condo medical-route item 18 by rating three.
  if ((information.dialog_group == 0U || information.dialog_group == 4U) &&
      information.advisory_line_count < 3U &&
      document.header.rating >= 3U &&
      !original_medical_route_available(document, information.floor)) {
    append_original_facility_advisory(information, resources, 18U);
  }
  // 1108:04c2 appends item 20 for a Parking tenant in state one.
  if (information.type == 11 && information.advisory_line_count < 3U &&
      signed_byte(tenant.exact_bytes[5]) == 1) {
    append_original_facility_advisory(information, resources, 20U);
  }
  // 1108:0520 appends item 21 when dialog groups 5/12 lie outside the
  // executable's accepted ten-floor lobby-route bands.
  if ((information.dialog_group == 5U || information.dialog_group == 12U) &&
      information.advisory_line_count < 3U &&
      !original_lobby_route_band_available(information.floor)) {
    append_original_facility_advisory(information, resources, 21U);
  }
  if (information.dialog_group == 12U &&
      information.advisory_line_count < 3U) {
    // Exact 1108:05e3 accepts linked service state -1 or 3 and applies the
    // distinct Fast Food/open-service frame-time intervals before item 24.
    const auto linked = load_u16(
        tenant.exact_bytes, 6U, document.header.byte_swapped);
    if (linked < document.retail.size()) {
      const auto linked_status =
          signed_byte(document.retail[linked].exact_bytes[2]);
      if (original_commercial_closed_advisory_required(
              information.type, linked_status,
              document.header.frame_time)) {
        append_original_facility_advisory(information, resources, 24U);
      }
    }
  }
  // 1108:070d follows serialized tenant link +6 into dc24 and appends item
  // 28 when the linked Cinema program state is at least two.
  if ((information.type == 29 || information.type == 30) &&
      information.advisory_line_count < 3U) {
    const auto linked = load_u16(
        tenant.exact_bytes, 6U, document.header.byte_swapped);
    if (linked < document.post_elevator.dc24_records.size() &&
        signed_byte(document.post_elevator.dc24_records[linked][6]) >= 2) {
      append_original_facility_advisory(information, resources, 28U);
    }
  }
  // 1108:0789 appends item 29 for Security facilities in tenant state five.
  if ((information.type == 20 || information.type == 21) &&
      information.advisory_line_count < 3U &&
      signed_byte(tenant.exact_bytes[5]) == 5) {
    append_original_facility_advisory(information, resources, 29U);
  }
  if (information.type >= 31 && information.type <= 33 &&
      information.advisory_line_count < 3U) {
    // Exact 1108:07fe transit-family advisory selection: item 31 before
    // 00f0, item 30 after day phase four, otherwise item 32 for variant two.
    if (document.header.frame_time < 0x00f0U) {
      append_original_facility_advisory(information, resources, 31U);
    } else if (original_day_phase(document.header.frame_time) > 4) {
      append_original_facility_advisory(information, resources, 30U);
    } else if (load_u16(tenant.exact_bytes, 6U,
                        document.header.byte_swapped) == 2U) {
      append_original_facility_advisory(information, resources, 32U);
    }
  }
  if (information.dialog_group <= 4U &&
      information.advisory_line_count < 3U) {
    // Exact 1108:0949 neighbor-noise message: facility name from STRL/710
    // followed by STRL/711 items 35 and 36, consuming one advisory line.
    const auto noise = original_noisy_neighbor(
        document, information.floor, information.tenant_index,
        information.type);
    if (noise != 0) {
      auto text = original_strl_entry(
          resources.find("STRL", 710),
          static_cast<std::uint16_t>(noise + 1));
      text += original_strl_entry(resources.find("STRL", 711), 35U);
      text += original_strl_entry(resources.find("STRL", 711), 36U);
      append_original_facility_advisory(information, std::move(text));
    }
  }
  if (information.type == 11 && information.advisory_line_count < 3U &&
      signed_byte(tenant.exact_bytes[5]) >= 2) {
    // Exact 1108:0a88 Parking linkage advisory. The six-byte cf9c record's
    // dword person link resolves the owner facility appended to item 34.
    const auto linked = load_u16(
        tenant.exact_bytes, 6U, document.header.byte_swapped);
    if (linked < document.post_elevator.cf9c_records.size()) {
      const auto person_index = load_u32(
          document.post_elevator.cf9c_records[linked], 2U,
          document.header.byte_swapped);
      if (person_index < document.people_count &&
          person_index < document.people.size()) {
        const auto& person = document.people[person_index].exact_bytes;
        const auto owner_floor = signed_byte(person[0]);
        const auto owner_key = signed_byte(person[1]);
        const auto location = original_tenant_location_from_key(
            document, owner_floor, owner_key);
        if (location) {
          auto text = original_strl_entry(
              resources.find("STRL", 711), 34U);
          text += original_facility_text(
              resources, document, location->first, location->second);
          append_original_facility_advisory(information, std::move(text));
        }
      }
    }
  }
}

std::optional<std::size_t> next_original_retail_person(
    const OriginalTdtDocument& document,
    std::size_t first,
    std::uint16_t retail_index) noexcept {
  // Exact 1218:08cd global scan used by 1100:2c23. The broad family branch
  // matches state 0x22 directly; types 6/10/12 instead match state five and
  // their owner tenant's linked Retail record.
  const auto limit = std::min<std::size_t>(
      document.people_count, document.people.size());
  for (std::size_t index = first; index < limit; ++index) {
    const auto& person = document.people[index].exact_bytes;
    const auto type = signed_byte(person[4]);
    switch (type) {
      case 3:
      case 4:
      case 5:
      case 7:
      case 9:
      case 18:
      case 29:
      case 33:
      case 36:
        if (signed_byte(person[5]) == 0x22 &&
            static_cast<std::int16_t>(signed_byte(person[6])) ==
                std::bit_cast<std::int16_t>(retail_index)) {
          return index;
        }
        break;
      case 6:
      case 10:
      case 12:
        if (signed_byte(person[5]) == 5) {
          const auto* owner = original_tenant_for_key(
              document, signed_byte(person[0]), signed_byte(person[1]));
          if (owner && load_u16(owner->exact_bytes, 6U,
                                document.header.byte_swapped) ==
                           retail_index) {
            return index;
          }
        }
        break;
      default:
        break;
    }
  }
  return std::nullopt;
}

std::optional<std::size_t> next_original_medical_person(
    const OriginalTdtDocument& document,
    std::size_t first,
    std::uint16_t medical_index) noexcept {
  // Exact 1218:0a89 scan used only by 1100:2d3e: Office/Condo people,
  // state 0x23, and a byte-6 Medical Center link equal to the requested
  // dbfc index. The people-count dword is the not-found sentinel.
  const auto limit = std::min<std::size_t>(
      document.people_count, document.people.size());
  for (std::size_t index = first; index < limit; ++index) {
    const auto& person = document.people[index].exact_bytes;
    const auto type = signed_byte(person[4]);
    if ((type == 7 || type == 9) &&
        signed_byte(person[5]) == 0x23 &&
        std::to_integer<std::uint8_t>(person[6]) ==
            static_cast<std::uint8_t>(medical_index)) {
      return index;
    }
  }
  return std::nullopt;
}

std::size_t original_facility_person_span(std::int8_t type) noexcept {
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

void build_original_facility_person_sprites(
    OriginalFacilityInformation& information,
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalTdtTenant& clicked) {
  // Exact 1100:2852 facility-person presentation dispatcher. Its
  // 1100:2c23/1100:2ec2/1100:307e helpers select BITMAP/700/702/703, frames,
  // and the DTMP
  // item-4/item-9 lineup geometry captured below.
  const auto dtmp = parse_original_dtmp(
      resources.find("DTMP", information.dialog_id));
  const auto rectangle = [&](std::size_t one_based)
      -> const OriginalDtmpRect* {
    if (one_based == 0U || one_based > dtmp.rectangles.size()) return nullptr;
    return &dtmp.rectangles[one_based - 1U];
  };
  const auto* first_rectangle = rectangle(4U);
  if (!first_rectangle || first_rectangle->right <= first_rectangle->left) {
    return;
  }

  int cursor{};
  const OriginalDtmpRect* current_rectangle = first_rectangle;
  bool second_row{};
  const auto reset_cursor = [&](int inset) {
    cursor = static_cast<int>(current_rectangle->left) + inset;
  };
  const auto append_person = [&](std::size_t person_index) {
    if (person_index >= document.people_count ||
        person_index >= document.people.size()) {
      return false;
    }
    const auto frame = original_person_portrait_frame(
        document, document.people[person_index]);
    if (frame < 0) return false;
    std::uint16_t bitmap_id = 700U;
    if (document.post_elevator.b928 != 0U &&
        document.post_elevator.b924 ==
            static_cast<std::int32_t>(person_index)) {
      bitmap_id = 703U;
    } else if (original_person_name_slot(document, person_index)) {
      bitmap_id = 702U;
    }
    // Exact 1100:37ef portrait rectangle: eight by twenty-four, widened by
    // eight more pixels for frames six and above before advancing the cursor.
    const int width = frame >= 6 ? 16 : 8;
    information.person_sprites.push_back({
        person_index, bitmap_id, frame, cursor,
        static_cast<int>(current_rectangle->top) + 5,
        width, 24});
    cursor += width;
    return true;
  };
  const auto advance_overflow_row = [&]() {
    if (static_cast<int>(current_rectangle->right) - 10 > cursor) {
      return true;
    }
    if (second_row) return false;
    const auto* next = rectangle(9U);
    if (!next || next->right <= next->left) return false;
    current_rectangle = next;
    second_row = true;
    reset_cursor(2);
    return true;
  };
  const auto linked_tenant = [&]() -> const OriginalTdtTenant* {
    return original_tenant_for_key(
        document, information.floor, signed_byte(clicked.exact_bytes[12]));
  };
  if (information.dialog_group <= 7U &&
      information.dialog_group != 5U) {
    const auto* owner = linked_tenant();
    if (!owner) return;
    reset_cursor(7);
    switch (information.type) {
      case 7:
        for (std::size_t ordinal = 0U; ordinal < 6U; ++ordinal) {
          const auto index = static_cast<std::uint64_t>(load_u32(
              owner->exact_bytes, 8U, document.header.byte_swapped)) +
              ordinal;
          if (index >= document.people_count || index >= document.people.size())
            break;
          if (signed_byte(document.people[static_cast<std::size_t>(index)]
                              .exact_bytes[5]) <= 0x10) {
            append_person(static_cast<std::size_t>(index));
          }
        }
        return;
      case 3:
      case 4:
      case 5: {
        const std::size_t count =
            signed_byte(owner->exact_bytes[4]) == 3 ? 2U : 3U;
        for (std::size_t ordinal = 1U; ordinal < count; ++ordinal) {
          const auto index = static_cast<std::uint64_t>(load_u32(
              owner->exact_bytes, 8U, document.header.byte_swapped)) +
              ordinal;
          if (index >= document.people_count || index >= document.people.size())
            break;
          if (signed_byte(document.people[static_cast<std::size_t>(index)]
                              .exact_bytes[5]) <= 0x10) {
            append_person(static_cast<std::size_t>(index));
          }
        }
        return;
      }
      case 9:
        for (std::size_t ordinal = 0U; ordinal < 3U; ++ordinal) {
          const auto index = static_cast<std::uint64_t>(load_u32(
              owner->exact_bytes, 8U, document.header.byte_swapped)) +
              ordinal;
          if (index >= document.people_count || index >= document.people.size())
            break;
          if (signed_byte(document.people[static_cast<std::size_t>(index)]
                              .exact_bytes[5]) <= 0x10) {
            append_person(static_cast<std::size_t>(index));
          }
        }
        return;
      case 15:
        for (std::size_t ordinal = 0U; ordinal < 6U; ++ordinal) {
          const auto index = static_cast<std::uint64_t>(load_u32(
              owner->exact_bytes, 8U, document.header.byte_swapped)) +
              ordinal;
          if (index >= document.people_count || index >= document.people.size())
            break;
          const auto& person =
              document.people[static_cast<std::size_t>(index)].exact_bytes;
          const auto state = signed_byte(person[5]);
          if ((state == 0 || state == 1) &&
              (signed_byte(person[7]) < 0 || person[7] == person[0])) {
            append_person(static_cast<std::size_t>(index));
          }
        }
        return;
      case 14:
        for (std::size_t ordinal = 0U; ordinal < 6U; ++ordinal) {
          const auto start = load_u32(
              owner->exact_bytes, 8U, document.header.byte_swapped);
          const auto wide = static_cast<std::uint64_t>(start) + ordinal;
          if (wide >= document.people_count || wide >= document.people.size())
            break;
          append_person(static_cast<std::size_t>(wide));
        }
        return;
      default:
        return;
    }
  }

  if (information.dialog_group == 5U ||
      information.dialog_group == 12U) {
    const auto linked = load_u16(
        clicked.exact_bytes, 6U, document.header.byte_swapped);
    if (linked >= document.retail.size()) return;
    const auto& record = document.retail[linked].exact_bytes;
    const auto state = signed_byte(record[2]);
    if (state == -1 || state == 3) return;
    const auto count = signed_byte(record[9]);
    reset_cursor(2);
    std::size_t candidate{};
    for (std::int16_t ordinal = 0;
         ordinal < count &&
         static_cast<int>(current_rectangle->right) - 10 > cursor;
         ++ordinal) {
      const auto person = next_original_retail_person(
          document, candidate, linked);
      if (!person) break;
      append_person(*person);
      candidate = *person + 1U;
    }
    return;
  }

  if (information.dialog_group == 10U ||
      information.dialog_group == 11U) {
    const auto linked = load_u16(
        clicked.exact_bytes, 6U, document.header.byte_swapped);
    if (linked >= document.post_elevator.dc24_records.size()) return;
    const auto& record = document.post_elevator.dc24_records[linked];
    if (signed_byte(record[6]) < 2) return;
    reset_cursor(2);
    for (std::size_t side = 0U; side < 2U; ++side) {
      const auto location = original_tenant_location_from_key(
          document, signed_byte(record[side]),
          signed_byte(record[2U + side]));
      if (!location) continue;
      const auto& owner =
          document.floors[static_cast<std::size_t>(location->first)]
              .tenants[location->second];
      const auto start = load_u32(
          owner.exact_bytes, 8U, document.header.byte_swapped);
      const auto count = original_facility_person_span(
          signed_byte(owner.exact_bytes[4]));
      for (std::size_t ordinal = 0U; ordinal < count; ++ordinal) {
        const auto wide = static_cast<std::uint64_t>(start) + ordinal;
        if (wide >= document.people_count || wide >= document.people.size())
          break;
        const auto person_index = static_cast<std::size_t>(wide);
        if (signed_byte(document.people[person_index].exact_bytes[5]) == 3) {
          if (append_person(person_index) && !advance_overflow_row()) return;
        }
      }
    }
    return;
  }

  if (information.dialog_group != 9U) return;
  reset_cursor(2);
  if (information.type == 13) {
    // Exact 1100:2d3e Medical Center lineup, including its separate
    // 1218:0a89 state-0x23 scanner and dbfc byte-two display count.
    const auto linked = load_u16(
        clicked.exact_bytes, 6U, document.header.byte_swapped);
    if (linked >= document.post_elevator.dbfc_dwords.size()) return;
    const auto count = std::bit_cast<std::int8_t>(
        static_cast<std::uint8_t>(
            document.post_elevator.dbfc_dwords[linked] >> 16U));
    std::size_t candidate{};
    for (std::int16_t ordinal = 0; ordinal < count; ++ordinal) {
      const auto person = next_original_medical_person(
          document, candidate, linked);
      if (!person) break;
      if (append_person(*person) && !advance_overflow_row()) return;
      candidate = *person + 1U;
    }
    return;
  }

  if (information.type < 36 || information.type > 40) return;
  const auto anchor_key = std::bit_cast<std::int16_t>(
      original_header_word(document, 34U));
  const auto* anchor = original_tenant_for_key(document, 109, anchor_key);
  // 1100:3124-312b compares this persisted word with signed JGE. Preserve
  // the high-bit rejection instead of admitting malformed values as >= 2.
  if (!anchor ||
      std::bit_cast<std::int16_t>(load_u16(
          anchor->exact_bytes, 6U, document.header.byte_swapped)) < 2) {
    return;
  }
  for (std::size_t floor_number = 109U;
       floor_number < document.floors.size() && floor_number <= 119U;
       ++floor_number) {
    for (const auto& owner : document.floors[floor_number].tenants) {
      const auto type = signed_byte(owner.exact_bytes[4]);
      if (type < 36 || type > 40) continue;
      const auto start = load_u32(
          owner.exact_bytes, 8U, document.header.byte_swapped);
      for (std::size_t ordinal = 0U; ordinal < 8U; ++ordinal) {
        const auto wide = static_cast<std::uint64_t>(start) + ordinal;
        if (wide >= document.people_count || wide >= document.people.size())
          break;
        const auto person_index = static_cast<std::size_t>(wide);
        if (signed_byte(document.people[person_index].exact_bytes[5]) == 3) {
          if (append_person(person_index) && !advance_overflow_row()) return;
        }
      }
    }
  }
}

void mark_original_movie_tenants(
    OriginalTdtDocument& document,
    const std::array<std::byte, 0x0c>& record) noexcept {
  const std::size_t count = signed_byte(record[7]) >= 0 ? 2U : 1U;
  for (std::size_t side = 0U; side < 2U; ++side) {
    const auto location = original_tenant_location_from_key(
        document, signed_byte(record[side]), signed_byte(record[2U + side]));
    if (!location) continue;
    auto& floor = document.floors[static_cast<std::size_t>(location->first)];
    for (std::size_t offset = 0U;
         offset < count && location->second + offset < floor.tenants.size();
         ++offset) {
      auto& tenant = floor.tenants[location->second + offset];
      tenant.exact_bytes[13] = std::byte{1};
      tenant.preserved_07_to_0f[6] = std::byte{1};
    }
  }
}

std::string original_default_person_name(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    std::int16_t frame) {
  if (frame < 0) return {};
  const auto entry = static_cast<std::uint16_t>(
      frame + ((frame == 10 &&
                original_day_phase(document.header.frame_time) >= 4)
                   ? 4
                   : 3));
  auto text = original_strl_entry(resources.find("STRL", 700), entry);
  if (text == "Homebody") text = "Mother with Baby";
  return text;
}

std::uint16_t original_person_information_dialog_id(
    const OriginalTdtDocument& document,
    std::size_t person_index) noexcept {
  if (person_index >= document.people.size() ||
      person_index >= document.people_count) {
    return 0U;
  }
  const auto& person = document.people[person_index].exact_bytes;
  const auto floor_number = signed_byte(person[0]);
  const auto key = std::to_integer<std::uint8_t>(person[1]);
  if (floor_number < 0 ||
      floor_number >= static_cast<std::int16_t>(document.floors.size()) ||
      key >= OriginalTdtFloor::kIndexCapacity) {
    return 0U;
  }
  const auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  const auto tenant_index = floor.tenant_index[key];
  if (tenant_index >= floor.tenants.size()) return 0U;
  const auto type = signed_byte(floor.tenants[tenant_index].exact_bytes[4]);
  return type == 14 || type == 15 ? 0x02fcU : 0x02fbU;
}

}  // namespace

std::array<std::string, 4> original_rent_choices(std::uint8_t group) {
  return original_rent_choices_impl(group);
}

bool original_commercial_closed_advisory_required(
    std::int8_t facility_type,
    std::int8_t linked_status,
    std::uint16_t frame_time) noexcept {
  if (linked_status != -1 && linked_status != 3) return false;
  const auto signed_time = std::bit_cast<std::int16_t>(frame_time);
  return facility_type == 6
      ? signed_time > 0x0640 && signed_time < 0x0898
      : signed_time > 0x00f0 && signed_time < 0x07d0;
}

OriginalFacilityPreviewBackingCounts
original_facility_preview_backing_counts(
    const OriginalTdtDocument& document,
    std::int16_t floor_number,
    std::size_t tenant_index,
    std::int16_t current_visible_cells,
    std::int16_t current_visible_floors) noexcept {
  // 1100:4514 mutates DS:777e/7780 only upward, then unconditionally mirrors
  // them into DS:7782/7784. The ordinary horizontal span uses signed JLE;
  // wrapped/malformed right-minus-left values therefore do not expand it.
  auto cells = current_visible_cells;
  auto floors = current_visible_floors;
  if (floor_number >= 0 &&
      floor_number < static_cast<std::int16_t>(document.floors.size())) {
    const auto& floor =
        document.floors[static_cast<std::size_t>(floor_number)];
    if (tenant_index < floor.tenants.size()) {
      const auto& tenant = floor.tenants[tenant_index];
      const auto type = signed_byte(tenant.exact_bytes[4]);
      if (type == 18 || type == 19 || type == 34 || type == 35) {
        if (cells < 31) cells = 31;
      } else {
        const auto span = std::bit_cast<std::int16_t>(
            static_cast<std::uint16_t>(tenant.right - tenant.left));
        if (span > cells) cells = span;
      }

      if ((type >= 18 && type <= 21) || (type >= 29 && type <= 35)) {
        if (floors < 2) floors = 2;
      } else if (type >= 36 && type <= 40) {
        if (floors < 5) floors = 5;
      }
    }
  }
  return {
      cells,
      floors,
      static_cast<std::int16_t>(static_cast<std::uint16_t>(cells) * 2U),
      floors,
  };
}

std::optional<std::size_t> original_information_person_sprite_hit(
    std::span<const OriginalFacilityPersonSprite> sprites,
    int x,
    int y) noexcept {
  // 1100:35b7 builds [x,y,x+width,y+24] and passes it to PTINRECT.
  for (const auto& sprite : sprites) {
    if (x >= sprite.destination_x &&
        x < sprite.destination_x + sprite.width &&
        y >= sprite.destination_y &&
        y < sprite.destination_y + sprite.height) {
      return sprite.person_index;
    }
  }
  return std::nullopt;
}

bool original_information_portrait_panel_hit(
    const OriginalDtmp& dtmp,
    int x,
    int y,
    bool include_item_9) noexcept {
  // 1100:4fba retrieves item 4; 1100:5043 repeats the same PTINRECT test for
  // item 9. Match PTINRECT's excluded right/bottom edges and missing items.
  constexpr std::array<std::size_t, 2> kPortraitItems{3U, 8U};
  const auto item_count = include_item_9 ? kPortraitItems.size() : 1U;
  for (std::size_t ordinal = 0U; ordinal < item_count; ++ordinal) {
    const auto item = kPortraitItems[ordinal];
    if (item >= dtmp.rectangles.size()) continue;
    const auto& rectangle = dtmp.rectangles[item];
    if (x >= rectangle.left && x < rectangle.right &&
        y >= rectangle.top && y < rectangle.bottom) {
      return true;
    }
  }
  return false;
}

OriginalFacilityPreviewDestination original_facility_preview_destination(
    const OriginalFacilityPreview& preview,
    int container_left,
    int container_top,
    int container_right,
    int container_bottom) noexcept {
  return original_facility_preview_destination_impl(
      preview, container_left, container_top, container_right,
      container_bottom);
}

void draw_original_facility_preview(
    HDC destination,
    const OriginalWorldRaster& raster,
    const OriginalFacilityPreview& preview,
    const RECT& container) noexcept {
  if (!destination || !preview.valid()) return;

  // 1100:446d-4493 creates a 0xcccc/0xcccc/0xcccc brush, fills the complete
  // DTMP item-2 rectangle, and destroys the brush before presenting pixels.
  HBRUSH backing = CreateSolidBrush(RGB(0xcc, 0xcc, 0xcc));
  if (backing) {
    FillRect(destination, &container, backing);
    DeleteObject(backing);
  }

  // 1100:03ac rendered the temporary WinG backing once before the modal
  // dialog. 1100:4439 always stretches that retained snapshot on repaint.
  if (raster.width <= 0 || raster.height <= 0 || raster.pixels.empty()) return;
  const auto draw = original_facility_preview_destination(
      preview, container.left, container.top, container.right,
      container.bottom);
  if (draw.width() <= 0 || draw.height() <= 0) return;

  BITMAPINFO bitmap{};
  bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap.bmiHeader.biWidth = raster.width;
  bitmap.bmiHeader.biHeight = -raster.height;
  bitmap.bmiHeader.biPlanes = 1U;
  bitmap.bmiHeader.biBitCount = 32U;
  bitmap.bmiHeader.biCompression = BI_RGB;
  const int previous_mode = SetStretchBltMode(destination, COLORONCOLOR);
  (void)StretchDIBits(
      destination, draw.left, draw.top, draw.width(), draw.height(),
      0, 0, preview.width, preview.height, raster.pixels.data(), &bitmap,
      DIB_RGB_COLORS, SRCCOPY);
  SetStretchBltMode(destination, previous_mode);
}

OriginalMagnifierTarget select_original_magnifier_target(
    const OriginalTdtDocument& document,
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept {
  const auto elevator = original_elevator_hit_from_client(
      document, client_x, client_y, view_x, view_y);
  if (elevator.hit) {
    const auto& shaft = document.elevators[elevator.elevator_index];
    if (elevator.car_index >= 0) {
      OriginalMagnifierTarget target{};
      target.kind = OriginalMagnifierTargetKind::elevator_car_information;
      target.dialog_id = original_elevator_information_dialog_id(shaft.type);
      target.elevator_index = elevator.elevator_index;
      target.elevator_car_index = elevator.car_index;
      target.floor = elevator.floor;
      return target;
    }
    if (elevator.floor < shaft.bottom_floor ||
        elevator.floor > shaft.top_floor || shaft.word_3c != 0U) {
      OriginalMagnifierTarget target{};
      target.kind = OriginalMagnifierTargetKind::elevator_control;
      target.dialog_id = 400U;
      target.elevator_index = elevator.elevator_index;
      target.floor = elevator.floor;
      return target;
    }
    // 10a0:04b2 returns zero for an empty in-span shaft with word_3c zero,
    // allowing the three later Magnifying Glass paths to inspect the click.
  }

  const auto vertical = original_vertical_transport_hit_from_client(
      document, client_x, client_y, view_x, view_y);
  if (vertical.hit) {
    // Exact 10c0:05cd calls 10c0:0606's point hit-test, opens 1100:11da for
    // the returned Stair/Escalator index, and reports the click handled.
    OriginalMagnifierTarget target{};
    target.kind =
        OriginalMagnifierTargetKind::vertical_transport_information;
    target.dialog_id = original_vertical_transport_information_dialog_id();
    target.vertical_transport_index = vertical.transport_index;
    target.floor = vertical.floor;
    return target;
  }

  const auto person = original_elevator_waiting_person_hit_from_client(
      document, client_x, client_y, view_x, view_y);
  if (person.hit) {
    const auto dialog_id =
        original_person_information_dialog_id(document, person.person_index);
    if (dialog_id != 0U) {
      OriginalMagnifierTarget target{};
      target.kind = OriginalMagnifierTargetKind::waiting_person_information;
      target.dialog_id = dialog_id;
      target.elevator_index = person.elevator_index;
      target.person_index = person.person_index;
      target.floor = person.floor;
      return target;
    }
  }

  // Exact final 11f8:0750 Magnifying Glass leg: hit-test the floor tenant and
  // open its 1100:03ac Facility Information dialog when the type has one.
  const auto facility = original_facility_hit_from_client(
      document, client_x, client_y, view_x, view_y);
  if (!facility.hit) return {};
  const auto& tenant = document.floors[static_cast<std::size_t>(facility.floor)]
                           .tenants[facility.tenant_index];
  const auto dialog_id = original_facility_information_dialog_id(
      signed_byte(tenant.exact_bytes[4]));
  if (!dialog_id) return {};

  OriginalMagnifierTarget target{};
  target.kind = OriginalMagnifierTargetKind::facility_information;
  target.dialog_id = *dialog_id;
  target.floor = facility.floor;
  target.tenant_index = facility.tenant_index;
  return target;
}

OriginalTransportInformationText original_transport_information_text(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalMagnifierTarget& target) {
  OriginalTransportInformationText text{};
  // 1100:327f and 1100:3431 share 1100:35b7/364a for their portrait rows.
  // Both begin at DTMP item 4 + (2,5), spill to item 9, and select the same
  // BITMAP/700/702/703 normal/named/VIP sources as Facility Information.
  const auto dtmp = parse_original_dtmp(
      resources.find("DTMP", target.dialog_id));
  const auto append_portraits = [&](std::span<const std::size_t> people) {
    if (dtmp.rectangles.size() < 4U) return;
    const OriginalDtmpRect* rectangle = &dtmp.rectangles[3U];
    bool second_row = false;
    int cursor = static_cast<int>(rectangle->left) + 2;
    const auto ensure_room = [&]() {
      if (cursor < static_cast<int>(rectangle->right) - 2) return true;
      if (second_row || dtmp.rectangles.size() < 9U) return false;
      rectangle = &dtmp.rectangles[8U];
      second_row = true;
      cursor = static_cast<int>(rectangle->left) + 2;
      return cursor < static_cast<int>(rectangle->right) - 2;
    };
    for (const auto person_index : people) {
      if (!ensure_room()) return;
      if (person_index >= document.people_count ||
          person_index >= document.people.size()) {
        continue;
      }
      const auto frame = original_person_portrait_frame(
          document, document.people[person_index]);
      if (frame < 0) continue;
      std::uint16_t bitmap_id = 700U;
      if (document.post_elevator.b928 != 0U &&
          document.post_elevator.b924 ==
              static_cast<std::int32_t>(person_index)) {
        bitmap_id = 703U;
      } else if (original_person_name_slot(document, person_index)) {
        bitmap_id = 702U;
      }
      const int width = frame >= 6 ? 16 : 8;
      text.person_sprites.push_back({
          person_index, bitmap_id, frame, cursor,
          static_cast<int>(rectangle->top) + 5, width, 24});
      cursor += width;
    }
  };

  if (target.kind ==
      OriginalMagnifierTargetKind::elevator_car_information) {
    if (target.elevator_index >= document.elevators.size() ||
        target.elevator_car_index < 0 ||
        static_cast<std::size_t>(target.elevator_car_index) >=
            document.elevators[target.elevator_index].car_records.size()) {
      return text;
    }
    const auto& elevator = document.elevators[target.elevator_index];
    const auto& car = elevator.car_records[
        static_cast<std::size_t>(target.elevator_car_index)].exact_bytes;
    text.primary = original_strl_entry(
        resources.find("STRL", 400),
        static_cast<std::uint16_t>(elevator.type + 1U));
    text.secondary = std::to_string(
        std::to_integer<std::uint8_t>(car[3])) + " / " +
        std::to_string(elevator.capacity);
    text.valid = !text.primary.empty();
    std::vector<std::size_t> passengers;
    const auto capacity = std::min<std::size_t>(elevator.capacity, 42U);
    passengers.reserve(capacity);
    for (std::size_t slot = 0U; slot < capacity; ++slot) {
      if (signed_byte(car[184U + slot]) < 0) continue;
      passengers.push_back(static_cast<std::size_t>(load_u32(
          car, 16U + slot * 4U, document.header.byte_swapped)));
    }
    append_portraits(passengers);
    return text;
  }

  if (target.kind ==
      OriginalMagnifierTargetKind::vertical_transport_information) {
    if (target.vertical_transport_index >=
        document.post_elevator.stairs_bd70.size()) {
      return text;
    }
    const auto& transport = document.post_elevator.stairs_bd70[
        target.vertical_transport_index];
    text.primary = original_strl_entry(
        resources.find("STRL", 400),
        static_cast<std::uint16_t>((transport.shape & 1U) + 4U));
    const auto wrapped = static_cast<std::uint16_t>(
        transport.word_6 + transport.word_8);
    text.secondary = std::to_string(std::bit_cast<std::int16_t>(wrapped));
    text.valid = !text.primary.empty();
    const auto wanted = std::bit_cast<std::int16_t>(wrapped);
    if (wanted > 0) {
      std::vector<std::size_t> passengers;
      passengers.reserve(static_cast<std::size_t>(wanted));
      // Literal 1218:0771 scan used by 1100:3431: the ordinary families must
      // be in state >= 0x40, while Housekeeping (type 15) uses state >= 3.
      for (std::size_t person_index = 0U;
           person_index < document.people_count &&
           person_index < document.people.size() &&
           passengers.size() < static_cast<std::size_t>(wanted);
           ++person_index) {
        const auto& person = document.people[person_index].exact_bytes;
        const auto type = signed_byte(person[4]);
        const auto state = signed_byte(person[5]);
        bool eligible = type == 15 ? state >= 3 : state >= 0x40;
        switch (type) {
          case 3:
          case 4:
          case 5:
          case 6:
          case 7:
          case 9:
          case 10:
          case 12:
          case 15:
          case 18:
          case 29:
          case 33:
          case 36:
            break;
          default:
            eligible = false;
            break;
        }
        if (eligible && signed_byte(person[8]) ==
                            static_cast<std::int16_t>(
                                target.vertical_transport_index)) {
          passengers.push_back(person_index);
        }
      }
      append_portraits(passengers);
    }
  }
  return text;
}

std::optional<std::size_t> original_person_name_slot(
    const OriginalTdtDocument& document,
    std::size_t person_index) noexcept {
  const auto count = std::min<std::size_t>(
      document.header.person_link_count,
      document.post_elevator.dce4_person_indices.size());
  for (std::size_t index = 0U; index < count; ++index) {
    if (document.post_elevator.dce4_person_indices[index] ==
        static_cast<std::int32_t>(person_index)) {
      return index;
    }
  }
  return std::nullopt;
}

std::string original_person_saved_name(const OriginalTdtDocument& document,
                                       std::size_t person_index) {
  const auto slot = original_person_name_slot(document, person_index);
  if (!slot || *slot >= document.person_link_names.size()) return {};
  return original_find_name_text(document.person_link_names[*slot]);
}

OriginalPersonNameResult set_original_person_name(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::string_view name) noexcept {
  if (person_index >= document.people_count ||
      person_index >= document.people.size()) {
    return {};
  }
  if (name.empty()) {
    return {OriginalPersonNameStatus::empty, false};
  }
  if (name.size() > 15U) {
    return {OriginalPersonNameStatus::too_long, false};
  }
  auto slot = original_person_name_slot(document, person_index);
  bool added = false;
  if (!slot) {
    const auto capacity = document.post_elevator.dce4_person_indices.size();
    document.header.person_link_count = static_cast<std::uint16_t>(
        std::min<std::size_t>(document.header.person_link_count, capacity));
    if (document.header.person_link_count == capacity) {
      return {OriginalPersonNameStatus::full, false};
    }
    slot = document.header.person_link_count;
    document.post_elevator.dce4_person_indices[*slot] =
        static_cast<std::int32_t>(person_index);
    ++document.header.person_link_count;
    if (document.person_link_names.size() <= *slot) {
      document.person_link_names.resize(*slot + 1U);
    }
    document.person_link_names[*slot].exact_bytes.fill(std::byte{0});
    added = true;
  } else if (document.person_link_names.size() <= *slot) {
    document.person_link_names.resize(*slot + 1U);
    document.person_link_names[*slot].exact_bytes.fill(std::byte{0});
  }
  auto& bytes = document.person_link_names[*slot].exact_bytes;
  std::memcpy(bytes.data(), name.data(), name.size());
  bytes[name.size()] = std::byte{0};
  return {added ? OriginalPersonNameStatus::added
                : OriginalPersonNameStatus::updated,
          true};
}

OriginalPersonNameResult remove_original_person_name(
    OriginalTdtDocument& document,
    std::size_t person_index) noexcept {
  if (person_index >= document.people_count ||
      person_index >= document.people.size()) {
    return {};
  }
  const auto slot = original_person_name_slot(document, person_index);
  if (!slot) return {OriginalPersonNameStatus::not_named, false};
  const bool removed = remove_original_find_entry(
      document, OriginalFindMode::person, *slot);
  return {removed ? OriginalPersonNameStatus::removed
                  : OriginalPersonNameStatus::not_named,
          removed};
}

OriginalPersonInformation original_person_information(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::size_t person_index,
    OriginalPersonInformationContext context) {
  OriginalPersonInformation information{};
  if (person_index >= document.people_count ||
      person_index >= document.people.size()) {
    return information;
  }
  const auto& record = document.people[person_index];
  const auto& person = record.exact_bytes;
  const auto owner_floor = signed_byte(person[0]);
  const auto owner_key = std::to_integer<std::uint8_t>(person[1]);
  if (owner_floor < 0 ||
      owner_floor >= static_cast<std::int16_t>(document.floors.size()) ||
      owner_key >= OriginalTdtFloor::kIndexCapacity) {
    return information;
  }
  const auto& floor = document.floors[static_cast<std::size_t>(owner_floor)];
  const auto owner_tenant_index = floor.tenant_index[owner_key];
  if (owner_tenant_index >= floor.tenants.size()) return information;
  const auto owner_type = signed_byte(
      floor.tenants[owner_tenant_index].exact_bytes[4]);

  information.valid = true;
  information.person_index = person_index;
  information.dialog_id = owner_type == 14 || owner_type == 15
                              ? 764U
                              : 763U;
  information.owner_floor = owner_floor;
  information.owner_key = owner_key;
  information.owner_tenant_index = owner_tenant_index;
  information.owner_type = static_cast<std::int8_t>(owner_type);
  information.portrait_frame = original_person_portrait_frame(
      document, record);

  const auto saved_slot = original_person_name_slot(document, person_index);
  const bool vip = document.post_elevator.b928 != 0U &&
                   document.post_elevator.b924 ==
                       static_cast<std::int32_t>(person_index);
  information.portrait_variant = vip
      ? OriginalPersonPortraitVariant::vip
      : saved_slot ? OriginalPersonPortraitVariant::named
                   : OriginalPersonPortraitVariant::normal;
  if (saved_slot) {
    information.display_name = original_person_saved_name(
        document, person_index);
  } else if (vip) {
    information.display_name = original_strl_entry(
        resources.find("STRL", 700), 2U);
  } else {
    information.display_name = original_default_person_name(
        resources, document, information.portrait_frame);
  }
  information.origin_text = original_person_origin_text(
      resources, document, owner_floor, owner_tenant_index,
      information.owner_type);
  information.activity_text = original_person_activity_text(
      resources, document, person_index);

  const auto [lower, upper] = original_information_thresholds(document, part);
  const auto sample_count = signed_byte(person[9]);
  std::int16_t evaluation{};
  if (person[9] != std::byte{0}) {
    evaluation = static_cast<std::int16_t>(
        std::bit_cast<std::int16_t>(load_u16(
            person, 14U, document.header.byte_swapped)) /
        sample_count);
  }
  information.evaluation = original_information_meter(
      evaluation, lower, upper, evaluation >= 0);

  if (owner_type != 14 && owner_type != 15) {
    const auto retained = static_cast<std::uint16_t>(
        load_u16(person, 12U, document.header.byte_swapped) & 0x03ffU);
    std::optional<std::uint16_t> stress{};
    switch (context) {
      case OriginalPersonInformationContext::main_world: {
        // 1100:1dca's DS:b3a6==0 leg omits the meter when word 10 is zero.
        // Otherwise it adds the live wrapping elapsed word after 11d8:0423's
        // signed Lobby discount.
        const auto start = load_u16(
            person, 10U, document.header.byte_swapped);
        if (start == 0U) break;
        auto elapsed = static_cast<std::uint16_t>(
            document.header.frame_time - start);
        if (signed_byte(person[7]) == 10 &&
            (document.header.lobby_height == 2U ||
             document.header.lobby_height == 3U)) {
          const auto discount = document.header.lobby_height == 2U
                                    ? std::int16_t{25}
                                    : std::int16_t{50};
          elapsed = std::bit_cast<std::int16_t>(elapsed) > discount
                        ? static_cast<std::uint16_t>(
                              elapsed - static_cast<std::uint16_t>(discount))
                        : 0U;
        }
        stress = static_cast<std::uint16_t>(retained + elapsed);
        break;
      }
      case OriginalPersonInformationContext::transport_dialog:
        // DS:b3a6==1 uses only word 12's retained low ten bits.
        stress = retained;
        break;
      case OriginalPersonInformationContext::facility_dialog:
        // Every other DS:b3a6 value reaches the helper with BX zero.
        stress = 0U;
        break;
    }
    if (stress) {
      information.stress = original_information_meter(
          std::bit_cast<std::int16_t>(*stress), lower, upper, true);
    }
  }
  return information;
}

std::optional<std::size_t> original_tenant_name_slot(
    const OriginalTdtDocument& document,
    std::int16_t floor,
    std::size_t tenant_index) noexcept {
  const auto link = original_tenant_name_link(document, floor, tenant_index);
  if (!link) return std::nullopt;
  const auto count = std::min<std::size_t>(
      document.header.tenant_link_count, 20U);
  const auto links = std::span<const std::byte>(
      document.post_elevator.dce4_or_dd34);
  for (std::size_t index = 0U; index < count; ++index) {
    if (load_u16(links, index * 2U, document.header.byte_swapped) == *link) {
      return index;
    }
  }
  return std::nullopt;
}

std::string original_tenant_saved_name(
    const OriginalTdtDocument& document,
    std::int16_t floor,
    std::size_t tenant_index) {
  const auto slot = original_tenant_name_slot(
      document, floor, tenant_index);
  if (!slot || *slot >= document.tenant_link_names.size()) return {};
  return original_find_name_text(document.tenant_link_names[*slot]);
}

OriginalTenantNameResult set_original_tenant_name(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::size_t tenant_index,
    std::string_view name) noexcept {
  const auto link = original_tenant_name_link(document, floor, tenant_index);
  if (!link) return {};
  if (name.empty()) {
    return {OriginalTenantNameStatus::empty, false};
  }
  if (name.size() > 15U) {
    return {OriginalTenantNameStatus::too_long, false};
  }

  auto slot = original_tenant_name_slot(document, floor, tenant_index);
  bool added = false;
  if (!slot) {
    document.header.tenant_link_count = static_cast<std::uint16_t>(
        std::min<std::size_t>(document.header.tenant_link_count, 20U));
    if (document.header.tenant_link_count == 20U) {
      return {OriginalTenantNameStatus::full, false};
    }
    slot = document.header.tenant_link_count;
    store_u16(document.post_elevator.dce4_or_dd34, *slot * 2U, *link,
              document.header.byte_swapped);
    ++document.header.tenant_link_count;
    if (document.tenant_link_names.size() <= *slot) {
      document.tenant_link_names.resize(*slot + 1U);
    }
    document.tenant_link_names[*slot].exact_bytes.fill(std::byte{0});
    added = true;
  } else if (document.tenant_link_names.size() <= *slot) {
    document.tenant_link_names.resize(*slot + 1U);
    document.tenant_link_names[*slot].exact_bytes.fill(std::byte{0});
  }

  auto& bytes = document.tenant_link_names[*slot].exact_bytes;
  std::memcpy(bytes.data(), name.data(), name.size());
  bytes[name.size()] = std::byte{0};
  return {added ? OriginalTenantNameStatus::added
                : OriginalTenantNameStatus::updated,
          true};
}

OriginalTenantNameResult remove_original_tenant_name(
    OriginalTdtDocument& document,
    std::int16_t floor,
    std::size_t tenant_index) noexcept {
  if (!original_tenant_name_link(document, floor, tenant_index)) return {};
  const auto slot = original_tenant_name_slot(
      document, floor, tenant_index);
  if (!slot) return {OriginalTenantNameStatus::not_named, false};
  const bool removed = remove_original_find_entry(
      document, OriginalFindMode::tenant, *slot);
  return {removed ? OriginalTenantNameStatus::removed
                  : OriginalTenantNameStatus::not_named,
          removed};
}

OriginalFacilityInformation original_facility_information(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::int16_t floor_number,
    std::size_t tenant_index) {
  OriginalFacilityInformation information{};
  if (floor_number < 0 ||
      floor_number >= static_cast<std::int16_t>(document.floors.size())) {
    return information;
  }
  const auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  if (tenant_index >= floor.tenants.size()) return information;
  const auto& tenant = floor.tenants[tenant_index];
  const auto type = static_cast<std::int8_t>(
      signed_byte(tenant.exact_bytes[4]));
  const auto dialog_id = original_facility_information_dialog_id(type);
  if (!dialog_id) return information;

  const auto group = original_facility_dialog_group(type);
  information.valid = true;
  information.dialog_id = *dialog_id;
  information.dialog_group = group;
  information.floor = floor_number;
  information.tenant_index = tenant_index;
  information.type = type;
  information.display_name = original_facility_text(
      resources, document, floor_number, tenant_index);
  information.preview = original_facility_preview(
      document, floor_number, tenant_index);

  const auto linked = load_u16(
      tenant.exact_bytes, 6U, document.header.byte_swapped);
  if ((type == 6 || type == 10 || type == 12) &&
      linked < document.retail.size()) {
    information.linked_record_index = linked;
  } else if ((type == 18 || type == 19 || type == 29 || type == 30 ||
              type == 34 || type == 35) &&
             linked < document.post_elevator.dc24_records.size()) {
    information.linked_record_index = linked;
  }

  if (group <= 5U) {
    information.rent_control_visible = true;
    information.rent_control_enabled =
        group != 4U || signed_byte(tenant.exact_bytes[5]) < 24;
    information.rent_choices = original_rent_choices(group);
    information.selected_rent_rate =
        std::to_integer<std::uint8_t>(tenant.exact_bytes[16]);

    const auto occupancy = original_facility_occupancy_code(
        document, tenant);
    if (occupancy != 0U) {
      information.occupancy_text = original_strl_entry(
          resources.find("STRL", 712), occupancy);
    }
    if ((group == 0U || group == 4U) && occupancy == 3U) {
      information.age_text = original_facility_age_text(resources, tenant);
    }
    if (group != 5U) {
      const auto performance = original_tenant_information_performance(
          document, floor_number, tenant_index);
      const auto [lower, upper] = original_information_thresholds(
          document, part);
      information.evaluation = original_information_meter(
          performance, lower, upper, performance >= 0);
    }
  }

  if (type == 6 || type == 10 || type == 12) {
    information.commercial_meter = original_commercial_information_meter(
        document, part, tenant);
    if (information.commercial_meter.visible) {
      information.commercial_value_text = std::to_string(
          information.commercial_meter.value);
    }
    if (type == 12 && linked < document.retail.size()) {
      // 1100:2715 formats signed Retail byte 10 and appends STRL/713 item 7.
      information.yesterday_profit_text = std::to_string(
          signed_byte(document.retail[linked].exact_bytes[10]));
      information.yesterday_profit_text += original_strl_entry(
          resources.find("STRL", 713), 7U);
    }
  }

  if (group == 10U &&
      linked < document.post_elevator.dc24_records.size()) {
    const auto& record = document.post_elevator.dc24_records[linked];
    const auto movie = signed_byte(record[7]);
    if (movie >= 0) {
      information.movie_title = original_strl_entry(
          resources.find("STRL", 420),
          static_cast<std::uint16_t>(movie + 1));
    }
    information.movie_length_text = original_movie_length_text(
        resources, record);
    information.movie_income_text = std::to_string(
        original_movie_information_income(record, part));
    information.movie_income_text += original_strl_entry(
        resources.find("STRL", 713), 7U);
  }
  build_original_facility_advisories(
      information, resources, document, part, tenant);
  build_original_facility_person_sprites(
      information, resources, document, tenant);
  return information;
}

bool set_original_facility_rent_rate(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::int16_t floor_number,
    std::size_t tenant_index,
    std::uint8_t rent_rate) noexcept {
  if (floor_number < 0 ||
      floor_number >= static_cast<std::int16_t>(document.floors.size()) ||
      rent_rate > 3U) {
    return false;
  }
  auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  if (tenant_index >= floor.tenants.size()) return false;
  auto& tenant = floor.tenants[tenant_index];
  const auto group = original_facility_dialog_group(
      static_cast<std::int8_t>(signed_byte(tenant.exact_bytes[4])));
  if (group > 5U ||
      (group == 4U && signed_byte(tenant.exact_bytes[5]) >= 24)) {
    return false;
  }
  if (std::to_integer<std::uint8_t>(tenant.exact_bytes[16]) == rent_rate) {
    return false;
  }
  tenant.exact_bytes[16] = static_cast<std::byte>(rent_rate);
  tenant.rent_rate = rent_rate;
  refresh_original_tenant_information_satisfaction(
      document, part, floor_number, tenant_index);
  return true;
}

OriginalMovieChoiceResult choose_original_movie(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    std::size_t linked_record_index,
    OriginalMovieChoice choice) noexcept {
  OriginalMovieChoiceResult result{};
  if (choice == OriginalMovieChoice::cancel) {
    result.handled = true;
    result.affordable = true;
    return result;
  }
  if (choice != OriginalMovieChoice::new_release &&
      choice != OriginalMovieChoice::classic) {
    return result;
  }
  result.handled = true;
  if (linked_record_index >= document.post_elevator.dc24_records.size()) {
    return result;
  }
  const auto cost_index = choice == OriginalMovieChoice::new_release
                              ? 34U
                              : 35U;
  result.cost = std::bit_cast<std::int16_t>(
      part.words_52_to_ac[cost_index]);
  result.affordable = document.header.balance >= result.cost;
  if (!result.affordable) return result;

  auto& record = document.post_elevator.dc24_records[linked_record_index];
  const auto incremented = static_cast<std::uint8_t>(
      std::to_integer<std::uint8_t>(record[7]) + 1U);
  const auto signed_incremented = std::bit_cast<std::int8_t>(incremented);
  const auto remainder = static_cast<std::int8_t>(signed_incremented % 7);
  record[7] = static_cast<std::byte>(static_cast<std::uint8_t>(
      remainder + (choice == OriginalMovieChoice::new_release ? 7 : 0)));
  record[9] = std::byte{0};
  document.header.balance = wrapping_subtract(
      document.header.balance, result.cost);
  document.header.construction_costs = wrapping_subtract(
      document.header.construction_costs, result.cost);
  mark_original_movie_tenants(document, record);
  result.changed = true;
  return result;
}

}  // namespace simtower
