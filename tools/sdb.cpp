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

void print_help(const std::vector<std::string>& args)
{
    if (args.size() == 1)
    {
        std::println(R"(Available commands:
            continue - Resume the process
            register - Commands for operating on registers
        )");
    }
    else if (is_prefix(args[1], "register"))
    {
        std::println(R"(Available commands:
            read
            read <register>
            read all
            write <register> <value>
        )");
    }
    else
    {
        std::println("No help available");
    }
}

void handle_register_read(sdb::process& process, const std::vector<std::string>& args)
{
    auto format = []<typename T>(T t)
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return std::format("{}", t);
        }
        else if constexpr (std::is_integral_v<T>)
        {
            return std::format("{:#0{}x}", t, sizeof(T) * 2 + 2);
        }
        else
        {
            // With fmtlib this would just be:
            // return fmt::format("[{:#04x}]", fmt::join(t, ","));
            std::string result{"["};
            for (const auto& val: t)
            {
                result = std::format("{}{:#04x},", result, static_cast<int>(val));
            }
            if (result.size() == 1)
            {
                // Empty array
                return std::string{"[]"};
            }

            // Dubiously swap the incorrect trailing , to a closing square bracket
            result[result.size() - 1] = ']';
            return result;
        }
    };

    if (args.size() == 2 || (args.size() == 3 && args[2] == "all"))
    {
        for (auto& info: sdb::g_register_infos)
        {
            auto should_print = (args.size() == 3 || info.type == sdb::register_type::Gpr) && info.name != "orig_rax";
            if (!should_print)
            {
                continue;
            }

            auto value = process.get_registers().read(info);
            std::println("{}:\t{}", info.name, std::visit(format, value));
        }
    }
    else if (args.size() == 3)
    {
        try
        {
            auto info = sdb::register_info_by_name(args[2]); 
            auto value = process.get_registers().read(info);
            std::println("{}:\t{}", info.name, std::visit(format, value));
        }
        catch (sdb::error& err)
        {
            std::println("No such register {}", args[2]);
            return;
        }
    }
    else
    {
        print_help({"help", "register"});
    }
}

void handle_register_write(sdb::process& process, const std::vector<std::string>& args)
{
}

void handle_register_command(sdb::process& process, const std::vector<std::string>& args)
{
    if (args.size() < 2)
    {
        print_help({"help", "register"});
        return;
    }

    if (is_prefix(args[1], "read"))
    {
        handle_register_read(process, args);
    }
    else if (is_prefix(args[1], "write"))
    {
        handle_register_write(process, args);
    }
    else
    {
        print_help({"help", "register"});
    }

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
    else if (is_prefix(command, "help"))
    {
        print_help(args);
    }
    else if (is_prefix(command, "register"))
    {
        handle_register_command(*process, args);
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
