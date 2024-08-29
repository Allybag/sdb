#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace sdb
{
class pipe
{
public:
    explicit pipe(bool close_on_exec);
    ~pipe();

    int get_read() const {return fds_[cReadFd]; }
    int get_write() const {return fds_[cWriteFd]; }
    int release_read();
    int release_write();
    void close_read();
    void close_write();

    std::vector<std::byte> read();
    void write(std::byte* bytes, std::size_t count);

private:
    static constexpr auto cReadFd{0};
    static constexpr auto cWriteFd{1};
    int fds_[2];
};
}
