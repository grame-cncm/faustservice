#include <sstream>
#include <iostream>
#include <fstream>

// libmicrohttpd
#include <microhttpd.h>

// Boost libraries
#include <boost/filesystem.hpp>

// libcryptopp
//#include <cryptopp/sha.h>
//#include <cryptopp/files.h>
//#include <cryptopp/hex.h>
#include <openssl/sha.h>

// libarchive
#include <archive.h>
#include <archive_entry.h>

#include "utilities.hh"
#include "server.hh"

// to use command line tools
#include <stdlib.h>

using namespace std;
namespace fs = boost::filesystem;

/*
 * Various responses to GET requests
 */

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

string invalidosorarchitecture =
    "<html><body>You have entered either an invalid operating system, an invalid architecture, or an invalid makefile command.\
    Requests should be of the form:<br/>os/architecture/command<br/>For example:<br/>osx/csound/binary</body></html>";

string invalidinstruction =
    "<html><body>The server only can generate binary, source, or svg for a given architecture.</body></html>";

string busypage =
    "<html><body>This server is busy, please try again later.</body></html>";

string completebuterrorpage =
    "<html><body>The upload has been completed but your Faust file is corrupt. It has not been registered.</body></html>";

string completebutmorethanoneDSPfile =
    "<html><body>The upload has been completed but there is more than one DSP file in your archive. Only one is allowed..</body></html>";

string completebutnoDSPfile =
    "<html><body>The upload has been completed but there is no DSP file in your archive. You must have one and only one.</body></html>";

string completebutdecompressionproblem =
    "<html><body>The upload has been completed but the server could not decompress the archive.</body></html>";

string completebutendoftheworld =
    "<html><body>An internal server error of epic proportions has occurred. This likely portends the end of the world.</body></html>";

string completebutnopipe =
    "<html><body>Could not create a PIPE to faust on the server.</body></html>";

string completebutnohash =
    "<html><body>The upload is completed but we could not generate a hash for your file.</body></html>";

string completebutcorrupt_head =
    "<html><body><p>The upload is completed but the file you uploaded is not a valid Faust file. \
  Make sure that it is either a file with an extension .dsp or an archive (tar.gz, tar.bz, tar \
  or zip) containing one .dsp file and potentially .lib files included by the .dsp file. \
  Furthermore, the code in these files must be valid faust code.</p> \
  <p>Below is the STDOUT and STDERR for the Faust code you tried to compile. \
  If the two are empty, then your file structure is wrong. Otherwise, they will tell you \
  why Faust failed.</p>"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    ;

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

string debugstub =
    "<html><body>Rien ne s'est cass&eacute; la figure. F&eacute;licitations !</body></html>";

/*
 * Validates that a Faust file or archive is sane and returns 0 for success
 * or 1 for failure.  If the evaluation fails, the appropriate error message
 * is set.  More info on the con_info structure is in server.hh.
 */

int
validate_faust(connection_info_struct *con_info)
{
    fs::path tmpdir = fs::temp_directory_path() / fs::unique_path("%%%%-%%%%-%%%%-%%%%");
    fs::create_directory(tmpdir);
    fs::path filename = fs::path(con_info->filename);
    fs::path old_full_filename = fs::path(con_info->tmppath) / filename;

    // libarchive stuff
    struct archive *my_archive;
    struct archive_entry *my_entry;

    my_archive = archive_read_new();
    archive_read_support_filter_all(my_archive);
    archive_read_support_format_all(my_archive);
    int archive_status = archive_read_open_filename(my_archive, old_full_filename.string().c_str(), 10240);

    // prepare for the tar

    if (!fs::is_regular_file(old_full_filename)) {
        fs::remove_all(tmpdir);
        con_info->answerstring = completebuterrorpage;
        return 1;
    } else if (old_full_filename.string().substr(old_full_filename.string().find_last_of(".") + 1) == "dsp") {
        fs::copy_file(old_full_filename, tmpdir / filename);
    } else if (archive_status == ARCHIVE_OK) {
        string dsp_file;
        while (archive_read_next_header(my_archive, &my_entry) == ARCHIVE_OK) {
            fs::path current_file = fs::path(archive_entry_pathname(my_entry));
            if (current_file.string().substr(current_file.string().find_last_of(".") + 1) == "dsp") {
                if (!dsp_file.empty()) {
                    archive_status = archive_read_free(my_archive);
                    fs::remove_all(tmpdir);
                    con_info->answerstring = completebutmorethanoneDSPfile;
                    return 1;
                }
                dsp_file = current_file.string();
                filename = dsp_file;
            }
            string newpath = fs::path(tmpdir / current_file).string();
            archive_entry_set_pathname(my_entry, newpath.c_str());
            archive_read_extract(my_archive, my_entry, ARCHIVE_EXTRACT_PERM);
        }
        archive_status = archive_read_free(my_archive);
        if (archive_status != ARCHIVE_OK) {
            fs::remove_all(tmpdir);
            con_info->answerstring = completebutdecompressionproblem;
            return 1;
        }
    } else {
        fs::remove_all(tmpdir);
        con_info->answerstring = completebutendoftheworld;
        return 1;
    }

    // in case a dsp file wasn't found
    if (filename.string() == "") {
        fs::remove_all(tmpdir);
        con_info->answerstring = completebutnoDSPfile;
        return 1;
    }

    string result = "";
    FILE *pipe = popen(("faust -a plot.cpp " + (tmpdir / filename).string() + " 2>&1").c_str(), "r");
    if (!pipe) {
        con_info->answerstring = completebutnopipe;
    } else {
        // Bleed off the pipe
        char buffer[128];
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != NULL) {
                result += buffer;
            }
        }
    }

    int exitstatus = pclose(pipe);
    fs::remove_all(tmpdir);

    if (exitstatus) {
        con_info->answerstring = completebutcorrupt_head + result + completebutcorrupt_tail;
    }

    return exitstatus;
}

