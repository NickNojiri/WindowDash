#ifndef ANALYTICS_H
#define ANALYTICS_H

#include "core/UsageStats.h"

#include <string>
#include <vector>

using core::AppUsage;

// Win32 side of usage tracking: finds the foreground app and feeds it, with
// the tick clock, to core::UsageStats, which does the accounting.
class Analytics {
public:
  Analytics();
  ~Analytics();
  void Update();
  std::vector<AppUsage> GetTopApps(int limit = 5);
  void Save(const std::string &filename);
  void Load(const std::string &filename);

private:
  core::UsageStats m_stats;
  std::string GetActiveWindowProcessName();
};

#endif // ANALYTICS_H
