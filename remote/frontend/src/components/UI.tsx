import type { ReactNode } from 'react'

export function LoadingScreen() { return <div className="loading-screen"><span className="loader" /><p>Checking your session...</p></div> }
export function EmptyState({ title, detail }: { title: string; detail: string }) { return <div className="empty"><span>Y/O</span><h3>{title}</h3><p>{detail}</p></div> }
export function Status({ value }: { value: string }) { return <span className={`status status-${value.toLowerCase()}`}>{value}</span> }
export function PageTitle({ eyebrow, title, action }: { eyebrow: string; title: string; action?: ReactNode }) {
  return <header className="page-title"><div><p>{eyebrow}</p><h1>{title}</h1></div>{action}</header>
}
export function Field({ label, ...props }: React.InputHTMLAttributes<HTMLInputElement> & { label: string }) {
  return <label className="field"><span>{label}</span><input {...props} /></label>
}
export function formatDate(value?: string | null) {
  if (!value) return 'Never'
  const date = new Date(value)
  return Number.isNaN(date.valueOf()) ? value : new Intl.DateTimeFormat('en', { dateStyle: 'medium' }).format(date)
}
