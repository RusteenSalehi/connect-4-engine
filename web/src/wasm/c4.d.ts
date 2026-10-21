// Types for the Emscripten output copied here by `npm run wasm` (c4.js itself is not in git).
// Only the parts the worker uses.
export interface C4Module {
  ccall(name: string, returnType: 'number', argTypes: string[], args: unknown[]): number;
}

export default function createModule(options?: {
  locateFile?: (path: string) => string;
}): Promise<C4Module>;
