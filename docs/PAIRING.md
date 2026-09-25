# Pairing — Claude Code and Codex on WindowDash

Each agent owns a lane (a set of files). Don't edit a file in the other lane until that
lane's branch has merged. Put requests under "Asks".

```text
BASE:            claude/windowdash-foundation (on top of main @ 83ef4fc)
TESTS:           ctest -> core: 11 tests, 0 failed checks
LANE 1 (Claude): DONE, awaiting review. Build fixes, core library + tests, tiling and
                 analytics bugs, event hooks instead of polling, CI, architecture doc
LANE 2 (Codex):  NOT STARTED
```

| Lane | Owner | Roadmap items | Files |
|---|---|---|---|
| 1 Foundation | Claude Code | architecture doc, split logic from Win32, event hooks, unit tests, CI packaging | `CMakeLists.txt`, `cmake/`, `src/core/`, `include/core/`, `tests/`, `.github/`, `src/WindowManager.cpp`, `src/Analytics.cpp`, `docs/ARCHITECTURE.md` |
| 2 Reliability & measurement | Codex | crash-safe logging, data files in `%LOCALAPPDATA%` with migration, Unicode (`W` APIs), a performance harness (CPU, memory, launch time, event latency) | new `src/log/`, new `src/paths/`, `src/bookmarks/`, `src/SystemMonitor.cpp`, new `bench/` |

`src/gui_main.cpp` is shared. Lane 2 may add calls to its own new modules, but should
not restructure the timer and hook code (Lane 1).

## Asks

- **To Codex, from Claude.** Please measure the event-driven window list against the old
  1 s poll (`83ef4fc`): idle CPU and time from a window opening to the list updating. The
  architecture doc's "Not measured" note stays in until that's done.

## Not yet claimed

Separating UI rendering from state (a view-model layer), a worker thread (only if the
measurements show one is needed), and testing a clean install and uninstall on a fresh
Windows VM (needs a real machine).
