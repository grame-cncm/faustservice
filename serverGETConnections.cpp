#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include "content.hh"
#include "htmlPages.hh"
#include "match.hh"
#include "server.hh"

extern int gVerbosity;

/**
 * Handler used to generate a 404 reply.
 *
 * @param cls a ’const char* ’ with the HTML webpage to return
 * @param mime mime type to use
 * @param session session handle
 * @param connection connection to use
 */

static int page_not_found(struct MHD_Connection* connection, const char* page, int length,
                          const char* mimetype)
{
    struct MHD_Response* response =
        MHD_create_response_from_buffer(length, (void*)page, MHD_RESPMEM_MUST_COPY);
    int ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_ENCODING, mimetype);
    MHD_destroy_response(response);
    return ret;
}

//------------------------------------------------------------------
// Actual callback method called every time a GET or POST request is received.
// by the server.

int FaustServer::dispatchGETConnections(struct MHD_Connection* connection, const std::string& url)
{
    // TArgs args;
    // MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, get_params, &args);
    if (gVerbosity >= 2) {
        std::cerr << "ANSWER GET CONNECTION " << url << std::endl;
    }

    if (matchExtension(url, ".php") ||
        (url.length() > 100 &&
         url.find("/webapp/") == std::string::npos) /*matchExtension(url, ".js")*/) {
        return page_not_found(connection, "/favicon.ico", 12, "image/x-icon");

    } else if (matchURL(url, "/")) {
        std::stringstream ss;
        ss << askpage_head << nr_of_uploading_clients << askpage_tail;
        return send_page(connection, ss.str().c_str(), ss.str().size(), MHD_HTTP_OK, "text/html");

    } else if (matchURL(url, "/targets")) {
        return send_page(connection, fTargets.c_str(), fTargets.size(), MHD_HTTP_OK,
                         "application/json");

    } else if (matchURL(url, "/version")) {
        // Get Faust version by running faust --version
        std::string version_cmd =
            "faust --version 2>&1 | head -1 | grep -o '[0-9]\\+\\.[0-9]\\+\\.[0-9]\\+'";
        FILE*       pipe    = popen(version_cmd.c_str(), "r");
        std::string version = "2.81.2";  // fallback
        if (pipe) {
            char buffer[128];
            if (fgets(buffer, sizeof(buffer), pipe)) {
                version = std::string(buffer);
                // Remove trailing newline
                version.erase(std::remove(version.begin(), version.end(), '\n'), version.end());
            }
            pclose(pipe);
        }
        return send_page(connection, version.c_str(), version.size(), MHD_HTTP_OK, "text/plain");

    } else if (matchURL(url, "/app")) {
        return serveAppInterface(connection);

    } else if (matchURL(url, "/verbosity0")) {
        gVerbosity = 0;
        std::stringstream ss;
        ss << "Verbosity " << gVerbosity;
        return send_page(connection, ss.str().c_str(), ss.str().size(), MHD_HTTP_OK, "text/html");

    } else if (matchURL(url, "/verbosity1")) {
        gVerbosity = 1;
        std::stringstream ss;
        ss << "Verbosity " << gVerbosity;
        return send_page(connection, ss.str().c_str(), ss.str().size(), MHD_HTTP_OK, "text/html");

    } else if (matchURL(url, "/verbosity2")) {
        gVerbosity = 2;
        std::stringstream ss;
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
        // Legacy redirection: /sha/diagram/foo.svg -> /sha/svg/foo.svg
        std::vector<std::string> U = decomposeURL(url);
        if (U.size() >= 4) {
            // URL format: /sha1/diagram/filename.svg -> serve from /sha1/svg/filename.svg
            fs::path filepath = fDirectory / U[1] / "svg" / U[3];
            if (fs::exists(filepath)) {
                return send_file(connection, filepath, "image/svg+xml");
            }
        }
        std::string error_msg = "SVG diagram not found";
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "image/svg+xml");

    } else if (matchURL(url, "/*/generated.cpp")) {
        // Serve the generated C++ file from validate_faust
        std::vector<std::string> U = decomposeURL(url);
        if (U.size() >= 2) {
            fs::path filepath = fDirectory / U[1] / "generated.cpp";
            if (fs::exists(filepath)) {
                return send_file(connection, filepath, "text/x-c");
            }
        }
        std::string error_msg = "File not found";
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "text/plain");

    } else if (matchURL(url, "/*/errors.log")) {
        // Serve the compilation errors log file from validate_faust
        std::vector<std::string> U = decomposeURL(url);
        if (U.size() >= 2) {
            fs::path filepath = fDirectory / U[1] / "errors.log";
            if (fs::exists(filepath)) {
                return send_file(connection, filepath, "text/plain");
            }
        }
        std::string error_msg = "No errors.log file found";
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "text/plain");

    } else if (matchURL(url, "/*/filename")) {
        // Serve the original filename for drag-and-drop functionality
        std::vector<std::string> U = decomposeURL(url);
        if (U.size() >= 2) {
            fs::path filepath = fDirectory / U[1] / "filename.txt";
            if (fs::exists(filepath)) {
                return send_file(connection, filepath, "text/plain");
            }
        }
        std::string error_msg = "No filename information found";
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "text/plain");

    } else if (matchURL(url, "/*/svg.zip")) {
        // Create and serve a ZIP file containing all SVG diagrams
        std::vector<std::string> U = decomposeURL(url);
        if (U.size() >= 2) {
            fs::path svg_dir = fDirectory / U[1] / "svg";
            if (fs::exists(svg_dir) && fs::is_directory(svg_dir)) {
                // Create temporary ZIP file
                fs::path temp_zip = fDirectory / U[1] / "temp_svg.zip";

                if (create_svg_zip(svg_dir, temp_zip)) {
                    // Send the ZIP file and then delete it
                    int result = send_file(connection, temp_zip, "application/zip");
                    try {
                        fs::remove(temp_zip);
                    } catch (const fs::filesystem_error& e) {
                        if (gVerbosity >= 1) {
                            std::cerr
                                << "Warning: Could not remove temp ZIP: " << e.code().message()
                                << std::endl;
                        }
                    }
                    return result;
                }
            }
        }
        std::string error_msg = "No SVG diagrams found or could not create ZIP";
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "text/plain");

    } else if (matchURL(url, "/*/webapp.zip")) {
        // Create and serve a ZIP file containing all webapp files
        std::vector<std::string> U = decomposeURL(url);
        if (U.size() >= 2) {
            std::string sha1 = U[1];
            std::string error_msg;

            // Ensure webapp exists, generate if needed
            if (!ensure_webapp_exists(sha1, error_msg)) {
                int status_code = (error_msg.find("Session not found") != std::string::npos ||
                                   error_msg.find("No DSP file found") != std::string::npos)
                                      ? MHD_HTTP_NOT_FOUND
                                      : MHD_HTTP_INTERNAL_SERVER_ERROR;
                return send_page(connection, error_msg.c_str(), error_msg.size(), status_code,
                                 "text/plain");
            }

            fs::path webapp_dir = fDirectory / sha1 / "webapp";
            if (fs::exists(webapp_dir) && fs::is_directory(webapp_dir)) {
                // Create temporary ZIP file
                fs::path temp_zip = fDirectory / sha1 / "temp_webapp.zip";

                if (create_webapp_zip(webapp_dir, temp_zip)) {
                    // Send the ZIP file and then delete it
                    int result = send_file(connection, temp_zip, "application/zip");
                    try {
                        fs::remove(temp_zip);
                    } catch (const fs::filesystem_error& e) {
                        if (gVerbosity >= 1) {
                            std::cerr << "Warning: Could not remove temp webapp ZIP: "
                                      << e.code().message() << std::endl;
                        }
                    }
                    return result;
                }
            }
        }
        std::string error_msg = "No webapp files found or could not create ZIP";
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "text/plain");

    } else if (matchURL(url, "/*/svg/*") && matchExtension(url, ".svg")) {
        // Serve SVG files from the svg/ directory created by validate_faust
        std::vector<std::string> U = decomposeURL(url);
        if (U.size() >= 4) {
            // URL format: /<sha1>/svg/<filename.svg>
            // U[0] is empty string, U[1] is sha1, U[2] is "svg", U[3] is filename
            fs::path filepath = fDirectory / U[1] / U[2] / U[3];
            if (fs::exists(filepath)) {
                return send_file(connection, filepath, "image/svg+xml");
            }
        }
        std::string error_msg = "SVG file not found";
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "image/svg+xml");

    } else if (matchBeginURL(url, "/*/web/pwa") || matchBeginURL(url, "/*/web/pwa-poly")) {
        return makeAndSendResourceFile(connection, url);

    } else if (matchURL(url, "/*/signals.svg")) {
        return serveSignalsSvg(connection, url);

    } else if (matchURL(url, "/*/tasks.svg")) {
        return serveTaskSvg(connection, url);

    } else if (matchURL(url, "/*/webapp")) {
        // Extract SHA from URL: /sha1/webapp -> sha1
        std::string sha1 = url.substr(1, url.find('/', 1) - 1);
        return generate_webapp_view(connection, sha1);

    } else if (url.find("/webapp/") != std::string::npos && url.size() > 40) {
        // Handle webapp assets using string matching instead of pattern matching
        // Expected format: /<sha>/webapp/path/to/file.ext
        if (gVerbosity >= 2) {
            std::cerr << "DEBUG: Processing webapp asset URL: " << url << std::endl;
        }

        // Find the position of "/webapp/"
        size_t webapp_pos = url.find("/webapp/");
        if (webapp_pos != std::string::npos) {
            // Extract SHA (everything before /webapp/)
            std::string sha1 = url.substr(1, webapp_pos - 1);  // Skip leading '/'

            // Extract asset path (everything after /webapp/)
            std::string asset_path = url.substr(webapp_pos + 8);  // Skip "/webapp/"

            if (!sha1.empty() && !asset_path.empty()) {
                auto session_dir = fDirectory / sha1;
                auto webapp_file = session_dir / "webapp" / asset_path;

                if (fs::exists(webapp_file)) {
                    // Determine MIME type based on file extension
                    std::string ext       = webapp_file.extension().string();
                    const char* mime_type = "application/octet-stream";
                    if (ext == ".html") {
                        mime_type = "text/html";
                    } else if (ext == ".js") {
                        mime_type = "application/javascript";
                    } else if (ext == ".css") {
                        mime_type = "text/css";
                    } else if (ext == ".wasm") {
                        mime_type = "application/wasm";
                    } else if (ext == ".json") {
                        mime_type = "application/json";
                    } else if (ext == ".png") {
                        mime_type = "image/png";
                    } else if (ext == ".svg") {
                        mime_type = "image/svg+xml";
                    }

                    return send_file(connection, webapp_file, mime_type);
                }
            }
        }

        if (gVerbosity >= 2) {
            std::cerr << "DEBUG: Webapp asset not found for URL: " << url << std::endl;
        }
        std::string error_msg = "Webapp asset not found";
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "text/plain");

    } else if (url.find(".js") != std::string::npos || url.find(".wasm") != std::string::npos ||
               url.find(".css") != std::string::npos || url.find(".json") != std::string::npos ||
               url.find(".png") != std::string::npos) {
        // Handle webapp asset requests that come as direct paths (for iframe compatibility)
        // Expected patterns: /<sha>/index.js, /<sha>/faust-ui/index.css,
        // /<sha>/faustwasm/create-node.js, etc.
        std::vector<std::string> parts = decomposeURL(url);
        if (parts.size() >= 2) {
            std::string sha1 = parts[1];

            // Reconstruct the full asset path (everything after /<sha>/)
            std::string asset_path = "";
            for (size_t i = 2; i < parts.size(); i++) {
                if (!asset_path.empty()) {
                    asset_path += "/";
                }
                asset_path += parts[i];
            }

            auto session_dir = fDirectory / sha1;
            auto webapp_file = session_dir / "webapp" / asset_path;

            if (fs::exists(webapp_file)) {
                // Determine MIME type based on file extension
                std::string ext       = webapp_file.extension().string();
                const char* mime_type = "application/octet-stream";
                if (ext == ".html") {
                    mime_type = "text/html";
                } else if (ext == ".js") {
                    mime_type = "application/javascript";
                } else if (ext == ".css") {
                    mime_type = "text/css";
                } else if (ext == ".wasm") {
                    mime_type = "application/wasm";
                } else if (ext == ".json") {
                    mime_type = "application/json";
                } else if (ext == ".png") {
                    mime_type = "image/png";
                } else if (ext == ".svg") {
                    mime_type = "image/svg+xml";
                }

                if (gVerbosity >= 2) {
                    std::cerr << "Serving webapp asset: " << webapp_file << " as " << mime_type
                              << std::endl;
                }

                return send_file(connection, webapp_file, mime_type);
            }
        }

        // If not found as webapp asset, return 404
        std::string error_msg = "Webapp asset not found: " + url;
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "text/plain");

    } else if (matchURL(url, "/sessions/list")) {
        return serveSessionsList(connection);

    } else if (matchURL(url, "/*/user_code.dsp")) {
        // Serve the original DSP code from a session
        std::vector<std::string> U = decomposeURL(url);
        if (U.size() >= 2) {
            fs::path filepath = fDirectory / U[1] / "user_code.dsp";
            if (fs::exists(filepath)) {
                return send_file(connection, filepath, "text/plain");
            }
        }
        std::string error_msg = "DSP file not found";
        return send_page(connection, error_msg.c_str(), error_msg.size(), MHD_HTTP_NOT_FOUND,
                         "text/plain");

    } else if (matchURL(url, "/favicon.ico")) {
        return page_not_found(connection, "/favicon.ico", 12, "image/x-icon");

    } else {
        if (gVerbosity >= 1) {
            std::cerr << "WARNING: INVALID URL " << url << std::endl;
        }
        return page_not_found(connection, "/favicon.ico", 12, "image/x-icon");
    }
}
