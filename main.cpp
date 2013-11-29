#include <sys/stat.h>
#include <iostream>

#include <boost/filesystem.hpp>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "server.hh"
#include "utilities.hh"

namespace fs = boost::filesystem;
namespace po = boost::program_options;

int gPort = 8888;
int gMaxClients = 2;

fs::path gCurrentDirectory;			///< root directory were makefiles and sessions and log are located
fs::path gSessionsDirectory;		///< directory where sessions are stored
fs::path gMakefilesDirectory;		///< directory containing all the "<os>/Makefile.<architecture>[-32bits|-64bits]" makefiles
fs::path gLogfile;					///< faustweb logfile


// Processes command line arguments using boost/parse_options
void process_cmdline(int argc, char* argv[])
{
    po::options_description desc("faustserver program options.");
    desc.add_options()
    ("daemon", "run the server in mode daemon")
    ("directory,d", po::value<string>(), "directory in which files will be written")
    ("help,h", "produce this help message")
    ("max-clients,m", po::value<int>(), "maximum number of clients allowed to concurrently upload")
    ("port,p", po::value<int>(), "the listening port");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << desc << endl;
        return;
    }

    if (vm.count("port")) {
        gPort = vm["port"].as<int>();
    }

    if (vm.count("max-clients")) {
        gMaxClients = vm["max-clients"].as<int>();
    }

    if (vm.count("logfile")) {
        gLogfile = vm["logfile"].as<string>();
    }
#if 0
    if (vm.count("directory")) {
        gDirectory = vm["directory"].as<string>();
    }
#endif
}

int main(int argc, char* argv[])
{

    // Set the various default paths
    gCurrentDirectory = fs::absolute(fs::current_path());
    gMakefilesDirectory = gCurrentDirectory / "makefiles";
    gSessionsDirectory = gCurrentDirectory / "sessions";

    process_cmdline(argc, argv);

    std::cerr 	<< "faustwed starting "
                << " port:" << gPort
                << " directory:" << gCurrentDirectory
                << std::endl;

    // check for ".../makefiles/" directory
    if (is_directory(gMakefilesDirectory)) {
        std::cerr << "Makefiles directory available at path " << gMakefilesDirectory << std::endl;
    } else {
        std::cerr << "ERROR : no makefiles directory available at path " << gMakefilesDirectory << std::endl;
        exit(1);
    }

    // if needed creates ".../sessions/" directory
    if (create_directory(gSessionsDirectory)) {
        std::cerr << "Create \"sessions\" directory at path " << gSessionsDirectory << std::endl;
    } else {
        std::cerr << "Reuse \"sessions\" directory at path " << gSessionsDirectory << std::endl;
    }

	// Create, start and stop the http server
    FaustServer server(gPort, gMaxClients, gSessionsDirectory, gMakefilesDirectory, gLogfile);

    if (!server.start()) {
        std::cerr << "unable to start webserver" << std::endl;
        return 1;
    }

    getchar();

    server.stop();

    return 0;
}