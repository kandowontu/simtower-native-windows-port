# Original resource-consumer map

This map is derived from the supplied NE executable's relocation-backed call
graph and per-function disassembly. It is the implementation boundary for the
native port; the rejected WebView prototype is not evidence for any entry.

| Resource / behavior | Original consumers | Native translation |
| --- | --- | --- |
| Win16 `DIALOG` templates | `1008:0085`, `1018:0000` and dialog filters | `original_dialog.cpp` parses all 48 ANSI templates and emits native Unicode `DLGTEMPLATE` data |
| `DTMP` dialog layouts | `1070:0005`, `1070:0231`, `1068:0567`, `1070:051f/0570/05a1/061d` | `original_dtmp.cpp` and `original_dtmp_runtime.cpp`; all 45 layouts and 332 RECTs tested |
| `ALRT` alert templates | `1208:0133/017a/0274/0369` | `original_alert.cpp`; parsing, `^0`-`^3` substitution, MessageBox flags, and result mapping tested |
| `STRL` Pascal strings | `1208:01a0` | `original_tables.cpp`; big-endian count and one-based entry walk tested |
| `PART/1000` | `1190:0005` | `original_tables.cpp`; exact 33 u16 + 4 u32 + 46 u16 mixed-width table tested |
| `YEN/1000..1002` | `1178:000c` and table readers | `original_tables.cpp`; all 135 u32 values tested |
| `TABL` / `TABM` | `1140:022c`, `1140:03f8` and build/rating callers | `original_tables.cpp`; count-prefixed tables, high-byte indirection, and rating height tested |
| Win3 `BITMAP` DIBs | `1030:0043`, `1208:049d/0529` | `original_dib.cpp`; all 242 8-bit DIBs parsed and native `SetDIBitsToDevice` path implemented |
| `MENU/TOWER_MENU` | main-window registration/startup | `original_ui.cpp`; constructed directly from original bytes, four roots and command IDs tested |
| `ACCELERATOR/TOWER_MENU` | main message loop | `original_ui.cpp`; constructed directly from original bytes, F1 command mapping tested |
| `GROUP_ICON` / `ICON` | `TOWER_APPICON` registration | `original_ui.cpp`; group selection and image creation tested |
| Main/info/map static surfaces | `1128:13fc`, `1120:0215`, `1160:0000`, `1168:02be` | `native_main.cpp`; original bitmap 320 and 352 blit positions translated |
| Main window classes / messages | `1258:0345`, `1128:05eb/08d6`, `1158:0597/05c3` | `native_main.cpp`; original class names, styles, logical sizes, message set, menu, accelerator and tick boundary translated |
| `CGPK` | `1038` Lobby tile cache, `10c0:0345`, `11f8:0680` | `original_world.cpp`; exact cell-major 8x36 Lobby and lobby-spanning Stair/Escalator banks translated and pixel-tested against CGPK/CLUT bytes |
| `WAVE` | `11c8:006b/0100/0135/0167/02c0/0390/0426/0597/08eb/0920/0978/09d2/0a31/0aab/0add` | `original_wave.cpp` and `original_audio.cpp`; exact RIFF boundary, 55 valid/3 malformed resources, two-channel priority arbitration, category gates, 600-coarse-tick (nominal 9.6-second) saturation rule, reserved channel, repeats, activation, and native `waveOut` playback translated and tested |

The embedded pack is generated from the exact 483 NE resource blocks and is
linked as `RCDATA` into the native executable. Runtime lookup does not open or
depend on the original executable.
