#include "original_tdt_file.hpp"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace simtower {

OriginalTdtDocument load_original_tdt_file(
    const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream.is_open()) {
    throw OriginalTdtFileError(OriginalTdtFileOperation::open,
                               "Could not open the SimTower data file");
  }

  const std::vector<char> characters(
      (std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  if (stream.bad()) {
    throw OriginalTdtFileError(OriginalTdtFileOperation::read,
                               "Could not read the SimTower data file");
  }

  std::vector<std::byte> bytes(characters.size());
  for (std::size_t index = 0; index < characters.size(); ++index) {
    bytes[index] = static_cast<std::byte>(
        static_cast<unsigned char>(characters[index]));
  }
  return parse_original_tdt(bytes);
}

void save_original_tdt_file(const std::filesystem::path& path,
                            const OriginalTdtDocument& document) {
  const auto bytes = serialize_original_tdt_game_save(document);
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream.is_open()) {
    throw OriginalTdtFileError(OriginalTdtFileOperation::create,
                               "Could not create the SimTower data file");
  }
  stream.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  if (!stream) {
    throw OriginalTdtFileError(OriginalTdtFileOperation::write,
                               "Could not write the SimTower data file");
  }
  stream.close();
  if (!stream) {
    throw OriginalTdtFileError(OriginalTdtFileOperation::write,
                               "Could not finish the SimTower data file");
  }
}

std::filesystem::path original_tdt_normalized_path(
    std::filesystem::path path) {
  // 10d0:04ab-04e8 uses the far-runtime strrchr over the complete selected
  // string, not a path-aware extension routine. A dot in a directory is
  // therefore still the replacement point when the file name has no dot.
  std::wstring text = path.wstring();
  const auto dot = text.find_last_of(L'.');
  if (dot == std::wstring::npos) {
    text.append(L".TDT");
  } else {
    text.replace(dot, std::wstring::npos, L".TDT");
  }
  return std::filesystem::path(std::move(text));
}

bool original_tdt_basename_is_valid(
    const std::filesystem::path& normalized_path) noexcept {
  // 10d0:0523-054f deliberately uses strchr('.') and strrchr('\\') on the
  // whole normalized path. The first dot, rather than the final extension,
  // ends the DOS basename measurement; a prior dotted directory consequently
  // produces the original signed-negative result and is accepted.
  const auto text = normalized_path.wstring();
  const auto dot = text.find(L'.');
  if (dot == std::wstring::npos) return false;
  const auto slash = text.find_last_of(L'\\');
  const std::ptrdiff_t basename_start =
      slash == std::wstring::npos
          ? 0
          : static_cast<std::ptrdiff_t>(slash) + 1;
  const auto length = static_cast<std::ptrdiff_t>(dot) - basename_start;
  return length <= 8;
}

std::wstring original_tower_window_title(std::wstring_view file_title) {
  if (file_title == L"untitled") {
    return L"SimTower - untitled";
  }
  std::wstring title = L"SimTower - ";
  title.append(file_title);
  if (title.size() >= 4U) {
    title.resize(title.size() - 4U);
  }
  return title;
}

}  // namespace simtower
