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
#include <cassert>
#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <regex>
#include <set>
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

#include "content.hh"
#include "faustOptions.hh"
#include "htmlPages.hh"
#include "match.hh"
#include "server.hh"
#include "utilities.hh"

// to use command line tools
#include <stdlib.h>

// Avoid using namespace std for better code clarity
namespace fs = std::filesystem;

extern int gVerbosity;
/*
 * Various responses to GET requests
 */

extern bool gAnyOrigin;  // when true adds Access-Control-Allow-Origin to http answers

#define MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN "Access-Control-Allow-Origin"

/*
 * Generates an SHA-1 key for Faust file or archive.
 */

static std::string generate_sha1(connection_info_struct* con_info)
{
    fs::path filepath = fs::path(con_info->tmppath) / fs::path(con_info->filename);

    return generateFileSHA1(filepath);
}

/*
 * Validates that a Faust file or archive is sane and returns 0 for success
 * or 1 for failure. If the evaluation fails, the appropriate error message
 * is set. More info on the con_info structure is in server.hh.
 * Create the session directory <sha>/ with the following structure:
 * sessions/<sha>/
 *   sourcecode/
 *     <filename>.dsp
 *     ...other content if from zip
 *   user_code.dsp      (a copy of sourcecode/<filename>.dsp)
 *   generated.cpp      (the generated C++ code)
 *   errors.log         (any errors that occurred during compilation)
 *   svg/               (the resulting svg blockdiagrams)
 *   targets/           (platform/architecture specific compilation directories)
 *     <platform>/<architecture>/
 *       Makefile
 *       <filename>.dsp   (copied from sourcecode/)
 *       ...other source files
 *
 * the compilation command is :
 *   cd sourcecode/
 *     && cp <filename>.dsp ../user_code.dsp
 *     && faust <filename>.dsp -o ../generated.cpp -svg 2> ../errors.log";
 *
 * if no compilation errors where produced then errors.log is empty
 * in this case we can move sourcecode/<filename>-svg/ to svg/
 * and available :
 *   <sha>/user_code.dsp
 *   <sha>/generated.cpp
 *   <sha>/svg/process.svg
 *   <sha>/errors.log       (empty)
 *
 * if some compilation errors where produced then errors.log is not empty and are only available :
 *   <sha>/user_code.dsp
 *   <sha>/errors.log
 *
 */

