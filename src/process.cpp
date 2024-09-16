#include <libsdb/process.hpp>

#include <libsdb/bit.hpp>
#include <libsdb/error.hpp>
#include <libsdb/pipe.hpp>

#include <sys/personality.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <format>
#include <print>

namespace
{
void exit_with_perror(sdb::pipe& channel, const std::string& prefix)
{
    auto message = std::format("{}: {}", prefix, std::strerror(errno));
    channel.write(reinterpret_cast<std::byte*>(message.data()), message.size());
    exit(-1);
}
}

sdb::stop_reason::stop_reason(int wait_status)
{
    if (WIFEXITED(wait_status))
    {
        reason = process_state::Exited;
        info = WEXITSTATUS(wait_status);
    }
    else if (WIFSIGNALED(wait_status))
    {
        reason = process_state::Terminated;
        info = WTERMSIG(wait_status);
    }
    else if (WIFSTOPPED(wait_status))
    {
        reason = process_state::Stopped;
        info = WSTOPSIG(wait_status);
    }

}

std::unique_ptr<sdb::process> sdb::process::launch(std::filesystem::path path, bool attach, std::optional<int> stdout_replacement)
{
    pipe channel(true); // We have to call pipe before we call fork()
    pid_t pid;
    if ((pid = fork()) < 0)
    {
        error::send_errno("fork failed");
    }

    if (pid == 0)
    {
        // We are in the child process
        channel.close_read();

        // Call before exec to disable Address Space Layout Randomisation
        personality(ADDR_NO_RANDOMIZE);

        if (stdout_replacement.has_value())
        {
            close(STDOUT_FILENO);
            if (dup2(stdout_replacement.value(), STDOUT_FILENO) < 0)
            {
                exit_with_perror(channel, "Failed to replace stdout");
            }
        }
        if (attach && ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0)
        {
            exit_with_perror(channel, "Tracing failed");
        }

        if (execlp(path.c_str(), path.c_str(), nullptr) < 0)
        {
            exit_with_perror(channel, "Exec failed");
        }
    }
    else
    {
        // We are in the parent process
        channel.close_write();
        auto data = channel.read();
        channel.close_read();

        if (!data.empty())
        {
            waitpid(pid, nullptr, 0);
            auto chars = reinterpret_cast<char*>(data.data());
            // TODO: This is size() + 1 in the book, but is not guaranteed legit
            error::send({chars, chars + data.size()});
        }
    }

    std::unique_ptr<process> proc{new process{pid, true, attach}};

    if (attach)
    {
        proc->wait_on_signal();
    }

    return proc;
}

std::unique_ptr<sdb::process> sdb::process::attach(pid_t pid)
{
    if (pid == 0)
    {
        error::send(std::format("Received invalid pid {}", pid));
    }

    if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0)
    {
        error::send_errno(std::format("Could not attach to pid {}", pid));
    }

    std::unique_ptr<process> proc{new process{pid, false, true}};
    proc->wait_on_signal();

    return proc;
}

sdb::process::~process()
{
    if (pid_ != 0)
    {
        int status;
        if (is_attached_)
        {
            if (state_ == process_state::Running)
            {
                kill(pid_, SIGSTOP);
                waitpid(pid_, &status, 0);
            }

            ptrace(PTRACE_DETACH, pid_, nullptr, nullptr);
            kill(pid_, SIGCONT);
        }

        if (terminate_on_end_)
        {
            kill(pid_, SIGKILL);
            waitpid(pid_, &status, 0);
        }
    }
}

void sdb::process::resume()
{
    auto program_counter = get_program_counter();
    if (breakpoint_sites_.enabled_stoppoint_at_address(program_counter))
    {
        auto& breakpoint = breakpoint_sites_.get_by_address(program_counter);
        breakpoint.disable();

        if (ptrace(PTRACE_SINGLESTEP, pid_, nullptr, nullptr) < 0)
        {
            error::send_errno("Could not single step");
        }

        // Wait until the process has executed the single instruction
        int wait_status;
        if (waitpid(pid_, &wait_status, 0) < 0)
        {
            error::send_errno("Could not waitpid");
        }
        breakpoint.enable();
    }

    if (ptrace(PTRACE_CONT, pid_, nullptr, nullptr) < 0)
    {
        error::send_errno("Could not resume");
    }

    state_ = process_state::Running;
}

sdb::stop_reason sdb::process::wait_on_signal()
{
    int wait_status;
    int options = 0;
    if (waitpid(pid_, &wait_status, options) < 0)
    {
        error::send_errno("waitpid failed");
    }

    stop_reason reason(wait_status);
    state_ = reason.reason;

    if (is_attached_ && state_ == process_state::Stopped)
    {
        read_all_registers();

        // Reset the program counter to before we executed int3 instruction
        auto instruction_start = get_program_counter() - 1;
        if (reason.info == SIGTRAP && breakpoint_sites_.enabled_stoppoint_at_address(instruction_start))
        {
            set_program_counter(instruction_start); 
        }
    }

    return reason;
}

