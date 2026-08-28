#include "original_tables.hpp"

#include "original_resources.hpp"

#include <stdexcept>

namespace simtower {

OriginalPartTable original_part_table(std::span<const std::byte> resource) {
  if (resource.size() < OriginalPartTable::kLogicalSize) {
    throw std::runtime_error("Truncated PART/1000 resource");
  }

  OriginalPartTable result;
  for (std::size_t index = 0; index < result.words_00_to_40.size(); ++index) {
    result.words_00_to_40[index] = original_be16(resource, index * 2U);
  }
  for (std::size_t index = 0; index < result.dwords_42_to_4e.size(); ++index) {
    result.dwords_42_to_4e[index] = original_be32(resource, 0x42U + index * 4U);
  }
  for (std::size_t index = 0; index < result.words_52_to_ac.size(); ++index) {
    result.words_52_to_ac[index] = original_be16(resource, 0x52U + index * 2U);
  }
  return result;
}

OriginalYenTable original_yen_table(std::span<const std::byte> resource) {
  constexpr std::size_t kLogicalSize = OriginalYenTable{}.size() * 4U;
  if (resource.size() < kLogicalSize) {
    throw std::runtime_error("Truncated YEN resource");
  }

  OriginalYenTable result;
  for (std::size_t index = 0; index < result.size(); ++index) {
    result[index] = original_be32(resource, index * 4U);
  }
  return result;
}

std::vector<std::uint16_t> original_word_table(
    std::span<const std::byte> resource) {
  if (resource.size() < 2U) {
    throw std::runtime_error("Truncated original word table header");
  }
  const std::size_t count = original_be16(resource, 0);
  if (count > (resource.size() - 2U) / 2U) {
    throw std::runtime_error("Truncated original word table entries");
  }

  std::vector<std::uint16_t> entries;
  entries.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    entries.push_back(original_be16(resource, 2U + index * 2U));
  }
  return entries;
}

std::uint16_t original_resolve_tabl_entry(
    std::span<const std::byte> tabl_resource,
    std::size_t index,
    const OriginalResources& resources) {
  const auto entries = original_word_table(tabl_resource);
  if (index >= entries.size()) {
    throw std::out_of_range("TABL entry index is out of range");
  }

  const std::uint16_t encoded = entries[index];
  const std::uint16_t tabm_number = encoded >> 8U;
  if (tabm_number == 0U) {
    return encoded;
  }

  const auto tabm = original_word_table(
      resources.find("TABM", static_cast<int>(1000U + tabm_number)));
  const std::size_t tabm_index = encoded & 0xFFU;
  // 1140:022c indexes the locked resource base directly. Word zero is the
  // count, so the low byte is one-based with respect to the vector returned
  // by original_word_table().
  if (tabm_index == 0U || tabm_index > tabm.size()) {
    throw std::out_of_range("TABM entry index is out of range");
  }
  return tabm[tabm_index - 1U];
}

std::string original_strl_entry(std::span<const std::byte> resource,
                                std::uint16_t one_based_index) {
  if (one_based_index == 0 || resource.size() < 2) {
    return {};
  }
  const std::uint16_t count = original_be16(resource, 0);
  if (one_based_index > count) {
    return {};
  }

  std::size_t cursor = 2;
  for (std::uint16_t index = 1; index < one_based_index; ++index) {
    if (cursor >= resource.size()) {
      throw std::runtime_error("Truncated STRL resource");
    }
    const auto length = static_cast<std::uint8_t>(resource[cursor]);
    cursor += 1U + length;
  }
  if (cursor >= resource.size()) {
    throw std::runtime_error("Truncated STRL resource");
  }
  const auto length = static_cast<std::uint8_t>(resource[cursor++]);
  if (length > resource.size() - cursor) {
    throw std::runtime_error("Truncated STRL entry");
  }
  return std::string(
      reinterpret_cast<const char*>(resource.data() + cursor),
      reinterpret_cast<const char*>(resource.data() + cursor + length));
}

}  // namespace simtower
