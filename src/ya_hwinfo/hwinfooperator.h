#include "platform_def.h"

namespace ya {

#if defined(YA_WINDOWS)
#include <Wbemidl.h>
#include <Windows.h>
#include <comdef.h>

#include <string>
#include <vector>

#pragma comment(lib, "wbemuuid.lib")

class WmiQuery {
 public:
  WmiQuery();
  ~WmiQuery();

  bool initialize();
  std::vector<std::wstring> query(const std::wstring& wmiClass,
                                  const std::wstring& property);

 private:
  IWbemServices* pSvc = nullptr;
  IWbemLocator* pLoc = nullptr;
  bool initialized = false;

  void cleanup();
};
#endif

}  // namespace ya
