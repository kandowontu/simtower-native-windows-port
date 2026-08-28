#include "original_info.hpp"

#include "original_dib.hpp"
#include "original_font.hpp"
#include "original_tables.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace simtower {
namespace {

constexpr COLORREF kDarkText = RGB(0x26, 0x26, 0x26);
constexpr COLORREF kWeekendText = RGB(0xb3, 0x00, 0x00);
constexpr COLORREF kFieldGray = RGB(0x99, 0x99, 0x99);
constexpr COLORREF kStatusGray = RGB(0xd9, 0xd9, 0xd9);

struct SavedDc {
  HDC dc{};
  int state{};

  explicit SavedDc(HDC value) : dc(value), state(SaveDC(value)) {}
  ~SavedDc() {
    if (state != 0) RestoreDC(dc, state);
  }

  SavedDc(const SavedDc&) = delete;
  SavedDc& operator=(const SavedDc&) = delete;
};

void fill_color(HDC dc, const RECT& rectangle, COLORREF color) {
  HBRUSH brush = CreateSolidBrush(color);
  if (!brush) throw std::runtime_error("CreateSolidBrush failed for info window");
  FillRect(dc, &rectangle, brush);
  DeleteObject(brush);
}

HFONT make_original_font(int pixel_height) {
  return original_cached_font(static_cast<std::int16_t>(pixel_height));
}

struct SelectedFont {
  HDC dc{};
  HFONT font{};
  HGDIOBJ previous{};

  SelectedFont(HDC destination, int pixel_height)
      : dc(destination), font(make_original_font(pixel_height)) {
    if (font) previous = SelectObject(dc, font);
  }
  ~SelectedFont() {
    if (previous) SelectObject(dc, previous);
  }

