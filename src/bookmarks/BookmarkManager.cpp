#include "bookmarks/BookmarkManager.h"
#include <fstream>
#include <windows.h>

BookmarkManager::BookmarkManager() { Load("bookmarks.dat"); }

BookmarkManager::~BookmarkManager() { Save("bookmarks.dat"); }

void BookmarkManager::AddBookmark(const std::string &path) {
  m_bookmarks.push_back(path);
  Save("bookmarks.dat");
}

void BookmarkManager::RemoveBookmark(int index) {
  if (index >= 0 && index < static_cast<int>(m_bookmarks.size())) {
    m_bookmarks.erase(m_bookmarks.begin() + index);
    Save("bookmarks.dat");
  }
}

void BookmarkManager::ExecuteAll() {
  for (const auto &path : m_bookmarks) {
    ShellExecute(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
  }
}

const std::vector<std::string> &BookmarkManager::GetBookmarks() const {
  return m_bookmarks;
}

void BookmarkManager::Save(const std::string &filename) {
  std::ofstream outfile(filename);
  if (outfile.is_open()) {
    for (const auto &path : m_bookmarks) {
      outfile << path << "\n";
    }
    outfile.close();
  }
}

void BookmarkManager::Load(const std::string &filename) {
  std::ifstream infile(filename);
  if (infile.is_open()) {
    m_bookmarks.clear();
    std::string line;
    while (std::getline(infile, line)) {
      if (!line.empty()) {
        m_bookmarks.push_back(line);
      }
    }
    infile.close();
  }
}
