#include <libsdb/libsdb.hpp>

#include <cstdio> // This include seems to be missing from readline
#include <readline/readline.h>
#include <readline/history.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>

#ifndef linux
// Bit dubious, allow me to compile on MacOS
using pid_t = int;
using ptrace_request = int;
long ptrace(ptrace_request request, pid_t pid, void *addr, void *data) { return 0l; }
#define PTRACE_ATTACH 0
#define PTRACE_CONT 0
#define PTRACE_TRACEME 0
#endif

#include <algorithm>
#include <string_view>
#include <sstream>
#include <string>
#include <print>
#include <unistd.h>
#include <vector>

namespace
{
    pid_t attach(int argc, const char** argv)
    {
        pid_t pid = 0;
        if (argc == 3 && argv[1] == std::string_view("-p"))
        {
            pid = std::atoi(argv[2]);
            if (pid <= 0)
            {
                std::println("Received invalid pid {}", pid);
                return -1;
            }

            if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0)
            {
                std::perror(std::format("Could not attach to pid {}", pid).c_str());
                return -1;
            }
        }
        else
        {
            const char* program_path = argv[1];
            if ((pid = fork()) < 0)
            {
                std::perror("fork failed");
                return -1;
            }

            if (pid == 0)
            {
                // We are in the child process
                if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0)
                {
                    std::perror("Tracing failed");
                    return -1;
                }

                if (execlp(program_path, program_path, nullptr) < 0)
                {
                    std::perror("Exec failed");
                    return -1;
                }
            }
        }

        return pid;
    }


    std::vector<std::string> split(std::string_view str, char delimiter)
    {
        std::vector<std::string> out{};
        std::stringstream ss{std::string{str}};
        std::string item;

        while (std::getline(ss, item, delimiter))
        {
            out.push_back(item);
        }

        return out;
    }

    bool is_prefix(std::string_view str, std::string_view of)
    {
        if (str.size() > of.size()) return false;
        return std::equal(str.begin(), str.end(), of.begin());
    }

    void resume(pid_t pid)
    {
        if (ptrace(PTRACE_CONT, pid, nullptr, nullptr) < 0)
        {
            std::println("Couldn't continue");
            std::exit(-1);
        }
    }

    void wait_on_signal(pid_t pid)
    {
        int wait_status;
        int options = 0;
        if (waitpid(pid, &wait_status, options) < 0)
        {
            std::perror("waitpid failed");
            std::exit(-1);
        }
    }

    void handle_command(pid_t pid, std::string_view line)
    {
        auto args = split(line, ' ');
        auto command = args[0];

        if (is_prefix(command, "continue"))
        {
            resume(pid);
            wait_on_signal(pid);
        }
        else
        {
            std::println("Error: Unknown command");
        }
    }
}

int main(int argc, const char** argv)
{
    if (argc == 1)
    {
        std::println("No arguments given");
        return -1;
    }

    pid_t pid = attach(argc, argv);

    int wait_status;
    int options{0};
    if (waitpid(pid, &wait_status, options) < 0)
    {
        std::perror("waitpid failed");
    }

    char* line_ptr = nullptr;
    while ((line_ptr = readline("sdb> ")) !=  nullptr)
    {
        std::string line;

        if (line_ptr == std::string_view{""})
        {
            free(line_ptr);
            if (history_length > 0)
            {
                line = history_list()[history_length - 1]->line;
            }
        }
        else
        {
            line = line_ptr;
            add_history(line_ptr);
            free(line_ptr);
        }

        if (!line.empty())
        {
            handle_command(pid, line);
        }
    }
}
