// Route-scoring shared constants. Not a binary-named function; hosts the
// cost-infinite sentinel and the stairs-extra-cost offset referenced by
// the per-scorer files in this directory.

export const ROUTE_COST_INFINITE = 0x7fff;
export const STAIRS_ROUTE_EXTRA_COST = 0x280; // 640
export const DIRECT_ROUTE_BASE_COST = 0x280; // 640 (11b8:168e direct branch)
export const DIRECT_ROUTE_FULL_QUEUE_COST = 0x3e8; // 1000 (replaces base when qByte == 0x28, mode != 0)
export const TRANSFER_ROUTE_BASE_COST = 0xbb8; // 3000 (11b8:168e transfer branch)
export const TRANSFER_ROUTE_FULL_QUEUE_COST = 0x1770; // 6000 (replaces base when qByte == 0x28, mode != 0)
export const QUEUE_FULL_COUNT = 0x28; // 40 (ring buffer capacity)
