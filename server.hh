#ifndef _SERVER_
#define _SERVER_

#include "microhttpd.h"
#include "utilities.hh"

using namespace std;

struct connection_info_struct
{
  int connectiontype;
  struct MHD_PostProcessor *postprocessor;
  FILE *fp;
  string tmppath;
  string answerstring;
  int answercode;
};

struct string_and_exitstatus
{
  string str;
  int exitstatus;
};

class FaustServer {
  int port_;
  struct MHD_Daemon* daemon_;

  static unsigned int nr_of_uploading_clients;
  static int faustGet (struct MHD_Connection *connection, TArgs &args);
  static int get_params (void *cls, enum MHD_ValueKind , const char *key, const char *data);
  static int send_page (struct MHD_Connection *connection, string page, int status_code);
  static int iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data, uint64_t off,
              size_t size);
  static void request_completed (void *cls, struct MHD_Connection *connection,
                   void **con_cls, enum MHD_RequestTerminationCode toe);
  static int answer_to_connection (void *cls, struct MHD_Connection *connection,
                       const char *url, const char *method,
                       const char *version, const char *upload_data,
                       size_t *upload_data_size, void **con_cls);

  public :
    FaustServer (int port);
    virtual ~FaustServer() {};
    bool start ();
    void stop ();
};

#endif