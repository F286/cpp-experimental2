#pragma once

#include "chunk_map.h"
#include "bucket_map_wrapper.h"

/// @brief chunk_map alias using bucket_map for inner storage.
template<typename T>
using layered_map = chunk_map<T, bucket_map_wrapper<LocalPosition, T>>;

