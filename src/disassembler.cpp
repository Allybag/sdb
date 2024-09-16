#include <libsdb/disassembler.hpp>

#include <Zydis/Zydis.h>

std::vector<sdb::disassembler::instruction> sdb::disassembler::disassemble(std::size_t instruction_count, std::optional<virtual_address> address)
{
    std::vector<instruction> result;
    result.reserve(instruction_count); 

    if (!address.has_value())
    {
        address = process_->get_program_counter();
    }

    static constexpr auto cMaxInstructionSize{15};
    auto code = process_->read_memory_without_traps(address.value(), instruction_count * cMaxInstructionSize);

    ZyanUSize offset = 0;
    ZydisDisassembledInstruction instr;

    while (ZYAN_SUCCESS(ZydisDisassembleATT(
        ZYDIS_MACHINE_MODE_LONG_64, address->addr(), code.data() + offset, code.size() - offset, &instr)) && instruction_count > 0)
    {
        result.push_back(instruction{address.value(), std::string(instr.text)});
        offset += instr.info.length;
        *address += instr.info.length;
        instruction_count--;
    }

    return result;
}
