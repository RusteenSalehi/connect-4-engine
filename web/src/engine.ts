// Promise-based front for the engine worker. The search runs in a Web Worker because Impossible
// searches for about a second, and running it on the page's own thread would freeze the page.

// Index = the engine's difficulty number.
export const DIFFICULTIES = ['Easy', 'Medium', 'Hard', 'Impossible'];

export interface EngineMove {
  column: number; // 0 to 6
  depth: number;
  score: number; // engine's point of view, before its move
  nodes: number;
  nps: number;
  timeMs: number;
}

// Messages between this file and engine.worker.ts.
export type Request =
  | { id: number; kind: 'move'; moves: string; difficulty: number; seed: number }
  | { id: number; kind: 'winningCells'; moves: string };
export type Response = { id: number; value?: unknown; error?: string };

const worker = new Worker(new URL('./engine.worker.ts', import.meta.url), { type: 'module' });

// Each request gets an id so its answer reaches the right promise.
let nextId = 0;
const pending = new Map<number, { resolve: (value: unknown) => void; reject: (e: Error) => void }>();

worker.onmessage = (event: MessageEvent<Response>) => {
  const { id, value, error } = event.data;
  const p = pending.get(id);
  pending.delete(id);
  if (error !== undefined) p?.reject(new Error(error));
  else p?.resolve(value);
};

// A worker that fails to load (for example, a missing c4.wasm) would otherwise leave every
// request waiting forever.
worker.onerror = (event) => {
  event.preventDefault();
  for (const p of pending.values()) p.reject(new Error(event.message || 'engine failed to load'));
  pending.clear();
};

function send(request: Request): Promise<unknown> {
  return new Promise((resolve, reject) => {
    pending.set(request.id, { resolve, reject });
    worker.postMessage(request);
  });
}

export async function chooseMove(moves: string, difficulty: number): Promise<EngineMove> {
  // A fresh seed per move keeps the weaker levels varied. Below 2^32 is well inside the
  // engine's limit (a non-negative integer below 2^53).
  const seed = Math.floor(Math.random() * 2 ** 32);
  const move = (await send({ id: nextId++, kind: 'move', moves, difficulty, seed })) as EngineMove;
  // -1 means the engine rejected the position (invalid or already finished), never a move.
  if (move.column < 0) throw new Error(`engine refused position "${moves}"`);
  return move;
}

// Bitmask of the cells in every line the last mover completed; 0 if nobody has won.
export async function winningCells(moves: string): Promise<number> {
  return (await send({ id: nextId++, kind: 'winningCells', moves })) as number;
}
