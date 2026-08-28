#pragma once

#include "original_tdt.hpp"
#include "original_tables.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace simtower {

enum class OriginalOfficePersonStepStatus : std::uint8_t {
  invalid_person,
  not_office,
  common_update_only,
  entered_office,
  left_office,
  malformed_tenant_link,
  malformed_route_table,
};

struct OriginalRecyclingStepResult {
  std::size_t touched{};
  // Argument passed to 1118:09be. Zero means that the original emitted no
  // information-panel notification; the observed Recycling codes are 3/4.
  std::uint8_t notification_code{};
  // 1088:00de calls 11c8:0167 with WAVE/2280 once after any midnight reset.
  bool play_transition_sound{};
};

struct OriginalFacilityPeopleCleanupResult {
  std::size_t finalized{};
  std::size_t retired{};
  // 1240:0198 clears the tracked special visitor and requests information
  // message 3003 when a deleted Hotel owns that person record.
  bool cleared_periodic_visitor{};
  std::uint16_t notification_code{};
};

enum class OriginalElevatorAssignmentSelectionStatus : std::uint8_t {
  invalid,
  immediate_service,
  assign_car,
};

struct OriginalElevatorAssignmentSelection {
  OriginalElevatorAssignmentSelectionStatus status{
      OriginalElevatorAssignmentSelectionStatus::invalid};
  std::uint8_t car_index{};
};

enum class OriginalElevatorFloorPeopleCleanupStatus : std::uint8_t {
  invalid,
  dispatch_required,
  cleaned,
};

struct OriginalElevatorFloorPeopleCleanupResult {
  OriginalElevatorFloorPeopleCleanupStatus status{
      OriginalElevatorFloorPeopleCleanupStatus::invalid};
  std::size_t car_passengers{};
  std::size_t waiting_passengers{};
};

enum class OriginalElevatorWaitingLane : std::uint8_t {
  first,
  second,
};

struct OriginalElevatorWaitingPersonHit {
  bool hit{};
  std::size_t elevator_index{};
  std::size_t person_index{};
  std::int16_t floor{-1};
  OriginalElevatorWaitingLane lane{OriginalElevatorWaitingLane::first};
  std::size_t queue_ordinal{};

  friend bool operator==(const OriginalElevatorWaitingPersonHit&,
                         const OriginalElevatorWaitingPersonHit&) = default;
};

enum class OriginalElevatorBoardingDestinationStatus : std::uint8_t {
  invalid_person,
  invalid_elevator,
  unsupported_family_state,
  no_route,
  selected,
};

// Exact destination chain used by 1210:0f0e. final_destination is the
// person's facility/parking/Lobby destination selected by the family helper;
// car_destination is either that serviced floor or 11b0:092f's matching
// transfer floor for this Elevator and direction.
struct OriginalElevatorBoardingDestination {
  OriginalElevatorBoardingDestinationStatus status{
      OriginalElevatorBoardingDestinationStatus::invalid_person};
  std::int16_t final_destination{-1};
  std::int16_t car_destination{-1};
};

struct OriginalElevatorPassengerVisualEvent {
  std::size_t elevator_index{};
  std::int16_t floor{};
  bool boarding{};
  bool direction_up{};
  std::size_t person_index{};

  friend bool operator==(const OriginalElevatorPassengerVisualEvent&,
                         const OriginalElevatorPassengerVisualEvent&) =
      default;
};

// Exact return values of the common person route resolver at `1210:0000`.
// The two negative native-only values protect malformed loaded data that the
// Win16 executable assumes is valid; the four nonnegative values retain the
// original ABI meanings verbatim.
enum class OriginalPersonRouteStatus : std::int16_t {
  invalid_person = -3,
  malformed_transport = -2,
  no_route = -1,
  elevator_queue_full = 0,
  stair = 1,
  elevator = 2,
  already_on_floor = 3,
};

struct OriginalPersonRouteRequest {
  std::int16_t source_floor{};
  std::int16_t destination_floor{};

  // Original bp+6. A nonzero value simultaneously enables the 11d8 metric
  // calls and selects ordinary/express elevators plus the full Stair branch;
  // zero selects service elevators and the odd-shape vertical-route branch.
  bool tracked_route{true};
  // Original bp+8: adds the exact 30/60 horizontal-distance penalties.
  bool add_distance_penalty{true};
  // Original bp+12: asks the caller to render 10a8:1b58 on route failure.
  bool visualize_failure{};

  // Process-only words read by 1210:0000. They are explicit because none is
  // serialized in a TDT person or transport record.
  std::uint16_t frame_time{};       // DS:b3de
  std::uint16_t queue_full_delay{}; // DS:dd7c
  std::uint16_t no_route_delay{};   // DS:dd80
  std::uint16_t stair_even_delay{}; // DS:ddb8
  std::uint16_t stair_odd_delay{};  // DS:ddba
  std::uint8_t calendar_phase{};    // DS:b3a0
  std::int8_t day_phase{};          // signed DS:b3a1
};

struct OriginalPersonRouteResult {
  OriginalPersonRouteStatus status{OriginalPersonRouteStatus::invalid_person};
  std::int16_t transport_index{-1};
  bool direction_up{};
  bool failure_visualization_requested{};
  bool queue_assignment_created{};
};

struct OriginalPersonTransportSelection {
  std::int16_t transport_index{-1};
  bool direction_up{};
};

enum class OriginalCathedralPersonStepStatus : std::uint8_t {
  invalid_person,
  not_cathedral,
  unhandled_state,
  routed,
  route_failed,
  arrived_cathedral,
  returned_to_lobby,
  malformed_route,
};

struct OriginalCathedralPersonStepResult {
  OriginalCathedralPersonStepStatus status{
      OriginalCathedralPersonStepStatus::invalid_person};
  OriginalPersonRouteResult route{};
  bool released_stair_counter{};
  // `1220:6037` calls `1040:00f0` only after route return 3 on the 20/60
  // inbound branch. The global ceremony mutation remains a separate exact
  // boundary and is represented explicitly rather than approximated here.
  bool cathedral_arrival_check_requested{};
  bool changed{};
};

enum class OriginalCathedralArrivalStatus : std::uint8_t {
  cathedral_absent,
  outside_arrival_window,
  population_rating_gate,
  waiting_for_people,
  already_maximum_rating,
  ceremony_started,
  malformed_cathedral,
};

struct OriginalCathedralArrivalResult {
  OriginalCathedralArrivalStatus status{
      OriginalCathedralArrivalStatus::cathedral_absent};
  std::size_t arrived_people{};
  std::int16_t effect_floor{-1};
  std::uint16_t effect_x{};
  bool rating_changed{};
  bool repaint_requested{};
  bool stop_both_audio_channels{};
  std::int32_t wave_resource{};
  std::uint16_t wave_repeat{};
  std::uint16_t wave_priority{};
};

