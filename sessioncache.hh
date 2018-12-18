#include <boost/filesystem.hpp>
#include <iostream>
#include <list>
#include <map>

namespace fs = boost::filesystem;

class SessionCache {
    const fs::path      fSessionsDir;
    int                 fMaxSize;    // maximum capacity of cache
    std::list<fs::path> fCacheList;  // store keys of cache

    // store references of key in cache
    std::map<fs::path, std::list<fs::path>::iterator> fCachePos;

   public:
    SessionCache(const fs::path& sessionDir, int size);
    void refer(const fs::path&);    // refer to an element in the cache
    void dispose(const fs::path&);  // this element is removed from the cache
};
