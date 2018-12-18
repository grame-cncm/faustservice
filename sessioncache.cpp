/* We can use stl container list as a double
ended queue to store the cache keys, with
the descending time of reference from front
to back and a set container to check presence
of a key. But to fetch the address of the key
in the list using find(), it takes O(N) time.
    This can be optimized by storing a reference
    (iterator) to each key in a hash map. */

#include <boost/filesystem.hpp>
#include <boost/utility.hpp>
#include <iostream>
#include <list>
#include <string>
#include <unordered_map>

namespace fs = boost::filesystem;

#include "sessioncache.hh"

using namespace std;

SessionCache::SessionCache(const fs::path& sessionDir, int size) : fSessionDir(sessionDir), fMaxSize(size)
{
    initFromSessionDir();
}

/* Refers key x with in the LRU cache */
void SessionCache::refer(string x)
{
    cerr << "REFER TO " << x << endl;

    // not present in cache
    if (fCachePos.find(x) == fCachePos.end()) {
        // cache is full
        if (fCacheList.size() == fMaxSize) {
            // delete least recently used element
            string last = fCacheList.back();
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

// display contents of cache
void SessionCache::display()
{
    for (auto s : fCacheList) cout << s << " ";
    cout << endl;
}

// dispose an element that leave the cache
void SessionCache::dispose(string s)
{
    cerr << "DISPOSE OF " << s << endl;
    try {
        fs::remove_all(fSessionDir / s);
    } catch (const boost::filesystem::filesystem_error& e) {
        cerr << "***ERROR removing " << s << " : " << e.code().message() << endl;
    }
}

void SessionCache::initFromSessionDir()
{
    for (auto x : fs::directory_iterator(fSessionDir)) {
        refer(x.path().filename().string());
    }
}