struct OriginalCathedralPersonDispatchResult {
  OriginalCathedralPersonStepResult step{};
  std::optional<OriginalCathedralArrivalResult> arrival{};
};

enum class OriginalSecurityPersonStepStatus : std::uint8_t {
  invalid_person,
  not_security,
  inactive,
  no_event,
  countdown,
  moved_floor,
  searching,
  search_exhausted,
  bomb_found,
  fire_extinguished,
  malformed_owner,
  malformed_floor,
};

struct OriginalSecurityEffectRequest {
  std::int16_t floor{-1};
  std::int16_t x{-1};

  [[nodiscard]] bool valid() const noexcept {
    return floor >= 0 && x >= 0;
  }
};

struct OriginalSecurityPersonStepResult {
  OriginalSecurityPersonStepStatus status{
      OriginalSecurityPersonStepStatus::invalid_person};
  bool changed{};
  bool bomb_found{};
  bool fire_extinguished{};
  bool disabled_other_responders{};
  OriginalSecurityEffectRequest effect{};
};

struct OriginalSecurityPeopleStepResult {
  std::size_t responders{};
  std::size_t changed{};
  std::size_t bombs_found{};
  std::size_t fire_bands_extinguished{};
  std::vector<OriginalSecurityEffectRequest> effects{};
};

enum class OriginalHousekeepingPersonStepStatus : std::uint8_t {
  invalid_person,
  not_housekeeping,
  unhandled_state,
  no_dirty_room,
  routed_to_room,
  arrived_room,
  cleaning_countdown,
  room_reopened,
  routed_home,
  returned_home,
  route_failed,
  malformed_room,
  malformed_route,
};

struct OriginalHousekeepingPersonStepResult {
  OriginalHousekeepingPersonStepStatus status{
      OriginalHousekeepingPersonStepStatus::invalid_person};
  OriginalPersonRouteResult route{};
  bool changed{};
  bool released_stair_counter{};
  bool room_status_changed{};
  bool hotel_guest_state_changed{};
  std::int16_t selected_room_floor{-1};
  std::int16_t selected_room_key{-1};
};

struct OriginalHousekeepingPeopleStepResult {
  std::size_t scanned{};
  std::size_t dispatched{};
  std::size_t changed{};
  std::size_t rooms_cleaned{};
  std::size_t rooms_reopened{};
};

enum class OriginalMetroPersonStepStatus : std::uint8_t {
  invalid_person,
  not_metro,
  unhandled_state,
  no_destination,
  routed_to_service,
  arrived_service,
  service_full,
  waiting_at_service,
  routed_home,
  returned_home,
  route_failed,
  malformed_service,
  malformed_route,
};

struct OriginalMetroPersonStepResult {
  OriginalMetroPersonStepStatus status{
      OriginalMetroPersonStepStatus::invalid_person};
  OriginalPersonRouteResult route{};
  bool changed{};
  bool released_stair_counter{};
  bool service_population_changed{};
  bool service_tenant_marked_dirty{};
  std::int16_t service_index{-1};
};

enum class OriginalFoodServicePersonStepStatus : std::uint8_t {
  invalid_person,
  not_food_service,
  unhandled_state,
  service_closed,
  reservation_blocked,
  routed_to_service,
  arrived_service,
  service_full,
  waiting_at_service,
  routed_home,
  returned_home,
  route_failed,
  malformed_owner,
  malformed_service,
  malformed_route,
};

struct OriginalFoodServicePersonStepResult {
  OriginalFoodServicePersonStepStatus status{
      OriginalFoodServicePersonStepStatus::invalid_person};
  OriginalPersonRouteResult route{};
  bool changed{};
  bool released_stair_counter{};
  bool reservation_changed{};
  bool service_population_changed{};
  bool service_tenant_marked_dirty{};
  bool service_history_changed{};
  std::int16_t service_index{-1};
};

enum class OriginalRetailPersonStepStatus : std::uint8_t {
  invalid_person,
  not_retail,
  unhandled_state,
  reservation_blocked,
  routed_to_store,
  arrived_store,
  store_full,
  waiting_at_store,
  routed_home,
  returned_home,
  route_failed,
  malformed_owner,
  malformed_service,
  malformed_route,
};

struct OriginalRetailPersonStepResult {
  OriginalRetailPersonStepStatus status{
      OriginalRetailPersonStepStatus::invalid_person};
  OriginalPersonRouteResult route{};
  bool changed{};
  bool released_stair_counter{};
  bool reservation_changed{};
  bool service_population_changed{};
  bool service_tenant_marked_dirty{};
  bool service_history_changed{};
  bool store_activated{};
  bool activation_visual_requested{};
  std::int16_t service_index{-1};
};

enum class OriginalEntertainmentPersonStepStatus : std::uint8_t {
  invalid_person,
  not_entertainment,
  unhandled_state,
  capacity_unavailable,
  routed_to_entertainment,
  arrived_entertainment,
  routed_to_service,
  arrived_service,
  service_full,
  service_unavailable,
  waiting_at_service,
  routed_from_service,
  returned_from_service,
  routed_home,
  returned_home,
  route_failed,
  malformed_owner,
  malformed_service,
  malformed_route,
};

struct OriginalEntertainmentPersonStepResult {
  OriginalEntertainmentPersonStepStatus status{
      OriginalEntertainmentPersonStepStatus::invalid_person};
  OriginalPersonRouteResult route{};
  bool changed{};
  bool released_stair_counter{};
  bool entertainment_capacity_changed{};
  bool entertainment_record_changed{};
  bool service_population_changed{};
  bool service_tenant_marked_dirty{};
  std::int16_t entertainment_index{-1};
  std::int16_t service_index{-1};
};

enum class OriginalCondoPersonStepStatus : std::uint8_t {
  invalid_person,
  not_condo,
  unhandled_state,
  routed_to_lobby,
  arrived_lobby,
  routed_home,
  arrived_home,
  routed_to_service,
  arrived_service,
  service_full,
  service_unavailable,
  waiting_at_service,
  routed_from_service,
  resident_synchronized,
  route_failed,
  malformed_owner,
  malformed_service,
  malformed_route,
};

struct OriginalCondoPersonStepResult {
  OriginalCondoPersonStepStatus status{
      OriginalCondoPersonStepStatus::invalid_person};
  OriginalPersonRouteResult route{};
  bool changed{};
  bool released_stair_counter{};
  bool owner_status_changed{};
  bool condo_activated{};
  bool activation_visual_requested{};
  bool service_population_changed{};
  bool service_tenant_marked_dirty{};
  std::int16_t service_index{-1};
};

