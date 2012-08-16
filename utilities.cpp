#include <sstream>
#include <dirent.h>

#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "utilities.hh"

string
boost_random (int size)
{
    stringstream ss;
    std::string chars (
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "1234567890");

    boost::random::random_device rng;
    boost::random::uniform_int_distribution<> index_dist (0, chars.size () - 1);
    for (int i = 0; i < 8; ++i) {
        ss << chars[index_dist (rng)];
    }
    return ss.str ();
}

vector<string>
getdir (string dir)
{
    DIR *dp;
    vector<string> files;
    struct dirent *dirp;
    if ((dp  = opendir (dir.c_str ())) == NULL) {
        return files;
    }

    while ((dirp = readdir (dp)) != NULL) {
        files.push_back (string (dirp->d_name));
    }
    closedir (dp);
    return files;
}
