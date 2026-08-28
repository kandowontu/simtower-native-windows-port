#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace simtower {

inline constexpr std::uint16_t kOriginalMovableGlobalAllocFlags = 0x0040U;
inline constexpr std::size_t kOriginalElevatorBlockCount = 24U;
inline constexpr std::size_t kOriginalElevatorBlockBytes = 0x345aU;
inline constexpr std::size_t kOriginalMedicalRouteIndexBytes = 0x009aU;
inline constexpr std::size_t kOriginalPeopleBlockBytes = 0x1000U;

struct OriginalOwnedStorageContract {
  std::size_t block_count{};
  std::size_t bytes_per_block{};
  std::uint16_t global_alloc_flags{};

  friend bool operator==(const OriginalOwnedStorageContract&,
                         const OriginalOwnedStorageContract&) = default;
};

// Exact allocation/free pairs at 1090:0014/0074 and 1170:0014/004e, plus
// 1238:001e/0073's single people block. Native document members own the same
// logical banks without exposing movable far-heap handles.
[[nodiscard]] constexpr OriginalOwnedStorageContract
original_elevator_storage_contract() noexcept {
  return {kOriginalElevatorBlockCount, kOriginalElevatorBlockBytes,
          kOriginalMovableGlobalAllocFlags};
}

[[nodiscard]] constexpr OriginalOwnedStorageContract
original_medical_storage_contract() noexcept {
  return {1U, kOriginalMedicalRouteIndexBytes,
          kOriginalMovableGlobalAllocFlags};
}

[[nodiscard]] constexpr OriginalOwnedStorageContract
original_people_storage_contract() noexcept {
  return {1U, kOriginalPeopleBlockBytes,
          kOriginalMovableGlobalAllocFlags};
}

enum class OriginalTdtStatus : std::uint16_t {
  ok = 0,
  short_transfer = 1,
  version_too_new = 4,
  version_too_old = 5,
  malformed = 6,
};

class OriginalTdtError : public std::runtime_error {
 public:
  OriginalTdtError(OriginalTdtStatus status, const char* message)
      : std::runtime_error(message), status_(status) {}

  [[nodiscard]] OriginalTdtStatus status() const noexcept { return status_; }

 private:
  OriginalTdtStatus status_;
};

struct OriginalTdtHeader {
  // Live transfer word and storage order. Opposite-endian imports are
  // normalized exactly as 10d0:1518 does, so these become version<<8 and
  // false after parsing; OriginalTdtDocument::exact_bytes retains the
  // untouched source stream for explicit lossless tooling.
  std::uint16_t raw_version{};
  std::uint8_t format_version{};
  bool byte_swapped{};
  std::uint16_t rating{};
  std::int32_t balance{};
  std::int32_t other_income{};
  std::int32_t construction_costs{};
  std::int32_t last_quarter_money{};
  std::uint16_t frame_time{};
  std::int32_t current_day{};
  std::uint16_t version_20_word{};
  std::uint16_t lobby_height{};
  std::uint16_t view_x{};
  std::uint16_t view_y{};
  // DS:b402, the active count for the dce4 32-bit person-index table. The
  // original caps pre-0x23 saves to ten entries; revisions 0x23+ hold twenty.
  std::uint16_t person_link_count{};
  // DS:b404, introduced by revision 0x23. This is the active count for the
  // twenty-entry dd34 tenant-link table; older revisions have no such word.
  std::uint16_t tenant_link_count{};
  std::vector<std::byte> exact_bytes{};
};

struct OriginalTdtLinkName {
  // 1188:043d writes every active runtime name as one fixed sixteen-byte
  // record after the core TDT transfer. 1188:02ea also accepts an older
  // 256-byte Pascal-string input form and normalizes it into this layout. The
  // first NUL terminates the original ANSI text; ordinary-record padding is
  // retained for deterministic byte-exact rebuilds.
  std::array<std::byte, 16> exact_bytes{};
};

struct OriginalTdtTenant {
  std::uint16_t left{};
  std::uint16_t right{};
  std::int8_t type{};
  std::uint8_t status{};
  std::uint8_t variant{};
  std::array<std::byte, 9> preserved_07_to_0f{};
  std::uint8_t rent_rate{};
  std::uint8_t subtype{};
  std::array<std::byte, 18> exact_bytes{};
};

struct OriginalTdtFloor {
  // 11f8:02ca allocates exactly 0xb4e bytes per floor:
  //   6-byte header + 150 * 18-byte tenant records + 94 * 2-byte indices.
  static constexpr std::size_t kTenantCapacity = 150;
  static constexpr std::size_t kIndexCapacity = 94;

  std::uint16_t left_edge{};
  std::uint16_t right_edge{};
  std::vector<OriginalTdtTenant> tenants{};
  std::array<std::uint16_t, kIndexCapacity> tenant_index{};
  std::size_t file_offset{};
};

struct OriginalTdtPersonRecord {
  // 1238:00cf resizes the Win16 people block to count*16. A native vector of
  // exact 16-byte records preserves the same indexed storage semantics.
  std::array<std::byte, 16> exact_bytes{};
};

