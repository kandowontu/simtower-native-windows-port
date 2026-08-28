#include "original_tdt.hpp"
#include "original_tdt_file.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<std::byte> read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  assert(stream.good());
  const std::vector<char> characters(
      (std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  std::vector<std::byte> bytes(characters.size());
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    bytes[index] = static_cast<std::byte>(static_cast<unsigned char>(characters[index]));
  }
  return bytes;
}

struct Expected {
  std::string_view name;
  std::size_t tenant_count;
  std::size_t tail_offset;
  std::uint32_t people_count;
  std::uint16_t rating;
  std::int32_t balance;
  std::uint16_t frame;
  std::int32_t day;
  std::uint16_t lobby;
  std::uint16_t view_x;
  std::uint16_t view_y;
  std::size_t built_elevators;
  std::size_t elevator_floor_records;
  std::size_t after_elevators;
  std::size_t trailing_bytes;
  std::int32_t total_population;
  std::int32_t total_income;
  std::int32_t total_maintenance;
  std::int16_t parking_connected;
  std::uint16_t bd5a_count;
  std::uint16_t person_link_count;
  std::uint16_t tenant_link_count;
};

std::string link_name_text(const simtower::OriginalTdtLinkName& name) {
  std::string text;
  for (const auto byte : name.exact_bytes) {
    const auto value = std::to_integer<unsigned char>(byte);
    if (value == 0U) break;
    text.push_back(static_cast<char>(value));
  }
  return text;
}

void reverse_word(std::vector<std::byte>& bytes, std::size_t offset) {
  std::swap(bytes[offset], bytes[offset + 1U]);
}

void reverse_dword(std::vector<std::byte>& bytes, std::size_t offset) {
  std::swap(bytes[offset], bytes[offset + 3U]);
  std::swap(bytes[offset + 1U], bytes[offset + 2U]);
}

void reverse_words(std::vector<std::byte>& bytes, std::size_t offset,
                   std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    reverse_word(bytes, offset + index * 2U);
  }
}

void reverse_dwords(std::vector<std::byte>& bytes, std::size_t offset,
                    std::size_t count) {
  for (std::size_t index = 0; index < count; ++index) {
    reverse_dword(bytes, offset + index * 4U);
  }
}

std::vector<std::byte> make_opposite_v24_fixture(
    const simtower::OriginalTdtDocument& document) {
  // Independently walk the exact revision-0x24 stream and apply the reversal
  // sites recovered from 10d0:1518. This fixture deliberately leaves cf88,
  // dbfc, dc24, and the last route dword untouched because the original does.
  auto bytes = simtower::serialize_original_tdt(document);
  assert(document.header.format_version == 0x24U);
  assert(!document.header.byte_swapped);
  assert(document.person_link_names.empty());
  assert(document.tenant_link_names.empty());
  assert(document.trailing_bytes.empty());

  bytes[0] = std::byte{0x24};
  bytes[1] = std::byte{0x00};
  std::size_t position = 2U;
  reverse_word(bytes, position); position += 2U;
  reverse_dwords(bytes, position, 4U); position += 16U;
  reverse_word(bytes, position); position += 2U;
  reverse_dword(bytes, position); position += 4U;
  reverse_word(bytes, position); position += 2U;
  reverse_word(bytes, position); position += 2U;
  reverse_words(bytes, position, 4U); position += 8U;
  reverse_words(bytes, position, 2U); position += 4U;
  reverse_words(bytes, position, 8U); position += 16U;
  reverse_word(bytes, position); position += 2U;
  reverse_word(bytes, position); position += 2U;
  reverse_words(bytes, position, 2U); position += 4U;
  reverse_dword(bytes, position); position += 4U;
  reverse_words(bytes, position, 245U); position += 490U;
  assert(position == 560U);

  for (const auto& floor : document.floors) {
    reverse_words(bytes, position, 3U);
    position += 6U;
    for (std::size_t index = 0; index < floor.tenants.size(); ++index) {
      reverse_word(bytes, position);
      reverse_word(bytes, position + 2U);
      reverse_word(bytes, position + 6U);
      reverse_dword(bytes, position + 8U);
      // Any source byte produces zero in runtime at 10d0:1b4d-1b62. A
      // nonzero asymmetric byte makes that destructive quirk observable.
      bytes[position + 17U] = std::byte{0xab};
      position += 18U;
    }
    reverse_words(bytes, position,
                  simtower::OriginalTdtFloor::kIndexCapacity);
    position += simtower::OriginalTdtFloor::kIndexCapacity * 2U;
  }

  reverse_dword(bytes, position);
  position += 4U;
  for (std::size_t index = 0; index < document.people.size(); ++index) {
    reverse_word(bytes, position + 2U);
    reverse_word(bytes, position + 10U);
    reverse_word(bytes, position + 12U);
    reverse_word(bytes, position + 14U);
    position += 16U;
  }
  for (std::size_t index = 0; index < document.retail.size(); ++index) {
    reverse_words(bytes, position + 12U, 3U);
    position += 18U;
  }
  for (const auto& elevator : document.elevators) {
    reverse_word(bytes, position + 0x3cU);
    reverse_word(bytes, position + 0x3eU);
    position += 0xc2U;
    if (elevator.used == 0U) continue;

    const std::size_t payload_offset = position;
    reverse_dwords(bytes, position, 0x1e0U / 4U);
    position += 0x1e0U + 0x78U + 0x78U;
    std::size_t unmapped_floors{};
    if (elevator.bottom_floor <= elevator.top_floor) {
      for (std::int16_t floor = elevator.bottom_floor;
           floor <= elevator.top_floor; ++floor) {
        if (simtower::original_elevator_floor_record_index(
                elevator.type, elevator.bottom_floor, elevator.top_floor,
                floor) < 0) {
          ++unmapped_floors;
        }
      }
    }
    if ((unmapped_floors & 1U) != 0U) {
      reverse_dwords(bytes, payload_offset + 0x190U, 80U);
    }
    for (std::size_t index = 0; index < elevator.floor_records.size(); ++index) {
      reverse_dwords(bytes, position + 4U, 80U);
      position += 0x144U;
    }
    for (std::size_t index = 0; index < elevator.car_records.size(); ++index) {
      reverse_word(bytes, position + 8U);
      reverse_word(bytes, position + 10U);
      reverse_dwords(bytes, position + 16U, 42U);
      position += 0x15aU;
    }
  }

  reverse_dwords(bytes, position, 22U); position += 0x58U;
  reverse_dwords(bytes, position, 33U); position += 0x84U;
  reverse_dword(bytes, position + 2U); position += 0x0cU;
  reverse_words(bytes, position + 0x16U, 10U); position += 0x2aU;
  reverse_words(bytes, position, 0x402U / 2U); position += 0x402U;
  reverse_words(bytes, position, 0x16U / 2U); position += 0x16U;
  for (std::size_t index = 0; index < 64U; ++index) {
    reverse_word(bytes, position + 2U);
    reverse_word(bytes, position + 6U);
    reverse_word(bytes, position + 8U);
    position += 10U;
  }
  for (std::size_t index = 0; index < 8U; ++index) {
    reverse_dwords(bytes, position, 0x1e0U / 4U);
    position += 0x1e4U;
  }
  position += 0x78U;  // cf10
  position += 0x14U;  // cf88: deliberately not reversed by 10d0:1518.
  for (std::size_t index = 0; index < 512U; ++index) {
    reverse_dword(bytes, position + 2U);
    position += 6U;
  }
  for (std::size_t index = 0; index < 16U; ++index) {
    reverse_dword(bytes, position);
    position += 6U;
  }
  position += 0x28U;  // dbfc: also transferred without reversal.
  position += 16U * 0x0cU;  // dc24
  reverse_dwords(bytes, position, 20U); position += 0x50U;
  reverse_words(bytes, position, 20U); position += 0x28U;
  reverse_words(bytes, position, 0x1102U / 2U); position += 0x1102U;
  reverse_words(bytes, position, 0x842U / 2U); position += 0x842U;
  reverse_words(bytes, position, 0xca2U / 2U); position += 0xca2U;
  reverse_words(bytes, position + 2U, 3U); position += 8U;
  assert(position == bytes.size());
  return bytes;
}

