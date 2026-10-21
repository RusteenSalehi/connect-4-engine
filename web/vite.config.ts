import { defineConfig } from 'vite';

export default defineConfig({
  // GitHub Pages serves the site from https://<user>.github.io/connect-4-engine/.
  base: '/connect-4-engine/',
  // c4.js is an ES module (it uses import.meta.url), so the worker bundle must be one too.
  worker: { format: 'es' },
});
