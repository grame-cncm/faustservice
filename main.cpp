#include <sys/stat.h>
#include <iostream>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "server.hh"
#include "utilities.hh"

int gPort = 8888;
int gMaxClients = 2;
string gDirectory = "";

namespace po = boost::program_options;

// Processes command line arguments using boost/parse_options
void
process_cmdline(int argc, char* argv[])
{
/*
	int i = 1;

	while (i < argc) {
		if (isCmd(argv[i], "-p", "--port")) {
			gPort = atoi(argv[i + 1]);
			i += 2;
		} else if (isCmd(argv[i], "-p", "--port")) {
			gMaxClients = atoi(argv[i + 1]);
			i += 2;
		} else if (isCmd(argv[i], "-d", "--directory")) {
			gDirectory = string(argv[i + 1]);
			i += 2;
		} else {
			//cerr << "faust: unrecognized option \"" << argv[i] <<"\"" << endl;
			i++;
			//err++;
		}
	}
*/
	po::options_description desc("faustserver program options.");
	desc.add_options()
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

	if (vm.count("directory")) {
		gDirectory = vm["directory"].as<string>();
	}

}

int
main(int argc, char* argv[])
{
	process_cmdline(argc, argv);
/*
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
      // Log the failure
      exit (EXIT_FAILURE);
   }


   // We need to keep the cwd where it is
   // which is why all of this is commented out.
   // Change the current working directory
   //if ( (chdir ("/")) < 0) {
      // Log the failure
      //exit (EXIT_FAILURE);
   //}


   // Close out the standard file descriptors
   //close (STDIN_FILENO);
   //close (STDOUT_FILENO);
   //close (STDERR_FILENO);
 */
	FaustServer server(gPort, gMaxClients, gDirectory);

	if (!server.start())
		return 1;

	getchar();

	server.stop();

	return 0;
}