static int validate_faust(connection_info_struct* con_info)
{
    fs::path filename      = fs::path(con_info->filename);
    fs::path uploaded_file = fs::path(con_info->tmppath) / filename;

    if (gVerbosity >= 1) {
        std::cerr << "\nENTER validate_faust for file: " << uploaded_file << std::endl;
    }

    // Generate SHA1 for session directory
    std::string sha1            = generate_sha1(con_info);
    fs::path    session_path    = fs::path(con_info->directory) / fs::path(sha1);
    fs::path    sourcecode_path = session_path / "sourcecode";

    if (gVerbosity >= 1) {
        std::cerr << "Session creation for: " << sha1 << std::endl;
    }

    // Create session structure
    if (!fs::is_directory(session_path)) {
        try {
            fs::create_directories(sourcecode_path);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error: can't create directory " << sourcecode_path << ": "
                      << e.code().message() << std::endl;
            con_info->answerstring = completebuterrorpage;
            return 1;
        }

        std::string main_dsp_filename;

        // Handle file upload: simple DSP file or archive
        if (!fs::is_regular_file(uploaded_file)) {
            con_info->answerstring = completebuterrorpage;
            if (gVerbosity >= 1) {
                std::cerr << "EXIT validate_faust: not regular file: " << uploaded_file
                          << std::endl;
            }
            return 1;
        }

        if (filename.extension() == ".dsp") {
            // Simple DSP file: copy to sourcecode/ with original name
            main_dsp_filename = filename.filename().string();
            fs::copy_file(uploaded_file, sourcecode_path / main_dsp_filename);
            if (gVerbosity >= 1) {
                std::cerr << "Copied DSP file: " << main_dsp_filename << std::endl;
            }
        } else {
            // Archive: extract to sourcecode/ and find main DSP file
            struct archive*          archive;
            struct archive_entry*    entry;
            std::vector<std::string> dsp_files;

            archive = archive_read_new();
            archive_read_support_filter_all(archive);
            archive_read_support_format_all(archive);
            int status = archive_read_open_filename(archive, uploaded_file.string().c_str(), 10240);

            if (status != ARCHIVE_OK) {
                con_info->answerstring = completebutdecompressionproblem;
                archive_read_free(archive);
                return 1;
            }

            // Extract all files with multiple protections against zip bombs
            const size_t MAX_EXTRACTED_SIZE  = 5 * 102400;               // 500KB limit
            const size_t MAX_ENTRIES         = 100;                      // 100 files max
            const auto   MAX_EXTRACTION_TIME = std::chrono::seconds(5);  // 5 seconds max

            size_t total_extracted        = 0;
            size_t entry_count            = 0;
            auto   start_time             = std::chrono::steady_clock::now();
            bool   size_limit_exceeded    = false;
            bool   time_limit_exceeded    = false;
            bool   entries_limit_exceeded = false;

            while (archive_read_next_header(archive, &entry) == ARCHIVE_OK) {
                // Check time limit
                auto current_time = std::chrono::steady_clock::now();
                if (current_time - start_time > MAX_EXTRACTION_TIME) {
                    if (gVerbosity >= 1) {
                        std::cerr << "Extraction time limit exceeded - stopping after "
                                  << entry_count << " entries" << std::endl;
                    }
                    time_limit_exceeded = true;
                    break;
                }

                // Check entries limit
                if (++entry_count > MAX_ENTRIES) {
                    if (gVerbosity >= 1) {
                        std::cerr << "Too many entries - stopping at " << MAX_ENTRIES << " files"
                                  << std::endl;
                    }
                    entries_limit_exceeded = true;
                    break;
                }

                fs::path entry_path = fs::path(archive_entry_pathname(entry));

                // SECURITY: Skip files with unsafe names (silent removal)
                std::string entry_filename = entry_path.filename().string();
                if (!entry_filename.empty() && !isSecureFilename(entry_filename)) {
                    if (gVerbosity >= 1) {
                        std::cerr << "SECURITY: Silently removing unsafe file from ZIP: "
                                  << entry_filename << std::endl;
                    }
                    entry_count--;  // Don't count removed files
                    continue;
                }

                // Skip system files
                if (entry_path.string().substr(0, 8) == "__MACOSX") {
                    if (gVerbosity >= 1) {
                        std::cerr << "Ignoring: " << entry_path << std::endl;
                    }
                    entry_count--;  // Don't count ignored files
                    continue;
                }

                // Check extraction size limit before processing
                size_t entry_size = archive_entry_size(entry);
                if (total_extracted + entry_size > MAX_EXTRACTED_SIZE) {
                    if (gVerbosity >= 1) {
                        std::cerr << "Extraction size limit exceeded - stopping at "
                                  << total_extracted << " bytes" << std::endl;
                    }
                    size_limit_exceeded = true;
                    break;
                }

                // Collect DSP files
                if (entry_path.extension() == ".dsp") {
                    dsp_files.push_back(entry_path.filename().string());
                    if (main_dsp_filename.empty()) {
                        main_dsp_filename = entry_path.filename().string();
                    }
                }

                // Extract file to sourcecode/
                fs::path dest_path = sourcecode_path / entry_path;
                fs::create_directories(dest_path.parent_path());
                archive_entry_set_pathname(entry, dest_path.string().c_str());
                archive_read_extract(archive, entry, ARCHIVE_EXTRACT_PERM);

                total_extracted += entry_size;
            }

            archive_read_free(archive);

            // Handle time limit exceeded case
            if (time_limit_exceeded) {
                if (gVerbosity >= 1) {
                    std::cerr << "Archive extraction timeout - creating toolong.dsp" << std::endl;
                }

                // Clear sourcecode directory
                fs::remove_all(sourcecode_path);
                fs::create_directories(sourcecode_path);

                // Create toolong.dsp with error message
                main_dsp_filename              = "toolong.dsp";
                fs::path      toolong_dsp_path = sourcecode_path / main_dsp_filename;
                std::ofstream toolong_file(toolong_dsp_path);
                toolong_file << "// archive took too long to decompress\n";
                toolong_file << "// maximum allowed: 5 seconds\n";
                toolong_file.close();
            }
            // Handle too many entries case
            else if (entries_limit_exceeded) {
                if (gVerbosity >= 1) {
                    std::cerr << "Too many entries - creating toomanyentries.dsp" << std::endl;
                }

                // Clear sourcecode directory
                fs::remove_all(sourcecode_path);
                fs::create_directories(sourcecode_path);

                // Create toomanyentries.dsp with error message
                main_dsp_filename                     = "toomanyentries.dsp";
                fs::path      toomanyentries_dsp_path = sourcecode_path / main_dsp_filename;
                std::ofstream toomanyentries_file(toomanyentries_dsp_path);
                toomanyentries_file << "// too many files in archive\n";
                toomanyentries_file << "// maximum allowed: 100 files\n";
                toomanyentries_file.close();
            }
            // Handle size limit exceeded case
            else if (size_limit_exceeded) {
                if (gVerbosity >= 1) {
                    std::cerr << "Archive too large - creating toobig.dsp" << std::endl;
                }

                // Clear sourcecode directory
                fs::remove_all(sourcecode_path);
                fs::create_directories(sourcecode_path);

                // Create toobig.dsp with error message
                main_dsp_filename             = "toobig.dsp";
                fs::path      toobig_dsp_path = sourcecode_path / main_dsp_filename;
                std::ofstream toobig_file(toobig_dsp_path);
                toobig_file << "// archive too large when decompressed\n";
                toobig_file << "// maximum allowed: 500KB\n";
                toobig_file.close();
            }
            // Handle multiple DSP files case
            else if (dsp_files.size() > 1) {
                if (gVerbosity >= 1) {
                    std::cerr << "Multiple DSP files found (" << dsp_files.size()
                              << ") - creating multi.dsp" << std::endl;
                }

                // Clear sourcecode directory
                fs::remove_all(sourcecode_path);
                fs::create_directories(sourcecode_path);

                // Create multi.dsp with error message
                main_dsp_filename            = "multi.dsp";
                fs::path      multi_dsp_path = sourcecode_path / main_dsp_filename;
                std::ofstream multi_file(multi_dsp_path);
                multi_file << "// more than one DSP file in your archive\n";
                multi_file << "// Found files: ";
                for (size_t i = 0; i < dsp_files.size(); ++i) {
                    if (i > 0) {
                        multi_file << ", ";
                    }
                    multi_file << dsp_files[i];
                }
                multi_file << "\n";
                multi_file.close();
            }
        }

        // Verify we found a main DSP file
        if (main_dsp_filename.empty()) {
            // Create empty.dsp with error message instead of failing
            main_dsp_filename            = "empty.dsp";
            fs::path      empty_dsp_path = sourcecode_path / main_dsp_filename;
            std::ofstream empty_file(empty_dsp_path);
            empty_file << "// no dsp file was provided\n";
            empty_file.close();
            if (gVerbosity >= 1) {
                std::cerr << "No DSP file found - created empty.dsp with error message"
                          << std::endl;
            }
        }

        // SINGLE COMPILATION: Copy main DSP file and compile in one step
        if (gVerbosity >= 2) {
            std::cerr << "SINGLE COMPILATION validate_faust" << std::endl;
        }

        // First, copy the DSP file
        std::string copy_cmd =
            "cd " + sourcecode_path.string() + " && cp " + main_dsp_filename + " ../user_code.dsp";
        if (gVerbosity >= 2) {
            std::cerr << "Executing copy: " << copy_cmd << std::endl;
        }

        int copy_result = system(copy_cmd.c_str());
        if (copy_result != 0) {
            if (gVerbosity >= 1) {
                std::cerr << "Failed to copy DSP file" << std::endl;
            }
            con_info->answerstring = completebutnopipe;
            return 1;
        }

        // Extract and process faustoptions
        fs::path    user_dsp_file    = session_path / "user_code.dsp";
        std::string raw_options      = extractFaustOptions(user_dsp_file);
        std::string filtered_options = filterFaustOptions(raw_options);

        // Create faustoptions.txt file
        fs::path      faustoptions_file = session_path / "faustoptions.txt";
        std::ofstream options_out(faustoptions_file);
        if (options_out.is_open()) {
            options_out << filtered_options;
            options_out.close();
            if (gVerbosity >= 2) {
                std::cerr << "Created faustoptions.txt with: '" << filtered_options << "'"
                          << std::endl;
            }
        }

        // Now compile using the extracted options
        std::string options_str = readFaustOptions(session_path);
        // SECURITY: Protect against stack overflow and resource exhaustion
        // - ulimit -s 8192: Limit stack size to 8MB (default is often 8MB on macOS, can be much
        // larger on Linux)
        // - ulimit -t 30: Limit CPU time to 30 seconds
        // Note: ulimit -v (virtual memory) not supported on macOS, removed for compatibility
        std::string faust_cmd = "cd " + sourcecode_path.string() +
                                " && (ulimit -s 8192; ulimit -t 30; faust " + options_str +
                                (options_str.empty() ? "" : " ") + main_dsp_filename +
                                " -o ../generated.cpp -svg 2> ../errors.log)";

        if (gVerbosity >= 2) {
            std::cerr << "Executing compilation: " << faust_cmd << std::endl;
        }

        // Execute Faust compilation
        FILE* faust_process = popen(faust_cmd.c_str(), "r");
        if (!faust_process) {
            if (gVerbosity >= 1) {
                std::cerr << "Cannot launch faust command" << std::endl;
            }
            con_info->answerstring = completebutnopipe;
            return 1;
        }

        // Read command output
        std::string compilation_output;
        char        buffer[1024];
        while (fgets(buffer, sizeof(buffer), faust_process) != nullptr) {
            compilation_output += buffer;
        }

        int return_code = pclose(faust_process);
        if (return_code != 0) {
            if (gVerbosity >= 1) {
                std::cerr << "Faust compilation failed with code " << return_code << std::endl;
                std::cerr << "Output: " << compilation_output << std::endl;
            }
            // Continue processing even with compilation errors - errors.log will contain the
            // details Client will check errors.log to determine if there were compilation errors
        }

        // Move generated SVG files to dedicated svg/ directory
        fs::path svg_path = session_path / "svg";
        try {
            fs::create_directories(svg_path);

            // Look for SVG files in sourcecode/ directory and its subdirectories
            for (const auto& entry : fs::recursive_directory_iterator(sourcecode_path)) {
                if (entry.is_regular_file() && entry.path().extension() == ".svg") {
                    fs::path svg_file = svg_path / entry.path().filename();
                    fs::rename(entry.path(), svg_file);
                    if (gVerbosity >= 2) {
                        std::cerr << "Moved SVG: \"" << entry.path().filename().string()
                                  << "\" to svg/" << std::endl;
                    }
                }
            }

            // Clean up empty SVG directories in sourcecode/
            for (const auto& entry : fs::directory_iterator(sourcecode_path)) {
                if (entry.is_directory() &&
                    entry.path().filename().string().find("-svg") != std::string::npos) {
                    if (fs::is_empty(entry.path())) {
                        fs::remove(entry.path());
                        if (gVerbosity >= 1) {
                            std::cerr << "Removed empty SVG directory: "
                                      << entry.path().filename().string() << std::endl;
                        }
                    }
                }
            }
        } catch (const fs::filesystem_error& e) {
            if (gVerbosity >= 1) {
                std::cerr << "Warning: Could not organize SVG files: " << e.code().message()
                          << std::endl;
            }
        }

        // Create metadata.json
        fs::path metadata_file = session_path / "metadata.json";
        try {
            std::ofstream metadata(metadata_file);
            metadata << "{\n";
            metadata << "  \"sha1\": \"" << sha1 << "\",\n";
            metadata << "  \"filename\": \"" << con_info->filename << "\",\n";
            metadata << "  \"compilation_time\": \"" << std::time(nullptr) << "\",\n";
            metadata << "  \"enhanced\": true,\n";
            metadata << "  \"single_compilation\": true,\n";
            metadata << "  \"structure\": {\n";
            metadata << "    \"sourcecode/\": \"Original source files\",\n";
            metadata << "    \"generated.cpp\": \"Generated C++ code\",\n";
            metadata << "    \"svg/\": \"Block diagram SVG files\",\n";
            metadata << "    \"metadata.json\": \"This file\"\n";
            metadata << "  }\n";
            metadata << "}\n";
            metadata.close();
        } catch (const fs::filesystem_error& e) {
            if (gVerbosity >= 1) {
                std::cerr << "Warning: Could not create metadata file: " << e.code().message()
                          << std::endl;
            }
        }

        // Save original filename for drag-and-drop functionality
        try {
            fs::path      filename_path = session_path / "filename.txt";
            std::ofstream filename_file(filename_path);
            filename_file << con_info->filename << std::endl;
            filename_file.close();
            if (gVerbosity >= 2) {
                std::cerr << "Saved original filename: " << con_info->filename << std::endl;
            }
        } catch (const fs::filesystem_error& e) {
            if (gVerbosity >= 1) {
                std::cerr << "Warning: Could not save filename: " << e.code().message()
                          << std::endl;
            }
        }

        // Create backward-compatible makefile structure
        create_basic_session(sourcecode_path, session_path, fs::path(con_info->makefile_directory));

        if (gVerbosity >= 2) {
            std::cerr << "Session structure created successfully for: " << sha1 << std::endl;
        }
    }

    con_info->answerstring = sha1;

    if (gVerbosity >= 2) {
        std::cerr << "EXIT validate_faust OK" << std::endl;
    }
    return 0;
}

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

    if (gVerbosity >= 2) {
        std::cerr << "ENTER MAKE " << ss.str() << std::endl;
    }
    if (0 == system(ss.str().c_str())) {
        p = dir / target;
    } else {
        if (gVerbosity >= 2) {
            std::cerr << __LINE__ << " makefile " << dir / target << " failed !!!" << std::endl;
        }
        p = "";
    }
    if (gVerbosity >= 2) {
        std::cerr << "EXIT MAKE " << p << std::endl;
    }

    return p;
}

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

