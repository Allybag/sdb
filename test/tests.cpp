#include <catch2/catch_test_macros.hpp>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>
#include <libsdb/pipe.hpp>
#include <libsdb/bit.hpp>

#include <sys/types.h>

#include <fstream>
#include <format>

using namespace sdb;

namespace
{
bool process_exists(pid_t pid)
{
    auto result = kill(pid, 0);
    return result != -1 && errno != ESRCH;
}

char get_process_status(pid_t pid)
{
    std::ifstream stat(std::format("/proc/{}/stat", pid));
    std::string data;
    std::getline(stat, data);
    auto last_paren_index = data.rfind(')');
    auto status_indicator_index = last_paren_index + 2;
    return data[status_indicator_index];
}
}

TEST_CASE("Launching", "process")
{
    auto proc = process::launch("yes");
    REQUIRE(process_exists(proc->pid()));
}

TEST_CASE("Launching non existent program", "process")
{
    REQUIRE_THROWS_AS(process::launch("Yahoo"), error);
}

TEST_CASE("Attaching", "process")
{
    auto target = process::launch("targets/run_endlessly", false);
    auto proc = process::attach(target->pid());
    static constexpr char cStoppedUnderTrace{'t'};
    REQUIRE(get_process_status(target->pid() == cStoppedUnderTrace));
}

TEST_CASE("Attaching invalid pid", "process")
{
    REQUIRE_THROWS_AS(process::attach(0), error);
}

TEST_CASE("Resuming", "process")
{
    static constexpr char cRunning{'R'};
    static constexpr char cSleeping{'S'};
    {
        auto proc = process::launch("targets/run_endlessly");
        proc->resume();

        auto status = get_process_status(proc->pid());
        auto success = (status == cRunning || status == cSleeping);
        REQUIRE(success);
    }

    {
        auto target = process::launch("targets/run_endlessly", false);
        auto proc = process::attach(target->pid());
        proc->resume();

        auto status = get_process_status(proc->pid());
        auto success = (status == cRunning || status == cSleeping);
        REQUIRE(success);
    }
}

TEST_CASE("Resume already terminated", "process")
{
    {
        auto proc = process::launch("targets/end_immediately");
        proc->resume();
        proc->wait_on_signal();
        REQUIRE_THROWS_AS(proc->resume(), error);
    }

    {
        auto target = process::launch("targets/end_immediately", false);
        auto proc = process::attach(target->pid());
        proc->resume();
        proc->wait_on_signal();
        REQUIRE_THROWS_AS(proc->resume(), error);
    }
}

TEST_CASE("Write register", "register")
{
    bool close_on_exec = false;
    sdb::pipe channel(close_on_exec);
    auto proc = process::launch("targets/reg_write", true, channel.get_write());
    channel.close_write();

    proc->resume();
    proc->wait_on_signal();

    auto& regs = proc->get_registers();
    regs.write_by_id(register_id::rsi, 0xcafecafe);

    proc->resume();
    proc->wait_on_signal();

    auto output = channel.read();
    REQUIRE(to_string_view(output) == "0xcafecafe");

    regs.write_by_id(register_id::mm0, 0xba5eba11);

    proc->resume();
    proc->wait_on_signal();

    output = channel.read();
    REQUIRE(to_string_view(output) == "0xba5eba11");

    regs.write_by_id(register_id::xmm0, 42.42);

    proc->resume();
    proc->wait_on_signal();

    output = channel.read();
    REQUIRE(to_string_view(output) == "42.42");

    // st0: First 80 bit x87 floating point register
    regs.write_by_id(register_id::st0, 42.42l);

    // fsw: FPU status word register
    // Bits 11 through 13 track the top of the stack
    // For some reason we should set this to 7?
    regs.write_by_id(register_id::fsw, std::uint16_t{0b0011100000000000});

    // ftw: FPU tag word register
    // Tracks which registers are valid (0b00), empty (0b11)), or "special"
    // st0: valid, st1 through st7: empty
    regs.write_by_id(register_id::ftw, std::uint16_t{0b0011111111111111});

    proc->resume();
    proc->wait_on_signal();

    output = channel.read();
    REQUIRE(to_string_view(output) == "42.42");
}

TEST_CASE("Read register", "register")
{
    auto proc = process::launch("targets/reg_read");
    auto& regs = proc->get_registers();

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<std::uint64_t>(register_id::r13) == 0xcafecafe);

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<std::uint8_t>(register_id::r13b) == 42);

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<byte64>(register_id::mm0) == to_byte64(0xba5eba11ull));

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<byte128>(register_id::xmm0) == to_byte128(64.125));

    proc->resume();
    proc->wait_on_signal();
    REQUIRE(regs.read_by_id_as<long double>(register_id::st0) == 64.125L);
}

