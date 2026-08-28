/**
 * Pure mapping between a saved camera view (our grid units) and the 1994
 * header's view-scroll words (world pixels). Extracted from `tdtFormat.ts`;
 * engine-free by design (the import already re-clamps through
 * `Simulation.deserialize`).
 */
import {
  TDT_DEFAULT_VIEW_X,
  TDT_DEFAULT_VIEW_Y,
  TDT_FLOOR_COUNT,
  TDT_FLOOR_OFFSET,
  TDT_FLOOR_PX,
  TDT_TILE_PX,
  TDT_VIEW_H,
  TDT_VIEW_W,
  TDT_WORLD_H,
  TDT_WORLD_W,
} from "./tdtConstants";

/**
 * Map a saved camera view (center in OUR grid units) to the header's
 * view-scroll words (top-left of the 1994 window, world px). Non-finite
 * members fall back to the New Tower default words; finite values clamp into
 * the window range the 1994 game can actually scroll to, so a view centered
 * on B10 lands as close as the window allows (ground at the bottom edge)
 * rather than out of range.
 */
export function viewWordsFromView(view: { tile: number; floor: number }): { x: number; y: number } {
  if (!Number.isFinite(view.tile) || !Number.isFinite(view.floor)) {
    return { x: TDT_DEFAULT_VIEW_X, y: TDT_DEFAULT_VIEW_Y };
  }
  const fTdt = view.floor + TDT_FLOOR_OFFSET;
  const centerX = view.tile * TDT_TILE_PX;
  // Floor f (index from the bottom) spans world y in
  // [(119 - f) * 36, (120 - f) * 36]; its center is the top edge + 18.
  const centerY = (TDT_FLOOR_COUNT - 1 - fTdt) * TDT_FLOOR_PX + TDT_FLOOR_PX / 2;
  const x = Math.max(0, Math.min(TDT_WORLD_W - TDT_VIEW_W, Math.round(centerX - TDT_VIEW_W / 2)));
  const y = Math.max(0, Math.min(TDT_WORLD_H - TDT_VIEW_H, Math.round(centerY - TDT_VIEW_H / 2)));
  // Never emit the (0, 0) "no saved view" sentinel for a REAL view: an
  // unclamped input (top-left extreme) would otherwise vanish on re-import.
  // One pixel of scroll is imperceptible but unambiguous.
  return x === 0 && y === 0 ? { x: 0, y: 1 } : { x, y };
}

/**
 * Inverse of {@link viewWordsFromView}: header view-scroll words → camera
 * center in OUR grid units. The (0, 0) pair is the 1994 "no saved view"
 * failure mode (the game then opens at the top-left sky) and maps to null so
 * the renderer falls back to centering. Values stay FRACTIONAL (a camera
 * center is fractional by nature, and rounding here would make
 * export → import → export drift by a word, breaking the exporter's
 * idempotence test), and are deliberately NOT clamped to our grid: this
 * walker stays engine-free, and every import already passes
 * `Simulation.deserialize`, whose trust boundary clamps the view along with
 * everything else.
 */
export function viewFromViewWords(x: number, y: number): { tile: number; floor: number } | null {
  if (x === 0 && y === 0) return null;
  const tile = (x + TDT_VIEW_W / 2) / TDT_TILE_PX;
  const centerY = y + TDT_VIEW_H / 2;
  const fTdt = TDT_FLOOR_COUNT - 1 - (centerY - TDT_FLOOR_PX / 2) / TDT_FLOOR_PX;
  return { tile, floor: fTdt - TDT_FLOOR_OFFSET };
}
