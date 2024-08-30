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
}
