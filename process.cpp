#include "process.hh"

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <sstream>

// The server is multithreaded: between fork and exec the child only calls
// async-signal-safe functions, on data prepared before fork.

static std::vector<char*> makeArgv(const std::vector<std::string>& argv)
{
    std::vector<char*> args;
    for (const std::string& arg : argv) {
        args.push_back(const_cast<char*>(arg.c_str()));
    }
    args.push_back(nullptr);
    return args;
}

static void setLimit(int resource, unsigned long value)
{
    if (value > 0) {
        struct rlimit limit;
        limit.rlim_cur = value;
        limit.rlim_max = value;
        setrlimit(resource, &limit);
    }
}

static void redirect(const char* file, int fd)
{
    int out = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) {
        _exit(127);
    }
    dup2(out, fd);
    close(out);
}

static int waitProgram(pid_t pid)
{
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return -1;
        }
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return -1;
}

int runProgram(const std::vector<std::string>& argv, const fs::path& workdir,
               const fs::path& stdout_file, const fs::path& stderr_file,
               const ProcessLimits& limits)
{
    if (argv.empty()) {
        return -1;
    }
    std::vector<char*> args       = makeArgv(argv);
    std::string        dir        = workdir.string();
    std::string        out_file   = stdout_file.string();
    std::string        err_file   = stderr_file.string();

    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        if (!dir.empty() && chdir(dir.c_str()) != 0) {
            _exit(127);
        }
        setLimit(RLIMIT_STACK, limits.stack_bytes);
        setLimit(RLIMIT_CPU, limits.cpu_seconds);
        if (!out_file.empty()) {
            redirect(out_file.c_str(), STDOUT_FILENO);
        }
        if (!err_file.empty()) {
            redirect(err_file.c_str(), STDERR_FILENO);
        }
        execvp(args[0], args.data());
        _exit(127);
    }
    return waitProgram(pid);
}

int captureProgram(const std::vector<std::string>& argv, std::string& output, bool with_stderr)
{
    output.clear();
    if (argv.empty()) {
        return -1;
    }
    std::vector<char*> args = makeArgv(argv);

    int fds[2];
    if (pipe(fds) != 0) {
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        if (with_stderr) {
            dup2(fds[1], STDERR_FILENO);
        } else {
            redirect("/dev/null", STDERR_FILENO);
        }
        close(fds[1]);
        execvp(args[0], args.data());
        _exit(127);
    }
    close(fds[1]);
    char    buffer[1024];
    ssize_t count;
    while ((count = read(fds[0], buffer, sizeof(buffer))) != 0) {
        if (count > 0) {
            output.append(buffer, count);
        } else if (errno != EINTR) {
            break;
        }
    }
    close(fds[0]);
    return waitProgram(pid);
}

std::vector<std::string> splitArguments(const std::string& options)
{
    std::istringstream       iss(options);
    std::vector<std::string> args;
    std::string              token;
    while (iss >> token) {
        args.push_back(token);
    }
    return args;
}
