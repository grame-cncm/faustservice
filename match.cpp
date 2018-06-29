#include "match.hh"
#include <iostream>
#include <string>
#include <vector>

// Boost libraries
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace std;

//----------------------------------------------------------------
// segmentUrl(), print the segments of an URL
// (used for debugging).

static void segmentUrl(const char* url)
{
    boost::filesystem::path U(url);
    cout << "SEGMENT URL: ";
    for (auto n : U) {
        cout << "[" << n.string() << "] ";
    }
    cout << endl;
}

//----------------------------------------------------------------
// decomposeURL(), decompose an URL into a vector of strings.
// Trailing / are removed

static vector<string> decomposeURL(const char* url)
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

bool matchURL(const char* url, const char* pat, vector<string>& data)
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

bool matchURL(const char* url, const char* pat)
{
    vector<string> ignore;
    return matchURL(url, pat, ignore);
}
