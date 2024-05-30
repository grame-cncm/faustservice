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

#include <fcntl.h>
#include <algorithm>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// libmicrohttpd
#include <microhttpd.h>

// libcryptopp
#include <openssl/sha.h>

// libarchive
#include <archive.h>
#include <archive_entry.h>

#include "htmlPages.hh"
#include "match.hh"
#include "server.hh"
#include "utilities.hh"

// to use command line tools
#include <stdlib.h>

using namespace std;

extern int gVerbosity;
/*
 * Various responses to GET requests
 */

extern bool gAnyOrigin;  // when true adds Access-Control-Allow-Origin to http answers

#define MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN "Access-Control-Allow-Origin"

/*
 * Generates an SHA-1 key for Faust file or archive.
 */

static string generate_sha1(connection_info_struct* con_info)
{
    fs::path filepath = fs::path(con_info->tmppath) / fs::path(con_info->filename);

    // read file content
    ifstream myFile(filepath.string().c_str(), ios::in | ios::binary);
    myFile.seekg(0, ios::end);
    int length = myFile.tellg();
    myFile.seekg(0, ios::beg);

    // char content[length];
    char* content = (char*)malloc(length);
    if (content == 0) {
        return "malloc-error";
    }
    myFile.read(content, length);
    myFile.close();

    // compute SHA1 key
    unsigned char obuf[20];
    SHA1((const unsigned char*)content, length, obuf);

    // convert SHA1 key into hexadecimal string
    string sha1key;
    for (int i = 0; i < 20; i++) {
        const char* H  = "0123456789ABCDEF";
        char        c1 = H[(obuf[i] >> 4)];
        char        c2 = H[(obuf[i] & 15)];
        sha1key += c1;
        sha1key += c2;
    }
    free(content);

    return sha1key;
}

/*
 * True if it is a .dsp or a .lib source file
 */

static bool isFaustFile(const fs::path& f)
{
    fs::path x = f.extension();
    bool     a = (x == ".dsp") || (x == ".lib");
    // std::cerr << "isFaustFile(" << f << ") = " << a << std::endl;
    return a;
}

/*
 * Copy all faust source files from src directory to destination directory
 */

static void copyFaustFiles(const fs::path& src, const fs::path& dst)
{
    assert(is_directory(src));
    assert(is_directory(dst));
    fs::directory_iterator end_iter;
    for (fs::directory_iterator f_iter(src); f_iter != end_iter; ++f_iter) {
        if (isFaustFile(f_iter->path())) {
            fs::copy_file(f_iter->path(), dst / f_iter->path().filename());
        }
    }
}

/*
 * Creates an arboreal structure in root with the appropriate makefiles.
 */

static void create_file_tree(fs::path srcdir, fs::path sha1path, fs::path makefile_directory)
{
    // std::cerr << "ENTER create_file_tree(" << sha1path << ", " << makefile_directory << ")" << std::endl;
    fs::directory_iterator end_iter;
    for (fs::directory_iterator os_iter(makefile_directory); os_iter != end_iter; ++os_iter) {
        if (fs::is_directory(os_iter->path())) {
            // std::cerr << "scanning OS directory " << os_iter->path() << std::endl;
            string OSname = os_iter->path().filename().string();
            // create_directory(sha1path/OSname);
            for (fs::directory_iterator makefile_iter(os_iter->path()); makefile_iter != end_iter; ++makefile_iter) {
                string makefileName = makefile_iter->path().filename().string();
                if (makefileName.substr(0, 9) == "Makefile.") {
                    string archName = makefileName.substr(9);
                    if (gVerbosity >= 2)
                        std::cerr << "scanning makefile " << makefile_iter->path() << ", makefile : " << makefileName
                                  << ", architecture : " << archName << std::endl;
                    fs::path dstdir = sha1path / OSname / archName;
                    if (gVerbosity >= 2) std::cerr << "dstdir = " << dstdir << std::endl;
                    create_directories(dstdir);
                    fs::copy_file(makefile_iter->path(), dstdir / "Makefile");
                    copyFaustFiles(srcdir, dstdir);
                }
            }
        }
    }

    // copy makefile.none to handle non-architecture-specific targets like mdoc.zip, etc
    fs::copy_file(fs::path(makefile_directory) / "Makefile.none", sha1path / "Makefile");
    copyFaustFiles(srcdir, sha1path);

    // std::cerr << "EXIT create_file_tree()" << std::endl;
}
/*
 * Validates that a Faust file or archive is sane and returns 0 for success
 * or 1 for failure. If the evaluation fails, the appropriate error message
 * is set. More info on the con_info structure is in server.hh.
 */