/*
 * Generates an SHA-1 key for Faust file or archive and returns 0 for success
 * or 1 for failure along with the key in the string_and_exitstatus structure.
 * If the evaluation fails, the appropriate error message is set. More info
 * on the con_info structure is in server.hh.
 */

string_and_exitstatus
generate_sha1(connection_info_struct *con_info)
{
    fs::path old_full_filename = fs::path(con_info->tmppath) / fs::path(con_info->filename);
    string source = old_full_filename.string();
    string hash = "";

    ifstream myFile (source.c_str (), ios::in | ios::binary);
    myFile.seekg (0, ios::end);
    int length = myFile.tellg();
    myFile.seekg (0, ios::beg);

    char result[length];
    // read data as a block:
    myFile.read (result, length);
    myFile.close();
    unsigned char obuf[20];
    string setter(result);

    SHA1((const unsigned char *)(setter.c_str()), setter.length(), obuf);
    char buffer[20];
    stringstream ss;
    
    for (int i=0; i < 20; i++) {
       sprintf(buffer, "%02x", obuf[i]);
       ss << buffer;
    }
    
    hash = ss.str();

    string_and_exitstatus res;
    res.exitstatus = hash.length() == 40 ? 0 : 1;
    res.str = hash;
    if (res.exitstatus) {
        con_info->answerstring = completebutnohash;
    }
    return res;
}

/*
 * Creates an arboreal structure in root with the appropriate makefiles.
 */

void
create_file_tree(fs::path sha1path, fs::path makefile_directory)
{
    fs::directory_iterator end_iter;
    for (fs::directory_iterator os_iter(makefile_directory); os_iter != end_iter; ++os_iter) {
        fs::path os_dir = os_iter->path().filename();
        // hack for root Makefile
        if (os_dir.string() == "Makefile.none") {
            fs::copy_file(os_iter->path(),
                          sha1path / fs::path("Makefile"));
        } else if (fs::is_directory(os_iter->path())) {
            fs::create_directory(sha1path / os_dir);
            for (fs::directory_iterator dir_iter(os_iter->path()); dir_iter != end_iter; ++dir_iter) {
                string fname = dir_iter->path().filename().string();
                if (fname.substr(0, fname.find_first_of(".")) == "Makefile") {
                    string dirname = fname.substr(fname.find_last_of(".") + 1);
                    fs::create_directory(sha1path / os_dir / fs::path (dirname));
                    fs::copy_file(makefile_directory / dir_iter->path(),
                                  sha1path / os_dir / fs::path(dirname) / fs::path("Makefile"));
                    for (fs::directory_iterator faust_iter(sha1path); faust_iter != end_iter; ++faust_iter) {
                        string maybe_link_me = faust_iter->path().filename().string();
                        if (maybe_link_me.substr(maybe_link_me.find_last_of(".") + 1) == "dsp"
                                || maybe_link_me.substr(maybe_link_me.find_last_of(".") + 1) == "lib") {
                            fs::create_symlink(faust_iter->path(),
                                               sha1path / os_dir / fs::path(dirname) / faust_iter->path().filename ());
                        }
                    }
                }
            }
        }
    }
}

