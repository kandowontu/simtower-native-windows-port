#include "original_resources.hpp"

#include "original_resources.generated.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace simtower {
namespace {

std::string uppercase(std::string_view value) {
  std::string result(value);
  std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
    return static_cast<char>(std::toupper(character));
  });
  return result;
}

std::span<const std::byte> view_descriptor(
    std::span<const std::byte> pack,
    const generated::ResourceDescriptor& descriptor) {
  const auto offset = static_cast<std::size_t>(descriptor.offset);
  const auto size = static_cast<std::size_t>(descriptor.size);
  if (offset > pack.size() || size > pack.size() - offset) {
    throw std::runtime_error("Original resource descriptor exceeds the embedded pack");
  }
  return pack.subspan(offset, size);
}

}  // namespace

OriginalResources OriginalResources::from_current_module() {
  // The self-contained pack replaces the per-resource GlobalAlloc/Lock and
  // unlock/free lifetimes at 1208:0cb5/0d75. LockResource on a PE resource
  // yields immutable module-owned storage whose lifetime is the process, so
  // find() can return stable spans without emulating Win16 lock counts.
  const HMODULE module = GetModuleHandleW(nullptr);
  const HRSRC resource = FindResourceW(
      module,
      MAKEINTRESOURCEW(generated::kResourcePackRCDATAId),
      RT_RCDATA);
  if (!resource) {
    throw std::runtime_error("Embedded original resource pack is missing");
  }
  const HGLOBAL loaded = LoadResource(module, resource);
  const auto* data = static_cast<const std::byte*>(LockResource(loaded));
  const DWORD size = SizeofResource(module, resource);
  if (!data || size == 0) {
    throw std::runtime_error("Embedded original resource pack could not be locked");
  }
  return OriginalResources({data, static_cast<std::size_t>(size)});
}

std::span<const std::byte> OriginalResources::find(
    std::string_view type,
    std::int32_t numeric_id) const {
  const std::string normalized_type = uppercase(type);
  for (const auto& descriptor : generated::kResources) {
    if (descriptor.type == normalized_type && descriptor.numeric_id == numeric_id) {
      return view_descriptor(pack_, descriptor);
    }
  }
  return {};
}

std::span<const std::byte> OriginalResources::find(
    std::string_view type,
    std::string_view string_id) const {
  const std::string normalized_type = uppercase(type);
  const std::string normalized_id = uppercase(string_id);
  for (const auto& descriptor : generated::kResources) {
    if (descriptor.type == normalized_type && uppercase(descriptor.string_id) == normalized_id) {
      return view_descriptor(pack_, descriptor);
    }
  }
  return {};
}

}  // namespace simtower
