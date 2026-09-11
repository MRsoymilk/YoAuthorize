import { defineConfig } from 'vitest/config'
import { loadEnv } from 'vite'
import { fileURLToPath } from 'node:url'
import react from '@vitejs/plugin-react'

type DevSettings = {
  FRONTEND_DEV_PORT?: string
  FRONTEND_API_TARGET?: string
}

export function resolveDevServer(values: DevSettings, overrides: DevSettings = {}) {
  const rawPort = overrides.FRONTEND_DEV_PORT ?? values.FRONTEND_DEV_PORT ?? '5173'
  const port = Number(rawPort)
  if (!/^\d+$/.test(rawPort) || !Number.isInteger(port) || port < 1 || port > 65535) {
    throw new Error('FRONTEND_DEV_PORT must be an integer from 1 to 65535')
  }

  const target = overrides.FRONTEND_API_TARGET ?? values.FRONTEND_API_TARGET ?? 'http://localhost:8088'
  let url: URL
  try {
    url = new URL(target)
  } catch {
    throw new Error('FRONTEND_API_TARGET must be an HTTP(S) origin without credentials, path, query, or hash')
  }
  // Check the raw form too: URL parsing normalizes dot paths and empty query/hash markers.
  if (!/^https?:\/\/[^/?#@\\\s]+\/?$/i.test(target) || url.pathname !== '/' || url.username || url.password || url.search || url.hash) {
    throw new Error('FRONTEND_API_TARGET must be an HTTP(S) origin without credentials, path, query, or hash')
  }

  return {
    port,
    strictPort: true,
    proxy: { '/api': { target, changeOrigin: true } },
  }
}

export default defineConfig(({ command, mode }) => {
  let server: ReturnType<typeof resolveDevServer> | undefined
  if (command === 'serve' && mode !== 'test') {
    // Keep deploy settings server-only; leave client envDir and envPrefix at Vite defaults.
    const deployEnv = loadEnv(mode, fileURLToPath(new URL('../deploy/', import.meta.url)), [
      'FRONTEND_DEV_PORT',
      'FRONTEND_API_TARGET',
    ])
    server = resolveDevServer({
      FRONTEND_DEV_PORT: deployEnv.FRONTEND_DEV_PORT,
      FRONTEND_API_TARGET: deployEnv.FRONTEND_API_TARGET,
    }, {
      FRONTEND_DEV_PORT: process.env.FRONTEND_DEV_PORT,
      FRONTEND_API_TARGET: process.env.FRONTEND_API_TARGET,
    })
  }
  return {
    plugins: [react()],
    server,
    test: {
      environment: 'jsdom',
      setupFiles: './src/test/setup.ts',
      globals: true,
    },
  }
})
