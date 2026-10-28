import { svelte } from '@sveltejs/vite-plugin-svelte'
import { defineConfig } from 'vite'

// BASE_PATH is set by the GitHub Pages workflow, which serves the app under /<repo>/.
export default defineConfig({
  base: process.env.BASE_PATH ?? '/',
  plugins: [svelte()],
})