int FaustServer::send_page(struct MHD_Connection* connection, const char* page, int length,
                           int status_code, const char* type, const char* location)
{
    struct MHD_Response* response =
        MHD_create_response_from_buffer(length, (void*)page, MHD_RESPMEM_MUST_COPY);

    if (response == 0) {
        return MHD_NO;
    }
    // Add security headers
    MHD_add_response_header(
        response, "Content-Security-Policy",
        "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; "
        "img-src 'self'; frame-ancestors "
        "'none'; form-action 'self';");
    MHD_add_response_header(response, "X-Frame-Options", "DENY");
    MHD_add_response_header(response, "X-Content-Type-Options", "nosniff");

    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, type ? type : "text/plain");
    if (location) {
        MHD_add_response_header(response, MHD_HTTP_HEADER_LOCATION, location);
        MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_EXPOSE_HEADERS,
                                MHD_HTTP_HEADER_LOCATION);
    }
    if (gAnyOrigin) {
        MHD_add_response_header(response, MHD_HTTP_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN, "*");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    }
    int ret = MHD_queue_response(connection, status_code, response);
    MHD_destroy_response(response);

    return ret;
}

/*
 * Callback called every time a GET or POST request is completed.
 * Note that this is NOT necessarily called once the entirety of
 * POST data is transfered. If the data is transfered in chunks,
 * this is called after every chunk.
 */

void FaustServer::request_completed(void*, struct MHD_Connection*, void** con_cls,
                                    enum MHD_RequestTerminationCode)
{
    if (gVerbosity >= 2) {
        std::cerr << "FaustServer::request_completed()" << endl;
    }

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

    } else if (target == "baresrc.cpp") {
        mimetype = "text/x-c";
        return true;

    } else if (target == "svg.zip") {
        mimetype = "application/zip";
        return true;

    } else if (target == "webapp.zip") {
        mimetype = "application/zip";
        return true;

    } else if (target == "mdoc.zip") {
        mimetype = "application/zip";
        return true;

    } else if (target == "installer.sh") {
        mimetype = "application/x-shellscript";
        return true;

    } else if (target == "index.html") {
        mimetype = "text/html";
        return true;

    } else {
        // For PWA assets, check file extension
        std::string extension = target.extension().string();
        if (extension == ".html") {
            mimetype = "text/html";
            return true;
        } else if (extension == ".js") {
            mimetype = "application/javascript";
            return true;
        } else if (extension == ".css") {
            mimetype = "text/css";
            return true;
        } else if (extension == ".json") {
            mimetype = "application/json";
            return true;
        } else if (extension == ".wasm") {
            mimetype = "application/wasm";
            return true;
        } else if (extension == ".png") {
            mimetype = "image/png";
            return true;
        } else if (extension == ".svg") {
            mimetype = "image/svg+xml";
            return true;
        } else if (extension == ".map") {
            mimetype = "application/json";
            return true;
        } else {
            return false;
        }
    }
}

