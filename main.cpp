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

#include <sys/stat.h>
#include <ctime>
#include <iostream>

#include <filesystem>
#include "CLI11.hpp"

#include <signal.h>
#include <unistd.h>
#include <string>

#include "server.hh"
#include "utilities.hh"

namespace fs = std::filesystem;

int  gPort        = 8888;
int  gMaxClients  = 2;
bool gAnyOrigin   = true;
int  gMaxSessions = 50;
int  gVerbosity   = 0;

fs::path gCurrentDirectory;   ///< root directory were makefiles and sessions and log are located
fs::path gSessionsDirectory;  ///< directory where sessions are stored
fs::path
    gMakefilesDirectory;  ///< directory containing all the "<os>/Makefile.<architecture>[-32bits|-64bits]" makefiles
fs::path gLogfile;        ///< faustweb logfile
std::string   gRecoverCmd;     ///< system command to launch for recovery after intercepted crash
char*    gArguments[256];

// Processes command line arguments using CLI11
static void process_cmdline(int argc, char* argv[])
{
    CLI::App app{"faustweb program options"};
    
    std::string sessions_dir;
    std::string logfile_str;
    
    app.add_option("-d,--sessions-dir", sessions_dir, "directory in which sessions files will be written");
    app.add_flag("-a,--any-origin", gAnyOrigin, "Adds any origin when answering requests");
    app.add_option("-m,--max-clients", gMaxClients, "maximum number of clients allowed to concurrently upload")
        ->default_val(2);
    app.add_option("-p,--port", gPort, "the listening port")
        ->default_val(8888);
    app.add_option("-n,--max-sessions", gMaxSessions, "maximum number of cached sessions")
        ->default_val(50);
    app.add_option("-v,--verbose", gVerbosity, "0: normal; 1: verbose; 2: very verbose")
        ->default_val(0)
        ->check(CLI::Range(0, 2));
    app.add_option("-r,--recover-cmd", gRecoverCmd, "program (usually self) to launch after crash recovery");
    app.add_option("--logfile", logfile_str, "log file path");
    
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        exit(app.exit(e));
    }
    
    // Set paths from strings if provided
    if (!sessions_dir.empty()) {
        gSessionsDirectory = sessions_dir;
    }
    if (!logfile_str.empty()) {
        gLogfile = logfile_str;
    }
}

static void printFaustVersion()
{
    int err = system("faust -v");
    if (err != 0) std::cerr << "ERROR: Faust not found";
}

static size_t computeSessionSize()
{
    size_t size = 0;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(gSessionsDirectory)) {
            if (!fs::is_directory(entry)) {
                size += fs::file_size(entry);
            }
        }
        return size;
    } catch (const std::filesystem::filesystem_error& e) {
        return 0;
    }
}

// static void recover()
// {
//     std::cerr << "execv(" << gRecoverCmd << ")" << endl;
//     execv(gRecoverCmd.c_str(), gArguments);
// }

static void _sigaction(int signal, siginfo_t*, void*)
{
    std::cerr << "\n\n";
    std::cerr << "SIGNAL #" << signal << " CATCHED!" << std::endl;
    if (gRecoverCmd.size() > 0) {
        std::cerr << "EXEC RECOVERING CMD: " << gRecoverCmd << endl;
        execv(gRecoverCmd.c_str(), gArguments);
    } else {
        std::cerr << "NO RECOVERING CMD -> EXIT" << endl;
        exit(-1);
    }
}

static void catchsigs()
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = _sigaction;
    sa.sa_flags     = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
}

int main(int argc, char* argv[], char* env[])
{
    catchsigs();

    // Make a copy the command arguments
    for (int i = 0; i < argc; i++) {
        gArguments[i] = argv[i];
    }
    gArguments[argc] = nullptr;

    // Set the various default paths
    gCurrentDirectory   = fs::absolute(fs::current_path());
    gMakefilesDirectory = gCurrentDirectory / "makefiles";
    gSessionsDirectory  = gCurrentDirectory / "sessions";

    try {
        process_cmdline(argc, argv);
    } catch (...) {
        std::cerr << "Unknown parameter" << std::endl;
        exit(1);
    }

    if (gVerbosity >= 0) {
        time_t tmNow = time(0);
        std::cerr << "faustweb starting " << ctime(&tmNow) << "\n"
                  << "         port: " << gPort << "\n"
                  << "    directory: " << gCurrentDirectory << "\n"
                  << "    makefiles: " << gMakefilesDirectory << "\n"
                  << " sessions dir: " << gSessionsDirectory << "\n"
                  << "sessions size: " << computeSessionSize() << "\n"
                  << "    verbosity: " << gVerbosity << "\n"
                  << "  recover-cmd: " << gRecoverCmd << "\n"
                  << std::endl;

        // Print Faust version
        printFaustVersion();

        // Print running environment
        if (gVerbosity >= 2) {
            std::cerr << "\n\nBEGIN ENVIRONMENT" << std::endl;
            for (int i = 0; env[i] != 0; i++) {
                std::cerr << env[i] << std::endl;
            }
            std::cerr << "END ENVIRONMENT\n\n" << std::endl;
        }
    }

    // Check for ".../makefiles/" directory
    if (is_directory(gMakefilesDirectory)) {
        if (gVerbosity >= 2) std::cerr << "Makefiles directory available at path " << gMakefilesDirectory << std::endl;
    } else {
        std::cerr << "ERROR: no makefiles directory available at path " << gMakefilesDirectory << std::endl;
        exit(1);
    }

    // If needed creates ".../sessions/" directory
    if (create_directory(gSessionsDirectory)) {
        if (gVerbosity >= 1) std::cerr << "Create \"sessions\" directory at path " << gSessionsDirectory << std::endl;
    } else {
        if (gVerbosity >= 1) std::cerr << "Reuse \"sessions\" directory at path " << gSessionsDirectory << std::endl;
    }

    // Create, start and stop the http server
    FaustServer server(gPort, gMaxClients, gSessionsDirectory, gMakefilesDirectory, gLogfile, gMaxSessions);

    if (!server.start()) {
        std::cerr << "ERROR: unable to start webserver ! Check if port " << gPort << " is available" << std::endl;
        exit(1);
    } else {
        if (gVerbosity >= 2) std::cerr << "webserver started succesfully" << std::endl;
    }

    std::cerr << "type ctrl-c to quit" << std::endl;

    while (true) {
        // We never stop the server
        sleep(30);
    }
    return 0;
}