static int validate_faust(connection_info_struct* con_info)
{
    fs::path tmpdir = fs::temp_directory_path() / fs::unique_path("%%%%-%%%%-%%%%-%%%%");
    fs::create_directory(tmpdir);
    fs::path filename          = fs::path(con_info->filename);
    fs::path old_full_filename = fs::path(con_info->tmppath) / filename;
    fs::path unzipedDir;

    if (gVerbosity >= 2) std::cerr << "\nENTER validate_faust for file : " << old_full_filename << std::endl;

    // libarchive stuff
    struct archive*       my_archive;
    struct archive_entry* my_entry;

    my_archive = archive_read_new();
    archive_read_support_filter_all(my_archive);
    archive_read_support_format_all(my_archive);
    int archive_status = archive_read_open_filename(my_archive, old_full_filename.string().c_str(), 10240);

    // prepare for the tar

    if (!fs::is_regular_file(old_full_filename)) {
        fs::remove_all(tmpdir);
        con_info->answerstring = completebuterrorpage;
        if (gVerbosity >= 1)
            std::cerr << "EXIT validate_faust with failure : not regular file : " << old_full_filename << std::endl;
        return 1;
    } else if (old_full_filename.string().substr(old_full_filename.string().find_last_of(".") + 1) == "dsp") {
        fs::copy_file(old_full_filename, tmpdir / filename);
    } else if (archive_status == ARCHIVE_OK) {
        string dsp_file;

        // BEGIN read archived files
        while (archive_read_next_header(my_archive, &my_entry) == ARCHIVE_OK) {
            fs::path current_file = fs::path(archive_entry_pathname(my_entry));
            if (gVerbosity >= 1) std::cerr << "archive_read_next_header : " << current_file << std::endl;

            if (current_file.string().substr(0, 8) == "__MACOSX") {
                if (gVerbosity >= 1) std::cerr << "Ignore file " << current_file << std::endl;
            } else {
                if (current_file.string().substr(current_file.string().find_last_of(".") + 1) == "dsp") {
                    if (!dsp_file.empty()) {
                        archive_status = archive_read_free(my_archive);
                        fs::remove_all(tmpdir);
                        con_info->answerstring = completebutmorethanoneDSPfile;
                        std::cerr << "ERROR, we have more than one dsp file " << current_file << std::endl;
                        return 1;
                    }
                    dsp_file = current_file.string();
                    filename = dsp_file;
                }
                unzipedDir     = fs::path(tmpdir / current_file).parent_path();
                string newpath = fs::path(tmpdir / current_file).string();
                archive_entry_set_pathname(my_entry, newpath.c_str());
                archive_read_extract(my_archive, my_entry, ARCHIVE_EXTRACT_PERM);
            }
        }
        // END read archived files

        archive_status = archive_read_free(my_archive);
        if (archive_status != ARCHIVE_OK) {
            fs::remove_all(tmpdir);
            con_info->answerstring = completebutdecompressionproblem;
            if (gVerbosity >= 1)
                std::cerr << "EXIT validate_faust with failure : Archive not OK case 1 : " << old_full_filename
                          << std::endl;
            return 1;
        }
    } else {
        fs::remove_all(tmpdir);
        con_info->answerstring = completebutendoftheworld;
        if (gVerbosity >= 1)
            std::cerr << "EXIT validate_faust with failure : Archive not OK case 2 : " << old_full_filename
                      << std::endl;
        return 1;
    }

    // in case a dsp file wasn't found
    if (filename.string() == "") {
        fs::remove_all(tmpdir);
        con_info->answerstring = completebutnoDSPfile;
        if (gVerbosity >= 1)
            std::cerr << "EXIT validate_faust with failure : empty filename : " << old_full_filename << std::endl;
        return 1;
    }

    if (gVerbosity >= 1) std::cerr << "TRY TO COMPILE validate_faust : " << (tmpdir / filename).string() << std::endl;
    string result = "";
    FILE*  pipe   = popen(("faust -a plot.cpp " + (tmpdir / filename).string() + " 2>&1").c_str(), "r");
    if (!pipe) {
        con_info->answerstring = completebutnopipe;
    } else {
        // Bleed off the pipe
        char buffer[256];
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != NULL) {
                // std::cerr << "READ PIPE validate_faust : " << buffer<< std::endl;
                result += buffer;
            }
        }
    }

    int exitstatus = pclose(pipe);

    // if (gVerbosity >= 1) std::cerr << "fs::remove_all(" << tmpdir << ")" << std::endl;
    // fs::remove_all(tmpdir);

    if (exitstatus) {
        if (gVerbosity >= 1)
            std::cerr << "EXIT validate_faust with failure : completebutcorrupt_head  : " << old_full_filename
                      << std::endl;
        con_info->answerstring = completebutcorrupt_head + result + completebutcorrupt_tail;
    }

    if (gVerbosity >= 2) std::cerr << "EXIT validate_faust is OK: " << old_full_filename << std::endl;

    string sha1 = generate_sha1(con_info);

    // build the sha1 directories here
    {
        fs::path sha1path = fs::path(con_info->directory) / fs::path(sha1);

        if (!fs::is_directory(sha1path)) {
            if (gVerbosity >= 2)
                std::cerr << "ENTER make_initial_faust_directory(" << con_info << ", " << sha1path << std::endl;

            try {
                // first time we have this file
                fs::create_directory(sha1path);
            } catch (const fs::filesystem_error& e) {
                std::cerr << "Warning : can't create directory " << sha1path << ":" << e.code().message() << std::endl;
            }
            string   filename(con_info->filename);
            fs::path old_full_filename = fs::path(con_info->tmppath) / filename;

            // libarchive stuff
            struct archive* my_archive;
            // struct archive_entry* my_entry;

            my_archive = archive_read_new();
            archive_read_support_filter_all(my_archive);
            archive_read_support_format_all(my_archive);
            int    archive_status = archive_read_open_filename(my_archive, old_full_filename.string().c_str(), 10240);
            string result         = "";

            if (!fs::is_regular_file(old_full_filename)) {
                con_info->answerstring = completebuterrorpage;
                return 1;

            } else if (filename.substr(filename.find_last_of(".") + 1) == "dsp") {
                create_file_tree(tmpdir, sha1path, fs::path(con_info->makefile_directory));

            } else if (archive_status == ARCHIVE_OK) {
                create_file_tree(unzipedDir, sha1path, fs::path(con_info->makefile_directory));

            } else {
                con_info->answerstring = completebutendoftheworld;
                return 1;
            }

            if (gVerbosity >= 2)
                std::cerr << "EXIT make_initial_faust_directory" << con_info << ", " << sha1 << std::endl;
        }
        con_info->answerstring = sha1;
    }

    fs::remove_all(tmpdir);
    return exitstatus;
}