/*
 * Function that sends a file in response to a GET
 */

int FaustServer::send_file(struct MHD_Connection* connection, const fs::path& filepath,
                           const char* mimetype)
{
    struct stat sbuf;
    int         fd;

    if (gVerbosity >= 2) {
        std::cerr << __LINE__ << " ENTER send_file : " << filepath << endl;
    }

    if ((-1 == (fd = open(filepath.string().c_str(), O_RDONLY))) || (0 != fstat(fd, &sbuf))) {
        std::cerr << __LINE__ << " error accessing file : " << filepath << endl;
        return send_page(connection, cannotcompile.c_str(), cannotcompile.size(),
                         MHD_HTTP_BAD_REQUEST, "text/html");
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

    if (gVerbosity >= 2) {
        std::cerr << __LINE__ << " EXIT send_file : " << filepath << endl;
    }
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
    fDaemon =
        MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, fPort, NULL, NULL,
                         (MHD_AccessHandlerCallback)&staticAnswerToConnection, this,
                         MHD_OPTION_NOTIFY_COMPLETED, request_completed, NULL, MHD_OPTION_END);

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

int FaustServer::staticAnswerToConnection(void* cls, struct MHD_Connection* connection,
                                          const char* rawurl, const char* method,
                                          const char* version, const char* upload_data,
                                          size_t* upload_data_size, void** con_cls)
{
    if (gVerbosity >= 1) {
        std::cerr << "\n==> ANSWER CONNECTION (" << rawurl << ", " << method << ", " << version
                  << ")" << std::endl;
    }
    std::string URL = simplifyURL(rawurl);

    FaustServer* server = (FaustServer*)cls;
    if (server == 0) {
        std::cerr << "ERROR: REAL BAD ERROR, NO SERVER !!!" << std::endl;
        exit(1);
    }
    try {
        if (0 == strcmp(method, "GET")) {
            return server->dispatchGETConnections(connection, URL);
        } else if (0 == strcmp(method, "POST")) {
            if (gVerbosity >= 2) {
                struct connection_info_struct* con_info = (connection_info_struct*)*con_cls;
                if (con_info) {
                    std::cerr << "Content of con_cls:" << std::endl;
                    std::cerr << "  Directory: " << '"' << con_info->directory << '"' << std::endl;
                    std::cerr << "  Makefile Directory: " << '"' << con_info->makefile_directory
                              << '"' << std::endl;
                    std::cerr << "  Filename: " << '"' << con_info->filename << '"' << std::endl;
                    std::cerr << "  TmpPath: " << '"' << con_info->tmppath << '"' << std::endl;
                    // Add more fields as needed
                } else {
                    std::cerr << "con_cls is NULL" << std::endl;
                }
            }
            return server->dispatchPOSTConnections(connection, URL, upload_data, upload_data_size,
                                                   con_cls);
        } else {
            return send_page(connection, errorpage.c_str(), errorpage.size(), MHD_HTTP_BAD_REQUEST,
                             "text/html");
        }
    } catch (exception const& e) {
        std::cerr << "ERROR: GLOBAL ERROR CATCHED " << e.what() << std::endl;
        return send_page(connection, errorpage.c_str(), errorpage.size(), MHD_HTTP_BAD_REQUEST,
                         "text/html");
    }
}

//
// Get the /precompile downloadable artifact name from the makefile
//
// The first line of the makefile is interrogated - if it is of the form
//
//    #FaustBinaryTarget: <artifact name>
//
// Then we use that. In all other cases we return "binary.zip", the original
// hard-coded value.
//
// Note that the artifact name still has to be a supported value - it can't be
// just anything
//
static const std::regex makefile_target_regex(R"(^#FaustBinaryTarget:\s*([^\s]+)\s*$)");
std::string             FaustServer::getMakefileArtifactName(const fs::path& make_file_path)
{
    std::ifstream file(make_file_path);
    if (file.is_open()) {
        std::string first_line;
        std::getline(file, first_line);
        file.close();

        std::smatch matches;
        if (std::regex_search(first_line, matches, makefile_target_regex)) {
            return matches.str(1);
        }
    }
    return "binary.zip";
}

//------------------------------------------------------------------
// Handle a GET command by "making" the appropriate resource file
// and returning it

int FaustServer::makeAndSendResourceFile(struct MHD_Connection* connection,
                                         const std::string&     raw_url)
{
    std::vector<std::string> U   = decomposeURL(raw_url);
    fs::path                 url = fs::path(raw_url);
    // Remove leading slash to make it relative for std::filesystem
    auto url_parent = url.parent_path();
    if (url_parent.is_absolute()) {
        url_parent = url_parent.relative_path();
    }

    // For platform/architecture targets, add "targets" prefix to the path
    fs::path fulldir;
    if (U.size() >= 4 && U[2] != "diagram" && U[2] != "svg") {
        // URL format: /{sha1}/{platform}/{architecture}/{target}
        // This includes all platforms: linux, osx, web, etc.
        fulldir = getDirectory() / U[1] / "targets" / U[2] / U[3];
    } else {
        fulldir = getDirectory() / url_parent;
    }
    fs::path target;
    // For web/pwa URLs, target includes the relative path after pwa/pwa-poly
    if (U.size() >= 4 && U[2] == "web" && (U[3] == "pwa" || U[3] == "pwa-poly")) {
        // Reconstruct path from segment 4 onwards: /{sha}/web/pwa/js/app.js -> js/app.js
        if (U.size() > 4) {
            for (size_t i = 4; i < U.size(); ++i) {
                target = target / U[i];
            }
        } else {
            // For URLs like /{sha}/web/pwa/, use the last segment as target
            target = url.filename();
        }
    } else {
        target = url.filename();
    }
    fs::path    makefile = fulldir / "Makefile";
    fs::path    location;
    const char* mimetype;
    bool        precompile = false;

    if (gVerbosity >= 2) {
        std::cerr << "\nUSING SESSION " << U[1] << "\n\n";
    }
    fSessionCache.refer(U[1]);

    if (gVerbosity >= 2) {
        std::cerr << "fulldir : " << fulldir << std::endl;
    }
    if (gVerbosity >= 2) {
        std::cerr << "makefile : " << makefile << std::endl;
    }

    // Check if we need to create the target directory on demand
    // URL format: /{sha1}/{platform}/{architecture}/{target} or /{sha1}/web/{architecture}/{asset}
    if (U.size() >= 4 && !fs::exists(makefile)) {
        std::string platform     = U[2];
        std::string architecture = U[3];
        fs::path    session_dir  = getDirectory() / U[1];

        if (gVerbosity >= 2) {
            std::cerr << "Target directory doesn't exist, creating on demand: "
                      << "platform=" << platform << ", arch=" << architecture << std::endl;
        }

        // Find the source directory - it might be the session root or a subdirectory
        fs::path source_dir = session_dir;
        if (fs::exists(session_dir / "source")) {
            source_dir = session_dir / "source";
        }

        // Create the target directory on demand
        if (!create_target_directory(source_dir, session_dir, fMakefileDirectory, platform,
                                     architecture)) {
            if (gVerbosity >= 1) {
                std::cerr << "Failed to create target directory for " << platform << "/"
                          << architecture << std::endl;
            }
        }
    }

    // Check for svg block-diagram requests first
    if (url.extension() == ".svg") {
        if (gVerbosity >= 2) {
            std::cerr << "Processing SVG file request" << std::endl;
        }
        fs::path fullfile = fulldir / target;
        if (!fs::exists(fullfile)) {
            if (gVerbosity >= 2) {
                std::cerr << "Diagram not created yet" << std::endl;
            }
            make(fullfile.parent_path().parent_path(), fs::path("diagram"));
        }
        return send_file(connection, fullfile, "image/svg+xml");
    }

    // Map of file extension and corresponding mimetypes
    static std::map<std::string, std::string> mimeTypes;

    // Insert key-value pairs
    mimeTypes[".js"]   = "application/javascript";
    mimeTypes[".css"]  = "text/css";
    mimeTypes[".wasm"] = "application/wasm";
    mimeTypes[".json"] = "application/json";
    mimeTypes[".png"]  = "image/png";
    mimeTypes[".flac"] = "audio/flac";
    mimeTypes[".wav"]  = "audio/wav";

    std::string ext = url.extension().string();

    // Check for files in mimeTypes map
    if (mimeTypes.count(ext) > 0) {
        if (gVerbosity >= 2) {
            std::cerr << "Processing " << mimeTypes[ext] << " request" << std::endl;
        }
        fs::path fullfile = fulldir / target;
        if (!fs::exists(fullfile)) {
            if (gVerbosity >= 2) {
                std::cerr << mimeTypes[ext] << " doesn't exist!" << std::endl;
            }
        }
        return send_file(connection, fullfile, mimeTypes[ext].c_str());
    }

    // Check if we are doing only a pre-compilation (without actually downloading the compiled file)
    if (target == "precompile") {
        precompile = true;
        target     = getMakefileArtifactName(makefile);
        location   = url.parent_path() / target;
    }

    // Analyze possible cases of errors
    if (!isValidTarget(target, mimetype)) {
        std::cerr << "Error : not a valid target " << target << " in raw_url " << raw_url
                  << std::endl;
        return send_page(connection, invalidinstruction.c_str(), invalidinstruction.size(),
                         MHD_HTTP_BAD_REQUEST, "text/html");
    } else if (!fs::is_regular_file(makefile)) {
        std::cerr << "Error : no makefile found in raw_url " << raw_url << std::endl;
        return send_page(connection, invalidinstruction.c_str(), invalidinstruction.size(),
                         MHD_HTTP_BAD_REQUEST, "text/html");
    }

    // We can call make
    fs::path filename = make(fulldir, target);

    if (!fs::is_regular_file(filename)) {
        std::cerr << "Error : make failed in raw_url " << raw_url << std::endl;
        return send_page(connection, cannotcompile.c_str(), cannotcompile.size(),
                         MHD_HTTP_BAD_REQUEST, "text/html");
    } else {
        if (!precompile) {
            return send_file(connection, filename, mimetype);
        } else {
            return send_page(connection, "DONE", 4, MHD_HTTP_OK, "text/html", location.c_str());
        }
    }
}

//------------------------------------------------------------------
// dispatchPOSTConnections(), handle POST of a Faust source file

int FaustServer::dispatchPOSTConnections(struct MHD_Connection* connection, const std::string& url,
                                         const char* upload_data, size_t* upload_data_size,
                                         void** con_cls)
{
    time_t tmNow = time(0);

    std::cerr << "New POST connection: " << ctime(&tmNow) << " URL: " << url << std::endl;
    if (gVerbosity >= 2) {
        std::cerr << "ANSWER POST CONNECTION " << url << std::endl;
    }
    if (NULL == *con_cls) {
        if (gVerbosity >= 2) {
            std::cerr << "PRE POST processing" << std::endl;
        }
        struct connection_info_struct* con_info;

        if (nr_of_uploading_clients >= getMaxClients()) {
            return send_page(connection, busypage.c_str(), busypage.size(),
                             MHD_HTTP_SERVICE_UNAVAILABLE, "text/html");
        }

        con_info = new connection_info_struct();
        if (NULL == con_info) {
            return MHD_NO;
        }
        con_info->directory          = getDirectory().string();
        con_info->makefile_directory = getMakefileDirectory().string();

        con_info->fp            = NULL;
        con_info->postprocessor = MHD_create_post_processor(
            connection, POSTBUFFERSIZE, (MHD_PostDataIterator)iterate_post, (void*)con_info);

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
        if (gVerbosity >= 2) {
            std::cerr << "HEY! POST processing" << std::endl;
        }
        struct connection_info_struct* con_info = (connection_info_struct*)*con_cls;

        if (0 != *upload_data_size) {
            // Still data to upload
            if (gVerbosity >= 2) {
                std::cerr << "POST processing, we have data to upload !" << std::endl;
            }
            int result = MHD_post_process(con_info->postprocessor, upload_data, *upload_data_size);
            *upload_data_size = 0;
            return result;
        } else {
            // No more data to upload, we can close the file
            if (gVerbosity >= 2) {
                std::cerr << "POST processing, NO MORE data to upload !" << std::endl;
            }
            // need to close the file before request_completed
            // so that it can be opened by the methods below
            if (con_info->fp) {
                if (gVerbosity >= 2) {
                    std::cerr << "POST processing, we can close the file !" << std::endl;
                }
                fclose(con_info->fp);
                con_info->fp = 0;
            }

            std::string sha1;
            if (validate_faust(con_info) == 0) {
                // warning validate_faust returns errocode 0 when the faust code is correct !
                std::vector<std::string> segments;
                sha1 = generate_sha1(con_info);  // TODO: Duplicated computation of the SHA key
                if (gVerbosity >= 2) {
                    if (gVerbosity >= 1) {
                        std::cerr << "POST processing, we have valid faust code. Its SHA1 key is = "
                                  << sha1 << std::endl;
                    }
                }
                if (matchURL(url, "/compile/*/*/*", segments)) {
                    std::string newurl("/");
                    newurl += sha1 + "/" + segments[2] + "/" + segments[3] + "/" + segments[4];
                    if (gVerbosity >= 2) {
                        std::cerr << "DIRECT COMPILATION: we are trying a direct compilation at "
                                  << newurl << endl;
                    }
                    return makeAndSendResourceFile(connection, newurl.c_str());
                } else {
                    return send_page(connection, con_info->answerstring.c_str(),
                                     con_info->answerstring.size(), con_info->answercode,
                                     "text/html");
                }
            } else {
                std::cerr << "POST processing, we have an INVALID faust code" << std::endl;
                std::string errormessage = "Invalid Faust Code";
                return send_page(connection, errormessage.c_str(), errormessage.size(),
                                 con_info->answercode, "text/html");
            }
        }

        std::cerr << "NEVER EXECUTED" << std::endl;
        return send_page(connection, errorpage.c_str(), errorpage.size(), MHD_HTTP_BAD_REQUEST,
                         "text/html");
    }
}

/*
 * Callback called every time a POST request comes in by the postprocessor.
 * To understand more about how postprocessors work, consult the microhttpd
 * documentation.
 */

// avoid deferencing invalid char* pointers when printing debg messages
static const char* secure_string(const char* str)
{
    if (str == NULL) {
        return "NULL";
    } else if (str[0] == 0) {
        return "EMPTY";
    } else {
        return str;
    }
}

int FaustServer::iterate_post(void* coninfo_cls, enum MHD_ValueKind kind, const char* key,
                              const char* filename, const char*              content_type,
                              const char* /*transfer_encoding*/, const char* data, uint64_t /*off*/,
                              size_t size)
{
    struct connection_info_struct* con_info = (connection_info_struct*)coninfo_cls;
    FILE*                          fp;

    // check all char* pointers are valid
    bool valid = key != NULL && key[0] != 0 && strlen(key) < 100 && filename != NULL &&
                 filename[0] != 0 && strlen(filename) < 100 && content_type != NULL &&
                 content_type[0] != 0 && strlen(content_type) < 100 && data != NULL;

    if (!valid) {
        // we dont' have a valid post
        std::cerr << "ERROR: some null or empty data in iterate_post" << std::endl;
        std::cerr << "ERROR iterate_post ("
                  << "kind : " << kind << ", key : " << secure_string(key)
                  << ", filename: " << secure_string(filename)
                  << ", content type: " << secure_string(content_type)
                  << ", data pointer: " << (void*)data << ", size: " << size << ")" << std::endl;
        return MHD_NO;  // We return
    }

    // We have a valid post
    if (gVerbosity >= 2) {
        std::cerr << "ENTER iterate_post ("
                  << "kind : " << kind << ", key : " << key << ", filename: " << filename
                  << ", content type: "
                  << content_type
                  //<< ", transfer_encoding: " << transfer_encoding
                  << ", data pointer: " << (void*)data << ", size: " << size << ")" << std::endl;
    }

    // SECURITY: Validate filename before processing
    if (!isSecureFilename(std::string(filename))) {
        if (gVerbosity >= 1) {
            std::cerr << "SECURITY: Rejecting unsafe filename: " << filename << std::endl;
        }
        con_info->answerstring = errorpage;
        con_info->answercode   = MHD_HTTP_BAD_REQUEST;
        return MHD_NO;
    }

    if (con_info->tmppath.empty()) {
        con_info->filename = filename;
        // Generate unique path using random number
        std::random_device              rd;
        std::mt19937                    gen(rd());
        std::uniform_int_distribution<> dis(10000, 99999);
        auto unique_name  = "tmp_" + std::to_string(dis(gen)) + "_" + std::to_string(dis(gen));
        con_info->tmppath = (fs::temp_directory_path() / unique_name).string();
        fs::create_directory(con_info->tmppath);
    }

    std::string full_path = (fs::path(con_info->tmppath) / fs::path(con_info->filename)).string();

    con_info->answerstring = servererrorpage;
    con_info->answercode   = MHD_HTTP_INTERNAL_SERVER_ERROR;

    if (0 != strcmp(key, "file")) {
        if (gVerbosity >= 2) {
            std::cerr << __LINE__ << " FaustServer::iterate_post" << std::endl;
        }
        return MHD_NO;
    }

    if (con_info->fp == 0) {
        if (NULL != (fp = fopen(full_path.c_str(), "rb"))) {
            fclose(fp);

            con_info->answerstring = fileexistspage;
            con_info->answercode   = MHD_HTTP_FORBIDDEN;
            if (gVerbosity >= 2) {
                std::cerr << __LINE__ << " FaustServer::iterate_post" << std::endl;
            }
            return MHD_NO;
        }

        con_info->fp = fopen(full_path.c_str(), "ab");
        if (con_info->fp == 0) {
            if (gVerbosity >= 2) {
                std::cerr << __LINE__ << " FaustServer::iterate_post" << std::endl;
            }
            return MHD_NO;
        }
    }

    if (size > 0) {
        if (!fwrite(data, size, sizeof(char), con_info->fp)) {
            if (gVerbosity >= 2) {
                std::cerr << __LINE__ << " FaustServer::iterate_post" << std::endl;
            }
            return MHD_NO;
        }
    }

    con_info->answercode = MHD_HTTP_OK;

    return MHD_YES;
}

FaustServer::FaustServer(int port, int max_clients, const fs::path& directory,
                         const fs::path& makefile_directory, const fs::path& logfile,
                         int maxSessions)
    : fPort(port),
      fMaxClients(max_clients),
      fDirectory(directory),
      fMakefileDirectory(makefile_directory),
      fLogfile(logfile),
      fDaemon(0),
      fTargets(""),
      fSessionCache(directory, maxSessions)
{
    // create a string containing the list possible targets by scanning the makefile directory. The
    // list is JSON formatted :   { "os1" : ["arch11", "arch12", ...],  "os2" : ["arch21", "arch22",
    // ...], ...} and stored in field targets
    std::stringstream ss;

    ss << '{';
    char sep1 = ' ';

    for (const auto& os_entry : fs::directory_iterator(makefile_directory)) {
        if (fs::is_directory(os_entry.path())) {
            auto OSname = os_entry.path().filename().string();

            // collect a vector of target names
            std::vector<std::string> V;
            for (const auto& makefile_entry : fs::directory_iterator(os_entry.path())) {
                auto makefileName = makefile_entry.path().filename().string();
                if (makefileName.substr(0, 9) == "Makefile.") {
                    V.push_back(makefileName.substr(9));
                }
            }

            // If it is a folder with makefiles inside, print it
            if (!V.empty()) {
                std::sort(V.begin(), V.end());
                ss << sep1 << std::endl << '"' << OSname << '"' << ": [";
                for (size_t i = 0; i < V.size(); i++) {
                    if (i > 0) {
                        ss << ',';
                    }
                    ss << '"' << V[i] << '"';
                }
                ss << ']';
                sep1 = ',';
            }
        }
    }
    ss << std::endl << "}";
    fTargets = ss.str();

    // Clear cache before running the server
    fSessionCache.clear();
}

//------------------------------------------------------------------
// Serve the app interface HTML page

int FaustServer::serveAppInterface(struct MHD_Connection* connection)
{
    // Read the app.html file
    fs::path app_file_path = fs::current_path() / "app.html";

    if (!fs::exists(app_file_path)) {
        return send_page(connection, "App interface not found", 23, MHD_HTTP_NOT_FOUND,
                         "text/html");
    }

    try {
        std::ifstream file(app_file_path);
        if (!file.is_open()) {
            return send_page(connection, "Cannot read app interface", 25,
                             MHD_HTTP_INTERNAL_SERVER_ERROR, "text/html");
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        file.close();

        return send_page(connection, content.c_str(), content.size(), MHD_HTTP_OK, "text/html");

    } catch (const std::exception& e) {
        if (gVerbosity >= 1) {
            std::cerr << "Error serving app interface: " << e.what() << std::endl;
        }
        return send_page(connection, "Error loading app interface", 27,
                         MHD_HTTP_INTERNAL_SERVER_ERROR, "text/html");
    }
}

//------------------------------------------------------------------
// Serve signals.svg with dynamic generation
//
int FaustServer::serveSignalsSvg(struct MHD_Connection* connection, const std::string& url)
{
    std::vector<std::string> U = decomposeURL(url);
    if (U.size() < 2) {
        std::string error_msg = "Invalid URL format for signals.svg";
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "text/plain");
    }

    std::string sha1             = U[1];
    fs::path    session_dir      = fDirectory / sha1;
    fs::path    signals_svg_path = session_dir / "signals.svg";
    fs::path    sourcecode_dir   = session_dir / "sourcecode";

    if (gVerbosity >= 2) {
        std::cerr << "Request for signals.svg: " << sha1 << std::endl;
    }

    // Check if signals.svg already exists
    if (fs::exists(signals_svg_path)) {
        if (gVerbosity >= 2) {
            std::cerr << "Serving existing signals.svg" << std::endl;
        }
        return send_file(connection, signals_svg_path, "image/svg+xml");
    }

    // Generate signals.svg if it doesn't exist
    if (!fs::exists(sourcecode_dir) || !fs::is_directory(sourcecode_dir)) {
        std::string error_msg = "Source code directory not found for session: " + sha1;
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "text/plain");
    }

    // Find the main DSP file in sourcecode directory
    std::string main_dsp_file;
    for (const auto& entry : fs::directory_iterator(sourcecode_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".dsp") {
            main_dsp_file = entry.path().filename().string();
            break;
        }
    }

    if (main_dsp_file.empty()) {
        std::string error_msg = "No DSP file found in session: " + sha1;
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "text/plain");
    }

    if (gVerbosity >= 2) {
        std::cerr << "Generating signals.svg for " << main_dsp_file << " in session " << sha1
                  << std::endl;
    }

    // Generate signals diagram
    std::string options_str = readFaustOptions(session_dir);
    std::string dot_file    = main_dsp_file + "-sig.dot";
    // SECURITY: Separate protections for faust vs dot commands
    // Faust -sg: strict limits (30s, stack protection) - same DSP as main compilation
    // dot: generous timeout (180s = 3min) but no stack limit - can be legitimately slow
    // Note: ulimit -v (virtual memory) not supported on macOS, removed for compatibility
    std::string generate_cmd =
        "cd " + sourcecode_dir.string() + " && (ulimit -s 8192; ulimit -t 30; faust " +
        options_str + (options_str.empty() ? "" : " ") + "-sg " + main_dsp_file + " -o /dev/null)" +
        " && (ulimit -t 180; dot -Tsvg " + dot_file + " -o ../signals.svg)" + " && rm " + dot_file;

    if (gVerbosity >= 2) {
        std::cerr << "Executing: " << generate_cmd << std::endl;
    }

    int result = system(generate_cmd.c_str());
    if (result != 0) {
        if (gVerbosity >= 1) {
            std::cerr << "Failed to generate signals.svg for session " << sha1
                      << " (exit code: " << result << ")" << std::endl;
        }
        std::string error_msg = "Failed to generate signal diagram";
        return send_page(connection, error_msg.c_str(), error_msg.size(),
                         MHD_HTTP_INTERNAL_SERVER_ERROR, "text/plain");
    }

    // Check if generation was successful
    if (fs::exists(signals_svg_path)) {
        if (gVerbosity >= 2) {
            std::cerr << "Successfully generated signals.svg" << std::endl;
        }
        return send_file(connection, signals_svg_path, "image/svg+xml");
    } else {
        if (gVerbosity >= 1) {
            std::cerr << "signals.svg was not created despite successful command execution"
                      << std::endl;
        }
        std::string error_msg = "Signal diagram generation completed but file not found";
        return send_page(connection, error_msg.c_str(), error_msg.size(),
                         MHD_HTTP_INTERNAL_SERVER_ERROR, "text/plain");
    }
}

