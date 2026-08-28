import type { ByteWriter } from "./tdtByteWriter";
import { TDT_STAMP_GENERATION, TDT_STAMP_MAGIC, TDT_STAMP_SIZE } from "./tdtConstants";

/**
 * The trailer Verticopolis stamps on a `.TDT` it writes, and the read of it.
 *
 * Both halves live here on purpose. They are the only two places that know the
 * trailer's shape, and a format marker whose writer and reader can drift apart
 * is worse than no marker at all: it would state a falsehood with authority.
 *
 * What the trailer is for, where it sits, and why the 1994 game does not care
 * about it: see {@link TDT_STAMP_MAGIC} and docs/canon/tdt-format.md §12a.
 */

/** Append the trailer. Must be the LAST thing written to the file. */
export function writeFormatStamp(w: ByteWriter): void {
  for (const ch of TDT_STAMP_MAGIC) w.u8(ch.charCodeAt(0));
  w.u16(TDT_STAMP_GENERATION);
}

/**
 * The generation our trailer claims, or null when the file carries none.
 *
 * Unstamped means one of ours from before the trailer existed, or a save the
 * 1994 game wrote (its own re-save of our file drops the trailer, since it only
 * writes the extent it knows). Callers must treat null as "decide by other
 * means", never as a reason to refuse the file.
 */
export function stampedGeneration(bytes: Uint8Array): number | null {
  if (bytes.length < TDT_STAMP_SIZE) return null;
  const at = bytes.length - TDT_STAMP_SIZE;
  for (let i = 0; i < TDT_STAMP_MAGIC.length; i++) {
    if (bytes[at + i] !== TDT_STAMP_MAGIC.charCodeAt(i)) return null;
  }
  const g = at + TDT_STAMP_MAGIC.length;
  return bytes[g] | (bytes[g + 1] << 8);
}
