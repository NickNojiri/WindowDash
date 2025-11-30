#ifndef ANALYTICS_H
#define ANALYTICS_H

#include <map>
#include <string>
#include <vector>

struct AppUsage {
  std::string name;
  long seconds;
};

class Analytics {
public:
  Analytics();
  ~Analytics();
  void Update();
  std::vector<AppUsage> GetTopApps(int limit = 5);
  void Save(const std::string &filename);
  void Load(const std::string &filename);

private:
  std::map<std::string, long> m_usageMap;
  std::string GetActiveWindowProcessName();
};

#endif // ANALYTICS_H
