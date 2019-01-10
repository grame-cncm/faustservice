#include "LRUSessionsCache.hh"
extern int gVerbosity;

// LRUSessionsCache : a system that limits the number of cached sessions
LRUSessionsCache::LRUSessionsCache(const fs::path& aSessionsDir, int aMaxSize)
    : fSessionsDir(aSessionsDir), fMaxSize(aMaxSize)
{
    try {
        // init the cache using the actual sessions in the sessions directory
        if (fs::is_directory(fSessionsDir)) {
            for (const auto& session : fs::directory_iterator(fSessionsDir)) {
                refer(session.path().filename());
            }
        }
    } catch (const boost::filesystem::filesystem_error& e) {
        if (gVerbosity >= 2)
            std::cerr << "Warning : sessions directory " << fSessionsDir << " doesn't exist yet " << e.code().message()
                      << std::endl;
    }
}

/* Refers key x with in the LRU cache */
void LRUSessionsCache::refer(const fs::path& x)
{
    if (gVerbosity >= 2) std::cerr << "REFER TO " << x << std::endl;

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
    if (gVerbosity >= 2) std::cerr << "DISPOSE OF " << s << std::endl;
    try {
        fs::remove_all(fSessionsDir / s);
    } catch (const boost::filesystem::filesystem_error& e) {
        std::cerr << "ERROR REMOVING CACHE ENTRY " << s << " : " << e.code().message() << std::endl;
    }
}