//------------------------------------------------------------------
// Serve task.svg with dynamic generation
//
int FaustServer::serveTaskSvg(struct MHD_Connection* connection, const std::string& url)
{
    std::vector<std::string> U = decomposeURL(url);
    if (U.size() < 2) {
        std::string error_msg = "Invalid URL format for tasks.svg";
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "text/plain");
    }

    std::string sha1           = U[1];
    fs::path    session_dir    = fDirectory / sha1;
    fs::path    task_svg_path  = session_dir / "tasks.svg";
    fs::path    sourcecode_dir = session_dir / "sourcecode";

    if (gVerbosity >= 2) {
        std::cerr << "Request for tasks.svg: " << sha1 << std::endl;
    }

    // Check if tasks.svg already exists
    if (fs::exists(task_svg_path)) {
        if (gVerbosity >= 2) {
            std::cerr << "Serving existing tasks.svg" << std::endl;
        }
        return send_file(connection, task_svg_path, "image/svg+xml");
    }

    // Generate tasks.svg if it doesn't exist
    if (!fs::exists(sourcecode_dir) || !fs::is_directory(sourcecode_dir)) {
        std::string error_msg = "Source code directory not found for session: " + sha1;
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "text/plain");
    }

    // Find the main DSP file in sourcecode directory
    std::string main_dsp_file;
    for (const auto& entry : fs::directory_iterator(sourcecode_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".dsp") {
            main_dsp_file = entry.path().filename().string();
            break;
        }
    }

    if (main_dsp_file.empty()) {
        std::string error_msg = "No DSP file found in session: " + sha1;
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "text/plain");
    }

    if (gVerbosity >= 2) {
        std::cerr << "Generating tasks.svg for " << main_dsp_file << " in session " << sha1
                  << std::endl;
    }

    // Generate task diagram
    std::string options_str  = readFaustOptions(session_dir);
    std::string dot_file     = main_dsp_file + ".dot";
    std::string generate_cmd = "cd " + sourcecode_dir.string() + " && faust " + options_str +
                               (options_str.empty() ? "" : " ") + "-vec -tg " + main_dsp_file +
                               " -o /dev/null" + " && dot -Tsvg " + dot_file + " -o ../tasks.svg" +
                               " && rm " + dot_file;

    if (gVerbosity >= 2) {
        std::cerr << "Executing: " << generate_cmd << std::endl;
    }

    int result = system(generate_cmd.c_str());
    if (result != 0) {
        if (gVerbosity >= 1) {
            std::cerr << "Failed to generate tasks.svg for session " << sha1
                      << " (exit code: " << result << ")" << std::endl;
        }
        std::string error_msg = "Failed to generate task diagram";
        return send_page(connection, error_msg.c_str(), error_msg.size(),
                         MHD_HTTP_INTERNAL_SERVER_ERROR, "text/plain");
    }

    // Check if generation was successful
    if (fs::exists(task_svg_path)) {
        if (gVerbosity >= 2) {
            std::cerr << "Successfully generated tasks.svg" << std::endl;
        }
        return send_file(connection, task_svg_path, "image/svg+xml");
    } else {
        if (gVerbosity >= 1) {
            std::cerr << "tasks.svg was not created despite successful command execution"
                      << std::endl;
        }
        std::string error_msg = "Task diagram generation completed but file not found";
        return send_page(connection, error_msg.c_str(), error_msg.size(),
                         MHD_HTTP_INTERNAL_SERVER_ERROR, "text/plain");
    }
}