/*
 * Makes an initial directory whose name is the SHA-1 key passed in for
 * a Faust file or archive, returning 0 for success or 1 for failure.
 * If the evaluation fails, the appropriate error message is set.
 * More info on the con_info structure is in server.hh.
 */

// TODO: merge with validate function if possible...
int
make_initial_faust_directory(connection_info_struct *con_info, string sha1)
{
    fs::path sha1path = fs::path(con_info->directory) / fs::path(sha1);
    if (fs::is_directory(sha1path)) {
        con_info->answerstring = completebutalreadythere_head + sha1 + completebutalreadythere_tail;
        return 1;
    }

    fs::create_directory(sha1path);
    fs::path filename(con_info->filename);
    fs::path old_full_filename = fs::path(con_info->tmppath) / filename;

    // libarchive stuff
    struct archive *my_archive;
    struct archive_entry *my_entry;

    my_archive = archive_read_new();
    archive_read_support_filter_all(my_archive);
    archive_read_support_format_all(my_archive);
    int archive_status = archive_read_open_filename(my_archive, old_full_filename.string().c_str(), 10240);
    string result = "";

    if (!fs::is_regular_file(old_full_filename)) {
        con_info->answerstring = completebuterrorpage;
        return 1;
    } else if (filename.string().substr(filename.string().find_last_of(".") + 1) == "dsp") {
        fs::copy_file(old_full_filename, sha1path / filename);
    } else if (archive_status == ARCHIVE_OK) {
        string dsp_file;
        while (archive_read_next_header(my_archive, &my_entry) == ARCHIVE_OK) {
            fs::path current_file = fs::path(archive_entry_pathname(my_entry));
            string newpath = fs::path(sha1path / current_file).string();
            archive_entry_set_pathname(my_entry, newpath.c_str());
            archive_read_extract(my_archive, my_entry, ARCHIVE_EXTRACT_PERM);
        }
        archive_status = archive_read_free(my_archive);
        if (archive_status != ARCHIVE_OK) {
            con_info->answerstring = completebutdecompressionproblem;
            return 1;
        }
    } else {
        con_info->answerstring = completebutendoftheworld;
        return 1;
    }

    create_file_tree(sha1path, fs::path(con_info->makefile_directory));
    con_info->answerstring = completepage_head + sha1 + completepage_tail;
    return 0;
}

/*
 * returns the number of elements in a path
 * an implementation using directory_iterators was leading to an abort trap
 * should investigate further...
 */

int
pathsize(fs::path path, int n = 0)
{
    if (path.string()=="/" || path.string()=="." || path.string()=="") {
        return n;
    }

    return pathsize(path.parent_path(), n+1);
}

/*
 * Callback that puts GET parameters in a TArgs.  The TArgs typedef is defined
 * in utilities.hh.
 */

int
FaustServer::get_params(void *cls, enum MHD_ValueKind, const char *key, const char *data)
{
    TArgs* args = (TArgs*)cls;
    args->insert(pair<string, string> (string(key), string(data)));
    return MHD_YES;
}

/*
 * Function that sends a response to the MHD_Connection after a POST or GET
 * is effectuated.
 */

int
FaustServer::send_page(struct MHD_Connection *connection, const char *page, int length,
                       int status_code, const char * type = 0)
{
    int ret;
    struct MHD_Response *response;

    response =
        MHD_create_response_from_buffer(length, (void*)page,
                                        MHD_RESPMEM_PERSISTENT);
    if (!response) {
        return MHD_NO;
    }

    MHD_add_response_header (response, "Content-Type", type ? type : "text/plain");
    ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);

    return ret;
}

/*
 * Callback called every time a POST request comes in by the postprocessor.
 * To understand more about how postprocessors work, consult the microhttpd
 * documentation.
 */

int
FaustServer::iterate_post(void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
                          const char *filename, const char *content_type,
                          const char *transfer_encoding, const char *data, uint64_t off,
                          size_t size)
{
    struct connection_info_struct *con_info = (connection_info_struct*)coninfo_cls;
    FILE *fp;

    if (con_info->tmppath.empty()) {
        con_info->filename = filename;
        con_info->tmppath = (fs::temp_directory_path() / fs::unique_path("%%%%-%%%%-%%%%-%%%%")).string();
        fs::create_directory(con_info->tmppath);
    }

    string full_path = (fs::path(con_info->tmppath) / fs::path(con_info->filename)).string();

    con_info->answerstring = servererrorpage;
    con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;

    if (0 != strcmp(key, "file")) {
        return MHD_NO;
    }

    if (!con_info->fp) {
        if (NULL != (fp = fopen(full_path.c_str(), "rb"))) {
            fclose(fp);
            con_info->answerstring = fileexistspage;
            con_info->answercode = MHD_HTTP_FORBIDDEN;
            return MHD_NO;
        }

        con_info->fp = fopen(full_path.c_str(), "ab");
        if (!con_info->fp) {
            return MHD_NO;
        }
    }

    if (size > 0) {
        if (!fwrite(data, size, sizeof(char), con_info->fp)) {
            return MHD_NO;
        }
    }

    con_info->answercode = MHD_HTTP_OK;

    return MHD_YES;
}

