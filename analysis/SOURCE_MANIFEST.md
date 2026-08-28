# Supplied source manifest

Source location: local supplied game media (not published)

| Item | Size | SHA-256 |
| --- | ---: | --- |
| `simtower.vhd` | 33,554,432 bytes | `F5F4D870D6A7F0BA4AC6CC3DFDFBD0786FFF8759962F448192FB519452A12DF8` |
| extracted `MAXIS\SIMTOWER\SIMTOWER.EXE` | 6,213,632 bytes | `2BAA3F8517EA9DA8C8AB13BE9169C21DB5D66145DCDC83BBC50CF1ADDFD06992` |

The `.vhd` extension is misleading: the source is a raw MBR disk image with a bootable FAT16 type-04 partition beginning at LBA 63. `tools/extract_fat16.py` extracted 428 files in 10 directories without modifying the source image.
