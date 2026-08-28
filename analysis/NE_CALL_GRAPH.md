# Recursive NE call graph

Source SHA-256: `2baa3f8517ea9da8c8ab13be9169c21db5d66145dcdc83bbc50cf1addfd06992`

This inventory follows executable control flow from authoritative NE entry points and relocated far-call targets. It replaces the earlier linear-scan candidate count, which included call-like bytes from embedded data.

- Code segments: 78
- Code bytes: 309,486
- Reachable function entries: 1,175
- Named NE entries: 22
- Unique decoded instruction bytes: 297,229 (96.04%)
- Internal call sites: 4,948
- Resolved Win16 import call sites: 2,221
- Indirect call sites: 93
- Unresolved indirect jump sites: 3
- Recovered compiler jump tables: 170

## Exported/named entry points

| Address | Name | Instructions | Calls |
|---|---|---:|---:|
| `1000:0000` | `PROGRAM_ENTRY` | 81 | 12 |
| `1010:014c` | `SETUPSTARTUPDLGA` | 157 | 19 |
| `1010:0304` | `SETUPSTARTUPDLGB` | 141 | 16 |
| `1010:053f` | `ABOUTDLGPROC` | 359 | 62 |
| `1018:0067` | `NEWORLOADDLOGFILTER` | 139 | 28 |
| `1050:0000` | `CMDBTNWNDPROC` | 310 | 43 |
| `1050:05a7` | `CMDBTNSUBWNDPROC` | 320 | 59 |
| `1060:00d3` | `COUNTDLOGMAIN` | 336 | 47 |
| `1068:00a1` | `AHOTTADLOGFILTER` | 287 | 51 |
| `1098:0628` | `ELVDLOGMAIN` | 1082 | 152 |
| `1098:22f8` | `ELVPOPUP` | 403 | 44 |
| `10d8:006f` | `FINDDIALOGFILTER` | 260 | 51 |
| `1100:0116` | `PEPLEINFODLOGFILTER` | 212 | 39 |
| `1100:085b` | `TENANTINFODLOGFILTER` | 540 | 71 |
| `1100:0f10` | `ELVINFODLOGFILTER` | 231 | 41 |
| `1100:1248` | `ESCINFODLOGFILTER` | 234 | 42 |
| `1100:3a39` | `NAMEPEPLEDIALOGFILTER` | 250 | 49 |
| `1100:3dc4` | `NAMETENANTDIALOGFILTER` | 251 | 48 |
| `1100:4138` | `MOVIETITLEDIALOGFILTER` | 225 | 36 |
| `1120:0000` | `INFOWNDPROC` | 178 | 21 |
| `1158:0000` | `MAINWNDPROC` | 466 | 63 |
| `1168:0000` | `MAPWNDPROC` | 222 | 28 |

## Most-used resolved imports

| API | Call sites |
|---|---:|
| `USER!OFFSETRECT` | 160 |
| `USER!SETRECT` | 112 |
| `GDI!SELECTOBJECT` | 90 |
| `USER!REALIZEPALETTE` | 84 |
| `USER!SELECTPALETTE` | 83 |
| `KERNEL!GLOBALUNLOCK` | 60 |
| `USER!RELEASEDC` | 56 |
| `KERNEL!LSTRLEN` | 55 |
| `USER!GETDC` | 55 |
| `USER!INVALIDATERECT` | 54 |
| `KERNEL!GLOBALHANDLE` | 51 |
| `GDI!SETTEXTALIGN` | 44 |
| `KERNEL!LOCKRESOURCE` | 44 |
| `GDI!DELETEOBJECT` | 42 |
| `GDI!SETBKMODE` | 41 |
| `KERNEL!FREERESOURCE` | 40 |
| `GDI!TEXTOUT` | 33 |
| `GDI!MOVETO` | 32 |
| `KERNEL!GLOBALLOCK` | 32 |
| `USER!GETSYSTEMMETRICS` | 32 |
| `USER!PTINRECT` | 31 |
| `USER!GETCLIENTRECT` | 30 |
| `USER!SETWINDOWPOS` | 28 |
| `KERNEL!LSTRCAT` | 27 |
| `USER!SENDDLGITEMMESSAGE` | 27 |
| `GDI!GETSTOCKOBJECT` | 26 |
| `USER!UPDATEWINDOW` | 25 |
| `KERNEL!GLOBALFREE` | 24 |
| `KERNEL!GLOBALALLOC` | 23 |
| `USER!BEGINPAINT` | 21 |
| `USER!ENABLEWINDOW` | 21 |
| `USER!ENDDIALOG` | 21 |
| `USER!ENDPAINT` | 21 |
| `GDI!CREATESOLIDBRUSH` | 19 |
| `GDI!LINETO` | 19 |
| `KERNEL!LSTRCPY` | 18 |
| `KERNEL!MAKEPROCINSTANCE` | 18 |
| `USER!GETWINDOWRECT` | 18 |
| `KERNEL!FREEPROCINSTANCE` | 17 |
| `USER!GETDLGITEM` | 17 |
| `USER!GETSUBMENU` | 17 |
| `USER!MESSAGEBOX` | 16 |
| `USER!SETCLASSWORD` | 16 |
| `WING!WINGBITBLT` | 16 |
| `GDI!GETNEARESTPALETTEINDEX` | 14 |
| `USER!RELEASECAPTURE` | 13 |
| `USER!SHOWWINDOW` | 13 |
| `USER!GETSCROLLPOS` | 12 |
| `USER!FILLRECT` | 11 |
| `USER!GETMENU` | 11 |
