#include "WindowManager.h"
#include <algorithm>
#include <cmath>
#include <iostream>

WindowManager::WindowManager() { RefreshWindowList(); }

WindowManager::~WindowManager() {}

const std::vector<WindowInfo> &WindowManager::GetWindows() const {
  return m_visibleWindows;
}

void WindowManager::RefreshWindowList() {
  m_visibleWindows.clear();
  EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(this));
}

BOOL CALLBACK WindowManager::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
  WindowManager *pThis = reinterpret_cast<WindowManager *>(lParam);

  if (!IsWindowVisible(hwnd))
    return TRUE;

  // Removed IsIconic check so we can track and restore minimized windows

  int length = GetWindowTextLength(hwnd);
  if (length == 0)
    return TRUE;

  std::vector<char> buffer(length + 1);
  GetWindowText(hwnd, &buffer[0], length + 1);
  std::string title(&buffer[0]);

  if (title == "Program Manager" || title == "Settings" ||
      title == "Microsoft Text Input Application" ||
      title == "Window Manager" || title == "Window Dash")
    return TRUE;

  WindowInfo info;
  info.hwnd = hwnd;
  info.title = title;
  pThis->m_visibleWindows.push_back(info);

  return TRUE;
}

void WindowManager::TileWindows(HWND owner) {
  RefreshWindowList();
  if (m_visibleWindows.empty())
    return;

  RECT workArea;
  if (owner) {
    HMONITOR hMon = MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(MONITORINFO);
    if (GetMonitorInfo(hMon, &mi)) {
      workArea = mi.rcWork;
    } else {
      SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    }
  } else {
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
  }

  int screenX = workArea.left;
  int screenY = workArea.top;
  int screenW = workArea.right - workArea.left;
  int screenH = workArea.bottom - workArea.top;

  int count = static_cast<int>(m_visibleWindows.size());
  int cols = static_cast<int>(std::ceil(std::sqrt(count)));
  int rows = static_cast<int>(std::ceil((double)count / cols));

  if (count == 3) {
    cols = 2;
    rows = 2;
  }

  int winW = screenW / cols;
  int winH = screenH / rows;

  for (int i = 0; i < count; ++i) {
    int row = i / cols;
    int col = i % cols;
    int x = screenX + (col * winW);
    int y = screenY + (row * winH);

    if (count == 3) {
      if (i == 0) {
        MoveWindow(m_visibleWindows[i].hwnd, screenX, screenY, screenW / 2,
                   screenH, TRUE);
      } else if (i == 1) {
        MoveWindow(m_visibleWindows[i].hwnd, screenX + (screenW / 2), screenY,
                   screenW / 2, screenH / 2, TRUE);
      } else {
        MoveWindow(m_visibleWindows[i].hwnd, screenX + (screenW / 2),
                   screenY + (screenH / 2), screenW / 2, screenH / 2, TRUE);
      }
    } else if (count == 2) {
      if (i == 0)
        MoveWindow(m_visibleWindows[i].hwnd, screenX, screenY, screenW / 2,
                   screenH, TRUE);
      else
        MoveWindow(m_visibleWindows[i].hwnd, screenX + (screenW / 2), screenY,
                   screenW / 2, screenH, TRUE);
    } else if (count == 1) {
      ShowWindow(m_visibleWindows[i].hwnd, SW_MAXIMIZE);
    } else {
      MoveWindow(m_visibleWindows[i].hwnd, x, y, winW, winH, TRUE);
    }
    ShowWindow(m_visibleWindows[i].hwnd, SW_RESTORE);
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
