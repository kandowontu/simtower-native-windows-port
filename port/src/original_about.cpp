#include "original_about.hpp"

#include "original_dib.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace simtower {

OriginalAboutLayout derive_original_about_layout(
    const OriginalResources& resources) {
  const auto bitmap = original_dib_view(resources.find("BITMAP", 257));
  if (bitmap.width <= 0 || bitmap.height <= 0) {
    throw std::runtime_error("Invalid original About bitmap");
  }
  const int width = bitmap.width;
  const int height = bitmap.height;
  OriginalAboutLayout result{};
  result.window_width = width + 270;
  result.window_height = height + 20;
  result.title_bitmap = {
      10U, 10U,
      static_cast<std::uint16_t>(10 + width),
      static_cast<std::uint16_t>(10 + height)};
  result.credits_outer = {
      static_cast<std::uint16_t>(result.window_width - 250), 10U,
      static_cast<std::uint16_t>(result.window_width - 10),
      static_cast<std::uint16_t>(result.window_height - 10)};
  result.credits_inner = {
      static_cast<std::uint16_t>(result.credits_outer.left + 2U),
      static_cast<std::uint16_t>(result.credits_outer.top + 2U),
      static_cast<std::uint16_t>(result.credits_outer.right - 2U),
      static_cast<std::uint16_t>(result.credits_outer.bottom - 2U)};
  result.line_width = result.credits_inner.right - result.credits_inner.left;
  result.line_height = 16;
  result.timer_interval_ms = 55U;
  return result;
}

std::vector<std::string> original_about_credit_lines(
    const OriginalResources& resources) {
  const auto bytes = resources.find("TEXT", 128);
  std::vector<std::string> lines;
  std::size_t position{};
  while (position < bytes.size() && bytes[position] != std::byte{0}) {
    std::string line;
    while (position < bytes.size() && bytes[position] != std::byte{0} &&
           bytes[position] != std::byte{0x0d}) {
      line.push_back(static_cast<char>(
          std::to_integer<std::uint8_t>(bytes[position++])));
    }
    lines.push_back(std::move(line));
    if (position >= bytes.size() || bytes[position] == std::byte{0}) break;
    ++position;  // CR
    if (position < bytes.size() && bytes[position] == std::byte{0x0a}) {
      ++position;
    }
  }
  return lines;
}

std::vector<OriginalAboutScrollLine> original_about_visible_lines(
    const std::vector<std::string>& lines,
    std::uint64_t timer_ticks,
    int viewport_height) {
  std::vector<OriginalAboutScrollLine> visible;
  if (lines.empty() || timer_ticks == 0U || viewport_height <= 0) {
    return visible;
  }

  constexpr std::uint64_t line_height = 16U;
  const std::uint64_t first_sequence = timer_ticks >
          static_cast<std::uint64_t>(viewport_height)
      ? (timer_ticks - static_cast<std::uint64_t>(viewport_height)) /
            line_height
      : 0U;
  const std::uint64_t last_sequence = (timer_ticks - 1U) / line_height;
  visible.reserve(static_cast<std::size_t>(
      std::min<std::uint64_t>(last_sequence - first_sequence + 1U, 18U)));
  for (std::uint64_t sequence = first_sequence;
       sequence <= last_sequence; ++sequence) {
    const auto relative_ticks = timer_ticks - sequence * line_height;
    const int top = viewport_height - static_cast<int>(relative_ticks);
    if (top >= viewport_height || top + 16 <= 0) continue;
    visible.push_back({
        lines[static_cast<std::size_t>(sequence % lines.size())], top});
  }
  return visible;
}

}  // namespace simtower