/* TO REMOVE ?
static bool isMakefile(const fs::path& f)
{
    return f.stem() == "Makefile";
}
*/

/*
 * Makes an initial directory whose name is the SHA-1 key passed in for
 * a Faust file or archive, returning 0 for success or 1 for failure.
 * If the evaluation fails, the appropriate error message is set.
 * More info on the con_info structure is in server.hh.
 */

static fs::path make(const fs::path& dir, const fs::path& target)
{
    fs::path          p;
    std::stringstream ss;
    ss << "make -C " << dir << " " << target;

    if (gVerbosity >= 2) std::cerr << "ENTER MAKE " << ss.str() << std::endl;
    if (0 == system(ss.str().c_str())) {
        p = dir / target;
    } else {
        if (gVerbosity >= 2) std::cerr << __LINE__ << " makefile " << dir / target << " failed !!!" << std::endl;
        p = "";
    }
    if (gVerbosity >= 2) std::cerr << "EXIT MAKE " << p << std::endl;

    return p;
}

/*
 * returns the number of elements in a path
 * an implementation using directory_iterators was leading to an abort trap
 * should investigate further...
 */

/* TO REMOVE ?
static int pathsize(fs::path path, int n = 0)
{
    if (path.string()=="/" || path.string()=="." || path.string()=="") {
        return n;
    }

    return pathsize(path.parent_path(), n+1);
}
*/

