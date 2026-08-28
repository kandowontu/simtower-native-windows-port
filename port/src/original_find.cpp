#include "original_find.hpp"

#include "original_construction.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <span>

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
  if (offset + 4U > bytes.size()) {
    return std::numeric_limits<std::uint32_t>::max();
  }
  std::uint32_t value{};
  if (byte_swapped) {
    for (std::size_t index = 0; index < 4U; ++index) {
      value = (value << 8U) |
              std::to_integer<std::uint8_t>(bytes[offset + index]);
    }
  } else {
    for (std::size_t index = 4U; index-- > 0U;) {
      value = (value << 8U) |
              std::to_integer<std::uint8_t>(bytes[offset + index]);
    }
  }
  return value;
}

void store_u16(std::span<std::byte> bytes,
               std::size_t offset,
               std::uint16_t value,
               bool byte_swapped) noexcept {
  if (offset + 2U > bytes.size()) return;
  if (byte_swapped) {
    bytes[offset] = static_cast<std::byte>(value >> 8U);
    bytes[offset + 1U] = static_cast<std::byte>(value & 0xffU);
  } else {
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
  }
}

OriginalFindResolution not_in_tower_alert() noexcept {
  OriginalFindResolution result{};
  result.kind = OriginalFindResolutionKind::not_in_tower_alert;
  return result;
}

OriginalFindResolution lobby_alert(std::int16_t floor) noexcept {
  OriginalFindResolution result{};
  result.kind = OriginalFindResolutionKind::lobby_alert;
  result.floor = floor;
  return result;
}

OriginalFindResolution focus_at(int x,
                                int floor,
                                int client_width,
                                int client_height) noexcept {
  if (x < 0 || floor < 0 || floor >= 120) return {};
  OriginalFindResolution result{};
  result.kind = OriginalFindResolutionKind::focus;
  result.x = static_cast<std::int16_t>(x);
  result.floor = static_cast<std::int16_t>(floor);
  result.view = original_facility_focus_view(
      x, floor, client_width, client_height);
  return result;
}

OriginalFindResolution focus_tenant(const OriginalTdtDocument& document,
                                    int floor_number,
                                    int key,
                                    int client_width,
                                    int client_height) noexcept {
  if (floor_number < 0 || floor_number >= 120 || key < 0 ||
      key >= static_cast<int>(OriginalTdtFloor::kIndexCapacity)) {
    return {};
  }
  const auto& floor = document.floors[static_cast<std::size_t>(floor_number)];
  const auto tenant_index = floor.tenant_index[static_cast<std::size_t>(key)];
  if (tenant_index >= floor.tenants.size()) return {};
  const auto& tenant = floor.tenants[tenant_index];
  const auto type = signed_byte(tenant.exact_bytes[4]);
  if (type < 0) return {};
  const auto width = original_facility_width_cells(
      static_cast<std::uint16_t>(type));
  if (width == 0U) return {};
  // 10e0:078d uses tenant.left plus half of the type table's exact width.
  return focus_at(static_cast<int>(tenant.left) + width / 2U, floor_number,
                  client_width, client_height);
}

const OriginalTdtElevatorFloorRecord* find_floor_record(
    const OriginalTdtElevator& elevator,
    std::int16_t floor) noexcept {
  const auto mapped = original_elevator_floor_record_index(
      elevator.type, elevator.bottom_floor, elevator.top_floor, floor);
  if (mapped < 0) return nullptr;
  const auto found = std::find_if(
      elevator.floor_records.begin(), elevator.floor_records.end(),
      [mapped](const OriginalTdtElevatorFloorRecord& record) {
        return record.mapped_index == mapped;
      });
  return found == elevator.floor_records.end() ? nullptr : &*found;
}

