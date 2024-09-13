#pragma once

#include <cstdint>
#include <cstddef>
#include <libsdb/types.hpp>

namespace sdb
{
class process;

class breakpoint_site
{
public:
    using id_type = std::int32_t;

    breakpoint_site() = delete;
    breakpoint_site(const breakpoint_site&) = delete;
    breakpoint_site& operator=(const breakpoint_site&) = delete;

    id_type id() const { return id_; }

    void enable();
    void disable();

    bool is_enabled() const { return is_enabled_; }
    virtual_address address() const { return address_; }

    bool at_address(virtual_address address) const {
        return address_ == address;
    }

    bool in_range(virtual_address low, virtual_address high) const {
        return low <= address_ && high > address_;
    }

private:
    friend process;

    breakpoint_site(process& proc, virtual_address address);

    id_type id_;
    process* process_;
    virtual_address address_;
    bool is_enabled_;
    std::byte saved_data_;

};
}