/*
 * Callback that puts GET parameters in a TArgs. The TArgs typedef is defined
 * in utilities.hh.
 */

int FaustServer::get_params(void* cls, enum MHD_ValueKind, const char* key, const char* data)
{
    TArgs* args = (TArgs*)cls;
    args->insert(pair<string, string>(string(key), string(data)));
    return MHD_YES;
}

/*
 * Function that sends a response to the MHD_Connection after a POST or GET
 * is effectuated.
 */

int FaustServer::send_page(struct MHD_Connection* connection, const char* page, int length, int status_code,
                           const char* type = 0)
{
    struct MHD_Response* response = MHD_create_response_from_buffer(length, (void*)page, MHD_RESPMEM_MUST_COPY);

    if (response == 0) {
        return MHD_NO;

    } else {
        MHD_add_response_header(response, "Content-Type", type ? type : "text/plain");
        if (gAnyOrigin) MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");
        int ret = MHD_queue_response(connection, status_code, response);
        MHD_destroy_response(response);

        return ret;
    }
}

/**
 * Handler used to generate a 404 reply.
 *
 * @param cls a ’const char* ’ with the HTML webpage to return
 * @param mime mime type to use
 * @param session session handle
 * @param connection connection to use
 */

static int page_not_found(struct MHD_Connection* connection, const char* page, int length, const char* mimetype)
{
    struct MHD_Response* response = MHD_create_response_from_buffer(length, (void*)page, MHD_RESPMEM_MUST_COPY);
    int                  ret      = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_ENCODING, mimetype);
    MHD_destroy_response(response);
    return ret;
}

/*
 * Callback called every time a GET or POST request is completed.
 * Note that this is NOT necessarily called once the entirety of
 * POST data is transfered. If the data is transfered in chunks,
 * this is called after every chunk.
 */

void FaustServer::request_completed(void*, struct MHD_Connection*, void** con_cls, enum MHD_RequestTerminationCode)
{
    if (gVerbosity >= 2) std::cerr << "FaustServer::request_completed()" << endl;

    struct connection_info_struct* con_info = (connection_info_struct*)*con_cls;
    *con_cls                                = NULL;

    if (con_info != 0) {
        if (con_info->connectiontype == POST) {
            if (NULL != con_info->postprocessor) {
                MHD_destroy_post_processor(con_info->postprocessor);
                nr_of_uploading_clients--;
            }
            if (con_info->fp != 0) {
                fclose(con_info->fp);
                con_info->fp = 0;
            }
        }

        delete con_info;
    }
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

// Define here the various targets accepted by faustweb makefiles
static bool isValidTarget(const fs::path& target, const char*& mimetype)
{
    if (target == "binary.zip") {
        mimetype = "application/zip";
        return true;

    } else if (target == "binary.apk") {
        mimetype = "application/vnd.android.package-archive";
        return true;

    } else if (target == "src.cpp") {
        mimetype = "text/x-c";
        return true;

    } else if (target == "svg.zip") {
        mimetype = "application/zip";
        return true;

    } else if (target == "mdoc.zip") {
        mimetype = "application/zip";
        return true;

    } else if (target == "installer.sh") {
        mimetype = "application/x-shellscript";
        return true;

    } else {
        return false;
    }
}

/*
 * Function that sends a file in response to a GET
 */

int FaustServer::send_file(struct MHD_Connection* connection, const fs::path& filepath, const char* mimetype)
{
    struct stat sbuf;
    int         fd;

    if (gVerbosity >= 2) std::cerr << __LINE__ << " ENTER send_file : " << filepath << endl;

    if ((-1 == (fd = open(filepath.string().c_str(), O_RDONLY))) || (0 != fstat(fd, &sbuf))) {
        std::cerr << __LINE__ << " error accessing file : " << filepath << endl;
        return send_page(connection, cannotcompile.c_str(), cannotcompile.size(), MHD_HTTP_BAD_REQUEST, "text/html");
    }

    struct MHD_Response* response = MHD_create_response_from_fd(sbuf.st_size, fd);
    if (!response) {
        std::cerr << __LINE__ << " error can't create response : " << filepath << endl;
        return MHD_NO;
    }

    MHD_add_response_header(response, "Content-Type", mimetype);
    MHD_add_response_header(response, "Content-Location", filepath.filename().string().c_str());
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    if (gVerbosity >= 2) {
        if (ret == MHD_YES) {
            std::cerr << __LINE__ << " MHD_queue_response() return MHD_YES " << endl;
        } else {
            std::cerr << __LINE__ << " MHD_queue_response() return NOT MHD_YES but " << ret << endl;
        }
    }
    MHD_destroy_response(response);

    if (gVerbosity >= 2) std::cerr << __LINE__ << " EXIT send_file : " << filepath << endl;
    return ret;
}

// Start the Faust server - shallow wrapper around MHD_start_daemon

static void panicCallback(void*, const char* file, unsigned int line, const char* reason)
{
    cerr << "PANIC " << line << ':' << file << ' ' << reason << endl;
}

bool FaustServer::start()
{
    MHD_set_panic_func(&panicCallback, NULL);
    fDaemon = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, fPort, NULL, NULL,
                               (MHD_AccessHandlerCallback)&staticAnswerToConnection, this, MHD_OPTION_NOTIFY_COMPLETED,
                               request_completed, NULL, MHD_OPTION_END);

    return fDaemon != NULL;
}

