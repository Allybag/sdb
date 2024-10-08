#pragma once

#include <libsdb/registers.hpp>
#include <libsdb/types.hpp>
#include <libsdb/breakpoint_site.hpp>
#include <libsdb/stoppoint_collection.hpp>

#include <sys/types.h>
#include <signal.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

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

    static std::unique_ptr<process> launch(std::filesystem::path path, bool attach = true, std::optional<int> stdout_replacement = std::nullopt);
    static std::unique_ptr<process> attach(pid_t pid);

    void write_user_area(std::size_t offset, std::uint64_t data);
    void write_fprs(const user_fpregs_struct& fprs);
    void write_gprs(const user_regs_struct& fprs);

    void resume();
    stop_reason wait_on_signal();
    sdb::stop_reason step_instruction();

    pid_t pid() const { return pid_; }
    process_state state() const { return state_; }
    registers& get_registers() { return *registers_; }
    const registers& get_registers() const { return *registers_; }

    void set_program_counter(virtual_address address)
    {
        get_registers().write_by_id(register_id::rip, address.addr());
    }

    breakpoint_site& create_breakpoint_site(virtual_address address);


    std::vector<std::byte> read_memory(virtual_address address, std::size_t amount) const;
    std::vector<std::byte> read_memory_without_traps(virtual_address address, std::size_t amount) const;
    void write_memory(virtual_address address, span<const std::byte> data);

    template <typename T>
    T read_memory_as(virtual_address address) const
    {
        auto data = read_memory(address, sizeof(T));
        return from_bytes<T>(data.data());
    }

    stoppoint_collection<breakpoint_site>& breakpoint_sites()
    {
        return breakpoint_sites_;
    }
    const stoppoint_collection<breakpoint_site>& breakpoint_sites() const
    {
        return breakpoint_sites_;
    }

    virtual_address get_program_counter() const
    {
        return virtual_address{get_registers().read_by_id_as<std::uint64_t>(register_id::rip)}; 
    }

private:
    process(pid_t pid, bool terminate_on_end, bool is_attached) : pid_(pid), terminate_on_end_(terminate_on_end), is_attached_{is_attached}, registers_{new registers(*this)} { }

    void read_all_registers();

    pid_t pid_ = 0;
    bool terminate_on_end_ = true;
    bool is_attached_ = true;
    process_state state_ = process_state::Stopped;
    std::unique_ptr<registers> registers_;
    stoppoint_collection<breakpoint_site> breakpoint_sites_;
};
}
