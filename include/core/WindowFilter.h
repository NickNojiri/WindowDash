#pragma once

// Small string rules shared by the window list and the bookmark list.
// Pure logic, no Win32 (tests/test_core.cpp).

#include <cstdint>
#include <string>
#include <vector>

namespace core {

// Titles of shell and system windows that are visible but aren't apps a
// user would want tiled, plus WindowDash's own window.
bool IsExcludedTitle(const std::string &title);

// "C:\\Tools\\app.exe" -> "app.exe" (either slash style).
std::string DisplayName(const std::string &path);

// A window as the list box shows it. `id` is the HWND as an integer, so this
// header needs no Win32.
struct ListedWindow {
  std::uintptr_t id;
  std::string title;
  bool operator==(const ListedWindow &o) const {
    return id == o.id && title == o.title;
  }
};

} // namespace core
