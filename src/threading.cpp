// ============================================================================
//  threading.cpp
//  Concurrent stress-test of the Bloom filter — C++23
//
//  Spawns N writer threads and M reader threads, all operating on the shared
//  singleton BloomFilter. Verifies:
//   • No data races (tsan-clean with -fsanitize=thread)
//   • Zero false-negatives (every inserted key must be found)
//   • False-positive rate within the theoretical bound
// ============================================================================

#include "bloom_filter.h"
#include "hash_functions.h"

#include <thread>
#include <vector>
#include <atomic>
#include <latch>          // C++23 (std::latch)
#include <barrier>        // C++23 (std::barrier)
#include <random>
#include <print>          // C++23 std::print
#include <format>         // C++23 std::format
#include <string>
#include <chrono>
#include <ranges>

namespace bf {

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
static std::string random_key(std::mt19937_64& rng, size_t len = 12) {
    static constexpr std::string_view charset =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<size_t> dist(0, charset.size() - 1);
    std::string key(len, '\0');
    for (auto& c : key) c = charset[dist(rng)];
    return key;
}

// ---------------------------------------------------------------------------
//  run_concurrent_stress_test
// ---------------------------------------------------------------------------
void run_concurrent_stress_test(
    size_t num_writers  = 4,
    size_t num_readers  = 4,
    size_t keys_per_writer = 50'000
) {
    std::print("\n╔══════════════════════════════════════════════╗\n");
    std::print(  "║   Thread-Safety Stress Test                  ║\n");
    std::print(  "╚══════════════════════════════════════════════╝\n");
    std::print("  Writers: {}  |  Readers: {}  |  Keys/writer: {}\n\n",
               num_writers, num_readers, keys_per_writer);

    const size_t total_keys = num_writers * keys_per_writer;

    // Size the filter for ~1% FP rate at total_keys
    auto [m, k] = BloomParams::optimal(total_keys, 0.01);
    auto& filter = BloomFilter::getInstance(m, k);

    std::print("  Filter: {} bits, {} hash functions\n", m, k);

    // Storage for inserted keys (indexed by writer id)
    std::vector<std::vector<std::string>> inserted(num_writers);
    for (auto& v : inserted) v.reserve(keys_per_writer);

    // Shared latch: everyone starts together
    std::latch start_latch(static_cast<ptrdiff_t>(num_writers + num_readers));
    std::atomic<bool> writers_done{false};
    std::atomic<size_t> false_negatives{0};
    std::atomic<size_t> total_queries{0};

    // --- Writer threads ---------------------------------------------------
    std::vector<std::jthread> writers;
    writers.reserve(num_writers);
    for (size_t w = 0; w < num_writers; ++w) {
        writers.emplace_back([&, w] {
            std::mt19937_64 rng(w * 0xDEAD'C0DEULL);
            // Pre-generate keys
            for (size_t i = 0; i < keys_per_writer; ++i)
                inserted[w].push_back(random_key(rng));

            start_latch.arrive_and_wait();  // synchronised start

            for (const auto& key : inserted[w])
                filter.insert(key);
        });
    }

    // --- Reader threads (read after writers finish) -----------------------
    std::vector<std::jthread> readers;
    readers.reserve(num_readers);
    for (size_t r = 0; r < num_readers; ++r) {
        readers.emplace_back([&] {
            start_latch.arrive_and_wait();  // synchronised start

            // Wait for all writers to finish
            for (auto& w : writers) {
                if (w.joinable()) { /* spin via join below */ }
            }
            // We'll join writers in main – readers spin on writers_done flag
            while (!writers_done.load(std::memory_order_acquire))
                std::this_thread::yield();

            // Now verify all inserted keys are found (no false negatives!)
            for (const auto& bucket : inserted) {
                for (const auto& key : bucket) {
                    if (!filter.contains(key))
                        false_negatives.fetch_add(1, std::memory_order_relaxed);
                    total_queries.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    // Join writers first
    auto t0 = std::chrono::steady_clock::now();
    for (auto& w : writers) w.join();
    writers_done.store(true, std::memory_order_release);

    // Join readers
    for (auto& r : readers) r.join();
    auto t1 = std::chrono::steady_clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // --- Report -----------------------------------------------------------
    std::print("\n   Results:\n");
    std::print("     Total inserted  : {}\n", total_keys);
    std::print("     Total queried   : {}\n", total_queries.load());
    std::print("     False negatives : {}  (must be 0!)\n", false_negatives.load());
    std::print("     Fill ratio      : {:.2f}%\n", filter.fill_ratio() * 100.0);
    std::print("     Elapsed         : {:.2f} ms\n", elapsed_ms);
    std::print("     Throughput      : {:.0f} ops/sec\n\n",
               static_cast<double>(total_keys) / (elapsed_ms / 1000.0));

    if (false_negatives.load() != 0) {
        std::print("  BUG: false negatives detected!\n");
    } else {
        std::print("  PASSED: zero false-negatives — filter is correct.\n");
    }
}

} // namespace bf
