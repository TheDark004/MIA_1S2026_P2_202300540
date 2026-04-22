import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    port: 3000,
    proxy: {
      "/execute": "http://localhost:18080",
      "/health": "http://localhost:18080",
      "/login": "http://localhost:18080",
      "/get_disks": "http://localhost:18080",
      "/get_partitions": "http://localhost:18080",
      "/fs": "http://localhost:18080",
      "/reports": "http://localhost:18080",
    },
  },
});