enum class OriginalHotelPersonStepStatus : std::uint8_t {
  invalid_person,
  not_hotel,
  unhandled_state,
  departure_prepared,
  guest_synchronized,
  routed_from_hotel,
  departed_hotel,
  parking_unavailable,
  routed_to_hotel,
  arrived_hotel,
  routed_to_service,
  arrived_service,
  service_full,
  service_unavailable,
  waiting_at_service,
  routed_from_service,
  route_failed,
  malformed_owner,
  malformed_service,
  malformed_route,
};

// Process-only requests issued through 1068:0000 by the Hotel special-guest
// callbacks at 1240:0000/00d1/0130/0198. The original request codes are
// retained verbatim; only code 3000 carries a nonzero monetary amount.
struct OriginalHotelProcessRequest {
  std::uint16_t transaction_code{};
  std::int32_t amount{};
};

// Ordered process-only calls emitted while a translated person callback is
// mutating persisted state. The original writes income/notification text into
// the shared Info window immediately and opens Hotel dialogs synchronously, so
// aggregate counters alone cannot preserve the winning message or dialog order.
enum class OriginalPersonHostRequestKind : std::uint8_t {
  income_status,
  notification_status,
  hotel_dialog,
};

struct OriginalPersonHostRequest {
  OriginalPersonHostRequestKind kind{
      OriginalPersonHostRequestKind::income_status};
  std::uint16_t code{};
  std::int32_t argument{};

  friend bool operator==(const OriginalPersonHostRequest&,
                         const OriginalPersonHostRequest&) = default;
};

struct OriginalHotelPersonStepResult {
  OriginalHotelPersonStepStatus status{
      OriginalHotelPersonStepStatus::invalid_person};
  OriginalPersonRouteResult route{};
  bool changed{};
  bool released_stair_counter{};
  bool owner_status_changed{};
  bool room_activated{};
  bool room_checked_out{};
  bool checkout_visual_requested{};
  bool parking_changed{};
  bool periodic_visitor_changed{};
  bool service_population_changed{};
  bool service_tenant_marked_dirty{};
  // 1198:031a calls 1118:09be(5) when an eligible guest cannot obtain a
  // connected/capacity-bearing parking space.
  std::uint8_t notification_code{};
  std::int16_t service_index{-1};
  std::vector<OriginalHotelProcessRequest> process_requests{};
  std::vector<OriginalPersonHostRequest> host_requests{};
};

// Exact 1198:06a6/0650 parking-word readers. The assignment predicate tests
// the physical upper six bits in person byte 13; the destination decoder then
// arithmetic-shifts signed word 12 and subtracts it from lobby floor ten.
[[nodiscard]] bool original_person_has_parking(
    const OriginalTdtPersonRecord& person) noexcept;
[[nodiscard]] std::int16_t original_person_parking_floor(
    const OriginalTdtDocument& document,
    const OriginalTdtPersonRecord& person) noexcept;
[[nodiscard]] bool original_person_parking_eligible(
    const OriginalTdtDocument& document,
    const OriginalTdtPersonRecord& person,
    const OriginalTdtTenant& owner,
    std::int16_t owner_floor,
    std::int16_t owner_key) noexcept;

enum class OriginalTransitPersonDispatchStatus : std::uint8_t {
  invalid_person,
  common_update_only,
  hotel_arrived,
  hotel_departed,
  office_arrived,
  office_departed,
  condo_arrived,
  commercial_completed,
  terminal_state,
  housekeeping_reset,
  malformed_owner,
  malformed_service,
};

// Persisted/gameplay result of the shared transit-arrival switch at
// `1220:1aed`. The Win16 routine calls the common 11d8 metric pair for every
// family except Housekeeping, then performs only the state mutations listed
// in its type/state tables. Hotel request 3003 remains a process-only request
// returned to the native host.
struct OriginalTransitPersonDispatchResult {
  OriginalTransitPersonDispatchStatus status{
      OriginalTransitPersonDispatchStatus::invalid_person};
  std::size_t person_index{};
  std::int8_t person_type{};
  std::uint8_t state_before{};
  bool changed{};
  bool common_update_applied{};
  bool owner_status_changed{};
  bool parking_changed{};
  bool periodic_visitor_changed{};
  bool service_history_changed{};
  std::int16_t service_index{-1};
  std::vector<OriginalHotelProcessRequest> process_requests{};
  std::vector<OriginalPersonHostRequest> host_requests{};
};

enum class OriginalElevatorWaitingDispatchStatus : std::uint8_t {
  invalid_person,
  direct_dispatch,
  ring_dispatched,
  malformed_queue,
};

// Exact persisted ring rotation performed by `1210:1b41 -> 1210:1332`.
// Every entry ahead of the timed-out person is popped and sent through
// `1220:1aed` before the target. The DS:777c view-array mutation cannot live
// in a TDT, so it is exposed as an explicit native-host request.
struct OriginalElevatorWaitingDispatchResult {
  OriginalElevatorWaitingDispatchStatus status{
      OriginalElevatorWaitingDispatchStatus::invalid_person};
  std::vector<std::size_t> person_indices{};
  std::vector<OriginalTransitPersonDispatchResult> dispatches{};
  bool view_slot_restore_requested{};
  std::int16_t view_floor{-1};
};

enum class OriginalElevatorWaitTimeoutStatus : std::uint8_t {
  invalid_person,
  not_armed,
  pending,
  dispatched,
  malformed_queue,
};

struct OriginalElevatorWaitTimeoutResult {
  OriginalElevatorWaitTimeoutStatus status{
      OriginalElevatorWaitTimeoutStatus::invalid_person};
  OriginalElevatorWaitingDispatchResult dispatch{};
};

enum class OriginalOfficeNormalPersonStepStatus : std::uint8_t {
  invalid_person,
  not_office,
  unhandled_state,
  routed_to_lobby,
  arrived_lobby,
  routed_to_office,
  arrived_office,
  routed_to_service,
  arrived_service,
  service_full,
  service_unavailable,
  waiting_at_service,
  routed_to_medical,
  arrived_medical,
  medical_full,
  waiting_at_medical,
  routed_from_office,
  departed_office,
  parking_unavailable,
  route_failed,
  malformed_owner,
  malformed_service,
  malformed_medical_service,
  malformed_route,
};

