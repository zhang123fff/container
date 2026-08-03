#include "pch.h"

#include "C:\Users\86172\source\repos\高性能线程池-container测试\高性能线程池-container测试\lock_free_stack.hpp"   
#include <vector>
#include <thread>
#include<optional>

using namespace con;

// ---------- 基础功能测试 ----------
TEST(LockFreeStackTest, SingleThreadPushPop) {
    lock_free_stack<int> stack;
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.size(), 0);

    stack.push(1);      // 字面量是右值，OK
    stack.push(2);
    stack.push(3);
    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.size(), 3);

    int val;
    EXPECT_TRUE(stack.pop(val));
    EXPECT_EQ(val, 3);
    EXPECT_EQ(stack.size(), 2);

    EXPECT_TRUE(stack.pop(val));
    EXPECT_EQ(val, 2);
    EXPECT_TRUE(stack.pop(val));
    EXPECT_EQ(val, 1);

    EXPECT_FALSE(stack.pop(val));
    EXPECT_TRUE(stack.empty());
}

TEST(LockFreeStackTest, MoveOnlyType) {
    struct MoveOnly {
        int val;
        MoveOnly(int v) : val(v) {}
        MoveOnly(MoveOnly&&) noexcept = default;
        MoveOnly& operator=(MoveOnly&&) noexcept = default;
        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;
    };

    lock_free_stack<MoveOnly> stack;
    stack.push(MoveOnly(42));   // 临时对象是右值
    MoveOnly out(0);
    EXPECT_TRUE(stack.pop(out));
    EXPECT_EQ(out.val, 42);
}

TEST(LockFreeStackTest, LIFOOrder) {
    lock_free_stack<int> stack;
    const int N = 100;
    for (int i = 0; i < N; ++i) {
        int temp = i;           // 先存到局部变量
        stack.push(std::move(temp));  // 转为右值
    }
    for (int i = N - 1; i >= 0; --i) {
        int val;
        EXPECT_TRUE(stack.pop(val));
        EXPECT_EQ(val, i);
    }
}

TEST(LockFreeStackTest, EmptyPop) {
    lock_free_stack<int> stack;
    int dummy;
    EXPECT_FALSE(stack.pop(dummy));
}

TEST(LockFreeStackTest, EmptyPopDoesNotAffectSize) {
    lock_free_stack<int> stack;
    EXPECT_EQ(stack.size(), 0);
    int dummy;
    stack.pop(dummy);  // 应返回 false
    EXPECT_EQ(stack.size(), 0);  // size 不变
}

// ---------- 并发正确性测试 ----------
TEST(LockFreeStackTest, MultiThreadPushPopAllDataRetained) {
    const int num_threads = 8;
    const int ops_per_thread = 2000;

    lock_free_stack<int> stack;
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // 记录弹出元素
    std::vector<int> popped;
    std::mutex popped_mutex;

    // 先启动消费者（此时栈空，会立即返回 false）
    std::atomic<bool> producers_done{ false };
    for (int i = 0; i < num_threads; ++i) {
        consumers.emplace_back([&]() {
            int val;
            // 持续 pop 直到生产结束且栈空
            while (!producers_done.load(std::memory_order_acquire) || stack.size() > 0) {
                if (stack.pop(val)) {
                    std::lock_guard<std::mutex> lock(popped_mutex);
                    popped.push_back(val);
                }
            }
            });
    }

    // 生产者
    for (int i = 0; i < num_threads; ++i) {
        producers.emplace_back([&, i]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                int key = i * ops_per_thread + j;
                stack.push(std::move(key));  // key 转右值
            }
            });
    }

    for (auto& t : producers) t.join();
    producers_done.store(true, std::memory_order_release);
    for (auto& t : consumers) t.join();

    // 清空剩余元素
    int val;
    while (stack.pop(val)) {
        std::lock_guard<std::mutex> lock(popped_mutex);
        popped.push_back(val);
    }

    // 检查总数
    EXPECT_EQ(popped.size(), num_threads * ops_per_thread);

    // 检查所有值都在（multiset 比较）
    std::multiset<int> expected;
    for (int i = 0; i < num_threads; ++i)
        for (int j = 0; j < ops_per_thread; ++j)
            expected.insert(i * ops_per_thread + j);

    std::multiset<int> actual(popped.begin(), popped.end());
    EXPECT_EQ(actual, expected);
}