int waiting_person_width(const OriginalTdtDocument& document,
                         std::uint32_t person_index) noexcept {
  // Exact 10a8:1a88 width selector shared with the waiting-person painter.
  // Find must advance by the same one- or two-cell footprint to focus the
  // same ring entry the original draws.
  if (person_index >= document.people_count ||
      person_index >= document.people.size()) {
    return 0;
  }
  const auto& person = document.people[person_index].exact_bytes;
  const auto type = signed_byte(person[4]);
  const auto word_2 = load_u16(person, 2U, document.header.byte_swapped);
  switch (type) {
    case 3:
    case 4:
    case 5:
    case 7:
    case 14:
      return 1;
    case 9:
      return word_2 == 1U ? 2 : 1;
    case 15:
      return 2;
    default:
      return (word_2 & 7U) == 5U || (word_2 & 7U) == 7U ? 2 : 1;
  }
}

struct ShaftAtFloor {
  std::size_t index{};
  int x{};
};

std::vector<ShaftAtFloor> sorted_shafts_at_floor(
    const OriginalTdtDocument& document,
    std::int16_t floor) {
  std::vector<ShaftAtFloor> shafts;
  for (std::size_t index = 0; index < document.elevators.size(); ++index) {
    const auto& elevator = document.elevators[index];
    if (elevator.used == 0U ||
        floor < static_cast<std::int16_t>(elevator.bottom_floor - 1) ||
        floor > static_cast<std::int16_t>(elevator.top_floor + 1)) {
      continue;
    }
    shafts.push_back({index, elevator.x});
  }
  // Exact 10a8:00a8 diminishing-gap Shell sort. Although an individual
  // comparison swaps only strictly descending x values, earlier gap passes
  // can still reverse equal-x shafts indirectly; a stable_sort is therefore
  // not equivalent for 10a8:09e7's queue-position lookup.
  for (int gap = static_cast<int>(shafts.size());;) {
    gap /= 2;
    if (gap <= 0) break;
    for (int end = gap; end < static_cast<int>(shafts.size()); ++end) {
      for (int left = end - gap; left >= 0; left -= gap) {
        const auto right = static_cast<std::size_t>(left + gap);
        if (shafts[static_cast<std::size_t>(left)].x <= shafts[right].x) {
          break;
        }
        std::swap(shafts[static_cast<std::size_t>(left)], shafts[right]);
      }
    }
  }
  return shafts;
}

OriginalFindResolution focus_waiting_person(
    const OriginalTdtDocument& document,
    std::uint32_t person_index,
    std::size_t elevator_index,
    std::int16_t floor,
    bool first_lane,
    int client_width,
    int client_height) noexcept {
  // Exact 10a8:09e7 Find-person waiting-queue position orchestration. The
  // original's 10a8:1dd3 first looks the elevator up in its per-floor visible
  // cache; the native sorted shaft scan derives the same sequence directly,
  // then applies 10a8:1582/165d's left/right queue scans below.
  if (elevator_index >= document.elevators.size() || floor < 0 || floor >= 120) {
    return {};
  }
  const auto shafts = sorted_shafts_at_floor(document, floor);
  const auto shaft = std::find_if(
      shafts.begin(), shafts.end(),
      [elevator_index](const ShaftAtFloor& candidate) {
        return candidate.index == elevator_index;
      });
  if (shaft == shafts.end()) return {};
  const std::size_t sequence =
      static_cast<std::size_t>(shaft - shafts.begin());
  const auto& elevator = document.elevators[elevator_index];
  const auto* record = find_floor_record(elevator, floor);
  if (!record) return {};

  const auto& floor_state = document.floors[static_cast<std::size_t>(floor)];
  int begin{};
  int end{};
  if (first_lane) {
    // Exact 10a8:1582 left-lane boundary and ring-buffer scan geometry.
    begin = floor_state.left_edge;
    if (sequence != 0U && shafts[sequence - 1U].x >= begin) {
      begin = shafts[sequence].x -
              ((shafts[sequence].x - shafts[sequence - 1U].x - 4) >> 1);
    }
    end = shafts[sequence].x - 2;
  } else {
    // Exact 10a8:165d right-lane boundary and ring-buffer scan geometry.
    const int shaft_width = elevator.type == 0U ? 6 : 4;
    begin = shafts[sequence].x + shaft_width + 2;
    end = floor_state.right_edge;
    if (sequence + 1U != shafts.size() && shafts[sequence + 1U].x <= end) {
      end = shafts[sequence + 1U].x -
            ((shafts[sequence + 1U].x - shafts[sequence].x - 4) >> 1);
    }
  }

  const std::size_t count_offset = first_lane ? 0U : 2U;
  const std::size_t cursor_offset = first_lane ? 1U : 3U;
  const std::size_t table_offset = first_lane ? 4U : 164U;
  const auto count = signed_byte(record->exact_bytes[count_offset]);
  const auto ring_cursor =
      std::to_integer<std::uint8_t>(record->exact_bytes[cursor_offset]);
  if (count <= 0 || ring_cursor >= 40U) return {};

  int x = first_lane ? end - 1 : begin;
  for (std::size_t ordinal = 0;
       ordinal < static_cast<std::size_t>(count) &&
       (first_lane ? x >= begin : x < end);
       ++ordinal) {
    const auto slot = (ring_cursor + ordinal) % 40U;
    const auto queued = load_u32(record->exact_bytes,
                                 table_offset + slot * 4U,
                                 document.header.byte_swapped);
    if (queued == person_index) {
      return focus_at(x, floor, client_width, client_height);
    }
    const int width = waiting_person_width(document, queued);
    if (width == 0) return {};
    x += first_lane ? -width : width;
  }
  return {};
}

