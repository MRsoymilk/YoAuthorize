import { ApiError, api, errorMessage, getCsrfToken, request, setCsrfToken } from './api'

describe('API client', () => {
  it('uses cookie credentials and attaches CSRF to mutations', async () => {
    setCsrfToken('csrf-123')
    const fetchMock = vi.spyOn(globalThis, 'fetch').mockResolvedValue(new Response(JSON.stringify({ ok: true }), { status: 200, headers: { 'content-type': 'application/json' } }))
    await api.post('/devices/unbind', { id: 'one' })
    expect(fetchMock).toHaveBeenCalledWith('/api/v1/devices/unbind', expect.objectContaining({ credentials: 'include', method: 'POST' }))
    const headers = fetchMock.mock.calls[0][1]?.headers as Headers
    expect(headers.get('X-CSRF-Token')).toBe('csrf-123')
  })

  it('learns the CSRF token from a response header', async () => {
    vi.spyOn(globalThis, 'fetch').mockResolvedValue(new Response('{}', { status: 200, headers: { 'content-type': 'application/json', 'x-csrf-token': 'fresh' } }))
    await request('/auth/me')
    expect(getCsrfToken()).toBe('fresh')
  })

  it('learns the CSRF token from the me response body', async () => {
    vi.spyOn(globalThis, 'fetch').mockResolvedValue(new Response(JSON.stringify({ user: { id: '1' }, csrfToken: 'body-token' }), { status: 200, headers: { 'content-type': 'application/json' } }))
    await api.me()
    expect(getCsrfToken()).toBe('body-token')
  })

  it('normalizes structured and network errors', async () => {
    vi.spyOn(globalThis, 'fetch').mockResolvedValueOnce(new Response(JSON.stringify({ message: 'Email is invalid', code: 'BAD_EMAIL' }), { status: 422, headers: { 'content-type': 'application/json' } }))
    await expect(request('/test')).rejects.toMatchObject({ status: 422, code: 'BAD_EMAIL', message: 'Email is invalid' })
    vi.mocked(fetch).mockRejectedValueOnce(new TypeError('offline'))
    await expect(request('/test')).rejects.toEqual(expect.objectContaining({ status: 0, code: 'NETWORK_ERROR' }))
  })

  it('returns a stable fallback for unknown errors', () => {
    expect(errorMessage(new Error('internal detail'))).toBe('Something went wrong. Please try again.')
    expect(errorMessage(new ApiError(403, 'Forbidden'))).toBe('Forbidden')
  })
})
