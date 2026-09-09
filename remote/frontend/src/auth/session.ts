import type { User } from '../lib/types'

export type AuthState = { status: 'loading' | 'authenticated' | 'anonymous'; user: User | null }
export const initialAuthState: AuthState = { status: 'loading', user: null }
export const canAccessAdmin = (state: AuthState) => state.status === 'authenticated' && state.user?.role === 'admin'

export function authReducer(state: AuthState, action: { type: 'authenticated'; user: User } | { type: 'anonymous' }): AuthState {
  if (action.type === 'authenticated') return { status: 'authenticated', user: action.user }
  return { status: 'anonymous', user: null }
}
