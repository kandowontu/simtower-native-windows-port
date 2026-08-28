#include "original_wave.hpp"

#include <limits>
#include <string_view>

namespace simtower {
namespace {

std::uint16_t little_u16(std::span<const std::byte> bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(
      std::to_integer<std::uint8_t>(bytes[offset]) |
      (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U));
}

std::uint32_t little_u32(std::span<const std::byte> bytes, std::size_t offset) {
  return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
         (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U) |
         (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U) |
         (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U);
}

bool tag_is(std::span<const std::byte> bytes,
            std::size_t offset,
            std::string_view expected) {
  if (offset > bytes.size() || expected.size() > bytes.size() - offset) {
    return false;
  }
  for (std::size_t index = 0; index < expected.size(); ++index) {
    if (std::to_integer<unsigned char>(bytes[offset + index]) !=
        static_cast<unsigned char>(expected[index])) {
      return false;
    }
  }
  return true;
}

}  // namespace

OriginalWaveView parse_original_wave(std::span<const std::byte> resource) noexcept {
  OriginalWaveView result{};
  if (resource.size() < 12U || !tag_is(resource, 0, "RIFF") ||
      !tag_is(resource, 8, "WAVE")) {
    return {};
  }

  const std::uint64_t declared_size =
      static_cast<std::uint64_t>(little_u32(resource, 4)) + 8U;
  if (declared_size < 12U || declared_size > resource.size() ||
      declared_size > std::numeric_limits<std::uint32_t>::max()) {
    return {};
  }
  const std::size_t logical_size = static_cast<std::size_t>(declared_size);

  bool found_format = false;
  bool found_data = false;
  std::size_t cursor = 12U;
  while (cursor + 8U <= logical_size) {
    const std::uint32_t chunk_size = little_u32(resource, cursor + 4U);
    const std::size_t payload = cursor + 8U;
    if (chunk_size > logical_size - payload) {
      return {};
    }
    const std::size_t payload_size = static_cast<std::size_t>(chunk_size);

    if (tag_is(resource, cursor, "fmt ")) {
      if (found_format || payload_size < 16U) {
        return {};
      }
      result.format_tag = little_u16(resource, payload);
      result.channels = little_u16(resource, payload + 2U);
      result.samples_per_second = little_u32(resource, payload + 4U);
      result.average_bytes_per_second = little_u32(resource, payload + 8U);
      result.block_align = little_u16(resource, payload + 12U);
      result.bits_per_sample = little_u16(resource, payload + 14U);
      if (payload_size > 16U) {
        result.format_extra = resource.subspan(payload + 16U, payload_size - 16U);
      }
      found_format = true;
    } else if (tag_is(resource, cursor, "data")) {
      if (found_data) {
        return {};
      }
      result.samples = resource.subspan(payload, payload_size);
      found_data = true;
    }

    const std::uint64_t next = static_cast<std::uint64_t>(payload) +
                               payload_size + (payload_size & 1U);
    if (next > logical_size + 1ULL) {
      return {};
    }
    cursor = static_cast<std::size_t>(next);
  }

  // Every valid sound in this executable is PCM. Keeping this check here
  // matches WaveMixOpenWave's failure boundary for the three malformed WAVE
  // resource entries instead of guessing at their contents.
  if (!found_format || !found_data || result.format_tag != 1U ||
      result.channels == 0U || result.samples_per_second == 0U ||
      result.average_bytes_per_second == 0U || result.block_align == 0U ||
      result.bits_per_sample == 0U || result.samples.empty()) {
    return {};
  }
  result.logical_size = static_cast<std::uint32_t>(logical_size);
  return result;
}

}  // namespace simtower
