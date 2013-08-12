#ifndef _SERVER_
#define _SERVER_

#include "microhttpd.h"
#include "utilities.hh"

// Boost libraries
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace fs = boost::filesystem;


using namespace std;

struct connection_info_struct {
    int connectiontype; // GET or POST
    struct MHD_PostProcessor *postprocessor; // the POST processor used internally by microhttpd
    FILE *fp; // a pointer to the file to which the data is being written
    string tmppath; // the path in which the provisional file exists
    string filename; // the name of the file
    string answerstring; // the answer sent to the user after upload
    int answercode; // used internally by microhttpd to see where things went wrong or right
    string directory; // the path in which the final file exists
    string makefile_directory; // the path from which makefiles should be copied
};

struct string_and_exitstatus {
    string str;
    int exitstatus;
};

class FaustServer
{
    int port_;
    int max_clients_;
    fs::path directory_;
    fs::path makefile_directory_;
    fs::path logfile_;

    struct MHD_Daemon* daemon_;

    static unsigned int nr_of_uploading_clients;
    static int faustGet(struct MHD_Connection *connection, connection_info_struct *con_info, const char *raw_url, TArgs &args, const fs::path&  directory);
    static int get_params(void *cls, enum MHD_ValueKind, const char *key, const char *data);
    static int send_page(struct MHD_Connection *connection, const char *page, int length, int status_code, const char *type);
	static int send_file(struct MHD_Connection *connection, const fs::path& filepath);
    static int iterate_post(void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
                            const char *filename, const char *content_type,
                            const char *transfer_encoding, const char *data, uint64_t off,
                            size_t size);
    static void request_completed(void *cls, struct MHD_Connection *connection,
                                  void **con_cls, enum MHD_RequestTerminationCode toe);
    static int answer_to_connection(void *cls, struct MHD_Connection *connection,
                                    const char *url, const char *method,
                                    const char *version, const char *upload_data,
                                    size_t *upload_data_size, void **con_cls);

public:
    FaustServer(int port, int max_cleints, const fs::path& directory, const fs::path& makefile_directory, const fs::path& logfile);
    virtual ~FaustServer () {
    };
    const int getMaxClients();
    fs::path  getDirectory();
   	fs::path  getMakefileDirectory();
    fs::path  getLogfile();
    bool start();
    void stop();
};

#endif