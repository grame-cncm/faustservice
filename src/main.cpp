#include <sys/stat.h>
#include <iostream>

#include <boost/filesystem.hpp>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "server.hh"
#include "utilities.hh"

int gPort = 8888;
int gMaxClients = 2;
string gDirectory = "";
string gMakefileDirectory = "../makefiles";
string gLogfile = "faustserver.log";
bool gDaemon = false;

namespace fs = boost::filesystem;
namespace po = boost::program_options;

// Processes command line arguments using boost/parse_options
void
process_cmdline(int argc, char* argv[])
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

    if (vm.count("daemon")) {
        gDaemon = true;
    }

    if (vm.count("max-clients")) {
        gMaxClients = vm["max-clients"].as<int>();
    }

    if (vm.count("logfile")) {
        gLogfile = vm["logfile"].as<string>();
    }

    if (vm.count("directory")) {
        gDirectory = vm["directory"].as<string>();
    }
}

int
main(int argc, char* argv[])
{
    process_cmdline(argc, argv);

    if (gDaemon) {
        // Create an autonomous process
        pid_t pid, sid;
        // Fork off the parent process
        pid = fork ();
        if (pid < 0) {
            exit (EXIT_FAILURE);
        }
        // If we got a good PID, then we can exit the parent process.
        if (pid > 0) {
            exit (EXIT_SUCCESS);
        }

        // Change the file mode mask
        umask (0);

        // Open any logs here

        // Create a new SID for the child process
        sid = setsid ();

        if (sid < 0) {
            FILE *f = fopen ((fs::path (gDirectory) / fs::path (gLogfile)).string().c_str(), "ab");
            const char *error = "Could not create an SID for the process.";
            fwrite (error, strlen(error), sizeof (char), f);
            fclose (f);
            exit (EXIT_FAILURE);
        }


        // We need to keep the cwd where it is
        // which is why all of this is commented out.
        //if ( (chdir ("/")) < 0) {
        // Log the failure
        //exit (EXIT_FAILURE);
        //}


        // Close out the standard file descriptors
        close (STDIN_FILENO);
        close (STDOUT_FILENO);
        close (STDERR_FILENO);
    }

    if (!fs::is_directory(gMakefileDirectory))
        gMakefileDirectory = PKGDATADIR;

    FaustServer server(gPort, gMaxClients, gDirectory, gMakefileDirectory, gLogfile);

    if (!server.start()) {
        return 1;
    }

    getchar();

    server.stop();

    return 0;
}