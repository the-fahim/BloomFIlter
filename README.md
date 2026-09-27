# A BloomFilter

A high-performance, thread-safe Bloom filter implementation in **C++23**, featuring:

- **Lock-free** insert & lookup via `std::atomic<uint64_t>` bit arrays
- **Singleton pattern** with `std::call_once` — safe concurrent initialization
- **Custom XXH3 hashing** — three implementations (system, Boost.Hash2, portable C++23)
- **Concurrent stress testing** using `std::jthread` + `std::latch`
- **Optimal sizing helper** — computes `(m, k)` from target false-positive rate

---

## Project Structure

```
BloomFilter/
├── include/
│   ├── bloom_filter.h       # BloomFilter class + BloomParams sizing helper
│   ├── hash_functions.h     # XXH3Hasher, BoostXXHasher, XXH3Lite
│   ├── benchmark.h          # Timing utility
│   └── murmur3.h            # Legacy header (unused, kept for reference)
├── src/
│   ├── bloom_filter.cpp     # Core implementation
│   ├── hash_function.cpp    # Hash TU anchor
│   ├── threading.cpp        # Concurrent stress test
│   ├── murmur3.cpp          # Stub (not used)
│   └── main.cpp             # Demo + benchmarks
├── CMakeLists.txt
├── AGENT.md
└── README.md
```

---

## Prerequisites

| Tool | Min Version | Install (Arch Linux) |
|------|-------------|----------------------|
| `clang++` | 16+ (C++23) | `sudo pacman -S clang` |
| `cmake` | 3.25+ | `sudo pacman -S cmake` |
| `ninja` | any | `sudo pacman -S ninja` |
| `boost` | 1.70+ | `sudo pacman -S boost` |


All dependencies are pre-installed on this machine.

---

## Build

### Release (optimised — recommended)

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release

cmake --build build --parallel
```

### Debug (with AddressSanitizer + ThreadSanitizer)

```bash
cmake -S . -B build-debug -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug

cmake --build build-debug --parallel
```

### Clean rebuild

```bash
rm -rf build && cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

---

## Running

```bash
./build/bloom_filter
```

Expected output:

```
  ██████╗ ██╗      ██████╗  ██████╗ ███╗   ███╗
  ...
  FILTER  — Thread-Safe · XXH3 · C++23

╔══════════════════════════════════════════════╗
║   Hash Function Benchmark (500000 keys)      ║
╚══════════════════════════════════════════════╝
  XXH3 (system libxxhash)  :     0.00 ms
  Boost.Hash2 xxhash_64    :     5.81 ms
  XXH3Lite (C++23 manual)  :     0.00 ms

╔══════════════════════════════════════════════╗
║   False-Positive Rate Test                   ║
╚══════════════════════════════════════════════╝
  False negatives : 0  (must be 0)
  False positives : 972 / 100000 = 0.972%  (target ≤ 1.0%)

  Insert : 60.02 ms  (16.66 M ops/sec)
  Lookup : 49.97 ms  (20.01 M ops/sec)

  PASSED: zero false-negatives — filter is correct.
```

---

## Design

### Double-Hashing

Rather than using `k` independent hash functions, the filter derives all `k` bit
positions from just **two base hashes** (Kirsch–Mitzenmacher technique):

```
hash_i(key, i) = (hash_a(key) + i × hash_b(key)) % m_bits
```

`hash_b` is forced **odd** so it is co-prime with any power-of-2 `m`, guaranteeing full coverage.

### Lock-Free Bit Array

The bit array is a `std::vector<std::unique_ptr<std::atomic<uint64_t>>>`.
Each 64-bit word covers 64 bloom bits:

```cpp
// Insert: atomic OR
bit_array_[word]->fetch_or(1ULL << bit, std::memory_order_relaxed);

// Lookup: atomic load
(bit_array_[word]->load(std::memory_order_relaxed) >> bit) & 1ULL
```

No mutex is held on the hot path — all operations are lock-free.

### Optimal Sizing

```cpp
// n = expected insertions, fp_rate = target false-positive probability
auto [m, k] = bf::BloomParams::optimal(100'000, 0.01);  // 1% FP
auto& filter = bf::BloomFilter::getInstance(m, k);
```

### Hash Implementations

| Class | Description |
|-------|-------------|
| `bf::XXH3Hasher` | Uses system `libxxhash` — fastest, SIMD-vectorised |
| `bf::BoostXXHasher` | Uses `boost::hash2::xxhash_64` + `xxh3_128` — header-only |
| `bf::XXH3Lite` | Hand-rolled C++23 portable implementation of XXH3 core algorithm |

---

## Benchmarks

Measured on Release build (`clang++ -O3 -march=native`):

| Operation | Time (1M keys) | Throughput |
|-----------|---------------|------------|
| Insert    | ~60 ms        | 16.7 M ops/sec |
| Lookup    | ~50 ms        | 20.0 M ops/sec |
| FP rate   | —             | 0.97% (target ≤ 1%) |

### Thread-Safety Test

4 writer threads × 4 reader threads, 100K total keys:

```
False negatives : 0       ← must always be 0
Throughput      : ~4.5 M ops/sec (concurrent)
```

---

## References

- [C++23 Reference — cppreference.com](https://cppreference.com/cpp/23)
- [XXH3 Algorithm — Cyan4973/xxHash](https://github.com/Cyan4973/xxHash)
- [Boost.Hash2 — boostorg/hash2](https://github.com/boostorg/hash2)
- [Kirsch–Mitzenmacher Double Hashing](https://www.eecs.harvard.edu/~michaelm/postscripts/tr-02-05.pdf)

## Hornorable Mentions
- [Video 1](https://www.youtube.com/watch?v=qL8HSt7NWT0&t=447s)
- [Video 2](https://www.youtube.com/watch?v=Teb32XBUNhc&t=2476s)
- [Video 3](https://www.youtube.com/watch?v=PK-eED8FSJM&t=9s)
- [Video 4](https://www.youtube.com/watch?v=n-S9DBwPGTo&t=742s)

- [Video 5](https://www.youtube.com/watch?v=Bay3X9PAX5k&t=428s)
- [Video 6](https://www.youtube.com/watch?v=RSwjdlTp108&pp=ygUNYmxvb20gZmlsdGVycw%3D%3D)
- [Video 7](https://www.youtube.com/watch?v=gn8d7z7itas)