// Stop the Faust server - shallow wrapper around MHD_stop_daemon

void FaustServer::stop()
{
    if (fDaemon) {
        MHD_stop_daemon(fDaemon);
    }

    fDaemon = 0;
}

/*
 * Static Callback called every time a GET or POST request is received.
 * by the server.
 */

int FaustServer::staticAnswerToConnection(void* cls, struct MHD_Connection* connection, const char* rawurl,
                                          const char* method, const char* version, const char* upload_data,
                                          size_t* upload_data_size, void** con_cls)
{
    if (gVerbosity >= 1)
        std::cerr << "\n==> ANSWER CONNECTION (" << rawurl << ", " << method << ", " << version << ")" << std::endl;
    string URL = simplifyURL(rawurl);

    FaustServer* server = (FaustServer*)cls;
    if (server == 0) {
        std::cerr << "ERROR: REAL BAD ERROR, NO SERVER !!!" << std::endl;
        exit(1);
    }
    try {
        if (0 == strcmp(method, "GET")) {
            return server->dispatchGETConnections(connection, URL);
        } else if (0 == strcmp(method, "POST")) {
            return server->dispatchPOSTConnections(connection, URL, upload_data, upload_data_size, con_cls);
        } else {
            return send_page(connection, errorpage.c_str(), errorpage.size(), MHD_HTTP_BAD_REQUEST, "text/html");
        }
    } catch (exception const& e) {
        std::cerr << "ERROR: GLOBAL ERROR CATCHED " << e.what() << std::endl;
        return send_page(connection, errorpage.c_str(), errorpage.size(), MHD_HTTP_BAD_REQUEST, "text/html");
    }
}

//------------------------------------------------------------------
// Actual callback method called every time a GET or POST request is received.
// by the server.