OriginalFindResolution focus_transit_person(
    const OriginalTdtDocument& document,
    std::uint32_t person_index,
    int client_width,
    int client_height) noexcept {
  // Exact 10e0:0814 dispatcher and 10e0:09ce/0aa0/0ad8 Elevator-car,
  // Stair/Escalator, and two-lane waiting-position helpers.
  const auto& person = document.people[person_index].exact_bytes;
  const auto transit = signed_byte(person[8]);
  const auto fallback_floor = signed_byte(person[7]);
  if (transit < 0) return lobby_alert(fallback_floor);
  if (transit < 0x40) {
    const auto index = static_cast<std::size_t>(transit);
    if (index >= document.post_elevator.stairs_bd70.size()) {
      return lobby_alert(fallback_floor);
    }
    const auto& stair = document.post_elevator.stairs_bd70[index];
    return focus_at(static_cast<int>(stair.x) + 4,
                    static_cast<int>(stair.floor) + 1,
                    client_width, client_height);
  }

  int elevator_number = transit - 0x40;
  bool first_lane = true;
  if (elevator_number >= 24) {
    elevator_number -= 24;
    first_lane = false;
  }
  if (elevator_number < 0 || elevator_number >= 24) {
    return lobby_alert(fallback_floor);
  }
  const auto elevator_index = static_cast<std::size_t>(elevator_number);
  const auto& elevator = document.elevators[elevator_index];
  const auto capacity = std::min<std::size_t>(elevator.capacity, 42U);
  for (const auto& car_record : elevator.car_records) {
    const auto& car = car_record.exact_bytes;
    if (car[15] == std::byte{0}) continue;
    for (std::size_t slot = 0; slot < capacity; ++slot) {
      if (load_u32(car, 16U + slot * 4U,
                   document.header.byte_swapped) != person_index) {
        continue;
      }
      int y = signed_byte(car[0]) * 36;
      const int phase = signed_byte(car[1]) * 6;
      y += signed_byte(car[4]) != 0 ? -phase : phase;
      // x86 IDIV truncates toward zero, matching native signed division.
      const int floor = y / 36;
      const int width = elevator.type == 0U ? 6 : 4;
      return focus_at(static_cast<int>(elevator.x) + width / 2, floor,
                      client_width, client_height);
    }
  }
  const auto waiting = focus_waiting_person(
      document, person_index, elevator_index, signed_byte(person[7]),
      first_lane, client_width, client_height);
  return waiting.focused() ? waiting : lobby_alert(fallback_floor);
}

bool supports_common_person_find(std::int16_t type) noexcept {
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
      return true;
    default:
      return false;
  }
}

}  // namespace

