import { authReducer, canAccessAdmin, initialAuthState } from './session'
import type { User } from '../lib/types'

const user = (role: User['role']): User => ({ id: 'u1', name: 'Ada', email: 'ada@example.com', role })

describe('authentication state', () => {
  it('moves from loading to an authenticated session without persisting a token', () => {
    const state = authReducer(initialAuthState, { type: 'authenticated', user: user('user') })
    expect(state).toEqual({ status: 'authenticated', user: user('user') })
    expect(localStorage).toHaveLength(0)
  })

  it('clears the user for anonymous sessions', () => {
    expect(authReducer({ status: 'authenticated', user: user('user') }, { type: 'anonymous' })).toEqual({ status: 'anonymous', user: null })
  })

  it('allows only authenticated administrators into admin routes', () => {
    expect(canAccessAdmin({ status: 'authenticated', user: user('admin') })).toBe(true)
    expect(canAccessAdmin({ status: 'authenticated', user: user('user') })).toBe(false)
    expect(canAccessAdmin(initialAuthState)).toBe(false)
  })
})
