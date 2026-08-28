#include "original_tdt.hpp"

#include <algorithm>

namespace simtower {
namespace {

class Reader {
 public:
  Reader(std::span<const std::byte> bytes, bool byte_swapped)
      : bytes_(bytes), byte_swapped_(byte_swapped) {}

  [[nodiscard]] std::size_t position() const noexcept { return position_; }
  [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - position_; }

  std::uint8_t u8() {
    require(1);
    return std::to_integer<std::uint8_t>(bytes_[position_++]);
  }

  std::uint16_t u16() {
    require(2);
    const auto first = std::to_integer<std::uint8_t>(bytes_[position_]);
    const auto second = std::to_integer<std::uint8_t>(bytes_[position_ + 1]);
    position_ += 2;
    return byte_swapped_
        ? static_cast<std::uint16_t>((first << 8U) | second)
        : static_cast<std::uint16_t>(first | (second << 8U));
  }

  std::uint32_t u32() {
    require(4);
    std::uint32_t value{};
    if (byte_swapped_) {
      for (int index = 0; index < 4; ++index) {
        value = (value << 8U) |
                std::to_integer<std::uint8_t>(bytes_[position_ + index]);
      }
    } else {
      for (int index = 3; index >= 0; --index) {
        value = (value << 8U) |
                std::to_integer<std::uint8_t>(bytes_[position_ + index]);
      }
    }
    position_ += 4;
    return value;
  }

  std::span<const std::byte> take(std::size_t count) {
    require(count);
    const auto result = bytes_.subspan(position_, count);
    position_ += count;
    return result;
  }

 private:
  void require(std::size_t count) const {
    if (count > bytes_.size() - position_) {
      throw OriginalTdtError(OriginalTdtStatus::short_transfer,
                             "The SimTower save ends inside a required structure");
    }
  }

  std::span<const std::byte> bytes_;
  std::size_t position_{};
  bool byte_swapped_{};
};

std::uint16_t raw_little_u16(std::span<const std::byte> bytes) {
  if (bytes.size() < 2) {
    throw OriginalTdtError(OriginalTdtStatus::short_transfer,
                           "The SimTower save has no version word");
  }
  return static_cast<std::uint16_t>(
      std::to_integer<std::uint8_t>(bytes[0]) |
      (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[1])) << 8U));
}

std::uint16_t endian_u16(std::span<const std::byte> bytes, bool byte_swapped) {
  if (bytes.size() < 2) {
    throw OriginalTdtError(OriginalTdtStatus::short_transfer,
                           "The SimTower save ends inside a word");
  }
  const auto first = std::to_integer<std::uint8_t>(bytes[0]);
  const auto second = std::to_integer<std::uint8_t>(bytes[1]);
  return byte_swapped
      ? static_cast<std::uint16_t>((first << 8U) | second)
      : static_cast<std::uint16_t>(first | (second << 8U));
}

std::uint32_t endian_u32(std::span<const std::byte> bytes, bool byte_swapped) {
  if (bytes.size() < 4) {
    throw OriginalTdtError(OriginalTdtStatus::short_transfer,
                           "The SimTower save ends inside a dword");
  }
  std::uint32_t value{};
  if (byte_swapped) {
    for (std::size_t index = 0; index < 4; ++index) {
      value = (value << 8U) | std::to_integer<std::uint8_t>(bytes[index]);
    }
  } else {
    for (std::size_t index = 4; index-- > 0;) {
      value = (value << 8U) | std::to_integer<std::uint8_t>(bytes[index]);
    }
  }
  return value;
}

template <std::size_t Size>
void copy_exact(std::span<const std::byte> source,
                std::array<std::byte, Size>& destination) {
  std::copy(source.begin(), source.end(), destination.begin());
}

void store_u16(std::span<std::byte> destination, std::size_t offset,
               std::uint16_t value, bool byte_swapped) {
  if (offset > destination.size() || destination.size() - offset < 2) {
    throw OriginalTdtError(OriginalTdtStatus::short_transfer,
                           "A SimTower save write crossed its destination");
  }
  if (byte_swapped) {
    destination[offset] = static_cast<std::byte>(value >> 8U);
    destination[offset + 1] = static_cast<std::byte>(value & 0xffU);
  } else {
    destination[offset] = static_cast<std::byte>(value & 0xffU);
    destination[offset + 1] = static_cast<std::byte>(value >> 8U);
  }
}

void store_u32(std::span<std::byte> destination, std::size_t offset,
               std::uint32_t value, bool byte_swapped) {
  if (offset > destination.size() || destination.size() - offset < 4) {
    throw OriginalTdtError(OriginalTdtStatus::short_transfer,
                           "A SimTower save write crossed its destination");
  }
  for (std::size_t index = 0; index < 4; ++index) {
    const std::size_t shift_index = byte_swapped ? 3U - index : index;
    destination[offset + index] =
        static_cast<std::byte>((value >> (shift_index * 8U)) & 0xffU);
  }
}

void append(std::vector<std::byte>& destination,
            std::span<const std::byte> source) {
  destination.insert(destination.end(), source.begin(), source.end());
}

void reverse_word(std::span<std::byte> bytes, std::size_t offset) {
  std::swap(bytes[offset], bytes[offset + 1U]);
}

void reverse_dword(std::span<std::byte> bytes, std::size_t offset) {
  std::swap(bytes[offset], bytes[offset + 3U]);
  std::swap(bytes[offset + 1U], bytes[offset + 2U]);
}

void reverse_words(std::span<std::byte> bytes, std::size_t offset,
                   std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    reverse_word(bytes, offset + index * 2U);
  }
}

void reverse_dwords(std::span<std::byte> bytes, std::size_t offset,
                    std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    reverse_dword(bytes, offset + index * 4U);
  }
}

// Convert the raw structures retained by the native document from the file
// byte order accepted by 10d0:1518 to the little-endian runtime layout that
// 10d0:0b3a subsequently saves. These transformations are all involutions;
// the one exception is the original tenant-byte quirk documented below.
void normalize_opposite_header(std::vector<std::byte>& bytes,
                               std::uint8_t version) {
  std::size_t offset = 2U;  // The compatibility routine already consumed it.
  reverse_word(bytes, offset); offset += 2U;         // b3cc
  reverse_dwords(bytes, offset, 4U); offset += 16U;  // b3ce..b3da
  reverse_word(bytes, offset); offset += 2U;         // b3de
  reverse_dword(bytes, offset); offset += 4U;        // b3e0
  if (version >= 0x20U) {
    reverse_word(bytes, offset); offset += 2U;       // b3e4
  }
  reverse_word(bytes, offset); offset += 2U;         // b3e6
  reverse_words(bytes, offset, 4U); offset += 8U;    // b3e8..b3ee
  reverse_words(bytes, offset, 2U); offset += 4U;    // b3f0/b3f2
  reverse_words(bytes, offset, 8U); offset += 16U;   // b3f4..b402
  if (version >= 0x23U) {
    reverse_word(bytes, offset); offset += 2U;       // b404
  }
  reverse_word(bytes, offset); offset += 2U;         // b406
  reverse_words(bytes, offset, 2U); offset += 4U;    // b408/b40a
  reverse_dword(bytes, offset); offset += 4U;        // b40c
  reverse_words(bytes, offset, 245U);                // b410..b5f9
}

