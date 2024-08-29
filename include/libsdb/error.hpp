#pragma once

#include <stdexcept>
#include <cstring>
#include <format>

namespace sdb
{
class error : public std::runtime_error
{
public:
    static void send(const std::string& what) { throw error(what); }
    static void send_errno(const std::string& prefix)
    {
        throw error{std::format("{}: {}", prefix, std::strerror(errno))};
    }
private:
    error(const std::string& what) : std::runtime_error{what} {}
};
}
