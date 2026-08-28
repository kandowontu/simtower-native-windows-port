# Native port recovered-routine coverage audit

> Address annotations are mapping evidence, not behavioral-equivalence proof. This audit excludes `web/` and `references/`.

- Recovered function candidates: 1,175
- Unique addresses cited in native source: 1,484
- Exact recovered function starts cited in native source: 911
- Additional candidates inferred from an in-body native citation: 0
- Candidates with no native-source citation: 264
- Source-mapped candidates with no test citation: 2

## Candidates without native-source citations by segment

| Segment | Candidates |
| --- | ---: |
| `1000` | 187 |
| `1260` | 58 |
| `1268` | 19 |

## Highest-priority candidates with no native-source mapping

| Address | Name | Bytes | Inbound callers | Reachable-root heuristic | Inventory claim |
| --- | --- | ---: | ---: | :---: | :---: |
| `1000:3994` |  | 30 | 134 | yes | no |
| `1000:45ab` |  | 28 | 48 | yes | no |
| `1260:3622` |  | 19 | 30 | yes | no |
| `1260:3649` |  | 113 | 23 | yes | no |
| `1000:1d8a` |  | 251 | 20 | yes | no |
| `1000:2176` |  | 1095 | 2 | yes | no |
| `1260:1497` |  | 1136 | 1 | yes | no |
| `1260:0ed6` |  | 919 | 2 | yes | no |
| `1260:36ba` |  | 22 | 20 | yes | no |
| `1260:08bb` |  | 751 | 2 | yes | no |
| `1260:2b3b` |  | 1032 | 1 | no | no |
| `1000:3bfe` |  | 25 | 14 | yes | no |
| `1260:21cd` |  | 948 | 0 | no | no |
| `1260:26c6` |  | 702 | 4 | no | no |
| `1000:20a6` |  | 62 | 12 | yes | no |
| `1000:042e` |  | 795 | 0 | no | no |
| `1260:0c0a` |  | 381 | 3 | yes | no |
| `1260:1281` |  | 88 | 9 | yes | no |
| `1000:4089` |  | 402 | 2 | yes | no |
| `1000:3338` |  | 354 | 3 | yes | no |
| `1000:272a` |  | 209 | 6 | yes | no |
| `1000:1260` |  | 38 | 9 | yes | no |
| `1260:1c3c` |  | 673 | 1 | no | no |
| `1000:3ce3` |  | 81 | 8 | yes | no |
| `1000:17b3` |  | 167 | 6 | yes | no |
| `1000:38cc` |  | 23 | 9 | yes | no |
| `1000:35af` |  | 397 | 1 | yes | no |
| `1268:0304` |  | 133 | 6 | yes | no |
| `1260:31fc` |  | 579 | 2 | no | no |
| `1268:00e8` |  | 200 | 4 | yes | no |
| `1260:1edd` |  | 486 | 3 | no | no |
| `1000:45cd` |  | 33 | 7 | yes | no |
| `1000:3ffd` |  | 29 | 7 | yes | no |
| `1000:16ec` |  | 163 | 4 | yes | no |
| `1000:44b1` |  | 250 | 2 | yes | no |
| `1000:2664` |  | 197 | 3 | yes | no |
| `1000:3184` |  | 290 | 1 | yes | no |
| `1000:0b24` |  | 240 | 2 | yes | no |
| `1000:1408` |  | 43 | 6 | yes | yes |
| `1000:1c7b` |  | 271 | 1 | yes | no |
| `1000:2f7d` |  | 519 | 1 | no | no |
| `1000:37aa` |  | 106 | 4 | yes | no |
| `1000:2110` |  | 47 | 5 | yes | no |
| `1000:0e1a` |  | 188 | 2 | yes | no |
| `1000:2ddc` |  | 392 | 3 | no | no |
| `1260:1393` |  | 184 | 2 | yes | no |
| `1000:32eb` |  | 39 | 5 | yes | no |
| `1268:05ae` |  | 230 | 1 | yes | no |
| `1000:1434` |  | 35 | 5 | yes | no |
| `1000:3f9e` |  | 28 | 5 | yes | no |

The inferred-containing count can over-associate annotations when recovered candidates overlap. Exact starts remain the conservative metric.
