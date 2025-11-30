#include "Analytics.h"
#include "MacroManager.h"
#include "SystemMonitor.h"
#include "WindowManager.h"
#include <commctrl.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <windows.h>

// Link with comctl32.lib
// #pragma comment(lib, "comctl32.lib")

// Constants for UI controls
#define ID_BUTTON_TILE 1
#define ID_BUTTON_MINIMIZE 2
#define ID_LABEL_COUNT 3
#define ID_CHECKBOX_TOP 4
#define ID_STATUSBAR 5
#define ID_LISTBOX 6
#define ID_BUTTON_SWITCH 7
#define ID_BUTTON_FOCUS 8
#define ID_LABEL_CPU 9
#define ID_LABEL_RAM 10
#define ID_LISTBOX_STATS 11
#define ID_BTN_ADD_MACRO 12
#define ID_BTN_LAUNCH_MACROS 13
#define ID_LISTBOX_MACROS 14
#define ID_TIMER_REFRESH 1001

#define ID_HOTKEY_TILE 2001
#define ID_HOTKEY_FOCUS 2002

// Global variables
WindowManager wm;
SystemMonitor sysMon;
Analytics analytics;
MacroManager macroMgr;
HWND hLabelCount;
HWND hLabelCpu;
HWND hLabelRam;
HWND hStatusBar;
HWND hCheckboxTop;
HWND hListBox;
HWND hListBoxStats;
HWND hListBoxMacros;
HWND hMainWnd;
bool isFocusMode = false;

void UpdateStatus(const char *message) {
  SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)message);
}

void RefreshWindowList() {
  // Save current selection
  int curSel = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
  HWND selectedHwnd = NULL;
  if (curSel != LB_ERR) {
    selectedHwnd = (HWND)SendMessage(hListBox, LB_GETITEMDATA, curSel, 0);
  }

  wm.RefreshWindowList();
  const auto &windows = wm.GetWindows();

  // Update Label
  char buffer[64];
  sprintf(buffer, "Active Windows: %d", (int)windows.size());
  SetWindowText(hLabelCount, buffer);

  // Update ListBox
  int listCount = SendMessage(hListBox, LB_GETCOUNT, 0, 0);
  bool needsFullRefresh = (listCount != static_cast<int>(windows.size()));
  if (!needsFullRefresh && selectedHwnd) {
    bool foundSelected = false;
    for (const auto &win : windows) {
      if (win.hwnd == selectedHwnd) {
        foundSelected = true;
        break;
      }
    }
    if (!foundSelected) {
      needsFullRefresh = true;
    }
  }

  if (needsFullRefresh) {
    SendMessage(hListBox, LB_RESETCONTENT, 0, 0);
    for (const auto &win : windows) {
      int index =
          SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)win.title.c_str());
      SendMessage(hListBox, LB_SETITEMDATA, index, (LPARAM)win.hwnd);

      if (win.hwnd == selectedHwnd) {
        SendMessage(hListBox, LB_SETCURSEL, index, 0);
      }
    }
  }
}

void UpdateSystemStats() {
  double cpu = sysMon.GetCpuUsage();
  int ram = sysMon.GetMemoryUsagePercentage();

  char cpuBuf[32];
  sprintf(cpuBuf, "CPU: %.1f%%", cpu);
  SetWindowText(hLabelCpu, cpuBuf);

  char ramBuf[32];
  sprintf(ramBuf, "RAM: %d%%", ram);
  SetWindowText(hLabelRam, ramBuf);
}

void UpdateAnalytics() {
  analytics.Update();

  SendMessage(hListBoxStats, LB_RESETCONTENT, 0, 0);
  auto topApps = analytics.GetTopApps(5);
  for (const auto &app : topApps) {
    char buf[128];
    sprintf(buf, "%s: %lds", app.name.c_str(), app.seconds);
    SendMessage(hListBoxStats, LB_ADDSTRING, 0, (LPARAM)buf);
  }
}

void UpdateMacroList() {
  SendMessage(hListBoxMacros, LB_RESETCONTENT, 0, 0);
  const auto &programs = macroMgr.GetPrograms();
  for (const auto &path : programs) {
    // Extract filename from path for display
    std::string filename = path;
    size_t lastSlash = path.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
      filename = path.substr(lastSlash + 1);
    }
    SendMessage(hListBoxMacros, LB_ADDSTRING, 0, (LPARAM)filename.c_str());
  }
}

