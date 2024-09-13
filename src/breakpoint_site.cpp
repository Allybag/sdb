#include <libsdb/breakpoint_site.hpp>

namespace
{
auto get_next_id() {
    static sdb::breakpoint_site::id_type id = 0;
    return ++id;
}
}

sdb::breakpoint_site::breakpoint_site(process& proc, virtual_address address) : process_{&proc}, address_{address}, is_enabled_{false}, saved_data_{} {
        id_ = get_next_id();
}

