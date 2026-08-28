# Headless boundary audit

This ledger classifies the exact native mappings that do not have a direct
headless test citation. It is generated from the conservative recovered-routine
coverage index after the 2026-08-27 static host-order pass.

The two entries below are not unported game-owned routines. One is the mapped
GUI process adapter whose outer operation cannot be represented by a direct
headless citation; the other is an unreachable diagnostic formatter. The
explicitly permitted 2026-08-27 runtime pass exercised native WinMain through
startup, New/Open/Cancel, quoted-path save loading, and shutdown. The final
90-second native loaded-state check ran on a separate, non-interactive Windows
desktop. Its start and end captures retained `1st WD/1Q/1st Year` after the
native Fast Mode host cadence was corrected to the reference-observed 58 ms.
The production `waveOut` open/prepare/write/stop/close path was also exercised
against WAVE_MAPPER with same-format digital silence, so no audible noise was
produced.

| Boundary class | Recovered starts | Why it remains outside direct headless execution |
| --- | --- | --- |
| GUI process entry | `1000:0000` | Enters the Win16 runtime and application handoff. Native `WinMain` was runtime-smoke-tested through New, Open, Cancel, reciprocal save loading, the isolated 90-second timing check, and clean exit, but remains ineligible for a direct headless citation. |
| No-inbound diagnostic artifact | `1208:0dad` | Unreachable `wvsprintf`/`OutputDebugString` formatter with no inbound call or relocation; it contributes no recovered game behavior. |

Current measurable boundary: 911/911 recovered game-owned starts have exact
native mappings, 909/911 mapped starts have direct test citations, and these
two classified outer boundaries do not. The separate 264-candidate support set is
confined to compiler/runtime segments and remains documented in
`NATIVE_RUNTIME_CLASSIFICATION.md`.

This classification is not by itself fidelity proof. Muted side-by-side startup,
New Tower, caption, file dialog, reciprocal save acceptance, loaded-state soak,
and silent hardware PCM validation are complete. Exact embedded WAVE bytes and
the unmuted default branch retain the original samples; only the explicit
hardware-smoke payload was replaced by same-format digital silence.
