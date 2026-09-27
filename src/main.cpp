// ============================================================================
//  main.cpp
//  Bloom Filter demo + benchmark — C++23
// ============================================================================

#include "bloom_filter.h"
#include "hash_functions.h"
#include "benchmark.h"

#include <print>     // C++23
#include <format>    // C++23
#include <chrono>
#include <random>
#include <string>
#include <vector>
#include <ranges>

// Declared in threading.cpp
namespace bf { void run_concurrent_stress_test(size_t, size_t, size_t); }

// ---------------------------------------------------------------------------
//  Hash benchmark: compare XXH3 system, Boost.Hash2, and XXH3Lite
// ---------------------------------------------------------------------------
static void benchmark_hashes(size_t n = 500'000) {
    std::print("╔══════════════════════════════════════════════╗\n");
    std::print("║   Hash Function Benchmark ({} keys)      ║\n", n);
    std::print("╚══════════════════════════════════════════════╝\n");

    Benchmark bench(n);
    bench.generate_keys(16);

    // System XXH3
    double t_xxh3 = bench.measure([](const std::string& s) {
        return bf::XXH3Hasher::hash_a(s);
    });
    std::print("  XXH3 (system libxxhash)  : {:>8.2f} ms\n", t_xxh3);

    // Boost.Hash2 xxhash_64
    double t_boost = bench.measure([](const std::string& s) {
        return bf::BoostXXHasher::hash_a(s);
    });
    std::print("  Boost.Hash2 xxhash_64    : {:>8.2f} ms\n", t_boost);

    // XXH3Lite (C++23 portable)
    double t_lite = bench.measure([](const std::string& s) {
        return bf::XXH3Lite::hash(s);
    });
    std::print("  XXH3Lite (C++23 manual)  : {:>8.2f} ms\n\n", t_lite);
}

// ---------------------------------------------------------------------------
//  False-positive rate test
// ---------------------------------------------------------------------------
static void false_positive_test() {
    std::print("╔══════════════════════════════════════════════╗\n");
    std::print("║   False-Positive Rate Test                   ║\n");
    std::print("╚══════════════════════════════════════════════╝\n");

    constexpr size_t N  = 100'000;
    constexpr double FP = 0.01;   // target 1%

    auto [m, k] = bf::BloomParams::optimal(N, FP);
    std::print("  Optimal params for n={}, fp={:.0f}%:\n", N, FP * 100);
    std::print("  m = {} bits ({:.1f} KB), k = {} hash functions\n\n",
               m, static_cast<double>(m) / 8192.0, k);

    // Use a fresh bloom filter (we reset via a local instance, not singleton)
    // For demo: we use the same singleton but note it was already used above.
    // In a real app, you'd instantiate a new BloomFilter per use-case.
    bf::BloomFilter& bf_ref = bf::BloomFilter::getInstance(m, k);

    std::mt19937_64 rng(42);
    auto rand_str = [&](size_t len = 20) {
        static constexpr std::string_view ch =
            "abcdefghijklmnopqrstuvwxyz0123456789";
        std::uniform_int_distribution<size_t> d(0, ch.size()-1);
        std::string s(len, '\0');
        for (auto& c : s) c = ch[d(rng)];
        return s;
    };

    // Insert N keys
    std::vector<std::string> inserted;
    inserted.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        inserted.push_back(rand_str());
        bf_ref.insert(inserted.back());
    }

    // Verify zero false-negatives
    size_t fn = 0;
    for (const auto& s : inserted) {
        if (!bf_ref.contains(s)) ++fn;
    }

    // Probe N keys NOT inserted
    size_t fp_count = 0;
    for (size_t i = 0; i < N; ++i) {
        std::string probe = rand_str(22); // different length → very unlikely collision
        if (bf_ref.contains(probe)) ++fp_count;
    }

    double fp_actual = static_cast<double>(fp_count) / static_cast<double>(N);

    std::print("  False negatives : {}  (must be 0)\n", fn);
    std::print("  False positives : {} / {} = {:.3f}%  (target ≤ {:.1f}%)\n\n",
               fp_count, N, fp_actual * 100.0, FP * 100.0);
    std::print("  Fill ratio      : {:.2f}%\n\n", bf_ref.fill_ratio() * 100.0);
}

// ---------------------------------------------------------------------------
//  Bloom filter throughput benchmark
// ---------------------------------------------------------------------------
static void throughput_benchmark(size_t n = 1'000'000) {
    std::print("╔══════════════════════════════════════════════╗\n");
    std::print("║   Bloom Filter Throughput Benchmark          ║\n");
    std::print("╚══════════════════════════════════════════════╝\n");

    auto& bf_ref = bf::BloomFilter::getInstance();

    Benchmark bench(n);
    bench.generate_keys(12);

    double t_insert = bench.measure([&](const std::string& s) {
        bf_ref.insert(s);
    });
    double t_lookup = bench.measure([&](const std::string& s) {
        (void)bf_ref.contains(s);
    });

    auto throughput = [&](double ms) {
        return static_cast<double>(n) / (ms / 1000.0) / 1'000'000.0;
    };

    std::print("  Insert : {:>8.2f} ms  ({:.2f} M ops/sec)\n",
               t_insert, throughput(t_insert));
    std::print("  Lookup : {:>8.2f} ms  ({:.2f} M ops/sec)\n\n",
               t_lookup, throughput(t_lookup));
}

// ---------------------------------------------------------------------------
//  main
// ---------------------------------------------------------------------------
int main() {
    std::print("\n");
    std::print("  ██████╗ ██╗      ██████╗  ██████╗ ███╗   ███╗\n");
    std::print("  ██╔══██╗██║     ██╔═══██╗██╔═══██╗████╗ ████║\n");
    std::print("  ██████╔╝██║     ██║   ██║██║   ██║██╔████╔██║\n");
    std::print("  ██╔══██╗██║     ██║   ██║██║   ██║██║╚██╔╝██║\n");
    std::print("  ██████╔╝███████╗╚██████╔╝╚██████╔╝██║ ╚═╝ ██║\n");
    std::print("  ╚═════╝ ╚══════╝ ╚═════╝  ╚═════╝ ╚═╝     ╚═╝\n");
    std::print("  FILTER  — Thread-Safe · XXH3 · C++23\n\n");

    benchmark_hashes();
    false_positive_test();
    throughput_benchmark();
    bf::run_concurrent_stress_test(4, 4, 25'000);

    std::print("  Done. \n\n");
    return 0;
}
