#include <sstream>

#include <microhttpd.h>

#include "utilities.hh"
#include "server.hh"

using namespace std;

string askpage_head = "<html><body>\n\
                       Upload a Faust file, please.<br>\n\
                       There are ";


string askpage_tail = " clients uploading at the moment.<br>\n\
                       <form action=\"/filepost\" method=\"post\" enctype=\"multipart/form-data\">\n\
                       <input name=\"file\" type=\"file\">\n\
                       <input type=\"submit\" value=\" Send \"></form>\n\
                       </body></html>";


string cannotcompile =
  "<html><body>Could not execute the provided DSP program for the given architecture file.</body></html>";

string nosha1present =
  "<html><body>The given SHA1 key is not present in the directory.</body></html>";

string busypage =
  "<html><body>This server is busy, please try again later.</body></html>";

string completebuterrorpage =
  "<html><body>The upload has been completed but your Faust file is corrupt. It has not been registered.</body></html>";

string completebutnohash =
  "<html><body>The upload is completed but we could not generate a hash for your file.</body></html>";

string completebutcorrupt_head =
  "<html><body><p>The upload is completed but the file you uploaded is not a valid Faust file. \
  Make sure that it is either a file with an extension .dsp or an archive (tar.gz, tar.bz, tar \
  or zip) containing one .dsp file and potentially .lib files included by the .dsp file. \
  Furthermore, the code in these files must be valid faust code.</p> \
  <p>Below is the STDOUT and STDERR for the Faust code you tried to compile. \
  If the two are empty, then your file structure is wrong. Otherwise, they will tell you \
  why Faust failed.</p>";

string completebutcorrupt_tail =
  "</body></html>";

string completebutalreadythere_head =
  "<html><body>The upload is completed but it looks like you have already uploaded this file.<br />Here is its SHA1 key: ";

string completebutalreadythere_tail =
  "<br />Use this key for all subsequent GET commands.</body></html>";

string completepage_head =
  "<html><body>The upload is completed.<br />Here is its SHA1 key: ";

string completepage_tail =
  "<br />Use this key for all subsequent GET commands.</body></html>";

string errorpage =
  "<html><body>This doesn't seem to be right.</body></html>";

string servererrorpage =
  "<html><body>An internal server error has occured.</body></html>";

string fileexistspage =
  "<html><body>This file already exists.</body></html>";

int
validate_faust (connection_info_struct *con_info)
{
  FILE *pipe = popen (("python validate_faust.py "+con_info->tmppath).c_str (), "r");
  string result = "";
  if (!pipe)
    con_info->answerstring = completebuterrorpage;
  else
    {
      // Bleed off the pipe
      char buffer[128];
      while (!feof (pipe))
        {
          if (fgets (buffer, 128, pipe) != NULL)
            result += buffer;
        }
    }
  // for debugging, add print commands to the python script
  // and do cout << result << endl;
  int exitstatus = pclose (pipe);
  if (exitstatus)
    con_info->answerstring = completebutcorrupt_head + result + completebutcorrupt_tail;
  return exitstatus;
}

string_and_exitstatus
generate_sha1 (connection_info_struct *con_info)
{
  FILE *pipe = popen (("python generate_sha1.py "+con_info->tmppath).c_str (), "r");
  string result = "";
  if (!pipe)
    {
      con_info->answerstring = completebuterrorpage;
    }
  else
    {
      char buffer[128];
      while (!feof (pipe))
        {
          if (fgets (buffer, 128, pipe) != NULL)
            result += buffer;
        }
    }
  string_and_exitstatus res;
  res.exitstatus = pclose (pipe);
  res.str = result;
  if (res.exitstatus)
    con_info->answerstring = completebutnohash;
  return res;
}

