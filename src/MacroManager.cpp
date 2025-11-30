#include "MacroManager.h"
#include <fstream>
#include <windows.h>

MacroManager::MacroManager() { Load("macros.dat"); }

MacroManager::~MacroManager() { Save("macros.dat"); }

void MacroManager::AddProgram(const std::string &path) {
  m_programs.push_back(path);
  Save("macros.dat");
}

void MacroManager::RemoveProgram(int index) {
  if (index >= 0 && index < static_cast<int>(m_programs.size())) {
    m_programs.erase(m_programs.begin() + index);
    Save("macros.dat");
  }
}

void MacroManager::ExecuteAll() {
  for (const auto &path : m_programs) {
    ShellExecute(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
  }
}

const std::vector<std::string> &MacroManager::GetPrograms() const {
  return m_programs;
}

void MacroManager::Save(const std::string &filename) {
  std::ofstream outfile(filename);
  if (outfile.is_open()) {
    for (const auto &path : m_programs) {
      outfile << path << "\n";
    }
    outfile.close();
  }
}

void MacroManager::Load(const std::string &filename) {
  std::ifstream infile(filename);
  if (infile.is_open()) {
    m_programs.clear();
    std::string line;
    while (std::getline(infile, line)) {
      if (!line.empty()) {
        m_programs.push_back(line);
      }
    }
    infile.close();
  }
}