  SelectedFont(const SelectedFont&) = delete;
  SelectedFont& operator=(const SelectedFont&) = delete;
};

void draw_clipped_dib(HDC dc,
                      std::span<const std::byte> resource,
                      int x,
                      int y,
                      const RECT& clip) {
  // 1208:0a42 installs a CreateRectRgnIndirect region, selects it as the clip,
  // and deletes it. SaveDC/IntersectClipRect/RestoreDC preserves that visible
  // result without leaking the clip into later native drawing.
  SavedDc saved(dc);
  IntersectClipRect(dc, clip.left, clip.top, clip.right, clip.bottom);
  draw_original_dib(dc, resource, x, y);
}

std::int32_t scaled_balance(std::int32_t balance) noexcept {
  // 1118:01c3 uses a 32-bit IMUL by 100 and keeps the low dword.
  const std::uint32_t wrapped =
      static_cast<std::uint32_t>(balance) * std::uint32_t{100};
  return std::bit_cast<std::int32_t>(wrapped);
}

std::string strl(const OriginalResources& resources, std::uint16_t index) {
  return original_strl_entry(resources.find("STRL", 713), index);
}

bool set_status(const OriginalResources& resources,
                OriginalInfoStatusState& state,
                std::int32_t resource_id,
                std::uint16_t string_index,
                std::int16_t priority,
                std::int16_t maximum_replaced_priority,
                std::uint32_t now_tick) {
  if (state.priority > maximum_replaced_priority) return false;
  if (string_index == 0U) {
    state.text.clear();
    state.started_tick = 0U;
    state.priority = 0;
    return true;
  }
  state.text = original_strl_entry(resources.find("STRL", resource_id),
                                   string_index);
  state.started_tick = now_tick;
  state.priority = priority;
  return true;
}

void draw_date(HDC dc, const OriginalInfoContent& content) {
  // 1118:0ba4 constructs this 152x16 field at (151,5); the caller's shared
  // eight-pixel palette-frame offset yields client coordinates (151,13).
  const RECT field{151, 13, 303, 29};
  fill_color(dc, field, kFieldGray);

  SelectedFont font(dc, 14);
  SetBkMode(dc, TRANSPARENT);
  SetTextAlign(dc, TA_UPDATECP | TA_BASELINE);
  MoveToEx(dc, 159, 28, nullptr);
  if (!content.date_red.empty()) {
    SetTextColor(dc, kWeekendText);
    TextOutA(dc, 0, 0, content.date_red.data(),
             static_cast<int>(content.date_red.size()));
  }
  SetTextColor(dc, kDarkText);
  TextOutA(dc, 0, 0, content.date_dark.data(),
           static_cast<int>(content.date_dark.size()));
}

void draw_rating(HDC dc,
                 const OriginalResources& resources,
                 std::uint16_t rating) {
  if (rating == 6U) {
    const RECT clip{42, 10, 148, 32};
    draw_clipped_dib(dc, resources.find("BITMAP", 327), clip.left, clip.top,
                     clip);
    return;
  }

  // 1118:0b67 returns each 21x19 star strip at x=43+(index-1)*21, y=3;
  // applying the palette-frame offset produces the rectangles below.
  for (int index = 1; index <= 5; ++index) {
    const RECT clip{43 + (index - 1) * 21, 11,
                    43 + index * 21, 30};
    const int bitmap = index <= static_cast<int>(rating) ? 322 : 323;
    draw_clipped_dib(dc, resources.find("BITMAP", bitmap), clip.left,
                     clip.top, clip);
  }
}

void draw_balance(HDC dc, const std::string& text) {
  // 1118:0bda's 70x14 base rectangle plus 1118:0143's (10,8) offset.
  const RECT field{354, 13, 424, 27};
  fill_color(dc, field, kFieldGray);

  SelectedFont font(dc, 13);
  SetTextColor(dc, kDarkText);
  SetBkMode(dc, TRANSPARENT);
  SetTextAlign(dc, TA_UPDATECP | TA_RIGHT | TA_BASELINE);
  MoveToEx(dc, field.right - 1, field.bottom - 1, nullptr);
  TextOutA(dc, 0, 0, text.data(), static_cast<int>(text.size()));
}

void draw_status(HDC dc, const std::string& text) {
  // 1118:0c0f's 262x11 status rectangle plus the eight-pixel client offset.
  const RECT field{41, 33, 303, 44};
  const RECT highlight{41, 44, 303, 45};
  fill_color(dc, field, kStatusGray);
  fill_color(dc, highlight, RGB(0xff, 0xff, 0xff));

  SelectedFont font(dc, 12);
  SetTextColor(dc, kDarkText);
  SetBkMode(dc, TRANSPARENT);
  SetTextAlign(dc, TA_UPDATECP | TA_BASELINE);
  // 1118:02ee-02fb establishes the DrawText current position explicitly.
  // With TA_UPDATECP/TA_BASELINE this is observable: omitting it can leave the
  // status string at the current position inherited from the balance painter.
  MoveToEx(dc, field.left + 2, field.bottom, nullptr);
  RECT text_rect = field;
  DrawTextA(dc, text.data(), static_cast<int>(text.size()), &text_rect,
            DT_SINGLELINE);
}

void draw_population(HDC dc, const std::string& text) {
  // 1118:0c44's 86x14 population rectangle plus the client-frame offset.
  const RECT field{337, 31, 423, 45};
  fill_color(dc, field, kFieldGray);

  SelectedFont font(dc, 13);
  SIZE extent{};
  GetTextExtentPoint32A(dc, text.data(), static_cast<int>(text.size()),
                        &extent);
  SetTextColor(dc, kDarkText);
  SetBkMode(dc, TRANSPARENT);
  SetTextAlign(dc, TA_UPDATECP | TA_BASELINE);
  MoveToEx(dc, field.right - extent.cx, field.bottom - 2, nullptr);
  TextOutA(dc, 0, 0, text.data(), static_cast<int>(text.size()));
}

void draw_clock(HDC dc,
                const OriginalResources& resources,
                const OriginalInfoContent& content) {
  // 11f8:033a precomposes BITMAP/321 into 1118:0cad's shared-surface source.
  // Each 1118:073d paint restores it into 1118:0c79's 31x31 local face at
  // (5,5), then applies the eight-pixel palette-frame offset used below.
  const RECT clip{5, 13, 36, 44};
  draw_clipped_dib(dc, resources.find("BITMAP", 321), clip.left, clip.top,
                   clip);

  // The two hands preserve 1208:0d35's relative-line primitive: get the
  // current position and LineTo(current.x+dx, current.y+dy).
  HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
  if (!pen) throw std::runtime_error("CreatePen failed for info clock");
  HGDIOBJ previous = SelectObject(dc, pen);
  constexpr int center_x = 20;
  constexpr int center_y = 28;
  MoveToEx(dc, center_x, center_y, nullptr);
  LineTo(dc, center_x + content.minute_hand.x,
         center_y + content.minute_hand.y);
  MoveToEx(dc, center_x, center_y, nullptr);
  LineTo(dc, center_x + content.hour_hand.x,
         center_y + content.hour_hand.y);
  SelectObject(dc, previous);
  DeleteObject(pen);
}

}  // namespace