struct OriginalOfficeNormalPersonStepResult {
  OriginalOfficeNormalPersonStepStatus status{
      OriginalOfficeNormalPersonStepStatus::invalid_person};
  OriginalPersonRouteResult route{};
  bool changed{};
  bool released_stair_counter{};
  bool owner_status_changed{};
  bool office_activated{};
  bool activation_visual_requested{};
  bool parking_changed{};
  bool medical_population_changed{};
  bool medical_tenant_marked_dirty{};
  bool service_population_changed{};
  bool service_tenant_marked_dirty{};
  // 1198:031a uses code 5 for a failed eligible parking allocation;
  // 1170:061c uses code 6 when the Medical Center route is unavailable.
  std::uint8_t notification_code{};
  std::int16_t service_index{-1};
  std::int16_t medical_service_index{-1};
};

enum class OriginalPersonFamilyDispatchSource : std::uint8_t {
  dispatcher_16ab,
  elevator_car_0883,
  vertical_transport_1218,
};

enum class OriginalPersonFamilyDispatchStatus : std::uint8_t {
  invalid_person,
  no_handler,
  hotel,
  office,
  condo,
  retail,
  food_service,
  entertainment,
  metro,
  cathedral,
  housekeeping,
  security,
};

struct OriginalPersonFamilyDispatchResult {
  OriginalPersonFamilyDispatchStatus status{
      OriginalPersonFamilyDispatchStatus::invalid_person};
  OriginalPersonFamilyDispatchSource source{
      OriginalPersonFamilyDispatchSource::dispatcher_16ab};
  std::size_t person_index{};
  std::int8_t person_type{};
  bool changed{};
  bool activation_visual_requested{};
  bool checkout_visual_requested{};
  std::uint8_t notification_code{};
  std::vector<OriginalHotelProcessRequest> hotel_process_requests{};
  std::vector<OriginalPersonHostRequest> host_requests{};
  std::optional<OriginalCathedralArrivalResult> cathedral_arrival{};
  OriginalSecurityEffectRequest security_effect{};
};

enum class OriginalElevatorPassengerStepStatus : std::uint8_t {
  invalid_car,
  inactive_car,
  no_transfer,
  transferred,
  malformed_state,
};

struct OriginalElevatorPassengerStepResult {
  OriginalElevatorPassengerStepStatus status{
      OriginalElevatorPassengerStepStatus::invalid_car};
  std::size_t boarded{};
  std::size_t rejected{};
  std::size_t alighted{};
  std::optional<OriginalElevatorPassengerVisualEvent> boarding_visual{};
  std::optional<OriginalElevatorPassengerVisualEvent> alighting_visual{};
  std::vector<OriginalPersonFamilyDispatchResult> family_dispatches{};
};

enum class OriginalElevatorCarStepStatus : std::uint8_t {
  invalid_car,
  inactive_car,
  malformed_state,
  countdown_advanced,
  door_advanced,
  doors_opened,
  moved,
};

// One exact 1090:06fb car-state pass. The movement sound is the original
// 11c8:0167 call for WAVE/0x1772; assignment_created counts the old-floor
// queue lanes reclaimed through 1090:0a4c after the car moves away.
struct OriginalElevatorCarStepResult {
  OriginalElevatorCarStepStatus status{
      OriginalElevatorCarStepStatus::invalid_car};
  std::int16_t floor_before{-1};
  std::int16_t floor_after{-1};
  std::uint8_t motion_class{};
  std::size_t assignments_created{};
  bool movement_sound_requested{};
  bool changed{};
};

// Exact Elevator portion of 1090:03ab: advance every active car first, then
// run 1210:07a6 and 1210:0351 for every active car. 10a8:0000/022b owns one
// process-local cache entry per visible floor, Elevator, and transfer side;
// a later car overwrites only an event with that same three-part key.
struct OriginalElevatorFrameStepResult {
  std::size_t elevators_scanned{};
  std::size_t cars_scanned{};
  std::size_t cars_changed{};
  std::size_t movement_sound_requests{};
  std::size_t boarded{};
  std::size_t rejected{};
  std::size_t alighted{};
  std::vector<OriginalElevatorPassengerVisualEvent> transfer_visuals{};
  std::vector<OriginalPersonFamilyDispatchResult> family_dispatches{};
  bool changed{};
};

struct OriginalNativeElevatorFloorPeopleCleanupResult {
  OriginalElevatorFloorPeopleCleanupResult cleanup{};
  std::vector<OriginalPersonFamilyDispatchResult> family_dispatches{};
};

struct OriginalVerticalTransportPeopleCleanupResult {
  bool valid_transport_index{};
  std::vector<OriginalPersonFamilyDispatchResult> family_dispatches{};
};

struct OriginalTranslatedPeopleStepResult {
  std::size_t scanned{};
  std::size_t dispatched{};
  std::size_t changed{};
  std::size_t housekeeping_dispatched{};
  std::size_t metro_dispatched{};
  std::size_t food_service_dispatched{};
  std::size_t retail_dispatched{};
  std::size_t retail_activation_visual_requests{};
  std::size_t cathedral_dispatched{};
  std::size_t cathedral_arrival_checks{};
  std::size_t entertainment_dispatched{};
  std::size_t condo_dispatched{};
  std::size_t condo_activation_visual_requests{};
  std::size_t hotel_dispatched{};
  std::size_t hotel_checkout_visual_requests{};
  std::size_t hotel_parking_notifications{};
  // Exact 1118:09be arguments in person-table dispatch order. The aggregate
  // counters above remain useful diagnostics, while this sequence preserves
  // which message owns the shared transient field when several fire at once.
  std::vector<std::uint8_t> notification_codes{};
  std::vector<OriginalHotelProcessRequest> hotel_process_requests{};
  // Exact interleaving of 1118:0a49, 1118:09be, and Hotel 1068:0000 calls in
  // person-table dispatch order. The legacy counters/vectors above remain as
  // useful diagnostics but must not drive native host playback.
  std::vector<OriginalPersonHostRequest> host_requests{};
  std::size_t office_normal_dispatched{};
  std::size_t office_activation_visual_requests{};
  std::size_t office_parking_notifications{};
  std::size_t office_medical_notifications{};
  std::size_t elevator_timeout_checks{};
  std::size_t elevator_timeouts_triggered{};
  std::size_t elevator_transit_people_dispatched{};
  std::size_t elevator_timeout_view_requests{};
  std::size_t elevator_timeout_malformed_queues{};
  // Only the successful ceremony carries process-only focus/audio work that
  // the native host must consume after the persisted mutation completes.
  std::optional<OriginalCathedralArrivalResult> cathedral_ceremony{};
};

struct OriginalHotelCheckoutPresentation {
  bool repaint_balance{};
  bool play_cash_sound{};
};

// Exact once-per-frame DS:779e/02aa drain at 1090:0696 -> 1118:0143.
// Checkout sets the latch and cadence during the person pass; the later Info
// pass optionally plays WAVE/10013 (repeat 2, priority 3), repaints balance,
// restores the cadence word to one, and clears the latch.
[[nodiscard]] OriginalHotelCheckoutPresentation
consume_original_hotel_checkout_presentation(
    OriginalTdtDocument& document) noexcept;

