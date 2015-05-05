#include "FaustWebInterface.h"
#include "JsonParser.h"

#include <sstream>
#include <iostream>
#include <fstream> 
#include <curl/curl.h>

using namespace std;

#ifndef _WIN32
#include <libgen.h>
#else
#include <windows.h>

static char* basename(const char* fullpath)
{
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];

	_splitpath(fullpath, drive, dir, fname, ext);

	string fullname = fullpath;
	size_t pos = fullname.rfind(fname);

	return (char*)&fullpath[pos];
}

#endif

static string path_to_content(const string& path)
{
    ifstream file(path.c_str(), ifstream::binary);
    
    file.seekg (0, file.end);
    int size = file.tellg();
    file.seekg (0, file.beg);
    
    // And allocate buffer to that a single line can be read...
    char* buffer = new char[size + 1];
    file.read(buffer, size);
    
    // Terminate the string
    buffer[size] = 0;
    string result = buffer;
    file.close();
    delete [] buffer;
    return result;
}

// Standard Callback to store a server response in strinstream
static size_t store_Response(void* buf, size_t size, size_t nmemb, void* userp)
{
    ostream* os = static_cast<ostream*>(userp);
    streamsize len = size * nmemb;
    return (os->write(static_cast<char*>(buf), len)) ? len : 0;
}

// Standard Callback to store a server response in file
static size_t store_File(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

// Access to FaustWeb service - Sends a request to faustweb compilation service to know platforms and architecture supported by this
bool fw_get_available_targets(const string& url, vector<string>& platforms, map<string, vector<string> >& targets, string& error){
    
    string finalURL = url + "/targets";
    
    CURL *curl = curl_easy_init();
    
    bool isInitSuccessfull = false;
    
    if (curl) {
        
        ostringstream oss;
        
        curl_easy_setopt(curl, CURLOPT_URL, finalURL.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &store_Response);
        curl_easy_setopt(curl, CURLOPT_FILE, &oss);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60); 
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600);
        
        CURLcode res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            
            long respcode; //response code of the http transaction
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &respcode);
            
            if(respcode == 200){
                
                string response(oss.str());
                const char* json = response.c_str();
                     
                if (parseOperatingSystemsList(json, platforms, targets)) {
                    isInitSuccessfull = true;
                } else {
                    error = "Targets Could not be parsed.";
                }
            }
        } else {
            error = "Impossible to start connection";
        }
        curl_easy_cleanup(curl);
    } else {
        error = "Impossible to start connection";
    }
    
    return isInitSuccessfull;
}


// Access to FaustWeb service - Upload your faust application given a sourceFile, an operating system and an architecture
bool fw_export_string(const string& url, const string& name, 
                    const string& code, const string& os, 
                    const string& architecture, 
                    const string& output_type, 
                    const string& output_file, 
                    string& error) {

    string key("");
    
    if (fw_get_shakey_from_string(url, name, code, key, error)) {
        return fw_get_file_from_shakey(url, key, os, architecture, output_type, output_file, error);
    } else {
        return false;
    }
}

// Access to FaustWeb service - Upload your faust application given a sourceFile, an operating system and an architecture
bool fw_export_file(const string& url, 
                    const string& filename, 
                    const string& os, 
                    const string& architecture, 
                    const string& output_type, 
                    const string& output_file, 
                    string& error) {

	string base = basename((char*)filename.c_str());
    size_t pos = base.find(".dsp");
           
    if (pos != string::npos) {
    	return fw_export_string(url, base.substr(0, pos), path_to_content(filename), os, architecture, output_type, output_file, error);	
    }
    
    error = "The extension of the file is incorrect. It should be .dsp or .zip";
    return false;
}

// Access to FaustWeb service - Post your faust file and get a corresponding SHA-Key
bool fw_get_shakey_from_string(const string& url, const string& name, const string& code, string& key, string& error){

    CURL *curl = curl_easy_init();
    
    string boundary = "87142694621188";
    
    string data = "--" + boundary + "\r\n";
    data += "Content-Disposition: form-data; name=\"file\"; filename=\"";
    data += name;
    data += ".dsp\";\r\nContent-Type: text/plain\r\n\r\n";
    data += code;
    data += "\r\n--" + boundary + "--\r\n";
   
    string h1 = "Content-Type: multipart/form-data; boundary=" + boundary;
    stringstream h2; h2 << "Content-Length:" << data.length();

    struct curl_slist *headers=NULL;
    headers = curl_slist_append(headers, h1.c_str());
    headers = curl_slist_append(headers, h2.str().c_str());
    
    bool isInitSuccessfull = false;
    
    if (curl) {
        
        ostringstream oss;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &store_Response);
        curl_easy_setopt(curl, CURLOPT_FILE, &oss);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT , 60); 
        curl_easy_setopt(curl,CURLOPT_TIMEOUT, 600);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            
        CURLcode res = curl_easy_perform(curl);
  
        if (res != CURLE_OK) {
            error = oss.str();
        } else {
            
            long respcode; //response code of the http transaction
            curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE, &respcode);
            
            if (respcode == 200) {
                key = oss.str();
                isInitSuccessfull = true;
            } else if(respcode == 400) {
                error = "Impossible to generate SHA Key";
            }
        }
        
        curl_easy_cleanup(curl);
    } else {
        error = "Connection Impossible To Start";
    }
    
    return isInitSuccessfull;
}

bool fw_get_shakey_from_file(const string& url, const string& filename, string& key, string& error) {
		
	string base = basename((char*)filename.c_str());
    size_t pos = base.find(".dsp");

    if (pos != string::npos) {
		return fw_get_shakey_from_string(url, base.substr(0, pos), path_to_content(filename), key, error);
	}
    
    error = "The extension of the file is incorrect. It should be .dsp or .zip";
    return false;
}

// Access to FaustWeb service - Upload your faust application given the SHA-Key, an operating system and an architecture
bool fw_get_file_from_shakey(const string& url, 
                            const string& key, 
                            const string& os, 
                            const string& architecture, 
                            const string& output_type, 
                            const string& output_file, 
                            string& error) {
    
    bool isInitSuccessfull = false;
    string finalURL = url + "/" + key + "/" + os + "/" + architecture + "/" + output_type;
    FILE* file = fopen(output_file.c_str(), "w");
    
    if (file) {
        
        CURL *curl = curl_easy_init();
        
        if (curl) {
            
            curl_easy_setopt(curl, CURLOPT_URL, finalURL.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &store_File);
            curl_easy_setopt(curl, CURLOPT_FILE, file);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT , 60); 
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600);
            
            CURLcode res = curl_easy_perform(curl);
            
            if (res == CURLE_OK) {
                
                long respcode; //response code of the http transaction
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &respcode);
                
                if (respcode == 200) {
                    isInitSuccessfull = true;
                } else {
                    error = "Impossible to write file";
                }
            } else {
                error = "Compilation failed";
            }
            
            curl_easy_cleanup(curl);
        } else {
            error = "Connection impossible to start";
        }
        
        fclose(file);
    } else {
        error = "Impossible to open " + output_file;
    }
    
    return isInitSuccessfull;
}


