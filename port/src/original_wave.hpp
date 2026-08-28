#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace simtower {

struct OriginalWaveView {
  std::uint32_t logical_size{};
  std::uint16_t format_tag{};
  std::uint16_t channels{};
  std::uint32_t samples_per_second{};
  std::uint32_t average_bytes_per_second{};
  std::uint16_t block_align{};
  std::uint16_t bits_per_sample{};
  std::span<const std::byte> format_extra{};
  std::span<const std::byte> samples{};

  friend bool operator==(const OriginalWaveView&, const OriginalWaveView&) = default;
};

// Returns an empty view for a malformed or unsupported resource. The NE
// resource allocation can be larger than the RIFF file; logical_size follows
// the RIFF size field and deliberately excludes that allocation padding.
[[nodiscard]] OriginalWaveView parse_original_wave(
    std::span<const std::byte> resource) noexcept;

}  // namespace simtower
