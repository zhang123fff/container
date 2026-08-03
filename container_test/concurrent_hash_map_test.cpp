#include "pch.h"

#include "C:\Users\86172\source\repos\高性能线程池-container测试\高性能线程池-container测试\concurrent_hash_map.hpp"   
#include <vector>
#include <thread>
#include<optional>
using namespace con;   

// ==============================
// 1. 基本功能测试
// ==============================

TEST(ConcurrentHashMapTest, InsertAndFind) {
    concurrent_hash_map<int, std::string> map(16);

    EXPECT_TRUE(map.insert(1, "one"));
    EXPECT_TRUE(map.insert(2, "two"));
    EXPECT_TRUE(map.insert(3, "three"));

    auto handle = map.read_find(2);
    ASSERT_TRUE(handle.has_value());
    EXPECT_EQ(handle->get(), "two");

    auto not_found = map.read_find(99);
    EXPECT_FALSE(not_found.has_value());
}

TEST(ConcurrentHashMapTest, UpdateValue) {
    concurrent_hash_map<int, std::string> map(16);
    map.insert(1, "one");

    
    {
        auto write_handle = map.write_find(1);
        ASSERT_TRUE(write_handle.has_value());
        write_handle->get() = "uno";
    }

    auto read_handle = map.read_find(1);
    ASSERT_TRUE(read_handle.has_value());
    EXPECT_EQ(read_handle->get(), "uno");
}

TEST(ConcurrentHashMapTest, EraseKey) {
    concurrent_hash_map<int, std::string> map(16);
    map.insert(1, "one");
    map.insert(2, "two");

    EXPECT_TRUE(map.erase(1));
    EXPECT_FALSE(map.read_find(1).has_value());
    EXPECT_TRUE(map.read_find(2).has_value());

    EXPECT_FALSE(map.erase(99));
}

TEST(ConcurrentHashMapTest, DuplicateInsert) {
    concurrent_hash_map<int, std::string> map(16);
    EXPECT_TRUE(map.insert(1, "first"));
    EXPECT_FALSE(map.insert(1, "second"));

    auto handle = map.read_find(1);
    ASSERT_TRUE(handle.has_value());
    EXPECT_EQ(handle->get(), "first");
}

// ==============================
// 2. 构造测试
// ==============================

TEST(ConcurrentHashMapTest, ConstructFromInitializerList) {
    concurrent_hash_map<int, std::string> map{
        {1, "one"}, {2, "two"}, {3, "three"}
    };

    EXPECT_EQ(map.read_find(1)->get(), "one");
    EXPECT_EQ(map.read_find(2)->get(), "two");
    EXPECT_EQ(map.read_find(3)->get(), "three");
    EXPECT_FALSE(map.read_find(4).has_value());
}

TEST(ConcurrentHashMapTest, ConstructFromVector) {
    std::vector<std::pair<int, std::string>> data = {
        {1, "one"}, {2, "two"}, {3, "three"}
    };
    concurrent_hash_map<int, std::string> map(std::move(data), 8);

    EXPECT_EQ(map.read_find(1)->get(), "one");
    EXPECT_EQ(map.read_find(2)->get(), "two");
    EXPECT_EQ(map.read_find(3)->get(), "three");
}

// ==============================
// 3. 并发安全测试
// ==============================

TEST(ConcurrentHashMapTest, ConcurrentInsert) {
    concurrent_hash_map<int, int> map(32);
    const int num_threads = 10;
    const int ops_per_thread = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&map, t, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                int key = t * ops_per_thread + j;
                map.insert(key, key * 2);
            }
            });
    }

    for (auto& th : threads) {
        th.join();
    }

    for (int t = 0; t < num_threads; ++t) {
        for (int j = 0; j < ops_per_thread; ++j) {
            int key = t * ops_per_thread + j;
            auto handle = map.read_find(key);
            ASSERT_TRUE(handle.has_value());
            EXPECT_EQ(handle->get(), key * 2);
        }
    }
}

TEST(ConcurrentHashMapTest, ConcurrentWriteSameKey) {
    concurrent_hash_map<int, int> map(16);
    map.insert(0, 0);

    const int num_writers = 8;
    const int increments_per_writer = 100;
    std::vector<std::thread> writers;

    for (int i = 0; i < num_writers; ++i) {
        writers.emplace_back([&map, increments_per_writer]() {
            for (int j = 0; j < increments_per_writer; ++j) {
                auto handle = map.write_find(0);
                ASSERT_TRUE(handle.has_value());
                handle->get() = handle->get() + 1;
            }
            });
    }

    for (auto& w : writers) {
        w.join();
    }

    auto final_handle = map.read_find(0);
    ASSERT_TRUE(final_handle.has_value());
    EXPECT_EQ(final_handle->get(), num_writers * increments_per_writer);
}

TEST(ConcurrentHashMapTest, ConcurrentReadWrite) {
    concurrent_hash_map<int, int> map(32);
    const int num_elements = 100;
    for (int i = 0; i < num_elements; ++i) {
        map.insert(i, i);
    }

    std::vector<std::thread> threads;
    const int num_readers = 5;
    const int num_writers = 2;
    const int ops_per_thread = 200;

    for (int t = 0; t < num_writers; ++t) {
        threads.emplace_back([&map, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                int key = j % 50;
                auto handle = map.write_find(key);
                if (handle.has_value()) {
                    handle->get() = handle->get() + 1;
                }
            }
            });
    }

    for (int t = 0; t < num_readers; ++t) {
        threads.emplace_back([&map, ops_per_thread]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                int key = j % 50;
                auto handle = map.read_find(key);
                (void)handle;
            }
            });
    }

    for (auto& th : threads) {
        th.join();
    }

    for (int i = 0; i < num_elements; ++i) {
        auto handle = map.read_find(i);
        if (handle.has_value()) {
            EXPECT_GE(handle->get(), i);
        }
    }
}

// ==============================
// 4. 边界条件与异常情况
// ==============================

TEST(ConcurrentHashMapTest, EmptyTable) {
    concurrent_hash_map<int, std::string> map(4);
    EXPECT_FALSE(map.read_find(1).has_value());
    EXPECT_FALSE(map.erase(1));
    EXPECT_TRUE(map.insert(1, "first"));
}

TEST(ConcurrentHashMapTest, ZeroBucketThrows) {
    bool threw = false;
    try {
        concurrent_hash_map<int, int> map(0);
    }
    catch (const std::invalid_argument&) {
        threw = true;
    }
    EXPECT_TRUE(threw);
}

TEST(ConcurrentHashMapTest, LargeData) {
    const int N = 10000;
    concurrent_hash_map<int, int> map(64);
    for (int i = 0; i < N; ++i) {
        map.insert(i, i * 2);
    }
    for (int i = 0; i < N; ++i) {
        auto handle = map.read_find(i);
        ASSERT_TRUE(handle.has_value());
        EXPECT_EQ(handle->get(), i * 2);
    }
}

// ==============================
// 5. 性能测试
// ==============================

TEST(ConcurrentHashMapTest, PerformanceInsert) {
    concurrent_hash_map<int, int> map(128);
    const int N = 100000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; ++i) {
        map.insert(i, i);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    printf("[Performance] Insert %d elements took %lld ms\n", N, ms);
    SUCCEED();
}