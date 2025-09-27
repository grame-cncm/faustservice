#include "match.hh"
#include <iostream>
#include <string>
#include <vector>

// Standard filesystem library (C++17)
#include <filesystem>

namespace fs = std::filesystem;

extern int gVerbosity;

//----------------------------------------------------------------
// simplifyURL(), remove duplicated '/' in the URL

std::string simplifyURL(const char* url)
{
    const char* p = url;  // current char in url
    int         n = 0;    // number of successive '/'
    std::string r;        // resulting simplified URL

    for (char c = *p; c != 0; c = *(++p)) {
        n = (c == '/') ? n + 1 : 0;
        if (n < 2) r += c;
    }
    if (gVerbosity >= 2) std::cerr << "Simplify url " << url << " --> " << r << std::endl;
    return r;
}

//----------------------------------------------------------------
// decomposeURL(), decompose an URL into a vector of strings.
// Used internally by matchURL. Trailing / are removed

std::vector<std::string> decomposeURL(const std::string& url)
{
    fs::path U(url);
    std::vector<std::string> decomposition;
    for (auto n : U) {
        std::string s = n.string();
        if (s != ".") decomposition.push_back(s);
    }
    return decomposition;
}

//----------------------------------------------------------------
// matchURL() returns true if the url and the pattern match
// To match they must have the same number of elements
// and all these elements must be identical, or wildcards ( '*' ).
// Data contains the decomposition of the URL

bool matchURL(const std::string& url, const std::string& pat, std::vector<std::string>& data)
{
    std::vector<std::string> U = decomposeURL(url);
    std::vector<std::string> P = decomposeURL(pat);
    if (P.size() == U.size()) {
        for (size_t i = 0; i < P.size(); i++) {
            if ((P[i] != "*") && (P[i] != U[i])) {
                return false;
            }
        }
        data = U;
        if (gVerbosity >= 2) std::cout << "PATTERN " << pat << " MATCHES URL " << url << std::endl;
        return true;
    } else {
        return false;
    }
}

//----------------------------------------------------------------
// matchURL() returns true if the url and the pattern match.
// To match they must have the same number of elements
// and all these elements must be identical, or wildcards ( '*' ).

bool matchURL(const std::string& url, const std::string& pat)
{
    std::vector<std::string> ignore;
    bool           r = matchURL(url, pat, ignore);

    if (r && (gVerbosity >= 1))
        std::cout << "PATTERN " << pat << " MATCHES URL " << url << std::endl;
    if (!r && (gVerbosity >= 2))
        std::cout << "PATTERN " << pat << " DOES NOT MATCH URL " << url << std::endl;
    
    return r;
}

bool matchExtension(const std::string& url, const std::string& ext)
{
    size_t u = url.length();
    size_t e = ext.length();
    if (u > e) {
        size_t p = u - e;
        return url.find(ext, p) == p;
    } else {
        return false;
    }
}

bool matchBeginURL(const std::string& url, const std::string& pat)
{
    std::vector<std::string> U = decomposeURL(url);
    std::vector<std::string> P = decomposeURL(pat);
    if (P.size() <= U.size()) {
        for (size_t i = 0; i < P.size(); i++) {
            if ((P[i] != "*") && (P[i] != U[i])) {
                if (gVerbosity >= 2) std::cout << "PATTERN " << pat << " DOES NOT MATCH URL " << url << std::endl;
                return false;
            }
        }
        if (gVerbosity >= 2) std::cout << "PATTERN " << pat << " MATCHES URL " << url << std::endl;
        return true;
    } else {
        if (gVerbosity >= 2) std::cout << "PATTERN " << pat << " DOES NOT MATCH URL " << url << std::endl;
        return false;
    }
}
