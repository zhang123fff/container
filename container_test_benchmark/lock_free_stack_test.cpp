#include <benchmark/benchmark.h>
#include <thread>
#include <vector>
#include<random>
#include "C:\Users\86172\source\repos\高性能线程池-container测试\高性能线程池-container测试\lock_free_stack.hpp"
using namespace con;

static int GetRange(benchmark::State& state) {
    return static_cast<int>(state.range(0));
}

// ==================== 单线程 Push ====================
static void BM_Stack_Push_Single(benchmark::State& state) {
    const int N = GetRange(state);
    for (auto _ : state) {
        lock_free_stack<int> stack;
        for (int i = 0; i < N; ++i) {
            int val = i;
            stack.push(std::move(val));
        }
        benchmark::DoNotOptimize(stack);
    }
    state.SetComplexityN(N);
}
BENCHMARK(BM_Stack_Push_Single)->Range(1 << 10, 1 << 20)->Complexity();

// ==================== 单线程 Pop ====================
static void BM_Stack_Pop_Single(benchmark::State& state) {
    const int N = GetRange(state);
    for (auto _ : state) {
        lock_free_stack<int> stack;
        for (int i = 0; i < N; ++i) {
            int val = i;
            stack.push(std::move(val));
        }
        for (int i = 0; i < N; ++i) {
            int val;
            stack.pop(val);
            benchmark::DoNotOptimize(val);
        }
        benchmark::DoNotOptimize(stack);
    }
    state.SetComplexityN(N);
}
BENCHMARK(BM_Stack_Pop_Single)->Range(1 << 10, 1 << 20)->Complexity();

// ==================== 单线程混合 50% ====================
static void BM_Stack_Mixed_Single(benchmark::State& state) {
    const int N = GetRange(state);
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> coin(0, 99);
    for (auto _ : state) {
        lock_free_stack<int> stack;
        // 预填充一半，减少空 pop
        for (int i = 0; i < N / 2; ++i) {
            int val = i;
            stack.push(std::move(val));
        }
        for (int i = 0; i < N; ++i) {
            if (coin(rng) < 50) {
                int val = i;
                stack.push(std::move(val));
            }
            else {
                int val;
                if (stack.pop(val))
                    benchmark::DoNotOptimize(val);
            }
        }
        benchmark::DoNotOptimize(stack);
    }
    state.SetComplexityN(N);
}
BENCHMARK(BM_Stack_Mixed_Single)->Range(1 << 10, 1 << 20)->Complexity();

// ==================== 多线程 Push（手动 8 线程） ====================
static void BM_Stack_Push_Concurrent_8(benchmark::State& state) {
    const int N = GetRange(state);
    const int num_threads = 8;
    for (auto _ : state) {
        lock_free_stack<int> stack;
        std::vector<std::thread> threads;
        const int per_thread = N / num_threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&stack, t, per_thread]() {
                for (int j = 0; j < per_thread; ++j) {
                    int key = t * per_thread + j;
                    stack.push(std::move(key));
                }
                });
        }
        for (auto& th : threads) th.join();
        benchmark::DoNotOptimize(stack);
    }
    state.SetComplexityN(N);
}
BENCHMARK(BM_Stack_Push_Concurrent_8)->Range(1 << 12, 1 << 20)->Complexity();

// ==================== 多线程 Pop（预填充，手动 8 线程） ====================
static void BM_Stack_Pop_Concurrent_8(benchmark::State& state) {
    const int N = GetRange(state);
    const int num_threads = 8;
    for (auto _ : state) {
        lock_free_stack<int> stack;
        for (int i = 0; i < N; ++i) {
            int val = i;
            stack.push(std::move(val));
        }
        std::vector<std::thread> threads;
        const int per_thread = N / num_threads;
        std::atomic<int> total_popped{ 0 };
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                for (int j = 0; j < per_thread; ++j) {
                    int val;
                    while (!stack.pop(val))
                        std::this_thread::yield();
                    total_popped.fetch_add(1, std::memory_order_relaxed);
                    benchmark::DoNotOptimize(val);
                }
                });
        }
        for (auto& th : threads) th.join();
        while (total_popped.load() < N) {
            int val;
            if (stack.pop(val))
                total_popped.fetch_add(1, std::memory_order_relaxed);
        }
        benchmark::DoNotOptimize(stack);
    }
    state.SetComplexityN(N);
}
BENCHMARK(BM_Stack_Pop_Concurrent_8)->Range(1 << 12, 1 << 20)->Complexity();

