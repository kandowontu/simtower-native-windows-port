#include "original_dtmp.hpp"

#include <limits>
#include <stdexcept>

namespace simtower {
namespace {

std::uint16_t little_u16(std::span<const std::byte> data, std::size_t offset) {
  if (offset > data.size() || data.size() - offset < 2U) {
    throw std::runtime_error("Truncated original DTMP word");
  }
  return static_cast<std::uint16_t>(
      static_cast<std::uint16_t>(data[offset]) |
      (static_cast<std::uint16_t>(data[offset + 1]) << 8U));
}

int decimal_resource_id(const std::string& text) {
  if (text.empty()) {
    return -1;
  }
  int value = 0;
  for (const char character : text) {
    if (character < '0' || character > '9' ||
        value > (std::numeric_limits<int>::max() - (character - '0')) / 10) {
      throw std::runtime_error("Invalid original DTMP bitmap reference");
    }
    value = value * 10 + (character - '0');
  }
  return value;
}

}  // namespace

OriginalDtmp parse_original_dtmp(std::span<const std::byte> resource) {
  OriginalDtmp result;
  std::size_t cursor = 0;
  while (cursor < resource.size() && resource[cursor] != std::byte{0}) {
    result.bitmap_reference.push_back(static_cast<char>(resource[cursor++]));
  }
  if (cursor >= resource.size()) {
    throw std::runtime_error("Unterminated original DTMP bitmap reference");
  }
  ++cursor;
  result.bitmap_resource_id = decimal_resource_id(result.bitmap_reference);
  result.width_or_header = little_u16(resource, cursor);
  result.height_or_header = little_u16(resource, cursor + 2U);
  const std::size_t count = little_u16(resource, cursor + 4U);
  cursor += 6U;
  if (count > (resource.size() - cursor) / 8U) {
    throw std::runtime_error("Truncated original DTMP rectangle array");
  }
  result.rectangles.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    result.rectangles.push_back({
        little_u16(resource, cursor),
        little_u16(resource, cursor + 2U),
        little_u16(resource, cursor + 4U),
        little_u16(resource, cursor + 6U),
    });
    cursor += 8U;
  }
  return result;
}

}  // namespace simtower
