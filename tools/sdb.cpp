#include <libsdb/libsdb.hpp>


#include <libsdb/disassembler.hpp>
#include <libsdb/error.hpp>
#include <libsdb/process.hpp>

#include <cstdio> // This include seems to be missing from readline
#include <readline/readline.h>
#include <readline/history.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>

#include <algorithm>
#include <charconv>
#include <iostream>
#include <optional>
#include <string_view>
#include <sstream>
#include <string>
#include <print>
#include <utility>
#include <vector>

#include <unistd.h>

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

// Wrapper around std::from_chars which allows 0xblah and must use whole str
template <class Integral>
std::optional<Integral> to_integral(std::string_view sv, int base = 10)
{
    auto begin = sv.begin();
    if (base == 16 && sv.size() > 1 && begin[0] == '0' && begin[1] == 'x')
    {
        begin += 2;
    }

    Integral result;
    auto from_chars_result = std::from_chars(begin, sv.end(), result, base);

    if (from_chars_result.ptr != sv.end())
    {
        return std::nullopt;
    }

    return result;
}

template <>
std::optional<std::byte> to_integral(std::string_view sv, int base)
{
    auto as_unsigned_eight_bit = to_integral<std::uint8_t>(sv, base);
    if (!as_unsigned_eight_bit.has_value())
    {
        return std::nullopt;
    }

    return static_cast<std::byte>(as_unsigned_eight_bit.value());
}

template <class Float>
std::optional<Float> to_float(std::string_view sv)
{
    Float result;
    auto from_chars_result = std::from_chars(sv.begin(), sv.end(), result);

    if (from_chars_result.ptr != sv.end())
    {
        return std::nullopt;
    }

    return result;
}

void error_unless(bool condition)
{
    if (!condition)
    {
        sdb::error::send("Invalid format");
    }
};

template <std::size_t N>
auto parse_vector(std::string_view text)
{

    std::array<std::byte, N> bytes;
    const char* c = text.data();
    error_unless(*c++ == '[');

    for (auto i = 0; i < N - 1; ++i)
    {
        bytes[i] = to_integral<std::byte>({c, 4}, 16).value();
        c += 4;
        error_unless(*c++ == ',');
    }

    // Deal with last value specially as there's no trailing comma
    bytes[N - 1] = to_integral<std::byte>({c, 4}, 16).value();
    c += 4;

    error_unless(*c++ == ']');
    error_unless(c == text.end());

    return bytes;
}

auto parse_vector(std::string_view text)
{
    std::vector<std::byte> bytes;
    const char* c = text.data();

    error_unless(*c++ == '[');

    while (*c != ']')
    {
        // Read four characters from c as a hex byte
        bytes.push_back(to_integral<std::byte>({c, 4}, 16).value());
        c += 4;

        if (*c == ',')
        {
            ++c;
        }
        else
        {
            error_unless(*c == ']');
        }
    }

    error_unless(++c == text.end());
    return bytes;
}

