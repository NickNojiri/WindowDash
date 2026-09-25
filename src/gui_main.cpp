#include "Analytics.h"
#include "SystemMonitor.h"
#include "WindowManager.h"
#include "bookmarks/BookmarkManager.h"
#include "core/WindowFilter.h"
#include <commctrl.h>
#include <cstdint>
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
#define ID_BTN_ADD_BOOKMARK 12
#define ID_BTN_LAUNCH_BOOKMARKS 13
#define ID_LISTBOX_BOOKMARKS 14
#define ID_BTN_TOGGLE_STATS 15
#define ID_BTN_TOGGLE_ANALYTICS 16
#define ID_TIMER_REFRESH 1001
#define ID_TIMER_WINLIST 1002

// A burst of window events (an app opening several windows, a title
// ticking) is folded into one list refresh at most this long after the first
// event. The timer is not re-armed by later events, so a window whose title
// changes nonstop can't postpone the refresh indefinitely.
#define WINLIST_THROTTLE_MS 100
// The window list is event-driven; this is only a safety net in case an
// event is missed (see docs/ARCHITECTURE.md).
#define WINLIST_RESYNC_TICKS 10

#define ID_HOTKEY_TILE 2001
#define ID_HOTKEY_FOCUS 2002

// Global variables
WindowManager wm;
SystemMonitor sysMon;
Analytics analytics;
BookmarkManager bookmarkMgr;
HWND hLabelCount;
HWND hLabelCpu;
HWND hLabelRam;
HWND hStatusBar;
HWND hCheckboxTop;
HWND hListBox;
HWND hListBoxStats;
HWND hListBoxBookmarks;
HWND hMainWnd;
HWND hBtnToggleStats;
HWND hBtnToggleAnalytics;
HWND hLabelStatsTitle;
HWND hLabelAnalyticsTitle;
HWND hLabelBookmarksTitle;
HWND hBtnAddBookmark;
HWND hBtnLaunchBookmarks;
HWND hBtnTile;
HWND hBtnFocus;
HWND hBtnSwitch;

std::vector<HWINEVENTHOOK> winEventHooks;
int ticksSinceResync = 0;
bool winListRefreshPending = false;

bool isFocusMode = false;
bool isStatsExpanded = true;
bool isAnalyticsExpanded = true;

void UpdateStatus(const char *message) {
  SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)message);
}

void UpdateLayout() {
  int y = 10;

  // --- System Stats ---
  SetWindowPos(hLabelStatsTitle, NULL, 30, y, 200, 20, SWP_NOZORDER);
  SetWindowPos(hBtnToggleStats, NULL, 10, y, 20, 20, SWP_NOZORDER);
  SetWindowText(hBtnToggleStats, isStatsExpanded ? "v" : ">");
  y += 25;

  if (isStatsExpanded) {
    ShowWindow(hLabelCpu, SW_SHOW);
    ShowWindow(hLabelRam, SW_SHOW);
    SetWindowPos(hLabelCpu, NULL, 20, y, 100, 20, SWP_NOZORDER);
    SetWindowPos(hLabelRam, NULL, 130, y, 100, 20, SWP_NOZORDER);
    y += 25;
  } else {
    ShowWindow(hLabelCpu, SW_HIDE);
    ShowWindow(hLabelRam, SW_HIDE);
  }

  // --- Controls ---
  y += 10;
  SetWindowPos(hBtnTile, NULL, 10, y, 105, 30, SWP_NOZORDER);
  SetWindowPos(hBtnFocus, NULL, 125, y, 105, 30, SWP_NOZORDER);
  y += 40;

  SetWindowPos(hLabelCount, NULL, 10, y, 220, 25, SWP_NOZORDER);
  y += 30;

  SetWindowPos(hCheckboxTop, NULL, 10, y, 180, 25, SWP_NOZORDER);
  y += 30;

  // --- Window List ---
  SetWindowPos(hListBox, NULL, 10, y, 220, 150, SWP_NOZORDER);
  y += 160;

  SetWindowPos(hBtnSwitch, NULL, 10, y, 220, 30, SWP_NOZORDER);
  y += 40;

  // --- Analytics ---
  SetWindowPos(hLabelAnalyticsTitle, NULL, 30, y, 200, 20, SWP_NOZORDER);
  SetWindowPos(hBtnToggleAnalytics, NULL, 10, y, 20, 20, SWP_NOZORDER);
  SetWindowText(hBtnToggleAnalytics, isAnalyticsExpanded ? "v" : ">");
  y += 25;

  if (isAnalyticsExpanded) {
    ShowWindow(hListBoxStats, SW_SHOW);
    SetWindowPos(hListBoxStats, NULL, 10, y, 220, 80, SWP_NOZORDER);
    y += 90;
  } else {
    ShowWindow(hListBoxStats, SW_HIDE);
  }

  // --- Bookmarks ---
  SetWindowPos(hLabelBookmarksTitle, NULL, 10, y, 220, 20, SWP_NOZORDER);
  y += 20;

  SetWindowPos(hListBoxBookmarks, NULL, 10, y, 220, 60, SWP_NOZORDER);
  y += 70;

  SetWindowPos(hBtnAddBookmark, NULL, 10, y, 105, 30, SWP_NOZORDER);
  SetWindowPos(hBtnLaunchBookmarks, NULL, 125, y, 105, 30, SWP_NOZORDER);
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
  snprintf(buffer, sizeof(buffer), "Active Windows: %d", (int)windows.size());
  SetWindowText(hLabelCount, buffer);

  // Rebuild the list box only when a window or a title changed, so the
  // selection and scroll position survive unrelated events.
  static std::vector<core::ListedWindow> shown;
  std::vector<core::ListedWindow> now;
  for (const auto &win : windows)
    now.push_back({reinterpret_cast<std::uintptr_t>(win.hwnd), win.title});
  if (now == shown)
    return;
  shown = now;

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

void UpdateSystemStats() {
  if (!isStatsExpanded)
    return;

  double cpu = sysMon.GetCpuUsage();
  int ram = sysMon.GetMemoryUsagePercentage();

  char cpuBuf[32];
  snprintf(cpuBuf, sizeof(cpuBuf), "CPU: %.1f%%", cpu);
  SetWindowText(hLabelCpu, cpuBuf);

  char ramBuf[32];
  snprintf(ramBuf, sizeof(ramBuf), "RAM: %d%%", ram);
  SetWindowText(hLabelRam, ramBuf);
}

void UpdateAnalytics() {
  // Count usage whether or not the panel is open; only the drawing is skipped.
  analytics.Update();
  if (!isAnalyticsExpanded)
    return;

  SendMessage(hListBoxStats, LB_RESETCONTENT, 0, 0);
  auto topApps = analytics.GetTopApps(5);
  for (const auto &app : topApps) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s: %llds", app.name.c_str(), app.seconds);
    SendMessage(hListBoxStats, LB_ADDSTRING, 0, (LPARAM)buf);
  }
}

