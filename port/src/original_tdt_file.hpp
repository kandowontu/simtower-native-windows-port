#pragma once

#include "original_tdt.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace simtower {

struct OriginalTdtFileDialogProfile {
  std::uint32_t filter_index{};
  std::size_t maximum_file_characters{};
  std::size_t maximum_file_title_characters{};
  std::uint32_t flags{};

  friend bool operator==(const OriginalTdtFileDialogProfile&,
                         const OriginalTdtFileDialogProfile&) = default;
};

// Exact shared Win16 OPENFILENAME fields built by Open at 10d0:0122 and Save
// As at 10d0:03f1. The one belongs to nFilterIndex; the flags dword at DS:31fe
// is zero in both paths, so OFN_READONLY must not be synthesized.
[[nodiscard]] constexpr OriginalTdtFileDialogProfile
original_tdt_file_dialog_profile() noexcept {
  return {1U, 128U, 15U, 0U};
}

// Exact overwrite-confirmation word passed by the two save paths. Save As
// requests the 10d0:03f1 replacement prompt; normal Save clears it at
// 10d0:0305 because the current document path was already accepted.
[[nodiscard]] constexpr bool original_tdt_save_overwrite_prompt(
    bool save_as) noexcept {
  return save_as;
}

enum class OriginalTdtFileOperation {
  open,
  read,
  create,
  write,
};

// Exact 1000:2140 ownership gate used by 10d0:0777's failed-save cleanup.
// An initial create failure owns no target; every failure after create has a
// target that the original removes before reporting the error.
[[nodiscard]] constexpr bool original_failed_save_deletes_target(
    OriginalTdtFileOperation operation) noexcept {
  return operation != OriginalTdtFileOperation::create;
}

class OriginalTdtFileError : public std::runtime_error {
 public:
  OriginalTdtFileError(OriginalTdtFileOperation operation,
                       const char* message)
      : std::runtime_error(message), operation_(operation) {}

  [[nodiscard]] OriginalTdtFileOperation operation() const noexcept {
    return operation_;
  }

 private:
  OriginalTdtFileOperation operation_;
};

// File boundary around 10d0:0b3a and its exact 10d0:2a13 read/write transfer
// primitive. Parser status errors remain OriginalTdtError so the caller can
// reproduce the original 2/4/5 alert mapping independently from disk I/O
// errors.
[[nodiscard]] OriginalTdtDocument load_original_tdt_file(
    const std::filesystem::path& path);

void save_original_tdt_file(const std::filesystem::path& path,
                            const OriginalTdtDocument& document);

// 10d0:03f1 replaces the suffix beginning at the last dot anywhere in the
// complete selected path, or appends exactly ".TDT" when no dot exists.
[[nodiscard]] std::filesystem::path original_tdt_normalized_path(
    std::filesystem::path path);

// The Win16 save-as path measures from the last backslash to the first dot in
// the complete normalized path and rejects signed lengths greater than eight.
[[nodiscard]] bool original_tdt_basename_is_valid(
    const std::filesystem::path& normalized_path) noexcept;

// 10d0:0225/03f1 constructs "SimTower - <file title>" and then removes the
// four-character .TDT suffix. New towers use the literal "untitled".
[[nodiscard]] std::wstring original_tower_window_title(
    std::wstring_view file_title);

}  // namespace simtower
