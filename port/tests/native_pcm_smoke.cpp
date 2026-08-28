#include "original_audio.hpp"
#include "original_resources.hpp"
#include "original_wave.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: simtower_native_pcm_smoke original_resources.pack\n";
    return 2;
  }

  std::ifstream stream(std::filesystem::path(argv[1]), std::ios::binary);
  if (!stream) {
    std::cerr << "could not open resource pack\n";
    return 3;
  }
  const std::vector<char> characters{
      std::istreambuf_iterator<char>(stream),
      std::istreambuf_iterator<char>()};
  std::vector<std::byte> bytes(characters.size());
  for (std::size_t index = 0; index < characters.size(); ++index) {
    bytes[index] = static_cast<std::byte>(
        static_cast<unsigned char>(characters[index]));
  }

  const simtower::OriginalResources resources(bytes);
  const auto wave = simtower::parse_original_wave(resources.find("WAVE", 20000));
  if (wave.logical_size == 0U || wave.samples.empty()) {
    std::cerr << "startup WAVE/20000 did not parse\n";
    return 4;
  }

  // Direct live 11c8:006b and 11c8:0920 boundary smoke. The production
  // initialize/open/prepare/write/stop/close path runs against WAVE_MAPPER;
  // only the payload bytes are replaced with digital silence of the same
  // format and length, so this cannot make audible noise.
  simtower::OriginalAudioRuntime audio(resources);
  audio.set_host_output_muted(true);
  if (!audio.initialize()) {
    std::cerr << "no native PCM output endpoint\n";
    return 5;
  }
  if (!audio.play_resource(20000, 0U, 1U, GetTickCount())) {
    std::cerr << "waveOut submission failed\n";
    audio.shutdown();
    return 6;
  }
  if (!audio.channel_active(0U)) {
    std::cerr << "submitted channel did not become active\n";
    audio.shutdown();
    return 7;
  }

  std::cout << "silent PCM submitted: " << wave.samples.size()
            << " bytes, " << wave.samples_per_second << " Hz, "
            << wave.bits_per_sample << " bit, channel 0 active\n";
  audio.stop_all(true);
  audio.shutdown();
  return 0;
}
