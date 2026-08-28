#include "original_font.hpp"

#include <array>
#include <string_view>

namespace simtower {
namespace {

struct OriginalFontCache {
  bool initialized{};
  std::array<char, LF_FACESIZE> face_name{};
  std::array<std::int16_t, kOriginalFontCacheCapacity> heights{};
  std::array<HFONT, kOriginalFontCacheCapacity> handles{};
  std::size_t count{};
};

OriginalFontCache g_original_font_cache;

int CALLBACK find_arial_font(const LOGFONTA* font,
                             const TEXTMETRICA*,
                             DWORD,
                             LPARAM parameter) {
  auto* found = reinterpret_cast<bool*>(parameter);
  if (original_font_face_is_arial(font->lfFaceName)) {
    *found = true;
    return 0;
  }
  return 1;
}

void copy_face_name(std::string_view source) noexcept {
  auto& destination = g_original_font_cache.face_name;
  destination.fill('\0');
  const std::size_t count =
      source.size() < destination.size() - 1U
          ? source.size()
          : destination.size() - 1U;
  for (std::size_t index = 0U; index < count; ++index) {
    destination[index] = source[index];
  }
}

HFONT create_original_font(const OriginalFontCreationSpec& spec) noexcept {
  // The Win16 LOGFONT banks at DS:2608 are zero-initialized. The recovered
  // routines write only height, charset, output precision, and face name.
  return CreateFontA(-static_cast<int>(spec.pixel_height), 0, 0, 0,
                     FW_DONTCARE, FALSE, FALSE, FALSE,
                     spec.character_set, spec.output_precision,
                     CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH,
                     g_original_font_cache.face_name.data());
}

}  // namespace

void initialize_original_font_cache() noexcept {
  if (g_original_font_cache.initialized) return;

  // 1208:0a8d enumerates every screen font with a null face filter. Its
  // 1208:0b2b callback stops at an exact "Arial" match and otherwise leaves
  // the fallback "MS Sans Serif" selected.
  bool arial_available = false;
  HDC screen = GetDC(nullptr);
  if (screen) {
    EnumFontsA(screen, nullptr, find_arial_font,
               reinterpret_cast<LPARAM>(&arial_available));
    ReleaseDC(nullptr, screen);
  }
  copy_face_name(arial_available ? "Arial" : "MS Sans Serif");

  g_original_font_cache.heights[0] = kOriginalMinimumFontPixelHeight;
  g_original_font_cache.handles[0] = create_original_font(
      original_font_creation_spec(kOriginalMinimumFontPixelHeight, true));
  // The original publishes count one before CreateFontIndirect and retains
  // that entry even if GDI returns a null handle.
  g_original_font_cache.count = 1U;
  g_original_font_cache.initialized = true;
}

HFONT original_cached_font(std::int16_t requested_height) noexcept {
  if (!g_original_font_cache.initialized) initialize_original_font_cache();

  const auto decision = original_font_cache_decision(
      std::span<const std::int16_t>(g_original_font_cache.heights.data(),
                                    g_original_font_cache.count),
      requested_height);
  switch (decision.action) {
    case OriginalFontCacheAction::no_selection:
      return nullptr;
    case OriginalFontCacheAction::select_existing:
      return g_original_font_cache.handles[decision.slot];
    case OriginalFontCacheAction::create_and_select:
      break;
  }

  HFONT font = create_original_font(
      original_font_creation_spec(decision.pixel_height, false));
  g_original_font_cache.heights[decision.slot] = decision.pixel_height;
  g_original_font_cache.handles[decision.slot] = font;
  // 1208:0c65 increments the bank count only after successful creation.
  if (font) ++g_original_font_cache.count;
  return font;
}

void destroy_original_font_cache() noexcept {
  if (!g_original_font_cache.initialized) return;
  // 1208:0b6a deletes precisely the published handle count in slot order.
  for (std::size_t slot = 0U; slot < g_original_font_cache.count; ++slot) {
    if (g_original_font_cache.handles[slot]) {
      DeleteObject(g_original_font_cache.handles[slot]);
    }
  }
  g_original_font_cache = {};
}

}  // namespace simtower
