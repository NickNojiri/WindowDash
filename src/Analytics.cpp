#include "Analytics.h"
#include <algorithm>
#include <basetsd.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <windows.h>

#define PSAPI_VERSION 1
#include <psapi.h>

Analytics::Analytics() { Load("analytics.dat"); }

Analytics::~Analytics() { Save("analytics.dat"); }

void Analytics::Save(const std::string &filename) {
  std::ofstream outfile(filename);
  if (outfile.is_open()) {
    for (const auto &pair : m_usageMap) {
      outfile << pair.first << "=" << pair.second << "\n";
    }
    outfile.close();
  }
}

void Analytics::Load(const std::string &filename) {
  std::ifstream infile(filename);
  if (infile.is_open()) {
    std::string line;
    while (std::getline(infile, line)) {
      std::istringstream iss(line);
      std::string name;
      std::string secondsStr;
      if (std::getline(iss, name, '=') && std::getline(iss, secondsStr)) {
        try {
          long seconds = std::stol(secondsStr);
          m_usageMap[name] = seconds;
        } catch (...) {
        }
      }
    }
    infile.close();
  }
}

void Analytics::Update() {
  std::string processName = GetActiveWindowProcessName();
  if (!processName.empty()) {
    m_usageMap[processName]++;
  }
}

std::vector<AppUsage> Analytics::GetTopApps(int limit) {
  std::vector<AppUsage> apps;
  for (const auto &pair : m_usageMap) {
    apps.push_back({pair.first, pair.second});
  }

  // Sort by usage (descending)
  std::sort(apps.begin(), apps.end(), [](const AppUsage &a, const AppUsage &b) {
    return a.seconds > b.seconds;
  });

  if (apps.size() > static_cast<size_t>(limit)) {
    apps.resize(limit);
  }
  return apps;
}

std::string Analytics::GetActiveWindowProcessName() {
  HWND hwnd = GetForegroundWindow();
  if (!hwnd)
    return "";

  DWORD pid;
  GetWindowThreadProcessId(hwnd, &pid);

  HANDLE hProcess =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if (!hProcess)
    return "";

  char buffer[MAX_PATH];
  if (GetModuleBaseNameA(hProcess, NULL, buffer, MAX_PATH)) {
    CloseHandle(hProcess);
    return std::string(buffer);
  }

  CloseHandle(hProcess);
  return "Unknown";
}