// Adapter used by the exact Elevator cleanup scaffold. Car arrivals originate
// in `1210:0883`; waiting-ring removals originate in `10a0:1625 -> 1220:16ab`.
// The source argument preserves their one behavioral difference: Security is
// called only by the car-arrival switch.
using OriginalPersonFamilyDispatch = void (*)(
    OriginalTdtDocument& document,
    std::size_t person_index,
    OriginalPersonFamilyDispatchSource source,
    void* context) noexcept;

// Exact static translation of `1210:0000` and its complete `11b0:0fa5`
// selection chain. It resolves the transport from the persisted Stair,
// Elevator, cf10, route, and db9c graphs; updates Stair counters or the exact
// 40-entry Elevator waiting ring; applies the original metric delays; and
// writes person bytes 7/8 and word 10 in executable order.
[[nodiscard]] OriginalPersonRouteResult route_original_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPersonRouteRequest& request) noexcept;

// Read-only 11b0:0fa5 selector used by Facility Information's distance
// advisories. Unlike route_original_person this stops before queue, counter,
// metric, and person-record mutations.
[[nodiscard]] OriginalPersonTransportSelection
select_original_person_transport_for_information(
    const OriginalTdtDocument& document,
    std::int16_t source_floor,
    std::int16_t destination_floor,
    std::uint16_t person_x,
    bool tracked_route) noexcept;

// Reconstructs DS:dd7c/dd80/ddb8/ddba directly from PART/1000 and the
// process clock selectors from the persisted tower clock/day.
[[nodiscard]] OriginalPersonRouteRequest original_person_route_context(
    const OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept;

// Exact type-36 state table at `1220:6037`, including its leading
// `1210:1184` completed-Stair release, the four keys recovered from CS:6287,
// both calls into the common route resolver, and every route-return state.
[[nodiscard]] OriginalCathedralPersonStepResult
step_original_cathedral_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPersonRouteRequest& route_context) noexcept;

// Exact persisted/gameplay portion of `1040:00f0 -> 03bb/02b5`. It applies
// the PART/1000 population-rating gate, counts all forty type-36 people,
// updates Cathedral frame words and b406/b40c, and promotes the rating. The
// process-only 1080 effect, focus/repaint, channel stop, and WAVE/10008 calls
// are returned as explicit headless host requests.
[[nodiscard]] OriginalCathedralArrivalResult
apply_original_cathedral_arrival_check(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept;

// Complete type-36 family call, including 6037's conditional call into
// 1040:00f0 and its PART-backed process context.
[[nodiscard]] OriginalCathedralPersonDispatchResult
dispatch_original_cathedral_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPartTable& part) noexcept;

// Complete type-14 callback at `1220:67cf`, including both large helpers at
// `10f8:0701/0c06`, their shared `10f8:104a` movement primitive, bomb search
// completion, six-floor fire partitioning, and the process-only DS:77aa
// acceleration flag. The 1080:0000 view/effect call is returned explicitly.
[[nodiscard]] OriginalSecurityPersonStepResult
step_original_security_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPartTable& part) noexcept;

// Exact `1220:0f85 -> 6764` all-Security pass used by 1090:03ab while b406
// bomb/fire bits are active. It walks floor tenant records in order and then
// each facility's six-person span; no GUI or audio work is performed here.
[[nodiscard]] OriginalSecurityPeopleStepResult
step_original_security_people(
    OriginalTdtDocument& document,
    const OriginalPartTable& part);

// Exact type-15 state table at `1220:6383` plus its Hotel-room helpers at
// `1150:0000/01f5/03f3`. Each of the six records owns one floor modulo six,
// searches dirty Hotel rooms upward then downward, uses the common resolver's
// service-transport branch, cleans for exactly three callbacks, and restores
// the first Hotel guest before returning home.
[[nodiscard]] OriginalHousekeepingPersonStepResult
step_original_housekeeping_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    const OriginalPersonRouteRequest& route_context) noexcept;

// Exact type-15 subset of the normal `1220:0daf -> 6297` per-frame pass.
// The original starts at b3de modulo sixteen and visits every sixteenth
// person; bomb/fire bits select the separate Security pass and suppress this
// scan entirely. Wrapper state/transport/time gates are preserved.
[[nodiscard]] OriginalHousekeepingPeopleStepResult
step_original_housekeeping_people(
    OriginalTdtDocument& document,
    const OriginalPartTable& part) noexcept;

// Exact type-33 raw state table at `1220:5227` and its commercial-service
// helpers at `11a8:1472/12dc/1061/0cc2/0f11/0bd5`: Microsoft-runtime random
// destination selection, service population/status thresholds, dwell timing,
// completed-Stair release, and both common-route legs are retained.
[[nodiscard]] OriginalMetroPersonStepResult step_original_metro_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    const OriginalPartTable& part) noexcept;

// Exact shared Restaurant/Fast Food visitor table at `1220:4bde`, including
// `11a8:10b3/1159` reservation accounting, the common commercial entry/dwell
// helpers, and `11a8:1197/174e/17eb` attendance-history adjustment. Runtime
// tenant offsets +0x0a/+0x0c map to serialized bytes +4/+6 because each floor
// allocation begins with its six-byte header.
[[nodiscard]] OriginalFoodServicePersonStepResult
step_original_food_service_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    std::int16_t owner_key,
    const OriginalPartTable& part) noexcept;

// Exact Retail visitor table at `1220:4453` and wrapper `1220:426c`. It shares
// the commercial reservation/entry/history primitives, while preserving the
// Retail-only inactive-store activation at `1178:1140`: rent and population
// accounting, service/tenant mutation, all 48 person-metric resets, and the
// process-only `1118:0a49(6)` visual request returned to the host.
[[nodiscard]] OriginalRetailPersonStepResult step_original_retail_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    std::int16_t owner_key,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) noexcept;

// Exact shared Movie Theater/Party Hall visitor callback at `1220:5734`.
// This retains all eight CS:5ebd states, dc24 side-capacity reservation and
// rollback, arrival counters/dirtying, commercial detours, dwell handling,
// common routing, and completed-Stair release.
[[nodiscard]] OriginalEntertainmentPersonStepResult
step_original_entertainment_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    std::int16_t owner_key,
    const OriginalPartTable& part) noexcept;

// Exact type-9 Condo callback at `1220:3c09`: all twelve CS:423c states,
// occupancy/status transitions, three-resident synchronization, commercial
// detours, inactive-unit reactivation, and route-return tables.
[[nodiscard]] OriginalCondoPersonStepResult step_original_condo_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    std::int16_t owner_key,
    std::uint16_t owner_ordinal,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) noexcept;