struct OriginalTdtRetailRecord {
  std::array<std::byte, 18> exact_bytes{};
};

struct OriginalTdtElevatorFloorRecord {
  std::int16_t mapped_index{};
  std::int8_t floor{};
  std::array<std::byte, 324> exact_bytes{};
};

struct OriginalTdtElevatorCarRecord {
  std::array<std::byte, 346> exact_bytes{};
};

struct OriginalTdtElevator {
  std::uint8_t used{};
  std::uint8_t type{};
  std::uint8_t capacity{};
  std::uint8_t cars{};
  std::array<std::byte, 56> schedule{};
  std::uint16_t word_3c{};
  std::uint16_t x{};
  std::int8_t top_floor{};
  std::int8_t bottom_floor{};
  std::array<std::byte, 120> serviced_floors{};
  std::array<std::byte, 8> car_home_floors{};

  std::size_t file_offset{};
  std::size_t file_header_size{};
  std::size_t payload_offset{};
  std::size_t payload_size{};
  std::array<std::byte, 194> reconstructed_header{};
  std::vector<std::byte> exact_file_header{};
  std::array<std::byte, 480> block_c2{};
  std::array<std::byte, 120> block_2a2{};
  std::array<std::byte, 120> block_31a{};
  std::vector<OriginalTdtElevatorFloorRecord> floor_records{};
  std::array<OriginalTdtElevatorCarRecord, 8> car_records{};
};

struct OriginalTdtFinance {
  std::array<std::int32_t, 10> population_by_category{};
  std::int32_t total_population{};
  std::array<std::int32_t, 10> income_by_category{};
  std::int32_t total_income{};
  std::array<std::int32_t, 10> maintenance_by_category{};
  std::int32_t total_maintenance{};
};

struct OriginalTdtStairRecord {
  std::uint8_t used{};
  std::uint8_t shape{};
  std::uint16_t x{};
  std::int8_t floor{};
  std::uint8_t byte_5{};
  std::uint16_t word_6{};
  std::uint16_t word_8{};
  std::array<std::byte, 10> exact_bytes{};
};

// Exact post-elevator transfer order from 10d0:1271-150a. Names retain the
// original DS destinations where the semantic owner is not translated yet.
struct OriginalTdtPostElevatorTail {
  std::size_t file_offset{};
  std::array<std::byte, 0x58> b846{};
  std::array<std::array<std::int32_t, 11>, 2> b846_series{};
  std::array<std::byte, 0x84> finance_b89e{};
  OriginalTdtFinance finance{};
  std::array<std::byte, 0x0c> b922{};
  std::uint8_t b922_flag{};
  std::uint8_t b923{};
  std::int32_t b924{};
  std::uint8_t b928{};
  std::uint8_t b929{};
  std::uint8_t b92a{};
  std::uint8_t b92b{};
  std::uint8_t b92c{};
  std::uint8_t b92d{};
  std::array<std::byte, 0x2a> b92e{};
  std::uint8_t b92e_counter{};
  std::array<std::uint16_t, 10> b944_words{};
  std::array<std::byte, 0x402> parking_b958{};
  std::int16_t parking_connected{};
  std::array<std::uint16_t, 512> parking_entries{};
  std::array<std::byte, 0x16> bd5a{};
  std::uint16_t bd5a_count{};
  std::array<std::uint16_t, 10> bd5c_entries{};
  std::array<OriginalTdtStairRecord, 64> stairs_bd70{};
  std::array<std::array<std::byte, 0x1e4>, 8> routes_bff0{};
  std::array<std::byte, 0x78> cf10{};
  std::array<std::uint16_t, 10> cf88_words{};
  std::array<std::array<std::byte, 6>, 512> cf9c_records{};
  std::array<std::array<std::byte, 6>, 16> db9c_records{};
  std::array<std::uint32_t, 10> dbfc_dwords{};
  std::array<std::array<std::byte, 0x0c>, 16> dc24_records{};
  std::vector<std::byte> version_23_dce4{};
  std::array<std::int32_t, 20> dce4_person_indices{};
  std::array<std::byte, 0x28> dce4_or_dd34{};
  // 11a8:0000 allocates and zeroes the exact 0x2400 Retail bank followed by
  // these 0x1102/0x842/0xca2 commercial-service banks. Native fixed arrays
  // preserve their addresses-by-index without Win16 movable-memory handles.
  std::array<std::byte, 0x1102> dynamic_dd5c{};
  std::array<std::byte, 0x842> dynamic_dd60{};
  std::array<std::byte, 0xca2> dynamic_dd64{};
  std::vector<std::byte> version_18_dd6c{};
  std::size_t end_offset{};
};