void AddMacro() {
  char filename[MAX_PATH] = "";
  OPENFILENAME ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hMainWnd;
  ofn.lpstrFilter = "Executables (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

  if (GetOpenFileName(&ofn)) {
    macroMgr.AddProgram(filename);
    UpdateMacroList();
    UpdateStatus("Program added to workspace.");
  }
}

void SwitchToSelectedWindow() {
  int curSel = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
  if (curSel == LB_ERR)
    return;

  HWND hwnd = (HWND)SendMessage(hListBox, LB_GETITEMDATA, curSel, 0);
  wm.SwitchToWindow(hwnd);
  UpdateStatus("Switched to window.");
}

void FocusMode() {
  if (isFocusMode) {
    wm.RestoreAll();
    isFocusMode = false;
    SetWindowText(GetDlgItem(hMainWnd, ID_BUTTON_FOCUS), "Focus Mode");
    UpdateStatus("Windows Restored.");
    return;
  }

  int curSel = SendMessage(hListBox, LB_GETCURSEL, 0, 0);
  if (curSel == LB_ERR) {
    UpdateStatus("Select a window to focus on.");
    return;
  }

  HWND hwnd = (HWND)SendMessage(hListBox, LB_GETITEMDATA, curSel, 0);
  wm.MinimizeAllBut(hwnd);
  isFocusMode = true;
  SetWindowText(GetDlgItem(hMainWnd, ID_BUTTON_FOCUS), "Restore All");
  UpdateStatus("Focus Mode Activated.");
}

