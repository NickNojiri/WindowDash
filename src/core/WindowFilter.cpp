#include "core/WindowFilter.h"

namespace core {

bool IsExcludedTitle(const std::string &title) {
  static const char *const kExcluded[] = {
      "Program Manager", "Settings", "Microsoft Text Input Application",
      "Window Manager", "Window Dash"};
  for (const char *t : kExcluded)
    if (title == t)
      return true;
  return false;
}

std::string DisplayName(const std::string &path) {
  auto slash = path.find_last_of("\\/");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

} // namespace core
