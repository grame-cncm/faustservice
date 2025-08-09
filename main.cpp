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
#include <fstream>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include <signal.h>
#include <unistd.h>
#include <string>

#include "FaustWebService.h"
#include "MCPServer.h"
#include "server.hh"
#include "utilities.hh"

namespace fs = boost::filesystem;
namespace po = boost::program_options;

int      gPort        = 8888;
int      gMaxClients  = 2;
bool     gAnyOrigin   = true;
int      gMaxSessions = 50;
int      gVerbosity   = 0;
bool     gMCPMode     = false;  ///< MCP server mode on stdin/stdout
fs::path gMCPLogFile;           ///< Log file for MCP mode debugging

fs::path gCurrentDirectory;   ///< root directory were makefiles and sessions and log are located
fs::path gSessionsDirectory;  ///< directory where sessions are stored
fs::path
    gMakefilesDirectory;  ///< directory containing all the "<os>/Makefile.<architecture>[-32bits|-64bits]" makefiles
fs::path gLogfile;        ///< faustweb logfile
string   gRecoverCmd;     ///< system command to launch for recovery after intercepted crash
char*    gArguments[256];

// Processes command line arguments using boost/parse_options
static void process_cmdline(int argc, char* argv[])
{
    po::options_description desc("faustweb program options");
    desc.add_options()("sessions-dir,d", po::value<string>(), "directory in which sessions files will be written");
    desc.add_options()("help,h", "produce this help message");
    desc.add_options()("any-origin,a", "Adds any origin when answering requests");
    desc.add_options()("max-clients,m", po::value<int>(), "maximum number of clients allowed to concurrently upload");
    desc.add_options()("port,p", po::value<int>(), "the listening port");
    desc.add_options()("max-sessions,n", po::value<int>(), "maximum number of cached sessions");
    desc.add_options()("verbose,v", po::value<int>(), "0: normal; 1: verbose; 2: very verbose");
    desc.add_options()("recover-cmd,r", po::value<std::string>(),
                       "program (usually self) to launch after crash recovery");
    desc.add_options()("mcp", "enable MCP (Model Context Protocol) server mode on stdin/stdout");
    desc.add_options()("mcp-log", po::value<std::string>(), "log file for MCP mode debugging");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << desc << endl;
        exit(0);
    }

    if (vm.count("port")) {
        gPort = vm["port"].as<int>();
    }

    if (vm.count("max-clients")) {
        gMaxClients = vm["max-clients"].as<int>();
    }

    if (vm.count("max-sessions")) {
        gMaxSessions = vm["max-sessions"].as<int>();
    }

    if (vm.count("logfile")) {
        gLogfile = vm["logfile"].as<string>();
    }

    if (vm.count("allow-any-origin")) {
        gAnyOrigin = true;
    }

    if (vm.count("sessions-dir")) {
        gSessionsDirectory = vm["sessions-dir"].as<string>();
    }

    if (vm.count("verbose")) {
        gVerbosity = vm["verbose"].as<int>();
    }

    if (vm.count("recover-cmd")) {
        gRecoverCmd = vm["recover-cmd"].as<string>();
    }

    if (vm.count("mcp")) {
        gMCPMode = true;
    }

    if (vm.count("mcp-log")) {
        gMCPLogFile = vm["mcp-log"].as<string>();
    }
}

static void printFaustVersion()
{
    // In MCP mode, redirect system output to stderr
    if (gMCPMode) {
        // int err = system("faust -v >&2");  // Redirect stdout to stderr
        // if (err != 0) std::cerr << "ERROR: Faust not found" << std::endl;
    } else {
        int err = system("faust -v");
        if (err != 0) std::cerr << "ERROR: Faust not found" << std::endl;
    }
}

