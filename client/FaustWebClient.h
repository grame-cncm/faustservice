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

#ifndef FAUSTWEB_CLIENT_H
#define FAUSTWEB_CLIENT_H

#include <string>
#include <vector>
#include <map>

/* Access to FaustWeb service - Requests the supported platforms and architecture of a FaustWeb service
 * 
 * @param url - FaustWeb server URL
 * @param targets - Associates a platform with a vector of available architectures
 * @param error - In case the research fails, the error is filled
 *
 * @return true if no error was encountered
 */
bool fw_get_available_targets(const std::string& url, std::map<std::string, std::vector<std::string> >& targets, std::string& error);

//---------------------EXPORT IN 1 FUNCTION CALL

/* Access to FaustWeb service - Get the Faust application given a sourcefile, an operating system and an architecture
 *
 * @param url - FaustWeb server URL
 * @param filename - Faust source file
 * @param os - Wanted Operating System
 * @param architecture - Wanted architecture
 * @param output_type - There are 4 types of files : "binary.zip", "binary.apk", "installer.sh" and "src.cpp"
 * @param output_file - Location of file to create
 * @param error - In case the export fails, the error is filled
 *
 * @return true if no error was encountered
 */
bool fw_export_file(const std::string& url, const std::string& filename, const std::string& os, const std::string& architecture, const std::string& output_type, const std::string& output_file, std::string& error);

/* Access to FaustWeb service - Get the Faust application given a program name, a program, an operating system and an architecture
 *
 * @param url - FaustWeb server URL
 * @param name - Name of the program you compile
 * @param pgm - Faust program as a string
 * @param os - Wanted Operating System
 * @param architecture - Wanted architecture
 * @param output_type - There are 4 types of files : "binary.zip", "binary.apk", "installer.sh" and "src.cpp"
 * @param output_file - Location of file to create
 * @param error - In case the export fails, the error is filled
 *
 * @return true if no error was encountered
 */
bool fw_export_string(const std::string& url, const std::string& name, const std::string& pgm, const std::string& os, const std::string& architecture, const std::string& output_type, const std::string& output_file, std::string& error);

//---------------------EXPORT DIVIDED IN 2 FUNCTION CALLS

/* Access to FaustWeb service - Post the Faust DSP as a file and get the corresponding SHA-Key
 *
 * @param url - FaustWeb server URL
 * @param file - Faust source file
 * @param key - Output SHA-Key
 * @param error - In case the export fails, the error is filled
 *
 * @return true if no error was encountered
 */
bool fw_get_shakey_from_file(const std::string& url, const std::string& file, std::string& key, std::string& error);

/* Access to FaustWeb service - Post the Faust DSP as a string and get the corresponding SHA-Key
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

/* Access to FaustWeb service - Get your Faust application given the SHA-Key, an operating system and an architecture
 *
 *  Multiple requests can be sent with the same SHA-Key
 *
 * @param url - FaustWeb server URL
 * @param key - SHA-Key corresponding to your Faust code
 * @param os - Wanted Operating System
 * @param architecture - Wanted architecture
 * @param output_file - Location of file to create
 * @param output_type - There are 4 types of files : "binary.zip", "binary.apk", "installer.sh" and "src.cpp"
 * @param error - In case the export fails, the error is filled
 *
 * @return true if no error was encountered
 */
bool fw_get_file_from_shakey(const std::string& url, const std::string& key, const std::string& os, const std::string& architecture, const std::string& output_type, const std::string& output_file, std::string& error);

#endif