//------------------------------------------------------------------
// Helper function to generate webapp if it doesn't exist
// Returns true if webapp exists or was successfully generated, false otherwise
//
bool FaustServer::ensure_webapp_exists(const std::string& sha1, std::string& error_msg)
{
    if (gVerbosity >= 1) {
        std::cerr << "ENSURE WEBAPP EXISTS for " << sha1 << std::endl;
    }

    auto session_dir = fDirectory / sha1;
    if (!fs::exists(session_dir)) {
        error_msg = "Session not found: " + sha1;
        return false;
    }

    auto sourcecode_dir = session_dir / "sourcecode";
    auto webapp_dir     = session_dir / "webapp";

    // Check if webapp already exists
    auto index_html_path = webapp_dir / "index.html";
    if (fs::exists(index_html_path)) {
        if (gVerbosity >= 2) {
            std::cerr << "Webapp already exists for session " << sha1 << std::endl;
        }
        return true;
    }

    if (gVerbosity >= 2) {
        std::cerr << "Generating webapp for " << sha1 << std::endl;
    }

    // Get the main DSP file name
    std::string main_dsp_file;
    for (const auto& entry : fs::directory_iterator(sourcecode_dir)) {
        if (entry.path().extension() == ".dsp") {
            main_dsp_file = entry.path().filename().string();
            break;
        }
    }

    if (main_dsp_file.empty()) {
        error_msg = "No DSP file found in session";
        return false;
    }

    if (gVerbosity >= 2) {
        std::cerr << "Generating webapp for " << main_dsp_file << " in session " << sha1
                  << std::endl;
    }

    // Generate web application using faust2wasm-ts
    std::string generate_cmd = "cd " + sourcecode_dir.string() + " && faust2wasm-ts " +
                               main_dsp_file + " ../webapp -pwa 2> ../errors.log";

    if (gVerbosity >= 2) {
        std::cerr << "Executing: " << generate_cmd << std::endl;
    }

    int result = system(generate_cmd.c_str());
    if (result != 0) {
        if (gVerbosity >= 1) {
            std::cerr << "Failed to generate webapp for session " << sha1
                      << " (exit code: " << result << ")" << std::endl;
        }
        error_msg = "Failed to generate web application";
        return false;
    }

    // Check if generation was successful
    if (fs::exists(index_html_path)) {
        if (gVerbosity >= 2) {
            std::cerr << "Successfully generated webapp" << std::endl;
        }
        return true;
    } else {
        if (gVerbosity >= 1) {
            std::cerr << "webapp was not created despite successful command execution" << std::endl;
        }
        error_msg = "Web application generation completed but index.html not found";
        return false;
    }
}