void normalize_opposite_tenant(OriginalTdtTenant& tenant) {
  auto exact = std::span<std::byte>(tenant.exact_bytes);
  reverse_word(exact, 0U);
  reverse_word(exact, 2U);
  reverse_word(exact, 6U);
  reverse_dword(exact, 8U);
  // 10d0:1b4d-1b62 zero-extends byte +0x17, word-swaps it, and writes AL
  // back. Consequently every opposite-endian tenant subtype becomes zero.
  exact[17U] = std::byte{0};
  tenant.variant = std::to_integer<std::uint8_t>(exact[6U]);
  std::copy_n(exact.begin() + 7U, tenant.preserved_07_to_0f.size(),
              tenant.preserved_07_to_0f.begin());
  tenant.rent_rate = std::to_integer<std::uint8_t>(exact[16U]);
  tenant.subtype = 0U;
}

void normalize_opposite_elevator(OriginalTdtElevator& elevator) {
  reverse_word(elevator.reconstructed_header, 0x3cU);
  reverse_word(elevator.reconstructed_header, 0x3eU);
  reverse_dwords(elevator.block_c2, 0U, elevator.block_c2.size() / 4U);

  std::size_t unmapped_floors{};
  if (elevator.bottom_floor <= elevator.top_floor) {
    for (std::int16_t floor = elevator.bottom_floor;
         floor <= elevator.top_floor; ++floor) {
      if (original_elevator_floor_record_index(
              elevator.type, elevator.bottom_floor, elevator.top_floor,
              floor) < 0) {
        ++unmapped_floors;
      }
    }
  }
  if ((unmapped_floors & 1U) != 0U) {
    // 10d0:20b5 performs the swap even when 10a0:17ee returned -1. The
    // resulting address range is the contiguous runtime span 0x252..0x391.
    reverse_dwords(elevator.block_c2, 0x190U, 20U);
    reverse_dwords(elevator.block_2a2, 0U,
                   elevator.block_2a2.size() / 4U);
    reverse_dwords(elevator.block_31a, 0U,
                   elevator.block_31a.size() / 4U);
  }
  for (auto& record : elevator.floor_records) {
    reverse_dwords(record.exact_bytes, 4U, 80U);
  }
  for (auto& car : elevator.car_records) {
    reverse_word(car.exact_bytes, 8U);
    reverse_word(car.exact_bytes, 10U);
    reverse_dwords(car.exact_bytes, 16U, 42U);
  }
}

void normalize_opposite_tail(OriginalTdtPostElevatorTail& tail,
                             std::uint8_t version) {
  reverse_dwords(tail.b846, 0U, tail.b846.size() / 4U);
  reverse_dwords(tail.finance_b89e, 0U,
                 tail.finance_b89e.size() / 4U);
  reverse_dword(tail.b922, 2U);
  reverse_words(tail.b92e, 0x16U, 10U);
  reverse_words(tail.parking_b958, 0U,
                tail.parking_b958.size() / 2U);
  reverse_words(tail.bd5a, 0U, tail.bd5a.size() / 2U);
  for (auto& stair : tail.stairs_bd70) {
    reverse_word(stair.exact_bytes, 2U);
    reverse_word(stair.exact_bytes, 6U);
    reverse_word(stair.exact_bytes, 8U);
  }
  for (auto& route : tail.routes_bff0) {
    reverse_dwords(route, 0U, 0x1e0U / 4U);
  }
  // 10d0:2620-2641 transfers cf88 as ten opaque two-byte chunks. It does
  // not call the word-reversal helper for them.
  for (auto& record : tail.cf9c_records) {
    reverse_dword(record, 2U);
  }
  for (auto& record : tail.db9c_records) {
    reverse_dword(record, 0U);
  }
  // 10d0:26e6-2711 likewise transfers the ten dbfc dwords without reversing
  // them; dc24 is wholly opaque in both transfer paths.
  if (version >= 0x23U) {
    reverse_dwords(tail.version_23_dce4, 0U, 20U);
    reverse_words(tail.dce4_or_dd34, 0U, 20U);
  } else {
    reverse_dwords(tail.dce4_or_dd34, 0U, 10U);
  }
  reverse_words(tail.dynamic_dd5c, 0U,
                tail.dynamic_dd5c.size() / 2U);
  reverse_words(tail.dynamic_dd60, 0U,
                tail.dynamic_dd60.size() / 2U);
  reverse_words(tail.dynamic_dd64, 0U,
                tail.dynamic_dd64.size() / 2U);
  if (version >= 0x18U && tail.version_18_dd6c.size() == 8U) {
    reverse_words(tail.version_18_dd6c, 2U, 3U);
  }
}

void normalize_opposite_runtime_records(OriginalTdtDocument& document,
                                        std::uint8_t version) {
  for (auto& floor : document.floors) {
    for (auto& tenant : floor.tenants) normalize_opposite_tenant(tenant);
  }
  for (auto& person : document.people) {
    reverse_word(person.exact_bytes, 2U);
    reverse_word(person.exact_bytes, 10U);
    reverse_word(person.exact_bytes, 12U);
    reverse_word(person.exact_bytes, 14U);
  }
  for (auto& retail : document.retail) {
    reverse_words(retail.exact_bytes, 12U, 3U);
  }
  for (auto& elevator : document.elevators) {
    normalize_opposite_elevator(elevator);
  }
  normalize_opposite_tail(document.post_elevator, version);
}

std::vector<std::byte> upgrade_header_to_current(
    const OriginalTdtHeader& header) {
  const std::uint8_t version = header.format_version;
  const std::size_t expected =
      556U + (version >= 0x20U ? 2U : 0U) +
      (version >= 0x23U ? 2U : 0U);
  if (header.exact_bytes.size() != expected) {
    throw OriginalTdtError(OriginalTdtStatus::short_transfer,
                           "The preserved SimTower header has the wrong size");
  }
  auto source = header.exact_bytes;
  if (header.byte_swapped) normalize_opposite_header(source, version);

  std::vector<std::byte> current(560U, std::byte{0});
  store_u16(current, 0U, 0x2400U, false);
  std::size_t source_offset = 2U;
  std::size_t current_offset = 2U;
  std::copy_n(source.begin() + source_offset, 24U,
              current.begin() + current_offset);
  source_offset += 24U;
  current_offset += 24U;
  if (version >= 0x20U) {
    std::copy_n(source.begin() + source_offset, 2U,
                current.begin() + current_offset);
    source_offset += 2U;
  }
  current_offset += 2U;  // Revision-0x20 word, zero for older inputs.
  std::copy_n(source.begin() + source_offset, 30U,
              current.begin() + current_offset);
  source_offset += 30U;
  current_offset += 30U;
  if (version >= 0x23U) {
    std::copy_n(source.begin() + source_offset, 2U,
                current.begin() + current_offset);
    source_offset += 2U;
  }
  current_offset += 2U;  // Revision-0x23 tenant-link count.
  std::copy(source.begin() + source_offset, source.end(),
            current.begin() + current_offset);
  return current;
}

