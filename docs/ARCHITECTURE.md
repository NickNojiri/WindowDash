# WindowDash Architecture

This describes the code as of commit `83ef4fc`. Every claim cites a file and
line. Items marked **verified** were checked by building the code. Items marked
**unverified** are read from the code and still need to be confirmed on a Windows
machine. This PR documents the code and does not change it.

## 1. Shape of the program

One Win32 GUI process, written in C++17 and using the ANSI (`char`) APIs.

| Module | Files | Role |
|---|---|---|
| GUI / entry point | `src/gui_main.cpp` | `WinMain`, the window procedure, all controls, the refresh timer, hotkeys |
| `WindowManager` | `src/WindowManager.cpp` | Enumerates top-level windows; tile, minimize-all-but, restore, switch |
| `SystemMonitor` | `src/SystemMonitor.cpp` | PDH query for total CPU %; `GlobalMemoryStatusEx` for RAM % |
| `Analytics` | `src/Analytics.cpp` | Counts foreground-app "seconds"; persists to `analytics.dat` |
| `BookmarkManager` | `src/bookmarks/BookmarkManager.cpp` | Program list for "Launch All"; persists to `bookmarks.dat` |
| `MacroManager` | `src/MacroManager.cpp` | **Dead code.** Replaced by `BookmarkManager` in `83ef4fc`. Nothing references it. |

## 2. Threads

**There is exactly one thread of our own: the UI thread.** The code never calls
`CreateThread`, `_beginthread`, `std::thread`, `std::async`, `SetWinEventHook`, or
`SetWindowsHookEx` (grep of `src/` and `include/`). All work in the app runs
inside the `GetMessage` loop at `gui_main.cpp:520`.

