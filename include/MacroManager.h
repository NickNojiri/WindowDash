#ifndef MACROMANAGER_H
#define MACROMANAGER_H

#include <string>
#include <vector>

class MacroManager {
public:
  MacroManager();
  ~MacroManager();

  void AddProgram(const std::string &path);
  void RemoveProgram(int index);
  void ExecuteAll();
  const std::vector<std::string> &GetPrograms() const;

  void Save(const std::string &filename);
  void Load(const std::string &filename);

private:
  std::vector<std::string> m_programs;
};

#endif // MACROMANAGER_H
