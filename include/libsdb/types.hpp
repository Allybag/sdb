#pragma once

#include <array>
#include <cstddef>
#include <vector>

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

template <typename T>
class span
{
public:
    span() = default;
    span(T* data, std::size_t size) : data_(data), size_(size) {}
    span(T* data, T* end): data_(data), size_(end - data) {}
    template <typename U>
    span(const std::vector<U>& vec) : data_(vec.data()), size_(vec.size()) {}

    T* begin() const { return data_; }
    T* end() const { return data_ + size_; }
    std::size_t size() const { return size_; }
    T& operator[](std::size_t n) { return *(data_ + n); }
    
private:
    T* data_{nullptr};
    std::size_t size_ = 0;
};
}