void UpdateBookmarkList() {
  SendMessage(hListBoxBookmarks, LB_RESETCONTENT, 0, 0);
  const auto &programs = bookmarkMgr.GetBookmarks();
  for (const auto &path : programs) {
    std::string filename = core::DisplayName(path);
    SendMessage(hListBoxBookmarks, LB_ADDSTRING, 0, (LPARAM)filename.c_str());
  }
}

void AddBookmark() {
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
    bookmarkMgr.AddBookmark(filename);
    UpdateBookmarkList();
    UpdateStatus("Bookmark added.");
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

// --- Window events ---

// Called on the UI thread (WINEVENT_OUTOFCONTEXT delivers through this
// thread's message loop), so no locking is needed. It only arms a short
// timer (if one isn't already pending); the refresh runs from WM_TIMER.
void CALLBACK OnWinEvent(HWINEVENTHOOK, DWORD, HWND hwnd, LONG idObject,
                         LONG idChild, DWORD, DWORD) {
  if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF || !hwnd)
    return;
  if (GetAncestor(hwnd, GA_ROOT) != hwnd)
    return;  // a child control, not a top-level window
  if (!winListRefreshPending) {
    winListRefreshPending = true;
    SetTimer(hMainWnd, ID_TIMER_WINLIST, WINLIST_THROTTLE_MS, NULL);
  }
}

// EVENT_OBJECT_CLOAKED / UNCLOAKED (Windows 8+), spelled out because the
// headers only define them for _WIN32_WINNT >= 0x0602. Older Windows never
// sends them, so hooking them there is harmless.
static const DWORD kEventObjectCloaked = 0x8017;
static const DWORD kEventObjectUncloaked = 0x8018;

