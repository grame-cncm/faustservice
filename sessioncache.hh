#include <boost/filesystem.hpp>
#include <boost/utility.hpp>
#include <iostream>
#include <list>
#include <string>
#include <unordered_map>

namespace fs = boost::filesystem;

using namespace std;

class SessionCache {
    const fs::path fSessionDir;
    int            fMaxSize;    // maximum capacity of cache
    list<string>   fCacheList;  // store keys of cache

    // store references of key in cache
    unordered_map<string, list<string>::iterator> fCachePos;

   public:
    SessionCache(const fs::path& sessionDir, int size);
    void initFromSessionDir();  // init the cache by reading the session dir
    void refer(string);         // refer to an element in the cache
    void display();
    void dispose(string);  // this element is removed from the cache
};
