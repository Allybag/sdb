#pragma once

#include <libsdb/process.hpp>

#include <optional>

namespace sdb
{

class disassembler
{
    struct instruction
    {
        virtual_address address;
        std::string text;
    };

public:
    disassembler(process& proc) : process_(&proc) {}

    std::vector<instruction> disassemble(std::size_t instruction_count, std::optional<virtual_address> address = std::nullopt);

private:
    process* process_;
};
}
