// TESTING THE FAUSTWEB INTERFACE

#include <iostream>
#include "../FaustWebInterface.h"

using namespace std;

int main(int argc, char **argv) {

    string err;
    map<string, vector<string> > targets;
    map<string, vector<string> >::iterator it;
    
    string url = "http://faustservice.grame.fr";
    bool res = fw_get_available_targets(url, targets, err);
    
    for (it = targets.begin(); it != targets.end(); it++) {
 		cout << "Platform : " << (*it).first << endl;
        vector<string> target = (*it).second;
     	for (int j = 0; j < target.size(); j++)
     		cout << "Target : " << target[j] << endl;
    }

	string key;
	string file = "test.dsp";
	
    if (fw_get_shakey_from_file(url, "test.dsp", key, err)){
    	cout << "SHA KEY IS " << key << endl;
        if (fw_get_file_from_shakey(url, key, "osx", "coreaudio-qt", "binary.zip", "binary.zip", err))
            cout << "FILE WRITTEN" << endl;
        else
            cout << err << endl;
    } else {
         cout<<"BAD REQUEST"<<endl;
    }

    return 0;
}