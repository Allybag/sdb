#pragma once

#include <array>
#include <cstddef>

namespace sdb
{
using byte64 = std::array<std::byte, 8>;
using byte128 = std::array<std::byte, 16>;

class virtual_address
{
public:
    virtual_address() = default;
    explicit virtual_address(std::uint64_t address) : address_(address) {}

    std::uint64_t addr() const
    {
        return address_;
    }

    virtual_address operator+(std::int64_t offset) const {
        return virtual_address(address_ + offset);
    }

    virtual_address operator-(std::int64_t offset) const {
        return virtual_address(address_ - offset);
    }

    virtual_address& operator+=(std::int64_t offset) {
        address_ += offset;
        return *this;
    }

    virtual_address& operator-=(std::int64_t offset) {
        address_ -= offset;
        return *this;
    }

    bool operator==(const virtual_address& other) const {
        return address_ == other.address_;
    }

    bool operator!=(const virtual_address& other) const {
        return address_ != other.address_;
    }

    bool operator<(const virtual_address& other) const {
        return address_ < other.address_;
    }

    bool operator<=(const virtual_address& other) const {
        return address_ <= other.address_;
    }

    bool operator>(const virtual_address& other) const {
        return address_ > other.address_;
    }

    bool operator>=(const virtual_address& other) const {
        return address_ >= other.address_;
    }

private:
    std::uint64_t address_{};
};
}
