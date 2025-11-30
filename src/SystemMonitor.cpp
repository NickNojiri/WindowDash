#include "SystemMonitor.h"
#include <iostream>

SystemMonitor::SystemMonitor() : cpuQuery(NULL), cpuTotal(NULL) {}

SystemMonitor::~SystemMonitor() {
  if (cpuQuery) {
    PdhCloseQuery(cpuQuery);
  }
}

void SystemMonitor::Initialize() {
  PdhOpenQuery(NULL, 0, &cpuQuery);
  // Use 0 instead of NULL for the 3rd argument (DWORD_PTR) to avoid
  // warnings/errors
  PdhAddEnglishCounter(cpuQuery, "\\Processor(_Total)\\% Processor Time", 0,
                       &cpuTotal);
  PdhCollectQueryData(cpuQuery);
}

double SystemMonitor::GetCpuUsage() {
  PDH_FMT_COUNTERVALUE counterVal;

  PdhCollectQueryData(cpuQuery);
  PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
  return counterVal.doubleValue;
}

int SystemMonitor::GetMemoryUsagePercentage() {
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  GlobalMemoryStatusEx(&memInfo);
  return memInfo.dwMemoryLoad;
}
