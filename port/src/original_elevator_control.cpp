#include "original_elevator_control.hpp"

#include "original_people.hpp"
#include "original_simulation.hpp"

#include <algorithm>
#include <bit>
#include <cstdio>
#include <utility>

namespace simtower {
namespace {

constexpr OriginalDtmpRect kGridRectangle{18U, 195U, 136U, 391U};

std::int16_t signed_byte(std::byte value) noexcept {
  return std::bit_cast<std::int8_t>(
      std::to_integer<std::uint8_t>(value));
}

std::uint16_t load_u16(const std::array<std::byte, 346>& bytes,
                       std::size_t offset,
                       bool byte_swapped) noexcept {
  const auto first = std::to_integer<std::uint8_t>(bytes[offset]);
  const auto second = std::to_integer<std::uint8_t>(bytes[offset + 1U]);
  return byte_swapped
      ? static_cast<std::uint16_t>((first << 8U) | second)
      : static_cast<std::uint16_t>(first | (second << 8U));
}

bool state_elevator(const OriginalTdtDocument& document,
                    const OriginalElevatorControlState& state,
                    const OriginalTdtElevator*& elevator) noexcept {
  if (!state.valid || state.elevator_index >= document.elevators.size() ||
      state.schedule_bank >= 2U || state.day_phase >= 6U) {
    return false;
  }
  elevator = &document.elevators[state.elevator_index];
  return elevator->used != 0U;
}

bool state_elevator(OriginalTdtDocument& document,
                    const OriginalElevatorControlState& state,
                    OriginalTdtElevator*& elevator) noexcept {
  const OriginalTdtElevator* source{};
  if (!state_elevator(
          static_cast<const OriginalTdtDocument&>(document), state,
          source)) {
    return false;
  }
  elevator = &document.elevators[state.elevator_index];
  return true;
}

bool point_in_rect(const OriginalDtmpRect& rectangle,
                   int x,
                   int y) noexcept {
  return x >= rectangle.left && x < rectangle.right &&
         y >= rectangle.top && y < rectangle.bottom;
}

bool floor_mode_allowed(const OriginalTdtDocument& document,
                        const OriginalTdtElevator& elevator,
                        std::int16_t floor) noexcept {
  if (elevator.type == 0U && floor > 10 && (floor - 9) % 15 != 0) {
    return false;
  }
  return !(floor >= 11 &&
           floor < static_cast<std::int16_t>(
                       10U + document.header.lobby_height));
}

}  // namespace

OriginalElevatorControlState make_original_elevator_control_state(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::uint8_t calendar_phase,
    std::int8_t day_phase) noexcept {
  OriginalElevatorControlState state{};
  if (elevator_index >= document.elevators.size()) return state;
  const auto& elevator = document.elevators[elevator_index];
  if (elevator.used == 0U || elevator.bottom_floor > elevator.top_floor) {
    return state;
  }
  state.valid = true;
  state.elevator_index = elevator_index;
  state.schedule_bank = calendar_phase < 2U ? calendar_phase : 0U;
  // 1098:0786-0793 copies signed DS:b3a1 byte-for-byte and changes only the
  // exact phase-six value to five. It does not clamp other persisted values.
  state.day_phase = day_phase == 6
                        ? 5U
                        : std::bit_cast<std::uint8_t>(day_phase);
  state.scroll_min = elevator.bottom_floor;
  if (static_cast<int>(elevator.top_floor) - elevator.bottom_floor > 14) {
    state.scroll_max = static_cast<std::int16_t>(elevator.top_floor - 14);
    state.scroll_position = state.scroll_max;
  } else {
    state.scroll_max = state.scroll_min;
    state.scroll_position = state.scroll_min;
  }
  return state;
}

std::string original_elevator_control_title(
    const OriginalResources& resources,
    const OriginalTdtDocument& document,
    std::size_t elevator_index) {
  if (elevator_index >= document.elevators.size()) return {};
  return original_strl_entry(
      resources.find("STRL", 400),
      static_cast<std::size_t>(document.elevators[elevator_index].type) + 1U);
}

std::int16_t original_elevator_control_waiting_value(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state) noexcept {
  if (state.schedule_bank >= 2U || state.day_phase >= 6U) return 0;
  return signed_byte(elevator.schedule[original_elevator_control_schedule_index(
      14U, state.schedule_bank, state.day_phase)]);
}

std::int16_t original_elevator_control_departure_units(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state) noexcept {
  if (state.schedule_bank >= 2U || state.day_phase >= 6U) return 0;
  return signed_byte(elevator.schedule[original_elevator_control_schedule_index(
      42U, state.schedule_bank, state.day_phase)]);
}

std::uint8_t original_elevator_control_floor_mode(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state) noexcept {
  if (state.schedule_bank >= 2U || state.day_phase >= 6U) return 0U;
  return std::to_integer<std::uint8_t>(elevator.schedule[
      original_elevator_control_schedule_index(
          28U, state.schedule_bank, state.day_phase)]);
}

std::string original_elevator_control_waiting_text(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state) {
  char text[16]{};
  std::snprintf(text, sizeof(text), "%2d",
                static_cast<int>(
                    original_elevator_control_waiting_value(elevator,
                                                            state)));
  return text;
}

std::string original_elevator_control_departure_text(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state) {
  char text[16]{};
  std::snprintf(text, sizeof(text), "%2d",
                static_cast<int>(
                    original_elevator_control_departure_units(elevator,
                                                              state)) *
                    30);
  return text;
}

bool original_elevator_control_select_bank(
    OriginalElevatorControlState& state,
    std::uint8_t bank) noexcept {
  if (!state.valid || bank >= 2U || state.schedule_bank == bank) return false;
  state.schedule_bank = bank;
  return true;
}

bool original_elevator_control_select_phase(
    OriginalElevatorControlState& state,
    std::uint8_t phase) noexcept {
  if (!state.valid || phase >= 6U || state.day_phase == phase) return false;
  state.day_phase = phase;
  return true;
}

bool original_elevator_control_adjust_waiting(
    OriginalTdtDocument& document,
    const OriginalElevatorControlState& state,
    int delta) noexcept {
  OriginalTdtElevator* elevator{};
  if (!state_elevator(document, state, elevator)) return false;
  const auto index = original_elevator_control_schedule_index(
      14U, state.schedule_bank, state.day_phase);
  const int old_value =
      std::to_integer<std::uint8_t>(elevator->schedule[index]);
  const int value = std::clamp(old_value + delta, 1, 100);
  if (value == old_value) return false;
  elevator->schedule[index] = static_cast<std::byte>(value);
  return true;
}

bool original_elevator_control_adjust_departure(
    OriginalTdtDocument& document,
    const OriginalElevatorControlState& state,
    int delta) noexcept {
  OriginalTdtElevator* elevator{};
  if (!state_elevator(document, state, elevator)) return false;
  const auto index = original_elevator_control_schedule_index(
      42U, state.schedule_bank, state.day_phase);
  const int old_value =
      std::to_integer<std::uint8_t>(elevator->schedule[index]);
  const int value = std::clamp(old_value + delta, 0, 3);
  if (value == old_value) return false;
  elevator->schedule[index] = static_cast<std::byte>(value);
  return true;
}

bool original_elevator_control_set_floor_mode(
    OriginalTdtDocument& document,
    const OriginalElevatorControlState& state,
    std::uint8_t mode) noexcept {
  OriginalTdtElevator* elevator{};
  if (!state_elevator(document, state, elevator) || mode >= 3U) return false;
  const auto index = original_elevator_control_schedule_index(
      28U, state.schedule_bank, state.day_phase);
  if (elevator->schedule[index] == static_cast<std::byte>(mode)) return false;
  elevator->schedule[index] = static_cast<std::byte>(mode);
  return true;
}

std::uint16_t original_elevator_control_show_bitmap(
    const OriginalTdtElevator& elevator) noexcept {
  return elevator.word_3c != 0U ? 407U : 408U;
}

bool original_elevator_control_toggle_show(
    OriginalTdtDocument& document,
    std::size_t elevator_index) noexcept {
  if (elevator_index >= document.elevators.size() ||
      document.elevators[elevator_index].used == 0U) {
    return false;
  }
  auto& show = document.elevators[elevator_index].word_3c;
  show = show != 0U ? 0U : 1U;
  return true;
}

bool original_elevator_control_has_scrollbar(
    const OriginalElevatorControlState& state) noexcept {
  return state.valid && state.scroll_max > state.scroll_min;
}

bool original_elevator_control_scroll(
    OriginalElevatorControlState& state,
    OriginalElevatorControlScrollCommand command,
    std::int16_t thumb_position) noexcept {
  if (!state.valid || state.scroll_max <= state.scroll_min) return false;
  int requested = state.scroll_position;
  switch (command) {
    case OriginalElevatorControlScrollCommand::line_up:
      --requested;
      break;
    case OriginalElevatorControlScrollCommand::line_down:
      ++requested;
      break;
    case OriginalElevatorControlScrollCommand::page_up:
      requested -= 14;
      break;
    case OriginalElevatorControlScrollCommand::page_down:
      requested += 14;
      break;
    case OriginalElevatorControlScrollCommand::thumb_position:
    case OriginalElevatorControlScrollCommand::thumb_track:
      requested = thumb_position;
      break;
  }
  requested = std::clamp(
      requested, static_cast<int>(state.scroll_min),
      static_cast<int>(state.scroll_max));
  if (requested == state.scroll_position) return false;
  state.scroll_position = static_cast<std::int16_t>(requested);
  return true;
}

std::int16_t original_elevator_control_visible_lowest_floor(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state) noexcept {
  if (!original_elevator_control_has_scrollbar(state)) {
    return elevator.bottom_floor;
  }
  return static_cast<std::int16_t>(
      elevator.bottom_floor + state.scroll_max - state.scroll_position);
}

std::int16_t original_elevator_control_visible_floor(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state,
    std::int16_t row) noexcept {
  return static_cast<std::int16_t>(
      original_elevator_control_visible_lowest_floor(elevator, state) + row);
}

OriginalDtmpRect original_elevator_control_cell_rect(
    std::int16_t visual_column,
    std::int16_t row) noexcept {
  const int left = static_cast<int>(kGridRectangle.left) + 1 +
                   (static_cast<int>(visual_column) + 1) *
                       kOriginalElevatorControlCellSize;
  const int top = static_cast<int>(kGridRectangle.top) + 1 +
                  (14 - static_cast<int>(row)) *
                      kOriginalElevatorControlCellSize;
  return {
      static_cast<std::uint16_t>(left),
      static_cast<std::uint16_t>(top),
      static_cast<std::uint16_t>(left + 12),
      static_cast<std::uint16_t>(top + 12),
  };
}

std::optional<OriginalDtmpRect>
original_elevator_control_current_car_frame(
    const OriginalTdtDocument& document,
    const OriginalElevatorControlState& state,
    std::size_t car_index,
    std::int16_t visual_column) noexcept {
  // 1098:1e33 adds the shaft bottom floor to GetScrollPos/GetScrollRange,
  // reducing to bottom+max-position, then subtracts that visible-lowest
  // floor from the selected car's signed current-floor byte.
  const OriginalTdtElevator* elevator{};
  if (!state_elevator(document, state, elevator) || visual_column < 0 ||
      car_index >= elevator->car_records.size() ||
      elevator->car_records[car_index].exact_bytes[15] == std::byte{0}) {
    return std::nullopt;
  }
  const auto row = static_cast<std::int16_t>(
      signed_byte(elevator->car_records[car_index].exact_bytes[0]) -
      original_elevator_control_visible_lowest_floor(*elevator, state));
  if (row < 0 || row >= kOriginalElevatorControlVisibleFloors) {
    return std::nullopt;
  }
  return original_elevator_control_cell_rect(visual_column, row);
}

OriginalElevatorControlGridHit original_elevator_control_grid_hit(
    const OriginalTdtDocument& document,
    const OriginalElevatorControlState& state,
    int x,
    int y) noexcept {
  const OriginalTdtElevator* elevator{};
  if (!state_elevator(document, state, elevator)) return {};

  for (std::int16_t row = 0; row < kOriginalElevatorControlVisibleFloors;
       ++row) {
    if (point_in_rect(original_elevator_control_cell_rect(-1, row), x, y)) {
      return {OriginalElevatorControlGridKind::service_floor,
              original_elevator_control_visible_floor(*elevator, state, row),
              row, -1, -1};
    }
  }

  std::int16_t visual_column = 0;
  for (std::size_t car_index = 0U;
       car_index < elevator->car_records.size(); ++car_index) {
    if (elevator->car_records[car_index].exact_bytes[15] == std::byte{0}) {
      continue;
    }
    for (std::int16_t row = 0; row < kOriginalElevatorControlVisibleFloors;
         ++row) {
      if (point_in_rect(
              original_elevator_control_cell_rect(visual_column, row),
              x, y)) {
        return {
            OriginalElevatorControlGridKind::car,
            original_elevator_control_visible_floor(*elevator, state, row),
            row, visual_column, static_cast<std::int16_t>(car_index)};
      }
    }
    ++visual_column;
  }
  return {};
}

std::string original_elevator_control_floor_label(std::int16_t floor) {
  auto displayed = static_cast<std::int16_t>(floor - 9);
  if (displayed <= 0) --displayed;
  return std::to_string(displayed);
}

OriginalElevatorControlFloorCellPlan
original_elevator_control_floor_cell_plan(
    const OriginalTdtElevator& elevator,
    const OriginalElevatorControlState& state,
    std::int16_t row) {
  OriginalElevatorControlFloorCellPlan plan{};
  plan.floor = original_elevator_control_visible_floor(elevator, state, row);
  plan.above_top = plan.floor > elevator.top_floor;
  if (plan.above_top) return plan;

  plan.serviced = plan.floor >= 0 && plan.floor < 120 &&
      elevator.serviced_floors[static_cast<std::size_t>(plan.floor)] !=
          std::byte{0};
  plan.label = original_elevator_control_floor_label(plan.floor);
  const auto displayed = static_cast<std::int16_t>(plan.floor - 9);
  plan.small_font = displayed >= 100;
  plan.horizontal_inset = plan.small_font ? 3 : 1;
  plan.vertical_inset = plan.small_font ? -1 : 1;
  return plan;
}

std::uint16_t original_elevator_control_car_bitmap(
    const OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index,
    std::int16_t floor) noexcept {
  if (elevator_index >= document.elevators.size() || floor < 0 ||
      floor >= 120) {
    return 0U;
  }
  const auto& elevator = document.elevators[elevator_index];
  if (elevator.used == 0U || car_index >= elevator.car_records.size()) {
    return 0U;
  }
  const auto& car = elevator.car_records[car_index].exact_bytes;
  if (car[15] == std::byte{0}) return 0U;

  const auto current_floor = signed_byte(car[0]);
  const auto target_floor = signed_byte(car[13]);
  const auto home_floor = signed_byte(elevator.car_home_floors[car_index]);
  const auto pending = load_u16(car, 10U, document.header.byte_swapped);
  const auto floor_index = static_cast<std::size_t>(floor);
  const auto owner = static_cast<std::byte>(car_index + 1U);
  const bool alternate_direction = car[4] != std::byte{0};

  if (current_floor == floor) {
    if (home_floor == floor && pending == 0U && car[12] == std::byte{0}) {
      return kOriginalElevatorControlCarBitmapBase + 7U;
    }
    if (alternate_direction) {
      return car[3] == static_cast<std::byte>(elevator.type)
                 ? kOriginalElevatorControlCarBitmapBase + 8U
                 : kOriginalElevatorControlCarBitmapBase;
    }
    return car[3] == static_cast<std::byte>(elevator.type)
               ? kOriginalElevatorControlCarBitmapBase + 9U
               : kOriginalElevatorControlCarBitmapBase + 1U;
  }
  if (target_floor == floor) {
    return kOriginalElevatorControlCarBitmapBase +
           (alternate_direction ? 4U : 5U);
  }
  if (car[226U + floor_index] != std::byte{0}) {
    return kOriginalElevatorControlCarBitmapBase + 6U;
  }
  if (alternate_direction) {
    if (elevator.block_2a2[floor_index] == owner) {
      return kOriginalElevatorControlCarBitmapBase + 2U;
    }
    if (elevator.block_31a[floor_index] == owner) {
      return kOriginalElevatorControlCarBitmapBase + 3U;
    }
  } else {
    if (elevator.block_31a[floor_index] == owner) {
      return kOriginalElevatorControlCarBitmapBase + 3U;
    }
    if (elevator.block_2a2[floor_index] == owner) {
      return kOriginalElevatorControlCarBitmapBase + 2U;
    }
  }
  return 0U;
}

OriginalElevatorServiceFloorGate
original_elevator_control_service_floor_gate(
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
  if (elevator.used == 0U) {
    return OriginalElevatorServiceFloorGate::inactive_shaft;
  }
  if (floor < elevator.bottom_floor || floor > elevator.top_floor) {
    return OriginalElevatorServiceFloorGate::outside_shaft;
  }
  for (std::size_t index = 0U; index < elevator.car_records.size(); ++index) {
    if (elevator.car_records[index].exact_bytes[15] != std::byte{0} &&
        signed_byte(elevator.car_home_floors[index]) == floor) {
      return OriginalElevatorServiceFloorGate::active_car_home;
    }
  }
  if (elevator.serviced_floors[static_cast<std::size_t>(floor)] ==
          std::byte{0} &&
      !floor_mode_allowed(document, elevator, floor)) {
    return OriginalElevatorServiceFloorGate::forbidden_new_stop;
  }
  return OriginalElevatorServiceFloorGate::eligible;
}

bool original_elevator_control_add_service_floor(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor) noexcept {
  if (original_elevator_control_service_floor_gate(
          document, elevator_index, floor) !=
      OriginalElevatorServiceFloorGate::eligible) {
    return false;
  }
  auto& serviced = document.elevators[elevator_index]
                       .serviced_floors[static_cast<std::size_t>(floor)];
  if (serviced != std::byte{0}) return false;
  serviced = std::byte{1};
  rebuild_original_transport_route_graphs(document);
  return true;
}

OriginalNativeElevatorFloorPeopleCleanupResult
original_elevator_control_remove_service_floor(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::int16_t floor,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) noexcept {
  OriginalNativeElevatorFloorPeopleCleanupResult result{};
  if (original_elevator_control_service_floor_gate(
          document, elevator_index, floor) !=
      OriginalElevatorServiceFloorGate::eligible) {
    return result;
  }
  if (document.elevators[elevator_index]
          .serviced_floors[static_cast<std::size_t>(floor)] ==
      std::byte{0}) {
    return result;
  }

  auto working = document;
  working.elevators[elevator_index]
      .serviced_floors[static_cast<std::size_t>(floor)] = std::byte{0};
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

bool original_elevator_control_set_car_home(
    OriginalTdtDocument& document,
    std::size_t elevator_index,
    std::size_t car_index,
    std::int16_t floor) noexcept {
  if (elevator_index >= document.elevators.size() || floor < 0 ||
      floor >= 120) {
    return false;
  }
  auto& elevator = document.elevators[elevator_index];
  if (elevator.used == 0U || car_index >= elevator.car_records.size() ||
      elevator.car_records[car_index].exact_bytes[15] == std::byte{0} ||
      elevator.serviced_floors[static_cast<std::size_t>(floor)] ==
          std::byte{0} ||
      !floor_mode_allowed(document, elevator, floor)) {
    return false;
  }
  const auto encoded = static_cast<std::byte>(
      static_cast<std::uint8_t>(static_cast<std::int8_t>(floor)));
  if (elevator.car_home_floors[car_index] == encoded) return false;
  elevator.car_home_floors[car_index] = encoded;
  return true;
}

std::optional<std::uint8_t> original_elevator_control_popup_selection(
    int x,
    int y,
    int width,
    int height) noexcept {
  if (width < 0 || height < 0 || x < 0 || y < 0 || x > width ||
      y > height) {
    return std::nullopt;
  }
  return static_cast<std::uint8_t>((y * 3) / (height + 1));
}

std::optional<OriginalDtmpRect> original_elevator_control_popup_highlight(
    std::uint8_t mode,
    int width,
    int height) noexcept {
  if (mode > 2U || width < 0 || height < 0 || width > 0xffff ||
      height > 0xffff) {
    return std::nullopt;
  }
  const int row_height = height / 3;
  const int top = static_cast<int>(mode) * row_height;
  return OriginalDtmpRect{
      0U, static_cast<std::uint16_t>(top),
      static_cast<std::uint16_t>(width),
      static_cast<std::uint16_t>(top + row_height)};
}

bool begin_original_elevator_control_isolation(
    OriginalTdtDocument& document,
    OriginalElevatorControlState& state,
    bool& build_mode) noexcept {
  if (!state.valid || state.isolation_active ||
      state.elevator_index >= document.elevators.size()) {
    return false;
  }
  state.saved_build_mode = build_mode;
  for (std::size_t index = 0U; index < document.elevators.size(); ++index) {
    state.saved_elevator_used[index] = document.elevators[index].used;
    document.elevators[index].used =
        index == state.elevator_index ? 1U : 0U;
  }
  state.saved_elevator_record = document.elevators[state.elevator_index];
  state.isolation_active = true;
  build_mode = false;
  return true;
}

std::size_t original_elevator_control_preview_frame_count(
    const OriginalPartTable& part,
    std::uint16_t rating) noexcept {
  const std::size_t upper_index =
      rating == 1U || rating == 2U ? 8U : rating == 3U ? 9U : 10U;
  const auto doubled = static_cast<std::uint16_t>(
      static_cast<std::uint16_t>(part.words_00_to_40[upper_index]) * 2U);
  const auto signed_doubled = std::bit_cast<std::int16_t>(doubled);
  return signed_doubled > 0
             ? static_cast<std::size_t>(signed_doubled)
             : 0U;
}

OriginalElevatorControlPreviewResult
prepare_original_elevator_control_preview(
    OriginalTdtDocument& document,
    OriginalElevatorControlState& state,
    const OriginalPartTable& part,
    const OriginalYenTable& rent_income) {
  OriginalElevatorControlPreviewResult result{};
  if (!state.valid || !state.isolation_active ||
      state.elevator_index >= document.elevators.size() ||
      !state.saved_elevator_record) {
    return result;
  }

  result.frames = original_elevator_control_preview_frame_count(
      part, document.header.rating);
  for (std::size_t frame = 0U; frame < result.frames; ++frame) {
    document.header.frame_time = static_cast<std::uint16_t>(
        document.header.frame_time + 1U);
    const auto step = step_original_elevator_frame(
        document, part, rent_income, true);
    result.cars_changed += step.cars_changed;
    result.movement_sound_requests += step.movement_sound_requests;
  }
  document.header.frame_time = static_cast<std::uint16_t>(
      document.header.frame_time -
      static_cast<std::uint16_t>(result.frames));

  auto& live = document.elevators[state.elevator_index];
  const auto& saved = *state.saved_elevator_record;
  live.block_2a2 = saved.block_2a2;
  live.block_31a = saved.block_31a;
  for (auto& record : live.floor_records) {
    const auto source = std::find_if(
        saved.floor_records.begin(), saved.floor_records.end(),
        [&](const OriginalTdtElevatorFloorRecord& candidate) {
          return candidate.mapped_index == record.mapped_index;
        });
    if (source == saved.floor_records.end()) continue;
    std::copy_n(source->exact_bytes.begin(), 4U,
                record.exact_bytes.begin());
  }
  for (std::size_t car_index = 0U;
       car_index < live.car_records.size(); ++car_index) {
    auto& destination = live.car_records[car_index].exact_bytes;
    const auto& source = saved.car_records[car_index].exact_bytes;
    std::copy_n(source.begin(), 15U, destination.begin());
    std::copy(source.begin() + 16U, source.end(),
              destination.begin() + 16U);
  }
  result.prepared = true;
  return result;
}

bool resume_original_elevator_control_isolation(
    OriginalTdtDocument& document,
    OriginalElevatorControlState& state,
    bool& build_mode) noexcept {
  if (!state.valid || !state.isolation_active ||
      state.elevator_index >= document.elevators.size() ||
      !state.saved_elevator_record) {
    return false;
  }
  // 10f0:0719 retains precisely schedule offsets +04/+12/+20/+2e and
  // word_3c from the simulated record before the 0x345a restore.
  const auto current_schedule =
      document.elevators[state.elevator_index].schedule;
  const auto current_show =
      document.elevators[state.elevator_index].word_3c;
  document.elevators[state.elevator_index] =
      std::move(*state.saved_elevator_record);
  document.elevators[state.elevator_index].schedule = current_schedule;
  document.elevators[state.elevator_index].word_3c = current_show;
  for (std::size_t index = 0U; index < document.elevators.size(); ++index) {
    document.elevators[index].used = state.saved_elevator_used[index];
  }
  build_mode = state.saved_build_mode;
  state.isolation_active = false;
  state.saved_elevator_record.reset();
  return true;
}

}  // namespace simtower
