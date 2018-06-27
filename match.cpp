#include "match.hh"
#include <vector>
#include <string>
#include <iostream>

// Boost libraries
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace std;

void segmentUrl(const char* url)
{
    boost::filesystem::path U(url);
    cout << "SEGMENT URL: ";
    for (auto n : U) {
        cout << "[" << n.string() << "] ";
    }
    cout << endl;
}

vector<string> decomposeURL(const char* url)
{
    boost::filesystem::path U(url);
    vector<string> decomposition;
    for (auto n : U) {
        decomposition.push_back(n.string());
    }
    return decomposition;
}

bool matchURL(const char* url, const char* pat)
{
    vector<string> U = decomposeURL(url);
    vector<string> P = decomposeURL(pat);
    if (P.size() <= U.size()) {
        for (size_t i = 0; i < P.size(); i++) {
            if ((P[i] != "*") && (P[i] != U[i])) { return false; }
        }
        return true;
    } else {
        return false;
    }
}

bool matchURL(const char* url, const char* pat, vector<string>& data)
{
    vector<string> U = decomposeURL(url);
    vector<string> P = decomposeURL(pat);
    if (P.size() <= U.size()) {
        for (size_t i = 0; i < P.size(); i++) {
            if ((P[i] != "*") && (P[i] != U[i])) { return false; }
        }
        data = U;
        return true;
    } else {
        return false;
    }
}