TEST(LockFreeStackTest, ConcurrentPushThenPopAll) {
    const int num_threads = 10;
    const int ops_per_thread = 1000;

    lock_free_stack<int> stack;

    // 阶段1：所有线程只 push
    {
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                for (int j = 0; j < ops_per_thread; ++j) {
                    int key = i * ops_per_thread + j;
                    stack.push(std::move(key));  // 转右值
                }
                });
        }
        for (auto& t : threads) t.join();
    }

    // 阶段2：所有线程只 pop
    std::vector<int> popped;
    std::mutex popped_mutex;
    {
        std::vector<std::thread> threads;
        std::atomic<int> total_popped{ 0 };
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&]() {
                int val;
                while (total_popped.load(std::memory_order_relaxed) < num_threads * ops_per_thread) {
                    if (stack.pop(val)) {
                        total_popped.fetch_add(1, std::memory_order_relaxed);
                        std::lock_guard<std::mutex> lock(popped_mutex);
                        popped.push_back(val);
                    }
                }
                });
        }
        for (auto& t : threads) t.join();
    }

    // 验证数据完整性
    std::multiset<int> expected;
    for (int i = 0; i < num_threads; ++i)
        for (int j = 0; j < ops_per_thread; ++j)
            expected.insert(i * ops_per_thread + j);

    std::multiset<int> actual(popped.begin(), popped.end());
    EXPECT_EQ(actual, expected);
}

// ---------- 内存安全测试 ----------
TEST(LockFreeStackTest, NoDoubleFreeOrUseAfterFree) {
    const int num_threads = 6;
    const int ops = 5000;
    lock_free_stack<int> stack;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ops; ++j) {
                if (j % 3 == 0) {
                    int val = j;
                    stack.push(std::move(val));  // 转右值
                }
                else if (j % 3 == 1) {
                    int v;
                    stack.pop(v);  // 可能失败但不影响
                }
                else {
                    int val1 = j;
                    stack.push(std::move(val1));  // 转右值
                    int v;
                    stack.pop(v);
                }
            }
            });
    }
    for (auto& t : threads) t.join();
    // 使用 AddressSanitizer 编译，验证无 use-after-free / double-free
    SUCCEED();
}

// ---------- 边界与压力测试 ----------
TEST(LockFreeStackTest, LargeNumberOfNodes) {
    lock_free_stack<size_t> stack;
    const size_t N = 100000;
    for (size_t i = 0; i < N; ++i) {
        size_t temp = i;           // 先存到局部变量
        stack.push(std::move(temp));  // 转为右值
    }
    EXPECT_EQ(stack.size(), N);
    for (size_t i = 0; i < N; ++i) {
        size_t v;
        EXPECT_TRUE(stack.pop(v));
        EXPECT_EQ(v, N - 1 - i);
    }
    EXPECT_EQ(stack.size(), 0);
}

TEST(LockFreeStackTest, StackReuseAfterEmpty) {
    lock_free_stack<int> stack;
    int val;
    for (int round = 0; round < 10; ++round) {
        for (int i = 0; i < 100; ++i) {
            int temp = i;
            stack.push(std::move(temp));  // 转右值
        }
        for (int i = 0; i < 100; ++i) {
            ASSERT_TRUE(stack.pop(val));
        }
    }
    EXPECT_TRUE(stack.empty());
}

TEST(LockFreeStackTest, PushAfterEmptyPop) {
    lock_free_stack<int> stack;
    int val;

    int temp1 = 1;
    stack.push(std::move(temp1));
    stack.pop(val);
    // 栈已空
    int temp2 = 2;
    stack.push(std::move(temp2));
    EXPECT_TRUE(stack.pop(val));
    EXPECT_EQ(val, 2);
}

// 1. 高压力混合 push/pop（暴露 use-after-free）
TEST(LockFreeStackTest, HighPressureMixedPushPop) {
    lock_free_stack<int> stack;
    const int num_threads = 12;
    const int duration_ms = 3000;
    std::atomic<bool> stop{ false };
    std::atomic<int> ops{ 0 };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            while (!stop.load(std::memory_order_relaxed)) {
                if (rand() % 2) {
                    int val = rand();
                    stack.push(std::move(val));
                }
                else {
                    int val;
                    stack.pop(val);
                }
                ops.fetch_add(1, std::memory_order_relaxed);
            }
            });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    stop.store(true);
    for (auto& t : threads) t.join();

    int val;
    while (stack.pop(val));
    std::cout << "[HighPressure] Total ops: " << ops.load() << std::endl;
    SUCCEED();
}

