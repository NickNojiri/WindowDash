#ifndef BOOKMARKMANAGER_H
#define BOOKMARKMANAGER_H

#include <string>
#include <vector>

class BookmarkManager {
public:
  BookmarkManager();
  ~BookmarkManager();

  void AddBookmark(const std::string &path);
  void RemoveBookmark(int index);
  void ExecuteAll();
  const std::vector<std::string> &GetBookmarks() const;

  void Save(const std::string &filename);
  void Load(const std::string &filename);

private:
  std::vector<std::string> m_bookmarks;
};

#endif // BOOKMARKMANAGER_H
