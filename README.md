# Window Dash

A powerful window management dashboard for Windows.

## Features
- **Window Grid**: Tile all your open windows instantly.
- **Focus Mode**: Minimize distractions by focusing on one window.
- **System Stats**: Real-time CPU and RAM usage monitoring.
- **Analytics**: Track your most used applications.
- **Workspace Launcher**: Create and launch sets of programs with one click.

## Build Instructions
Requires MinGW-w64 (`g++`, `windres`) and CMake 3.16+.

```bash
cmake -S . -B build -G "MinGW Makefiles"   # or -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure  # unit tests
```

This produces `build/WindowDash.exe`, a statically linked GUI-subsystem exe.
CI builds it on every push and uploads it as an artifact (Actions → build → Artifacts).

Without CMake, the equivalent single command is:
```bash
windres resources/app.rc -O coff -o resources/app.o
g++ -std=c++17 -D_WIN32_WINNT=0x0600 -Iinclude -o WindowDash.exe src/gui_main.cpp src/WindowManager.cpp src/SystemMonitor.cpp src/Analytics.cpp src/bookmarks/BookmarkManager.cpp resources/app.o -mwindows -static -lcomctl32 -lpdh -lpsapi -lcomdlg32
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the threading model, handle ownership, and known defects.