void start_original_find_marker(
    OriginalFindMarkerState& state,
    const OriginalFindResolution& resolution,
    std::uint32_t selected_person,
    std::uint32_t now_tick) noexcept {
  if (!resolution.focused()) {
    reset_original_find_marker(state);
    return;
  }
  // 10e0:078d/09ce/0aa0/0ad8 leave the target in cell/floor coordinates;
  // FINDDIALOGFILTER 10d8:02ce then retains the selected link and tick.
  state.cell_x = resolution.x;
  state.floor = resolution.floor;
  state.selected_person = selected_person;
  state.started_tick = now_tick;
  state.phase = 0U;
}

void reset_original_find_marker(OriginalFindMarkerState& state) noexcept {
  // Literal 10e0:04cf DS:77b4..77c0 reset.
  state = {};
}

bool expire_original_find_marker(OriginalFindMarkerState& state,
                                 std::uint32_t now_tick) noexcept {
  if (!state.active()) return false;
  // 10e0:051d uses the shared 1000:39ea signed-dword magnitude helper after
  // wrapping subtraction, then waits while that magnitude is <= 300.
  const std::uint32_t magnitude =
      original_tick_magnitude_delta(now_tick, state.started_tick);
  if (std::bit_cast<std::int32_t>(magnitude) <= 300) return false;
  reset_original_find_marker(state);
  return true;
}

std::string original_find_name_text(const OriginalTdtLinkName& name) {
  std::string text;
  for (const auto byte : name.exact_bytes) {
    const auto value = std::to_integer<std::uint8_t>(byte);
    if (value == 0U) break;
    text.push_back(static_cast<char>(value));
  }
  return text;
}

std::string original_find_floor_text(std::int16_t floor) {
  // 10e0:06ef-0748: stored floor 10 becomes decimal 1. Lower values load
  // one-based STRL/712 item 2 ("B") and append decimal (10 - floor).
  if (floor >= 10) return std::to_string(static_cast<int>(floor) - 9);
  return "B" + std::to_string(10 - static_cast<int>(floor));
}

std::vector<OriginalFindEntry> original_find_entries(
    const OriginalTdtDocument& document,
    OriginalFindMode mode) {
  std::vector<OriginalFindEntry> entries;
  if (mode == OriginalFindMode::person) {
    const auto count = std::min<std::size_t>(
        document.header.person_link_count,
        document.post_elevator.dce4_person_indices.size());
    entries.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      entries.push_back({
          static_cast<std::uint32_t>(
              document.post_elevator.dce4_person_indices[index]),
          index < document.person_link_names.size()
              ? original_find_name_text(document.person_link_names[index])
              : std::string{},
      });
    }
    return entries;
  }

  const auto count = std::min<std::size_t>(
      document.header.tenant_link_count, 20U);
  entries.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    entries.push_back({
        load_u16(document.post_elevator.dce4_or_dd34, index * 2U,
                 document.header.byte_swapped),
        index < document.tenant_link_names.size()
            ? original_find_name_text(document.tenant_link_names[index])
            : std::string{},
    });
  }
  return entries;
}

bool remove_original_find_entry(OriginalTdtDocument& document,
                                OriginalFindMode mode,
                                std::size_t selected_index) noexcept {
  if (mode == OriginalFindMode::person) {
    auto& count = document.header.person_link_count;
    count = static_cast<std::uint16_t>(std::min<std::size_t>(
        count, document.post_elevator.dce4_person_indices.size()));
    if (selected_index >= count) return false;
    auto& links = document.post_elevator.dce4_person_indices;
    for (std::size_t source = selected_index + 1U; source < count; ++source) {
      links[source - 1U] = links[source];
    }
    --count;
    links[count] = -1;
    if (selected_index < document.person_link_names.size()) {
      document.person_link_names.erase(
          document.person_link_names.begin() +
          static_cast<std::ptrdiff_t>(selected_index));
    }
    return true;
  }

  auto& count = document.header.tenant_link_count;
  count = std::min<std::uint16_t>(count, 20U);
  if (selected_index >= count) return false;
  auto bytes = std::span<std::byte>(document.post_elevator.dce4_or_dd34);
  for (std::size_t source = selected_index + 1U; source < count; ++source) {
    store_u16(bytes, (source - 1U) * 2U,
              load_u16(bytes, source * 2U, document.header.byte_swapped),
              document.header.byte_swapped);
  }
  --count;
  store_u16(bytes, static_cast<std::size_t>(count) * 2U, 0U,
            document.header.byte_swapped);
  if (selected_index < document.tenant_link_names.size()) {
    document.tenant_link_names.erase(
        document.tenant_link_names.begin() +
        static_cast<std::ptrdiff_t>(selected_index));
  }
  return true;
}

