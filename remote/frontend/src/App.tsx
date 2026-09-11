import { Navigate, Route, Routes } from 'react-router-dom'
import { AdminGuard, UserGuard } from './auth/Guards'
import { Shell } from './components/Shell'
import { ForgotPasswordPage, LoginPage, RegisterPage, ResetPasswordPage, VerifyEmailPage } from './pages/AuthPages'
import { AdminAuditPage, AdminCodesPage, AdminLicensesPage, AdminProductsPage, AdminUsersPage } from './pages/AdminPages'
import { DashboardPage, DevicesPage, LicensesPage } from './pages/UserPages'

export function App() {
  return <Routes>
    <Route path="/" element={<Navigate to="/login" replace />} />
    <Route path="/login" element={<LoginPage />} />
    <Route path="/register" element={<RegisterPage />} />
    <Route path="/verify-email" element={<VerifyEmailPage />} />
    <Route path="/forgot-password" element={<ForgotPasswordPage />} />
    <Route path="/reset-password" element={<ResetPasswordPage />} />
    <Route element={<UserGuard />}><Route element={<Shell />}>
      <Route path="/app" element={<DashboardPage />} />
      <Route path="/app/licenses" element={<LicensesPage />} />
      <Route path="/app/devices" element={<DevicesPage />} />
      <Route element={<AdminGuard />}>
        <Route path="/admin/users" element={<AdminUsersPage />} />
        <Route path="/admin/products" element={<AdminProductsPage />} />
        <Route path="/admin/licenses" element={<AdminLicensesPage />} />
        <Route path="/admin/codes" element={<AdminCodesPage />} />
        <Route path="/admin/audit" element={<AdminAuditPage />} />
      </Route>
    </Route></Route>
    <Route path="*" element={<Navigate to="/" replace />} />
  </Routes>
}