OriginalTdtDocument normalize_for_original_game_save(
    const OriginalTdtDocument& source) {
  OriginalTdtDocument current = source;
  const std::uint8_t source_version = source.header.format_version;
  current.header.exact_bytes = upgrade_header_to_current(source.header);

  if (source.header.byte_swapped) {
    normalize_opposite_runtime_records(current, source_version);
  }

  auto& tail = current.post_elevator;
  if (source_version < 0x23U) {
    tail.version_23_dce4.assign(0x50U, std::byte{0});
    for (std::size_t index = 0; index < tail.dce4_person_indices.size();
         ++index) {
      store_u32(tail.version_23_dce4, index * 4U,
                static_cast<std::uint32_t>(tail.dce4_person_indices[index]),
                false);
    }
    // Pre-0x23 files contain dce4 here. The current stream writes a distinct
    // zero-initialized dd34 tenant-link table after the expanded dce4 block.
    tail.dce4_or_dd34.fill(std::byte{0});
    current.header.tenant_link_count = 0U;
    current.tenant_link_names.clear();
  }
  if (source_version < 0x18U) {
    tail.version_18_dd6c.assign(8U, std::byte{0});
  }

  current.header.raw_version = 0x2400U;
  current.header.format_version = 0x24U;
  current.header.byte_swapped = false;
  return current;
}

}  // namespace

void carry_original_process_random_state(
    const OriginalTdtDocument* active,
    OriginalTdtDocument& replacement) noexcept {
  if (active != nullptr) {
    replacement.random_state = active->random_state;
  }
}

std::int16_t original_elevator_floor_record_index(
    std::uint8_t type,
    std::int8_t bottom_floor,
    std::int8_t top_floor,
    std::int16_t floor) noexcept {
  if (type == 0U) {
    if (floor <= 10) {
      return static_cast<std::int16_t>(floor - 1);
    }
    const std::int16_t delta = static_cast<std::int16_t>(floor - 10);
    const std::int16_t quotient = static_cast<std::int16_t>(delta / 15);
    const std::int16_t remainder = static_cast<std::int16_t>(delta % 15);
    return remainder == 14
        ? static_cast<std::int16_t>(quotient + 10)
        : static_cast<std::int16_t>(-1);
  }
  if (floor > top_floor) {
    return -1;
  }
  return static_cast<std::int16_t>(floor - bottom_floor);
}

