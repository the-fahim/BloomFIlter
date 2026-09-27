#pragma once
#include <chrono>
#include <vector>
#include <string>
#include <functional>
#include <random>
#include <iostream>

class Benchmark {
public:
    Benchmark(size_t test_set_size)
        : test_set(test_set_size) {
        keys.reserve(test_set_size);
    }

    // Generate random strings for testing
    void generate_keys(size_t length = 10) {
        static const char charset[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);

        for (size_t i = 0; i < test_set; i++) {
            std::string key;
            key.reserve(length);
            for (size_t j = 0; j < length; j++) {
                key += charset[dist(rng)];
            }
            keys.push_back(std::move(key));
        }
    }

    // Benchmark any function that takes a const std::string&
    template<typename Func>
    double measure(Func f) {
        auto start = std::chrono::high_resolution_clock::now();
        for (auto &key : keys) {
            f(key);
        }
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> elapsed = end - start;
        return elapsed.count(); // in ms
    }

    // Get the keys
    const std::vector<std::string>& get_keys() const {
        return keys;
    }

private:
    size_t test_set;
    std::vector<std::string> keys;
};