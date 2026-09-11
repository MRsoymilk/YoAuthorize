import { useState } from 'react'
import { NavLink, Outlet, useNavigate } from 'react-router-dom'
import { Activity, Boxes, KeyRound, LayoutGrid, LogOut, Menu, MonitorSmartphone, ShieldCheck, Ticket, Users, X } from 'lucide-react'
import { useAuth } from '../auth/AuthProvider'

const userLinks = [
  ['/app', 'Overview', LayoutGrid], ['/app/licenses', 'Licenses', KeyRound], ['/app/devices', 'Devices', MonitorSmartphone],
] as const
const adminLinks = [
  ['/admin/users', 'Users', Users], ['/admin/products', 'Products & features', Boxes], ['/admin/licenses', 'Licenses', ShieldCheck], ['/admin/codes', 'Activation codes', Ticket], ['/admin/audit', 'Audit log', Activity],
] as const

export function Shell() {
  const [open, setOpen] = useState(false)
  const { user, logout } = useAuth()
  const navigate = useNavigate()
  const signOut = async () => { await logout(); navigate('/login') }
  return <div className="shell">
    <button className="menu-button" aria-label="Open navigation" onClick={() => setOpen(true)}><Menu /></button>
    {open && <button className="scrim" aria-label="Close navigation" onClick={() => setOpen(false)} />}
    <aside className={open ? 'sidebar open' : 'sidebar'}>
      <div className="brand"><span className="brand-mark">Y</span><div>YoAuthorize<small>REMOTE CONTROL</small></div><button aria-label="Close navigation" onClick={() => setOpen(false)}><X /></button></div>
      <nav onClick={() => setOpen(false)}>
        <p>Workspace</p>{userLinks.map(([to, label, Icon]) => <NavLink key={to} to={to} end={to === '/app'}><Icon />{label}</NavLink>)}
        {user?.role === 'admin' && <><p>Administration</p>{adminLinks.map(([to, label, Icon]) => <NavLink key={to} to={to}><Icon />{label}</NavLink>)}</>}
      </nav>
      <div className="profile"><div className="avatar">{user?.name?.[0]?.toUpperCase() || user?.email[0].toUpperCase()}</div><div><strong>{user?.name || 'Account'}</strong><small>{user?.email}</small></div><button title="Sign out" onClick={() => void signOut()}><LogOut /></button></div>
    </aside>
    <main className="main"><Outlet /></main>
  </div>
}