void make_legacy_revision(simtower::OriginalTdtDocument& document,
                          std::uint8_t version) {
  assert(version >= 0x17U && version < 0x23U);
  document.header.exact_bytes.erase(document.header.exact_bytes.begin() + 58U,
                                    document.header.exact_bytes.begin() + 60U);
  if (version < 0x20U) {
    document.header.exact_bytes.erase(document.header.exact_bytes.begin() + 26U,
                                      document.header.exact_bytes.begin() + 28U);
    document.header.version_20_word = 0U;
  }
  document.header.raw_version = static_cast<std::uint16_t>(version) << 8U;
  document.header.format_version = version;
  document.header.byte_swapped = false;
  document.header.tenant_link_count = 0U;
  document.post_elevator.version_23_dce4.clear();
  document.tenant_link_names.clear();
  if (version < 0x18U) document.post_elevator.version_18_dd6c.clear();
}

}  // namespace

int main(int argc, char** argv) {
  // Direct 1090:0014/0074, 1170:0014/004e, and 1238:0073 storage-ownership
  // coverage: fixed native banks replace the exact movable Win16 blocks.
  assert(simtower::original_elevator_storage_contract() ==
         (simtower::OriginalOwnedStorageContract{24U, 0x345aU, 0x0040U}));
  assert(simtower::original_medical_storage_contract() ==
         (simtower::OriginalOwnedStorageContract{1U, 0x009aU, 0x0040U}));
  assert(simtower::original_people_storage_contract() ==
         (simtower::OriginalOwnedStorageContract{1U, 0x1000U, 0x0040U}));
  // Direct 1000:2140 coverage: a create failure has no owned target, while
  // every later failed-save operation deletes the partial target.
  assert(!simtower::original_failed_save_deletes_target(
      simtower::OriginalTdtFileOperation::create));
  assert(simtower::original_failed_save_deletes_target(
      simtower::OriginalTdtFileOperation::open));
  assert(simtower::original_failed_save_deletes_target(
      simtower::OriginalTdtFileOperation::read));
  assert(simtower::original_failed_save_deletes_target(
      simtower::OriginalTdtFileOperation::write));
  assert(argc == 6);
  constexpr Expected expected[] = {
      {"s2b-exported.TDT", 2, 23876, 0, 1, 20000, 27, 0, 1, 1180, 3636,
       0, 0, 37752, 8194, 0, 0, 0, 0, 0, 0, 0},
      {"ElDlux.TDT", 1412, 49256, 15360, 5, 277677, 169, 309, 2, 1005, 3180,
       23, 340, 499276, 0, 10540, 57200, 49050, 70, 1, 0, 3},
      {"RoyalA.TDT", 2480, 68480, 22784, 6, 1485527, 1706, 206, 3, 210, 3750,
       23, 313, 628536, 0, 16558, 153390, 53720, 240, 7, 0, 0},
      {"SimEmpire.TDT", 2714, 72692, 33024, 6, 2881961, 482, 161, 3, 480, 720,
       24, 284, 790680, 0, 20003, 132760, 64660, 278, 7, 0, 3},
  };
  for (int argument = 1; argument < 5; ++argument) {
    const auto bytes = read_file(argv[argument]);
    const auto document = simtower::parse_original_tdt(bytes);
    const auto& want = expected[argument - 1];
    assert(std::filesystem::path(argv[argument]).filename().string() == want.name);
    assert(document.header.raw_version == 0x2400);
    assert(document.header.format_version == 0x24);
    assert(!document.header.byte_swapped);
    assert(document.header.exact_bytes.size() == 560);
    assert(document.header.rating == want.rating);
    assert(document.header.balance == want.balance);
    assert(document.header.frame_time == want.frame);
    assert(document.header.current_day == want.day);
    assert(document.header.lobby_height == want.lobby);
    assert(document.header.view_x == want.view_x);
    assert(document.header.view_y == want.view_y);
    assert(document.header.person_link_count == want.person_link_count);
    assert(document.header.tenant_link_count == want.tenant_link_count);
    assert(document.person_link_names.size() == want.person_link_count);
    assert(document.tenant_link_names.size() == want.tenant_link_count);
    std::size_t tenant_count = 0;
    for (const auto& floor : document.floors) {
      tenant_count += floor.tenants.size();
    }
    assert(tenant_count == want.tenant_count);
    assert(document.people_offset - 4 == want.tail_offset);
    // Direct 1238:00cf coverage: each fixture's persisted count resizes the
    // native people vector to the exact count of sixteen-byte records.
    assert(document.people_count == want.people_count);
    assert(document.people.size() == want.people_count);
    assert(document.retail_offset == document.people_offset + want.people_count * 16ULL);
    assert(document.elevator_table_offset == document.retail_offset + 0x2400);
    assert(document.elevator_table_offset <= bytes.size());
    std::size_t built_elevators = 0;
    std::size_t elevator_floor_records = 0;
    for (const auto& elevator : document.elevators) {
      assert(elevator.file_header_size == 194);
      assert(elevator.exact_file_header.size() == elevator.file_header_size);
      if (elevator.used != 0) {
        ++built_elevators;
        elevator_floor_records += elevator.floor_records.size();
        assert(elevator.payload_size ==
               3488 + elevator.floor_records.size() * 324ULL);
      } else {
        assert(elevator.payload_size == 0);
      }
    }
    assert(built_elevators == want.built_elevators);
    assert(elevator_floor_records == want.elevator_floor_records);
    if (document.after_elevators_offset != want.after_elevators) {
      std::fprintf(stderr, "%.*s: after elevators got %zu, wanted %zu\n",
                   static_cast<int>(want.name.size()), want.name.data(),
                   document.after_elevators_offset, want.after_elevators);
      assert(false);
    }
    if (want.name == "ElDlux.TDT") {
      const auto& first = document.elevators[0];
      assert(first.used == 1);
      assert(first.type == 0);
      assert(first.capacity == 42);
      assert(first.cars == 8);
      assert(first.x == 199);
      assert(first.bottom_floor == 1);
      assert(first.top_floor == 84);
      assert(first.floor_records.size() == 15);
    }
    assert(document.post_elevator.file_offset == document.after_elevators_offset);
    assert(document.post_elevator.end_offset ==
           document.after_elevators_offset + 19204);
    assert(document.trailing_bytes.size() == want.trailing_bytes);
    const auto find_name_bytes =
        (document.person_link_names.size() +
         document.tenant_link_names.size()) *
        16U;
    assert(document.post_elevator.end_offset + find_name_bytes +
               document.trailing_bytes.size() ==
           bytes.size());
    assert(document.post_elevator.finance.total_population == want.total_population);
    assert(document.post_elevator.finance.total_income == want.total_income);
    assert(document.post_elevator.finance.total_maintenance ==
           want.total_maintenance);
    assert(document.post_elevator.parking_connected == want.parking_connected);
    assert(document.post_elevator.bd5a_count == want.bd5a_count);
    if (want.name == "ElDlux.TDT") {
      assert(link_name_text(document.tenant_link_names[0]) == "Dave's Tavern");
      assert(link_name_text(document.tenant_link_names[1]) == "Dan's Dew Bar ");
      assert(link_name_text(document.tenant_link_names[2]) == "Mama's Bistro");
    } else if (want.name == "SimEmpire.TDT") {
      assert(link_name_text(document.tenant_link_names[0]) == "Jedi Temple");
      assert(link_name_text(document.tenant_link_names[1]) == "Jedi Council");
      assert(link_name_text(document.tenant_link_names[2]) == "Jedi Order");
    }
    if (want.name == "ElDlux.TDT") {
      const auto& first_stair = document.post_elevator.stairs_bd70[0];
      assert(first_stair.used == 1);
      assert(first_stair.shape == 2);
      assert(first_stair.x == 122);
      assert(first_stair.floor == 10);
    }
    assert(simtower::serialize_original_tdt(document) == bytes);
    assert(simtower::serialize_original_tdt_lossless(document) == bytes);

    if (want.name == "ElDlux.TDT") {
      auto edited = document;
      edited.header.balance += 1234;
      edited.elevators[0].x += 3;
      edited.post_elevator.finance.total_income += 500;
      edited.post_elevator.parking_connected += 1;
      edited.post_elevator.stairs_bd70[0].x += 2;
      edited.people[0].exact_bytes[0] ^= std::byte{0x5a};
      edited.retail[0].exact_bytes[5] ^= std::byte{0x33};
      const auto rebuilt = simtower::serialize_original_tdt(edited);
      const auto reparsed = simtower::parse_original_tdt(rebuilt);
      assert(reparsed.header.balance == edited.header.balance);
      assert(reparsed.elevators[0].x == edited.elevators[0].x);
      assert(reparsed.post_elevator.finance.total_income ==
             edited.post_elevator.finance.total_income);
      assert(reparsed.post_elevator.parking_connected ==
             edited.post_elevator.parking_connected);
      assert(reparsed.post_elevator.stairs_bd70[0].x ==
             edited.post_elevator.stairs_bd70[0].x);
      assert(reparsed.people[0].exact_bytes == edited.people[0].exact_bytes);
      assert(reparsed.retail[0].exact_bytes == edited.retail[0].exact_bytes);
    }
  }

  // Direct 10d0:2a8e predicate coverage: no active document and an allocated
  // floor-10 slot with zero tenants skip the prompt; the first live tenant
  // enters its save/discard/cancel transaction.
  const auto fresh = simtower::make_original_new_tdt();
  // Direct 1180:0000/1188:0000/1238:001e/00b0 initialization coverage: all
  // sixteen entertainment records receive FE floor sentinels, the revision-23
  // eighty-byte name-link backing begins GMEM_ZEROINIT clean, and the reset
  // path grows the complete 512-person bank with zero-initialized records.
  assert(fresh.people.size() == 512U && fresh.people_count == 512U);
  for (const auto& record : fresh.post_elevator.dc24_records) {
    assert(record[0] == std::byte{0xfe} &&
           record[1] == std::byte{0xfe});
  }
  assert(fresh.post_elevator.version_23_dce4.size() == 0x50U);
  assert(std::all_of(fresh.post_elevator.version_23_dce4.begin(),
                     fresh.post_elevator.version_23_dce4.end(),
                     [](std::byte value) { return value == std::byte{0}; }));
  assert(!simtower::original_tower_transition_requires_confirmation(nullptr));
  assert(!simtower::original_tower_transition_requires_confirmation(&fresh));
  auto prompt_tower = fresh;
  prompt_tower.floors[10].tenants.emplace_back();
  assert(simtower::original_tower_transition_requires_confirmation(
      &prompt_tower));
  const auto fresh_bytes = simtower::serialize_original_tdt(fresh);
  assert(simtower::serialize_original_tdt_game_save(fresh) == fresh_bytes);
  assert(fresh_bytes.size() == 65112U);
  const auto reparsed_fresh = simtower::parse_original_tdt(fresh_bytes);
  assert(reparsed_fresh.header.raw_version == 0x2400U);
  assert(reparsed_fresh.header.rating == 1U);
  assert(reparsed_fresh.header.balance == 20000);
  assert(reparsed_fresh.header.other_income == 0);
  assert(reparsed_fresh.header.construction_costs == 0);
  assert(reparsed_fresh.header.last_quarter_money == 20000);
  // Direct 1200:0000 coverage: fresh simulation timing begins at 0x09e5.
  assert(reparsed_fresh.header.frame_time == 0x09e5U);
  assert(reparsed_fresh.header.current_day == 0);
  assert(reparsed_fresh.header.lobby_height == 0);
  assert(reparsed_fresh.header.view_x == 0);
  assert(reparsed_fresh.header.view_y == 0);
  assert(reparsed_fresh.people_count == 512U);
  assert(reparsed_fresh.people.size() == 512U);
  for (const auto& person : reparsed_fresh.people) {
    for (const auto byte : person.exact_bytes) {
      assert(byte == std::byte{0});
    }
  }
  for (const auto& floor : reparsed_fresh.floors) {
    assert(floor.left_edge == 0U);
    assert(floor.right_edge == 0U);
    assert(floor.tenants.empty());
  }
  // Direct 11a8:0000 coverage: the 0x2400 Retail allocation and all three
  // commercial-service banks begin zero-filled before 00b4 writes sentinels.
  assert(std::ranges::all_of(reparsed_fresh.post_elevator.dynamic_dd5c,
                             [](std::byte value) {
                               return value == std::byte{0};
                             }));
  assert(std::ranges::all_of(reparsed_fresh.post_elevator.dynamic_dd60,
                             [](std::byte value) {
                               return value == std::byte{0};
                             }));
  assert(std::ranges::all_of(reparsed_fresh.post_elevator.dynamic_dd64,
                             [](std::byte value) {
                               return value == std::byte{0};
                             }));
  // Direct 11a8:00b4 coverage: every one of the 512 service slots starts
  // ff/00/03 followed by fifteen zero bytes.
  for (const auto& retail : reparsed_fresh.retail) {
    assert(retail.exact_bytes[0] == std::byte{0xff});
    assert(retail.exact_bytes[1] == std::byte{0});
    assert(retail.exact_bytes[2] == std::byte{3});
    for (std::size_t byte = 3U; byte < retail.exact_bytes.size(); ++byte) {
      assert(retail.exact_bytes[byte] == std::byte{0});
    }
  }
  for (const auto& elevator : reparsed_fresh.elevators) {
    assert(elevator.used == 0U);
  }
  // Direct 1060:0000 coverage: all three ten-category finance bands and
  // their totals are zero on a fresh tower.
  const auto& fresh_finance = reparsed_fresh.post_elevator.finance;
  assert(std::ranges::all_of(fresh_finance.population_by_category,
                             [](std::int32_t value) { return value == 0; }));
  assert(std::ranges::all_of(fresh_finance.income_by_category,
                             [](std::int32_t value) { return value == 0; }));
  assert(std::ranges::all_of(fresh_finance.maintenance_by_category,
                             [](std::int32_t value) { return value == 0; }));
  assert(fresh_finance.total_population == 0);
  assert(fresh_finance.total_income == 0);
  assert(fresh_finance.total_maintenance == 0);
  // Direct 1148:0000 coverage: the selected Elevator sentinel starts at -1.
  assert(reparsed_fresh.post_elevator.b924 == -1);
  assert(reparsed_fresh.post_elevator.b929 == 0U);
  assert(reparsed_fresh.post_elevator.b92a == 0U);
  assert(reparsed_fresh.post_elevator.b92b == 0U);
  // Direct 11f0:0000 coverage: the pending-construction ring starts empty at
  // head zero.
  assert(reparsed_fresh.post_elevator.b92e_counter == 0U);
  assert(reparsed_fresh.post_elevator.b92e[1] == std::byte{0});

  // The rating prerequisite/message bytes at DS:b929..b92b occupy the
  // previously opaque middle of the persisted twelve-byte b922 block.
  auto rating_flags = fresh;
  rating_flags.post_elevator.b929 = 0x91U;
  rating_flags.post_elevator.b92a = 0xa2U;
  rating_flags.post_elevator.b92b = 0xb3U;
  const auto reparsed_rating_flags = simtower::parse_original_tdt(
      simtower::serialize_original_tdt(rating_flags));
  assert(reparsed_rating_flags.post_elevator.b929 == 0x91U);
  assert(reparsed_rating_flags.post_elevator.b92a == 0xa2U);
  assert(reparsed_rating_flags.post_elevator.b92b == 0xb3U);
  // Direct 10f8:0000 coverage: all ten Security/Housekeeping slots start ffff.
  for (const auto word : reparsed_fresh.post_elevator.cf88_words) {
    assert(word == 0xffffU);
  }
  // Direct 1198:016e coverage: every Parking lookup begins unused (floor ff).
  for (const auto& record : reparsed_fresh.post_elevator.cf9c_records) {
    assert(record[0] == std::byte{0xff});
  }
  // Direct 1170:00e1 coverage: every medical-service lookup starts ffff.
  for (const auto dword : reparsed_fresh.post_elevator.dbfc_dwords) {
    assert(dword == 0x0000ffffU);
  }
  for (const auto& record : reparsed_fresh.post_elevator.dc24_records) {
    assert(record[0] == std::byte{0xfe});
    assert(record[1] == std::byte{0xfe});
  }
  assert(reparsed_fresh.trailing_bytes.empty());
  assert(reparsed_fresh.person_link_names.empty());
  assert(reparsed_fresh.tenant_link_names.empty());
  assert(simtower::serialize_original_tdt(reparsed_fresh) == fresh_bytes);

  // Direct 1188:02ea/043d coverage. Legacy exporters store each active Find
  // name
  // as a 256-byte Pascal record. The original consumes the extra 240 bytes,
  // shifts the counted text over the length byte, and saves it thereafter as
  // the ordinary sixteen-byte NUL-terminated Windows record.
  {
    auto named = fresh;
    named.header.person_link_count = 1U;
    named.header.tenant_link_count = 1U;
    named.person_link_names.resize(1U);
    named.tenant_link_names.resize(1U);
    constexpr std::string_view person_name = "Alice";
    constexpr std::string_view tenant_name = "Cafe";
    for (std::size_t index = 0; index < person_name.size(); ++index) {
      named.person_link_names[0].exact_bytes[index] =
          static_cast<std::byte>(person_name[index]);
    }
    for (std::size_t index = 0; index < tenant_name.size(); ++index) {
      named.tenant_link_names[0].exact_bytes[index] =
          static_cast<std::byte>(tenant_name[index]);
    }

    const auto normalized_names = simtower::serialize_original_tdt(named);
    const std::size_t names_offset = normalized_names.size() - 32U;
    std::vector<std::byte> legacy_names(normalized_names.begin(),
                                        normalized_names.begin() + names_offset);
    const auto append_pascal_name = [&legacy_names](std::string_view text,
                                                    std::byte padding) {
      assert(!text.empty() && text.size() < 16U);
      const std::size_t offset = legacy_names.size();
      legacy_names.resize(offset + 256U, padding);
      legacy_names[offset] = static_cast<std::byte>(text.size());
      for (std::size_t index = 0; index < text.size(); ++index) {
        legacy_names[offset + 1U + index] =
            static_cast<std::byte>(text[index]);
      }
    };
    append_pascal_name(person_name, std::byte{0xa5});
    append_pascal_name(tenant_name, std::byte{0x5a});

    const auto loaded = simtower::parse_original_tdt(legacy_names);
    assert(link_name_text(loaded.person_link_names[0]) == person_name);
    assert(link_name_text(loaded.tenant_link_names[0]) == tenant_name);
    for (std::size_t index = person_name.size() + 1U; index < 16U; ++index) {
      assert(loaded.person_link_names[0].exact_bytes[index] == std::byte{0});
    }
    for (std::size_t index = tenant_name.size() + 1U; index < 16U; ++index) {
      assert(loaded.tenant_link_names[0].exact_bytes[index] == std::byte{0});
    }
    assert(loaded.trailing_bytes.empty());
    assert(simtower::serialize_original_tdt_lossless(loaded) == legacy_names);
    assert(simtower::serialize_original_tdt(loaded) == normalized_names);

    legacy_names.pop_back();
    try {
      (void)simtower::parse_original_tdt(legacy_names);
      assert(false);
    } catch (const simtower::OriginalTdtError& error) {
      assert(error.status() == simtower::OriginalTdtStatus::short_transfer);
    }
  }

  // Direct 1188:007e/01be ownership-table coverage and the same loader's
  // 1188:061c/06dc b402/b404 rebuild. Native vectors replace the two twenty-
  // pointer allocate/reset/free tables; repeated keys collapse into their
  // first slot, while the later LSTRCPY overwrites only through its NUL and
  // therefore retains the older name's remaining bytes.
  {
    const auto make_name = [](std::string_view text) {
      assert(text.size() < 16U);
      simtower::OriginalTdtLinkName result{};
      for (std::size_t index = 0; index < text.size(); ++index) {
        result.exact_bytes[index] = static_cast<std::byte>(text[index]);
      }
      return result;
    };
    auto duplicates = fresh;
    duplicates.header.person_link_count = 3U;
    duplicates.post_elevator.dce4_person_indices[0] = 7;
    duplicates.post_elevator.dce4_person_indices[1] = 7;
    duplicates.post_elevator.dce4_person_indices[2] = 9;
    duplicates.person_link_names = {
        make_name("LongName"), make_name("B"), make_name("Carol")};
    duplicates.header.tenant_link_count = 3U;
    duplicates.post_elevator.dce4_or_dd34[0] = std::byte{0x34};
    duplicates.post_elevator.dce4_or_dd34[1] = std::byte{0x12};
    duplicates.post_elevator.dce4_or_dd34[2] = std::byte{0x34};
    duplicates.post_elevator.dce4_or_dd34[3] = std::byte{0x12};
    duplicates.post_elevator.dce4_or_dd34[4] = std::byte{0x78};
    duplicates.post_elevator.dce4_or_dd34[5] = std::byte{0x56};
    duplicates.tenant_link_names = {
        make_name("Restaurant"), make_name("X"), make_name("Cinema")};

    const auto duplicate_bytes =
        simtower::serialize_original_tdt(duplicates);
    const auto compacted = simtower::parse_original_tdt(duplicate_bytes);
    assert(compacted.header.person_link_count == 2U);
    assert(compacted.post_elevator.dce4_person_indices[0] == 7);
    assert(compacted.post_elevator.dce4_person_indices[1] == 9);
    assert(compacted.person_link_names.size() == 2U);
    assert(link_name_text(compacted.person_link_names[0]) == "B");
    assert(compacted.person_link_names[0].exact_bytes[2] == std::byte{'n'});
    assert(link_name_text(compacted.person_link_names[1]) == "Carol");
    assert(compacted.header.tenant_link_count == 2U);
    assert(compacted.post_elevator.dce4_or_dd34[0] == std::byte{0x34});
    assert(compacted.post_elevator.dce4_or_dd34[1] == std::byte{0x12});
    assert(compacted.post_elevator.dce4_or_dd34[2] == std::byte{0x78});
    assert(compacted.post_elevator.dce4_or_dd34[3] == std::byte{0x56});
    assert(compacted.tenant_link_names.size() == 2U);
    assert(link_name_text(compacted.tenant_link_names[0]) == "X");
    assert(compacted.tenant_link_names[0].exact_bytes[2] == std::byte{'s'});
    assert(link_name_text(compacted.tenant_link_names[1]) == "Cinema");
    assert(simtower::serialize_original_tdt_lossless(compacted) ==
           duplicate_bytes);
    const auto reparsed_compacted = simtower::parse_original_tdt(
        simtower::serialize_original_tdt(compacted));
    assert(reparsed_compacted.header.person_link_count == 2U);
    assert(reparsed_compacted.header.tenant_link_count == 2U);
  }

  // 10d0:0b3a never preserves the imported family on write. Exercise an
  // independently byte-reversed fixture with asymmetric values across every
  // raw record family and require the game-facing serializer to recreate the
  // original current/little-endian runtime stream.
  {
    auto runtime = fresh;
    runtime.header.rating = 0x1234U;
    runtime.header.balance = 0x10203040;
    runtime.header.view_x = 0x5678U;
    runtime.header.exact_bytes[70U] = std::byte{0x34};
    runtime.header.exact_bytes[71U] = std::byte{0x12};

    simtower::OriginalTdtTenant tenant{};
    tenant.left = 0x1122U;
    tenant.right = 0x3344U;
    tenant.type = 7;
    tenant.status = 9U;
    tenant.variant = 0x66U;
    tenant.preserved_07_to_0f = {
        std::byte{0x55}, std::byte{0x44}, std::byte{0x33},
        std::byte{0x22}, std::byte{0x11}, std::byte{0x7a},
        std::byte{0x7b}, std::byte{0x7c}, std::byte{0x7d}};
    tenant.rent_rate = 0x88U;
    tenant.subtype = 0U;
    tenant.exact_bytes = {
        std::byte{0x22}, std::byte{0x11}, std::byte{0x44},
        std::byte{0x33}, std::byte{7}, std::byte{9},
        std::byte{0x66}, std::byte{0x55}, std::byte{0x44},
        std::byte{0x33}, std::byte{0x22}, std::byte{0x11},
        std::byte{0x7a}, std::byte{0x7b}, std::byte{0x7c},
        std::byte{0x7d}, std::byte{0x88}, std::byte{0}};
    runtime.floors[0].tenants.push_back(tenant);
    runtime.floors[0].tenant_index[0] = 0x7788U;
    runtime.people[0].exact_bytes[2] = std::byte{0x12};
    runtime.people[0].exact_bytes[3] = std::byte{0x34};
    runtime.people[0].exact_bytes[10] = std::byte{0x56};
    runtime.people[0].exact_bytes[11] = std::byte{0x78};
    runtime.retail[0].exact_bytes[12] = std::byte{0x9a};
    runtime.retail[0].exact_bytes[13] = std::byte{0xbc};
    auto& elevator = runtime.elevators[0];
    elevator.used = 1U;
    elevator.type = 0U;
    elevator.capacity = 0x2aU;
    elevator.cars = 1U;
    elevator.word_3c = 0x1357U;
    elevator.x = 0x2468U;
    elevator.bottom_floor = 10;
    elevator.top_floor = 11;  // One mapped and one unmapped compatibility pass.
    elevator.block_c2[0] = std::byte{0x04};
    elevator.block_c2[1] = std::byte{0x03};
    elevator.block_c2[2] = std::byte{0x02};
    elevator.block_c2[3] = std::byte{0x01};
    elevator.block_c2[0x190U] = std::byte{0x14};
    elevator.block_c2[0x191U] = std::byte{0x13};
    elevator.block_c2[0x192U] = std::byte{0x12};
    elevator.block_c2[0x193U] = std::byte{0x11};
    elevator.block_2a2[0] = std::byte{0x24};
    elevator.block_2a2[1] = std::byte{0x23};
    elevator.block_2a2[2] = std::byte{0x22};
    elevator.block_2a2[3] = std::byte{0x21};
    elevator.block_31a[0] = std::byte{0x34};
    elevator.block_31a[1] = std::byte{0x33};
    elevator.block_31a[2] = std::byte{0x32};
    elevator.block_31a[3] = std::byte{0x31};
    elevator.floor_records.resize(1U);
    elevator.floor_records[0].mapped_index = 9;
    elevator.floor_records[0].floor = 10;
    elevator.floor_records[0].exact_bytes[4] = std::byte{0x44};
    elevator.floor_records[0].exact_bytes[5] = std::byte{0x43};
    elevator.floor_records[0].exact_bytes[6] = std::byte{0x42};
    elevator.floor_records[0].exact_bytes[7] = std::byte{0x41};
    elevator.car_records[0].exact_bytes[8] = std::byte{0x52};
    elevator.car_records[0].exact_bytes[9] = std::byte{0x51};
    elevator.car_records[0].exact_bytes[16] = std::byte{0x64};
    elevator.car_records[0].exact_bytes[17] = std::byte{0x63};
    elevator.car_records[0].exact_bytes[18] = std::byte{0x62};
    elevator.car_records[0].exact_bytes[19] = std::byte{0x61};
    runtime.post_elevator.routes_bff0[0][0] = std::byte{0x04};
    runtime.post_elevator.routes_bff0[0][1] = std::byte{0x03};
    runtime.post_elevator.routes_bff0[0][2] = std::byte{0x02};
    runtime.post_elevator.routes_bff0[0][3] = std::byte{0x01};
    runtime.post_elevator.cf88_words[0] = 0x1234U;
    runtime.post_elevator.cf9c_records[0][2] = std::byte{0x44};
    runtime.post_elevator.cf9c_records[0][3] = std::byte{0x33};
    runtime.post_elevator.cf9c_records[0][4] = std::byte{0x22};
    runtime.post_elevator.cf9c_records[0][5] = std::byte{0x11};
    runtime.post_elevator.db9c_records[0][0] = std::byte{0x88};
    runtime.post_elevator.db9c_records[0][1] = std::byte{0x77};
    runtime.post_elevator.db9c_records[0][2] = std::byte{0x66};
    runtime.post_elevator.db9c_records[0][3] = std::byte{0x55};
    runtime.post_elevator.dbfc_dwords[0] = 0x11223344U;
    runtime.post_elevator.dynamic_dd5c[0] = std::byte{0xaa};
    runtime.post_elevator.dynamic_dd5c[1] = std::byte{0xbb};
    runtime.post_elevator.version_18_dd6c[2] = std::byte{0xcc};
    runtime.post_elevator.version_18_dd6c[3] = std::byte{0xdd};

    const auto expected_current = simtower::serialize_original_tdt(runtime);
    const auto opposite_bytes = make_opposite_v24_fixture(runtime);
    const auto opposite = simtower::parse_original_tdt(opposite_bytes);
    assert(!opposite.header.byte_swapped);
    assert(opposite.header.raw_version == 0x2400U);
    assert(opposite.header.rating == runtime.header.rating);
    assert(opposite.header.balance == runtime.header.balance);
    assert(opposite.floors[0].tenant_index[0] == 0x7788U);
    assert(opposite.floors[0].tenants[0].variant == 0x66U);
    assert(opposite.floors[0].tenants[0].preserved_07_to_0f[0] ==
           std::byte{0x55});
    assert(opposite.floors[0].tenants[0].subtype == 0U);
    // These are deliberate compatibility omissions, not generic endian reads.
    assert(opposite.post_elevator.cf88_words[0] == 0x1234U);
    assert(opposite.post_elevator.dbfc_dwords[0] == 0x11223344U);
    assert(opposite.people[0].exact_bytes == runtime.people[0].exact_bytes);
    assert(opposite.retail[0].exact_bytes == runtime.retail[0].exact_bytes);
    assert(opposite.elevators[0].block_c2 == runtime.elevators[0].block_c2);
    assert(opposite.elevators[0].block_2a2 == runtime.elevators[0].block_2a2);
    assert(opposite.elevators[0].block_31a == runtime.elevators[0].block_31a);
    assert(opposite.elevators[0].floor_records[0].exact_bytes ==
           runtime.elevators[0].floor_records[0].exact_bytes);
    assert(opposite.elevators[0].car_records[0].exact_bytes ==
           runtime.elevators[0].car_records[0].exact_bytes);
    assert(opposite.post_elevator.routes_bff0[0] ==
           runtime.post_elevator.routes_bff0[0]);
    assert(opposite.post_elevator.dynamic_dd5c ==
           runtime.post_elevator.dynamic_dd5c);
    assert(simtower::serialize_original_tdt_lossless(opposite) ==
           opposite_bytes);
    assert(simtower::serialize_original_tdt(opposite) == expected_current);
    const auto converted =
        simtower::serialize_original_tdt_game_save(opposite);
    assert(converted == expected_current);
    const auto converted_document = simtower::parse_original_tdt(converted);
    assert(!converted_document.header.byte_swapped);
    assert(converted_document.header.format_version == 0x24U);
    assert(converted_document.floors[0].tenants[0].subtype == 0U);
  }

  // Revision 0x18 covers all legacy migrations in this transfer family:
  // byte-wide floor indexes, split elevator headers, segmented car records,
  // the pre-0x19 active-car default, and the pre-0x23 dce4-only link table.
  {
    auto legacy = fresh;
    make_legacy_revision(legacy, 0x18U);
    legacy.header.person_link_count = 1U;
    legacy.post_elevator.dce4_person_indices[0] = 0x10203040;
    auto& elevator = legacy.elevators[0];
    elevator.used = 1U;
    elevator.type = 1U;
    elevator.capacity = 0xeeU;
    elevator.cars = 1U;
    elevator.bottom_floor = 0;
    elevator.top_floor = 0;
    elevator.floor_records.resize(1U);
    elevator.floor_records[0].mapped_index = 0;
    elevator.floor_records[0].floor = 0;
    for (std::size_t index = 0; index < elevator.car_records.size(); ++index) {
      auto& car = elevator.car_records[index].exact_bytes;
      car[0] = static_cast<std::byte>(index + 1U);
      car[0x0fU] = std::byte{0};
      car[0x10U] = std::byte{0x21};
      car[0xa0U] = std::byte{0xee};  // Omitted 24-byte legacy gap.
      car[0xb8U] = std::byte{0x43};
      car[0xdcU] = std::byte{0xdd};  // Omitted 6-byte legacy gap.
      car[0xe2U] = std::byte{0x65};
    }
    const auto legacy_bytes = simtower::serialize_original_tdt(legacy);
    const auto loaded = simtower::parse_original_tdt(legacy_bytes);
    assert(loaded.header.format_version == 0x18U);
    assert(loaded.elevators[0].file_header_size == 0xb4U);
    assert(loaded.elevators[0].capacity == 0x15U);
    assert(loaded.elevators[0].payload_size == 3572U);
    for (const auto& car : loaded.elevators[0].car_records) {
      assert(car.exact_bytes[0x0fU] == std::byte{1});
      assert(car.exact_bytes[0x10U] == std::byte{0x21});
      assert(car.exact_bytes[0xa0U] == std::byte{0});
      assert(car.exact_bytes[0xb8U] == std::byte{0x43});
      assert(car.exact_bytes[0xdcU] == std::byte{0});
      assert(car.exact_bytes[0xe2U] == std::byte{0x65});
    }
    const auto upgraded = simtower::serialize_original_tdt_game_save(loaded);
    const auto current = simtower::parse_original_tdt(upgraded);
    assert(current.header.raw_version == 0x2400U);
    assert(current.header.format_version == 0x24U);
    assert(!current.header.byte_swapped);
    assert(current.header.tenant_link_count == 0U);
    assert(current.post_elevator.dce4_person_indices[0] == 0x10203040);
    assert(current.elevators[0].file_header_size == 0xc2U);
    assert(current.elevators[0].capacity == 0x15U);
    assert(current.elevators[0].payload_size == 3812U);
  }

  // Direct 11f8:02ca coverage: the 0xb4e-byte runtime floor object reserves
  // 150 tenant records but only persists 94 lookup words. These capacities
  // are independent.
  {
    auto capacity = fresh;
    capacity.floors[0].tenants.resize(
        simtower::OriginalTdtFloor::kTenantCapacity);
    const auto capacity_bytes = simtower::serialize_original_tdt(capacity);
    const auto reparsed_capacity =
        simtower::parse_original_tdt(capacity_bytes);
    assert(reparsed_capacity.floors[0].tenants.size() == 150U);
    capacity.floors[0].tenants.emplace_back();
    try {
      (void)simtower::serialize_original_tdt(capacity);
      assert(false);
    } catch (const simtower::OriginalTdtError& error) {
      assert(error.status() == simtower::OriginalTdtStatus::malformed);
    }
  }

  // The original parser clamps corrupt b402 values to the physical table,
  // but the native writer must not emit a file that cannot represent its
  // declared active person-link count.
  {
    auto links = fresh;
    links.header.person_link_count = 21U;
    try {
      (void)simtower::serialize_original_tdt(links);
      assert(false);
    } catch (const simtower::OriginalTdtError& error) {
      assert(error.status() == simtower::OriginalTdtStatus::malformed);
    }

    links = fresh;
    links.header.raw_version = 0x2200U;
    links.header.format_version = 0x22U;
    links.header.exact_bytes.erase(links.header.exact_bytes.begin() + 58,
                                   links.header.exact_bytes.begin() + 60);
    links.header.person_link_count = 11U;
    try {
      (void)simtower::serialize_original_tdt(links);
      assert(false);
    } catch (const simtower::OriginalTdtError& error) {
      assert(error.status() == simtower::OriginalTdtStatus::malformed);
    }
  }

  // 10a0:17ee preserves the original signed arithmetic: type zero maps
  // floors 1..10 directly and then only floors 24 + 15n, while every nonzero
  // type returns floor-bottom unless floor is above top. In particular, it
  // does not clamp floors more than one level below bottom to -1.
  assert(simtower::original_elevator_floor_record_index(0, 1, 99, 0) == -1);
  assert(simtower::original_elevator_floor_record_index(0, 1, 99, -1) == -2);
  assert(simtower::original_elevator_floor_record_index(0, 1, 99, 1) == 0);
  assert(simtower::original_elevator_floor_record_index(0, 1, 99, 10) == 9);
  assert(simtower::original_elevator_floor_record_index(0, 1, 99, 11) == -1);
  assert(simtower::original_elevator_floor_record_index(0, 1, 99, 23) == -1);
  assert(simtower::original_elevator_floor_record_index(0, 1, 99, 24) == 10);
  assert(simtower::original_elevator_floor_record_index(0, 1, 99, 25) == -1);
  assert(simtower::original_elevator_floor_record_index(0, 1, 99, 39) == 11);
  assert(simtower::original_elevator_floor_record_index(1, 24, 38, 24) == 0);
  assert(simtower::original_elevator_floor_record_index(1, 24, 38, 38) == 14);
  assert(simtower::original_elevator_floor_record_index(1, 24, 38, 39) == -1);
  assert(simtower::original_elevator_floor_record_index(1, 24, 38, 23) == -1);
  assert(simtower::original_elevator_floor_record_index(1, 24, 38, 22) == -2);
  assert(simtower::original_elevator_floor_record_index(0xff, -9, 9, -8) == 1);

  {
    // 1000:3a18/3a2f owns one process-wide CRT state, not one seed per TDT.
    // New/Open document replacement must retain the live state, while the
    // first document installed at process startup keeps the initial seed one.
    auto active = simtower::make_original_new_tdt();
    active.random_state = 0x89abcdefU;
    auto replacement = simtower::make_original_new_tdt();
    simtower::carry_original_process_random_state(&active, replacement);
    assert(replacement.random_state == 0x89abcdefU);

    auto startup = simtower::make_original_new_tdt();
    simtower::carry_original_process_random_state(nullptr, startup);
    assert(startup.random_state == 1U);
  }

  const auto expect_status = [](std::vector<std::byte> bytes,
                                simtower::OriginalTdtStatus status) {
    try {
      (void)simtower::parse_original_tdt(bytes);
      assert(false);
    } catch (const simtower::OriginalTdtError& error) {
      assert(error.status() == status);
    }
  };
  expect_status({std::byte{0x00}, std::byte{0x25}},
                simtower::OriginalTdtStatus::version_too_new);
  expect_status({std::byte{0x00}, std::byte{0x16}},
                simtower::OriginalTdtStatus::version_too_old);
  expect_status({std::byte{0x25}, std::byte{0x00}},
                simtower::OriginalTdtStatus::version_too_new);
  expect_status({std::byte{0x00}, std::byte{0x24}},
                simtower::OriginalTdtStatus::short_transfer);

  const std::filesystem::path roundtrip_path(argv[5]);
  auto legacy_file_save = fresh;
  make_legacy_revision(legacy_file_save, 0x17U);
  // Direct 10d0:0777/0b3a/2a13 disk-transaction coverage: create, serialize,
  // transfer, close, reopen, normalize, and compare the upgraded document.
  simtower::save_original_tdt_file(roundtrip_path, legacy_file_save);
  const auto upgraded_from_disk =
      simtower::load_original_tdt_file(roundtrip_path);
  assert(upgraded_from_disk.header.raw_version == 0x2400U);
  assert(upgraded_from_disk.header.format_version == 0x24U);
  assert(!upgraded_from_disk.header.byte_swapped);

  const auto source = simtower::load_original_tdt_file(argv[2]);
  simtower::save_original_tdt_file(roundtrip_path, source);
  const auto from_disk = simtower::load_original_tdt_file(roundtrip_path);
  assert(simtower::serialize_original_tdt(from_disk) ==
         simtower::serialize_original_tdt(source));
  assert(std::filesystem::remove(roundtrip_path));

  // Direct 10d0:0122/03f1 common-dialog profile coverage. Both paths use
  // filter index one, 128/15-character limits, and a zero flags dword; the
  // recovered one is not an OFN_READONLY flag.
  assert((simtower::original_tdt_file_dialog_profile() ==
          simtower::OriginalTdtFileDialogProfile{1U, 128U, 15U, 0U}));
  // Direct 10d0:0305 coverage: normal Save clears overwrite confirmation;
  // Save As retains the replacement prompt.
  static_assert(!simtower::original_tdt_save_overwrite_prompt(false));
  static_assert(simtower::original_tdt_save_overwrite_prompt(true));

  // Direct 10d0:03f1 coverage: the Save As loop uses complete-string
  // strrchr('.') replacement, complete-string strchr('.')/strrchr('\\') DOS
  // basename validation, and an unconditional four-byte title-suffix trim.
  assert(simtower::original_tdt_normalized_path("Tower") ==
         std::filesystem::path("Tower.TDT"));
  assert(simtower::original_tdt_normalized_path("Tower.foo") ==
         std::filesystem::path("Tower.TDT"));
  assert(simtower::original_tdt_normalized_path(
             LR"(C:\dir.with.dot\Tower)") ==
         std::filesystem::path(LR"(C:\dir.with.TDT)"));
  assert(simtower::original_tdt_normalized_path(
             LR"(C:\save\12345678.foo.bar)") ==
         std::filesystem::path(LR"(C:\save\12345678.foo.TDT)"));
  assert(simtower::original_tdt_basename_is_valid("12345678.TDT"));
  assert(!simtower::original_tdt_basename_is_valid("123456789.TDT"));
  assert(simtower::original_tdt_basename_is_valid(
      LR"(C:\save\12345678.foo.TDT)"));
  assert(!simtower::original_tdt_basename_is_valid(
      LR"(C:\save\123456789.foo.TDT)"));
  assert(simtower::original_tdt_basename_is_valid(
      LR"(C:\dir.with.dot\123456789.TDT)"));
  // 10d0:054b-0566 rejects only lengths greater than eight; the common
  // dialog is responsible for excluding an otherwise empty selection.
  assert(simtower::original_tdt_basename_is_valid(".TDT"));
  // Direct 10d0:0225 coverage for the accepted-load title transaction: append
  // the selected file title and remove exactly its final four suffix bytes.
  assert(simtower::original_tower_window_title(L"ElDlux.TDT") ==
         L"SimTower - ElDlux");
  assert(simtower::original_tower_window_title(L"untitled") ==
         L"SimTower - untitled");
  return 0;
}