//------------------------------------------------------------------
// Generate and serve web application
//
int FaustServer::generate_webapp_view(struct MHD_Connection* connection, const std::string& sha1)
{
    std::string error_msg;
    if (!ensure_webapp_exists(sha1, error_msg)) {
        int status_code = (error_msg.find("Session not found") != std::string::npos ||
                           error_msg.find("No DSP file found") != std::string::npos)
                              ? MHD_HTTP_NOT_FOUND
                              : MHD_HTTP_INTERNAL_SERVER_ERROR;
        return send_page(connection, error_msg.c_str(), error_msg.size(), status_code,
                         "text/plain");
    }

    // Webapp exists, serve index.html
    auto webapp_dir      = fDirectory / sha1 / "webapp";
    auto index_html_path = webapp_dir / "index.html";
    return send_file(connection, index_html_path, "text/html");
}

//------------------------------------------------------------------
// Serve sessions list as JSON
//
int FaustServer::serveSessionsList(struct MHD_Connection* connection)
{
    if (gVerbosity >= 2) {
        std::cerr << "Request for sessions list" << std::endl;
    }

    std::ostringstream json;
    json << "{\n  \"sessions\": [\n";

    bool first = true;
    try {
        // Iterate through all session directories
        if (fs::exists(fDirectory) && fs::is_directory(fDirectory)) {
            std::vector<std::pair<fs::path, std::time_t>> sessions;

            // Collect all sessions with their modification times
            for (const auto& entry : fs::directory_iterator(fDirectory)) {
                if (entry.is_directory()) {
                    fs::path session_dir   = entry.path();
                    fs::path filename_file = session_dir / "filename.txt";

                    // Only include sessions that have a filename.txt file
                    if (fs::exists(filename_file)) {
                        std::time_t mod_time =
                            fs::last_write_time(session_dir).time_since_epoch().count();
                        sessions.emplace_back(session_dir, mod_time);
                    }
                }
            }

            // Sort sessions by modification time (oldest first)
            std::sort(sessions.begin(), sessions.end(),
                      [](const auto& a, const auto& b) { return a.second < b.second; });

            // Generate JSON for each session
            for (const auto& [session_dir, mod_time] : sessions) {
                std::string sha1          = session_dir.filename().string();
                fs::path    filename_file = session_dir / "filename.txt";

                // Read the original filename
                std::string filename = "unknown.dsp";
                try {
                    std::ifstream file(filename_file);
                    if (file.is_open()) {
                        std::getline(file, filename);
                        file.close();
                        // Remove any trailing whitespace/newlines
                        filename.erase(filename.find_last_not_of(" \n\r\t") + 1);
                    }
                } catch (const std::exception& e) {
                    if (gVerbosity >= 1) {
                        std::cerr << "Warning: Could not read filename for session " << sha1 << ": "
                                  << e.what() << std::endl;
                    }
                }

                if (!first) {
                    json << ",\n";
                }
                first = false;

                json << "    {\n";
                json << "      \"sha1\": \"" << sha1 << "\",\n";
                json << "      \"filename\": \"" << filename << "\",\n";
                json << "      \"timestamp\": " << mod_time << "\n";
                json << "    }";
            }
        }
    } catch (const fs::filesystem_error& e) {
        if (gVerbosity >= 1) {
            std::cerr << "Error reading sessions directory: " << e.what() << std::endl;
        }
        std::string error_msg = "Error reading sessions";
        return send_page(connection, error_msg.c_str(), error_msg.size(),
                         MHD_HTTP_INTERNAL_SERVER_ERROR, "text/plain");
    }

    json << "\n  ]\n}";

    std::string json_str = json.str();
    if (gVerbosity >= 2) {
        std::cerr << "Returning " << (first ? 0 : json_str.length()) << " bytes of sessions data"
                  << std::endl;
    }

    return send_page(connection, json_str.c_str(), json_str.size(), MHD_HTTP_OK,
                     "application/json");
}