/*
 * Callback called every time a GET or POST request is completed.
 * Note that this is NOT necessarily called once the entirety of
 * POST data is transfered. If the data is transfered in chunks,
 * this is called after every chunk.
 */

void
FaustServer::request_completed(void *cls, struct MHD_Connection *connection,
                               void **con_cls, enum MHD_RequestTerminationCode toe)
{
    struct connection_info_struct *con_info = (connection_info_struct*)*con_cls;

    if (NULL == con_info) {
        return;
    }

    if (con_info->connectiontype == POST) {
        if (NULL != con_info->postprocessor) {
            MHD_destroy_post_processor(con_info->postprocessor);
            nr_of_uploading_clients--;
        }
        if (con_info->fp) {
            fclose(con_info->fp);
        }
    }

    free(con_info);
    *con_cls = NULL;
}

/*
 * Callback called every time a GET or POST request is received.
 * by the server.
 */

int
FaustServer::answer_to_connection(void *cls, struct MHD_Connection *connection,
                                  const char *url, const char *method,
                                  const char *version, const char *upload_data,
                                  size_t *upload_data_size, void **con_cls)
{
    FaustServer *server = (FaustServer*)cls;

    if (NULL == *con_cls) {
        struct connection_info_struct *con_info;

        if (nr_of_uploading_clients >= server->getMaxClients()) {
            return send_page(connection, busypage.c_str (), busypage.size (), MHD_HTTP_SERVICE_UNAVAILABLE, "text/html");
        }

        con_info = new connection_info_struct();
        con_info->directory = server->getDirectory();
        con_info->makefile_directory = server->getMakefileDirectory();

        if (NULL == con_info) {
            return MHD_NO;
        }

        con_info->fp = NULL;

        if (0 == strcmp(method, "POST")) {
            con_info->postprocessor =
                MHD_create_post_processor(connection, POSTBUFFERSIZE,
                                          iterate_post, (void*)con_info);

            if (NULL == con_info->postprocessor) {
                free(con_info);
                return MHD_NO;
            }

            nr_of_uploading_clients++;

            con_info->connectiontype = POST;
            con_info->answercode = MHD_HTTP_OK;
            con_info->answerstring = errorpage;
        } else {
            con_info->connectiontype = GET;
        }

        *con_cls = (void*)con_info;

        return MHD_YES;
    }

    if (0 == strcmp(method, "GET")) {
        TArgs args;
        MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, get_params, &args);
        if (!args.size() && strcmp(url, "/") == 0) {
            stringstream ss;
            ss << askpage_head << nr_of_uploading_clients << askpage_tail;
            return send_page(connection, ss.str().c_str (), ss.str().size(), MHD_HTTP_OK, "text/html");
        }
        struct connection_info_struct *con_info = (connection_info_struct*)*con_cls;
        return faustGet(connection, con_info, url, args, server->getDirectory());
    }

    if (0 == strcmp(method, "POST")) {
        struct connection_info_struct *con_info = (connection_info_struct*)*con_cls;

        if (0 != *upload_data_size) {
            MHD_post_process(con_info->postprocessor, upload_data,
                             *upload_data_size);
            *upload_data_size = 0;

            return MHD_YES;
        } else {
            // need to close the file before request_completed
            // so that it can be opened by the methods below
            if (con_info->fp) {
                fclose(con_info->fp);
            }

            if (!validate_faust(con_info)) {
                string_and_exitstatus sha1 = generate_sha1(con_info);
                if (!sha1.exitstatus) {
                    (void)make_initial_faust_directory(con_info, sha1.str);
                }
            }
            return send_page(connection, con_info->answerstring.c_str(),
                             con_info->answerstring.size(), con_info->answercode, "text/html");
        }
    }

    return send_page(connection, errorpage.c_str(), errorpage.size(), MHD_HTTP_BAD_REQUEST, "text/html");
}

// Number of clients currently uploading

unsigned int FaustServer::nr_of_uploading_clients = 0;

