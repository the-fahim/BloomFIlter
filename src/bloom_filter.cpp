// ============================================================================
//  bloom_filter.cpp
//  Thread-safe Bloom Filter implementation — C++23
// ============================================================================

#include "bloom_filter.h"
#include "hash_functions.h"

#include <cassert>
#include <bit>          // std::popcount

namespace bf {

// ---- singleton storage -----------------------------------------------------
std::unique_ptr<BloomFilter> BloomFilter::instance_;
std::once_flag               BloomFilter::init_flag_;

// ============================================================================
//  Constructor
// ============================================================================
BloomFilter::BloomFilter(size_t m_bits, size_t k_hashes)
    : m_bits_(m_bits)
    , k_hashes_(k_hashes)
{
    if (m_bits_ == 0)  throw std::invalid_argument("m_bits must be > 0");
    if (k_hashes_ == 0) throw std::invalid_argument("k_hashes must be > 0");

    const size_t num_words = (m_bits_ + 63) / 64;
    bit_array_.reserve(num_words);
    for (size_t i = 0; i < num_words; ++i) {
        bit_array_.emplace_back(std::make_unique<std::atomic<uint64_t>>(0));
    }
}

// ============================================================================
//  Singleton accessor
// ============================================================================
BloomFilter& BloomFilter::getInstance(size_t m_bits, size_t k_hashes) {
    std::call_once(init_flag_, [&] {
        instance_ = std::unique_ptr<BloomFilter>(new BloomFilter(m_bits, k_hashes));
    });
    return *instance_;
}

// ============================================================================
//  Double-hash function
//    hash_i(s, i) = (hash_a(s) + i * hash_b(s)) % m_bits_
// ============================================================================
size_t BloomFilter::hash_i(std::string_view s, size_t i) const noexcept {
    const uint64_t ha = Hasher::hash_a(s);
    const uint64_t hb = Hasher::hash_b(s);
    return (ha + i * hb) % m_bits_;
}

// ============================================================================
//  Bit helpers
// ============================================================================
void BloomFilter::set_bit(size_t idx) noexcept {
    const size_t word = idx >> 6;          // idx / 64
    const size_t bit  = idx & 63u;         // idx % 64
    bit_array_[word]->fetch_or(1ULL << bit, std::memory_order_relaxed);
}

bool BloomFilter::test_bit(size_t idx) const noexcept {
    const size_t word = idx >> 6;
    const size_t bit  = idx & 63u;
    return (bit_array_[word]->load(std::memory_order_relaxed) >> bit) & 1ULL;
}

// ============================================================================
//  insert  — lock-free via atomic fetch_or
// ============================================================================
void BloomFilter::insert(std::string_view s) noexcept {
    for (size_t i = 0; i < k_hashes_; ++i) {
        set_bit(hash_i(s, i));
    }
}

// ============================================================================
//  contains  — lock-free via atomic load
// ============================================================================
bool BloomFilter::contains(std::string_view s) const noexcept {
    for (size_t i = 0; i < k_hashes_; ++i) {
        if (!test_bit(hash_i(s, i))) return false;
    }
    return true;   // probable member (may be false-positive)
}

// ============================================================================
//  Diagnostics
// ============================================================================
size_t BloomFilter::bits_set() const noexcept {
    size_t count = 0;
    for (const auto& word : bit_array_) {
        count += static_cast<size_t>(
            std::popcount(word->load(std::memory_order_relaxed))
        );
    }
    return count;
}

double BloomFilter::fill_ratio() const noexcept {
    return static_cast<double>(bits_set()) / static_cast<double>(m_bits_);
}

} // namespace bf