#include <sstream>
#include <dirent.h>

#include "utilities.hh"

vector<string>
getdir(string dir)
{
	DIR *dp;
	vector<string> files;
	struct dirent *dirp;
	if ((dp = opendir(dir.c_str())) == NULL) {
		return files;
	}

	while ((dirp = readdir(dp)) != NULL) {
		files.push_back(string(dirp->d_name));
	}
	closedir(dp);
	return files;
}
