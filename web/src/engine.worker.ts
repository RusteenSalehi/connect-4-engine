// Runs the WebAssembly engine off the page's main thread. Answers one request at a time; the
// engine itself is synchronous.

import createModule from './wasm/c4.js';
import wasmUrl from './wasm/c4.wasm?url';
import type { EngineMove, Request, Response } from './engine';

// Emscripten looks for c4.wasm next to c4.js by default. Handing it the URL Vite assigned
// instead means the file is found in `npm run dev` and in the build, where it lives under the
// /connect-4-engine/ base with a content hash in its name.
const modulePromise = createModule({ locateFile: () => wasmUrl });

self.onmessage = async (event: MessageEvent<Request>) => {
  const request = event.data;
  let response: Response;
  try {
    const c4 = await modulePromise;
    const stat = (name: string) => c4.ccall(name, 'number', [], []);
    if (request.kind === 'winningCells') {
      response = { id: request.id, value: c4.ccall('c4_winning_cells', 'number', ['string'], [request.moves]) };
    } else {
      const column = c4.ccall('c4_choose_move', 'number', ['string', 'number', 'number'], [
        request.moves,
        request.difficulty,
        request.seed,
      ]);
      const move: EngineMove = {
        column,
        depth: stat('c4_last_depth'),
        score: stat('c4_last_score'),
        nodes: stat('c4_last_nodes'),
        nps: stat('c4_last_nps'),
        timeMs: stat('c4_last_time_ms'),
      };
      response = { id: request.id, value: move };
    }
  } catch (e) {
    response = { id: request.id, error: String(e) };
  }
  self.postMessage(response);
};