(`ShellExecute` and `GetOpenFileName` may start threads inside the OS for their
own use. We don't own those threads or share state with them.)

### What runs, and when

| Trigger | Where | Work done on the UI thread |
|---|---|---|
| Static initialization, **before `WinMain`** | globals at `gui_main.cpp:38-41` | `WindowManager()` calls `EnumWindows` (`WindowManager.cpp:7`). `Analytics()` reads `analytics.dat` (`Analytics.cpp:12`). `BookmarkManager()` reads `bookmarks.dat` (`BookmarkManager.cpp:5`). |
| `WM_CREATE` | `gui_main.cpp:280` | `sysMon.Initialize()` (opens the PDH query and takes the first sample), `RegisterHotKey` x2, creates all child controls, `SetTimer(1000 ms)` |
| `WM_TIMER`, ~1 Hz | `gui_main.cpp:402-407` | `RefreshWindowList()` → `UpdateSystemStats()` → `UpdateAnalytics()` (see the tick breakdown below) |
| Button press / hotkey | `gui_main.cpp:410-461` | Tile, Focus, Restore, Switch, and Launch run synchronously. Tile, Focus, and Restore enumerate the windows again first (`WindowManager.cpp:50,130,142`). |
| `WM_DESTROY` | `gui_main.cpp:463` | Unregisters hotkeys, `KillTimer`, `PostQuitMessage` |
| Static destruction, **after `WinMain` returns** | globals | `~Analytics` writes `analytics.dat`. `~BookmarkManager` writes `bookmarks.dat`. `~SystemMonitor` closes the PDH query. |

### One refresh tick (`WM_TIMER`)

```
WM_TIMER (USER timer, 1000 ms, low priority, coalesced by the OS)
├─ RefreshWindowList()                          gui_main.cpp:134   always
│   ├─ EnumWindows over every top-level window  WindowManager.cpp:17
│   │    per visible window: GetWindowTextLength + GetWindowText
│   ├─ SetWindowText(count label)
│   └─ listbox rebuild (only if the count changed or the selected HWND vanished)
├─ UpdateSystemStats()                          gui_main.cpp:180   only if Stats panel expanded
│   ├─ PdhCollectQueryData + PdhGetFormattedCounterValue   SystemMonitor.cpp:24-25
│   ├─ GlobalMemoryStatusEx
│   └─ SetWindowText x2
└─ UpdateAnalytics()                            gui_main.cpp:196   only if Analytics panel expanded
    ├─ GetForegroundWindow → OpenProcess → GetModuleBaseNameA → CloseHandle
    ├─ usage[name]++
    └─ LB_RESETCONTENT + up to 5 LB_ADDSTRING  (rebuilt on every tick)
```

### The two questions the roadmap asked

* **Where does PDH sampling run?** On the UI thread, inside `WM_TIMER`
  (`gui_main.cpp:405` → `SystemMonitor.cpp:24`). It is skipped while the Stats
  panel is collapsed (`gui_main.cpp:181`).
* **Does window discovery poll?** Yes. It runs a full `EnumWindows` pass every
  timer tick (~1 s) and again on demand before Tile, Focus, and Restore. It
  never subscribes to window events.

### What this means for the planned threading work (not done here)

* **W2 (move PDH off the UI thread):** this applies, because sampling is on the
  UI thread. Whether it's *worth* doing is not yet known. One
  `PdhCollectQueryData` on a single `_Total` counter is normally short. Measure
  first. The code already provides a free A/B test: collapsing the Stats panel
  removes all PDH work from the tick, and collapsing Analytics removes the
  `OpenProcess` work. Run `scripts/measure_idle.ps1` once with both panels
  expanded and once with both collapsed. The difference is the cost of those
  two stages.
* **W3 (`SetWinEventHook` instead of polling):** this applies, because
  discovery polls at 1 Hz. The hook would also fix defect D5 below (stale titles
  and stale entries).

## 3. Native handle ownership and lifetime

| Handle | Created | Owner | Released | Notes |
|---|---|---|---|---|
| Window class `WindowDashClass` | `RegisterClass`, `gui_main.cpp:498` | process | never (freed at process exit) | Return value not checked |
| Main `HWND` (`hMainWnd`) | `CreateWindowEx`, `gui_main.cpp:500` | us | `DefWindowProc(WM_CLOSE)` → `DestroyWindow` | |
| Child control `HWND`s (buttons, labels, 3 listboxes, status bar) | `WM_CREATE`, `gui_main.cpp:289-396` | main window | destroyed automatically with the parent | Globals keep dangling copies after `WM_DESTROY`. Harmless, because nothing touches them afterward. |
| Global hotkeys `ID_HOTKEY_TILE`/`FOCUS` (Alt+Shift+T/F) | `RegisterHotKey`, `gui_main.cpp:285-286` | us | `UnregisterHotKey`, `gui_main.cpp:464-465` | A failed registration (another app owns the chord) is silently ignored |
| Timer `ID_TIMER_REFRESH` | `SetTimer`, `gui_main.cpp:399` | us | `KillTimer`, `gui_main.cpp:466` | |
| `PDH_HQUERY cpuQuery` | `PdhOpenQuery`, `SystemMonitor.cpp:13` (on `WM_CREATE`) | `SystemMonitor` (global) | `PdhCloseQuery` in the destructor, `SystemMonitor.cpp:8`, **after `WinMain` returns** | Return codes not checked (D7) |
| `PDH_HCOUNTER cpuTotal` | `PdhAddEnglishCounter`, `SystemMonitor.cpp:16` | the query | closed together with the query | |
| Process `HANDLE` (foreground app) | `OpenProcess`, `Analytics.cpp:78` | `Analytics::GetActiveWindowProcessName` | `CloseHandle` on both return paths, `Analytics.cpp:85,89` | Opened and closed once per tick. No leak. |
| Foreign `HWND`s in `WindowManager::m_visibleWindows` and in listbox item data | `EnumWindows` | **other processes** | never (not ours) | Can go stale between ticks. `SwitchToWindow` checks `IsWindow` (`WindowManager.cpp:149`). Tile, Focus, and Restore enumerate again first. |
| `HDC` in `WM_PAINT` | `BeginPaint`, `gui_main.cpp:472` | us | `EndPaint`, `gui_main.cpp:474` | |
| Background brush `COLOR_WINDOW + 1` | system color index | system | never delete | |

To draw it on a whiteboard: one thread and one window. There are two kinds of
kernel/PDH handle (the PDH query and the per-tick process handle), both released
deterministically. The foreign `HWND`s are only borrowed, never owned.

## 4. Persistence

All data files are opened by **relative path**, so they land in the process's
current working directory, not next to the exe. Launching from a shortcut with a
different "Start in" folder gives a different set of bookmarks and analytics.

| File | Format | Written |
|---|---|---|
| `bookmarks.dat` | one path per line | on every add/remove, and at static destruction |
| `analytics.dat` | `process.exe=<ticks>` per line | **only at static destruction** (a clean exit) |
| `macros.dat` | one path per line | dead code (`MacroManager`) |

`bookmarks.dat` is committed to the repo (as an empty file). `.gitignore` covers
`macros.dat` and `analytics.dat` but not `bookmarks.dat`.

## 5. Defects found while reading (not fixed in this PR)

| # | Defect | Where | Status |
|---|---|---|---|
| D1 | The README build command fails to link: it lists `src/MacroManager.cpp` but not `src/bookmarks/BookmarkManager.cpp`, which `gui_main.cpp` needs since `83ef4fc`. | `README.md:16` | **verified** (MinGW-w64 g++ 13: `undefined reference to BookmarkManager::…`). **Fixed** in the CMake/CI commit. |
| D2 | The CMake build produces a **console-subsystem** exe, so a console window opens next to the GUI. The target is named `resizer_gui`, and it compiles the dead `MacroManager.cpp` via `GLOB_RECURSE`. | `CMakeLists.txt:14` | **verified** (`objdump -p`: `Subsystem 3 (Windows CUI)`). **Fixed** in the CMake/CI commit; CI now asserts subsystem 2. |
| D3 | **Analytics stops counting while its panel is collapsed**: `UpdateAnalytics` returns before `analytics.Update()`. "Top Apps (Time)" undercounts any app used while the panel is closed. | `gui_main.cpp:197-200` | read from code |
| D4 | Analytics "seconds" are really `WM_TIMER` ticks. Those are low-priority and coalesced, and they don't arrive while the UI thread is blocked, so the count is not wall-clock time. Time is also counted while the user is away (no idle check). The data is saved only on a clean window close. When Windows ends the session, the process can be terminated after `WM_ENDSESSION` without running static destructors, so all usage since launch is lost. | `Analytics.cpp:14,46-51` | read from code; the logoff data loss is **unverified** |
| D5 | The window list goes stale. The listbox is rebuilt only when the window *count* changes or the selected HWND vanishes. Title changes (for example browser tab switches) never show. If one window closes and another opens within the same tick, the list keeps the dead entry and misses the new one. | `gui_main.cpp:151-177` | read from code |
| D6 | Tiling a single window maximizes it and then immediately undoes that. `ShowWindow(SW_MAXIMIZE)` is followed unconditionally by `ShowWindow(SW_RESTORE)`. | `WindowManager.cpp:121,125` | read from code; behavior **unverified** on Windows |
| D7 | PDH return codes are ignored. If `PdhOpenQuery` or `PdhAddEnglishCounter` fails (for example, corrupted perf counters), `GetCpuUsage` returns an uninitialized `PDH_FMT_COUNTERVALUE`. | `SystemMonitor.cpp:13-26` | read from code |
| D8 | Apps running elevated (or protected) are not counted when WindowDash runs non-elevated. `OpenProcess(PROCESS_QUERY_INFORMATION \| PROCESS_VM_READ)` fails for them. `PROCESS_QUERY_LIMITED_INFORMATION` + `QueryFullProcessImageName` would work. | `Analytics.cpp:79` | read from code |
| D9 | Hidden UWP ("cloaked") windows are filtered by a hard-coded title list (`"Settings"`, `"Microsoft Text Input Application"`, …). This also hides any real window that has one of those titles. `DwmGetWindowAttribute(DWMWA_CLOAKED)` is the direct test. | `WindowManager.cpp:36-39` | read from code |
| D10 | After the Stats panel is re-expanded, the first CPU value averages the whole collapsed period, because PDH computes over the time between two collects. | `gui_main.cpp:181`, `SystemMonitor.cpp:24` | read from code |
| D11 | Tiling a *minimized* window calls `MoveWindow` before `SW_RESTORE`. Restore may use the saved normal placement and ignore the move. | `WindowManager.cpp:123-125` | **unverified**, suspected |
