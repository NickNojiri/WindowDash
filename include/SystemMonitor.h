#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <pdh.h>
#include <windows.h>

class SystemMonitor {
public:
  SystemMonitor();
  ~SystemMonitor();

  void Initialize();
  double GetCpuUsage();
  int GetMemoryUsagePercentage();

private:
  PDH_HQUERY cpuQuery;
  PDH_HCOUNTER cpuTotal;
};
