#ifndef FAUSTWEBINTERFACE_H
#define FAUSTWEBINTERFACE_H

#include <string>
#include <vector>
#include <map>

using namespace std;

/* Access to FaustWeb service - Requests the supported platforms and architecture of a faustweb service
 * 
 *
 * @param url - FaustWeb server URL 
 * @param platforms - If the research is sucessfull, the vector is filled with available platforms
 * @param targets - Associates a platform with a vector of available architectures
 * @param error - In case the research fails, the error is filled
 *
 * @return true if no error was encountered
 */
bool fw_get_available_targets(const std::string& url, std::vector<std::string>& platforms, std::map<std::string, std::vector<std::string> >& targets, string& error);

//---------------------EXPORT IN 1 FUNCTION CALL

/* Access to FaustWeb service - Upload your Faust application given a sourceFile, an operating system and an architecture
 *
 * @param url - FaustWeb server URL
 * @param filename - Faust source file
 * @param os - Wanted Operating System
 * @param architecture - Wanted architecture
 * @param output_type - There are 3 types of files : "binary.zip", "binary.apk" and "src.cpp"
 * @param output_file - Location of file to create
 * @param error - In case the export fails, the error is filled
 *
 * @return true if no error was encountered
 */
bool fw_export_file(const std::string& url, const std::string& filename, const std::string& os, const std::string& architecture, const std::string& output_type, const std::string& output_file, std::string& error);

/* Access to FaustWeb service - Upload your Faust application given a program name, a program, an operating system and an architecture
 *
 * @param url - FaustWeb server URL
 * @param name - Name of the program you compile
 * @param pgm - Faust program as a string
 * @param os - Wanted Operating System
 * @param architecture - Wanted architecture
 * @param output_type - There are 3 types of files : "binary.zip", "binary.apk" and "src.cpp"
 * @param output_file - Location of file to create
 * @param error - In case the export fails, the error is filled
 *
 * @return true if no error was encountered
 */
bool fw_export_string(const std::string& url, const std::string& name, const std::string& pgm, const std::string& os, const std::string& architecture, const std::string& output_type, const std::string& output_file, std::string& error);


//---------------------EXPORT DIVIDED IN 2 FUNCTION CALLS

/* Access to FaustWeb service - Post your Faust file and get the corresponding SHA-Key
 *
 * @param url - FaustWeb server URL
 * @param file - Faust source file
 * @param key - Output SHA-Key
 * @param error - In case the export fails, the error is filled
 *
 * @return true if no error was encountered
 */
bool fw_get_shakey_from_file(const std::string& url, const std::string& file, std::string& key, std::string& error);

/* Access to FaustWeb service - Post your Faust program and get the corresponding SHA-Key
 *
 * @param url - FaustWeb server URL
 * @param name - Name of the program you compile
 * @param pgm - Faust program as a string
 * @param key - Output SHA-Key
 * @param error - In case the export fails, the error is filled
 *
 * @return true if no error was encountered
 */
bool fw_get_shakey_from_string(const std::string& url, const std::string& name, const std::string& pgm, std::string& key, std::string& error);

/* Access to FaustWeb service - Upload your Faust application given the SHA-Key, an operating system and an architecture
 *
 *  Multiple requests can be sent with the same SHA-Key
 *
 * @param url - FaustWeb server URL
 * @param key - SHA-Key corresponding to your Faust code
 * @param os - Wanted Operating System
 * @param architecture - Wanted architecture
 * @param output_file - Location of file to create
 * @param output_type - There are 3 types of files : "binary.zip", "binary.apk" and "src.cpp"
 * @param error - In case the export fails, the error is filled
 *
 * @return true if no error was encountered
 */
bool fw_get_file_from_shakey(const std::string& url, const std::string& key, const std::string& os, const std::string& architecture, const std::string& output_type, const std::string& output_file, std::string& error);

#endif

