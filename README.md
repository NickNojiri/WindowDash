# Window Dash

A powerful window management dashboard for Windows.

## Features
- **Window Grid**: Tile all your open windows instantly.
- **Focus Mode**: Minimize distractions by focusing on one window.
- **System Stats**: Real-time CPU and RAM usage monitoring.
- **Analytics**: Track your most used applications.
- **Workspace Launcher**: Create and launch sets of programs with one click.

## Build

Needs CMake 3.16+ and either Visual Studio 2019+ or MinGW-w64.

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release        # unit tests for the Win32-free core
```

The app is `build/Release/WindowDash.exe` (Visual Studio) or `build/WindowDash.exe`
(MinGW, a single static .exe). To cross-compile from Linux:

```bash
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-win
```

On Linux, `cmake -S . -B build` builds and tests only the core library. CI
(`.github/workflows/ci.yml`) does all three builds, runs the tests, launches the app for
5 seconds on Windows, and uploads the `.exe`. A `v*` tag attaches it to a GitHub release.

## How it works

See **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** for the modules, the threading
model, how window events flow, and who owns each native handle.
