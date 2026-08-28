#pragma once

#include <bit>
#include <cstdint>

namespace simtower {

inline constexpr std::uint32_t kOriginalWaveMixPumpPeriodMs = 0x30U;

// Fast Mode deliberately bypasses 1200:0196's ordinary six-coarse-tick
// scheduler gate.  In the Win16 executable its effective cadence is still
// bounded by the non-preemptive full-frame host pass.  The controlled
// reference capture (DOSBox-X at the repository's documented 12000
// cycles/ms) advanced approximately 777 frames in 45 seconds, yielding the
// 58-ms native host period below.  Keeping this adapter outside the recovered
// dispatcher preserves its exact Fast Mode branch while preventing a modern
// native busy loop from advancing several game days per second.
inline constexpr std::uint32_t kNativeFastModeFramePeriodMs = 58U;

[[nodiscard]] constexpr bool native_fast_mode_frame_due(
    std::uint32_t previous_frame_ms,
    std::uint32_t now_ms) noexcept {
  return static_cast<std::int32_t>(now_ms - previous_frame_ms) >=
         static_cast<std::int32_t>(kNativeFastModeFramePeriodMs);
}

struct OriginalWaveMixPumpPlan {
  bool due{};
  bool drain_callback_messages{};
  bool pump_backend{};
  std::uint32_t next_deadline{};

  friend bool operator==(const OriginalWaveMixPumpPlan&,
                         const OriginalWaveMixPumpPlan&) = default;
};

// Exact scheduling half of 11e0:0e84. DS:0258 is a rolling deadline rather
// than the last observed tick: one due call drains every message 0x03BD,
// optionally pumps WAVMIX when any of DS:de2a/de2c/de2e is nonzero, and then
// advances the deadline by exactly 48 ms. The unsigned JA comparison and
// wrapping addition are deliberate.
[[nodiscard]] constexpr OriginalWaveMixPumpPlan original_wavemix_pump_plan(
    std::uint32_t deadline,
    std::uint32_t now,
    bool elevator_sounds_enabled,
    bool event_sounds_enabled,
    bool background_sounds_enabled) noexcept {
  const std::uint32_t next = deadline + kOriginalWaveMixPumpPeriodMs;
  if (next > now) {
    return {.next_deadline = deadline};
  }
  return {
      .due = true,
      .drain_callback_messages = true,
      .pump_backend = elevator_sounds_enabled || event_sounds_enabled ||
                      background_sounds_enabled,
      .next_deadline = next,
  };
}

// 1208:05e6 first runs 11e0:0e84's throttled WAVMIX-message pump, obtains
// Win16 GetTickCount, and passes the signed dword through 1000:39b5 with
// CL=4. The native waveOut backend has no WAVMIX window message to dispatch,
// but every caller must still consume this exact 1/16-ms coarse clock.
[[nodiscard]] constexpr std::uint32_t original_coarse_tick(
    std::uint32_t milliseconds) noexcept {
  std::uint32_t shifted = milliseconds >> 4U;
  if ((milliseconds & 0x80000000U) != 0U) {
    shifted |= 0xf0000000U;
  }
  return shifted;
}

// 1000:39ea returns the unsigned magnitude of a wrapping signed dword. The
// original uses it for every elapsed-time comparison fed by 1208:05e6.
[[nodiscard]] constexpr std::uint32_t original_tick_magnitude_delta(
    std::uint32_t now_tick,
    std::uint32_t started_tick) noexcept {
  const std::uint32_t wrapped = now_tick - started_tick;
  return std::bit_cast<std::int32_t>(wrapped) < 0
             ? std::uint32_t{0} - wrapped
             : wrapped;
}

}  // namespace simtower