sdb::stop_reason sdb::process::step_instruction()
{
    std::optional<sdb::breakpoint_site*> disabled_breakpoint; 
    auto program_counter = get_program_counter();
    if (breakpoint_sites_.enabled_stoppoint_at_address(program_counter))
    {
        auto& breakpoint = breakpoint_sites_.get_by_address(program_counter);
        breakpoint.disable();
        disabled_breakpoint = &breakpoint;
    }

    if (ptrace(PTRACE_SINGLESTEP, pid_, nullptr, nullptr) < 0)
    {
        error::send_errno("Could not single step");
    }

    auto reason = wait_on_signal();

    if (disabled_breakpoint.has_value())
    {
        disabled_breakpoint.value()->enable();
    }

    return reason;
}

void sdb::process::read_all_registers()
{
    if (ptrace(PTRACE_GETREGS, pid_, nullptr, &get_registers().data_.regs) < 0)
    {
        error::send_errno("Could not read general purporse registers");
    }

    if (ptrace(PTRACE_GETFPREGS, pid_, nullptr, &get_registers().data_.i387) < 0)
    {
        error::send_errno("Could not read floating point registers");
    }

    for (int i = 0; i < 8; ++i)
    {
        auto id = static_cast<int>(register_id::dr0) + i;
        auto info = register_info_by_id(static_cast<register_id>(id));

        errno = 0;
        std::int64_t data = ptrace(PTRACE_PEEKUSER, pid_, info.offset, nullptr);
        if (errno != 0)
        {
            error::send_errno(std::format("Could not read debug register {}", i));
        }

        get_registers().data_.u_debugreg[i] = data;
    }
}

void sdb::process::write_user_area(std::size_t offset, std::uint64_t data)
{
    if (ptrace(PTRACE_POKEUSER, pid_, offset, data) < 0)
    {
        error::send_errno("Could not write to user area");
    }
}

void sdb::process::write_fprs(const user_fpregs_struct& fprs)
{
    if (ptrace(PTRACE_SETFPREGS, pid_, nullptr, &fprs) < 0)
    {
        error::send_errno("Could not write floating point registers");
    }
}

void sdb::process::write_gprs(const user_regs_struct& gprs)
{
    if (ptrace(PTRACE_SETREGS, pid_, nullptr, &gprs) < 0)
    {
        error::send_errno("Could not write general purpose registers");
    }
}

sdb::breakpoint_site& sdb::process::create_breakpoint_site(virtual_address address)
{
    if (breakpoint_sites_.contains_address(address))
    {
        error::send(std::format("Breakpoint site already created at 0x{:#x}", address.addr()));
    }

    return breakpoint_sites_.push(std::unique_ptr<breakpoint_site>(new breakpoint_site(*this, address)));
}

std::vector<std::byte> sdb::process::read_memory(sdb::virtual_address address, std::size_t amount) const
{
    std::vector<std::byte> result(amount);

    iovec local_descriptor{result.data(), result.size()};
    iovec remote_descriptor{reinterpret_cast<void*>(address.addr()), amount};

    int local_count{1};
    int remote_count{1};
    int flags{0}; // Always set to 0 for process_vm_readv
    if (process_vm_readv(pid_, &local_descriptor, local_count, &remote_descriptor, remote_count, flags) < 0)
    {
        error::send_errno("Could not read process memory");
    }

    return result;
}

void sdb::process::write_memory(sdb::virtual_address address, span<const std::byte> data)
{
    std::size_t bytes_written = 0;
    while (bytes_written < data.size())
    {
        auto remaining = data.size() - bytes_written;
        std::uint64_t word; // The size ptrace allows to write
        if (remaining >= sizeof(word))
        {
            word = from_bytes<std::uint64_t>(data.begin() + bytes_written);
        }
        else
        {
            // We can only write 8 bytes, so if writing less we must first
            // read 8 bytes, and write back in the existing bytes + our data
            auto read = read_memory(address + bytes_written, sizeof(word));
            auto word_data = reinterpret_cast<char*>(&word);
            std::memcpy(word_data, data.begin() + bytes_written, remaining);
            std::memcpy(word_data + remaining, read.data() + remaining, sizeof(word) - remaining);
        }

        if (ptrace(PTRACE_POKEDATA, pid_, address + bytes_written, word) < 0)
        {
            error::send_errno("Could not write process memory");
        }

        bytes_written += sizeof(word);
    }
}
