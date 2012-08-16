#include <sys/stat.h>

#include "server.hh"
#include "utilities.hh"

int
main ()
{

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
  close (STDIN_FILENO);
  close (STDOUT_FILENO);
  close (STDERR_FILENO);

  FaustServer server (PORT);

  if (!server.start ())
    return 1;

  getchar ();

  server.stop ();

  return 0;
}