struct OriginalTdtDocument {
  OriginalTdtHeader header{};
  std::array<OriginalTdtFloor, 120> floors{};
  std::uint32_t people_count{};
  // Native owned storage replaces 1238:0073's Win16 people-block free path.
  std::vector<OriginalTdtPersonRecord> people{};
  std::size_t people_offset{};
  std::array<OriginalTdtRetailRecord, 512> retail{};
  std::size_t retail_offset{};
  std::size_t elevator_table_offset{};
  // Native value storage replaces 1090:0014/1090:0074's 24 separately
  // GlobalAlloc/GlobalFree-managed elevator blocks.
  std::array<OriginalTdtElevator, kOriginalElevatorBlockCount> elevators{};
  std::size_t after_elevators_offset{};
  OriginalTdtPostElevatorTail post_elevator{};
  std::vector<OriginalTdtLinkName> person_link_names{};
  std::vector<OriginalTdtLinkName> tenant_link_names{};
  // Bytes after the exact 1188:043d name records. Normal original saves have
  // none, but third-party exporters can append opaque data that must survive.
  std::vector<std::byte> trailing_bytes{};
  std::vector<std::byte> exact_bytes{};
  // Process-local selectors at DS:7952/7958/795a. They reset when a tower is
  // created or loaded and are deliberately not part of the TDT byte stream.
  std::uint16_t restaurant_service_variant{};
  std::uint16_t retail_service_variant{};
  std::uint16_t fast_food_service_variant{};
  // Process-local Microsoft C runtime rand() state at DS:0bd4/0bd6. The
  // executable starts with seed one and advances state = state*0x015a4e35+1.
  // It is intentionally absent from the TDT byte stream and must survive a
  // native document replacement.
  std::uint32_t random_state{1U};
  // Process-local DS:77aa used by the Security fire-response family. The
  // first extinguished fire band raises it, making subsequent 10f8:104a
  // responder moves immediate. Event start/load reset it to zero and it is
  // deliberately absent from the TDT byte stream.
  bool security_event_accelerated{};
  // Process-local words set by Hotel checkout 1178:0eac. DS:31b8 controls
  // the exact even/eighth checkout cadence in DS:02aa; DS:779e is raised by
  // every checkout. None is serialized in TDT.
  std::uint16_t hotel_checkout_count{};
  bool hotel_checkout_effect_cadence{};
  bool hotel_checkout_effect_active{};
  // Process-local 7 * 0x16-byte table behind DS:dd68 (1170:06f7). Native
  // ownership replaces 1170:0014/1170:004e's GlobalAlloc/GlobalFree wrappers.
  std::array<std::byte, kOriginalMedicalRouteIndexBytes> medical_route_index{};
};

// Preserve the process-global Microsoft C runtime RNG state when New/Open
// replaces the native document model. Static call-graph recovery shows that
// the CRT seed writer at 1000:3a18 has no inbound call or relocation, while
// all gameplay randomness advances through 1000:3a2f. A null active document
// represents process startup and leaves the replacement's initial seed one.
void carry_original_process_random_state(
    const OriginalTdtDocument* active,
    OriginalTdtDocument& replacement) noexcept;

// Exact translation of 10a0:17ee. Type zero uses the original compressed
// express-floor map (1..10, then 24, 39, 54, ...); other types are contiguous.
[[nodiscard]] std::int16_t original_elevator_floor_record_index(
    std::uint8_t type,
    std::int8_t bottom_floor,
    std::int8_t top_floor,
    std::int16_t floor) noexcept;

// Parses the exact header/floor/people/retail/elevator sequence consumed by
// 10d0:0b3a and its byte-swapped compatibility path at 10d0:1518. The
// full post-elevator transfer sequence is retained in its original blocks.
[[nodiscard]] OriginalTdtDocument parse_original_tdt(
    std::span<const std::byte> bytes);

// Translates the complete fresh-tower constructor chain rooted at 10d0:086c
// and the floor-derived rebuild at 10d0:0ac2 into a serializable 0x24 state.
// No reference save is used as a template.
[[nodiscard]] OriginalTdtDocument make_original_new_tdt();

// Exact 10d0:2a8e New/Open/Exit confirmation routine, including its
// 10d0:2a9e/2aa5 predicate. The executable
// checks that a document is active and that floor 10's allocated tenant count
// is nonzero; it does not track whether the tower changed since its last save.
[[nodiscard]] bool original_tower_transition_requires_confirmation(
    const OriginalTdtDocument* document) noexcept;

// Until every tail structure is represented as native state, this is the
// intentionally lossless write path: no field is guessed or normalized.
[[nodiscard]] std::vector<std::byte> serialize_original_tdt_lossless(
    const OriginalTdtDocument& document);

// Rebuilds the represented 10d0:0b3a stream from native fields while carrying
// every still-opaque byte forward. Variable elevator payload sizes are allowed
// when the caller supplies the corresponding floor-record vectors.
[[nodiscard]] std::vector<std::byte> serialize_original_tdt(
    const OriginalTdtDocument& document);

// Exact game-facing write policy from 10d0:0b3a: the executable always emits
// revision 0x24 in little-endian order, upgrading imported legacy and
// opposite-endian runtime structures before writing them.
[[nodiscard]] std::vector<std::byte> serialize_original_tdt_game_save(
    const OriginalTdtDocument& document);

}  // namespace simtower