// ==================== 高竞争持续混合（手动 8 线程） ====================
static void BM_Stack_Mixed_Concurrent_8(benchmark::State& state) {
    const int ops_per_thread = GetRange(state);
    const int num_readers = 6;
    const int num_writers = 2;
    for (auto _ : state) {
        lock_free_stack<int> stack;
        const int initial_size = ops_per_thread / 2;
        for (int i = 0; i < initial_size; ++i) {
            int val = i;
            stack.push(std::move(val));
        }
        std::vector<std::thread> threads;
        for (int t = 0; t < num_writers; ++t) {
            threads.emplace_back([&stack, ops_per_thread]() {
                std::mt19937 rng(std::random_device{}());
                for (int j = 0; j < ops_per_thread; ++j) {
                    int val = static_cast<int>(rng());
                    stack.push(std::move(val));
                }
                });
        }
        for (int t = 0; t < num_readers; ++t) {
            threads.emplace_back([&stack, ops_per_thread]() {
                for (int j = 0; j < ops_per_thread; ++j) {
                    int val;
                    if (stack.pop(val))
                        benchmark::DoNotOptimize(val);
                }
                });
        }
        for (auto& th : threads) th.join();
        benchmark::DoNotOptimize(stack);
    }
    state.SetComplexityN(ops_per_thread * (num_readers + num_writers));
}
BENCHMARK(BM_Stack_Mixed_Concurrent_8)->Range(1 << 10, 1 << 16)->Complexity();

// ==================== 利用框架自动多线程的纯 Push ====================
static void BM_Stack_Push_AutoThreads(benchmark::State& state) {
    static lock_free_stack<int> stack;
    for (auto _ : state) {
        int val = static_cast<int>(state.iterations());
        stack.push(std::move(val));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Stack_Push_AutoThreads)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->UseRealTime();

// ==================== 利用框架自动多线程的纯 Pop ====================
static void BM_Stack_Pop_AutoThreads(benchmark::State& state) {
    static lock_free_stack<int> stack;
    static std::once_flag flag;
    std::call_once(flag, [] {
        for (int i = 0; i < 10'000'000; ++i) {
            int val = i;
            stack.push(std::move(val));
        }
        });

    for (auto _ : state) {
        int val;
        if (stack.pop(val)) {
            benchmark::DoNotOptimize(val);
        }
        else {
            state.PauseTiming();
            std::this_thread::yield();   // 避免长时间 sleep
            state.ResumeTiming();
        }
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Stack_Pop_AutoThreads)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->UseRealTime();

// ==================== 利用框架自动多线程的混合操作 ====================
static void BM_Stack_Mixed_AutoThreads(benchmark::State& state) {
    static lock_free_stack<int> stack;
    static std::once_flag flag;
    std::call_once(flag, [] {
        for (int i = 0; i < 500'000; ++i) {
            int val = i;
            stack.push(std::move(val));
        }
        });

    thread_local std::mt19937 rng(std::random_device{}() +
        std::hash<std::thread::id>()(std::this_thread::get_id()));
    thread_local std::uniform_int_distribution<int> coin(0, 99);

    for (auto _ : state) {
        if (coin(rng) < 50) {
            int val = static_cast<int>(rng());
            stack.push(std::move(val));
        }
        else {
            int val;
            if (stack.pop(val))
                benchmark::DoNotOptimize(val);
            else {
                state.PauseTiming();
                std::this_thread::yield();   // 失败时不计时也不 sleep
                state.ResumeTiming();
            }
        }
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Stack_Mixed_AutoThreads)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->UseRealTime();

BENCHMARK_MAIN();