// Exact Hotel type-3..5 callback at `1220:3154`: all ten CS:38b9 states,
// room occupancy synchronization, parking allocation/cleanup, tracked
// special-guest transactions, commercial detours, activation/checkout
// accounting, common routing, and completed-Stair release.
[[nodiscard]] OriginalHotelPersonStepResult step_original_hotel_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    std::int16_t owner_key,
    std::uint16_t owner_ordinal,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income);

// Exact normal type-7 callback at `1220:23e4`: all sixteen CS:2e52
// states, Office occupancy bands, parking commute, commercial and Medical
// Center detours, inactive-office activation, common routing, and completed-
// Stair release. This is distinct from step_original_office_person, which is
// the type-7 branch reached by the shared transit dispatcher at 1220:1aed.
[[nodiscard]] OriginalOfficeNormalPersonStepResult
step_original_office_normal_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::int16_t owner_floor,
    std::int16_t owner_key,
    std::uint16_t owner_ordinal,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income);

// Complete raw family switch at `1220:16ab`. `elevator_car_0883` selects the
// otherwise-identical demolition/car-arrival switch, whose only additional
// live branch is type-14 Security at `1220:67cf`.
[[nodiscard]] OriginalPersonFamilyDispatchResult
dispatch_original_person_family(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{},
    OriginalPersonFamilyDispatchSource source =
        OriginalPersonFamilyDispatchSource::dispatcher_16ab);

// Complete live family switch of the normal `1220:0daf` sixteen-way pass.
// Hotel, Office, Retail, Restaurant/Fast Food, Housekeeping, Metro,
// Cathedral, Movie Theater/Party Hall, and Condo records are resolved
// and dispatched in original person-index order, retaining every recovered
// `1220:2e92/1220:2068/1220:426c/1220:49fa/1220:6297/1220:50e2/1220:5edd/`
// `1220:55b8/1220:38e1` wrapper gate. Elevator-wait
// states execute the complete signed `1220:1637` timeout and ordered
// `1210:1b41 -> 1332 -> 1220:1aed` cross-family ring dispatch.
[[nodiscard]] OriginalTranslatedPeopleStepResult
step_original_translated_people(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{});

// Exact tracked/untracked Stair-span predicates at 11b0:0dc0/0e80 and the
// general/odd-only Stair candidate scorers at 11b0:141c/14c9. Public core
// boundaries retain the original six-/three-floor cutoffs, byte flags,
// direction checks, and wrapping signed score.
[[nodiscard]] bool original_full_stair_span_available(
    const OriginalTdtDocument& document,
    std::int16_t source_floor,
    std::int16_t destination_floor) noexcept;
[[nodiscard]] bool original_odd_stair_span_available(
    const OriginalTdtDocument& document,
    std::int16_t source_floor,
    std::int16_t destination_floor) noexcept;
[[nodiscard]] std::optional<std::int16_t> score_original_stair(
    const OriginalTdtStairRecord& stair,
    std::int16_t source_floor,
    std::int16_t destination_floor,
    std::uint16_t person_x,
    bool odd_only,
    bool& direction_up) noexcept;

// Shared exact 11b0:0a21/0ad4 transfer scan. The first of sixteen db9c
// records containing the source route bit, a different floor, and any bit in
// the destination graph decides the direction.
[[nodiscard]] bool find_original_transfer_direction(
    const OriginalTdtDocument& document,
    std::size_t source_bit,
    std::int16_t source_floor,
    std::uint32_t destination_graph,
    bool& direction_up) noexcept;

// Exact type-7 occupancy byte transitions at 1220:6bef/6cb6. These are
// public core boundaries so the Win16 byte wrap, late-day zero-to-eight
// transition, and dirty mark can be tested independently of person routing.
void enter_original_office(OriginalTdtTenant& owner) noexcept;
void leave_original_office(OriginalTdtDocument& document,
                           OriginalTdtTenant& owner) noexcept;

// Exact 1170:056f Medical Center selector. The Office floor chooses one of
// seven 22-byte route banks, an empty bank falls back to bank zero, and one
// Microsoft-rand result selects a signed service index from its live words.
[[nodiscard]] std::int16_t select_original_medical_service(
    OriginalTdtDocument& document,
    std::int16_t owner_floor) noexcept;

// Exact type-7 branch of 1220:1aed, including the common 11d8:00fc/0000
// accounting calls and the two parallel-table destinations recovered from
// CS:2004/2014. movement_delta is the original word at DS:b3de.
[[nodiscard]] OriginalOfficePersonStepStatus step_original_office_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::uint16_t movement_delta);

// Exact complete cross-family arrival dispatcher at `1220:1aed`, including
// its common metric calls, Hotel/Office/Condo occupancy transitions,
// Restaurant/Fast Food/Retail attendance history, parking/special-visitor
// cleanup, and Housekeeping reset.
[[nodiscard]] OriginalTransitPersonDispatchResult
dispatch_original_transit_person(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::uint16_t movement_delta,
    const OriginalPartTable& part);

// Exact `1210:1b41` waiting-ring rotation. For an Elevator byte-8 code it
// drains entries through the requested person in queue order; a non-Elevator
// code directly invokes the shared transit dispatcher once.
[[nodiscard]] OriginalElevatorWaitingDispatchResult
dispatch_original_elevator_waiting_people(
    OriginalTdtDocument& document,
    std::size_t person_index,
    std::uint16_t movement_delta,
    const OriginalPartTable& part);

// Exact `1220:1637` timestamp/timeout gate. PART/1000 word zero is DS:dd7a;
// both subtraction and comparison retain the executable's signed 16-bit
// wraparound behavior.
[[nodiscard]] OriginalElevatorWaitTimeoutResult
step_original_elevator_wait_timeout(
    OriginalTdtDocument& document,
    std::size_t person_index,
    const OriginalPartTable& part);

// Exact type-7 helper from the nightly person reset at 1220:0000. The public
// complete switch below also handles every other live family branch.
[[nodiscard]] std::size_t reset_original_office_people_for_day(
    OriginalTdtDocument& document) noexcept;

// Complete family switch from 1220:0000 for every person type on its jump
// table: Hotel 3..5, Restaurant 6, Office 7, Condo 9, Retail 10, Fast Food
// 12, Security 14, Housekeeping 15, Movie 18, Party Hall 29, Metro 33, and
// Cathedral 36. Types outside that original switch remain byte-exact.
[[nodiscard]] std::size_t reset_original_people_for_day(
    OriginalTdtDocument& document) noexcept;

