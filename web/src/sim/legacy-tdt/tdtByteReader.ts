/**
 * Bounds-checked little-endian reader for the `.TDT` binary walk. Extracted
 * from `tdtFormat.ts`. Every read names the block it happened in (via
 * {@link ByteReader.enterBlock}) so an overrun throws a typed, honest
 * "truncated at <block>" instead of a raw RangeError.
 */
import { LegacyImportError } from "./tdtConstants";

export class ByteReader {
  private pos = 0;
  private block = "header";
  private readonly view: DataView;

  constructor(private readonly bytes_: Uint8Array) {
    this.view = new DataView(bytes_.buffer, bytes_.byteOffset, bytes_.byteLength);
  }

  /** Name the block subsequent reads belong to (for truncation messages). */
  enterBlock(name: string): void {
    this.block = name;
  }

  remaining(): number {
    return this.bytes_.byteLength - this.pos;
  }

  offset(): number {
    return this.pos;
  }

  /** The underlying buffer, for tail structures we locate by scanning for a
   *  record signature rather than by a byte offset we can't pin down. */
  raw(): Uint8Array {
    return this.bytes_;
  }

  private need(n: number): void {
    if (n < 0 || this.remaining() < n) {
      throw new LegacyImportError(
        `This SimTower save is cut short. The file ends in the middle of its ${this.block}.`,
      );
    }
  }

  skip(n: number): void {
    this.need(n);
    this.pos += n;
  }

  /** Read `n` raw bytes as a copy (so later reads can't mutate it). */
  bytes(n: number): Uint8Array {
    this.need(n);
    const out = this.bytes_.slice(this.pos, this.pos + n);
    this.pos += n;
    return out;
  }

  u8(): number {
    this.need(1);
    return this.view.getUint8(this.pos++);
  }

  i8(): number {
    this.need(1);
    return this.view.getInt8(this.pos++);
  }

  u16(): number {
    this.need(2);
    const v = this.view.getUint16(this.pos, true);
    this.pos += 2;
    return v;
  }

  i16(): number {
    this.need(2);
    const v = this.view.getInt16(this.pos, true);
    this.pos += 2;
    return v;
  }

  u32(): number {
    this.need(4);
    const v = this.view.getUint32(this.pos, true);
    this.pos += 4;
    return v;
  }

  i32(): number {
    this.need(4);
    const v = this.view.getInt32(this.pos, true);
    this.pos += 4;
    return v;
  }
}
