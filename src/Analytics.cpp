#include "Analytics.h"
#include "core/WindowFilter.h"

#include <fstream>
#include <windows.h>

Analytics::Analytics() { Load("analytics.dat"); }

Analytics::~Analytics() { Save("analytics.dat"); }

void Analytics::Save(const std::string &filename) {
  std::ofstream outfile(filename);
  if (outfile.is_open())
    m_stats.Save(outfile);
}

void Analytics::Load(const std::string &filename) {
  std::ifstream infile(filename);
  if (infile.is_open())
    m_stats.Load(infile);
}

void Analytics::Update() {
  m_stats.Tick(GetActiveWindowProcessName(), GetTickCount64());
}

std::vector<AppUsage> Analytics::GetTopApps(int limit) {
  return m_stats.Top(limit < 0 ? 0 : static_cast<std::size_t>(limit));
}

std::string Analytics::GetActiveWindowProcessName() {
  HWND hwnd = GetForegroundWindow();
  if (!hwnd)
    return "";

  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);

  // LIMITED_INFORMATION also works for elevated and protected processes,
  // which PROCESS_QUERY_INFORMATION | PROCESS_VM_READ is refused for.
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!hProcess)
    return "";

  char buffer[MAX_PATH];
  DWORD size = MAX_PATH;
  std::string name;
  if (QueryFullProcessImageNameA(hProcess, 0, buffer, &size))
    name = core::DisplayName(std::string(buffer, size));
  CloseHandle(hProcess);
  return name;
}
