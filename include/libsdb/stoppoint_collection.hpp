#pragma once

#include <libsdb/error.hpp>
#include <libsdb/types.hpp>

#include <algorithm>
#include <memory>
#include <vector>

namespace sdb
{
template <typename Stoppoint>
class stoppoint_collection
{
public:
    Stoppoint& push(std::unique_ptr<Stoppoint> stoppoint);

    bool contains_id(typename Stoppoint::id_type id) const;
    bool contains_address(virtual_address address) const;
    bool enabled_stoppoint_at_address(virtual_address address) const;

    Stoppoint& get_by_id(typename Stoppoint::id_type id);
    const Stoppoint& get_by_id(typename Stoppoint::id_type id) const;
    Stoppoint& get_by_address(virtual_address address);
    const Stoppoint& get_by_address(virtual_address address) const;

    std::vector<Stoppoint*> get_in_region(virtual_address low, virtual_address high) const;

    void remove_by_id(typename Stoppoint::id_type id);
    void remove_by_address(virtual_address address);

    template <typename F>
    void for_each(F f);
    template <typename F>
    void for_each(F f) const;

    std::size_t size() const { return stoppoints_.size(); }
    bool empty() const { return stoppoints_.empty(); }
private:
    using points_t = std::vector<std::unique_ptr<Stoppoint>>;

    typename points_t::iterator find_by_id(typename Stoppoint::id_type id);
    typename points_t::const_iterator find_by_id(typename Stoppoint::id_type id) const; 
    typename points_t::iterator find_by_address(virtual_address address);
    typename points_t::const_iterator find_by_address(virtual_address address) const;

    points_t stoppoints_;
};

template <typename Stoppoint>
Stoppoint& stoppoint_collection<Stoppoint>::push(std::unique_ptr<Stoppoint> stoppoint)
{
    stoppoints_.push_back(std::move(stoppoint));
    return *stoppoints_.back();
}

template <typename Stoppoint>
auto stoppoint_collection<Stoppoint>::find_by_id(typename Stoppoint::id_type id) -> typename points_t::iterator
{
    return std::find_if(begin(stoppoints_), end(stoppoints_), [=](auto& point) { return point->id() == id; });
}

template <typename Stoppoint>
auto stoppoint_collection<Stoppoint>::find_by_id(typename Stoppoint::id_type id) const -> typename points_t::const_iterator
{
    return const_cast<stoppoint_collection*>(this)->find_by_id(id);
}

template <typename Stoppoint>
auto stoppoint_collection<Stoppoint>::find_by_address(virtual_address address)
        -> typename points_t::iterator
{
    return std::find_if(begin(stoppoints_), end(stoppoints_), [=](auto& point) { return point->at_address(address); });
}

template <typename Stoppoint>
auto stoppoint_collection<Stoppoint>::find_by_address(virtual_address address) const
        -> typename points_t::const_iterator
{
    return const_cast<stoppoint_collection*>(this)->find_by_address(address);
}

template <typename Stoppoint>
bool stoppoint_collection<Stoppoint>::contains_id(typename Stoppoint::id_type id) const
{
    return find_by_id(id) != end(stoppoints_);
}

template <typename Stoppoint>
bool stoppoint_collection<Stoppoint>::contains_address(virtual_address address) const
{
    return find_by_address(address) != end(stoppoints_);
}

template <typename Stoppoint>
bool stoppoint_collection<Stoppoint>::enabled_stoppoint_at_address(
virtual_address address) const
{
    return contains_address(address) && get_by_address(address).is_enabled();
}

template <typename Stoppoint>
Stoppoint& stoppoint_collection<Stoppoint>::get_by_id(typename Stoppoint::id_type id)
{
    auto it = find_by_id(id);
    if (it == end(stoppoints_))
    {
        error::send("Invalid stoppoint id");
    }

    return **it; // Iterator to unique pointer to Stoppoint
}

template <typename Stoppoint>
const Stoppoint& stoppoint_collection<Stoppoint>::get_by_id(typename Stoppoint::id_type id) const
{
    return const_cast<stoppoint_collection*>(this)->get_by_id(id);  
}

template <typename Stoppoint>
Stoppoint& stoppoint_collection<Stoppoint>::get_by_address(virtual_address address)
{
    auto it = find_by_address(address);
    if (it == end(stoppoints_))
    {
        error::send(std::format("Stoppoint not found at address {}", address.addr()));
    }

    return **it;
}

template <typename Stoppoint>
const Stoppoint& stoppoint_collection<Stoppoint>::get_by_address(virtual_address address) const
{
    return const_cast<stoppoint_collection*>(this)->get_by_address(address);  
}

template <typename Stoppoint>
std::vector<Stoppoint*> stoppoint_collection<Stoppoint>::get_in_region(virtual_address low, virtual_address high) const
{
    std::vector<Stoppoint*> result{};
    for (auto& site : stoppoints_)
    {
        if (site->in_range(low, high))
        {
            result.push_back(&*site);
        }
    }

    return result;
}

template <typename Stoppoint>
void stoppoint_collection<Stoppoint>::remove_by_id(typename Stoppoint::id_type id)
{
    auto it = find_by_id(id);
    (**it).disable();
    stoppoints_.erase(it);
}
template <typename Stoppoint>
void stoppoint_collection<Stoppoint>::remove_by_address(virtual_address address)
{
    auto it = find_by_address(address);
    (**it).disable();
    stoppoints_.erase(it);
}

template <typename Stoppoint>
template <typename F>
void stoppoint_collection<Stoppoint>::for_each(F func)
{
    for (auto& point: stoppoints_)
    {
        func(*point);
    }
}

template <typename Stoppoint>
template <typename F>
void stoppoint_collection<Stoppoint>::for_each(F func) const
{
    for (const auto& point: stoppoints_)
    {
        func(*point);
    }
}



}
