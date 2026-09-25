# WindowDash architecture

A single-window Win32 app (C++17, ANSI Win32 API). About 1,000 lines.

## Modules

| Layer | Files | Depends on Win32? | Tested by |
|---|---|---|---|
| Core logic | `src/core/TileLayout.cpp`, `UsageStats.cpp`, `WindowFilter.cpp` | No | `tests/test_core.cpp` (any OS, CI) |
| Window discovery and actions | `src/WindowManager.cpp` | Yes: `EnumWindows`, `MoveWindow`, `ShowWindow`, DWM | CI smoke run only |
| Usage tracking | `src/Analytics.cpp` (foreground process lookup, feeds `core::UsageStats`) | Yes | core part unit-tested |
| System stats | `src/SystemMonitor.cpp` (PDH CPU counter, `GlobalMemoryStatusEx`) | Yes | CI smoke run only |
| Bookmarks | `src/bookmarks/BookmarkManager.cpp` (list file, `ShellExecute`) | Yes | — |
| UI | `src/gui_main.cpp` (window procedure, controls, layout, timers, hooks) | Yes | CI smoke run only |

The rule for new code: anything that is a decision (a layout, a count, a filter, a file
format) goes in `src/core` with a test. The Win32 files only gather input and apply the
decision.

## Threading model

**One thread.** Everything runs on the UI thread that owns the main window: the message
loop in `WinMain`, the window procedure, the timers, and the window-event callbacks.
There is no worker thread and no shared state, so there are no locks.

- Window events come from `SetWinEventHook` with `WINEVENT_OUTOFCONTEXT`. Windows
  delivers those callbacks through the hooking thread's message loop, which is the UI
  thread.
- Per event, the callback only starts a 100 ms timer (`ID_TIMER_WINLIST`), unless one is
  already pending. A burst of events, like an app opening three windows, turns into one
  `EnumWindows` pass. Because later events don't restart the timer, a window whose title
  changes nonstop can't hold the refresh back for more than 100 ms.
- Each work item is short: one `EnumWindows` pass (~a few dozen windows), one PDH sample,
  and one `OpenProcess` call. If profiling ever shows one of them blocking input, move it
  to a worker thread that posts results back with `PostMessage`. The UI would still own
  all the controls.

## Event flow

```
window opened / closed / shown / hidden / renamed / minimized / cloaked
  └─ OnWinEvent (top-level windows only)   → SetTimer(ID_TIMER_WINLIST, 100 ms) unless pending
       └─ WM_TIMER ID_TIMER_WINLIST        → RefreshWindowList()
            └─ list box rebuilt only if a window or a title changed

every 1 s: WM_TIMER ID_TIMER_REFRESH
  ├─ UpdateSystemStats()   (skipped while the panel is collapsed)
  ├─ UpdateAnalytics()     (always counts; draws only while expanded)
  └─ every 10th tick: RefreshWindowList() as a safety net for a missed event
```

Before this design the window list was rebuilt by polling `EnumWindows` every second.
By design a change should now show up about 100 ms after the event, and an idle desktop
costs one enumeration every 10 s instead of every second. Neither is measured yet (see
Known limitations).

The hook ranges are split on purpose. `EVENT_OBJECT_CREATE..EVENT_OBJECT_NAMECHANGE`
as one range would include `EVENT_OBJECT_LOCATIONCHANGE`, which fires continuously while
anything moves.

## Native resource ownership

| Resource | Created in | Released in | Notes |
|---|---|---|---|
| Main window and child controls | `WinMain` / `WM_CREATE` | `DestroyWindow` (by the system on close) | Children are destroyed with the parent |
| Win-event hooks (`HWINEVENTHOOK`) | `HookWindowEvents()` in `WM_CREATE` | `UnhookWindowEvents()` in `WM_DESTROY` | Kept in `winEventHooks` |
| Global hotkeys (Alt+Shift+T / F) | `WM_CREATE` | `WM_DESTROY` | System-wide; a second instance fails to register them |
| Timers | `WM_CREATE`, `OnWinEvent` | `WM_DESTROY` (`KillTimer`) | |
| PDH query | `SystemMonitor::Initialize()` | `~SystemMonitor()` | Global object, destroyed after `WinMain` returns |
| Process handle | `Analytics::GetActiveWindowProcessName()` | Same function, on every path | `PROCESS_QUERY_LIMITED_INFORMATION` only |
| Other apps' `HWND`s | Owned by those apps | — | Stored as plain values and may be stale; `SwitchToWindow` checks `IsWindow` first |

## Data files

`analytics.dat` (`name=seconds` lines) and `bookmarks.dat` (one path per line) are read
when the global objects are constructed and written when they are destroyed (bookmarks
are also saved on every change).

## Known limitations

- **Relative data paths.** Both files live in the working directory, so launching from a
  different shortcut or folder starts empty stats. They belong in `%LOCALAPPDATA%`.
- **Usage can be lost.** Usage is only saved on a clean exit, so a crash or a forced
  kill loses everything counted since launch.
- **ANSI API.** Window titles and paths outside the system code page (for example emoji
  or CJK text on an English system) come through as `?`. Fixing this means moving to the
  `W` APIs and UTF-8 internally.
- **No logging.** Diagnostics go only to `OutputDebugString` (visible in DebugView).
  There is no log file or crash report.
- **Not measured.** CPU use, memory, launch time and event-to-list latency haven't been
  measured yet. The event design above is expected to cut idle work, but that is not
  proven.
