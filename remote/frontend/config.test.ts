// @vitest-environment node
import { afterEach, describe, expect, it, vi } from 'vitest'
import { build, loadEnv } from 'vite'
import { fileURLToPath } from 'node:url'
import config, { resolveDevServer } from './vite.config'

vi.mock('vite', async importOriginal => ({
  ...await importOriginal<typeof import('vite')>(),
  loadEnv: vi.fn(),
}))

afterEach(() => {
  vi.unstubAllEnvs()
  vi.resetAllMocks()
})

describe('Vite config callback', () => {
  it.each([
    { command: 'build' as const, mode: 'production' },
    { command: 'build' as const, mode: 'development' },
    { command: 'serve' as const, mode: 'test' },
  ])('ignores invalid dev settings for $command in $mode mode', async env => {
    vi.stubEnv('FRONTEND_DEV_PORT', 'invalid-port')
    vi.stubEnv('FRONTEND_API_TARGET', 'invalid-target')
    const resolved = await config(env)
    expect(resolved.server).toBeUndefined()
    expect(loadEnv).not.toHaveBeenCalled()
  })

  it('loads module-relative deploy settings for serve with process precedence and no client exposure', async () => {
    vi.mocked(loadEnv).mockReturnValue({
      FRONTEND_DEV_PORT: '6000',
      FRONTEND_API_TARGET: 'http://deploy.example:8088',
      DATABASE_PASSWORD: 'deploy-secret-marker',
      VITE_DEPLOY_SECRET: 'deploy-public-prefix-marker',
    })
    vi.stubEnv('FRONTEND_DEV_PORT', '7000')
    vi.stubEnv('FRONTEND_API_TARGET', 'https://caddy.example:8443')
    const resolved = await config({ command: 'serve', mode: 'development' })
    expect(loadEnv).toHaveBeenCalledExactlyOnceWith('development',
      fileURLToPath(new URL('../deploy/', import.meta.url)), ['FRONTEND_DEV_PORT', 'FRONTEND_API_TARGET'])
    expect(resolved.server).toEqual({
      port: 7000, strictPort: true,
      proxy: { '/api': { target: 'https://caddy.example:8443', changeOrigin: true } },
    })
    expect(resolved.envDir).toBeUndefined()
    expect(resolved.envPrefix).toBeUndefined()
    expect(resolved.define).toBeUndefined()
    expect(JSON.stringify(resolved)).not.toContain('marker')
  })

  it('keeps private process values out of generated client artifacts', async () => {
    vi.stubEnv('FRONTEND_DEV_PORT', 'invalid-port-marker')
    vi.stubEnv('FRONTEND_API_TARGET', 'invalid-target-marker')
    vi.stubEnv('DATABASE_PASSWORD', 'private-password-marker')
    const result = await build({
      ...await config({ command: 'build', mode: 'production' }),
      configFile: false,
      logLevel: 'silent',
      plugins: [{
        name: 'env-probe',
        resolveId: id => id === 'virtual:env-probe' ? '\0env-probe' : undefined,
        load: id => id === '\0env-probe' ? 'console.log(import.meta.env)' : undefined,
      }],
      build: { write: false, rollupOptions: { input: 'virtual:env-probe' } },
    })
    const artifacts = JSON.stringify(result)
    expect(artifacts).toContain('PROD')
    expect(artifacts).not.toContain('marker')
    expect(artifacts).not.toContain('DATABASE_PASSWORD')
    expect(artifacts).not.toContain('FRONTEND_API_TARGET')
    expect(artifacts).not.toContain('FRONTEND_DEV_PORT')
    expect(loadEnv).not.toHaveBeenCalled()
  })
})

describe('frontend dev server settings', () => {
  it('defaults to a strict port and the Compose Caddy proxy without rewriting API paths', () => {
    expect(resolveDevServer({})).toEqual({
      port: 5173,
      strictPort: true,
      proxy: { '/api': { target: 'http://localhost:8088', changeOrigin: true } },
    })
  })

  it('uses selected deploy values and gives process overrides precedence', () => {
    const values = { FRONTEND_DEV_PORT: '6000', FRONTEND_API_TARGET: 'https://caddy.example:8443/' }
    expect(resolveDevServer(values)).toMatchObject({
      port: 6000,
      proxy: { '/api': { target: values.FRONTEND_API_TARGET, changeOrigin: true } },
    })
    expect(resolveDevServer(values, {
      FRONTEND_DEV_PORT: '7000', FRONTEND_API_TARGET: 'http://localhost:8080',
    })).toMatchObject({ port: 7000, proxy: { '/api': { target: 'http://localhost:8080' } } })
  })

  it.each(['1', '65535'])('accepts boundary port %s', port => {
    expect(resolveDevServer({ FRONTEND_DEV_PORT: port }).port).toBe(Number(port))
  })

  it.each(['', '0', '65536', '-1', '1.5', '5173abc', '1e3', '0x50', '+80', ' 80 ', 'Infinity'])('rejects invalid port %j', port => {
    expect(() => resolveDevServer({ FRONTEND_DEV_PORT: port })).toThrow('FRONTEND_DEV_PORT')
  })

  it.each(['http://localhost:8080', 'https://caddy.example/', 'http://[::1]:8088', 'https://external.example:443'])('preserves explicit target %s', target => {
    expect(resolveDevServer({ FRONTEND_API_TARGET: target }).proxy['/api']).toEqual({ target, changeOrigin: true })
  })

  it.each(['', 'not a url', 'ftp://localhost', '//localhost', 'http://user:pass@localhost', 'http://@localhost', 'http://localhost/api', 'http://localhost/api/..', 'http://localhost?x=1', 'http://localhost?', 'http://localhost#x', 'http://localhost#', 'http://localhost:65536', ' http://localhost', 'http://localhost\\api'])('rejects invalid target %j', target => {
    expect(() => resolveDevServer({ FRONTEND_API_TARGET: target })).toThrow('FRONTEND_API_TARGET')
  })

  it('does not fall back when a process override is empty', () => {
    expect(() => resolveDevServer({ FRONTEND_DEV_PORT: '5173' }, { FRONTEND_DEV_PORT: '' })).toThrow('FRONTEND_DEV_PORT')
    expect(() => resolveDevServer({ FRONTEND_API_TARGET: 'http://localhost' }, { FRONTEND_API_TARGET: '' })).toThrow('FRONTEND_API_TARGET')
  })
})