int FaustServer::dispatchGETConnections(struct MHD_Connection* connection, const string& url)
{
    // TArgs args;
    // MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, get_params, &args);
    if (gVerbosity >= 2) std::cerr << "ANSWER GET CONNECTION " << url << std::endl;

    if (matchExtension(url, ".php") || matchExtension(url, ".js")) {
        return page_not_found(connection, "/favicon.ico", 12, "image/x-icon");

    } else if (matchURL(url, "/")) {
        stringstream ss;
        ss << askpage_head << nr_of_uploading_clients << askpage_tail;
        return send_page(connection, ss.str().c_str(), ss.str().size(), MHD_HTTP_OK, "text/html");

    } else if (matchURL(url, "/targets")) {
        return send_page(connection, fTargets.c_str(), fTargets.size(), MHD_HTTP_OK, "application/json");

    } else if (matchURL(url, "/verbosity0")) {
        gVerbosity = 0;
        stringstream ss;
        ss << "Verbosity " << gVerbosity;
        return send_page(connection, ss.str().c_str(), ss.str().size(), MHD_HTTP_OK, "text/html");

    } else if (matchURL(url, "/verbosity1")) {
        gVerbosity = 1;
        stringstream ss;
        ss << "Verbosity " << gVerbosity;
        return send_page(connection, ss.str().c_str(), ss.str().size(), MHD_HTTP_OK, "text/html");

    } else if (matchURL(url, "/verbosity2")) {
        gVerbosity = 2;
        stringstream ss;
        ss << "Verbosity " << gVerbosity;
        return send_page(connection, ss.str().c_str(), ss.str().size(), MHD_HTTP_OK, "text/html");

        /*
            } else if (matchURL(url, "/crash1")) {
                // simulate crash -- to be removed in production
                exit(-1);

            } else if (matchURL(url, "/crash2")) {
                // simulate crash -- to be removed in production
                int* p = 0;
                *p     = 5 / (*p);
                return page_not_found(connection, "/crash2", 7, "image/x-icon");
        */
    } else if (matchURL(url, "/*/*/*/installer.sh")) {
        return makeAndSendResourceFile(connection, url);

    } else if (matchURL(url, "/*/*/*/binary.zip")) {
        return makeAndSendResourceFile(connection, url);

    } else if (matchURL(url, "/*/*/*/precompile")) {
        return makeAndSendResourceFile(connection, url);

    } else if (matchURL(url, "/*/*/*/binary.apk")) {
        return makeAndSendResourceFile(connection, url);

    } else if (matchURL(url, "/*/diagram/*") && matchExtension(url, ".svg")) {
        return makeAndSendResourceFile(connection, url);

    } else if (matchURL(url, "/favicon.ico")) {
        return page_not_found(connection, "/favicon.ico", 12, "image/x-icon");

    } else {
        if (gVerbosity >= 1) std::cerr << "WARNING: We should have a rule to match this URL " << url << std::endl;
        return page_not_found(connection, "/favicon.ico", 12, "image/x-icon");
    }
}

//------------------------------------------------------------------
// Handle a GET command by "making" the appropriate resource file
// and returning it

int FaustServer::makeAndSendResourceFile(struct MHD_Connection* connection, const string& raw_url)
{
    vector<string> U        = decomposeURL(raw_url);
    fs::path       url      = fs::path(raw_url);
    fs::path       fulldir  = this->getDirectory() / url.parent_path();
    fs::path       target   = url.filename();
    fs::path       makefile = fulldir / "Makefile";
    const char*    mimetype;
    bool           precompile = false;

    if (gVerbosity >= 2) std::cerr << "\nUSING SESSION " << U[1] << "\n\n";
    fSessionCache.refer(U[1]);

    // Check for svg block-diagram requests first
    if (url.extension() == ".svg") {
        if (gVerbosity >= 2) std::cerr << "Processing SVG file request" << std::endl;
        fs::path fullfile = fulldir / target;
        if (!boost::filesystem::exists(fullfile)) {
            if (gVerbosity >= 2) std::cerr << "Diagram not created yet" << std::endl;
            make(fullfile.parent_path().parent_path(), fs::path("diagram"));
        }
        return send_file(connection, fullfile, "image/svg+xml");
    }

    // Check if we are doing only a pre-compilation (without actually downloading the compiled file)
    if (target == "precompile") {
        precompile = true;
        target     = "binary.zip";
    }

    // Analyze possible cases of errors
    if (!isValidTarget(target, mimetype)) {
        std::cerr << "Error : not a valid target " << target << " in raw_url " << raw_url << std::endl;
        return send_page(connection, invalidinstruction.c_str(), invalidinstruction.size(), MHD_HTTP_BAD_REQUEST,
                         "text/html");
    } else if (!fs::is_regular_file(makefile)) {
        std::cerr << "Error : No makefile found "
                  << " in raw_url " << raw_url << std::endl;
        return send_page(connection, invalidinstruction.c_str(), invalidinstruction.size(), MHD_HTTP_BAD_REQUEST,
                         "text/html");
    }

    // we can call make
    fs::path filename = make(fulldir, target);

    if (!fs::is_regular_file(filename)) {
        std::cerr << "Error : Make Failed "
                  << " in raw_url " << raw_url << std::endl;
        return send_page(connection, cannotcompile.c_str(), cannotcompile.size(), MHD_HTTP_BAD_REQUEST, "text/html");
    } else {
        if (!precompile) {
            return send_file(connection, filename, mimetype);
        } else {
            return send_page(connection, "DONE", 4, MHD_HTTP_OK, "text/html");
        }
    }
}

