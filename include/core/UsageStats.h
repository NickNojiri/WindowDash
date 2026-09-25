#pragma once

// Time spent per foreground application. Pure logic, no Win32: the caller
// passes the process name and a millisecond clock, so it is unit tested
// (tests/test_core.cpp).

#include <cstdint>
#include <istream>
#include <map>
#include <ostream>
#include <string>
#include <vector>

namespace core {

struct AppUsage {
  std::string name;
  long long seconds;
};

class UsageStats {
public:
  // Longest gap credited to one app between two ticks. A longer gap means the
  // machine slept or the UI thread stalled; that time is not "usage".
  static constexpr std::uint64_t kMaxCreditMs = 5000;

  // Credit the time since the previous tick to `app` (the app in the
  // foreground now). The first tick only starts the clock. An empty name
  // (nothing in front, or access denied) credits nobody.
  void Tick(const std::string &app, std::uint64_t nowMs);

  // Most-used apps first; ties broken by name so the list doesn't reorder
  // itself between refreshes.
  std::vector<AppUsage> Top(std::size_t limit) const;

  long long SecondsFor(const std::string &app) const;

  // File format: one "name=seconds" line per app. Names may contain '=':
  // the number is taken after the last one. Bad lines are skipped.
  void Load(std::istream &in);
  void Save(std::ostream &out) const;

private:
  std::map<std::string, std::uint64_t> m_ms;
  bool m_started = false;
  std::uint64_t m_lastMs = 0;
};

} // namespace core