// Exact post-allocation/post-load transient reset at 10b0:031a. The original
// save contains all sixteen person bytes, but this reconstruction pass
// deliberately replaces the live movement/timer fields for only the listed
// facility families before simulation resumes.
[[nodiscard]] std::size_t initialize_original_people_runtime_state(
    OriginalTdtDocument& document) noexcept;

// Exact first post-load reconstruction pass at 10b0:0072. It normalizes the
// live tenant status/links and their referenced Retail/DC24 runtime records
// before 10b0:031a reconstructs person movement state. Persisted fields are
// updated exactly because the original writes back into the loaded blocks.
[[nodiscard]] std::size_t initialize_original_tenant_runtime_state(
    OriginalTdtDocument& document) noexcept;

// Exact scheduled person/transit cleanup at 1220:1059. It walks every
// tenant-owned person span through 1220:10af, releases active people from the
// persisted Stair/Escalator or Elevator passenger/waiting tables, finalizes
// the 11d8 movement counters, and runs the unconditional type-7 parking exit
// path. frame_time is the original scheduler word DS:b3de consumed by
// 1210:1c46/11d8:00fc.
[[nodiscard]] std::size_t sweep_original_people_transit(
    OriginalTdtDocument& document,
    std::uint16_t frame_time) noexcept;

// Exact person-selection geometry at 10a8:0aae. The executable rebuilds a
// per-floor list of Elevator shafts, shell-sorts it by x, divides the open
// floor spans at the same integer midpoints, then walks each persisted
// forty-entry waiting ring in display order. This returns the selected
// persisted person instead of invoking 1100:0000's modeless information
// window, leaving that host action explicit for the native UI layer.
[[nodiscard]] OriginalElevatorWaitingPersonHit
original_elevator_waiting_person_hit_from_client(
    const OriginalTdtDocument& document,
    int client_x,
    int client_y,
    int view_x,
    int view_y) noexcept;

// Exact selection predicate of 1218:0000 before its family-state calls.
// It scans the persisted people table for active Hotel, Office, Condo,
// Retail, Restaurant/Fast Food, Metro, Entertainment, Cathedral, and
// Housekeeping people whose signed byte-8 transport index matches the
// selected bd70 record. Housekeeping uses the original lower state threshold
// of three; all other handled families require signed state >= 0x40.
[[nodiscard]] std::size_t count_original_vertical_transport_cleanup_people(
    const OriginalTdtDocument& document,
    std::size_t transport_index) noexcept;

// Complete `1218:0000` person pass used before Stair/Escalator demolition.
// It retains the executable's signed state/index gates and person-table order,
// then invokes the exact raw family callback with the owner bytes/word stored
// in each person record. Process-only requests remain explicit in the returned
// family results for the native host.
[[nodiscard]] OriginalVerticalTransportPeopleCleanupResult
cleanup_original_vertical_transport_people(
    OriginalTdtDocument& document,
    std::size_t transport_index,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{});

// Exact family destination helpers at 1220:685d/692c/69ae/6aba/6b11,
// followed by 11b0:092f's serviced-floor/transfer-floor selection.
[[nodiscard]] OriginalElevatorBoardingDestination
select_original_elevator_boarding_destination(
    const OriginalTdtDocument& document,
    std::size_t person_index,
    std::size_t elevator_index,
    std::int16_t current_floor,
    bool direction_up) noexcept;

// Exact passenger half of the per-car pass: 1210:07a6 -> 0883 removes every
// arrival for the open floor, then 1210:0351 -> 1332/0f0e/1a3b boards the applicable
// waiting lane(s), including direction reversal, capacity, occupancy and
// one-frame transfer-visual semantics. Car movement/door advancement remains
// the distinct 1090:06fb pass.
[[nodiscard]] OriginalElevatorPassengerStepResult
step_original_elevator_car_passengers(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{},
    bool isolation_active = false,
    std::function<void(const OriginalPersonFamilyDispatchResult&)>
        family_dispatch_callback = {});

// Exact 1090:06fb movement/countdown/door/assignment state machine. Calendar
// and day schedule selectors are reconstructed from the persisted tower clock.
[[nodiscard]] OriginalElevatorCarStepResult
step_original_elevator_car_state(OriginalTdtDocument& document,
                                 std::size_t elevator_index,
                                 std::size_t car_index,
                                 std::function<void()>
                                     movement_sound_callback = {});

struct OriginalElevatorFrameHostHooks {
  std::function<void()> movement_sound{};
  std::function<void(const OriginalPersonFamilyDispatchResult&)>
      family_dispatch{};
  std::function<void()> elevator_checkpoint{};
};

// Exact per-Elevator two-loop order inside 1090:03ab: for each used shaft,
// run all eight car-state passes, then all eight alighting/boarding pairs,
// then the direct 11e0:0e84 host checkpoint before advancing to the next
// shaft. The optional callback exposes only that host boundary; gameplay
// ordering remains in this frame-level entry point.
[[nodiscard]] OriginalElevatorFrameStepResult
step_original_elevator_frame(
    OriginalTdtDocument& document,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{},
    bool isolation_active = false,
    OriginalElevatorFrameHostHooks host_hooks = {});

// Exact persisted-data mutation performed by 1210:1ac5. The original copies
// the selected car-slot person index to its return object, writes ff to the
// parallel destination-floor byte, and writes ffffffff to the person-index
// dword. It deliberately does not update the car's passenger, destination,
// or per-floor occupancy counts; those belong to its callers.
[[nodiscard]] std::optional<std::uint32_t>
pop_original_elevator_car_passenger_slot(OriginalTdtDocument& document,
                                         std::size_t elevator_index,
                                         std::size_t car_index,
                                         std::size_t slot_index) noexcept;

// Exact 1090:0dfc car selector used when a floor's up/down waiting lane has
// no owner. direction_up is the original nonzero word argument. The
// schedule comparison consumes DS:b3a0/b3a1, supplied here explicitly so the
// persisted-data layer remains independent of the native host clock.
[[nodiscard]] OriginalElevatorAssignmentSelection
select_original_elevator_assignment_car(const OriginalTdtElevator& elevator,
                                        std::int16_t floor,
                                        bool direction_up,
                                        std::uint8_t calendar_phase,
                                        std::int8_t day_phase) noexcept;

// Exact 1090:0a4c mutation around the selector: leave an existing owner or
// an immediately available car untouched; otherwise store car+1 in block_2a2
// (up) or block_31a (down), increment that car's word-10 assignment count,
// and run the already translated 1090:0bcf car-target recomputation.
[[nodiscard]] bool assign_original_elevator_waiting_floor(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    bool direction_up,
    std::uint8_t calendar_phase,
    std::int8_t day_phase) noexcept;

