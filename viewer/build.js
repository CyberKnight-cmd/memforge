/**
 * MemForge P9 - Single-file standalone HTML bundler
 * Inlines style.css and app.js into viewer/standalone.html
 */

const fs = require('fs');
const path = require('path');

const viewerDir = __dirname;
const htmlPath = path.join(viewerDir, 'index.html');
const cssPath = path.join(viewerDir, 'style.css');
const jsPath = path.join(viewerDir, 'app.js');
const standalonePath = path.join(viewerDir, 'standalone.html');

let html = fs.readFileSync(htmlPath, 'utf8');
const css = fs.readFileSync(cssPath, 'utf8');
const js = fs.readFileSync(jsPath, 'utf8');

// Replace external CSS link with inline <style>
html = html.replace(
  '<link rel="stylesheet" href="style.css">',
  `<style>\n${css}\n</style>`
);

// Replace external JS script with inline <script>
html = html.replace(
  '<script src="app.js"></script>',
  `<script>\n${js}\n</script>`
);

fs.writeFileSync(standalonePath, html, 'utf8');
console.log(`Generated self-contained standalone viewer: ${standalonePath} (${(fs.statSync(standalonePath).size / 1024).toFixed(1)} KB)`);