int
make_initial_faust_directory (connection_info_struct *con_info, string sha1, string original_filename)
{
  FILE *pipe = popen (("python make_initial_faust_directory.py "+con_info->tmppath+" "+sha1+" "+original_filename).c_str (), "r");
  string result = "";
  if (!pipe)
    {
      con_info->answerstring = completebuterrorpage;
    }
  else
    {
      // Bleed off the pipe
      char buffer[128];
      while (!feof (pipe))
        {
          if (fgets (buffer, 128, pipe) != NULL)
            result += buffer;
        }
    }
  // for debugging, add print commands to the python script
  // and do cout << result << endl;
  int exitstatus = pclose (pipe);
  if (exitstatus)
     con_info->answerstring = completebutalreadythere_head + sha1 + completebutalreadythere_tail;
  else
     con_info->answerstring = completepage_head + sha1 + completepage_tail;

  return exitstatus;
}

int
FaustServer::get_params (void *cls, enum MHD_ValueKind , const char *key, const char *data)
{
  TArgs* args = (TArgs*)cls;
  args->insert (pair<string,string> (string (key), string (data)));
  return MHD_YES;
}

int
FaustServer::send_page (struct MHD_Connection *connection, string page,
           int status_code)
{
  int ret;
  struct MHD_Response *response;

  response =
    MHD_create_response_from_buffer (page.size (), (void *) page.c_str (),
				     MHD_RESPMEM_PERSISTENT);
  if (!response)
    return MHD_NO;

  ret = MHD_queue_response (connection, status_code, response);
  MHD_destroy_response (response);

  return ret;
}

int
FaustServer::iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
              const char *filename, const char *content_type,
              const char *transfer_encoding, const char *data, uint64_t off,
              size_t size)
{
  struct connection_info_struct *con_info = (connection_info_struct *)coninfo_cls;
  FILE *fp;

  if (con_info->tmppath.empty ())
    con_info->tmppath = "/tmp/faust" + boost_random (10) + filename;

  con_info->answerstring = servererrorpage;
  con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;

  if (0 != strcmp (key, "file"))
    return MHD_NO;

  if (!con_info->fp)
    {
      if (NULL != (fp = fopen (con_info->tmppath.c_str (), "rb")))
        {
          fclose (fp);
          con_info->answerstring = fileexistspage;
          con_info->answercode = MHD_HTTP_FORBIDDEN;
          return MHD_NO;
        }

      con_info->fp = fopen (con_info->tmppath.c_str (), "ab");
      if (!con_info->fp)
        return MHD_NO;
    }

  if (size > 0)
    {
      if (!fwrite (data, size, sizeof (char), con_info->fp))
        return MHD_NO;
    }
  fclose (con_info->fp);

  if (!validate_faust (con_info))
    {
      string_and_exitstatus sha1 = generate_sha1 (con_info);
      if (!sha1.exitstatus)
        (void) make_initial_faust_directory (con_info, sha1.str, string (filename));
    }
    
  con_info->answercode = MHD_HTTP_OK;

  return MHD_YES;
}

void
FaustServer::request_completed (void *cls, struct MHD_Connection *connection,
                   void **con_cls, enum MHD_RequestTerminationCode toe)
{
  struct connection_info_struct *con_info = (connection_info_struct *)*con_cls;

  if (NULL == con_info)
    return;

  if (con_info->connectiontype == POST)
    {
      if (NULL != con_info->postprocessor)
        {
          MHD_destroy_post_processor (con_info->postprocessor);
          nr_of_uploading_clients--;
        }
      // shouldn't happen - paranoia check
      if (con_info->fp)
        fclose (con_info->fp);
    }

  free (con_info);
  *con_cls = NULL;
}


