#pragma once

#include <libsdb/types.hpp>

#include <cstring>
#include <cstddef>

namespace sdb
{
template <typename To>
To from_bytes(const std::byte* bytes)
{
    To result;
    std::memcpy(&result, bytes, sizeof(To));
    return result;
}

template <typename From>
std::byte* as_bytes(From& from)
{
    return reinterpret_cast<std::byte*>(&from);
}

template <typename From>
const std::byte* as_bytes(const From& from)
{
    return reinterpret_cast<const std::byte*>(&from);
}

template <typename From>
byte128 to_byte128(From src)
{
    byte128 result{};
    std::memcpy(&result, &src, sizeof(From));
    return result;
}

template <typename From>
byte64 to_byte64(From src)
{
    byte64 result{};
    std::memcpy(&result, &src, sizeof(From));
    return result;
}
}

