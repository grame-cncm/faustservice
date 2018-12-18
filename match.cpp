#include "match.hh"
#include <iostream>
#include <string>
#include <vector>

// Boost libraries
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace std;

//----------------------------------------------------------------
// simplifyURL(), remove duplicated '/' in the URL

string simplifyURL(const char* url)
{
    const char* p = url;  // current char in url
    int         n = 0;    // number of successive '/'
    string      r;        // resulting simplified URL

    for (char c = *p; c != 0; c = *(++p)) {
        n = (c == '/') ? n + 1 : 0;
        if (n < 2) r += c;
    }
    cerr << "Simplify url " << url << " --> " << r << endl;
    return r;
}

//----------------------------------------------------------------
// decomposeURL(), decompose an URL into a vector of strings.
// Used internally by matchURL. Trailing / are removed

vector<string> decomposeURL(const string& url)
{
    boost::filesystem::path U(url);
    vector<string>          decomposition;
    for (auto n : U) {
        string s = n.string();
        if (s != ".") decomposition.push_back(s);
    }
    return decomposition;
}

//----------------------------------------------------------------
// matchURL() returns true if the url and the pattern match
// To match they must have the same number of elements
// and all these elements must be identical, or wildcards ( '*' ).
// Data contains the decomposition of the URL

bool matchURL(const string& url, const char* pat, vector<string>& data)
{
    vector<string> U = decomposeURL(url);
    vector<string> P = decomposeURL(pat);
    if (P.size() == U.size()) {
        for (size_t i = 0; i < P.size(); i++) {
            if ((P[i] != "*") && (P[i] != U[i])) {
                return false;
            }
        }
        data = U;
        cout << "PATTERN " << pat << " MATCHES URL " << url << endl;
        return true;
    } else {
        return false;
    }
}

//----------------------------------------------------------------
// matchURL() returns true if the url and the pattern match.
// To match they must have the same number of elements
// and all these elements must be identical, or wildcards ( '*' ).

bool matchURL(const string& url, const char* pat)
{
    vector<string> ignore;
    bool           r = matchURL(url, pat, ignore);
    if (r)
        cout << "MATCH " << pat << " <== " << url << endl;
    else
        cout << "DONT MATCH " << pat << " <== " << url << endl;
    return r;
}
