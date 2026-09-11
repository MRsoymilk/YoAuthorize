import type { SessionResponse } from './types'

const API_BASE = (import.meta.env.VITE_API_BASE_URL as string | undefined)?.replace(/\/$/, '') || '/api/v1'
let csrfToken: string | null = null

export class ApiError extends Error {
  constructor(public status: number, message: string, public code?: string, public details?: unknown) {
    super(message)
    this.name = 'ApiError'
  }
}

export function setCsrfToken(token: string | null) { csrfToken = token }
export function getCsrfToken() { return csrfToken }

function messageForStatus(status: number) {
  if (status === 401) return 'Your session has expired. Please sign in again.'
  if (status === 403) return 'You do not have permission to do that.'
  if (status === 404) return 'The requested resource was not found.'
  if (status === 429) return 'Too many requests. Please wait and try again.'
  if (status >= 500) return 'The service is temporarily unavailable. Please try again.'
  return 'The request could not be completed.'
}

async function parseResponse(response: Response): Promise<unknown> {
  if (response.status === 204) return undefined
  const contentType = response.headers.get('content-type') || ''
  if (!contentType.includes('application/json')) return response.ok ? response.text() : undefined
  try { return await response.json() } catch { return undefined }
}

export async function request<T>(path: string, init: RequestInit = {}): Promise<T> {
  const method = (init.method || 'GET').toUpperCase()
  const headers = new Headers(init.headers)
  headers.set('Accept', 'application/json')
  if (init.body && !(init.body instanceof FormData)) headers.set('Content-Type', 'application/json')
  if (!['GET', 'HEAD', 'OPTIONS'].includes(method) && csrfToken) headers.set('X-CSRF-Token', csrfToken)

  let response: Response
  try {
    response = await fetch(`${API_BASE}${path}`, { ...init, method, headers, credentials: 'include' })
  } catch {
    throw new ApiError(0, 'Unable to reach the service. Check your connection and try again.', 'NETWORK_ERROR')
  }

  const headerToken = response.headers.get('x-csrf-token')
  if (headerToken) csrfToken = headerToken
  const data = await parseResponse(response)
  if (!response.ok) {
    const body = data && typeof data === 'object' ? data as Record<string, unknown> : {}
    throw new ApiError(response.status, typeof body.message === 'string' ? body.message : messageForStatus(response.status), typeof body.code === 'string' ? body.code : undefined, body.details)
  }
  return data as T
}

export const api = {
  get: <T>(path: string) => request<T>(path),
  post: <T>(path: string, body?: unknown) => request<T>(path, { method: 'POST', body: body === undefined ? undefined : JSON.stringify(body) }),
  put: <T>(path: string, body?: unknown) => request<T>(path, { method: 'PUT', body: body === undefined ? undefined : JSON.stringify(body) }),
  patch: <T>(path: string, body?: unknown) => request<T>(path, { method: 'PATCH', body: body === undefined ? undefined : JSON.stringify(body) }),
  delete: <T>(path: string) => request<T>(path, { method: 'DELETE' }),
  me: async () => {
    const result = await request<SessionResponse>('/auth/me')
    if (result.csrfToken) setCsrfToken(result.csrfToken)
    return result
  },
}

export function errorMessage(error: unknown) {
  return error instanceof ApiError ? error.message : 'Something went wrong. Please try again.'
}