// Public boundary for the exact `1090:0bcf` recomputation shared by the
// queue-assignment and Elevator Finger cleanup callers. It selects the
// primary target, direction, released floor owners, and secondary target.
void recompute_original_elevator_car_state(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index) noexcept;

// Exact individual cleanup primitives used by shaft shrinking. Unlike
// `10a0:14cc`, both `10a0:0819` and `10a0:0b87` drain every removed floor's
// `1625` waiting rings before moving the endpoint and then run `14fa` for
// every removed floor. These entry points keep that ordering expressible
// without folding the two operations back into the ordinary stop-removal
// transaction below.
[[nodiscard]] OriginalElevatorFloorPeopleCleanupResult
cleanup_original_elevator_waiting_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    std::uint16_t waiting_delay,
    OriginalPersonFamilyDispatch dispatch,
    void* context = nullptr) noexcept;
[[nodiscard]] OriginalElevatorFloorPeopleCleanupResult
cleanup_original_elevator_car_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    OriginalPersonFamilyDispatch dispatch,
    void* context = nullptr,
    bool suppress_car_family_dispatch = false) noexcept;

// Exact single-car `10a0:154a` primitive used by `10a0:036e` car
// demolition. It performs the same arrival/owner/recompute work as the car
// half above, but only for the selected active car.
[[nodiscard]] OriginalElevatorFloorPeopleCleanupResult
cleanup_original_elevator_selected_car_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index,
    std::int16_t floor,
    OriginalPersonFamilyDispatch dispatch,
    void* context = nullptr,
    bool suppress_car_family_dispatch = false) noexcept;

// Exact `10a0:14cc -> 14fa/154a/1625` cleanup after a service byte has been
// toggled off. Car passengers are popped through `1210:0883` order, direction
// owners are released through `154a`, and both floor-record rings are drained
// through `1625`. A null dispatcher performs no mutation when any person is
// present. `waiting_delay` is DS:dd7e, added by `11d8:02f7` for non-type-2
// elevator queues before the family callback. The original b3ae suppression
// of car-arrival family callbacks is supplied explicitly.
[[nodiscard]] OriginalElevatorFloorPeopleCleanupResult
cleanup_original_elevator_service_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    std::uint16_t waiting_delay,
    OriginalPersonFamilyDispatch dispatch,
    void* context = nullptr,
    bool suppress_car_family_dispatch = false) noexcept;

// Self-contained overload of the same `10a0:14cc` cleanup. It supplies the
// native `1210:0883/1220:16ab` family dispatcher and returns all process-only
// visual/audio/notification requests instead of requiring an external
// callback or the original executable.
[[nodiscard]] OriginalNativeElevatorFloorPeopleCleanupResult
cleanup_original_elevator_service_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    std::uint16_t waiting_delay,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{},
    bool suppress_car_family_dispatch = false) noexcept;

// Self-contained native overloads of the two shrink-order primitives above.
[[nodiscard]] OriginalNativeElevatorFloorPeopleCleanupResult
cleanup_original_elevator_waiting_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    std::uint16_t waiting_delay,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{}) noexcept;
[[nodiscard]] OriginalNativeElevatorFloorPeopleCleanupResult
cleanup_original_elevator_car_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{},
    bool suppress_car_family_dispatch = false) noexcept;
[[nodiscard]] OriginalNativeElevatorFloorPeopleCleanupResult
cleanup_original_elevator_selected_car_floor_people(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index,
    std::int16_t floor,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income = OriginalYenTable{},
    bool suppress_car_family_dispatch = false) noexcept;

// Exact facility-owned person cleanup performed by 11f8:35ac through
// 1220:10af with its literal nonzero third argument. It first executes the
// same family-specific transit/Office cleanup as the scheduled sweep, then
// retires the referenced person span, removes dce4 links, and applies the
// Hotel parking/special-visitor side effects.
[[nodiscard]] OriginalFacilityPeopleCleanupResult
cleanup_original_facility_people(OriginalTdtDocument& document,
                                 std::int16_t floor,
                                 std::size_t tenant_index,
                                 std::uint16_t frame_time) noexcept;

// Exact pre-midnight tenant sweep at 1228:086b (frame 09c4). Low Hotel and
// Condo status bands become 0x10; low Office bands become 0x08.
[[nodiscard]] std::size_t prepare_original_facilities_for_night(
    OriginalTdtDocument& document) noexcept;

// Exact tenant-state sweep at 1228:0968, scheduled at frame zero before the
// per-family day-start calls. calendar_phase is DS:b3a0 from 1200:0558.
[[nodiscard]] std::size_t advance_original_facilities_for_day(
    OriginalTdtDocument& document,
    std::uint8_t calendar_phase) noexcept;

// Exact complementary tenant-state sweep at 1228:0b59, scheduled at frame
// 0x0640. It advances occupied Hotel/Office/Condo bands and selects frame one
// for Metro and Cathedral parts.
[[nodiscard]] std::size_t advance_original_facilities_for_evening(
    OriginalTdtDocument& document) noexcept;

// Exact self-contained Hotel pair repair at 1130:01e2. Qualifying type 3..5
// records at status 0x38 or above refresh adjacent Hotel records and preserve
// the original loop's unusual skip/revisit behavior. day_phase is DS:b3a1.
[[nodiscard]] std::size_t repair_original_hotel_pair_states(
    OriginalTdtDocument& document,
    std::int8_t day_phase) noexcept;

// Exact scheduled dce4 person-link filters at 1188:0977 and 1188:0a20.
// Entries are persisted 32-bit person-record indices, not Win16 pointers;
// removal shifts the table, decrements b402, and writes the -1 tail sentinel.
[[nodiscard]] std::size_t remove_original_nightly_person_links(
    OriginalTdtDocument& document) noexcept;
[[nodiscard]] std::size_t remove_original_hotel_person_links(
    OriginalTdtDocument& document) noexcept;

// Exact scheduled Recycling Center family in segment 1088. The phase pass is
// invoked with requested phases 0..5 at frames 0640..0a06; its internally
// selected phase comes from 1088:0250's signed population-per-center bands.
[[nodiscard]] OriginalRecyclingStepResult advance_original_recycling_phase(
    OriginalTdtDocument& document,
    std::uint8_t requested_phase) noexcept;

// Exact frame-zero pass at 1088:00de and its frame-0020 companion at
// 1088:01d1. Notification and sound requests are returned to keep the state
// translation testable without opening any UI or audio device.
[[nodiscard]] OriginalRecyclingStepResult reset_original_recycling_for_day(
    OriginalTdtDocument& document) noexcept;
[[nodiscard]] std::size_t finish_original_recycling_day_start(
    OriginalTdtDocument& document) noexcept;

}  // namespace simtower
