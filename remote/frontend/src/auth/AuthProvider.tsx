import { createContext, useContext, useEffect, useReducer, type ReactNode } from 'react'
import { api, setCsrfToken } from '../lib/api'
import { authReducer, initialAuthState, type AuthState } from './session'

type AuthContextValue = AuthState & { refresh: () => Promise<void>; logout: () => Promise<void> }
const AuthContext = createContext<AuthContextValue | null>(null)

export function AuthProvider({ children }: { children: ReactNode }) {
  const [state, dispatch] = useReducer(authReducer, initialAuthState)
  const refresh = async () => {
    try { dispatch({ type: 'authenticated', user: (await api.me()).user }) }
    catch { setCsrfToken(null); dispatch({ type: 'anonymous' }) }
  }
  useEffect(() => { void refresh() }, [])
  const logout = async () => {
    try { await api.post('/auth/logout') } finally { setCsrfToken(null); dispatch({ type: 'anonymous' }) }
  }
  return <AuthContext.Provider value={{ ...state, refresh, logout }}>{children}</AuthContext.Provider>
}

export function useAuth() {
  const context = useContext(AuthContext)
  if (!context) throw new Error('useAuth must be used inside AuthProvider')
  return context
}
