# Window Dash

A powerful window management dashboard for Windows.

## Features
- **Window Grid**: Tile all your open windows instantly.
- **Focus Mode**: Minimize distractions by focusing on one window.
- **System Stats**: Real-time CPU and RAM usage monitoring.
- **Analytics**: Track your most used applications.
- **Workspace Launcher**: Create and launch sets of programs with one click.

## Build Instructions
1.  Ensure you have `g++` (MinGW) installed.
2.  Run the build task in VS Code or use the following command:
    ```bash
    g++ -std=c++17 -D_WIN32_WINNT=0x0600 -Iinclude -o WindowDash.exe src/gui_main.cpp src/WindowManager.cpp src/SystemMonitor.cpp src/Analytics.cpp src/MacroManager.cpp resources/app.o -lcomctl32 -lpdh -lpsapi -mwindows -lcomdlg32
    ```
