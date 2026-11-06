#ifndef _PROCESS_
#define _PROCESS_

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

/*
 * Programs are launched directly with their arguments, never through a shell:
 * each argument reaches the program as one word, whatever characters it holds.
 */

// Resource limits applied to the launched program (0 = unchanged)
struct ProcessLimits {
    unsigned long stack_bytes = 0;
    unsigned long cpu_seconds = 0;
};

// Runs argv[0] (searched in PATH) with its arguments and waits for it. An empty workdir
// keeps the current directory, an empty stdout_file or stderr_file keeps the server's.
// Returns the exit code, 128 + the signal number if it was killed, -1 if it could not
// be launched.
int runProgram(const std::vector<std::string>& argv, const fs::path& workdir = {},
               const fs::path& stdout_file = {}, const fs::path& stderr_file = {},
               const ProcessLimits& limits = {});

// Runs argv[0] like runProgram and collects its standard output (and its error output
// when with_stderr is set, otherwise discarded). Returns the exit code like runProgram.
int captureProgram(const std::vector<std::string>& argv, std::string& output,
                   bool with_stderr = false);

// Splits a space-separated list of options into separate arguments
std::vector<std::string> splitArguments(const std::string& options);

#endif
