// 1288:c05a TowerRouteQueueRecord (324-byte per floor-slot)
//
// Per-floor route-request ring buffer, mirroring the binary's
// TowerRouteQueueRecord layout (2 count/head bytes per direction + two
// 40-entry request ref arrays). Routing-wise the only observable part is
// the FIFO order of the ring and the size-40 silent wrap.

const ROUTE_QUEUE_CAPACITY = 40;

/**
 * Fixed-40-entry ring of route request ids for one direction of one floor.
 *
 * Binary quirk: size-40 ring silently overwrites head on 41st enqueue.
 * `enqueue_request_into_route_queue` (1218:1002) computes the write slot as
 * `(head + count) % 40` with no capacity check. On the 41st enqueue the
 * write index wraps back to `head`, overwriting the oldest entry; `count`
 * then saturates at 40 (the binary increments count but the 41st slot is
 * the one we just clobbered, so effectively head advances).
 */
export class RouteRequestRing {
	readonly capacity: number = ROUTE_QUEUE_CAPACITY;
	items: string[];
	head: number;
	count: number;

	constructor() {
		this.items = new Array(ROUTE_QUEUE_CAPACITY).fill("");
		this.head = 0;
		this.count = 0;
	}

	/** Reconstitute a RouteRequestRing from a plain-object snapshot. */
	static from(data: {
		items: string[];
		head: number;
		count: number;
	}): RouteRequestRing {
		const buf = new RouteRequestRing();
		const len = Math.min(ROUTE_QUEUE_CAPACITY, data.items.length);
		for (let i = 0; i < len; i++) buf.items[i] = data.items[i] ?? "";
		buf.head = data.head % ROUTE_QUEUE_CAPACITY;
		buf.count = Math.min(ROUTE_QUEUE_CAPACITY, Math.max(0, data.count));
		return buf;
	}

	get size(): number {
		return this.count;
	}

	get isFull(): boolean {
		return this.count >= ROUTE_QUEUE_CAPACITY;
	}

	get isEmpty(): boolean {
		return this.count === 0;
	}

	/**
	 * Append a request id to the ring.
	 *
	 * Binary quirk: size-40 ring silently overwrites head on 41st enqueue.
	 * When the ring is already full, the write slot is `(head + 40) % 40 ==
	 * head`, so the oldest entry is clobbered and the effective queue order
	 * shifts by one (head now points at what used to be index 1). Returns
	 * true unconditionally — the binary has no full flag.
	 */
	push(item: string): boolean {
		const writeIndex = (this.head + this.count) % ROUTE_QUEUE_CAPACITY;
		this.items[writeIndex] = item;
		if (this.count < ROUTE_QUEUE_CAPACITY) {
			this.count += 1;
		} else {
			// Binary quirk: size-40 ring silently overwrites head on 41st enqueue.
			// The 41st push writes at index `head`, and the emulated count stays
			// pinned at 40; logical head advances so the newly-written item is
			// the new tail (last-to-pop).
			this.head = (this.head + 1) % ROUTE_QUEUE_CAPACITY;
		}
		return true;
	}

	/** Remove and return the oldest item, or undefined if empty. */
	pop(): string | undefined {
		if (this.count <= 0) return undefined;
		const item = this.items[this.head];
		this.head = (this.head + 1) % ROUTE_QUEUE_CAPACITY;
		this.count -= 1;
		return item;
	}

	/** Return all items in queue order without removing them. */
	peekAll(): string[] {
		return Array.from(
			{ length: this.count },
			(_, i) => this.items[(this.head + i) % ROUTE_QUEUE_CAPACITY] ?? "",
		);
	}

	/**
	 * Scan-and-remove: pop the first occurrence of `item` while preserving
	 * the relative order of the remaining entries. Returns true if removed.
	 * Mirrors the compaction pass of `remove_request_from_unit_queue`
	 * (1218:142a).
	 */
	removeFirst(item: string): boolean {
		if (this.count === 0) return false;
		const remaining: string[] = [];
		let found = false;
		while (!this.isEmpty) {
			const popped = this.pop();
			if (!found && popped === item) {
				found = true;
				continue;
			}
			if (popped !== undefined) remaining.push(popped);
		}
		for (const id of remaining) this.push(id);
		return found;
	}
}

export const ROUTE_QUEUE_CAPACITY_CONST = ROUTE_QUEUE_CAPACITY;
