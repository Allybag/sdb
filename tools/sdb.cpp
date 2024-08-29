#include <libsdb/libsdb.hpp>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>

#include <cstdio> // This include seems to be missing from readline
#include <readline/readline.h>
#include <readline/history.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>

#include <algorithm>
#include <iostream>
#include <string_view>
#include <sstream>
#include <string>
#include <print>
#include <unistd.h>
#include <vector>

namespace
{
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

std::unique_ptr<sdb::process> attach(int argc, const char** argv)
{
    if (argc == 3 && argv[1] == std::string_view("-p"))
    {
        pid_t pid = std::atoi(argv[2]);
        return sdb::process::attach(pid);
    }
    else
    {
        const char* program_path = argv[1];
        return sdb::process::launch(program_path);
    }
}

void print_stop_reason(const sdb::process& process, sdb::stop_reason reason)
{
    std::print("Process {} ", process.pid());
    switch (reason.reason)
    {
        case sdb::process_state::Exited:
            std::print("exited with status {}", reason.info);
            break;
        case sdb::process_state::Terminated:
            std::print("terminated with signal {}", sigabbrev_np(reason.info));
            break;
        case sdb::process_state::Stopped:
            std::print("stopped with signal {}", sigabbrev_np(reason.info));
            break;
    }
    std::println("");
}

void handle_command(std::unique_ptr<sdb::process>& process, std::string_view line)
{
    auto args = split(line, ' ');
    auto command = args[0];

    if (is_prefix(command, "continue"))
    {
        process->resume();
        auto reason = process->wait_on_signal();
        print_stop_reason(*process, reason);
    }
    else
    {
        std::println("Error: Unknown command");
    }
}

void main_loop(std::unique_ptr<sdb::process>& process)
{
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
            try
            {
                handle_command(process, line);
            }
            catch (const sdb::error& err)
            {
                std::println("sdb error: {}", err.what());
                std::cout << std::flush;
            }
        }
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

    try
    {
        auto process = attach(argc, argv);
        main_loop(process);
    }
    catch (const sdb::error& err)
    {
        std::println("sdb error: {}", err.what());
        std::cout << std::flush;
    }
}
