#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <cstring>
#include <string>
#include <vector>
#include <windows.h>

struct WindowInfo {
  HWND hwnd;
  std::string title;
};

class WindowManager {
public:
  WindowManager();
  ~WindowManager();

  void RefreshWindowList();
  const std::vector<WindowInfo> &GetWindows() const;

  void TileWindows(HWND owner = NULL);
  void MinimizeAllBut(HWND targetHwnd);
  void RestoreAll();
  void SwitchToWindow(HWND hwnd);

private:
  std::vector<WindowInfo> m_visibleWindows;
  static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
};