OriginalTdtDocument parse_original_tdt(std::span<const std::byte> bytes) {
  OriginalTdtDocument document{};
  document.exact_bytes.assign(bytes.begin(), bytes.end());

  const std::uint16_t raw_version = raw_little_u16(bytes);
  const std::uint8_t low = static_cast<std::uint8_t>(raw_version & 0xffU);
  const std::uint8_t high = static_cast<std::uint8_t>(raw_version >> 8U);
  // 10d0:0b6a-0b81 recognizes the opposite-endian family only when the low
  // byte is nonzero and the high byte is zero.
  const bool byte_swapped = low != 0U && high == 0U;
  const std::uint8_t version = byte_swapped ? low : high;
  if (version < 0x17U) {
    throw OriginalTdtError(OriginalTdtStatus::version_too_old,
                           "The SimTower save version is older than 0x17");
  }
  if (version > 0x24U) {
    throw OriginalTdtError(OriginalTdtStatus::version_too_new,
                           "The SimTower save version is newer than 0x24");
  }

  Reader reader(bytes, byte_swapped);
  document.header.raw_version = raw_version;
  document.header.format_version = version;
  document.header.byte_swapped = byte_swapped;
  (void)reader.u16();  // raw version, already classified above
  document.header.rating = reader.u16();
  document.header.balance = static_cast<std::int32_t>(reader.u32());
  document.header.other_income = static_cast<std::int32_t>(reader.u32());
  document.header.construction_costs = static_cast<std::int32_t>(reader.u32());
  document.header.last_quarter_money = static_cast<std::int32_t>(reader.u32());
  document.header.frame_time = reader.u16();
  document.header.current_day = static_cast<std::int32_t>(reader.u32());
  if (version >= 0x20U) {
    document.header.version_20_word = reader.u16();
  }
  document.header.lobby_height = reader.u16();
  (void)reader.u16();  // b3e8
  (void)reader.u16();  // b3ea
  (void)reader.u16();  // b3ec
  (void)reader.u16();  // b3ee
  document.header.view_x = reader.u16();
  document.header.view_y = reader.u16();
  (void)reader.u16();  // b3f4
  (void)reader.u16();  // b3f6
  (void)reader.u16();  // b3f8
  (void)reader.u16();  // b3fa
  (void)reader.u16();  // b3fc
  (void)reader.u16();  // b3fe
  (void)reader.u16();  // b400
  document.header.person_link_count = reader.u16();  // b402
  if (version < 0x23U && document.header.person_link_count > 10U) {
    // Exact 10d0:0dab compatibility clamp for the old ten-dword dce4 block.
    document.header.person_link_count = 10U;
  } else if (document.header.person_link_count > 20U) {
    throw OriginalTdtError(OriginalTdtStatus::malformed,
                           "The SimTower person-link count exceeds dce4");
  }
  if (version >= 0x23U) {
    document.header.tenant_link_count = reader.u16();  // b404
    if (document.header.tenant_link_count > 20U) {
      throw OriginalTdtError(OriginalTdtStatus::malformed,
                             "The SimTower tenant-link count exceeds dd34");
    }
  }
  (void)reader.u16();  // b406
  (void)reader.u32();  // b408
  (void)reader.u32();  // b40c
  (void)reader.take(0x1eaU);  // b410 fixed misc block
  document.header.exact_bytes.assign(bytes.begin(), bytes.begin() + reader.position());

  for (auto& floor : document.floors) {
    floor.file_offset = reader.position();
    const std::uint16_t tenant_count = reader.u16();
    floor.left_edge = reader.u16();
    floor.right_edge = reader.u16();
    // The record array and the following index table have different sizes in
    // the original 0xb4e-byte runtime allocation. The executable reserves
    // 150 tenant records at +6, then persists 94 index words from +0xa92.
    if (tenant_count > OriginalTdtFloor::kTenantCapacity) {
      throw OriginalTdtError(OriginalTdtStatus::malformed,
                             "A SimTower floor contains more than 150 tenants");
    }
    floor.tenants.reserve(tenant_count);
    for (std::uint16_t index = 0; index < tenant_count; ++index) {
      const auto exact = reader.take(18);
      OriginalTdtTenant tenant{};
      copy_exact(exact, tenant.exact_bytes);
      Reader tenant_reader(exact, byte_swapped);
      tenant.left = tenant_reader.u16();
      tenant.right = tenant_reader.u16();
      tenant.type = static_cast<std::int8_t>(tenant_reader.u8());
      tenant.status = tenant_reader.u8();
      tenant.variant = tenant_reader.u8();
      copy_exact(tenant_reader.take(9), tenant.preserved_07_to_0f);
      tenant.rent_rate = tenant_reader.u8();
      tenant.subtype = tenant_reader.u8();
      if (byte_swapped) {
        // 10d0:1aee-1b3e swaps the runtime word at tenant +6 and reverses
        // the dword at +8 after the raw record transfer. Keep exact_bytes in
        // source-file order for lossless tooling, but expose the runtime byte
        // fields used directly by gameplay in their post-loader positions.
        tenant.variant = std::to_integer<std::uint8_t>(exact[7U]);
        tenant.preserved_07_to_0f[0] = exact[6U];
        tenant.preserved_07_to_0f[1] = exact[11U];
        tenant.preserved_07_to_0f[2] = exact[10U];
        tenant.preserved_07_to_0f[3] = exact[9U];
        tenant.preserved_07_to_0f[4] = exact[8U];
        std::copy_n(exact.begin() + 12U, 4U,
                    tenant.preserved_07_to_0f.begin() + 5U);
        // Exact 10d0:1b4d-1b62's zero-extend/swap/write-AL sequence.
        tenant.subtype = 0U;
      }
      floor.tenants.push_back(tenant);
    }
    for (auto& entry : floor.tenant_index) {
      // Before 0x24 the loader widens 94 file bytes into its word table.
      entry = version >= 0x24U ? reader.u16() : reader.u8();
    }
  }

  document.people_count = reader.u32();
  document.people_offset = reader.position();
  const std::uint64_t people_bytes =
      static_cast<std::uint64_t>(document.people_count) * 16ULL;
  if (people_bytes > reader.remaining()) {
    throw OriginalTdtError(OriginalTdtStatus::short_transfer,
                           "The SimTower save ends inside its people table");
  }
  document.people.reserve(document.people_count);
  for (std::uint32_t index = 0; index < document.people_count; ++index) {
    OriginalTdtPersonRecord person{};
    copy_exact(reader.take(person.exact_bytes.size()), person.exact_bytes);
    document.people.push_back(person);
  }
  document.retail_offset = reader.position();
  for (auto& retail : document.retail) {
    copy_exact(reader.take(retail.exact_bytes.size()), retail.exact_bytes);
  }
  document.elevator_table_offset = reader.position();

  for (auto& elevator : document.elevators) {
    elevator.file_offset = reader.position();
    if (version >= 0x21U) {
      const auto file_header = reader.take(elevator.reconstructed_header.size());
      elevator.exact_file_header.assign(file_header.begin(), file_header.end());
      copy_exact(file_header, elevator.reconstructed_header);
      elevator.file_header_size = elevator.reconstructed_header.size();
    } else {
      // 10d0:0fd4-1053 reads 47 bytes, clears +0x3c, reads 133 bytes
      // at +0x3d, then duplicates byte +0x2e across the 14-byte legacy gap.
      const auto prefix = reader.take(0x2fU);
      const auto suffix = reader.take(0x85U);
      elevator.exact_file_header.insert(elevator.exact_file_header.end(),
                                        prefix.begin(), prefix.end());
      elevator.exact_file_header.insert(elevator.exact_file_header.end(),
                                        suffix.begin(), suffix.end());
      std::copy(prefix.begin(), prefix.end(), elevator.reconstructed_header.begin());
      std::copy(suffix.begin(), suffix.end(),
                elevator.reconstructed_header.begin() + 0x3dU);
      std::fill(elevator.reconstructed_header.begin() + 0x2eU,
                elevator.reconstructed_header.begin() + 0x3cU,
                elevator.reconstructed_header[0x2eU]);
      elevator.file_header_size = prefix.size() + suffix.size();
    }

    const auto header = std::span<const std::byte>(elevator.reconstructed_header);
    elevator.used = std::to_integer<std::uint8_t>(header[0]);
    elevator.type = std::to_integer<std::uint8_t>(header[1]);
    elevator.capacity = std::to_integer<std::uint8_t>(header[2]);
    elevator.cars = std::to_integer<std::uint8_t>(header[3]);
    std::copy_n(header.begin() + 4, elevator.schedule.size(),
                elevator.schedule.begin());
    // 10d0:1518 swaps only the two header words at +0x3c and +0x3e.
    elevator.word_3c = endian_u16(header.subspan(0x3cU, 2), byte_swapped);
    elevator.x = endian_u16(header.subspan(0x3eU, 2), byte_swapped);
    elevator.top_floor = static_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(header[0x40U]));
    elevator.bottom_floor = static_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(header[0x41U]));
    std::copy_n(header.begin() + 0x42U, elevator.serviced_floors.size(),
                elevator.serviced_floors.begin());
    std::copy_n(header.begin() + 0xbaU, elevator.car_home_floors.size(),
                elevator.car_home_floors.begin());
    elevator.payload_offset = reader.position();

    if (elevator.used == 0U) {
      continue;
    }

    copy_exact(reader.take(elevator.block_c2.size()), elevator.block_c2);
    copy_exact(reader.take(elevator.block_2a2.size()), elevator.block_2a2);
    copy_exact(reader.take(elevator.block_31a.size()), elevator.block_31a);
    if (elevator.bottom_floor <= elevator.top_floor) {
      for (std::int16_t floor = elevator.bottom_floor;
           floor <= elevator.top_floor; ++floor) {
        const std::int16_t mapped_index = original_elevator_floor_record_index(
            elevator.type, elevator.bottom_floor, elevator.top_floor, floor);
        if (mapped_index < 0) {
          continue;
        }
        OriginalTdtElevatorFloorRecord record{};
        record.mapped_index = mapped_index;
        record.floor = static_cast<std::int8_t>(floor);
        copy_exact(reader.take(record.exact_bytes.size()), record.exact_bytes);
        elevator.floor_records.push_back(record);
      }
    }
    for (auto& car : elevator.car_records) {
      if (version >= 0x22U) {
        copy_exact(reader.take(car.exact_bytes.size()), car.exact_bytes);
      } else {
        // 10d0:10f2-11dc and 10d0:2180-2218 reconstruct the 0x15a-byte
        // runtime record from four legacy file segments. The omitted 24- and
        // 6-byte gaps retain their zero-initialized runtime values.
        const auto prefix = reader.take(0x10U);
        const auto first = reader.take(0x90U);
        const auto second = reader.take(0x24U);
        const auto suffix = reader.take(0x78U);
        std::copy(prefix.begin(), prefix.end(), car.exact_bytes.begin());
        std::copy(first.begin(), first.end(), car.exact_bytes.begin() + 0x10U);
        std::copy(second.begin(), second.end(), car.exact_bytes.begin() + 0xb8U);
        std::copy(suffix.begin(), suffix.end(), car.exact_bytes.begin() + 0xe2U);
      }
      if (version < 0x19U) {
        car.exact_bytes[0x0fU] = std::byte{1};
      }
    }
    if (version < 0x22U) {
      if (elevator.type == 0U) {
        elevator.capacity = 0x2aU;
      } else if (elevator.type == 1U || elevator.type == 2U) {
        elevator.capacity = 0x15U;
      }
      elevator.reconstructed_header[2U] =
          static_cast<std::byte>(elevator.capacity);
    }
    elevator.payload_size = reader.position() - elevator.payload_offset;
  }
  document.after_elevators_offset = reader.position();

  auto& tail = document.post_elevator;
  tail.file_offset = reader.position();
  copy_exact(reader.take(tail.b846.size()), tail.b846);
  copy_exact(reader.take(tail.finance_b89e.size()), tail.finance_b89e);
  copy_exact(reader.take(tail.b922.size()), tail.b922);
  copy_exact(reader.take(tail.b92e.size()), tail.b92e);
  copy_exact(reader.take(tail.parking_b958.size()), tail.parking_b958);
  copy_exact(reader.take(tail.bd5a.size()), tail.bd5a);
  for (auto& stair : tail.stairs_bd70) {
    copy_exact(reader.take(stair.exact_bytes.size()), stair.exact_bytes);
  }
  for (auto& route : tail.routes_bff0) {
    copy_exact(reader.take(route.size()), route);
  }
  copy_exact(reader.take(tail.cf10.size()), tail.cf10);
  for (auto& word : tail.cf88_words) {
    // 10d0:2620-2641 omits the opposite-endian word reversal here.
    word = endian_u16(reader.take(2U), false);
  }
  for (auto& record : tail.cf9c_records) {
    copy_exact(reader.take(record.size()), record);
  }
  for (auto& record : tail.db9c_records) {
    copy_exact(reader.take(record.size()), record);
  }
  for (auto& dword : tail.dbfc_dwords) {
    // 10d0:26e6-2711 also leaves these four-byte chunks unreversed.
    dword = endian_u32(reader.take(4U), false);
  }
  for (auto& record : tail.dc24_records) {
    copy_exact(reader.take(record.size()), record);
  }
  if (version >= 0x23U) {
    const auto exact = reader.take(0x50U);
    tail.version_23_dce4.assign(exact.begin(), exact.end());
  }
  copy_exact(reader.take(tail.dce4_or_dd34.size()), tail.dce4_or_dd34);
  copy_exact(reader.take(tail.dynamic_dd5c.size()), tail.dynamic_dd5c);
  copy_exact(reader.take(tail.dynamic_dd60.size()), tail.dynamic_dd60);
  copy_exact(reader.take(tail.dynamic_dd64.size()), tail.dynamic_dd64);
  if (version >= 0x18U) {
    const auto exact = reader.take(8U);
    tail.version_18_dd6c.assign(exact.begin(), exact.end());
  }
  tail.end_offset = reader.position();

  const auto decode_dword = [byte_swapped](std::span<const std::byte> source,
                                            std::size_t offset) {
    return static_cast<std::int32_t>(
        endian_u32(source.subspan(offset, 4), byte_swapped));
  };
  const auto b846 = std::span<const std::byte>(tail.b846);
  for (std::size_t series = 0; series < tail.b846_series.size(); ++series) {
    for (std::size_t entry = 0; entry < tail.b846_series[series].size(); ++entry) {
      tail.b846_series[series][entry] =
          decode_dword(b846, (series * 11U + entry) * 4U);
    }
  }

  const auto finance = std::span<const std::byte>(tail.finance_b89e);
  for (std::size_t index = 0; index < 10; ++index) {
    tail.finance.population_by_category[index] = decode_dword(finance, index * 4U);
    tail.finance.income_by_category[index] = decode_dword(finance, 44U + index * 4U);
    tail.finance.maintenance_by_category[index] =
        decode_dword(finance, 88U + index * 4U);
  }
  tail.finance.total_population = decode_dword(finance, 40U);
  tail.finance.total_income = decode_dword(finance, 84U);
  tail.finance.total_maintenance = decode_dword(finance, 128U);

  const auto dce4 = version >= 0x23U
                        ? std::span<const std::byte>(tail.version_23_dce4)
                        : std::span<const std::byte>(tail.dce4_or_dd34);
  const std::size_t dce4_count = version >= 0x23U ? 20U : 10U;
  for (std::size_t index = 0; index < dce4_count; ++index) {
    tail.dce4_person_indices[index] = decode_dword(dce4, index * 4U);
  }

  const auto b922 = std::span<const std::byte>(tail.b922);
  tail.b922_flag = std::to_integer<std::uint8_t>(b922[0]);
  tail.b923 = std::to_integer<std::uint8_t>(b922[1]);
  tail.b924 = decode_dword(b922, 2U);
  tail.b928 = std::to_integer<std::uint8_t>(b922[6]);
  tail.b929 = std::to_integer<std::uint8_t>(b922[7]);
  tail.b92a = std::to_integer<std::uint8_t>(b922[8]);
  tail.b92b = std::to_integer<std::uint8_t>(b922[9]);
  tail.b92c = std::to_integer<std::uint8_t>(b922[10]);
  tail.b92d = std::to_integer<std::uint8_t>(b922[11]);
  const auto b92e = std::span<const std::byte>(tail.b92e);
  tail.b92e_counter = std::to_integer<std::uint8_t>(b92e[0]);
  for (std::size_t index = 0; index < tail.b944_words.size(); ++index) {
    tail.b944_words[index] =
        endian_u16(b92e.subspan(0x16U + index * 2U, 2U), byte_swapped);
  }

  const auto parking = std::span<const std::byte>(tail.parking_b958);
  tail.parking_connected = static_cast<std::int16_t>(
      endian_u16(parking.subspan(0, 2), byte_swapped));
  for (std::size_t index = 0; index < tail.parking_entries.size(); ++index) {
    tail.parking_entries[index] =
        endian_u16(parking.subspan(2U + index * 2U, 2U), byte_swapped);
  }

  const auto bd5a = std::span<const std::byte>(tail.bd5a);
  tail.bd5a_count = endian_u16(bd5a.subspan(0, 2), byte_swapped);
  for (std::size_t index = 0; index < tail.bd5c_entries.size(); ++index) {
    tail.bd5c_entries[index] =
        endian_u16(bd5a.subspan(2U + index * 2U, 2U), byte_swapped);
  }
  for (auto& stair : tail.stairs_bd70) {
    const auto exact = std::span<const std::byte>(stair.exact_bytes);
    stair.used = std::to_integer<std::uint8_t>(exact[0]);
    stair.shape = std::to_integer<std::uint8_t>(exact[1]);
    stair.x = endian_u16(exact.subspan(2, 2), byte_swapped);
    stair.floor = static_cast<std::int8_t>(
        std::to_integer<std::uint8_t>(exact[4]));
    stair.byte_5 = std::to_integer<std::uint8_t>(exact[5]);
    stair.word_6 = endian_u16(exact.subspan(6, 2), byte_swapped);
    stair.word_8 = endian_u16(exact.subspan(8, 2), byte_swapped);
  }

  // Direct 1188:02ea coverage. Each ordinary Windows name is one sixteen-byte
  // C-string record. A leading byte in 1..15 instead identifies the older
  // 256-byte Pascal-string form: the routine shifts that many characters over
  // the length byte, appends NUL, and consumes the remaining 240 bytes before
  // handing the text to 1188:061c/06dc. Their freshly GlobalAlloc-zeroed
  // sixteen-byte destinations make the normalized bytes after NUL zero.
  const auto read_link_name = [&reader]() {
    OriginalTdtLinkName name{};
    const auto first_sixteen = reader.take(name.exact_bytes.size());
    const auto legacy_length =
        std::to_integer<std::uint8_t>(first_sixteen.front());
    if (legacy_length > 0U && legacy_length < name.exact_bytes.size()) {
      std::copy_n(first_sixteen.begin() + 1,
                  static_cast<std::size_t>(legacy_length),
                  name.exact_bytes.begin());
      name.exact_bytes[legacy_length] = std::byte{0};
      (void)reader.take(0xf0U);
    } else {
      // Retain ordinary serialized padding for deterministic byte-exact
      // tooling. The first NUL still defines every gameplay-facing name.
      copy_exact(first_sixteen, name.exact_bytes);
    }
    return name;
  };

  // Native vector ownership replaces 1188:007e/01be's two twenty-pointer
  // allocation/free tables. 1188:061c/06dc rebuild the active tables while
  // these records are read. Duplicate keys reuse their first slot and
  // LSTRCPY the later text over the existing sixteen-byte allocation, leaving
  // bytes after the new NUL intact.
  const auto overwrite_c_string = [](OriginalTdtLinkName& destination,
                                     const OriginalTdtLinkName& source) {
    for (std::size_t index = 0; index < source.exact_bytes.size(); ++index) {
      destination.exact_bytes[index] = source.exact_bytes[index];
      if (source.exact_bytes[index] == std::byte{0}) break;
    }
  };

  const auto source_person_keys = tail.dce4_person_indices;
  const std::size_t source_person_count = document.header.person_link_count;
  document.header.person_link_count = 0U;
  document.person_link_names.reserve(source_person_count);
  for (std::size_t source_index = 0; source_index < source_person_count;
       ++source_index) {
    const auto name = read_link_name();
    const auto key = source_person_keys[source_index];
    const auto active_end = tail.dce4_person_indices.begin() +
                            document.header.person_link_count;
    const auto existing = std::find(tail.dce4_person_indices.begin(),
                                    active_end, key);
    if (existing != active_end) {
      overwrite_c_string(
          document.person_link_names[static_cast<std::size_t>(
              existing - tail.dce4_person_indices.begin())],
          name);
      continue;
    }
    const std::size_t destination = document.header.person_link_count;
    tail.dce4_person_indices[destination] = key;
    document.person_link_names.push_back(name);
    ++document.header.person_link_count;
  }

  std::array<std::uint16_t, 20> source_tenant_keys{};
  const std::size_t source_tenant_count = document.header.tenant_link_count;
  for (std::size_t index = 0; index < source_tenant_count; ++index) {
    source_tenant_keys[index] = endian_u16(
        std::span<const std::byte>(tail.dce4_or_dd34).subspan(index * 2U, 2U),
        byte_swapped);
  }
  std::array<std::uint16_t, 20> active_tenant_keys{};
  document.header.tenant_link_count = 0U;
  document.tenant_link_names.reserve(source_tenant_count);
  for (std::size_t source_index = 0; source_index < source_tenant_count;
       ++source_index) {
    const auto name = read_link_name();
    const auto key = source_tenant_keys[source_index];
    const auto active_end = active_tenant_keys.begin() +
                            document.header.tenant_link_count;
    const auto existing =
        std::find(active_tenant_keys.begin(), active_end, key);
    if (existing != active_end) {
      overwrite_c_string(
          document.tenant_link_names[static_cast<std::size_t>(
              existing - active_tenant_keys.begin())],
          name);
      continue;
    }
    const std::size_t destination = document.header.tenant_link_count;
    active_tenant_keys[destination] = key;
    store_u16(tail.dce4_or_dd34, destination * 2U, key, byte_swapped);
    document.tenant_link_names.push_back(name);
    ++document.header.tenant_link_count;
  }
  const auto trailing = reader.take(reader.remaining());
  document.trailing_bytes.assign(trailing.begin(), trailing.end());
  if (byte_swapped) {
    // 10d0:1518 is a load-only compatibility routine. It reverses selected
    // fields in-place and leaves the live Win16 data segment in the same
    // little-endian runtime layout as 10d0:0b3a. Keep document.exact_bytes as
    // the untouched source for serialize_original_tdt_lossless(), but expose
    // all gameplay-facing structures exactly as the original does after load.
    normalize_opposite_header(document.header.exact_bytes, version);
    normalize_opposite_runtime_records(document, version);
    document.header.raw_version =
        static_cast<std::uint16_t>(version) << 8U;
    document.header.byte_swapped = false;
  }
  return document;
}

