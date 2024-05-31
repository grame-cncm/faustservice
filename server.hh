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

#ifndef _FAUST_SERVER_
#define _FAUST_SERVER_

#include "LRUSessionsCache.hh"
#include "microhttpd.h"
#include "utilities.hh"

// Boost libraries
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace fs = boost::filesystem;

struct connection_info_struct {
    int                       connectiontype;  // GET or POST
    struct MHD_PostProcessor* postprocessor;   // the POST processor used internally by microhttpd
    FILE*                     fp;              // a pointer to the file to which the data is being written
    string                    tmppath;         // the path in which the provisional file exists
    string                    filename;        // the name of the file
    string                    answerstring;    // the answer sent to the user after upload
    int                       answercode;      // used internally by microhttpd to see where things went wrong or right
    string                    directory;       // the path in which the final file exists
    string                    makefile_directory;  // the path from which makefiles should be copied
};

struct string_and_exitstatus {
    string str;
    int    exitstatus;
};

class FaustServer {
    int                fPort;
    unsigned int       fMaxClients;
    fs::path           fDirectory;
    fs::path           fMakefileDirectory;
    fs::path           fLogfile;
    struct MHD_Daemon* fDaemon;
    string             fTargets;
    LRUSessionsCache   fSessionCache;

   public:
    FaustServer(int port, int max_clients, const fs::path& directory, const fs::path& makefile_directory,
                const fs::path& logfile, int maxSessions);

    virtual ~FaustServer() = default;
    ;

    bool start();
    void stop();

    unsigned int getMaxClients() const
    {
        return fMaxClients;  ///< Max number of clients allowed to connect at a given time.
    }

    fs::path getDirectory() const
    {
        return fDirectory;  ///< Directory to which the uploaded files are being written.
    }

    fs::path getMakefileDirectory() const
    {
        return fMakefileDirectory;  ///< Directory that the makefiles should be copied from.
    }

    fs::path getLogfile() const
    {
        return fLogfile;  ///< Path to the logfile.
    }

   private:
    static unsigned int nr_of_uploading_clients;

    static int get_params(void* cls, enum MHD_ValueKind, const char* key, const char* data);
    static int send_page(struct MHD_Connection* connection, const char* page, int length, int status_code,
                         const char* type);
    static int send_file(struct MHD_Connection* connection, const fs::path& filepath, const char* mimetype);
    static int iterate_post(void* coninfo_cls, enum MHD_ValueKind kind, const char* key, const char* filename,
                            const char* content_type, const char* transfer_encoding, const char* data, uint64_t off,
                            size_t size);

    static void request_completed(void* cls, struct MHD_Connection* connection, void** con_cls,
                                  enum MHD_RequestTerminationCode toe);
    static int  staticAnswerToConnection(void* cls, struct MHD_Connection* connection, const char* url,
                                         const char* method, const char* version, const char* upload_data,
                                         size_t* upload_data_size, void** con_cls);

    int dispatchGETConnections(struct MHD_Connection* connection, const string& url);
    int dispatchPOSTConnections(struct MHD_Connection* connection, const string& url, const char* upload_data,
                                size_t* upload_data_size, void** con_cls);

    int makeAndSendResourceFile(struct MHD_Connection* connection, const string& raw_url);
    std::string getMakefileArtifactName(const fs::path&);
};

#endif
