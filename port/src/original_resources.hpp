#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace simtower {

class OriginalResources {
 public:
  OriginalResources() = default;
  explicit OriginalResources(std::span<const std::byte> pack) : pack_(pack) {}

  static OriginalResources from_current_module();

  // Native packed-resource equivalent of the FindResource/LoadResource
  // wrapper at 1208:045c. Returned spans remain owned by the embedded pack.
  [[nodiscard]] std::span<const std::byte> find(std::string_view type,
                                                std::int32_t numeric_id) const;
  [[nodiscard]] std::span<const std::byte> find(std::string_view type,
                                                 std::string_view string_id) const;
  [[nodiscard]] std::span<const std::byte> pack() const { return pack_; }

 private:
  std::span<const std::byte> pack_{};
};

}  // namespace simtower