// --- GUI Logic ---

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  switch (uMsg) {
  case WM_CREATE:
    hMainWnd = hwnd;
    sysMon.Initialize();

    // Register Global Hotkeys
    RegisterHotKey(hwnd, ID_HOTKEY_TILE, MOD_ALT | MOD_SHIFT, 'T');
    RegisterHotKey(hwnd, ID_HOTKEY_FOCUS, MOD_ALT | MOD_SHIFT, 'F');

    // --- Stats Panel (Top) ---
    CreateWindow("STATIC", "System Stats", WS_VISIBLE | WS_CHILD | SS_CENTER,
                 10, 10, 220, 20, hwnd, NULL, NULL, NULL);

    hLabelCpu =
        CreateWindow("STATIC", "CPU: -", WS_VISIBLE | WS_CHILD | SS_LEFT, 20,
                     35, 100, 20, hwnd, (HMENU)ID_LABEL_CPU, NULL, NULL);

    hLabelRam =
        CreateWindow("STATIC", "RAM: -", WS_VISIBLE | WS_CHILD | SS_LEFT, 130,
                     35, 100, 20, hwnd, (HMENU)ID_LABEL_RAM, NULL, NULL);

    // --- Controls ---
    CreateWindow("BUTTON", "Tile Grid",
                 WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 70,
                 105, 30, hwnd, (HMENU)ID_BUTTON_TILE,
                 (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    CreateWindow("BUTTON", "Focus Mode",
                 WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 125, 70,
                 105, 30, hwnd, (HMENU)ID_BUTTON_FOCUS,
                 (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    // Label Count
    hLabelCount = CreateWindow(
        "STATIC", "Active Windows: 0", WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE,
        10, 110, 220, 25, hwnd, (HMENU)ID_LABEL_COUNT,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    // Checkbox
    hCheckboxTop = CreateWindow(
        "BUTTON", "Always on Top", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 10,
        140, 180, 25, hwnd, (HMENU)ID_CHECKBOX_TOP,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    // ListBox (Windows)
    hListBox =
        CreateWindow("LISTBOX", NULL,
                     WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_BORDER |
                         LBS_NOTIFY | LBS_HASSTRINGS,
                     10, 170, 220, 150, hwnd, (HMENU)ID_LISTBOX,
                     (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    // Analytics Label
    CreateWindow("STATIC", "Top Apps (Time)", WS_VISIBLE | WS_CHILD | SS_CENTER,
                 10, 330, 220, 20, hwnd, NULL, NULL, NULL);

    // ListBox (Stats)
    hListBoxStats =
        CreateWindow("LISTBOX", NULL,
                     WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_BORDER |
                         LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
                     10, 350, 220, 80, hwnd, (HMENU)ID_LISTBOX_STATS,
                     (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    // Switch Button
    CreateWindow("BUTTON", "Switch To",
                 WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 10, 440,
                 220, 30, hwnd, (HMENU)ID_BUTTON_SWITCH,
                 (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    // --- Workspace Launcher ---
    CreateWindow("STATIC", "Workspace Launcher",
                 WS_VISIBLE | WS_CHILD | SS_CENTER, 10, 480, 220, 20, hwnd,
                 NULL, NULL, NULL);

    hListBoxMacros =
        CreateWindow("LISTBOX", NULL,
                     WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_BORDER |
                         LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
                     10, 500, 220, 60, hwnd, (HMENU)ID_LISTBOX_MACROS,
                     (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    CreateWindow("BUTTON", "Add Program",
                 WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 10, 570,
                 105, 30, hwnd, (HMENU)ID_BTN_ADD_MACRO,
                 (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    CreateWindow("BUTTON", "Launch All",
                 WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 125, 570,
                 105, 30, hwnd, (HMENU)ID_BTN_LAUNCH_MACROS,
                 (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    // Initial load of macros
    UpdateMacroList();

    // Status Bar
    hStatusBar = CreateWindowEx(
        0, STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0,
        0, 0, hwnd, (HMENU)ID_STATUSBAR,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    // Timer for auto-refresh (every 1 second)
    SetTimer(hwnd, ID_TIMER_REFRESH, 1000, NULL);
    break;

  case WM_TIMER:
    if (wParam == ID_TIMER_REFRESH) {
      RefreshWindowList();
      UpdateSystemStats();
      UpdateAnalytics();
    }
    break;

  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case ID_BUTTON_TILE:
      wm.TileWindows(hMainWnd);
      UpdateStatus("Windows Tiled.");
      break;
    case ID_BUTTON_SWITCH:
      SwitchToSelectedWindow();
      break;
    case ID_BUTTON_FOCUS:
      FocusMode();
      break;
    case ID_BTN_ADD_MACRO:
      AddMacro();
      break;
    case ID_BTN_LAUNCH_MACROS:
      macroMgr.ExecuteAll();
      UpdateStatus("Launching workspace...");
      break;
    case ID_CHECKBOX_TOP:
      if (IsDlgButtonChecked(hwnd, ID_CHECKBOX_TOP)) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      } else {
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      }
      break;
    case ID_LISTBOX:
      if (HIWORD(wParam) == LBN_DBLCLK) {
        SwitchToSelectedWindow();
      }
      break;
    }
    break;

  case WM_HOTKEY:
    if (wParam == ID_HOTKEY_TILE) {
      wm.TileWindows(hMainWnd);
      UpdateStatus("HotKey: Windows Tiled.");
    } else if (wParam == ID_HOTKEY_FOCUS) {
      FocusMode();
      UpdateStatus(isFocusMode ? "HotKey: Focus Mode ON"
                               : "HotKey: Focus Mode OFF");
    }
    break;

  case WM_DESTROY:
    UnregisterHotKey(hwnd, ID_HOTKEY_TILE);
    UnregisterHotKey(hwnd, ID_HOTKEY_FOCUS);
    KillTimer(hwnd, ID_TIMER_REFRESH);
    PostQuitMessage(0);
    return 0;

  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
    EndPaint(hwnd, &ps);
  }
    return 0;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/,
                   LPSTR /*lpCmdLine*/, int nCmdShow) {
  const char CLASS_NAME[] = "WindowDashClass";

  // Initialize Common Controls (for Status Bar)
  INITCOMMONCONTROLSEX icex;
  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC = ICC_BAR_CLASSES;
  InitCommonControlsEx(&icex);

  WNDCLASS wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);

  RegisterClass(&wc);

  HWND hwnd = CreateWindowEx(
      0,             // Optional window styles.
      CLASS_NAME,    // Window class
      "Window Dash", // Window text
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, // Window style
      CW_USEDEFAULT, CW_USEDEFAULT, 260,
      650,       // Size and position (Increased height for macros)
      NULL,      // Parent window
      NULL,      // Menu
      hInstance, // Instance handle
      NULL       // Additional application data
  );

  if (hwnd == NULL) {
    return 0;
  }

  ShowWindow(hwnd, nCmdShow);

  MSG msg = {};
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return 0;
}
