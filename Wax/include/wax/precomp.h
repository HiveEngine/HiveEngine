#pragma once

// Wax precompiled header

// C++ Standard Library (minimal usage)
#include <cstddef>      // size_t, std::byte
#include <cstdint>      // uint32_t, etc.
#include <cstring>      // std::memset
#include <utility>      // std::move, std::forward
#include <type_traits>  // std::is_same, etc.
#include <concepts>     // C++20 concepts
#include <initializer_list>

// Hive core
#include <hive/core/assert.h>

// Comb allocators (concepts only, not implementations)
// #include <comb/allocator_concept.h>
