import { Navigate, Outlet, useLocation } from 'react-router-dom'
import { LoadingScreen } from '../components/UI'
import { useAuth } from './AuthProvider'

export function UserGuard() {
  const auth = useAuth()
  const location = useLocation()
  if (auth.status === 'loading') return <LoadingScreen />
  if (auth.status === 'anonymous') return <Navigate to="/login" state={{ from: location }} replace />
  return <Outlet />
}

export function AdminGuard() {
  const auth = useAuth()
  if (auth.status === 'loading') return <LoadingScreen />
  if (auth.status !== 'authenticated') return <Navigate to="/login" replace />
  if (auth.user?.role !== 'admin') return <Navigate to="/app" replace />
  return <Outlet />
}