/*
 * Function that handles all get requests, sending back the
 * appropriate response.
 *
 * All the components in the URL represent a file structure
 * created when the file was uploaded. The last component of
 * the url represents an instruction to the Makefile.
 *
 * Any GET parameters are set as environmetnal variables before
 * calling the makefile.
 */

int
FaustServer::faustGet(struct MHD_Connection *connection, connection_info_struct *con_info, const char *raw_url, TArgs &args, string directory)
{
    // The parent_path of the URL must be valid in the file tree
    // we examine.
    fs::path basedir(con_info->directory);
    fs::path url(raw_url);
    int nelts = pathsize(url);

    if (!fs::is_directory(basedir / url.parent_path())
            || !(nelts == 2 || nelts == 4)) {
        return send_page(connection, invalidosorarchitecture.c_str(), invalidosorarchitecture.size(), MHD_HTTP_BAD_REQUEST, "text/html");
    }

    if (url.filename() != "binary"
            && url.filename() != "source"
            && url.filename() != "svg") {
        return send_page(connection, invalidinstruction.c_str(), invalidinstruction.size(), MHD_HTTP_BAD_REQUEST, "text/html");
    }

    // dangerous operation...but have to do it for makefiles
    fs::path old_path(fs::current_path());
    fs::current_path(basedir / url.parent_path());
    for (map<string, string>::iterator it = args.begin (); it != args.end(); it++) {
        string setter = (it->first+"="+it->second).c_str();
        char *caution = new char[setter.length()+1];
        setter.copy(caution, setter.length()+1);
        if (0 != putenv(caution)) {
            delete[] caution;
            return send_page(connection, cannotcompile.c_str(), cannotcompile.size(), MHD_HTTP_BAD_REQUEST, "text/html");
        }
        delete[] caution;        
    }
    if (0 != system(("make "+url.filename().string()).c_str()))
        return send_page(connection, cannotcompile.c_str(), cannotcompile.size(), MHD_HTTP_BAD_REQUEST, "text/html");

    string filename = "";

    fs::directory_iterator end_iter;
    for (fs::directory_iterator os_iter(basedir / url.parent_path()); os_iter != end_iter; ++os_iter) {
        string os_dir = os_iter->path().string();
        if (os_dir.substr(os_dir.find_last_of(".") + 1) == "dsp") {
            filename = os_dir;
            break;
        }
    }

    string mimetype = "text/plain";
    if (url.filename() == "binary") {
        filename = filename.substr(0,filename.find_first_of("."))+".zip";
        mimetype = "application/zip";
    } else if (url.filename() == "source") {
        filename = filename.substr(0,filename.find_first_of("."))+".cpp";
    } else if (url.filename() == "svg") {
        filename = filename.substr(0,filename.find_first_of("."))+"-svg.zip";
    }

    if (!fs::is_regular_file(filename)) {
        return send_page(connection, cannotcompile.c_str(), cannotcompile.size(), MHD_HTTP_BAD_REQUEST, "text/html");
    }

    ifstream myFile (filename.c_str (), ios::in | ios::binary);
    myFile.seekg (0, ios::end);
    int length = myFile.tellg();
    myFile.seekg (0, ios::beg);

    char result[length];
    // read data as a block:
    myFile.read (result, length);
    myFile.close();

    fs::current_path(old_path);
    return send_page(connection, result, length, MHD_HTTP_OK, mimetype.c_str ());

}

// Get the maximum number of clients allowed to connect at a given time.

const int
FaustServer::getMaxClients()
{
    return max_clients_;
}

// Get the directory to which the uploaded files are being written.

const string
FaustServer::getDirectory()
{
    return directory_;
}

// Get the directory that the makefiles should be copied from.

const string
FaustServer::getMakefileDirectory()
{
    return makefile_directory_;
}

// Get the path to the logfile.

const string
FaustServer::getLogfile()
{
    return logfile_;
}

// Start the Faust server - shallow wrapper around MHD_start_daemon

bool
FaustServer::start()
{
    daemon_ = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, port_, NULL, NULL,
                               &answer_to_connection, this,
                               MHD_OPTION_NOTIFY_COMPLETED, request_completed,
                               NULL, MHD_OPTION_END);
    return daemon_ != NULL;
}

// Stop the Faust server - shallow wrapper around MHD_stop_daemon

void
FaustServer::stop()
{
    if (daemon_) {
        MHD_stop_daemon(daemon_);
    }

    daemon_ = 0;
}


// Constructor for the Faust server

FaustServer::FaustServer (int port, int max_clients, string directory, string makefile_directory, string logfile)
    : port_(port), max_clients_(max_clients), directory_(directory), makefile_directory_(makefile_directory), logfile_(logfile)
{
}