void HookWindowEvents() {
  // Separate ranges on purpose: 0x8000-0x800C would include
  // EVENT_OBJECT_LOCATIONCHANGE, which fires on every mouse-driven move.
  const DWORD ranges[][2] = {
      {EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE},        // open, close, show, hide
      {EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_NAMECHANGE},  // title changed
      {EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND},
      {kEventObjectCloaked, kEventObjectUncloaked},  // virtual desktops, UWP
  };
  for (const auto &r : ranges) {
    HWINEVENTHOOK h =
        SetWinEventHook(r[0], r[1], NULL, OnWinEvent, 0, 0,
                        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (h)
      winEventHooks.push_back(h);
  }
}

void UnhookWindowEvents() {
  for (HWINEVENTHOOK h : winEventHooks)
    UnhookWinEvent(h);
  winEventHooks.clear();
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

    HookWindowEvents();

    // --- Stats Panel (Top) ---
    hBtnToggleStats =
        CreateWindow("BUTTON", "v", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 10,
                     10, 20, 20, hwnd, (HMENU)ID_BTN_TOGGLE_STATS,
                     (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    hLabelStatsTitle =
        CreateWindow("STATIC", "System Stats", WS_VISIBLE | WS_CHILD | SS_LEFT,
                     30, 10, 200, 20, hwnd, NULL, NULL, NULL);

    hLabelCpu =
        CreateWindow("STATIC", "CPU: -", WS_VISIBLE | WS_CHILD | SS_LEFT, 20,
                     35, 100, 20, hwnd, (HMENU)ID_LABEL_CPU, NULL, NULL);

    hLabelRam =
        CreateWindow("STATIC", "RAM: -", WS_VISIBLE | WS_CHILD | SS_LEFT, 130,
                     35, 100, 20, hwnd, (HMENU)ID_LABEL_RAM, NULL, NULL);

    // --- Controls ---
    hBtnTile =
        CreateWindow("BUTTON", "Tile Grid",
                     WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10,
                     70, 105, 30, hwnd, (HMENU)ID_BUTTON_TILE,
                     (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    hBtnFocus =
        CreateWindow("BUTTON", "Focus Mode",
                     WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 125,
                     70, 105, 30, hwnd, (HMENU)ID_BUTTON_FOCUS,
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

    // Switch Button
    hBtnSwitch =
        CreateWindow("BUTTON", "Switch To",
                     WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 10,
                     440, 220, 30, hwnd, (HMENU)ID_BUTTON_SWITCH,
                     (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    // --- Analytics ---
    hBtnToggleAnalytics =
        CreateWindow("BUTTON", "v", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 10,
                     330, 20, 20, hwnd, (HMENU)ID_BTN_TOGGLE_ANALYTICS,
                     (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    hLabelAnalyticsTitle = CreateWindow("STATIC", "Top Apps (Time)",
                                        WS_VISIBLE | WS_CHILD | SS_LEFT, 30,
                                        330, 200, 20, hwnd, NULL, NULL, NULL);

    // ListBox (Stats)
    hListBoxStats =
        CreateWindow("LISTBOX", NULL,
                     WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_BORDER |
                         LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
                     10, 350, 220, 80, hwnd, (HMENU)ID_LISTBOX_STATS,
                     (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    // --- Bookmarks Launcher ---
    hLabelBookmarksTitle =
        CreateWindow("STATIC", "Bookmarks", WS_VISIBLE | WS_CHILD | SS_CENTER,
                     10, 480, 220, 20, hwnd, NULL, NULL, NULL);

    hListBoxBookmarks =
        CreateWindow("LISTBOX", NULL,
                     WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_BORDER |
                         LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
                     10, 500, 220, 60, hwnd, (HMENU)ID_LISTBOX_BOOKMARKS,
                     (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    hBtnAddBookmark =
        CreateWindow("BUTTON", "Add Bookmark",
                     WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 10,
                     570, 105, 30, hwnd, (HMENU)ID_BTN_ADD_BOOKMARK,
                     (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    hBtnLaunchBookmarks =
        CreateWindow("BUTTON", "Launch All",
                     WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 125,
                     570, 105, 30, hwnd, (HMENU)ID_BTN_LAUNCH_BOOKMARKS,
                     (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    // Initial load
    UpdateBookmarkList();
    UpdateLayout();

    // Status Bar
    hStatusBar = CreateWindowEx(
        0, STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0,
        0, 0, hwnd, (HMENU)ID_STATUSBAR,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    RefreshWindowList();

    // Stats and analytics tick every second; the window list follows events.
    SetTimer(hwnd, ID_TIMER_REFRESH, 1000, NULL);
    break;

  case WM_TIMER:
    if (wParam == ID_TIMER_WINLIST) {
      KillTimer(hwnd, ID_TIMER_WINLIST);
      winListRefreshPending = false;
      RefreshWindowList();
      ticksSinceResync = 0;
    } else if (wParam == ID_TIMER_REFRESH) {
      if (++ticksSinceResync >= WINLIST_RESYNC_TICKS) {
        RefreshWindowList();
        ticksSinceResync = 0;
      }
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
    case ID_BTN_ADD_BOOKMARK:
      AddBookmark();
      break;
    case ID_BTN_LAUNCH_BOOKMARKS:
      bookmarkMgr.ExecuteAll();
      UpdateStatus("Launching bookmarks...");
      break;
    case ID_BTN_TOGGLE_STATS:
      isStatsExpanded = !isStatsExpanded;
      UpdateLayout();
      break;
    case ID_BTN_TOGGLE_ANALYTICS:
      isAnalyticsExpanded = !isAnalyticsExpanded;
      UpdateLayout();
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
    KillTimer(hwnd, ID_TIMER_WINLIST);
    UnhookWindowEvents();
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
