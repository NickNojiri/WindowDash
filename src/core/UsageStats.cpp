#include "core/UsageStats.h"

#include <algorithm>

namespace core {

void UsageStats::Tick(const std::string &app, std::uint64_t nowMs) {
  if (m_started && !app.empty() && nowMs > m_lastMs)
    m_ms[app] += std::min(nowMs - m_lastMs, kMaxCreditMs);
  m_started = true;
  m_lastMs = nowMs;
}

std::vector<AppUsage> UsageStats::Top(std::size_t limit) const {
  std::vector<AppUsage> apps;
  for (const auto &kv : m_ms)
    apps.push_back({kv.first, static_cast<long long>(kv.second / 1000)});
  std::sort(apps.begin(), apps.end(), [](const AppUsage &a, const AppUsage &b) {
    if (a.seconds != b.seconds)
      return a.seconds > b.seconds;
    return a.name < b.name;
  });
  if (apps.size() > limit)
    apps.resize(limit);
  return apps;
}

long long UsageStats::SecondsFor(const std::string &app) const {
  auto it = m_ms.find(app);
  return it == m_ms.end() ? 0 : static_cast<long long>(it->second / 1000);
}

void UsageStats::Load(std::istream &in) {
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    auto eq = line.rfind('=');
    if (eq == std::string::npos || eq == 0 || eq + 1 == line.size())
      continue;
    const std::string digits = line.substr(eq + 1);
    if (digits.find_first_not_of("0123456789") != std::string::npos ||
        digits.size() > 15)
      continue;
    m_ms[line.substr(0, eq)] = std::stoull(digits) * 1000;
  }
}

void UsageStats::Save(std::ostream &out) const {
  for (const auto &kv : m_ms)
    out << kv.first << '=' << kv.second / 1000 << '\n';
}

} // namespace core
