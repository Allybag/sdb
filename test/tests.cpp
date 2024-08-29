#include <catch2/catch_test_macros.hpp>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>

#include <sys/types.h>

using namespace sdb;

namespace
{
bool process_exists(pid_t pid)
{
    auto result = kill(pid, 0);
    return result != -1 && errno != ESRCH;
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