TEST_CASE("Create breakpoint site", "breakpoint")
{
    auto proc = process::launch("targets/run_endlessly");
    auto& site = proc->create_breakpoint_site(virtual_address{42});
    REQUIRE(site.address().addr() == 42); 
}

TEST_CASE("Breakpoint site ids increase", "breakpoint")
{
    auto proc = process::launch("targets/run_endlessly");

    auto& site1 = proc->create_breakpoint_site(virtual_address{42});
    REQUIRE(site1.address().addr() == 42); 

    auto& site2 = proc->create_breakpoint_site(virtual_address{43});
    REQUIRE(site2.id() == site1.id() + 1);

    auto& site3 = proc->create_breakpoint_site(virtual_address{44});
    REQUIRE(site3.id() == site1.id() + 2);

    auto& site4 = proc->create_breakpoint_site(virtual_address{45});
    REQUIRE(site4.id() == site1.id() + 3);
}

TEST_CASE("Can find breakpoint site", "breakpoint") {
    auto proc = process::launch("targets/run_endlessly");
    const auto& cproc = proc;

    proc->create_breakpoint_site(virtual_address{ 42 });
    proc->create_breakpoint_site(virtual_address{ 43 });
    proc->create_breakpoint_site(virtual_address{ 44 });
    proc->create_breakpoint_site(virtual_address{ 45 });

    auto& s1 = proc->breakpoint_sites().get_by_address(virtual_address{ 44 });
    REQUIRE(proc->breakpoint_sites().contains_address(virtual_address{ 44 }));
    REQUIRE(s1.address().addr() == 44);

    auto& cs1 = cproc->breakpoint_sites().get_by_address(virtual_address{ 44 });
    REQUIRE(cproc->breakpoint_sites().contains_address(virtual_address{ 44 }));
    REQUIRE(cs1.address().addr() == 44);

    auto& s2 = proc->breakpoint_sites().get_by_id(s1.id() + 1);
    REQUIRE(proc->breakpoint_sites().contains_id(s1.id() + 1));
    REQUIRE(s2.id() == s1.id() + 1);
    REQUIRE(s2.address().addr() == 45);

    auto& cs2 = proc->breakpoint_sites().get_by_id(cs1.id() + 1);
    REQUIRE(cproc->breakpoint_sites().contains_id(cs1.id() + 1));

    REQUIRE(cs2.id() == cs1.id() + 1);
    REQUIRE(cs2.address().addr() == 45);
}

TEST_CASE("Cannot find breakpoint site", "breakpoint")
{
    auto proc = process::launch("targets/run_endlessly");
    const auto& cproc = proc;
    REQUIRE_THROWS_AS(
        proc->breakpoint_sites().get_by_address(virtual_address{ 44 }), error);
    REQUIRE_THROWS_AS(proc->breakpoint_sites().get_by_id(44), error);
    REQUIRE_THROWS_AS(
        cproc->breakpoint_sites().get_by_address(virtual_address{ 44 }), error);
    REQUIRE_THROWS_AS(cproc->breakpoint_sites().get_by_id(44), error);
}

TEST_CASE("Breakpoint site list size and emptiness", "breakpoint")
{
    auto proc = process::launch("targets/run_endlessly");
    const auto& cproc = proc;

    REQUIRE(proc->breakpoint_sites().empty());
    REQUIRE(proc->breakpoint_sites().size() == 0);
    REQUIRE(cproc->breakpoint_sites().empty());
    REQUIRE(cproc->breakpoint_sites().size() == 0);

    proc->create_breakpoint_site(virtual_address{ 42 });
    REQUIRE(!proc->breakpoint_sites().empty());
    REQUIRE(proc->breakpoint_sites().size() == 1);
    REQUIRE(!cproc->breakpoint_sites().empty());
    REQUIRE(cproc->breakpoint_sites().size() == 1);

    proc->create_breakpoint_site(virtual_address{ 43 });
    REQUIRE(!proc->breakpoint_sites().empty());
    REQUIRE(proc->breakpoint_sites().size() == 2);
    REQUIRE(!cproc->breakpoint_sites().empty());
    REQUIRE(cproc->breakpoint_sites().size() == 2);
}

TEST_CASE("Can iterate breakpoint sites", "breakpoint")
{
    auto proc = process::launch("targets/run_endlessly");
    const auto& cproc = proc;

    proc->create_breakpoint_site(virtual_address{ 42 });
    proc->create_breakpoint_site(virtual_address{ 43 });
    proc->create_breakpoint_site(virtual_address{ 44 });
    proc->create_breakpoint_site(virtual_address{ 45 });

    proc->breakpoint_sites().for_each([addr = 42](auto& site) mutable {
        REQUIRE(site.address().addr() == addr++);
    });

    cproc->breakpoint_sites().for_each([addr = 42](auto& site) mutable {
        REQUIRE(site.address().addr() == addr++);
    });
}
