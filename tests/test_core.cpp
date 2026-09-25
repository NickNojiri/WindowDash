// Unit tests for the Win32-free core (src/core). Plain C++, no framework:
// run with `ctest` or the binary directly; exit code is the failure count.

#include "core/TileLayout.h"
#include "core/UsageStats.h"
#include "core/WindowFilter.h"

#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
const char *g_test = "";

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      ++g_failures;                                                            \
      std::printf("FAIL %s: %s (line %d)\n", g_test, #cond, __LINE__);         \
    }                                                                          \
  } while (0)

using core::Rect;

// Every pixel of `area` is covered exactly once by `rects`.
bool CoversExactly(const std::vector<Rect> &rects, const Rect &area) {
  long long sum = 0;
  for (const auto &r : rects) {
    if (r.x < area.x || r.y < area.y || r.x + r.w > area.x + area.w ||
        r.y + r.h > area.y + area.h || r.w <= 0 || r.h <= 0)
      return false;
    sum += static_cast<long long>(r.w) * r.h;
  }
  for (std::size_t i = 0; i < rects.size(); ++i)
    for (std::size_t j = i + 1; j < rects.size(); ++j) {
      const Rect &a = rects[i], &b = rects[j];
      bool apart = a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y ||
                   b.y + b.h <= a.y;
      if (!apart)
        return false;
    }
  return sum == static_cast<long long>(area.w) * area.h;
}

void TestTileSingleWindowMaximizes() {
  Rect area{0, 0, 1920, 1040};
  auto plan = core::PlanTiles(1, area);
  CHECK(plan.maximizeSingle);
  CHECK(plan.rects.size() == 1 && plan.rects[0] == area);
}

void TestTileNothingToTile() {
  CHECK(core::PlanTiles(0, {0, 0, 100, 100}).rects.empty());
  CHECK(core::PlanTiles(-1, {0, 0, 100, 100}).rects.empty());
}

void TestTileTwoAndThree() {
  Rect area{0, 0, 1920, 1040};
  auto two = core::PlanTiles(2, area);
  CHECK(!two.maximizeSingle);
  CHECK(two.rects[0] == (Rect{0, 0, 960, 1040}));
  CHECK(two.rects[1] == (Rect{960, 0, 960, 1040}));

  auto three = core::PlanTiles(3, area);
  CHECK(three.rects[0] == (Rect{0, 0, 960, 1040}));
  CHECK(three.rects[1] == (Rect{960, 0, 960, 520}));
  CHECK(three.rects[2] == (Rect{960, 520, 960, 520}));
}

void TestTileGridShape() {
  auto plan = core::PlanTiles(5, {0, 0, 1200, 800});  // 3 cols x 2 rows
  CHECK(plan.rects.size() == 5);
  CHECK(plan.rects[0] == (Rect{0, 0, 400, 400}));
  CHECK(plan.rects[3] == (Rect{0, 400, 400, 400}));
}

void TestTileNoGapsWithOddSizesAndOffsetMonitor() {
  // A second monitor to the left of the primary, with an odd work area.
  Rect area{-1366, 31, 1366, 737};
  for (int n : {1, 2, 3, 4, 6, 9}) {
    auto plan = core::PlanTiles(n, area);
    CHECK(plan.rects.size() == static_cast<std::size_t>(n));
    CHECK(CoversExactly(plan.rects, area));
  }
  // Partly-filled last row: tiles stay inside the area and don't overlap.
  auto seven = core::PlanTiles(7, area);
  CHECK(seven.rects.size() == 7);
}

void TestUsageCreditsElapsedTimeToTheForegroundApp() {
  core::UsageStats s;
  s.Tick("code.exe", 1000);      // starts the clock, credits nothing
  s.Tick("code.exe", 3000);      // +2 s to code
  s.Tick("chrome.exe", 4500);    // +1.5 s to chrome
  s.Tick("chrome.exe", 5000);    // +0.5 s to chrome
  CHECK(s.SecondsFor("code.exe") == 2);
  CHECK(s.SecondsFor("chrome.exe") == 2);
}

void TestUsageCapsLongGaps() {
  core::UsageStats s;
  s.Tick("a.exe", 0);
  s.Tick("a.exe", 60 * 60 * 1000);   // one hour asleep
  CHECK(s.SecondsFor("a.exe") == 5);
}

void TestUsageIgnoresEmptyNameAndClockGoingBack() {
  core::UsageStats s;
  s.Tick("", 0);
  s.Tick("", 2000);
  s.Tick("a.exe", 1000);   // clock went backwards: no credit
  CHECK(s.Top(10).empty());
}

void TestUsageTopIsStableOnTies() {
  core::UsageStats s;
  std::istringstream in("zeta.exe=5\nalpha.exe=5\nmid.exe=9\n");
  s.Load(in);
  auto top = s.Top(10);
  CHECK(top.size() == 3);
  CHECK(top[0].name == "mid.exe");
  CHECK(top[1].name == "alpha.exe" && top[2].name == "zeta.exe");
  CHECK(s.Top(1).size() == 1);
}

void TestUsageLoadSaveRoundTripAndBadLines() {
  core::UsageStats s;
  std::istringstream in("a=b.exe=12\r\n"   // '=' in the name, Windows line end
                        "no-number=\n"
                        "=5\n"
                        "neg.exe=-3\n"
                        "huge.exe=99999999999999999999\n"
                        "ok.exe=7\n");
  s.Load(in);
  CHECK(s.SecondsFor("a=b.exe") == 12);
  CHECK(s.SecondsFor("ok.exe") == 7);
  CHECK(s.Top(10).size() == 2);

  std::ostringstream out;
  s.Save(out);
  CHECK(out.str() == "a=b.exe=12\nok.exe=7\n");
}

void TestExcludedTitlesAndDisplayName() {
  CHECK(core::IsExcludedTitle("Program Manager"));
  CHECK(core::IsExcludedTitle("Window Dash"));
  CHECK(!core::IsExcludedTitle("Window Dash - notes.txt"));
  CHECK(core::DisplayName("C:\\Tools\\app.exe") == "app.exe");
  CHECK(core::DisplayName("/usr/bin/tool") == "tool");
  CHECK(core::DisplayName("plain.exe") == "plain.exe");
}

} // namespace

int main() {
  const std::pair<const char *, std::function<void()>> tests[] = {
      {"tile single maximizes", TestTileSingleWindowMaximizes},
      {"tile nothing", TestTileNothingToTile},
      {"tile two and three", TestTileTwoAndThree},
      {"tile grid shape", TestTileGridShape},
      {"tile no gaps", TestTileNoGapsWithOddSizesAndOffsetMonitor},
      {"usage credits elapsed", TestUsageCreditsElapsedTimeToTheForegroundApp},
      {"usage caps gaps", TestUsageCapsLongGaps},
      {"usage ignores empty", TestUsageIgnoresEmptyNameAndClockGoingBack},
      {"usage stable ties", TestUsageTopIsStableOnTies},
      {"usage load/save", TestUsageLoadSaveRoundTripAndBadLines},
      {"filter and display name", TestExcludedTitlesAndDisplayName},
  };
  for (const auto &t : tests) {
    g_test = t.first;
    t.second();
  }
  std::printf("%zu tests, %d failed checks\n", sizeof(tests) / sizeof(tests[0]),
              g_failures);
  return g_failures;
}
