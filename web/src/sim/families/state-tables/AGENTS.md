# families/state-tables/ — Binary dispatch jump tables

TS translations of the binary's CS-relative jump tables for family dispatchers. Phase 5a populates them as documentation; Phase 5b wires them in.

## Files

### `family-prologue.ts`
0x22-entry table at cs:1c71 mapping `family_code - 3` → prologue handler.

### `office.ts`
Tables at cs:2005 (refresh), cs:2aac (dispatch), cs:1c51 (prologue).

### `hotel.ts`
Table at cs:1c41 (shared family 3/4/5 prologue).

### `condo.ts`
Table at cs:1c2d (family-9 prologue).
