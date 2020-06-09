#include <string>

namespace wotreplay
{
  struct Version
  {
    static const std::string BUILD_VERSION;
    static const std::string BUILD_DATE;
    static const std::string GIT_SHA1;
    static const std::string GIT_DATE;
    static const std::string GIT_COMMIT_SUBJECT;
  };
}