#include "LRUSessionsCache.hh"

// LRUSessionsCache : a system that limits the number of cached sessions
LRUSessionsCache::LRUSessionsCache(const fs::path& aSessionsDir, int aMaxSize)
    : fSessionsDir(aSessionsDir), fMaxSize(aMaxSize)
{
    // init the cache using the actual sessions in the sessions directory
    for (const auto& session : fs::directory_iterator(fSessionsDir)) {
        refer(session.path().filename());
    }
}

/* Refers key x with in the LRU cache */
void LRUSessionsCache::refer(const fs::path& x)
{
    std::cerr << "REFER TO " << x << std::endl;

    // not present in cache
    if (fSessionPos.find(x) == fSessionPos.end()) {
        // cache is full
        if (fSessionsList.size() == fMaxSize) {
            // delete least recently used element
            fs::path last = fSessionsList.back();
            fSessionsList.pop_back();
            fSessionPos.erase(last);
            dispose(last);
        }
    } else {
        fSessionsList.erase(fSessionPos[x]);
    }

    // update reference
    fSessionsList.push_front(x);
    fSessionPos[x] = fSessionsList.begin();
}

// dispose an element that leave the cache
void LRUSessionsCache::dispose(const fs::path& s)
{
    std::cerr << "DISPOSE OF " << s << std::endl;
    try {
        fs::remove_all(fSessionsDir / s);
    } catch (const boost::filesystem::filesystem_error& e) {
        std::cerr << "ERROR REMOVING CACHE ENTRY " << s << " : " << e.code().message() << std::endl;
    }
}
