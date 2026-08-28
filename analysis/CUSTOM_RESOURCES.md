# Custom resource audit

`tools/decode_custom_resources.py` decodes every custom resource extracted from the supplied `SIMTOWER.EXE`. It writes the complete machine-readable result to `original/assets_converted/custom/manifest.json` and regenerates `web/src/sim/original-tuning.generated.ts` from the source bytes.

| Type | Count | Decoded structure | Runtime use |
| --- | ---: | --- | --- |
| ALRT | 6 | Little-endian MessageBox button mode, one preserved word, then a NUL-terminated CP1252 template using `^0`-`^3` substitutions | Native parser, formatter, MessageBox flags, and result mapping translated from `1208:0133/017a/0274/0369` |
| DTMP | 45 | Optional NUL-terminated decimal bitmap ID, two little-endian words, a little-endian rectangle count, then little-endian RECTs | Native sizing, bitmap painting, child positioning, and dialog chrome translated from `1070:0005/0231` and `1068:0567` |
| PART | 1 | 33 big-endian u16 values, four big-endian u32 values, then 46 big-endian u16 values (174 logical bytes) | Routing delays, operational/capacity thresholds, rating thresholds, fire/rescue values, parking rates, and bomb/rescue costs are live |
| TABL | 7 | Facility catalog/order table and six count-prefixed rating layouts | Original build-palette ordering/rating layouts audited |
| TABM | 22 | Count-prefixed big-endian u16 entry lists | Original build-menu group membership audited |
| TEXT | 1 | NUL-terminated CP1252 about/version text | Version and credits preserved |
| YEN | 3 | 45-entry big-endian u32 tables | Construction costs, income payouts, and periodic expenses are live |

The PART consumer at NE segment 51 copies source offset `N` to static `DS:dd7a+N`; runtime captures expose the same globals at `DS:e5ee+N`. This establishes the mixed-width 174-byte layout without guessing from zero padding. The generated manifest records both addresses for every field and preserves signed and unsigned interpretations where labels remain unknown.

DTMP byte order is uniform after its leading C string. An empty prefix makes
the first two words the window's pixel width and height. A decimal prefix makes
it a BITMAP resource reference, and that bitmap supplies the window size and
background. Each following rectangle either positions child ID `index + 1` or,
when its bottom word is zero, places the bitmap whose ID is in the right word.
This replaces the rejected decoder's incorrect big-endian interpretation.

The YEN construction table plus the supplied help and string resources corrected five earlier placeholder costs: lobby `$5,000`, parking `$3,000`, express elevator `$400,000`, service elevator `$100,000`, and metro `$1,000,000`.

All original allocated blocks and their SHA-256 hashes remain in the manifest. A semantic name is assigned only when corroborated by a binary consumer or an independent original UI/help value; unresolved PART fields remain losslessly indexed by byte offset.
