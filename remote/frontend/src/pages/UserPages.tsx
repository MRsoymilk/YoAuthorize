import { useEffect, useState } from 'react'
import { Link } from 'react-router-dom'
import { ArrowUpRight, KeyRound, MonitorSmartphone, Radio, ShieldCheck } from 'lucide-react'
import { EmptyState, formatDate, PageTitle, Status } from '../components/UI'
import { api, errorMessage } from '../lib/api'
import type { Device, License } from '../lib/types'

function useResource<T>(path: string, initial: T) {
  const [data, setData] = useState(initial); const [error, setError] = useState(''); const [loading, setLoading] = useState(true)
  const reload = async () => { setLoading(true); try { setData(await api.get<T>(path)); setError('') } catch (e) { setError(errorMessage(e)) } finally { setLoading(false) } }
  useEffect(() => { void reload() }, [path])
  return { data, error, loading, reload }
}

export function DashboardPage() {
  const licenses = useResource<License[]>('/licenses', []); const devices = useResource<Device[]>('/devices', [])
  const active = licenses.data.filter((item) => item.status === 'active').length
  return <><PageTitle eyebrow="Workspace pulse" title="Your access, at a glance" /><section className="hero-grid"><article className="pulse-card"><div><Radio /><span>LIVE STATUS</span></div><strong>{active}</strong><p>active license{active === 1 ? '' : 's'} in your workspace</p><i /></article><article className="stat-card"><KeyRound /><span>Licenses</span><strong>{licenses.loading ? '—' : licenses.data.length}</strong><Link to="/app/licenses">View licenses <ArrowUpRight /></Link></article><article className="stat-card acid"><MonitorSmartphone /><span>Bound devices</span><strong>{devices.loading ? '—' : devices.data.length}</strong><Link to="/app/devices">Manage devices <ArrowUpRight /></Link></article></section><section className="section"><div className="section-head"><div><p>RECENT ENTITLEMENTS</p><h2>License activity</h2></div><Link to="/app/licenses">View all</Link></div>{licenses.error && <div className="notice error">{licenses.error}</div>}<div className="license-strip">{licenses.data.slice(0, 3).map((license) => <article key={license.id}><div><ShieldCheck /><Status value={license.status} /></div><h3>{license.productName}</h3><code>{license.key}</code><p>{license.expiresAt ? `Expires ${formatDate(license.expiresAt)}` : 'Perpetual access'}</p></article>)}</div></section></>
}

export function LicensesPage() {
  const { data, error, loading } = useResource<License[]>('/licenses', [])
  return <><PageTitle eyebrow="Entitlements" title="Licenses" /><div className="notice">License keys are partially masked. Contact your administrator if you need a key reissued.</div>{error && <div className="notice error">{error}</div>}{!loading && !data.length ? <EmptyState title="No licenses yet" detail="Assigned licenses will appear here." /> : <div className="cards">{data.map((license) => <article className="license-card" key={license.id}><div className="card-top"><span className="product-glyph">{license.productName.slice(0, 2).toUpperCase()}</span><Status value={license.status} /></div><h2>{license.productName}</h2><code>{license.key}</code><dl><div><dt>Expires</dt><dd>{license.expiresAt ? formatDate(license.expiresAt) : 'Perpetual'}</dd></div><div><dt>Devices</dt><dd>{license.deviceCount} / {license.deviceLimit}</dd></div></dl>{license.features?.length ? <div className="tags">{license.features.map((feature) => <span key={feature}>{feature}</span>)}</div> : null}</article>)}</div>}</>
}

export function DevicesPage() {
  const { data, error, loading, reload } = useResource<Device[]>('/devices', []); const [working, setWorking] = useState('')
  const unbind = async (device: Device) => { if (!confirm(`Unbind ${device.name}? The device will lose access immediately.`)) return; setWorking(device.id); try { await api.delete(`/devices/${encodeURIComponent(device.id)}`); await reload() } finally { setWorking('') } }
  return <><PageTitle eyebrow="Trusted hardware" title="Devices" /><p className="page-intro">Review machines currently bound to your licenses. Unbind hardware you no longer recognize or use.</p>{error && <div className="notice error">{error}</div>}{!loading && !data.length ? <EmptyState title="No bound devices" detail="Devices appear after a successful product activation." /> : <div className="table-wrap"><table><thead><tr><th>Device</th><th>Fingerprint</th><th>Activated</th><th>Last seen</th><th /></tr></thead><tbody>{data.map((device) => <tr key={device.id}><td><strong>{device.name}</strong><small>{device.platform || 'Unknown platform'}</small></td><td><code>{device.fingerprint}</code></td><td>{formatDate(device.activatedAt)}</td><td>{formatDate(device.lastSeenAt)}</td><td><button className="danger-link" disabled={working === device.id} onClick={() => void unbind(device)}>{working === device.id ? 'Unbinding...' : 'Unbind'}</button></td></tr>)}</tbody></table></div>}</>
}
