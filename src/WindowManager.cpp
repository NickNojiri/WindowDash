#include "WindowManager.h"
#include "core/TileLayout.h"
#include "core/WindowFilter.h"

#include <cstdio>
#include <dwmapi.h>
#include <string>

// DWMWA_CLOAKED (Windows 8+). Spelled out because the headers only define it
// for _WIN32_WINNT >= 0x0602 and this app still targets Vista.
static const DWORD kDwmwaCloaked = 14;

WindowManager::WindowManager() { RefreshWindowList(); }

WindowManager::~WindowManager() {}

const std::vector<WindowInfo> &WindowManager::GetWindows() const {
  return m_visibleWindows;
}

void WindowManager::RefreshWindowList() {
  m_visibleWindows.clear();
  EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(this));
}

// Suspended UWP apps and virtual-desktop windows are "visible" but cloaked:
// on screen they don't exist, so tiling them leaves empty holes.
static bool IsCloaked(HWND hwnd) {
  DWORD cloaked = 0;
  return SUCCEEDED(DwmGetWindowAttribute(hwnd, kDwmwaCloaked, &cloaked,
                                         sizeof(cloaked))) &&
         cloaked != 0;
}

BOOL CALLBACK WindowManager::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
  WindowManager *pThis = reinterpret_cast<WindowManager *>(lParam);

  // Minimized windows are kept (not filtered with IsIconic) so Focus Mode
  // can restore them.
  if (!IsWindowVisible(hwnd) || IsCloaked(hwnd))
    return TRUE;

  int length = GetWindowTextLength(hwnd);
  if (length == 0)
    return TRUE;

  std::vector<char> buffer(length + 1);
  GetWindowText(hwnd, &buffer[0], length + 1);
  std::string title(&buffer[0]);

  if (core::IsExcludedTitle(title))
    return TRUE;

  pThis->m_visibleWindows.push_back({hwnd, title});
  return TRUE;
}

void WindowManager::TileWindows(HWND owner) {
  RefreshWindowList();
  if (m_visibleWindows.empty())
    return;

  RECT workArea;
  MONITORINFO mi = {};
  mi.cbSize = sizeof(MONITORINFO);
  if (owner &&
      GetMonitorInfo(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &mi)) {
    workArea = mi.rcWork;
  } else {
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
  }

  const core::Rect area{workArea.left, workArea.top,
                        workArea.right - workArea.left,
                        workArea.bottom - workArea.top};
  const int count = static_cast<int>(m_visibleWindows.size());
  const core::TilePlan plan = core::PlanTiles(count, area);

  char debugBuf[256];
  snprintf(debugBuf, sizeof(debugBuf),
           "[WindowDash] Tiling %d windows. Screen: %d,%d %dx%d\n", count,
           area.x, area.y, area.w, area.h);
  OutputDebugString(debugBuf);

  for (int i = 0; i < count; ++i) {
    HWND hwnd = m_visibleWindows[i].hwnd;
    // Restore first: moving a maximized or minimized window and restoring it
    // afterwards snaps it back to its old size, undoing the tile.
    ShowWindow(hwnd, SW_RESTORE);
    if (plan.maximizeSingle) {
      ShowWindow(hwnd, SW_MAXIMIZE);
    } else {
      const core::Rect &r = plan.rects[i];
      MoveWindow(hwnd, r.x, r.y, r.w, r.h, TRUE);
    }
  }
}

void WindowManager::MinimizeAllBut(HWND targetHwnd) {
  RefreshWindowList();
  for (const auto &win : m_visibleWindows) {
    if (win.hwnd != targetHwnd) {
      ShowWindow(win.hwnd, SW_MINIMIZE);
    } else {
      ShowWindow(win.hwnd, SW_RESTORE);
      SetForegroundWindow(win.hwnd);
    }
  }
}

void WindowManager::RestoreAll() {
  RefreshWindowList();
  for (const auto &win : m_visibleWindows) {
    ShowWindow(win.hwnd, SW_RESTORE);
  }
}

void WindowManager::SwitchToWindow(HWND hwnd) {
  if (IsWindow(hwnd)) {
    if (IsIconic(hwnd)) {
      ShowWindow(hwnd, SW_RESTORE);
    }
    SetForegroundWindow(hwnd);
  }
}