// 2. ABA 问题暴露测试
TEST(LockFreeStackTest, ABAProblem) {
    lock_free_stack<int> stack;
    std::atomic<bool> stop{ false };
    std::atomic<int> pushes{ 0 }, pops{ 0 };

    std::thread pusher([&]() {
        for (int i = 0; !stop.load(); i += 2) {
            int val = i;
            stack.push(std::move(val));
            pushes.fetch_add(1);
        }
        });

    std::thread popper([&]() {
        while (!stop.load()) {
            int val;
            if (stack.pop(val)) {
                pops.fetch_add(1);
                int new_val = -val;
                stack.push(std::move(new_val));
                pushes.fetch_add(1);
            }
        }
        });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    stop.store(true);
    pusher.join();
    popper.join();

    int v;
    while (stack.pop(v)) pops.fetch_add(1);

    SUCCEED();
}

// 3. 延迟回收压力测试
TEST(LockFreeStackTest, ReclamationStress) {
    lock_free_stack<int> stack;
    const int num_threads = 20;
    const int items_per_thread = 1000;

    for (int i = 0; i < num_threads * items_per_thread; ++i) {
        int val = i;
        stack.push(std::move(val));
    }

    std::vector<std::thread> threads;
    std::atomic<long long> sum{ 0 };
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < items_per_thread; ++j) {
                int val;
                if (stack.pop(val)) {
                    sum.fetch_add(val);
                    std::this_thread::yield();
                }
            }
            });
    }

    for (auto& t : threads) t.join();

    const int N = num_threads * items_per_thread;
    long long expected = (long long)(N - 1) * N / 2;
    EXPECT_EQ(sum.load(), expected);
}

// 4. 持续混合操作下的数据完整性检查
TEST(LockFreeStackTest, ContinuousMixedWithIntegrity) {
    lock_free_stack<int> stack;
    const int num_threads = 8;
    const int duration_ms = 2000;
    std::atomic<bool> stop{ false };

    std::atomic<int> total_pushed{ 0 };
    std::atomic<int> total_popped{ 0 };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            while (!stop.load(std::memory_order_relaxed)) {
                int action = rand() % 3;
                if (action == 0) {
                    int val = rand();
                    stack.push(std::move(val));
                    total_pushed.fetch_add(1);
                }
                else if (action == 1) {
                    int val;
                    if (stack.pop(val)) {
                        total_popped.fetch_add(1);
                    }
                }
                else {
                    // 混合：先 push 再 pop
                    int val1 = rand();
                    stack.push(std::move(val1));
                    total_pushed.fetch_add(1);
                    int val2;
                    if (stack.pop(val2)) {
                        total_popped.fetch_add(1);
                    }
                }
            }
            });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    stop.store(true);
    for (auto& t : threads) t.join();

    // 清空剩余元素
    int val;
    while (stack.pop(val)) {
        total_popped.fetch_add(1);
    }

    // 最终 push 总数应等于 pop 总数（如果 ABA 导致元素丢失，这里会不相等）
    EXPECT_EQ(total_pushed.load(), total_popped.load());
}

// 5. 大容量单线程压力测试（检测内存泄漏或碎片化）
TEST(LockFreeStackTest, MassiveSingleThreadStress) {
    lock_free_stack<int> stack;
    const int N = 500000;

    for (int i = 0; i < N; ++i) {
        int val = i;
        stack.push(std::move(val));
    }

    EXPECT_EQ(stack.size(), N);

    for (int i = N - 1; i >= 0; --i) {
        int val;
        EXPECT_TRUE(stack.pop(val));
        EXPECT_EQ(val, i);
    }

    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.size(), 0);
}

// 6. 空栈与满栈快速交替（检测回收链表管理）
TEST(LockFreeStackTest, RapidEmptyFullCycle) {
    lock_free_stack<int> stack;
    const int cycles = 1000;
    const int items_per_cycle = 50;

    for (int c = 0; c < cycles; ++c) {
        // 填满
        for (int i = 0; i < items_per_cycle; ++i) {
            int val = i;
            stack.push(std::move(val));
        }
        // 清空
        for (int i = 0; i < items_per_cycle; ++i) {
            int val;
            ASSERT_TRUE(stack.pop(val));
        }
        ASSERT_TRUE(stack.empty());
    }
}

// ---------- 返回值验证测试 ----------
TEST(LockFreeStackTest, PushAlwaysReturnsTrue) {
    lock_free_stack<int> stack;
    int a = 42;
    EXPECT_TRUE(stack.push(std::move(a)));
    int b = 100;
    EXPECT_TRUE(stack.push(std::move(b)));
}

TEST(LockFreeStackTest, PopReturnsFalseWhenEmpty) {
    lock_free_stack<int> stack;
    int val;
    EXPECT_FALSE(stack.pop(val));

    int temp = 1;
    stack.push(std::move(temp));
    stack.pop(val);
    EXPECT_FALSE(stack.pop(val));
}