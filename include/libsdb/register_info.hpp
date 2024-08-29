#pragma once

#include <sys/user.h>
#include <cstdint>
#include <cstddef>
#include <string_view>

namespace sdb
{
enum class register_id
{
    #define DEFINE_REGISTER(name,dwarf_id,size,offset,type,format) name
    #include <libsdb/detail/registers.inc>
    #undef DEFINE_REGISTER
};

enum class register_type
{
    Gpr,    // General Purpose Register
    SubGpr, // Subregister of a General Purpose Register
    Fpr,    // Floating point register
    Dr      // Debug register?
};

enum class register_format
{
    UnsignedInt,
    DoubleFloat,
    LongDouble,
    Vector
};

struct register_info
{
    register_id id;
    std::string_view name;
    std::int32_t dwarf_id;
    std::size_t size;
    std::size_t offset;
    register_type type;
    register_format format;
};

inline constexpr const register_info g_register_infos[] = {
    #define DEFINE_REGISTER(name,dwarf_id,size,offset,type,format) \
        { register_id::name, #name, dwarf_id, size, offset, type, format }
    #include <libsdb/detail/registers.inc>
    #undef DEFINE_REGISTER
};
}
