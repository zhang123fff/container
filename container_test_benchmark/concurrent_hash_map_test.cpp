#include <benchmark/benchmark.h>
#include <thread>
#include <vector>
#include "C:\Users\86172\source\repos\高性能线程池-container测试\高性能线程池-container测试\concurrent_hash_map.hpp"
using namespace con;

// 1. 单线程插入
static void BM_HashMap_Insert_Single(benchmark::State& state) {
    const int N = state.range(0);
    for (auto _ : state) {
        concurrent_hash_map<int, int> map(128);
        for (int i = 0; i < N; ++i) {
            map.insert(i, i);
        }
        benchmark::DoNotOptimize(map);
    }
    state.SetComplexityN(N);
}
BENCHMARK(BM_HashMap_Insert_Single)->Range(1 << 10, 1 << 20)->Complexity();

// 2. 单线程查找
static void BM_HashMap_Find_Single(benchmark::State& state) {
    const int N = state.range(0);
    concurrent_hash_map<int, int> map(128);
    for (int i = 0; i < N; ++i) map.insert(i, i);
    for (auto _ : state) {
        for (int i = 0; i < N; ++i) {
            auto handle = map.read_find(i);
            benchmark::DoNotOptimize(handle);
        }
    }
    state.SetComplexityN(N);
}
BENCHMARK(BM_HashMap_Find_Single)->Range(1 << 10, 1 << 20)->Complexity();

// 3. 单线程删除
static void BM_HashMap_Erase_Single(benchmark::State& state) {
    const int N = state.range(0);
    for (auto _ : state) {
        concurrent_hash_map<int, int> map(128);
        for (int i = 0; i < N; ++i) map.insert(i, i);
        for (int i = 0; i < N; ++i) {
            map.erase(i);
        }
        benchmark::DoNotOptimize(map);
    }
    state.SetComplexityN(N);
}
BENCHMARK(BM_HashMap_Erase_Single)->Range(1 << 10, 1 << 18)->Complexity();

// 4. 8 线程并发插入
static void BM_HashMap_Insert_Concurrent(benchmark::State& state) {
    const int N = state.range(0);
    const int num_threads = 8;
    for (auto _ : state) {
        concurrent_hash_map<int, int> map(256);
        std::vector<std::thread> threads;
        const int per_thread = N / num_threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&map, t, per_thread]() {
                for (int j = 0; j < per_thread; ++j) {
                    int key = t * per_thread + j;
                    map.insert(key, key);
                }
                });
        }
        for (auto& th : threads) th.join();
        benchmark::DoNotOptimize(map);
    }
    state.SetComplexityN(N);
}
BENCHMARK(BM_HashMap_Insert_Concurrent)->Range(1 << 12, 1 << 20)->Complexity();

// 5. 8 读 2 写 混合
static void BM_HashMap_Mixed_ReadWrite(benchmark::State& state) {
    const int N = state.range(0);
    const int num_readers = 8;
    const int num_writers = 2;
    for (auto _ : state) {
        concurrent_hash_map<int, int> map(128);
        for (int i = 0; i < N; ++i) map.insert(i, i);
        std::vector<std::thread> threads;
        const int ops_per_thread = 1000;
        for (int t = 0; t < num_writers; ++t) {
            threads.emplace_back([&map, ops_per_thread, N]() {
                for (int j = 0; j < ops_per_thread; ++j) {
                    int key = j % N;
                    auto handle = map.write_find(key);
                    if (handle.has_value()) handle->get() = handle->get() + 1;
                }
                });
        }
        for (int t = 0; t < num_readers; ++t) {
            threads.emplace_back([&map, ops_per_thread, N]() {
                for (int j = 0; j < ops_per_thread; ++j) {
                    int key = j % N;
                    auto handle = map.read_find(key);
                    benchmark::DoNotOptimize(handle);
                }
                });
        }
        for (auto& th : threads) th.join();
    }
    state.SetComplexityN(N);
}
BENCHMARK(BM_HashMap_Mixed_ReadWrite)->Range(1 << 10, 1 << 18)->Complexity();

BENCHMARK_MAIN();