bool set_original_info_construction_status(
    const OriginalResources& resources,
    OriginalInfoStatusState& state,
    std::uint16_t string_index,
    std::uint32_t now_tick) {
  // 1118:0933 is unconditional and loads STRL/1003 at priority two.
  return set_status(resources, state, 1003, string_index, 2, 0x7fff,
                    now_tick);
}

bool set_original_info_notification_status(
    const OriginalResources& resources,
    OriginalInfoStatusState& state,
    std::uint16_t string_index,
    std::uint32_t now_tick) {
  // 1118:09be is likewise unconditional, but uses STRL/1010.
  return set_status(resources, state, 1010, string_index, 2, 0x7fff,
                    now_tick);
}

bool set_original_info_income_status(const OriginalResources& resources,
                                     OriginalInfoStatusState& state,
                                     std::uint16_t string_index,
                                     std::uint32_t now_tick) {
  // 1118:0a49 returns without drawing while a positive-priority message is
  // live. Its STRL/1007 messages retain priority zero.
  return set_status(resources, state, 1007, string_index, 0, 0, now_tick);
}

bool set_original_info_command_status(const OriginalResources& resources,
                                      OriginalInfoStatusState& state,
                                      std::uint16_t string_index,
                                      std::uint32_t now_tick) {
  // 1118:0ad5 can replace income/command text but not priority-two text.
  return set_status(resources, state, 1009, string_index, 1, 1, now_tick);
}

bool expire_original_info_status(OriginalInfoStatusState& state,
                                 std::uint32_t now_tick) noexcept {
  if (state.started_tick == 0U) return false;
  // 1118:090c performs wrapping subtraction, then 1000:39ea returns the
  // signed dword's magnitude before the signed comparison with 300.
  const std::uint32_t magnitude =
      original_tick_magnitude_delta(now_tick, state.started_tick);
  if (std::bit_cast<std::int32_t>(magnitude) <= 300) return false;
  state.text.clear();
  state.started_tick = 0U;
  state.priority = 0;
  return true;
}

std::string original_info_ordinal(const OriginalResources& resources,
                                  std::int32_t value) {
  std::uint16_t suffix = 14U;
  if (value < 10 || value >= 20) {
    const std::int32_t remainder = value % 10;
    if (remainder >= 1 && remainder <= 3) {
      suffix = static_cast<std::uint16_t>(10 + remainder);
    }
  }
  return std::to_string(value) + strl(resources, suffix);
}