OriginalTdtDocument make_original_new_tdt() {
  OriginalTdtDocument document{};

  // 10d0:086c writes these globals after the process data segment and all
  // GMEM_ZEROINIT allocations have supplied the remaining zero bytes. Exact
  // 1060:0000 clears the eleven-dword population, income, and maintenance
  // finance bands; value-initializing the native document performs the same
  // fresh-tower reset before the explicit nonzero fields below are installed.
  // Exact 11f0:0000 likewise clears the pending-queue count and head bytes
  // DS:b92e/b92f, represented by b92e_counter and b92e[1].
  document.header.raw_version = 0x2400U;
  document.header.format_version = 0x24U;
  document.header.byte_swapped = false;
  document.header.rating = 1U;                 // DS:b3cc
  document.header.balance = 20000;             // DS:b3ce
  document.header.other_income = 0;            // DS:b3d2
  document.header.construction_costs = 0;       // DS:b3d6
  document.header.last_quarter_money = 20000;  // DS:b3da
  document.header.frame_time = 0x09e5U;        // 1200:0000
  document.header.current_day = 0;
  document.header.version_20_word = 0;
  document.header.lobby_height = 0;
  document.header.view_x = 0;
  document.header.view_y = 0;
  document.header.person_link_count = 0;
  document.header.tenant_link_count = 0;
  document.header.exact_bytes.resize(560U, std::byte{0});
  // DS:b3e8..b3ee are the four explicit -1 sentinels at 10d0:0978-098a.
  std::fill(document.header.exact_bytes.begin() + 30,
            document.header.exact_bytes.begin() + 38, std::byte{0xff});

  // 1238:001e establishes the first 256-record GMEM_ZEROINIT pool and
  // 1238:029f clears each exact 16-byte record. 10d0:086c then calls
  // 1238:00b0, which resets the logical count and invokes 1238:0230 twice,
  // yielding 512 zeroed simulation records. The count is the allocated pool
  // capacity and is transferred verbatim by 10d0:0b3a.
  document.people.resize(512U);
  document.people_count = static_cast<std::uint32_t>(document.people.size());

  // 11a8:00b4 initializes every 18-byte service slot with only these two
  // nonzero sentinels; the allocation is otherwise zero-filled.
  for (auto& retail : document.retail) {
    retail.exact_bytes[0] = std::byte{0xff};
    retail.exact_bytes[2] = std::byte{3};
  }

  auto& tail = document.post_elevator;
  tail.b924 = -1;  // 1148:0000
  tail.cf88_words.fill(0xffffU);  // 10f8:0000
  for (auto& record : tail.cf9c_records) {
    record[0] = std::byte{0xff};  // 1198:016e
  }
  tail.dbfc_dwords.fill(0x0000ffffU);  // 1170:00e1
  for (auto& record : tail.dc24_records) {
    record[0] = std::byte{0xfe};  // 1180:0000
    record[1] = std::byte{0xfe};
  }
  tail.version_23_dce4.assign(0x50U, std::byte{0});  // 1188:0000
  tail.version_18_dd6c.assign(8U, std::byte{0});     // 11b8:0000

  return document;
}