int
FaustServer::answer_to_connection (void *cls, struct MHD_Connection *connection,
                       const char *url, const char *method,
                       const char *version, const char *upload_data,
                       size_t *upload_data_size, void **con_cls)
{
  if (NULL == *con_cls)
    {
      struct connection_info_struct *con_info;

      if (nr_of_uploading_clients >= MAXCLIENTS)
        return send_page (connection, busypage, MHD_HTTP_SERVICE_UNAVAILABLE);

      con_info = new connection_info_struct ();
      if (NULL == con_info)
        return MHD_NO;

      con_info->fp = NULL;

      if (0 == strcmp (method, "POST"))
        {
          con_info->postprocessor =
            MHD_create_post_processor (connection, POSTBUFFERSIZE,
                                       iterate_post, (void *) con_info);

          if (NULL == con_info->postprocessor)
            {
              free (con_info);
              return MHD_NO;
            }

          nr_of_uploading_clients++;

          con_info->connectiontype = POST;
          con_info->answercode = MHD_HTTP_OK;
          con_info->answerstring = errorpage;
        }
      else
        con_info->connectiontype = GET;

      *con_cls = (void *) con_info;

      return MHD_YES;
    }

  if (0 == strcmp (method, "GET"))
    {
      TArgs args;
      MHD_get_connection_values (connection, MHD_GET_ARGUMENT_KIND, get_params, &args);
      if (!args.size ())
        {
          stringstream ss;
          ss << askpage_head << nr_of_uploading_clients << askpage_tail;
          return send_page (connection, ss.str (), MHD_HTTP_OK);
        }
      return faustGet (connection, args);
    }

  if (0 == strcmp (method, "POST"))
    {
      struct connection_info_struct *con_info = (connection_info_struct *)*con_cls;

      if (0 != *upload_data_size)
        {
          MHD_post_process (con_info->postprocessor, upload_data,
                            *upload_data_size);
          *upload_data_size = 0;

          return MHD_YES;
        }
      else
        return send_page (connection, con_info->answerstring,
                          con_info->answercode);
    }

  return send_page (connection, errorpage, MHD_HTTP_BAD_REQUEST);
}

unsigned int FaustServer::nr_of_uploading_clients = 0;

int
FaustServer::faustGet (struct MHD_Connection *connection, TArgs &args)
{
  if (args["sha1"].empty ())
    return send_page (connection, nosha1present, MHD_HTTP_BAD_REQUEST);

  // need more sophisticated error messages below - need ot verify that sha1 exists, for example...

  string architecture_file = args["a"];
  if (architecture_file.empty ())
    architecture_file = "plot.cpp";

  // lists all the files in the directory
  vector<string> filesindir = getdir (args["sha1"]);
  // we've already verified that there is only 1 dsp file in the python script, so we find that
  string dspfile;
  for (unsigned int i = 0; i < filesindir.size (); i++)
    {
      string fn = filesindir[i];
      if (fn.substr (fn.find_last_of (".") + 1) == "dsp")
        {
          dspfile = fn;
          break;
        }
    }
  FILE *pipe = popen (("faust -a "+architecture_file+" "+args["sha1"]+"/"+dspfile).c_str (), "r");
  string result = "";
  if (!pipe)
    return send_page (connection, cannotcompile, MHD_HTTP_BAD_REQUEST);
  else
    {
      // Bleed off the pipe
      char buffer[128];
      while (!feof (pipe))
        {
          if (fgets (buffer, 128, pipe) != NULL)
            result += buffer;
        }
    }

  if (pclose (pipe))
    return send_page (connection, cannotcompile, MHD_HTTP_BAD_REQUEST);

  return send_page (connection, result, MHD_HTTP_OK);
}

bool
FaustServer::start ()
{
  daemon_ = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, port_, NULL, NULL,
                              &answer_to_connection, NULL,
                              MHD_OPTION_NOTIFY_COMPLETED, request_completed,
                              NULL, MHD_OPTION_END);
  return daemon_ != NULL;
}

void
FaustServer::stop ()
{
  if (daemon_)
    MHD_stop_daemon (daemon_);

  daemon_ = 0;
}


FaustServer::FaustServer (int port) : port_ (port)
{
}