cmake_minimum_required(VERSION 3.16)
project(WindowDash VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(WD_WARNINGS /W4 /permissive-)
else()
  set(WD_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# Win32-free logic (layout, usage accounting, list rules). Builds and is
# tested on any platform.
add_library(wd_core STATIC
  src/core/TileLayout.cpp
  src/core/UsageStats.cpp
  src/core/WindowFilter.cpp)
target_include_directories(wd_core PUBLIC include)
target_compile_options(wd_core PRIVATE ${WD_WARNINGS})

enable_testing()
add_executable(test_core tests/test_core.cpp)
target_link_libraries(test_core PRIVATE wd_core)
target_compile_options(test_core PRIVATE ${WD_WARNINGS})
add_test(NAME core COMMAND test_core)

# The Windows app itself.
if(WIN32)
  enable_language(RC)
  add_executable(WindowDash WIN32
    src/gui_main.cpp
    src/WindowManager.cpp
    src/SystemMonitor.cpp
    src/Analytics.cpp
    src/bookmarks/BookmarkManager.cpp
    resources/app.rc)
  target_compile_definitions(WindowDash PRIVATE
    _WIN32_WINNT=0x0600 _CRT_SECURE_NO_WARNINGS)
  target_compile_options(WindowDash PRIVATE ${WD_WARNINGS})
  target_link_libraries(WindowDash PRIVATE
    wd_core user32 comctl32 gdi32 pdh psapi comdlg32 dwmapi)
  if(MINGW)
    # One self-contained .exe: no libstdc++/libgcc DLLs to ship alongside.
    target_link_options(WindowDash PRIVATE -static)
  endif()
endif()
