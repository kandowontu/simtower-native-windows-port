#include "original_dialog.hpp"

#include <windows.h>

#include <bit>
#include <stdexcept>
#include <string>
#include <string_view>

namespace simtower {
namespace {

std::int16_t original_dialog_word(std::int32_t value) noexcept {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

std::int16_t original_dialog_add(std::int16_t left,
                                 std::int16_t right) noexcept {
  return original_dialog_word(static_cast<std::uint16_t>(left) +
                              static_cast<std::uint16_t>(right));
}

std::int16_t original_dialog_subtract(std::int16_t left,
                                      std::int16_t right) noexcept {
  return original_dialog_word(static_cast<std::uint16_t>(left) -
                              static_cast<std::uint16_t>(right));
}

class Reader {
 public:
  explicit Reader(std::span<const std::byte> data) : data_(data) {}

  std::uint8_t u8() {
    require(1);
    return static_cast<std::uint8_t>(data_[position_++]);
  }
  std::uint16_t u16() {
    require(2);
    const auto value = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data_[position_]) |
        (static_cast<std::uint16_t>(data_[position_ + 1]) << 8U));
    position_ += 2;
    return value;
  }
  std::int16_t i16() { return static_cast<std::int16_t>(u16()); }
  std::uint32_t u32() {
    const std::uint32_t low = u16();
    return low | (static_cast<std::uint32_t>(u16()) << 16U);
  }
  std::string cstring(std::uint8_t first = 0, bool has_first = false) {
    std::string value;
    if (has_first) {
      value.push_back(static_cast<char>(first));
    }
    while (true) {
      const auto character = u8();
      if (character == 0U) {
        return value;
      }
      value.push_back(static_cast<char>(character));
    }
  }
  void skip(std::size_t count) {
    require(count);
    position_ += count;
  }

