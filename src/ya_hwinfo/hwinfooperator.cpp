#include "hwinfooperator.h"
#if defined(YA_WINDOWS)
#include <sstream>

WmiQuery::WmiQuery() {}

WmiQuery::~WmiQuery() { cleanup(); }

bool WmiQuery::initialize() {
  HRESULT hres;

  hres = CoInitializeEx(0, COINIT_MULTITHREADED);
  if (FAILED(hres)) return false;

  hres =
      CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
                           RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
  if (FAILED(hres)) return false;

  hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, reinterpret_cast<LPVOID*>(&pLoc));
  if (FAILED(hres)) return false;

  hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0,
                             &pSvc);
  if (FAILED(hres)) return false;

  hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                           RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                           NULL, EOAC_NONE);
  if (FAILED(hres)) return false;

  initialized = true;
  return true;
}

std::vector<std::wstring> WmiQuery::query(const std::wstring& wmiClass,
                                          const std::wstring& property) {
  std::vector<std::wstring> results;

  if (!initialized) return results;

  IEnumWbemClassObject* pEnumerator = nullptr;
  HRESULT hres = pSvc->ExecQuery(
      _bstr_t(L"WQL"),
      _bstr_t((L"SELECT " + property + L" FROM " + wmiClass).c_str()),
      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
      &pEnumerator);

  if (FAILED(hres)) return results;

  IWbemClassObject* pclsObj = nullptr;
  ULONG uReturn = 0;

  while (pEnumerator) {
    HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
    if (0 == uReturn) break;

    VARIANT vtProp;
    VariantInit(&vtProp);

    hr = pclsObj->Get(property.c_str(), 0, &vtProp, 0, 0);
    if (SUCCEEDED(hr)) {
      std::wstring value;

      switch (vtProp.vt) {
        case VT_ARRAY | VT_BSTR: {
          LONG lbound, ubound;
          if (FAILED(SafeArrayGetLBound(vtProp.parray, 1, &lbound)) ||
              FAILED(SafeArrayGetUBound(vtProp.parray, 1, &ubound))) {
            break;
          }

          std::wstringstream ss;
          ss << L"{";

          bool first = true;
          for (LONG i = lbound; i <= ubound; ++i) {
            BSTR bstr = nullptr;
            if (FAILED(SafeArrayGetElement(vtProp.parray, &i, &bstr)) ||
                bstr == nullptr)
              continue;

            std::wstring ws(bstr, SysStringLen(bstr));
            SysFreeString(bstr);

            if (!first) {
              ss << L", ";
            }
            ss << L"\"" << ws << L"\"";
            first = false;
          }

          ss << L"}";
          value = ss.str();
        } break;
        case VT_BSTR:
          value = vtProp.bstrVal;
          break;
        case VT_I2:   // short
        case VT_UI2:  // unsigned short
          value = std::to_wstring(vtProp.uiVal);
          break;
        case VT_I4:   // long
        case VT_UI4:  // unsigned long
          value = std::to_wstring(vtProp.ulVal);
          break;
        case VT_BOOL:
          value = (vtProp.boolVal == VARIANT_TRUE) ? L"true" : L"false";
          break;
        default:
          value = L"";
          break;
      }

      results.emplace_back(value);
    }

    VariantClear(&vtProp);
    pclsObj->Release();
  }

  pEnumerator->Release();
  return results;
}

void WmiQuery::cleanup() {
  if (pSvc) {
    pSvc->Release();
    pSvc = nullptr;
  }
  if (pLoc) {
    pLoc->Release();
    pLoc = nullptr;
  }
  CoUninitialize();
  initialized = false;
}
#endif
