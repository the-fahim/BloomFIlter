#pragma once

// ============================================================================
//  bloom_filter.h
//  Thread-safe, high-performance Bloom Filter — C++23
//
//  Design:
//   • Singleton pattern  (double-checked locking with std::call_once)
//   • Bit-array backed by std::vector<std::atomic<uint64_t>>
//     → lock-free Insert / Contains via fetch_or / load
//   • Double-hashing: hash_i = (hash_a + i * hash_b) % m_bits
//   • Parameterisable false-positive rate helper (optimal_params)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <string_view>
#include <span>
#include <format>          // C++23
#include <print>           // C++23
#include <stdexcept>

namespace bf {

// ----------------------------------------------------------------------------
//  Optimal Bloom filter sizing helper
// ----------------------------------------------------------------------------
struct BloomParams {
    size_t m_bits;   // total bit count
    size_t k_hashes; // number of hash functions

    /// Compute optimal (m, k) given expected insertions n and false-positive p
    [[nodiscard]]
    static constexpr BloomParams optimal(size_t n, double fp_rate = 0.01) {
        const double ln2 = 0.693147180559945;
        double m = -static_cast<double>(n) * std::log(fp_rate) / (ln2 * ln2);
        double k =  (m / static_cast<double>(n)) * ln2;
        return { static_cast<size_t>(m) + 1, std::max(size_t{1}, static_cast<size_t>(k)) };
    }
};

// ============================================================================
//  BloomFilter
// ============================================================================
class BloomFilter {
public:
    // ------------------------------------------------------------------
    //  Singleton access
    //  First call wins: subsequent calls ignore m_bits / k_hashes.
    // ------------------------------------------------------------------
    static BloomFilter& getInstance(
        size_t m_bits   = 1 << 20,   // ~1 M bits by default
        size_t k_hashes = 7
    );

    // Convenience: create via BloomParams
    static BloomFilter& getInstance(BloomParams p) {
        return getInstance(p.m_bits, p.k_hashes);
    }

    // ------------------------------------------------------------------
    //  Core operations  (thread-safe, lock-free)
    // ------------------------------------------------------------------
    void insert(std::string_view s) noexcept;
    [[nodiscard]] bool contains(std::string_view s) const noexcept;

    // ------------------------------------------------------------------
    //  Diagnostics
    // ------------------------------------------------------------------
    [[nodiscard]] size_t bit_count()   const noexcept { return m_bits_;   }
    [[nodiscard]] size_t hash_count()  const noexcept { return k_hashes_; }
    [[nodiscard]] double fill_ratio()  const noexcept;
    [[nodiscard]] size_t bits_set()    const noexcept;

    // ------------------------------------------------------------------
    //  Non-copyable / non-movable (singleton semantics)
    // ------------------------------------------------------------------
    BloomFilter(const BloomFilter&)            = delete;
    BloomFilter& operator=(const BloomFilter&) = delete;
    BloomFilter(BloomFilter&&)                 = delete;
    BloomFilter& operator=(BloomFilter&&)      = delete;

private:
    // Private constructor
    BloomFilter(size_t m_bits, size_t k_hashes);

    // Double-hash: returns bit index for round i
    [[nodiscard]] size_t hash_i(std::string_view s, size_t i) const noexcept;

    // Bit-level helpers
    void   set_bit(size_t idx) noexcept;
    [[nodiscard]] bool test_bit(size_t idx) const noexcept;

    // ---- data members ------------------------------------------------
    const size_t m_bits_;
    const size_t k_hashes_;

    // Each element is 64 bits; each bit corresponds to one bloom bit.
    // unique_ptr<atomic> → stable addresses when vector reallocates.
    std::vector<std::unique_ptr<std::atomic<uint64_t>>> bit_array_;

    // Singleton infrastructure
    static std::unique_ptr<BloomFilter> instance_;
    static std::once_flag               init_flag_;
};

} // namespace bf