cmake_minimum_required(VERSION 3.16)
project(WindowDash VERSION 1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT WIN32)
  message(FATAL_ERROR "WindowDash is a Win32 app. Build with MinGW-w64 on "
                      "Windows, or cross-compile with CMAKE_SYSTEM_NAME=Windows.")
endif()

# Logic shared by the app and the unit tests.
# MacroManager.cpp is intentionally not built: it is dead code, superseded by
# BookmarkManager (see docs/ARCHITECTURE.md).
add_library(windowdash_core STATIC
    src/WindowManager.cpp
    src/SystemMonitor.cpp
    src/Analytics.cpp
    src/bookmarks/BookmarkManager.cpp)
target_include_directories(windowdash_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(windowdash_core PUBLIC _WIN32_WINNT=0x0600 _CRT_SECURE_NO_WARNINGS)
target_link_libraries(windowdash_core PUBLIC user32 gdi32 pdh psapi)

# WIN32 selects the GUI subsystem (-mwindows), so no console window opens.
add_executable(WindowDash WIN32 src/gui_main.cpp resources/app.rc)
target_link_libraries(WindowDash PRIVATE windowdash_core comctl32 comdlg32)

if(MINGW)
  # Static runtime so the exe runs on a machine without MinGW DLLs.
  target_link_options(WindowDash PRIVATE -static)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(windowdash_tests tests/test_main.cpp)
  target_link_libraries(windowdash_tests PRIVATE windowdash_core)
  if(MINGW)
    target_link_options(windowdash_tests PRIVATE -static)
  endif()
  # The test exe moves into its own temp directory before touching *.dat.
  add_test(NAME windowdash_tests COMMAND windowdash_tests)
endif()
