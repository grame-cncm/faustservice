#include <boost/filesystem.hpp>
#include <iostream>
#include <list>
#include <map>

namespace fs = boost::filesystem;

// LRUSessionsCache : a system that limits the number of cached sessions
class LRUSessionsCache {
    typedef std::map<fs::path, std::list<fs::path>::iterator> PosMap;

    const fs::path      fSessionsDir;   // directory containing all the sessions
    int                 fMaxSize;       // maximum capacity of sessions
    std::list<fs::path> fSessionsList;  // list of sessions
    PosMap              fSessionPos;    // session position in the list

   public:
    LRUSessionsCache(const fs::path& aSessionsDir, int aMaxSize);

    void refer(const fs::path& aSession);    // refer to a session in the cache
    void dispose(const fs::path& aSession);  // LRU session is removed from the cache
};
