#include <libsdb/process.hpp>
#include <libsdb/registers.hpp>
#include <libsdb/bit.hpp>

#include <algorithm>
#include <print>
#include <type_traits>

namespace
{
template <typename T>
sdb::byte128 widen(const sdb::register_info& info, T t)
{
    using namespace sdb;
    if constexpr (std::is_floating_point_v<T>)
    {
        if (info.format == register_format::DoubleFloat)
        {
            return to_byte128(static_cast<double>(t)); 
        }
        if (info.format == register_format::LongDouble)
        {
            return to_byte128(static_cast<long double>(t)); 
        }
    }
    else if constexpr (std::is_signed_v<T>)
    {
        if (info.format == register_format::UnsignedInt)
        {
            switch (info.size)
            {
                case 2:
                    return to_byte128(static_cast<std::int16_t>(t)); 
                case 4:
                    return to_byte128(static_cast<std::int32_t>(t)); 
                case 8:
                    return to_byte128(static_cast<std::int64_t>(t)); 
            }
        }
    }

    auto result = to_byte128(t); 
    std::fill(as_bytes(result) + sizeof(T), as_bytes(result) + info.size + 1, std::byte(0));
    return result;
}
}

sdb::registers::value sdb::registers::read(const register_info& info) const
{
    auto bytes = as_bytes(data_);

    if (info.format == register_format::UnsignedInt)
    {
        switch (info.size)
        {
            case 1:
                return from_bytes<std::uint8_t>(bytes + info.offset);
            case 2:
                return from_bytes<std::uint16_t>(bytes + info.offset);
            case 4:
                return from_bytes<std::uint32_t>(bytes + info.offset);
            case 8:
                return from_bytes<std::uint64_t>(bytes + info.offset);
        }
    }
    else if (info.format == register_format::DoubleFloat)
    {
        return from_bytes<double>(bytes + info.offset);
    }
    else if (info.format == register_format::LongDouble)
    {
        return from_bytes<long double>(bytes + info.offset);
    }
    else if (info.format == register_format::Vector && info.size == 8)
    {
        return from_bytes<byte64>(bytes + info.offset);
    }
    else
    {
        return from_bytes<byte128>(bytes + info.offset);
    }
}

void sdb::registers::write(const register_info& info, value val)
{
    auto bytes = as_bytes(data_);
    std::visit([&info, &bytes](auto& v) {
        if (sizeof(v) <= info.size)
        {
            auto wide_value = widen(info, v);
            auto value_bytes = as_bytes(wide_value);
            std::copy(value_bytes, value_bytes + sizeof(v), bytes + info.offset);
        }
        else
        {
            std::println("sdb::registers::write called with mismatched register size {} and value size {}", info.size, sizeof(v));
            std::terminate();
        }
    }, val);

    if (info.type == register_type::Fpr)
    {
        proc_->write_fprs(data_.i387); 
    }
    else
    {
        // Apparently PTRACE_PEEKUSER and PTRACE_POKEUSER need 8 byte aligned addresses
        auto aligned_offset = info.offset & ~0b111;
        proc_->write_user_area(aligned_offset, from_bytes<std::uint64_t>(bytes + aligned_offset));
    }
}
