#include "sessioncache.hh"

SessionCache::SessionCache(const fs::path& sessionDir, int size) : fSessionsDir(sessionDir), fMaxSize(size)
{
    for (const auto& x : fs::directory_iterator(fSessionsDir)) {
        refer(x.path().filename());
    }
}

/* Refers key x with in the LRU cache */
void SessionCache::refer(const fs::path& x)
{
    std::cerr << "REFER TO " << x << std::endl;

    // not present in cache
    if (fCachePos.find(x) == fCachePos.end()) {
        // cache is full
        if (fCacheList.size() == fMaxSize) {
            // delete least recently used element
            fs::path last = fCacheList.back();
            fCacheList.pop_back();
            fCachePos.erase(last);
            dispose(last);
        }
    } else {
        fCacheList.erase(fCachePos[x]);
    }

    // update reference
    fCacheList.push_front(x);
    fCachePos[x] = fCacheList.begin();
}

// dispose an element that leave the cache
void SessionCache::dispose(const fs::path& s)
{
    std::cerr << "DISPOSE OF " << s << std::endl;
    try {
        fs::remove_all(fSessionsDir / s);
    } catch (const boost::filesystem::filesystem_error& e) {
        std::cerr << "ERROR REMOVING CACHE ENTRY " << s << " : " << e.code().message() << std::endl;
    }
}
