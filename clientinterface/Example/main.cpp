// TESTING THE FAUSTWEB INTERFACE

#include <iostream>
#include "../FaustWebInterface.h"

using namespace std;

int main(int argc, char **argv) {

    string err;
    map<string, vector<string> > targets;
    vector<string> platforms;
    
    string url = "http://faustservice.grame.fr";
    bool res = fw_get_available_targets(url, platforms, targets, err);
    
    for (int i = 0; i < platforms.size(); i++) {
 		cout << "Platform : " << platforms[i] << endl;
     	for(int j = 0; j < targets[platforms[i]].size(); j++)
     		cout << "Target : " << targets[platforms[i]][j] << endl;
    }

	string key;
	string file = "test.dsp";
	
    if (fw_get_shakey_from_file(url, "test.dsp", key, err)){
    	cout<<"SHA KEY IS "<<key<<endl;
        if (fw_get_file_from_shakey(url, key, "osx", "coreaudio-qt", "binary.zip", "binary.zip", err))
            cout<<"FILE WRITTEN"<<endl;
        else
            cout<<err<<endl;
    } else {
         cout<<"BAD REQUEST"<<endl;
    }

    return 0;
}