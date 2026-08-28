import { defineConfig } from "vite";

export default defineConfig({
  base: "./",
  publicDir: "../original/assets_converted",
  build: {
    target: "es2022",
    assetsInlineLimit: 0,
    sourcemap: true,
  },
});
