#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace simtower {

inline constexpr std::size_t kOriginalFontCacheCapacity = 10U;
inline constexpr std::int16_t kOriginalMinimumFontPixelHeight = 9;

enum class OriginalFontCacheAction : std::uint8_t {
  no_selection,
  select_existing,
  create_and_select,
};

struct OriginalFontCacheDecision {
  OriginalFontCacheAction action{};
  std::size_t slot{};
  std::int16_t pixel_height{};

  friend bool operator==(const OriginalFontCacheDecision&,
                         const OriginalFontCacheDecision&) = default;
};

// Exact 1208:0ba7 decision boundary. A full ten-entry bank returns before
// clamping or searching (and therefore refuses even an already-cached height).
// Otherwise requests below nine alias the initial nine-pixel entry.
[[nodiscard]] constexpr OriginalFontCacheDecision
original_font_cache_decision(std::span<const std::int16_t> cached_heights,
                             std::int16_t requested_height) noexcept {
  if (cached_heights.size() >= kOriginalFontCacheCapacity) {
    return {OriginalFontCacheAction::no_selection, cached_heights.size(),
            requested_height};
  }
  const std::int16_t pixel_height =
      requested_height < kOriginalMinimumFontPixelHeight
          ? kOriginalMinimumFontPixelHeight
          : requested_height;
  for (std::size_t slot = 0U; slot < cached_heights.size(); ++slot) {
    if (cached_heights[slot] == pixel_height) {
      return {OriginalFontCacheAction::select_existing, slot, pixel_height};
    }
  }
  return {OriginalFontCacheAction::create_and_select,
          cached_heights.size(), pixel_height};
}

struct OriginalFontCreationSpec {
  std::int16_t pixel_height{};
  std::uint8_t character_set{};
  std::uint8_t output_precision{};

  friend bool operator==(const OriginalFontCreationSpec&,
                         const OriginalFontCreationSpec&) = default;
};

// 1208:0a8d creates the seeded nine-pixel font before 1208:0ba7 begins
// cloning new entries. Only cloned entries receive OUT_TT_ONLY_PRECIS (7).
[[nodiscard]] constexpr OriginalFontCreationSpec
original_font_creation_spec(std::int16_t pixel_height,
                            bool initial_entry) noexcept {
  return {pixel_height, 1U, static_cast<std::uint8_t>(initial_entry ? 0U : 7U)};
}

// Exact 1208:0b2b EnumFonts callback predicate. It stops only for a
// case-sensitive, complete "Arial" face-name match.
[[nodiscard]] constexpr bool original_font_face_is_arial(
    std::string_view face_name) noexcept {
  return face_name == "Arial";
}

// Process-global Win32 adapter for 1208:0a8d, 1208:0ba7, and 1208:0b6a.
// Returned handles are borrowed from the original ten-entry bank and remain
// valid until destroy_original_font_cache; callers must not delete them.
void initialize_original_font_cache() noexcept;
[[nodiscard]] HFONT original_cached_font(
    std::int16_t requested_height) noexcept;
void destroy_original_font_cache() noexcept;

}  // namespace simtower
