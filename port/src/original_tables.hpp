#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace simtower {

class OriginalResources;

// Exact translation of original routine 1208:0603.
[[nodiscard]] constexpr std::uint16_t original_be16(std::span<const std::byte> data,
                                                     std::size_t offset) {
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(data[offset]) << 8U) |
      static_cast<std::uint16_t>(data[offset + 1]));
}

// Exact translation of original routine 1208:063a.
[[nodiscard]] constexpr std::uint32_t original_be32(std::span<const std::byte> data,
                                                     std::size_t offset) {
  return (static_cast<std::uint32_t>(data[offset]) << 24U) |
         (static_cast<std::uint32_t>(data[offset + 1]) << 16U) |
         (static_cast<std::uint32_t>(data[offset + 2]) << 8U) |
         static_cast<std::uint32_t>(data[offset + 3]);
}

// Original function 1190:0005 copies PART/1000 into globals with this exact
// shape. Keep the entries indexed until their consumers prove semantics.
struct OriginalPartTable {
  static constexpr std::size_t kLogicalSize = 0xAE;
  std::array<std::uint16_t, 33> words_00_to_40{};
  std::array<std::uint32_t, 4> dwords_42_to_4e{};
  std::array<std::uint16_t, 46> words_52_to_ac{};
};

// All observed YEN consumers index fixed arrays of 45 big-endian dwords.
using OriginalYenTable = std::array<std::uint32_t, 45>;

[[nodiscard]] OriginalPartTable original_part_table(
    std::span<const std::byte> resource);
[[nodiscard]] OriginalYenTable original_yen_table(
    std::span<const std::byte> resource);

// Count-prefixed big-endian word arrays used by rating TABL resources
// (1140:01df/022c) and TABM resources (1140:0470).
[[nodiscard]] std::vector<std::uint16_t> original_word_table(
    std::span<const std::byte> resource);

// Exact indirection performed by 1140:022c. A TABL word whose high byte is
// zero is returned directly. Otherwise the high byte selects TABM/(1000+N)
// and the low byte is the original one-based TABM word index (word zero is
// the table count).
[[nodiscard]] std::uint16_t original_resolve_tabl_entry(
    std::span<const std::byte> tabl_resource,
    std::size_t index,
    const OriginalResources& resources);

// Original 1140:03f8 window-height expression.
[[nodiscard]] constexpr std::uint16_t original_rating_window_height(
    std::uint16_t entry_count) {
  return static_cast<std::uint16_t>(((entry_count + 1U) / 2U) * 32U + 61U);
}

// Exact STRL entry walk performed by original routine 1208:01a0. Entry
// indices are one-based in the original callers.
[[nodiscard]] std::string original_strl_entry(std::span<const std::byte> resource,
                                               std::uint16_t one_based_index);

}  // namespace simtower
