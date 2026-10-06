// Unit tests for the non-GUI logic. No framework: each CHECK that fails is
// printed, and the process exits non-zero if any failed (ctest reads that).
//
// Analytics and BookmarkManager load/save *.dat in the current directory from
// their constructors/destructors, so main() first moves into a fresh temp
// directory (never the user's real data) and each test deletes those files.

#include "Analytics.h"
#include "bookmarks/BookmarkManager.h"
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <windows.h>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    ++g_checks;                                                                \
    if (!(cond)) {                                                             \
      ++g_failures;                                                            \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
    }                                                                          \
  } while (0)

static void WriteFile(const char *name, const std::string &content) {
  std::ofstream out(name, std::ios::binary);
  out << content;
}

static void ResetDataFiles() {
  std::remove("analytics.dat");
  std::remove("bookmarks.dat");
  std::remove("test_input.dat");
  std::remove("test_output.dat");
}

// --- Analytics ---

static void TestAnalyticsTopAppsSortedDescending() {
  ResetDataFiles();
  WriteFile("test_input.dat", "a.exe=5\nb.exe=50\nc.exe=20\n");
  Analytics analytics;
  analytics.Load("test_input.dat");

  auto top = analytics.GetTopApps(5);
  CHECK(top.size() == 3);
  if (top.size() == 3) {
    CHECK(top[0].name == "b.exe" && top[0].seconds == 50);
    CHECK(top[1].name == "c.exe" && top[1].seconds == 20);
    CHECK(top[2].name == "a.exe" && top[2].seconds == 5);
  }
}

static void TestAnalyticsTopAppsRespectsLimit() {
  ResetDataFiles();
  WriteFile("test_input.dat", "a.exe=1\nb.exe=2\nc.exe=3\nd.exe=4\n");
  Analytics analytics;
  analytics.Load("test_input.dat");

  auto top = analytics.GetTopApps(2);
  CHECK(top.size() == 2);
  if (top.size() == 2) {
    CHECK(top[0].name == "d.exe");
    CHECK(top[1].name == "c.exe");
  }
  CHECK(analytics.GetTopApps(0).empty());
}

static void TestAnalyticsLoadSkipsMalformedLines() {
  ResetDataFiles();
  WriteFile("test_input.dat", "good.exe=7\n"
                              "no_equals_sign\n"
                              "not_a_number.exe=abc\n"
                              "missing_value.exe=\n"
                              "\n");
  Analytics analytics;
  analytics.Load("test_input.dat");

  auto top = analytics.GetTopApps(10);
  CHECK(top.size() == 1);
  if (top.size() == 1) {
    CHECK(top[0].name == "good.exe" && top[0].seconds == 7);
  }
}

static void TestAnalyticsSaveLoadRoundTrip() {
  ResetDataFiles();
  WriteFile("test_input.dat", "x.exe=3\ny.exe=9\n");
  {
    Analytics a;
    a.Load("test_input.dat");
    a.Save("test_output.dat");
  }
  std::remove("analytics.dat"); // written by ~Analytics above
  Analytics b;
  b.Load("test_output.dat");
  auto top = b.GetTopApps(10);
  CHECK(top.size() == 2);
  if (top.size() == 2) {
    CHECK(top[0].name == "y.exe" && top[0].seconds == 9);
    CHECK(top[1].name == "x.exe" && top[1].seconds == 3);
  }
}

static void TestAnalyticsPersistsOnDestruction() {
  ResetDataFiles();
  WriteFile("test_input.dat", "kept.exe=4\n");
  {
    Analytics a;
    a.Load("test_input.dat");
  } // ~Analytics saves to analytics.dat
  Analytics b; // constructor loads analytics.dat
  auto top = b.GetTopApps(10);
  CHECK(top.size() == 1);
  if (top.size() == 1) {
    CHECK(top[0].name == "kept.exe" && top[0].seconds == 4);
  }
}

// --- BookmarkManager ---

static void TestBookmarkAddPersistsImmediately() {
  ResetDataFiles();
  {
    BookmarkManager mgr;
    CHECK(mgr.GetBookmarks().empty());
    mgr.AddBookmark("C:\\Tools\\one.exe");
    mgr.AddBookmark("C:\\Tools\\two.exe");
  }
  BookmarkManager reloaded;
  const auto &b = reloaded.GetBookmarks();
  CHECK(b.size() == 2);
  if (b.size() == 2) {
    CHECK(b[0] == "C:\\Tools\\one.exe");
    CHECK(b[1] == "C:\\Tools\\two.exe");
  }
}

static void TestBookmarkRemove() {
  ResetDataFiles();
  WriteFile("bookmarks.dat", "a.exe\nb.exe\nc.exe\n");
  {
    BookmarkManager mgr;
    mgr.RemoveBookmark(1);
    CHECK(mgr.GetBookmarks() == std::vector<std::string>({"a.exe", "c.exe"}));

    // Out-of-range indexes are ignored.
    mgr.RemoveBookmark(-1);
    mgr.RemoveBookmark(2);
    CHECK(mgr.GetBookmarks().size() == 2);
  }
  BookmarkManager reloaded;
  CHECK(reloaded.GetBookmarks() ==
        std::vector<std::string>({"a.exe", "c.exe"}));
}

static void TestBookmarkLoadSkipsBlankLinesAndReplaces() {
  ResetDataFiles();
  WriteFile("bookmarks.dat", "first.exe\n");
  WriteFile("test_input.dat", "\nx.exe\n\ny.exe\n");
  BookmarkManager mgr;
  CHECK(mgr.GetBookmarks() == std::vector<std::string>({"first.exe"}));
  mgr.Load("test_input.dat");
  CHECK(mgr.GetBookmarks() == std::vector<std::string>({"x.exe", "y.exe"}));
}

static char g_tempPath[MAX_PATH];
static std::string g_scratchDir;

static bool EnterScratchDirectory() {
  if (!GetTempPathA(MAX_PATH, g_tempPath))
    return false;
  g_scratchDir = std::string(g_tempPath) + "windowdash_tests_" +
                 std::to_string(GetCurrentProcessId());
  CreateDirectoryA(g_scratchDir.c_str(), NULL);
  return SetCurrentDirectoryA(g_scratchDir.c_str()) != 0;
}

static void LeaveScratchDirectory() {
  ResetDataFiles();
  SetCurrentDirectoryA(g_tempPath);
  RemoveDirectoryA(g_scratchDir.c_str());
}

int main() {
  if (!EnterScratchDirectory()) {
    std::printf("FAIL: could not create a scratch directory\n");
    return 1;
  }

  TestAnalyticsTopAppsSortedDescending();
  TestAnalyticsTopAppsRespectsLimit();
  TestAnalyticsLoadSkipsMalformedLines();
  TestAnalyticsSaveLoadRoundTrip();
  TestAnalyticsPersistsOnDestruction();
  TestBookmarkAddPersistsImmediately();
  TestBookmarkRemove();
  TestBookmarkLoadSkipsBlankLinesAndReplaces();
  LeaveScratchDirectory();

  std::printf("%d checks, %d failed\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
