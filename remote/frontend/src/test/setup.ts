import '@testing-library/jest-dom/vitest'
import { afterEach, vi } from 'vitest'
import { setCsrfToken } from '../lib/api'

afterEach(() => {
  setCsrfToken(null)
  vi.restoreAllMocks()
})