 private:
  void require(std::size_t count) const {
    if (count > data_.size() - position_) {
      throw std::runtime_error("Truncated original Win16 dialog");
    }
  }
  std::span<const std::byte> data_;
  std::size_t position_ = 0;
};

OriginalDialogValue read_value(Reader& reader, bool predefined_class = false) {
  const std::uint8_t first = reader.u8();
  if (first == 0U) {
    return {};
  }
  if (first == 0xFFU) {
    return {OriginalDialogValue::Kind::ordinal, {}, reader.u16()};
  }
  if (predefined_class && first >= 0x80U) {
    return {OriginalDialogValue::Kind::ordinal, {}, first};
  }
  return {OriginalDialogValue::Kind::text,
          reader.cstring(first, true), 0};
}

std::wstring cp1252_to_wide(std::string_view text) {
  if (text.empty()) {
    return {};
  }
  const int count = MultiByteToWideChar(
      1252, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (count <= 0) {
    throw std::runtime_error("Could not decode original dialog CP1252 text");
  }
  std::wstring result(static_cast<std::size_t>(count), L'\0');
  MultiByteToWideChar(1252, 0, text.data(), static_cast<int>(text.size()),
                      result.data(), count);
  return result;
}

class TemplateWriter {
 public:
  void align4() {
    while (data_.size() % 4U) {
      data_.push_back(std::byte{0});
    }
  }
  void u16(std::uint16_t value) {
    data_.push_back(static_cast<std::byte>(value & 0xFFU));
    data_.push_back(static_cast<std::byte>(value >> 8U));
  }
  void i16(std::int16_t value) { u16(static_cast<std::uint16_t>(value)); }
  void u32(std::uint32_t value) {
    u16(static_cast<std::uint16_t>(value & 0xFFFFU));
    u16(static_cast<std::uint16_t>(value >> 16U));
  }
  void wide(std::string_view text) {
    for (const wchar_t character : cp1252_to_wide(text)) {
      u16(static_cast<std::uint16_t>(character));
    }
    u16(0);
  }
  void value(const OriginalDialogValue& value) {
    switch (value.kind) {
      case OriginalDialogValue::Kind::none:
        u16(0);
        break;
      case OriginalDialogValue::Kind::text:
        wide(value.text);
        break;
      case OriginalDialogValue::Kind::ordinal:
        u16(0xFFFFU);
        u16(value.ordinal);
        break;
    }
  }
  [[nodiscard]] std::vector<std::byte> finish() && { return std::move(data_); }

 private:
  std::vector<std::byte> data_;
};

}  // namespace

OriginalDialogScreenPosition original_dialog_center_position(
    OriginalDialogScreenRect desktop,
    OriginalDialogScreenRect window) noexcept {
  const auto desktop_width = original_dialog_subtract(
      original_dialog_word(desktop.right), original_dialog_word(desktop.left));
  const auto desktop_height = original_dialog_subtract(
      original_dialog_word(desktop.bottom), original_dialog_word(desktop.top));
  const auto window_width = original_dialog_subtract(
      original_dialog_word(window.right), original_dialog_word(window.left));
  const auto window_height = original_dialog_subtract(
      original_dialog_word(window.bottom), original_dialog_word(window.top));
  return {
      static_cast<std::int32_t>(
          original_dialog_subtract(desktop_width, window_width) / 2),
      static_cast<std::int32_t>(
          original_dialog_subtract(desktop_height, window_height) / 2),
  };
}

OriginalDialog parse_original_dialog(std::span<const std::byte> resource) {
  Reader reader(resource);
  OriginalDialog result;
  result.style = reader.u32();
  const std::uint8_t item_count = reader.u8();
  result.x = reader.i16();
  result.y = reader.i16();
  result.width = reader.i16();
  result.height = reader.i16();
  result.menu = read_value(reader);
  result.window_class = read_value(reader);
  result.caption = read_value(reader);
  if ((result.style & DS_SETFONT) != 0U) {
    result.font_point_size = reader.u16();
    result.font_face = reader.cstring();
  }

  result.items.reserve(item_count);
  for (std::uint8_t index = 0; index < item_count; ++index) {
    OriginalDialogItem item;
    item.x = reader.i16();
    item.y = reader.i16();
    item.width = reader.i16();
    item.height = reader.i16();
    item.id = reader.u16();
    item.style = reader.u32();
    item.window_class = read_value(reader, true);
    item.text = read_value(reader);
    reader.skip(reader.u8());
    result.items.push_back(std::move(item));
  }
  return result;
}

void apply_original_dialog_font_point_size(
    OriginalDialog& dialog,
    std::uint16_t point_size) noexcept {
  dialog.style |= DS_SETFONT;
  dialog.font_point_size = point_size;
}

std::string format_original_dialog_caret_arguments(std::string text,
                                                   std::int32_t argument) {
  if (text.find('^') == std::string::npos) return text;

  // 1068:04d4-053c walks the original text once. A caret consumes the next
  // byte even when it is not zero, and every ^0 pair expands independently.
  std::string result;
  result.reserve(text.size());
  for (std::size_t source = 0U; source < text.size();) {
    if (text[source] != '^') {
      result.push_back(text[source++]);
      continue;
    }
    ++source;
    if (source >= text.size()) break;
    if (text[source] == '0') result.append(std::to_string(argument));
    ++source;
  }
  return result;
}

std::string format_original_dialog_argument(std::string text,
                                            std::int32_t argument) {
  text = format_original_dialog_caret_arguments(std::move(text), argument);
  const std::size_t marker = text.find("#0");
  if (marker == std::string::npos) {
    return text;
  }

  // 1068:0175-0256 falls back to #0 after ^0 and calls 1000:39ea for a
  // wrapping 32-bit absolute value. INT32_MIN therefore remains negative.
  auto magnitude = std::bit_cast<std::uint32_t>(argument);
  if (argument < 0) {
    magnitude = 0U - magnitude;
  }
  text.replace(marker, 2U,
               std::to_string(std::bit_cast<std::int32_t>(magnitude)));
  return text;
}

OriginalEventDialogAction original_event_dialog_action(
    std::uint16_t command,
    std::int32_t argument,
    std::int32_t balance,
    bool timer_fired) noexcept {
  if (command == 1U) {
    return OriginalEventDialogAction::close_decline;
  }
  if (command != 2U) {
    return OriginalEventDialogAction::ignore;
  }

  // 1068:03a4-03b9 uses a 32-bit ADD followed by a signed comparison. Keep
  // its two's-complement wrap instead of invoking signed-overflow UB.
  const auto wrapped = std::bit_cast<std::int32_t>(
      std::bit_cast<std::uint32_t>(balance) +
      std::bit_cast<std::uint32_t>(argument));
  if (argument < 0 && wrapped < 0) {
    return timer_fired
        ? OriginalEventDialogAction::close_decline
        : OriginalEventDialogAction::
              warn_insufficient_funds_then_close_decline;
  }
  return OriginalEventDialogAction::close_accept;
}

OriginalDialogScreenPosition original_dialog_screen_position(
    OriginalDialogScreenRect desktop,
    OriginalDialogScreenRect window,
    std::int32_t requested_left) noexcept {
  // 11e0:0b52 first offsets the window rectangle to (0,0), so these are
  // signed Win16 outer-window dimensions. It then uses desktop.right and
  // desktop.bottom directly; Windows 3.1's desktop begins at the origin.
  const auto desktop_right = original_dialog_word(desktop.right);
  const auto desktop_bottom = original_dialog_word(desktop.bottom);
  const auto width = original_dialog_subtract(
      original_dialog_word(window.right), original_dialog_word(window.left));
  const auto height = original_dialog_subtract(
      original_dialog_word(window.bottom), original_dialog_word(window.top));

  std::int16_t left = original_dialog_word(requested_left);
  if (left == 0) {
    left = static_cast<std::int16_t>(
        original_dialog_subtract(desktop_right, width) / 2);
  }

  std::int16_t top = 80;
  if (original_dialog_add(height, 80) < desktop_bottom) {
    top = static_cast<std::int16_t>(
        original_dialog_subtract(desktop_bottom, height) / 2);
  }
  if (top < 43) {
    top = 43;
  }
  if (original_dialog_add(height, top) > desktop_bottom) {
    top = original_dialog_subtract(desktop_bottom, height);
  }
  return {left, top};
}

std::vector<std::byte> build_native_dialog_template(
    const OriginalDialog& dialog) {
  if (dialog.items.size() > 0xFFFFU) {
    throw std::runtime_error("Original dialog contains too many items");
  }

  TemplateWriter writer;
  writer.u32(dialog.style);
  writer.u32(0);  // no Win16 extended-style field exists
  writer.u16(static_cast<std::uint16_t>(dialog.items.size()));
  writer.i16(dialog.x);
  writer.i16(dialog.y);
  writer.i16(dialog.width);
  writer.i16(dialog.height);
  writer.value(dialog.menu);
  writer.value(dialog.window_class);
  writer.value(dialog.caption);
  if ((dialog.style & DS_SETFONT) != 0U) {
    writer.u16(dialog.font_point_size);
    writer.wide(dialog.font_face);
  }

  for (const auto& item : dialog.items) {
    writer.align4();
    writer.u32(item.style);
    writer.u32(0);
    writer.i16(item.x);
    writer.i16(item.y);
    writer.i16(item.width);
    writer.i16(item.height);
    writer.u16(item.id);
    writer.value(item.window_class);
    writer.value(item.text);
    writer.u16(0);  // every supplied Win16 item has zero creation bytes
  }
  return std::move(writer).finish();
}

std::optional<std::uint16_t> original_facility_information_dialog_id(
    std::int8_t type) noexcept {
  if (type < 0) return std::nullopt;

  std::uint16_t group = 8U;
  switch (type) {
    case 0:
    case 24:
    case 25:
    case 26:
    case 46:
      return std::nullopt;
    case 7:
      group = 0U;
      break;
    case 3:
      group = 1U;
      break;
    case 4:
      group = 2U;
      break;
    case 5:
      group = 3U;
      break;
    case 9:
      group = 4U;
      break;
    case 10:
      group = 5U;
      break;
    case 15:
      group = 6U;
      break;
    case 14:
      group = 7U;
      break;
    case 13:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
      group = 9U;
      break;
    case 18:
    case 19:
    case 34:
    case 35:
      group = 10U;
      break;
    case 29:
    case 30:
      group = 11U;
      break;
    case 6:
    case 12:
      group = 12U;
      break;
    default:
      break;
  }
  return static_cast<std::uint16_t>(0x02ecU + group);
}

std::uint16_t original_elevator_information_dialog_id(
    std::uint8_t type) noexcept {
  return type == 0U ? 0x02f9U : 0x02faU;
}

}  // namespace simtower