//------------------------------------------------------------------
// dispatchPOSTConnections(), handle POST of a Faust source file

int FaustServer::dispatchPOSTConnections(struct MHD_Connection* connection, const string& url, const char* upload_data,
                                         size_t* upload_data_size, void** con_cls)
{
    time_t tmNow = time(0);

    std::cerr << "New POST connection: " << ctime(&tmNow) << " URL: " << url << std::endl;
    if (gVerbosity >= 2) std::cerr << "ANSWER POST CONNECTION " << url << std::endl;
    if (NULL == *con_cls) {
        if (gVerbosity >= 2) std::cerr << "PRE POST processing" << std::endl;
        struct connection_info_struct* con_info;

        if (nr_of_uploading_clients >= this->getMaxClients()) {
            return send_page(connection, busypage.c_str(), busypage.size(), MHD_HTTP_SERVICE_UNAVAILABLE, "text/html");
        }

        con_info = new connection_info_struct();
        if (NULL == con_info) {
            return MHD_NO;
        }
        con_info->directory          = this->getDirectory().string();
        con_info->makefile_directory = this->getMakefileDirectory().string();

        con_info->fp = NULL;
        con_info->postprocessor =
            MHD_create_post_processor(connection, POSTBUFFERSIZE, (MHD_PostDataIterator)iterate_post, (void*)con_info);

        if (NULL == con_info->postprocessor) {
            delete con_info;
            return MHD_NO;
        }

        nr_of_uploading_clients++;

        con_info->connectiontype = POST;
        con_info->answercode     = MHD_HTTP_OK;
        con_info->answerstring   = errorpage;

        *con_cls = (void*)con_info;

        return MHD_YES;

    } else {
        if (gVerbosity >= 2) std::cerr << "HEY! POST processing" << std::endl;
        struct connection_info_struct* con_info = (connection_info_struct*)*con_cls;

        if (0 != *upload_data_size) {
            if (gVerbosity >= 2) std::cerr << "POST processing, we have data to upload !" << std::endl;
            int result        = MHD_post_process(con_info->postprocessor, upload_data, *upload_data_size);
            *upload_data_size = 0;
            return result;
        } else {
            if (gVerbosity >= 2) std::cerr << "POST processing, NO MORE data to upload !" << std::endl;
            // need to close the file before request_completed
            // so that it can be opened by the methods below
            if (con_info->fp) {
                if (gVerbosity >= 2) std::cerr << "POST processing, We can close the file !" << std::endl;
                fclose(con_info->fp);
                con_info->fp = 0;
            }

            string sha1;
            if (validate_faust(con_info) == 0) {
                // warning validate_faust returns errocode 0 when the faust code is correct !
                std::vector<std::string> segments;
                sha1 = generate_sha1(con_info);  // TODO: Duplicated computation of the SHA key
                if (gVerbosity >= 2)
                    if (gVerbosity >= 1)
                        std::cerr << "POST processing, We have valid faust code. Its SHA1 key is = " << sha1
                                  << std::endl;
                if (matchURL(url, "/compile/*/*/*", segments)) {
                    string newurl("/");
                    newurl += sha1 + "/" + segments[2] + "/" + segments[3] + "/" + segments[4];
                    if (gVerbosity >= 2)
                        std::cerr << "DIRECT COMPILATION: We are trying a direct compilation at " << newurl << endl;
                    return makeAndSendResourceFile(connection, newurl.c_str());
                } else {
                    return send_page(connection, con_info->answerstring.c_str(), con_info->answerstring.size(),
                                     con_info->answercode, "text/html");
                }
            } else {
                std::cerr << "POST processing, We have an INVALID faust code" << std::endl;
                std::string errormessage = "Invalid Faust Code";
                return send_page(connection, errormessage.c_str(), errormessage.size(), con_info->answercode,
                                 "text/html");
            }
        }

        std::cerr << "NEVER EXECUTED" << std::endl;
        return send_page(connection, errorpage.c_str(), errorpage.size(), MHD_HTTP_BAD_REQUEST, "text/html");
    }
}

/*
 * Callback called every time a POST request comes in by the postprocessor.
 * To understand more about how postprocessors work, consult the microhttpd
 * documentation.
 */

