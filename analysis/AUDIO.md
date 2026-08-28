# Original audio subsystem

This is a static, instruction-backed translation of selector `11c8`. It does
not rely on listening to, viewing, or behaviorally approximating the original.

## Resource corpus

The NE table contains 58 `WAVE` allocations. Fifty-five are valid mono,
8-bit PCM RIFF/WAVE files (1,638,925 sample bytes). IDs 8000, 9004, and 9007
do not contain RIFF headers in the supplied executable. The native parser
rejects those exact payloads at the same `WaveMixOpenWave` boundary instead of
inventing replacement data. Resource-allocation padding after each RIFF file
is excluded using the RIFF size field.

## `11c8` translation map

| Original | Recovered behavior | Native translation |
| --- | --- | --- |
| `006b` | clear two channels; initialize and activate WAVMIX | `OriginalAudioRuntime::initialize` |
| `0100` | play priority 5 on reserved channel 1 only when idle | `play_reserved_if_idle` |
| `0135` | stop both channels | `stop_all` |
| `0167` | category gates, channel selection, 600-coarse-tick (nominal 9.6 s) saturation flush, resource open, repeat/non-repeat play | `play_resource` |
| `02c0`, `0390` | gated channel stop and active query | `stop_channel`, `channel_active` |
| `03ab` | ambient gate: world mode bits 0/3 clear and PRNG magnitude divisible by 16 | `original_should_attempt_ambient_sound` |
| `0426` | event-to-resource mapping and contextual background selection | `original_wave_resource_for_sound_event`, `original_contextual_wave_resource` |
| `0597` | exact two-channel priority arbiter; priority 5 reserves channel 1 | `OriginalAudioArbiter::select_channel` |
| `05e8`, `0671` | choose one of six quarter/half/three-quarter tower probes | `original_ambient_probe` |
| `06b6` | classify an 18-byte facility record and linked state as a sound event | `original_sound_event_for_facility` |
| `08eb`, `0920`, `0978`, `09d2` | channel open/play/repeat/close | native `waveOut` backend |
| `0a31`, `0aab`, `0add` | shutdown, activate, deactivate | corresponding runtime methods |

`native_main.cpp` now preserves the startup device probe, WAVMIX initialization
fallback, post-mixer profile load, WAVE/20000 startup playback point, shutdown
path, and the three sound menu commands. The original binary associates command 40012 (labelled
Background in the MENU resource) with its Events word and command 40013
(labelled Events) with its Background word; the native command handler retains
that observed mismatch.

The original configuration sequence recovered at `1128:03ad-0535` probes
`%WINDIR%\SIMTOWER.INI`, then the installed program directory's
`simtower.ini`. If neither file exists, it clears all five sound words. For a
found file it reads section `Sound`, key `BeepOnly` with default zero. The
literal value one clears the master and all categories; any other value allows
`AllSounds`, `Elevator`, `Events`, and `Background` to be read with defaults of
one, provided WAVMIX initialized. The native pure transition is
`original_sound_profile_state`; its host reads the same optional override paths
and embeds the supplied installation's all-enabled INI values as the
self-contained release fallback. `1128:0b0d-0b95` check/gray menu state is also
applied from the resulting words.
