/************************************************************************
 FAUST Architecture File
 Copyright (C) 2017 GRAME, Centre National de Creation Musicale
 ---------------------------------------------------------------------
 This Architecture section is free software; you can redistribute it
 and/or modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 3 of
 the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; If not, see <http://www.gnu.org/licenses/>.
 
 EXCEPTION : As a special exception, you may create a larger work
 that contains this FAUST architecture section and distribute
 that work under terms of your choice, so long as this FAUST
 architecture section is not modified.
 
 ************************************************************************
 ************************************************************************/

#include <iostream>
#include <string.h>
#include <stdlib.h>

#include "FaustWebClient.h"

using namespace std;

static inline long lopt(char* argv[], const char* name, long def)
{
    int i;
    for (i = 0; argv[i]; i++) if (!strcmp(argv[i], name)) return atoi(argv[i+1]);
    return def;
}

static inline bool isopt(char* argv[], const char* name)
{
    int i;
    for (i = 0; argv[i]; i++) if (!strcmp(argv[i], name)) return true;
    return false;
}

static inline const char* lopts(char* argv[], const char* name, const char* def)
{
    int i;
    for (i = 0; argv[i]; i++) if (!strcmp(argv[i], name)) return argv[i+1];
    return def;
}

static inline string getExtension(const string& str)
{
    size_t pos = str.rfind('.');
    return (pos != string::npos) ? str.substr(pos) : "";
}

int main(int argc, char** argv)
{
    string url = lopts(argv, "-url", "http://faustservice.grame.fr");
    string platform = lopts(argv, "-platform", "osx");
    string target = lopts(argv, "-target", "coreaudio-qt");
    string type = lopts(argv, "-type", "binary.zip");
    bool is_service = isopt(argv, "-service");
    bool is_help1 = isopt(argv, "-help");
    bool is_help2 = isopt(argv, "-h");
    
    if (type != "binary.zip"
        && type != "binary.apk"
        && type != "installer.sh"
        && type != "src.cpp") {
        cout << "-type parameter is incorrect: use binary.zip, binary.apk, installer.sh, or src.cpp\n";
        return 0;
    }
    
    if (is_help1 || is_help2) {
        cout << "faustwebclient [-service] [-url <...>] [-platform <...>] [-target <...>] [-type <binary.zip, binary.apk, installer.sh, src.cpp>] <file.dsp>\n";
        cout << "Use '-service' to print all available platform/targets on the service URL (default 'http://faustservice.grame.fr')\n";
        cout << "Use '-url' to specify service URL (default 'http://faustservice.grame.fr')\n";
        cout << "Use '-platform' to specify compilation platform\n";
        cout << "Use '-target' to specify compilation target for the chosen platform\n";
        cout << "Use '-type' to specify type of result file: binary.zip, binary.apk, installer.sh, or src.cpp (default 'binary.zip')\n";
        return 0;
    }
  
    string err;
    
    if (is_service) {
        cout << "==========================" << endl;
        cout << "Available platform/targets" << endl;
        cout << "==========================" << endl;
        map<string, vector<string> > targets;
        map<string, vector<string> >::iterator it;
        bool res = fw_get_available_targets(url, targets, err);
        for (it = targets.begin(); it != targets.end(); it++) {
            cout << "Platform : " << (*it).first << endl;
            vector<string> target = (*it).second;
            for (int j = 0; j < target.size(); j++) {
                cout << "\tTarget : " << target[j] << endl;
            }
        }
        return 0;
    }

    string file = argv[argc - 1];
    
    cout << "===========================================" << endl;
    cout << "Service url : " << url << endl;
    cout << "Platform : " << platform << endl;
    cout << "Target : " << target << endl;
    cout << "Result type : " << type << endl;
    cout << "File : " << file << endl;
    cout << "===========================================" << endl;

    /*
    // Two steps access: first get the SHAKey, then get the file
    string key;
    if (fw_get_shakey_from_file(url, file, key, err)) {
    	cout << "SHAKey is " << key << endl;
        if (fw_get_file_from_shakey(url, key, platform, target, type, file + getExtension(type), err)) {
            cout << "File correctly written " << endl;
        } else {
            cout << err << endl;
        }
    } else {
        cout << "Bad request" << endl;
    }
    */
    
    // One step access
    string key;
    if (fw_export_file(url, file, platform, target, type, file + getExtension(type), err)) {
        cout << "File correctly written " << endl;
    } else {
        cout << err << endl;
    }

    return 0;
}