int FaustServer::iterate_post(void* coninfo_cls, enum MHD_ValueKind kind, const char* key, const char* filename,
                              const char* content_type, const char* /*transfer_encoding*/, const char* data,
                              uint64_t /*off*/, size_t size)
{
    struct connection_info_struct* con_info = (connection_info_struct*)coninfo_cls;
    FILE*                          fp;

    if (gVerbosity >= 2) {
        std::cerr << "ENTER iterate_post ("
                  << "kind : " << kind << ", key : " << key << ", filename: " << filename << ", content type: "
                  << content_type
                  //<< ", transfer_encoding: " << transfer_encoding
                  << ", data pointer: " << (void*)data << ", size: " << size << ")" << std::endl;
    }
    if (con_info->tmppath.empty()) {
        con_info->filename = filename;
        con_info->tmppath  = (fs::temp_directory_path() / fs::unique_path("%%%%-%%%%-%%%%-%%%%")).string();
        fs::create_directory(con_info->tmppath);
    }

    string full_path = (fs::path(con_info->tmppath) / fs::path(con_info->filename)).string();

    con_info->answerstring = servererrorpage;
    con_info->answercode   = MHD_HTTP_INTERNAL_SERVER_ERROR;

    if (0 != strcmp(key, "file")) {
        if (gVerbosity >= 2) std::cerr << __LINE__ << " FaustServer::iterate_post" << std::endl;
        return MHD_NO;
    }

    if (con_info->fp == 0) {
        if (NULL != (fp = fopen(full_path.c_str(), "rb"))) {
            fclose(fp);

            con_info->answerstring = fileexistspage;
            con_info->answercode   = MHD_HTTP_FORBIDDEN;
            if (gVerbosity >= 2) std::cerr << __LINE__ << " FaustServer::iterate_post" << std::endl;
            return MHD_NO;
        }

        con_info->fp = fopen(full_path.c_str(), "ab");
        if (con_info->fp == 0) {
            if (gVerbosity >= 2) std::cerr << __LINE__ << " FaustServer::iterate_post" << std::endl;
            return MHD_NO;
        }
    }

    if (size > 0) {
        if (!fwrite(data, size, sizeof(char), con_info->fp)) {
            if (gVerbosity >= 2) std::cerr << __LINE__ << " FaustServer::iterate_post" << std::endl;
            return MHD_NO;
        }
    }

    con_info->answercode = MHD_HTTP_OK;

    return MHD_YES;
}

FaustServer::FaustServer(int port, int max_clients, const fs::path& directory, const fs::path& makefile_directory,
                         const fs::path& logfile, int maxSessions)
    : fPort(port),
      fMaxClients(max_clients),
      fDirectory(directory),
      fMakefileDirectory(makefile_directory),
      fLogfile(logfile),
      fDaemon(0),
      fTargets(""),
      fSessionCache(directory, maxSessions)
{
    // create a string containing the list possible targets by scanning the makefile directory. The list is
    // JSON formatted :   { "os1" : ["arch11", "arch12", ...],  "os2" : ["arch21", "arch22", ...], ...}
    // and stored in field targets
    std::stringstream ss;

    ss << '{';
    char sep1 = ' ';

    fs::directory_iterator end_iter;
    for (fs::directory_iterator os_iter(makefile_directory); os_iter != end_iter; ++os_iter) {
        if (fs::is_directory(os_iter->path())) {
            string OSname = os_iter->path().filename().string();

            // collect a vector of target names
            vector<string> V;
            for (fs::directory_iterator makefile_iter(os_iter->path()); makefile_iter != end_iter; ++makefile_iter) {
                string makefileName = makefile_iter->path().filename().string();
                if (makefileName.substr(0, 9) == "Makefile.") {
                    V.push_back(makefileName.substr(9));
                }
            }

            // If it is a folder with makefiles inside, print it
            if (V.size() > 0) {
                std::sort(V.begin(), V.end());
                ss << sep1 << std::endl << '"' << OSname << '"' << ": [";
                for (unsigned int i = 0; i < V.size(); i++) {
                    if (i > 0) ss << ',';
                    ss << '"' << V[i] << '"';
                }
                ss << ']';
                sep1 = ',';
            }
        }
    }
    ss << std::endl << "}";
    fTargets = ss.str();
}