static size_t computeSessionSize()
{
    size_t size = 0;

    try {
        fs::recursive_directory_iterator it(gSessionsDirectory);
        for (; it != fs::recursive_directory_iterator(); ++it) {
            if (!fs::is_directory(*it)) size += fs::file_size(*it);
        }
        return size;
    } catch (const boost::filesystem::filesystem_error& e) {
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
    cerr << "\n\n";
    cerr << "SIGNAL #" << signal << " CATCHED!" << endl;
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
    // Use the executable path to find the base directory
    fs::path execPath = fs::absolute(fs::path(argv[0]));
    fs::path execDir = execPath.parent_path();
    
    // If execDir is empty (e.g., command found in PATH), use current directory
    if (execDir.empty() || !fs::exists(execDir / "makefiles")) {
        gCurrentDirectory = fs::absolute(fs::current_path());
    } else {
        gCurrentDirectory = execDir;
    }
    
    gMakefilesDirectory = gCurrentDirectory / "makefiles";
    gSessionsDirectory  = gCurrentDirectory / "sessions";

    try {
        process_cmdline(argc, argv);
    } catch (...) {
        cerr << "Unknown parameter" << endl;
        exit(1);
    }

    // In MCP mode, redirect all outputs
    std::ofstream*  mcpLogStream    = nullptr;
    std::streambuf* originalCoutBuf = nullptr;
    std::streambuf* originalCerrBuf = nullptr;

    if (gMCPMode) {
        // Save original buffers
        originalCoutBuf = std::cout.rdbuf();
        originalCerrBuf = std::cerr.rdbuf();

        if (!gMCPLogFile.empty()) {
            // Redirect stdout and stderr to log file
            mcpLogStream = new std::ofstream(gMCPLogFile.string(), std::ios::app);
            if (mcpLogStream->is_open()) {
                std::cout.rdbuf(mcpLogStream->rdbuf());
                std::cerr.rdbuf(mcpLogStream->rdbuf());
                // Log start of MCP session
                time_t tmNow = time(0);
                std::cerr << "\n=== MCP Mode Started ===" << std::endl;
                std::cerr << "Time: " << ctime(&tmNow) << std::endl;
            }
        } else {
            // No log file specified - redirect cout to cerr, cerr stays on stderr
            std::cout.rdbuf(std::cerr.rdbuf());
        }
    }

    if (gVerbosity >= 0 && !gMCPMode) {
        // Normal mode - print to stderr
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
    } else if (gMCPMode && !gMCPLogFile.empty()) {
        // MCP mode with log file - info already logged when we opened the file
        std::cerr << "FaustWeb HTTP server on port " << gPort << std::endl;
        std::cerr << "MCP log file: " << gMCPLogFile << std::endl;
    }

    // Print Faust version (only if not in MCP mode or if we have a log file)
    if (!gMCPMode || !gMCPLogFile.empty()) {
        printFaustVersion();
    }

    // Print running environment
    if (gVerbosity >= 2 && (!gMCPMode || !gMCPLogFile.empty())) {
        std::cerr << "\n\nBEGIN ENVIRONMENT" << std::endl;
        for (int i = 0; env[i] != 0; i++) {
            std::cerr << env[i] << std::endl;
        }
        std::cerr << "END ENVIRONMENT\n\n" << std::endl;
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

    if (!gMCPMode) {
        std::cerr << "type ctrl-c to quit" << std::endl;
    }

    if (gMCPMode) {
        // Start MCP server on stdin/stdout
        std::cerr << "Starting MCP server mode - HTTP server continues on port " << gPort << std::endl;

        // Create log stream (use cerr if no log file specified)
        std::ostream* logStream = &std::cerr;
        std::ofstream fileLogStream;
        if (!gMCPLogFile.empty()) {
            fileLogStream.open(gMCPLogFile.string(), std::ios::app);
            if (fileLogStream.is_open()) {
                logStream = &fileLogStream;
            }
        }

        // Restore stdout for MCP communication (was redirected to log)
        if (originalCoutBuf) {
            std::cout.rdbuf(originalCoutBuf);
        }

        // Create MCP server
        MCPServer mcpServer(std::cin, std::cout, *logStream, "FaustWeb", "1.0.0");

        // Create FaustWebService and register tools
        FaustWebService faustService(&mcpServer, &server, server.getTargets(), 
                                    gSessionsDirectory, gMakefilesDirectory);
        faustService.initialize();

        // Run the MCP server (blocking)
        mcpServer.run();

        // If MCP server exits, stop HTTP server
        server.stop();
    } else {
        // Normal mode - infinite loop
        while (true) {
            sleep(30);
        }
    }
    return 0;
}