bool original_tower_transition_requires_confirmation(
    const OriginalTdtDocument* document) noexcept {
  // 10d0:2a8e checks the active-document latch and floor-slot pointer/count
  // before entering its save/discard/cancel prompt path.
  return document != nullptr && document->floors.size() > 10U &&
         !document->floors[10U].tenants.empty();
}

std::vector<std::byte> serialize_original_tdt_lossless(
    const OriginalTdtDocument& document) {
  return document.exact_bytes;
}

std::vector<std::byte> serialize_original_tdt(
    const OriginalTdtDocument& document) {
  const bool byte_swapped = document.header.byte_swapped;
  const std::uint8_t version = document.header.format_version;
  const std::size_t expected_header_size =
      556U + (version >= 0x20U ? 2U : 0U) + (version >= 0x23U ? 2U : 0U);
  if (document.header.exact_bytes.size() != expected_header_size) {
    throw OriginalTdtError(OriginalTdtStatus::short_transfer,
                           "The preserved SimTower header has the wrong size");
  }
  if (document.people_count != document.people.size()) {
    throw OriginalTdtError(OriginalTdtStatus::malformed,
                           "The SimTower people count disagrees with its table");
  }
  const std::uint16_t person_link_capacity = version >= 0x23U ? 20U : 10U;
  if (document.header.person_link_count > person_link_capacity) {
    throw OriginalTdtError(
        OriginalTdtStatus::malformed,
        "The SimTower person-link count exceeds this revision's table");
  }
  if ((version < 0x23U && document.header.tenant_link_count != 0U) ||
      document.header.tenant_link_count > 20U) {
    throw OriginalTdtError(
        OriginalTdtStatus::malformed,
        "The SimTower tenant-link count exceeds this revision's table");
  }
  std::vector<std::byte> output = document.header.exact_bytes;
  auto header = std::span<std::byte>(output);

  // The raw version probe is always a little-endian host read, even for the
  // opposite-endian family delegated to 10d0:1518.
  store_u16(header, 0, document.header.raw_version, false);
  std::size_t position = 2;
  store_u16(header, position, document.header.rating, byte_swapped); position += 2;
  store_u32(header, position, static_cast<std::uint32_t>(document.header.balance),
            byte_swapped); position += 4;
  store_u32(header, position, static_cast<std::uint32_t>(document.header.other_income),
            byte_swapped); position += 4;
  store_u32(header, position,
            static_cast<std::uint32_t>(document.header.construction_costs),
            byte_swapped); position += 4;
  store_u32(header, position,
            static_cast<std::uint32_t>(document.header.last_quarter_money),
            byte_swapped); position += 4;
  store_u16(header, position, document.header.frame_time, byte_swapped); position += 2;
  store_u32(header, position,
            static_cast<std::uint32_t>(document.header.current_day),
            byte_swapped); position += 4;
  if (version >= 0x20U) {
    store_u16(header, position, document.header.version_20_word, byte_swapped);
    position += 2;
  }
  store_u16(header, position, document.header.lobby_height, byte_swapped);
  position += 2 + 8;  // b3e8..b3ee remain byte-exact
  store_u16(header, position, document.header.view_x, byte_swapped); position += 2;
  store_u16(header, position, document.header.view_y, byte_swapped);
  const std::size_t person_link_count_offset =
      56U - (version >= 0x20U ? 0U : 2U);
  store_u16(header, person_link_count_offset,
            document.header.person_link_count, byte_swapped);
  if (version >= 0x23U) {
    store_u16(header, person_link_count_offset + 2U,
              document.header.tenant_link_count, byte_swapped);
  }

  for (const auto& floor : document.floors) {
    if (floor.tenants.size() > OriginalTdtFloor::kTenantCapacity) {
      throw OriginalTdtError(OriginalTdtStatus::malformed,
                             "A SimTower floor contains more than 150 tenants");
    }
    std::array<std::byte, 6> floor_header{};
    store_u16(floor_header, 0,
              static_cast<std::uint16_t>(floor.tenants.size()), byte_swapped);
    store_u16(floor_header, 2, floor.left_edge, byte_swapped);
    store_u16(floor_header, 4, floor.right_edge, byte_swapped);
    append(output, floor_header);
    for (const auto& tenant : floor.tenants) {
      auto exact = tenant.exact_bytes;
      store_u16(exact, 0, tenant.left, byte_swapped);
      store_u16(exact, 2, tenant.right, byte_swapped);
      exact[4] = static_cast<std::byte>(tenant.type);
      exact[5] = static_cast<std::byte>(tenant.status);
      exact[6] = static_cast<std::byte>(tenant.variant);
      std::copy(tenant.preserved_07_to_0f.begin(),
                tenant.preserved_07_to_0f.end(), exact.begin() + 7);
      exact[16] = static_cast<std::byte>(tenant.rent_rate);
      exact[17] = static_cast<std::byte>(tenant.subtype);
      append(output, exact);
    }
    for (const auto entry : floor.tenant_index) {
      if (version >= 0x24U) {
        std::array<std::byte, 2> exact{};
        store_u16(exact, 0, entry, byte_swapped);
        append(output, exact);
      } else {
        if (entry > 0xffU) {
          throw OriginalTdtError(OriginalTdtStatus::malformed,
                                 "A pre-0x24 floor index does not fit in a byte");
        }
        output.push_back(static_cast<std::byte>(entry));
      }
    }
  }

  std::array<std::byte, 4> people_count{};
  store_u32(people_count, 0, document.people_count, byte_swapped);
  append(output, people_count);
  for (const auto& person : document.people) append(output, person.exact_bytes);
  for (const auto& retail : document.retail) append(output, retail.exact_bytes);

  for (const auto& elevator : document.elevators) {
    auto header = elevator.reconstructed_header;
    header[0] = static_cast<std::byte>(elevator.used);
    header[1] = static_cast<std::byte>(elevator.type);
    header[2] = static_cast<std::byte>(elevator.capacity);
    header[3] = static_cast<std::byte>(elevator.cars);
    std::copy(elevator.schedule.begin(), elevator.schedule.end(), header.begin() + 4);
    auto header_span = std::span<std::byte>(header);
    store_u16(header_span, 0x3cU, elevator.word_3c, byte_swapped);
    store_u16(header_span, 0x3eU, elevator.x, byte_swapped);
    header[0x40U] = static_cast<std::byte>(elevator.top_floor);
    header[0x41U] = static_cast<std::byte>(elevator.bottom_floor);
    std::copy(elevator.serviced_floors.begin(), elevator.serviced_floors.end(),
              header.begin() + 0x42U);
    std::copy(elevator.car_home_floors.begin(), elevator.car_home_floors.end(),
              header.begin() + 0xbaU);
    if (version >= 0x21U) {
      append(output, header);
    } else {
      append(output, std::span<const std::byte>(header).first(0x2fU));
      append(output, std::span<const std::byte>(header).subspan(0x3dU, 0x85U));
    }
    if (elevator.used == 0U) {
      continue;
    }
    append(output, elevator.block_c2);
    append(output, elevator.block_2a2);
    append(output, elevator.block_31a);
    for (const auto& floor_record : elevator.floor_records) {
      append(output, floor_record.exact_bytes);
    }
    for (const auto& car : elevator.car_records) {
      if (version >= 0x22U) {
        append(output, car.exact_bytes);
      } else {
        append(output, std::span<const std::byte>(car.exact_bytes).first(0x10U));
        append(output, std::span<const std::byte>(car.exact_bytes).subspan(
                           0x10U, 0x90U));
        append(output, std::span<const std::byte>(car.exact_bytes).subspan(
                           0xb8U, 0x24U));
        append(output, std::span<const std::byte>(car.exact_bytes).subspan(
                           0xe2U, 0x78U));
      }
    }
  }

  const auto& tail = document.post_elevator;
  auto b846 = tail.b846;
  auto b846_span = std::span<std::byte>(b846);
  for (std::size_t series = 0; series < tail.b846_series.size(); ++series) {
    for (std::size_t entry = 0; entry < tail.b846_series[series].size(); ++entry) {
      store_u32(b846_span, (series * 11U + entry) * 4U,
                static_cast<std::uint32_t>(tail.b846_series[series][entry]),
                byte_swapped);
    }
  }
  append(output, b846);

  auto finance = tail.finance_b89e;
  auto finance_span = std::span<std::byte>(finance);
  for (std::size_t index = 0; index < 10; ++index) {
    store_u32(finance_span, index * 4U,
              static_cast<std::uint32_t>(tail.finance.population_by_category[index]),
              byte_swapped);
    store_u32(finance_span, 44U + index * 4U,
              static_cast<std::uint32_t>(tail.finance.income_by_category[index]),
              byte_swapped);
    store_u32(finance_span, 88U + index * 4U,
              static_cast<std::uint32_t>(tail.finance.maintenance_by_category[index]),
              byte_swapped);
  }
  store_u32(finance_span, 40U,
            static_cast<std::uint32_t>(tail.finance.total_population), byte_swapped);
  store_u32(finance_span, 84U,
            static_cast<std::uint32_t>(tail.finance.total_income), byte_swapped);
  store_u32(finance_span, 128U,
            static_cast<std::uint32_t>(tail.finance.total_maintenance), byte_swapped);
  append(output, finance);

  auto b922 = tail.b922;
  b922[0] = static_cast<std::byte>(tail.b922_flag);
  b922[1] = static_cast<std::byte>(tail.b923);
  store_u32(b922, 2, static_cast<std::uint32_t>(tail.b924), byte_swapped);
  b922[6] = static_cast<std::byte>(tail.b928);
  b922[7] = static_cast<std::byte>(tail.b929);
  b922[8] = static_cast<std::byte>(tail.b92a);
  b922[9] = static_cast<std::byte>(tail.b92b);
  b922[10] = static_cast<std::byte>(tail.b92c);
  b922[11] = static_cast<std::byte>(tail.b92d);
  append(output, b922);
  auto b92e = tail.b92e;
  b92e[0] = static_cast<std::byte>(tail.b92e_counter);
  for (std::size_t index = 0; index < tail.b944_words.size(); ++index) {
    store_u16(b92e, 0x16U + index * 2U, tail.b944_words[index], byte_swapped);
  }
  append(output, b92e);

  auto parking = tail.parking_b958;
  store_u16(parking, 0, static_cast<std::uint16_t>(tail.parking_connected),
            byte_swapped);
  for (std::size_t index = 0; index < tail.parking_entries.size(); ++index) {
    store_u16(parking, 2U + index * 2U, tail.parking_entries[index], byte_swapped);
  }
  append(output, parking);
  auto bd5a = tail.bd5a;
  store_u16(bd5a, 0, tail.bd5a_count, byte_swapped);
  for (std::size_t index = 0; index < tail.bd5c_entries.size(); ++index) {
    store_u16(bd5a, 2U + index * 2U, tail.bd5c_entries[index], byte_swapped);
  }
  append(output, bd5a);
  for (const auto& stair : tail.stairs_bd70) {
    auto exact = stair.exact_bytes;
    exact[0] = static_cast<std::byte>(stair.used);
    exact[1] = static_cast<std::byte>(stair.shape);
    store_u16(exact, 2, stair.x, byte_swapped);
    exact[4] = static_cast<std::byte>(stair.floor);
    exact[5] = static_cast<std::byte>(stair.byte_5);
    store_u16(exact, 6, stair.word_6, byte_swapped);
    store_u16(exact, 8, stair.word_8, byte_swapped);
    append(output, exact);
  }
  for (const auto& route : tail.routes_bff0) append(output, route);
  append(output, tail.cf10);
  for (const auto word : tail.cf88_words) {
    std::array<std::byte, 2> exact{};
    store_u16(exact, 0, word, false);
    append(output, exact);
  }
  for (const auto& record : tail.cf9c_records) append(output, record);
  for (const auto& record : tail.db9c_records) append(output, record);
  for (const auto dword : tail.dbfc_dwords) {
    std::array<std::byte, 4> exact{};
    store_u32(exact, 0, dword, false);
    append(output, exact);
  }
  for (const auto& record : tail.dc24_records) append(output, record);
  auto dce4_or_dd34 = tail.dce4_or_dd34;
  if (version >= 0x23U) {
    if (tail.version_23_dce4.size() != 0x50U) {
      throw OriginalTdtError(OriginalTdtStatus::short_transfer,
                             "The SimTower revision-0x23 dce4 block is short");
    }
    auto dce4 = tail.version_23_dce4;
    for (std::size_t index = 0; index < tail.dce4_person_indices.size();
         ++index) {
      store_u32(dce4, index * 4U,
                static_cast<std::uint32_t>(tail.dce4_person_indices[index]),
                byte_swapped);
    }
    append(output, dce4);
  } else {
    for (std::size_t index = 0; index < 10U; ++index) {
      store_u32(dce4_or_dd34, index * 4U,
                static_cast<std::uint32_t>(tail.dce4_person_indices[index]),
                byte_swapped);
    }
  }
  append(output, dce4_or_dd34);
  append(output, tail.dynamic_dd5c);
  append(output, tail.dynamic_dd60);
  append(output, tail.dynamic_dd64);
  if (version >= 0x18U) append(output, tail.version_18_dd6c);
  const auto append_link_names = [&output](
                                     const std::vector<OriginalTdtLinkName>& names,
                                     std::size_t count) {
    static constexpr OriginalTdtLinkName empty{};
    for (std::size_t index = 0; index < count; ++index) {
      append(output, index < names.size() ? names[index].exact_bytes
                                          : empty.exact_bytes);
    }
  };
  append_link_names(document.person_link_names,
                    document.header.person_link_count);
  append_link_names(document.tenant_link_names,
                    document.header.tenant_link_count);
  append(output, document.trailing_bytes);
  return output;
}

std::vector<std::byte> serialize_original_tdt_game_save(
    const OriginalTdtDocument& document) {
  // 10d0:0b3a initializes its local version word to 0x2400 on every write.
  // Only reads can delegate to 10d0:1518, so saving always upgrades a legacy
  // or opposite-endian document to the current little-endian stream.
  return serialize_original_tdt(normalize_for_original_game_save(document));
}

}  // namespace simtower
