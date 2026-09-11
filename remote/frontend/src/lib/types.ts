export type Role = 'user' | 'admin'

export interface User {
  id: string
  email: string
  name: string
  role: Role
  status?: 'active' | 'pending' | 'suspended'
  createdAt?: string
}

export interface SessionResponse { user: User; csrfToken?: string }
export interface License {
  id: string
  key: string
  productName: string
  status: 'active' | 'expired' | 'revoked'
  expiresAt: string | null
  deviceLimit: number
  deviceCount: number
  features?: string[]
  userEmail?: string
}
export interface Device {
  id: string
  name: string
  fingerprint: string
  platform?: string
  lastSeenAt: string
  activatedAt: string
  licenseId?: string
}
export interface Product { id: string; name: string; code: string; description?: string; featureCount?: number }
export interface Feature { id: string; productId: string; name: string; code: string }
export interface ActivationCode { id: string; code: string; productName: string; status: string; createdAt: string; redeemedBy?: string }
export interface AuditLog { id: string; actorEmail?: string; action: string; target?: string; ipAddress?: string; createdAt: string }
export interface Page<T> { items: T[]; total: number; page?: number; pageSize?: number }
