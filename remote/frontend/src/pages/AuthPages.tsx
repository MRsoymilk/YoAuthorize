import { useState, type FormEvent, type ReactNode } from 'react'
import { Link, Navigate, useLocation, useNavigate, useSearchParams } from 'react-router-dom'
import { ArrowRight, Check, KeyRound } from 'lucide-react'
import { useAuth } from '../auth/AuthProvider'
import { Field } from '../components/UI'
import { api, errorMessage } from '../lib/api'

function AuthFrame({ eyebrow, title, text, children }: { eyebrow: string; title: string; text: string; children: ReactNode }) {
  return <main className="auth-page"><section className="auth-art"><Link className="auth-brand" to="/"><span>Y</span>YoAuthorize</Link><div><p>REMOTE LICENSING / 2026</p><h2>Control access.<br /><em>Keep momentum.</em></h2><div className="signal"><span /><span /><span /><span /></div></div><small>Encrypted sessions · Auditable actions · Precise controls</small></section><section className="auth-panel"><div className="auth-card"><p className="eyebrow">{eyebrow}</p><h1>{title}</h1><p className="lede">{text}</p>{children}</div></section></main>
}

function FormError({ message }: { message: string }) { return message ? <div className="form-error" role="alert">{message}</div> : null }

export function LoginPage() {
  const auth = useAuth(); const navigate = useNavigate(); const location = useLocation()
  const [error, setError] = useState(''); const [busy, setBusy] = useState(false)
  if (auth.status === 'authenticated') return <Navigate to="/app" replace />
  const submit = async (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault(); setBusy(true); setError('')
    const data = new FormData(event.currentTarget)
    try { await api.post('/auth/login', { email: data.get('email'), password: data.get('password') }); await auth.refresh(); navigate((location.state as { from?: { pathname?: string } } | null)?.from?.pathname || '/app', { replace: true }) }
    catch (e) { setError(errorMessage(e)) } finally { setBusy(false) }
  }
  return <AuthFrame eyebrow="Welcome back" title="Sign in to Remote" text="Manage licenses, activations, and trusted devices from one secure workspace."><form onSubmit={submit}><Field name="email" label="Email address" type="email" autoComplete="email" required /><Field name="password" label="Password" type="password" autoComplete="current-password" required /><div className="form-row"><label><input type="checkbox" name="remember" /> Keep me signed in</label><Link to="/forgot-password">Forgot password?</Link></div><FormError message={error} /><button className="primary" disabled={busy}>{busy ? 'Signing in...' : <>Sign in <ArrowRight /></>}</button></form><p className="auth-switch">New to YoAuthorize? <Link to="/register">Create an account</Link></p></AuthFrame>
}

export function RegisterPage() {
  const [sent, setSent] = useState(''); const [error, setError] = useState(''); const [busy, setBusy] = useState(false)
  const submit = async (event: FormEvent<HTMLFormElement>) => { event.preventDefault(); setBusy(true); setError(''); const data = new FormData(event.currentTarget); try { await api.post('/auth/register', { name: data.get('name'), email: data.get('email'), password: data.get('password') }); setSent(String(data.get('email'))) } catch (e) { setError(errorMessage(e)) } finally { setBusy(false) } }
  if (sent) return <AuthFrame eyebrow="One last step" title="Check your inbox" text={`We sent a verification link to ${sent}. It may take a minute to arrive.`}><div className="success-icon"><Check /></div><Link className="primary" to="/login">Return to sign in</Link></AuthFrame>
  return <AuthFrame eyebrow="Create account" title="Start with Remote" text="Register your workspace identity. Your email must be verified before you can sign in."><form onSubmit={submit}><Field name="name" label="Full name" autoComplete="name" required /><Field name="email" label="Work email" type="email" autoComplete="email" required /><Field name="password" label="Password" type="password" minLength={10} autoComplete="new-password" required /><small className="hint">Use at least 10 characters.</small><FormError message={error} /><button className="primary" disabled={busy}>{busy ? 'Creating account...' : <>Create account <ArrowRight /></>}</button></form><p className="auth-switch">Already registered? <Link to="/login">Sign in</Link></p></AuthFrame>
}

export function VerifyEmailPage() {
  const [params] = useSearchParams(); const [state, setState] = useState<'ready' | 'busy' | 'done' | 'error'>(params.get('token') ? 'ready' : 'error'); const [message, setMessage] = useState(params.get('token') ? '' : 'This verification link is incomplete.')
  const verify = async () => { setState('busy'); try { await api.post('/auth/verify-email', { token: params.get('token') }); setState('done') } catch (e) { setMessage(errorMessage(e)); setState('error') } }
  return <AuthFrame eyebrow="Email verification" title={state === 'done' ? 'Email verified' : 'Confirm your email'} text={state === 'done' ? 'Your account is ready. You can now sign in.' : 'Confirm this address to secure your YoAuthorize account.'}>{state === 'done' ? <><div className="success-icon"><Check /></div><Link className="primary" to="/login">Continue to sign in</Link></> : <><div className="success-icon"><KeyRound /></div><FormError message={message} /><button className="primary" onClick={() => void verify()} disabled={state !== 'ready'}>{state === 'busy' ? 'Verifying...' : 'Verify email'}</button></>}</AuthFrame>
}

export function ForgotPasswordPage() {
  const [sent, setSent] = useState(false); const [error, setError] = useState('')
  const submit = async (event: FormEvent<HTMLFormElement>) => { event.preventDefault(); const data = new FormData(event.currentTarget); try { await api.post('/auth/forgot-password', { email: data.get('email') }); setSent(true) } catch (e) { setError(errorMessage(e)) } }
  return <AuthFrame eyebrow="Account recovery" title="Reset your password" text={sent ? 'If that account exists, a reset link is on its way. Check your inbox.' : 'Enter your account email and we will send you a secure reset link.'}>{sent ? <><div className="success-icon"><Check /></div><Link className="primary" to="/login">Return to sign in</Link></> : <form onSubmit={submit}><Field name="email" label="Email address" type="email" required autoComplete="email" /><FormError message={error} /><button className="primary">Send reset link <ArrowRight /></button></form>}</AuthFrame>
}

export function ResetPasswordPage() {
  const [params] = useSearchParams(); const navigate = useNavigate(); const [error, setError] = useState(''); const [busy, setBusy] = useState(false)
  const submit = async (event: FormEvent<HTMLFormElement>) => { event.preventDefault(); const data = new FormData(event.currentTarget); if (data.get('password') !== data.get('confirm')) return setError('Passwords do not match.'); setBusy(true); try { await api.post('/auth/reset-password', { token: params.get('token'), password: data.get('password') }); navigate('/login?reset=success', { replace: true }) } catch (e) { setError(errorMessage(e)); setBusy(false) } }
  return <AuthFrame eyebrow="Account recovery" title="Choose a new password" text="Set a unique password with at least 10 characters."><form onSubmit={submit}><Field name="password" label="New password" type="password" minLength={10} required autoComplete="new-password" /><Field name="confirm" label="Confirm password" type="password" minLength={10} required autoComplete="new-password" /><FormError message={error || (!params.get('token') ? 'This reset link is incomplete.' : '')} /><button className="primary" disabled={busy || !params.get('token')}>{busy ? 'Updating...' : 'Update password'}</button></form></AuthFrame>
}
