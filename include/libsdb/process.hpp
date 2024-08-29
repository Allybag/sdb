#pragma once

#include <filesystem>
#include <memory>

#include <sys/types.h>
#include <signal.h>

#ifndef linux
// Bit dubious, allow me to compile on MacOS
using pid_t = int;
using ptrace_request = int;
using sig = int;
inline long ptrace(ptrace_request request, pid_t pid, void *addr, void *data) { return 0l; }
inline const char* sigabbrev_np(sig signal) { return ""; }
#define PTRACE_ATTACH 0
#define PTRACE_CONT 0
#define PTRACE_DETACH 0
#define PTRACE_TRACEME 0
#endif

namespace sdb
{
enum class process_state
{
    Stopped,
    Running,
    Exited,
    Terminated
};

struct stop_reason
{
    stop_reason(int wait_status);

    process_state reason;
    std::uint8_t info;
};

class process
{
public:
    process() = delete;
    process(const process&) = delete;
    process& operator=(const process&) = delete;

    ~process();

    static std::unique_ptr<process> launch(std::filesystem::path path);
    static std::unique_ptr<process> attach(pid_t pid);

    void resume();
    stop_reason wait_on_signal();

    pid_t pid() const { return pid_; }
    process_state state() const { return state_; }

private:
    process(pid_t pid, bool terminate_on_end) : pid_(pid), terminate_on_end_(terminate_on_end) { }
    pid_t pid_ = 0;
    bool terminate_on_end_ = true;
    process_state state_ = process_state::Stopped;
};
}
