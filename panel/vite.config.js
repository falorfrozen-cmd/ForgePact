import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

export default defineConfig({
  // Keep the terminal's earlier output (a Python traceback from the panel
  // server started beside this) instead of clearing it on start.
  clearScreen: false,
  plugins: [svelte()],
  // The fast loop: `py src/forgepact.py` serves /api on 8780 and this dev
  // server serves the page with hot reload. 5178 stays clear of the hub's 5177
  // and HS Offline Tracker's 5176, so all three can run at once.
  server: {
    port: 5178,
    strictPort: true,
    proxy: { '/api': 'http://127.0.0.1:8780' },
  },
  // Static files for src/forgepact.py to serve and build_release.py to bundle.
  build: { outDir: 'dist' },
});