void print_help(const std::vector<std::string>& args)
{
    if (args.size() == 1)
    {
        std::println(R"(Available commands:
            breakpoint  - Commands for operating on breakpoints
            continue    - Resume the process
            disassemble - Disassemble machine code to assembly
            memory      - Commands for operating on memory
            register    - Commands for operating on registers
            step        - Step over and execute a single instruction
        )");
    }
    else if (is_prefix(args[1], "breakpoint"))
    {
        std::println(R"(Available commands:
            list
            delete <id>
            disable <id>
            enable <id>
            set <address>
        )");
    }
    else if (is_prefix(args[1], "disassemble"))
    {
        std::println(R"(Available options:
            -c <number of instructions>
            -a <start address>
        )");
    }
    else if (is_prefix(args[1], "memory"))
    {
        std::println(R"(Available commands:
            read <address>
            read <address> <number of bytes to read>
            write <address> <bytes>
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

void print_disassembly(sdb::process& process, sdb::virtual_address address, std::size_t instuction_count)
{
    sdb::disassembler dis(process);
    auto instructions = dis.disassemble(instuction_count, address); 
    for (auto& instruction : instructions)
    {
        std::println("{:#018x}: {}", instruction.address.addr(), instruction.text);
    }
}

void handle_disassemble_command(sdb::process& process, const std::vector<std::string>& args)
{
    auto address = process.get_program_counter();
    std::size_t instruction_count = 5;

    auto it = args.begin() + 1;
    while (it != args.end())
    {
        if (*it == "-a" && it + 1 != args.end())
        {
            ++it;
            auto opt_addr = to_integral<std::uint64_t>(*it++, 16);
            if (!opt_addr)
            {
                sdb::error::send("Invalid address format");
            }

            address = sdb::virtual_address{ *opt_addr };
        }
        else if (*it == "-c" && it + 1 != args.end())
        {
            ++it;
            auto opt_n = to_integral<std::size_t>(*it++);
            if (!opt_n)
            {
                sdb::error::send("Invalid instruction count");
            }

            instruction_count = *opt_n;
        }
        else
        {
            print_help({ "help", "disassemble" });
            return;
        }
    }

    print_disassembly(process, address, instruction_count);
}

void handle_memory_read_command(sdb::process& process, const std::vector<std::string>& args)
{
    auto address = to_integral<std::uint64_t>(args[2], 16);
    if (!address)
    {
        sdb::error::send("Invalid address format");
    }

    auto read_byte_count = 32;
    if (args.size() == 4) 
    {
        auto bytes_arg = to_integral<std::size_t>(args[3]);
        if (!bytes_arg)
        {
            sdb::error::send("Invalid number of bytes to read");
        }
        read_byte_count = *bytes_arg;
    }

    auto data = process.read_memory(sdb::virtual_address{address.value()}, read_byte_count);

    for (std::size_t i = 0; i < data.size(); i += 16)
    {
        auto start = data.begin() + i;
        auto end = data.begin() + std::min(i + 16, data.size());
        std::print("{:#016x}: ", address.value() + i);
        for (auto it = start; it != end; it++)
        {
            std::print(" {:02x}", static_cast<std::uint8_t>(*it));
        }
        std::println();
    }
}
void handle_memory_write_command(sdb::process& process, const std::vector<std::string>& args)
{
    if (args.size() != 4)
    {
        print_help({"help", "memory"});
        return;
    }

    auto address = to_integral<std::uint64_t>(args[2], 16);
    if (!address.has_value())
    {
        sdb::error::send("Invalid address format");
    }

    auto data = parse_vector(args[3]);
    process.write_memory(sdb::virtual_address{address.value()}, {data.data(), data.size()});
}

void handle_memory_command(sdb::process& process, const std::vector<std::string>& args)
{
    if (args.size() < 3)
    {
        print_help({"help", "memory"});
        return;
    }

    if (is_prefix(args[1], "read"))
    {
        handle_memory_read_command(process, args);
    }
    else if (is_prefix(args[1], "write"))
    {
        handle_memory_write_command(process, args);
    }
    else
    {
        print_help({"help", "memory"});
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

sdb::registers::value parse_register_value(sdb::register_info info, std::string_view text)
{
    try
    {
        if (info.format == sdb::register_format::UnsignedInt)
        {
            switch (info.size)
            {
                case 1:
                    return to_integral<std::uint8_t>(text, 16).value();
                case 2:
                    return to_integral<std::uint16_t>(text, 16).value();
                case 4:
                    return to_integral<std::uint32_t>(text, 16).value();
                case 8:
                    return to_integral<std::uint64_t>(text, 16).value();
            }
        }
        else if (info.format == sdb::register_format::DoubleFloat)
        {
            return to_float<double>(text).value();
        }
        else if (info.format == sdb::register_format::LongDouble)
        {
            return to_float<long double>(text).value();
        }
        else if (info.format == sdb::register_format::Vector)
        {
            if (info.size == 8)
            {
                return parse_vector<8>(text);
            }
            else if (info.size == 16)
            {
                return parse_vector<16>(text);
            }
        }
    }
    catch (...)
    {
        sdb::error::send("Invalid format");
    }

    std::unreachable();
}

void handle_register_write(sdb::process& process, const std::vector<std::string>& args)
{
    if (args.size() != 4)
    {
        print_help({"help", "register"});
        return;
    }

    try
    {
        auto info = sdb::register_info_by_name(args[2]);
        auto value = parse_register_value(info, args[3]);
        process.get_registers().write(info, value);
    }
    catch (sdb::error& err)
    {
        std::println("{}", err.what());
        return;
    }
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
        auto program_path = argv[1];
        auto proc = sdb::process::launch(program_path);
        std::println("Launched process with pid {}", proc->pid());
        return proc;
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
            std::print("stopped with signal {} at {:#x}", sigabbrev_np(reason.info), process.get_program_counter().addr());
            break;
    }
    std::println("");
}

void handle_stop(sdb::process& process, sdb::stop_reason reason)
{
    print_stop_reason(process, reason);
    if (reason.reason == sdb::process_state::Stopped)
    {
        print_disassembly(process, process.get_program_counter(), 5);
    }
}

void handle_breakpoint_command(sdb::process& process, const std::vector<std::string>& args)
{
    if (args.size() < 2)
    {
        print_help({"help", "breakpoint"});
        return;
    }

    auto command = args[1];

    if (is_prefix(command, "list"))
    {
        if (process.breakpoint_sites().empty())
        {
            std::println("No breakpoints set");
        }
        else
        {
            std::println("Current breakpoints:");
            process.breakpoint_sites().for_each([] (auto& site) {
                std::println("{}: address = {:#x}, {}", site.id(), site.address().addr(), site.is_enabled() ? "enabled" : "disabled");
            });
        }
        return;
    }

    // All subcommands other than list take an additional argument
    if (args.size() < 3)
    {
        print_help({"help", "breakpoint"});
        return;
    }

    if (is_prefix(command, "set"))
    {
        auto address = to_integral<std::uint64_t>(args[2], 16);
        if (!address)
        {
            std::println("Breakpoint command expects address in 0x89ab format");
            return;
        }

        process.create_breakpoint_site(sdb::virtual_address{*address}).enable();
        return;
    }

    auto id = to_integral<sdb::breakpoint_site::id_type>(args[2]);
    if (!id.has_value())
    {
        std::println("Command expects breakpoint id");
    }

    if (is_prefix(command, "enable"))
    {
        process.breakpoint_sites().get_by_id(id.value()).enable();
    }
    else if (is_prefix(command, "disable"))
    {
        process.breakpoint_sites().get_by_id(id.value()).disable();
    }
    else if (is_prefix(command, "delete"))
    {
        process.breakpoint_sites().remove_by_id(id.value());
    }

}

void handle_command(std::unique_ptr<sdb::process>& process, std::string_view line)
{
    auto args = split(line, ' ');
    auto command = args[0];

    if (is_prefix(command, "help"))
    {
        print_help(args);
    }
    else if (is_prefix(command, "breakpoint"))
    {
        handle_breakpoint_command(*process, args);
    }
    else if (is_prefix(command, "continue"))
    {
        process->resume();
        auto reason = process->wait_on_signal();
        handle_stop(*process, reason);
    }
    else if (is_prefix(command, "disassemble"))
    {
        handle_disassemble_command(*process, args);
    }
    else if (is_prefix(command, "memory"))
    {
        handle_memory_command(*process, args);
    }
    else if (is_prefix(command, "register"))
    {
        handle_register_command(*process, args);
    }
    else if (is_prefix(command, "step"))
    {
        auto reason = process->step_instruction();
        handle_stop(*process, reason);
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
