#include <libsdb/pipe.hpp>
#include <libsdb/error.hpp>

#include <unistd.h>
#include <fcntl.h>

#include <utility>

#ifndef linux
int pipe2(int*, int) { return 0; }
#endif

sdb::pipe::pipe(bool close_on_exec)
{
    if (pipe2(fds_, close_on_exec ? O_CLOEXEC : 0) < 0)
    {
        error::send_errno("Pipe creation failed"); 
    }
}

std::vector<std::byte> sdb::pipe::read()
{
    static constexpr auto cBufferSize{1024};
    char buffer[cBufferSize];
    int chars_read;
    if ((chars_read = ::read(fds_[cReadFd], buffer, sizeof(buffer)) < 0))
    {
        error::send_errno("Could not read from pipe");
    }

    auto bytes = reinterpret_cast<std::byte*>(buffer);

    return std::vector<std::byte>(bytes, bytes + chars_read);
}

void sdb::pipe::write(std::byte* bytes, std::size_t count)
{
    if (::write(fds_[cWriteFd], bytes, count) < 0)
    {
        error::send_errno("Could not write to pipe"); 
    }
}

sdb::pipe::~pipe() 
{
    close_read();
    close_write();
}

int sdb::pipe::release_read()
{
    return std::exchange(fds_[cReadFd], -1);
}

int sdb::pipe::release_write()
{
    return std::exchange(fds_[cWriteFd], -1);
}

void sdb::pipe::close_read()
{
    if (fds_[cReadFd])
    {
        close(fds_[cReadFd]);
    }
}

void sdb::pipe::close_write()
{
    if (fds_[cWriteFd])
    {
        close(fds_[cWriteFd]);
    }
}