OriginalInfoClockTime original_info_clock_time(
    std::uint16_t frame_time) noexcept {
  const int phase = frame_time / 400U;
  const int delta = static_cast<int>(frame_time) - phase * 400;
  OriginalInfoClockTime result{};

  switch (phase) {
    case 0: {
      const int scaled = delta * 5;
      result.hour = scaled / 400 + 7;
      result.minute = (scaled % 400) * 60 / 400;
      break;
    }
    case 1:
      result.minute = delta * 60 / 800;
      break;
    case 2:
      result.minute = delta * 60 / 800 + 30;
      break;
    case 3:
    case 4:
    case 5: {
      const int scaled = delta * 4;
      result.hour = scaled / 400 + (phase == 3 ? 1 : phase == 4 ? 5 : 9);
      result.minute = (scaled % 400) * 60 / 400;
      if (result.hour >= 12) result.hour -= 12;
      break;
    }
    case 6: {
      const int scaled = delta * 12;
      result.hour = scaled / 400 + 1;
      result.minute = (scaled % 400) * 60 / 400;
      if (result.hour >= 12) result.hour -= 12;
      break;
    }
    default:
      break;
  }
  result.minute = std::min(result.minute, 59);
  return result;
}

OriginalInfoPoint original_info_clock_point(int index, int radius) noexcept {
  if (index < 0 || index >= 60 || radius < 0) return {};
  // 1200:0050 stores (index * 314) as a signed word, divides it by the
  // single-precision 3000.0 constant, and calls the runtime sin/cos helpers.
  // 1000:0f1c then FRNDINTs with the x87 round-down mode before 1000:1049
  // converts to an integer.
  const double angle = static_cast<double>(index * 314) / 3000.0;
  return {static_cast<int>(std::floor(std::sin(angle) * radius)),
          -static_cast<int>(std::floor(std::cos(angle) * radius))};
}

OriginalInfoContent build_original_info_content(
    const OriginalResources& resources,
    const OriginalTdtDocument* document,
    std::string_view status) {
  const std::uint16_t rating = document ? document->header.rating : 1U;
  const std::int32_t balance = document ? document->header.balance : 20000;
  const std::uint16_t frame_time =
      document ? document->header.frame_time : 0x09e5U;
  const std::int32_t current_day =
      document ? document->header.current_day : 0;
  const std::int32_t population =
      document ? document->post_elevator.finance.total_population : 0;

  OriginalInfoContent content{};
  content.rating = rating;
  // 1208:0004 is the executable's thin _wsprintf wrapper. Native translated
  // callers use bounded std::to_string/std::snprintf equivalents.
  content.balance = strl(resources, 15U) +
                    std::to_string(scaled_balance(balance));
  content.population = std::to_string(population);
  content.status.assign(status);

  const std::int32_t day_in_cycle = current_day % 3;
  if (day_in_cycle < 2) {
    content.date_dark = original_info_ordinal(resources, day_in_cycle + 1) +
                        strl(resources, 9U) + strl(resources, 8U);
  } else {
    content.date_red = strl(resources, 10U);
    content.date_dark = strl(resources, 8U);
  }
  content.date_dark += std::to_string((current_day / 3) % 4 + 1);
  content.date_dark += strl(resources, 4U);
  content.date_dark += strl(resources, 8U);
  content.date_dark += original_info_ordinal(resources, current_day / 12 + 1);
  content.date_dark += strl(resources, 1U);

  content.clock = original_info_clock_time(frame_time);
  content.minute_hand = original_info_clock_point(content.clock.minute, 15);
  const int hour_index = content.clock.hour * 5 + content.clock.minute / 12;
  content.hour_hand = original_info_clock_point(hour_index, 9);
  return content;
}

void draw_original_info(HDC destination,
                        const OriginalResources& resources,
                        const OriginalTdtDocument* document,
                        std::string_view status) {
  if (!destination) return;
  SavedDc saved(destination);
  draw_original_dib(destination, resources.find("BITMAP", 320), 0,
                    kOriginalInfoClientTop);
  const auto content =
      build_original_info_content(resources, document, status);
  draw_date(destination, content);
  draw_rating(destination, resources, content.rating);
  draw_balance(destination, content.balance);
  draw_status(destination, content.status);
  draw_population(destination, content.population);
  draw_clock(destination, resources, content);
}

}  // namespace simtower
