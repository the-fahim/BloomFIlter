#pragma once

// ============================================================================
//  hash_functions.h
//  Custom XXH3-based dual hash for Bloom filter – C++23
//
//  Strategy (double-hashing):
//    hash_i(key, i) = (hash_a + i * hash_b) % m
//
//  hash_a → XXH3_64bits with SEED_A
//  hash_b → XXH3_64bits with seed 1 (odd-forced so it's co-prime with any m)
//
//  Implementation uses the system libxxhash (XXH3 API) for maximum speed
//  AND provides a header-only C++23 portable fallback via Boost.Hash2.
// ============================================================================

#include <cstdint>
#include <string>
#include <string_view>
#include <span>
#include <bit>                     // C++23: std::byteswap, std::rotl
#include <array>

// ---------------------------------------------------------------------------
//  System XXH3 (fast path) – link with -lxxhash
// ---------------------------------------------------------------------------
#include <xxhash.h>

// ---------------------------------------------------------------------------
//  Boost.Hash2 (header-only portable path)
//  xxhash_64  → streaming 64-bit xxhash
//  xxh3_128   → full XXH3 128-bit implementation
// ---------------------------------------------------------------------------
#include <boost/hash2/xxhash.hpp>
#include <boost/hash2/xxh3.hpp>

namespace bf {

// ============================================================================
//  XXH3Hasher  – primary hasher used by the Bloom filter
// ============================================================================
class XXH3Hasher {
public:
    // --- seed constants (intentionally different) ---------------------------
    static constexpr uint64_t SEED_A = 0x9E3779B97F4A7C15ULL;  // golden ratio
    static constexpr uint64_t SEED_B = 0x6C62272E07BB0142ULL;  // fnv prime

    // hash_a: 64-bit XXH3 with SEED_A
    [[nodiscard]]
    static uint64_t hash_a(std::string_view s) noexcept {
        return XXH3_64bits_withSeed(s.data(), s.size(), SEED_A);
    }

    // hash_b: 64-bit XXH3 with SEED_B – forced odd so it's co-prime with 2^k
    [[nodiscard]]
    static uint64_t hash_b(std::string_view s) noexcept {
        uint64_t h = XXH3_64bits_withSeed(s.data(), s.size(), SEED_B);
        return h | 1ULL;   // ensure odd → co-prime with any power-of-2 m
    }

    // 128-bit variant (returns low64 + high64 packed)
    [[nodiscard]]
    static XXH128_hash_t hash128(std::string_view s, uint64_t seed = 0) noexcept {
        return XXH3_128bits_withSeed(s.data(), s.size(), seed);
    }
};

// ============================================================================
//  BoostXXHasher  – portable Boost.Hash2 based implementation (header-only)
//  Uses xxhash_64 from <boost/hash2/xxhash.hpp> via update()+result() API
// ============================================================================
class BoostXXHasher {
public:
    static constexpr uint64_t SEED_A = 0xDEADBEEFCAFEBABEULL;
    static constexpr uint64_t SEED_B = 0xBAADF00D8BADF00DULL;

    [[nodiscard]]
    static uint64_t hash_a(std::string_view s) noexcept {
        boost::hash2::xxhash_64 h(SEED_A);
        h.update(s.data(), s.size());
        // xxhash_64::result_type is uint64_t
        return static_cast<uint64_t>(h.result());
    }

    [[nodiscard]]
    static uint64_t hash_b(std::string_view s) noexcept {
        boost::hash2::xxhash_64 h(SEED_B);
        h.update(s.data(), s.size());
        return static_cast<uint64_t>(h.result()) | 1ULL;
    }

    // 128-bit via Boost's xxh3_128 (full XXH3)
    [[nodiscard]]
    static std::pair<uint64_t, uint64_t> hash128_a(std::string_view s) noexcept {
        boost::hash2::xxh3_128 h(SEED_A);
        h.update(s.data(), s.size());
        auto digest = h.result();  // digest<16>
        uint64_t lo, hi;
        __builtin_memcpy(&lo, digest.data(),   8);
        __builtin_memcpy(&hi, digest.data()+8, 8);
        return {lo, hi};
    }
};

// ============================================================================
//  Portable C++23 hand-rolled XXH3-lite
//  (Self-contained for demonstration / no external dep required)
//
//  Follows the XXH3 algorithm structure:
//    – short-input path (≤ 16 bytes)
//    – long-input path uses accumulation + scrambling
//  This is a simplified teaching implementation, not the full vectorised one.
// ============================================================================
class XXH3Lite {
    // --- XXH3 internal constants (from xxhash.h spec) ----------------------
    static constexpr uint64_t PRIME64_1 = 0x9E3779B185EBCA87ULL;
    static constexpr uint64_t PRIME64_2 = 0xC2B2AE3D27D4EB4FULL;
    static constexpr uint64_t PRIME64_3 = 0x165667B19E3779F9ULL;
    static constexpr uint64_t PRIME64_4 = 0x85EBCA77C2B2AE63ULL;
    static constexpr uint64_t PRIME64_5 = 0x27D4EB2F165667C5ULL;

    static constexpr uint64_t SECRET_WORD = 0xBE4BA423396CFEB8ULL;

    [[nodiscard]]
    static constexpr uint64_t avalanche(uint64_t h) noexcept {
        h ^= h >> 37;
        h *= PRIME64_3;
        h ^= h >> 32;
        return h;
    }

    [[nodiscard]]
    static constexpr uint64_t mix16(const uint8_t* p, uint64_t seed) noexcept {
        uint64_t lo, hi;
        __builtin_memcpy(&lo, p,   8);
        __builtin_memcpy(&hi, p+8, 8);
        return avalanche(lo ^ (hi + seed + SECRET_WORD));
    }

public:
    [[nodiscard]]
    static uint64_t hash(std::string_view s, uint64_t seed = 0) noexcept {
        const auto* data = reinterpret_cast<const uint8_t*>(s.data());
        const size_t len  = s.size();

        uint64_t h = seed + PRIME64_5 + len;

        size_t i = 0;
        // Process 16-byte stripes
        for (; i + 16 <= len; i += 16) {
            h ^= mix16(data + i, seed);
            h = std::rotl(h, 27) * PRIME64_1 + PRIME64_4;
        }
        // Remaining 8 bytes
        if (i + 8 <= len) {
            uint64_t v;
            __builtin_memcpy(&v, data + i, 8);
            h ^= avalanche(v ^ (seed + PRIME64_2));
            h = std::rotl(h, 29) * PRIME64_2 + PRIME64_3;
            i += 8;
        }
        // Remaining 4 bytes
        if (i + 4 <= len) {
            uint32_t v;
            __builtin_memcpy(&v, data + i, 4);
            h ^= static_cast<uint64_t>(v) * PRIME64_1;
            h = std::rotl(h, 23) * PRIME64_2 + PRIME64_3;
            i += 4;
        }
        // Remaining bytes
        while (i < len) {
            h ^= static_cast<uint64_t>(data[i]) * PRIME64_5;
            h = std::rotl(h, 11) * PRIME64_1;
            ++i;
        }
        return avalanche(h);
    }
};

// ============================================================================
//  Convenience alias: the hasher the BloomFilter actually uses
// ============================================================================
using Hasher = XXH3Hasher;

} // namespace bf