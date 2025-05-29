#pragma once
#include "layered_map.h"
#include <map>

/// @brief Write elements common to both layered_maps to output iterator.
template<typename T, typename OutIt>
OutIt set_intersection(const layered_map<T>& lhs,
                       const layered_map<T>& rhs,
                       OutIt out)
{
    chunk_map<T, bucket_map<LocalPosition, T>>::for_each_intersection(
        lhs, rhs,
        [&](GlobalPosition gp, const T& val)
        {
            *out++ = std::pair{gp, val};
        });
    return out;
}
