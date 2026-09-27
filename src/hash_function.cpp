// ============================================================================
//  hash_function.cpp
//  Translation-unit anchor for the hash_functions.h header.
//
//  All three hasher implementations (XXH3Hasher, BoostXXHasher, XXH3Lite)
//  are fully defined in the header (static member functions / constexpr).
//  This .cpp file exists solely so CMake has a real translation unit for
//  the BloomFilterLib static library build.
// ============================================================================

#include "hash_functions.h"

// Force emission of XXH3Lite::hash symbols in this translation unit.
// (Without the explicit address-take the optimiser might inline everything.)
namespace bf {
    [[maybe_unused]] static const auto _anchor_lite  = &XXH3Lite::hash;
    [[maybe_unused]] static const auto _anchor_a     = &XXH3Hasher::hash_a;
    [[maybe_unused]] static const auto _anchor_b     = &XXH3Hasher::hash_b;
} // namespace bf