OriginalFindResolution resolve_original_find_tenant(
    const OriginalTdtDocument& document,
    std::uint16_t tenant_link,
    int client_width,
    int client_height) noexcept {
  if (std::bit_cast<std::int16_t>(tenant_link) < 0) return {};
  return focus_tenant(document, tenant_link / 94U, tenant_link % 94U,
                      client_width, client_height);
}

OriginalFindResolution resolve_original_find_person(
    const OriginalTdtDocument& document,
    std::uint32_t person_index,
    int client_width,
    int client_height) noexcept {
  if (person_index >= document.people_count ||
      person_index >= document.people.size()) {
    return {};
  }
  const auto& person = document.people[person_index].exact_bytes;
  const auto type = signed_byte(person[4]);
  const auto state = signed_byte(person[5]);
  const auto owner = [&]() {
    return focus_tenant(document, signed_byte(person[0]), signed_byte(person[1]),
                        client_width, client_height);
  };

  if (type == 14) return owner();
  if (type == 15) {
    // 10e0:01f3 is the type-15 specialization: states zero/one can be at
    // their owner or in a Lobby, while state two has one rating/service/guest
    // exception before taking the same Lobby fallback.
    if (state == 0 || state == 1) {
      const auto current_floor = signed_byte(person[7]);
      return current_floor < 0 || current_floor == signed_byte(person[0])
                 ? owner()
                 : lobby_alert(current_floor);
    }
    if (state == 2) {
      const auto current_floor = signed_byte(person[7]);
      if (document.header.rating >= 4U) return lobby_alert(current_floor);
      const auto room_floor = signed_byte(person[6]);
      const auto room_key = std::bit_cast<std::int16_t>(load_u16(
          person, 12U, document.header.byte_swapped));
      if (room_floor >= 0 &&
          room_floor < static_cast<std::int16_t>(document.floors.size()) &&
          room_key >= 0 && room_key < static_cast<std::int16_t>(
                                         OriginalTdtFloor::kIndexCapacity)) {
        const auto& floor =
            document.floors[static_cast<std::size_t>(room_floor)];
        const auto tenant_index =
            floor.tenant_index[static_cast<std::size_t>(room_key)];
        if (tenant_index < floor.tenants.size()) {
          const auto& tenant = floor.tenants[tenant_index].exact_bytes;
          // 10e0:0382/03b2 address the tenant through the floor allocation,
          // whose six-byte header makes runtime +0x0a serialized byte +4.
          // Only Hotel types 3..5 take the exceptional direct-focus path.
          const auto room_type = signed_byte(tenant[4]);
          const auto first_person = load_u32(
              tenant, 8U, document.header.byte_swapped);
          if (room_type >= 3 && room_type <= 5 &&
              first_person < document.people_count &&
              first_person < document.people.size() &&
              signed_byte(document.people[first_person].exact_bytes[5]) == 3) {
            // 10e0:02ff-03f1 -> 1220:6ba9(room, 0) selects serialized tenant
            // dword +8 and focuses the assigned room, not Housekeeping's
            // owner facility, while that room's first guest is in state 3.
            return focus_tenant(document, room_floor, room_key,
                                client_width, client_height);
          }
        }
      }
      return lobby_alert(current_floor);
    }
    return focus_transit_person(document, person_index,
                                client_width, client_height);
  }
  if (!supports_common_person_find(type)) return {};

  if ((state >= 0 && state <= 5) || state == 16) {
    if (type != 18 && type != 29) return owner();
    // 10e0:00c0 -> 10e0:0bc6 redirects Movie/Party people through their
    // shared dc24 destination record.
    const auto owner_floor = signed_byte(person[0]);
    const auto owner_key = signed_byte(person[1]);
    if (owner_floor < 0 || owner_key < 0 || owner_floor >= 120 ||
        owner_key >= static_cast<int>(OriginalTdtFloor::kIndexCapacity)) {
      return {};
    }
    const auto& floor = document.floors[static_cast<std::size_t>(owner_floor)];
    const auto tenant_index = floor.tenant_index[static_cast<std::size_t>(owner_key)];
    if (tenant_index >= floor.tenants.size()) return {};
    const auto linked = std::bit_cast<std::int16_t>(load_u16(
        floor.tenants[tenant_index].exact_bytes, 6U,
        document.header.byte_swapped));
    if (linked < 0 || linked >= static_cast<std::int16_t>(
                                  document.post_elevator.dc24_records.size())) {
      return not_in_tower_alert();
    }
    const auto& record = document.post_elevator.dc24_records[
        static_cast<std::size_t>(linked)];
    const auto floor_number = signed_byte(record[0]);
    const auto key = signed_byte(record[2]);
    if (key < 0) return lobby_alert(floor_number);
    if (floor_number < 0 || floor_number >= 120 ||
        key >= static_cast<int>(OriginalTdtFloor::kIndexCapacity)) {
      return {};
    }
    const auto& destination_floor =
        document.floors[static_cast<std::size_t>(floor_number)];
    const auto destination_index =
        destination_floor.tenant_index[static_cast<std::size_t>(key)];
    if (destination_index >= destination_floor.tenants.size()) return {};
    // 10e0:0bc6 does not use the target tenant's type-width table. It adds
    // 15 cells for a paired dc24 record (byte +7 nonnegative), or 12 cells
    // for a single-sided record (byte +7 negative), to the target's left.
    const int half_width = signed_byte(record[7]) >= 0 ? 15 : 12;
    return focus_at(
        static_cast<int>(destination_floor.tenants[destination_index].left) +
            half_width,
        floor_number, client_width, client_height);
  }
  if (state == 34) {
    // 10e0:01be -> 10e0:0b61 resolves the indexed Retail/service record.
    const auto index = signed_byte(person[6]);
    if (index < 0 || index >= static_cast<std::int16_t>(document.retail.size())) {
      return not_in_tower_alert();
    }
    const auto& service = document.retail[static_cast<std::size_t>(index)].exact_bytes;
    const auto key = signed_byte(service[1]);
    return key < 0 ? lobby_alert(signed_byte(service[0]))
                   : focus_tenant(document, signed_byte(service[0]), key,
                                  client_width, client_height);
  }
  if (state == 35) {
    // 10e0:018c -> 10e0:0c72 resolves the packed dbfc floor/key record.
    const auto index = signed_byte(person[6]);
    if (index < 0 || index >= static_cast<std::int16_t>(
                                  document.post_elevator.dbfc_dwords.size())) {
      return not_in_tower_alert();
    }
    const auto record = document.post_elevator.dbfc_dwords[
        static_cast<std::size_t>(index)];
    const auto floor_number = std::bit_cast<std::int8_t>(
        static_cast<std::uint8_t>(record));
    const auto key = std::bit_cast<std::int8_t>(
        static_cast<std::uint8_t>(record >> 8U));
    return key < 0 ? lobby_alert(floor_number)
                   : focus_tenant(document, floor_number, key,
                                  client_width, client_height);
  }
  if ((state >= 32 && state <= 33) || (state >= 36 && state <= 39)) {
    return not_in_tower_alert();
  }
  return focus_transit_person(document, person_index,
                              client_width, client_height);
}

}  // namespace simtower
