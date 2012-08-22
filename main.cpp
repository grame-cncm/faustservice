#include <sys/stat.h>
#include <iostream>

#include "server.hh"
#include "utilities.hh"

int gPort = 8888;
int gMaxClients = 2;
string gDirectory = "";

//-- command line tools

static bool isCmd(const char* cmd, const char* kw1)
{
  return (strcmp(cmd, kw1) == 0);
}

static bool isCmd(const char* cmd, const char* kw1, const char* kw2)
{
  return (strcmp(cmd, kw1) == 0) || (strcmp(cmd, kw2) == 0);
}

void
process_cmdline (int argc, char* argv[])
{
  int i = 1;

  while (i < argc)
    {
      if (isCmd(argv[i], "-p", "--port"))
        {
          gPort = atoi (argv[i+1]);
          i += 2;
        }
      else if (isCmd(argv[i], "-p", "--port"))
        {
          gMaxClients = atoi (argv[i+1]);
          i += 2;
        }
      else if (isCmd(argv[i], "-d", "--directory"))
        {
          gDirectory = string (argv[i+1]);
          i += 2;
        }
      else
        {
          //cerr << "faust: unrecognized option \"" << argv[i] <<"\"" << endl;
          i++;
          //err++;
        }
    }
}

int
main (int argc, char* argv[])
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
  FaustServer server (gPort, gMaxClients, gDirectory);

  if (!server.start ())
    return 1;

  getchar ();

  server.stop ();

  return 0;
}