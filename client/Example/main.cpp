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
#include "../FaustWebClient.h"

using namespace std;

int main(int argc, char **argv)
{
    string err;
    map<string, vector<string> > targets;
    map<string, vector<string> >::iterator it;
    
    string url = "http://faustservice.grame.fr";
    bool res = fw_get_available_targets(url, targets, err);
    
    for (it = targets.begin(); it != targets.end(); it++) {
 		cout << "Platform : " << (*it).first << endl;
        vector<string> target = (*it).second;
        for (int j = 0; j < target.size(); j++) {
     		cout << "Target : " << target[j] << endl;
        }
    }

    string key;
    string file = "test.dsp";

    if (fw_get_shakey_from_file(url, "test.dsp", key, err)){
    	cout << "SHAKey is " << key << endl;
        if (fw_get_file_from_shakey(url, key, "osx", "coreaudio-qt", "binary.zip", "binary.zip", err)) {
            cout << "File written " << endl;
        } else {
            cout << err << endl;
        }
    } else {
        cout << "Bad request" << endl;
    }

    return 0;
}
