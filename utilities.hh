#ifndef _UTILITIES_
#define _UTILITIES_

#include <map>
#include <vector>
#include <string>

using namespace std;

#define POSTBUFFERSIZE 512

#define GET 0
#define POST 1

typedef map<string, string> TArgs;

string boost_random(int size);
vector<string> getdir(string dir);

#endif