/**
 * Little-endian byte writer for the `.TDT` exporter: the mirror of
 * `tdtFormat`'s ByteReader. Extracted from `tdtExport.ts`, where these were
 * ad-hoc closures over a shared `number[]`. A byte is appended low-first, and
 * {@link ByteWriter.setU16} back-patches a u16 already written (the header
 * aggregate counts are emitted as zero padding, then patched at fixed offsets
 * once their totals are known).
 */
export class ByteWriter {
  private readonly chunks: number[] = [];

  /** One byte (masked to 8 bits, matching the original `& 0xff` closures). */
  u8(v: number): void {
    this.chunks.push(v & 0xff);
  }

  /** A little-endian u16. */
  u16(v: number): void {
    this.chunks.push(v & 0xff, (v >> 8) & 0xff);
  }

  /** A little-endian i32 (two's complement via the byte masks). */
  i32(v: number): void {
    this.chunks.push(v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff);
  }

  /** `n` zero bytes. */
  pad(n: number): void {
    for (let i = 0; i < n; i++) this.chunks.push(0);
  }

  /** `n` bytes of 0xFF (the format's empty-slot sentinel). */
  padFF(n: number): void {
    for (let i = 0; i < n; i++) this.chunks.push(0xff);
  }

  /** Back-patch a little-endian u16 at an absolute byte offset already written
   *  (used for the header aggregate counts after the header is zero-padded).
   *  Both target bytes must already exist: patching past the end would leave
   *  sparse array holes that `toBytes` silently reads as 0, so an out-of-range
   *  offset fails loudly instead. */
  setU16(off: number, v: number): void {
    if (off < 0 || off + 2 > this.chunks.length) {
      throw new RangeError(
        `ByteWriter.setU16 offset ${off} is out of range for a ${this.chunks.length}-byte buffer; it only back-patches bytes already written`,
      );
    }
    this.chunks[off] = v & 0xff;
    this.chunks[off + 1] = (v >> 8) & 0xff;
  }

  /** Bytes written so far (the original read `chunks.length`). */
  get length(): number {
    return this.chunks.length;
  }

  /** The finished buffer. */
  toBytes(): Uint8Array {
    return new Uint8Array(this.chunks);
  }
}
