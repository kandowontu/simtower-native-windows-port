#include "original_tdt_file.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: simtower_native_save_fixture INPUT.TDT OUTPUT.TDT\n";
    return 2;
  }

  try {
    auto document = simtower::load_original_tdt_file(argv[1]);
    // A conspicuous but valid balance proves that the reference consumed the
    // native writer's current-revision stream rather than an untouched source
    // file copied into the validation image.
    constexpr std::int32_t kValidationBalance = 2345678;
    document.header.balance = kValidationBalance;
    simtower::save_original_tdt_file(argv[2], document);

    const auto reparsed = simtower::load_original_tdt_file(argv[2]);
    if (reparsed.header.balance != kValidationBalance) {
      throw std::runtime_error("native-written balance did not reparse");
    }
    std::cout << "wrote " << std::filesystem::file_size(argv[2])
              << " bytes with balance " << reparsed.header.balance << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
