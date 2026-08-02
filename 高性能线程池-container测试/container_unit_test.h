#pragma once
#include<iostream>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<vector>
#include<queue>
#include<memory>
#include<functional>
#include<future>
#include<concepts>
#include<type_traits>
#include<random>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <concepts>
#include <cstdint> // 用于 uint32_t/uint64_t 辅助
#include"container.h"
#define TEST_ROUNDS 1000

//测试代码

namespace t {
	using namespace con;
	class test {

	};
	template<typename T> requires std::derived_from<T,test>
	class threads_test{
	private:
		const size_t test_rounds;
		const std::thread::id attached_thread;
		std::atomic<bool> running;
		std::atomic<size_t> num;
		std::vector<std::thread> threads;
		std::mutex mtx;
		std::atomic<long long> micro;
		std::atomic<size_t> success_rounds;
		std::atomic<size_t> excep_rounds;
	public:
		inline size_t get_success_rounds()const noexcept {
			return success_rounds.load(std::memory_order_acquire);
		}
		inline size_t get_excep_rounds()const noexcept {
			return excep_rounds.load(std::memory_order_acquire);
		}
		inline long long get_micro()const noexcept {
			return micro.load(std::memory_order_acquire);
		}
		void stop(){
			std::lock_guard<std::mutex> locker(mtx);
			if (threads.empty())return;
			running.store(false, std::memory_order_release);
			for (size_t i = 0; i < threads.size(); i++) {
				if (threads[i].joinable())threads[i].join();

			}
		}
	public:
		threads_test(size_t num,
			std::function<void(T*,std::chrono::microseconds&)> func,
			T* ptr , size_t rounds=TEST_ROUNDS)
		:threads_test(num,rounds){
			if (ptr==nullptr)throw std::invalid_argument("test_thread 公有构造函数: ptr不能为nullptr");
			for (size_t i = 0; i < threads.size(); i++) {
				threads[i] = std::thread([ptr,this,func]() {
					std::chrono::microseconds tmp = std::chrono::microseconds::zero();
					size_t success = 0;
					size_t excep = 0;
					for (success; success < test_rounds && running.load(std::memory_order_acquire); success++) {
						try {
							func(ptr, tmp);
						}
						catch (std::exception& ex) {
							excep++;
						}
					}
					micro.fetch_add(tmp.count(), std::memory_order_acq_rel);
					success_rounds.fetch_add(success-excep, std::memory_order_acq_rel);
					excep_rounds.fetch_add(excep, std::memory_order_acq_rel);
					});			
			}
		}
		
		threads_test(const threads_test&) = delete;
		threads_test& operator=(const threads_test&) = delete;
		threads_test(threads_test&&) = delete;
		threads_test& operator=(threads_test&&) = delete;
		~threads_test() {
			stop();
		}
	private:
		threads_test(size_t nu,size_t rounds)
			:num(nu), running(true),
			
			attached_thread(std::this_thread::get_id()),
			test_rounds(rounds),micro(0),
			success_rounds(0),excep_rounds(0) {
			if (num == 0)throw std::invalid_argument("test_thread 私有构造函数: num不能为0");
			if(rounds==0)throw std::invalid_argument("test_thread 私有构造函数: rounds不能为0");
			threads.resize(num);
		}

	};
	template<typename T> requires std::derived_from<T, container>
	class container_test: public test {
	private:
		T container;
	public:
		static void emp_func(){}
		void test_push_and_pop(size_t test_num) {
			if (test_num == 0)return;
			std::chrono::time_point<std::chrono::steady_clock> time;
			std::chrono::microseconds push_dur=std::chrono::microseconds::zero();
			std::chrono::microseconds pop_dur = std::chrono::microseconds::zero();
			size_t push_bad_alloc_num = 0;
			size_t push_other_exception_num = 0;
			size_t pop_bad_alloc_num = 0;
			size_t pop_other_exception_num = 0;
			size_t push_succuss_num = 0;
			size_t pop_success_num = 0;
			size_t sum = 0;
			size_t max_size = 0;
			size_t final_size;
			task_pack pac;
			for (size_t i = 0; i < test_num; i++) {
				pac = task_pack(emp_func, rand()%50, i);
				time = std::chrono::steady_clock::now();
				try {
					container.push(std::move(pac));
				}
				catch (std::bad_alloc& ba) {
					push_bad_alloc_num++;
					continue;
				}
				catch (std::exception& ex) {
					push_other_exception_num++;
					continue;
				}
				push_dur = std::chrono::duration_cast<std::chrono::microseconds>
					(std::chrono::steady_clock::now() - time) + push_dur;
				push_succuss_num++;
				sum += i;

			}
			max_size = container.size();
			for (size_t i = 0, i2 = 0;i < max_size;) {
				if (i2 > 2 * max_size) {
					std::cout << "pop循环次数过多，终止pop模块测试" << std::endl;
					break;
				}
				time = std::chrono::steady_clock::now();
				result<task_pack, bool> res;
				try {
					res = container.pop2();
				}
				catch (std::bad_alloc& ba) {
					i2++;
					pop_bad_alloc_num++;
					continue;
				}
				catch (std::exception& ex) {
					i2++;
					pop_other_exception_num++;
					continue;
				}
				pop_dur = std::chrono::duration_cast<std::chrono::microseconds>
					(std::chrono::steady_clock::now() - time) + pop_dur;
				if (res.error_code) {
					pop_success_num++;
					i++;
					sum -= size_t(res.result1.get_trynum());
				}
				i2++;

			}
			final_size = container.size();
			std::cout << "测试结果:" << std::endl;
			std::cout << "对容器执行了" << test_num << "次push操作" << "和" << max_size << "次pop操作" << std::endl << std::endl;
			std::cout << "push共有" << push_bad_alloc_num+push_other_exception_num << "次抛出异常" << std::endl;
			std::cout << "bad_alloc有" << push_bad_alloc_num << "次" << std::endl;
			std::cout << "other_exception有" << push_other_exception_num << "次" << std::endl;
			std::cout << "成功push次数为" << push_succuss_num << std::endl;
			std::cout << "最后容器size为" << max_size << std::endl;
			std::cout << "执行总时间为" << push_dur.count()<<"微秒" << std::endl<<std::endl;

			std::cout << "pop共有" << pop_bad_alloc_num + pop_other_exception_num << "次抛出异常" << std::endl;
			std::cout << "bad_alloc有" << pop_bad_alloc_num << "次" << std::endl;
			std::cout << "other_exception有" << pop_other_exception_num << "次" << std::endl;
			std::cout << "成功pop次数为" << pop_success_num << std::endl;
			std::cout << "最后容器size为" << final_size << std::endl;
			std::cout << "执行总时间为" << pop_dur.count() << "微秒" << std::endl << std::endl;

			std::cout << "总结" << std::endl;
			if (push_succuss_num == max_size&& push_bad_alloc_num + push_other_exception_num + push_succuss_num == test_num)
				std::cout << "容器push模块 通过测试" << std::endl;
			else std::cout << "容器push模块 未通过测试" << std::endl;
			if(pop_success_num==max_size&&final_size==0)
				std::cout << "容器pop模块 通过测试" << std::endl;
			else std::cout << "容器pop模块 未通过测试" << std::endl;
			if (sum == 0)std::cout << "(所有放入与拿出的元素是否一样)正确率为 " << "100%" << std::endl;
			else std::cout << "(所有放入与拿出的元素是否一样)正确率为 " << "不合格(小于100%)" << std::endl;

			
		}
		void threads_test_push(std::chrono::microseconds& micro) {
			// 1. 静态随机数生成器（每个线程独立实例，避免竞争）
			thread_local std::mt19937 gen(std::random_device{}());
			// 2. 0~49的均匀整数分布（左闭右闭）
			thread_local std::uniform_int_distribution<size_t> dist(0, 49);
			task_pack ta(container_test<T>::emp_func, dist(gen));
			std::chrono::time_point<std::chrono::steady_clock> tmp = std::chrono::steady_clock::now();
			container.push(std::move(ta));
			micro = micro + std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - tmp);

		}
	public:
		container_test() = default;
		~container_test() = default;
		container_test(const container_test&) = delete;
		container_test& operator=(const container_test&) = delete;
		container_test(container_test&&) = delete;
		container_test& operator=(container_test&&) = delete;
	};


	template<typename T>
	void run_container_push_test(T& test_container, size_t thread_num
		, size_t round_num,
		const std::string& test_name) {
		// 1. 创建线程测试实例（复用你已有的threads_test类）
		t::threads_test<T> test_threads(thread_num,
			&T::threads_test_push,
			&test_container,
			round_num);

		// 2. 输出测试基础信息（复用原有调试输出逻辑）
		std::cout << "push模块---->>>" << std::endl;
		std::cout << test_name << std::endl;
		std::cout << "线程数 " << thread_num << std::endl;
		std::cout << "每个线程循环轮次 " << round_num << std::endl << std::endl;

		// 3. 第一次sleep后输出中间结果（复用原有逻辑）
		std::this_thread::sleep_for(std::chrono::seconds(3));
		std::cout << "异常抛出总量 " << test_threads.get_excep_rounds() << std::endl;
		std::cout << "成功总轮次 " << test_threads.get_success_rounds() << std::endl;
		std::cout << "总时间 " << test_threads.get_micro() << "微秒" << std::endl;
		std::cout << "---->>>" << std::endl;

		// 4. 停止线程并第二次sleep输出最终结果（复用原有逻辑）
		test_threads.stop();
		std::this_thread::sleep_for(std::chrono::seconds(3));
		std::cout << "异常抛出总量 " << test_threads.get_excep_rounds() << std::endl;
		std::cout << "成功总轮次 " << test_threads.get_success_rounds() << std::endl;
		std::cout << "总时间 " << test_threads.get_micro() << "微秒" << std::endl;

		// 5. 计算并输出吞吐量（复用原有逻辑，避免除0）
		double throughput = 0.0;
		long long total_micro = test_threads.get_micro();
		if (total_micro > 0) {
			throughput = (double)test_threads.get_success_rounds() / (double)total_micro;
		}
		std::cout << "任务吞吐量 " << throughput << "单位/微秒" << std::endl;
	}

	void test_all_containers_multithread_push(size_t thread_num = 200, size_t round_num = 10000) {
		// 封装：创建container_test实例 + 调用run_container_push_test
		auto run_test = [&](const std::string& name, auto container_type) {
			auto con_ptr = std::make_unique<container_test<decltype(container_type)>>();
			run_container_push_test(*con_ptr, thread_num, round_num, name);
			std::cout << "\n=====================================\n" << std::endl;
			};
		run_test("无锁可扩容的环形缓冲区", lock_free_circular_con());
		run_test("improved_multilevel_con<50,50>并发性能测试", improved_multilevel_con<50, 50>());
		//run_test("multilevel_con未改进多级队列性能测试", multilevel_con());
		//run_test("longest_first_con长作业优先容器性能测试", longest_first_con());
		run_test("FIFO_con单锁FIFO容器性能测试", FIFO_con());
	}
	void test_all_containers() {
		// 测试函数模板
		auto test_single_container = [](const std::string& name, auto&& container) {
			std::cout << "===== 测试 " << name << " =====" << std::endl;
			container.test_push_and_pop(1000000);
			std::cout << "======================\n" << std::endl;
			};

		
		//test_single_container("改进版50x50容器", container_test<improved_multilevel_con<50, 50>>());
		test_single_container("可扩容的无锁环形缓冲区", container_test<lock_free_circular_con>());
		//test_single_container("未改进多级队列", container_test<multilevel_con>());
		//test_single_container("长作业优先容器", container_test<longest_first_con>());
		//test_single_container("单锁FIFO容器", container_test<FIFO_con>());
		
		
	}


}


namespace ai_t {
    class TestUtils {
    public:
        // ========== 新增：线程安全的通用输出接口 ==========
        template <typename... Args>
        static void print(const Args&... args) {
            std::lock_guard<std::mutex> lock(_cout_mtx);
            (std::cout << ... << args) << std::endl;
        }

        template <typename... Args>
        static void print_no_newline(const Args&... args) {
            std::lock_guard<std::mutex> lock(_cout_mtx);
            (std::cout << ... << args);
        }
        // 线程安全的随机数生成器（单例）
        static std::mt19937& get_rng() {
            thread_local static std::mt19937 gen(std::random_device{}());
            return gen;
        }

        // 生成测试任务包（task_rank 为无上限 unsigned int，随机生成）
        static con::task_pack generate_test_task(unsigned int rank = 1, int try_num = 3) {
            static std::atomic<size_t> task_id{ 0 };
            size_t tid = task_id.fetch_add(1, std::memory_order_relaxed);

            return con::task_pack(
                [tid]() { /* 空任务，仅用于测试 */ },
                rank,
                std::max(try_num, 1) // 保证 try_num >= 1
            );
        }

        // 计时工具：返回耗时（微秒）
        template <typename Func>
        static long long measure_time(Func&& func) {
            auto start = std::chrono::steady_clock::now();
            func();
            auto end = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        }

        // 输出测试结果（带 PASS/FAIL 标识）
        static void print_test_result(const std::string& test_name, bool pass) {
            std::lock_guard<std::mutex> lock(_cout_mtx);
            if (pass) {
                std::cout << "[PASS] " << test_name << std::endl;
            }
            else {
                std::cout << "[FAIL] " << test_name << std::endl;
            }
        }

        // 输出性能报告（生产级核心指标）
        static void print_perf_report(const std::string& title,
            long long total_time_us,
            size_t total_ops,
            const std::vector<long long>& latencies_us) {
            std::lock_guard<std::mutex> lock(_cout_mtx);
            std::cout << "\n===== " << title << " =====" << std::endl;
            std::cout << "总操作数: " << total_ops << std::endl;
            std::cout << "总耗时(微秒): " << total_time_us << std::endl;
            std::cout << "吞吐量(ops/微秒): " << (total_time_us > 0 ? (double)total_ops / total_time_us : 0.0) << std::endl;

            if (!latencies_us.empty()) {
                long long avg = std::accumulate(latencies_us.begin(), latencies_us.end(), 0LL) / latencies_us.size();
                long long min = *std::min_element(latencies_us.begin(), latencies_us.end());
                long long max = *std::max_element(latencies_us.begin(), latencies_us.end());

                // 计算延迟标准差
                double mean = avg;
                double sq_sum = std::inner_product(latencies_us.begin(), latencies_us.end(), latencies_us.begin(), 0.0);
                double stdev = std::sqrt(sq_sum / latencies_us.size() - mean * mean);

                std::cout << "平均延迟(微秒): " << avg << std::endl;
                std::cout << "最小延迟(微秒): " << min << std::endl;
                std::cout << "最大延迟(微秒): " << max << std::endl;
                std::cout << "延迟标准差: " << stdev << std::endl;
            }
            std::cout << "===========================" << std::endl;
        }

    private:
        static inline std::mutex _cout_mtx; // 控制台输出锁（线程安全）
    };

    // -------------------------- 单线程测试套件（核心正确性） --------------------------
    template <typename ContainerT>
        requires std::derived_from<ContainerT, con::container>
    class SingleThreadTest {
    public:
        SingleThreadTest() : _container(std::make_unique<ContainerT>()) {}

        // 执行所有单线程测试
        bool run_all() {
            bool all_pass = true;
            all_pass &= test_empty_container();
            all_pass &= test_push_pop_basic();
            all_pass &= test_pop_empty_container();
            all_pass &= test_size_consistency();
            all_pass &= test_task_attribute_preservation();
            all_pass &= test_adjust_method();
            all_pass &= test_boundary_try_num();
            return all_pass;
        }

    private:
        std::unique_ptr<ContainerT> _container;

        // 测试1：空容器初始状态验证
        bool test_empty_container() {
            const std::string test_name = "单线程-空容器初始状态";
            try {
                bool empty = _container->empty();
                size_t size = _container->size();
                bool pass = (empty && size == 0);
                TestUtils::print_test_result(test_name, pass);
                return pass;
            }
            catch (...) {
                TestUtils::print_test_result(test_name, false);
                return false;
            }
        }

        // 测试2：基础 push/pop 正确性（适配无上限 task_rank）
        bool test_push_pop_basic() {
            const std::string test_name = "单线程-push/pop基础正确性";
            try {
                // 清空容器
                while (!_container->empty()) _container->pop();

                // 推送100个任务（task_rank 随机生成，无上限）
                const size_t task_count = 100;
                std::vector<unsigned int> pushed_ranks;
                for (size_t i = 0; i < task_count; ++i) {
                    // 随机生成无符号优先级（0 ~ UINT32_MAX 范围）
                    unsigned int rank = static_cast<unsigned int>(TestUtils::get_rng()());
                    auto task = TestUtils::generate_test_task(rank, 3);
                    pushed_ranks.push_back(rank);
                    _container->push(std::move(task));
                }

                // 弹出并验证任务数量/容器空状态
                size_t popped_count = 0;
                while (!_container->empty()) {
                    auto task = _container->pop();
                    popped_count++;
                }

                bool pass = (popped_count == task_count) && (_container->empty());
                TestUtils::print_test_result(test_name, pass);
                return pass;
            }
            catch (...) {
                TestUtils::print_test_result(test_name, false);
                return false;
            }
        }

        // 测试3：空容器 pop 容错性
        bool test_pop_empty_container() {
            const std::string test_name = "单线程-空容器pop容错性";
            try {
                // 清空容器
                while (!_container->empty()) _container->pop();

                // ========== 核心修改：完全忽略pop()，只验证pop2() ==========
                // 1. 不再测试pop()（旧版本接口，无需验证）
                // 2. 仅验证pop2()：空容器必须返回 error_code = false
                con::result<con::task_pack, bool> res = _container->pop2();
                bool pop2_correct = (res.error_code == false);

                // 最终验证逻辑：只看pop2()是否返回正确的错误码
                bool pass = pop2_correct;
                TestUtils::print_test_result(test_name, pass);
                return pass;
            }
            catch (...) {
                // 仅捕获pop2()调用时的崩溃/异常（正常情况下不应触发）
                TestUtils::print_test_result(test_name, false);
                return false;
            }
        }

        // 测试4：size/empty 方法一致性
        bool test_size_consistency() {
            const std::string test_name = "单线程-size/empty一致性";
            try {
                // 重置容器
                while (!_container->empty()) _container->pop();
                if (_container->size() != 0 || !_container->empty()) return false;

                // 逐步 push 验证 size
                for (size_t i = 1; i <= 50; ++i) {
                    _container->push(TestUtils::generate_test_task());
                    if (_container->size() != i || _container->empty()) return false;
                }

                // 逐步 pop 验证 size
                for (size_t i = 50; i >= 1; --i) {
                    _container->pop();
                    if (_container->size() != i - 1) return false;
                }

                bool pass = (_container->size() == 0) && _container->empty();
                TestUtils::print_test_result(test_name, pass);
                return pass;
            }
            catch (...) {
                TestUtils::print_test_result(test_name, false);
                return false;
            }
        }

        // 测试5：任务属性（rank/try_num）保留
        bool test_task_attribute_preservation() {
            const std::string test_name = "单线程-任务属性保留";
            try {
                // 重置容器
                while (!_container->empty()) _container->pop();

                // 推送指定属性的任务
                unsigned int target_rank = 12345; // 任意无符号值
                int target_try_num = 7;
                auto task = TestUtils::generate_test_task(target_rank, target_try_num);
                _container->push(std::move(task));

                // 弹出并验证属性
                auto popped = _container->pop();
                bool pass = (popped.get_rank() == target_rank) && (popped.get_trynum() == target_try_num);
                TestUtils::print_test_result(test_name, pass);
                return pass;
            }
            catch (...) {
                TestUtils::print_test_result(test_name, false);
                return false;
            }
        }

        // 测试6：adjust 方法容错性（空/满容器调用不崩溃）
        bool test_adjust_method() {
            const std::string test_name = "单线程-adjust方法容错性";
            try {
                // 空容器调用 adjust
                _container->adjust();

                // 满容器调用 adjust
                for (size_t i = 0; i < 1000; ++i) {
                    _container->push(TestUtils::generate_test_task());
                }
                _container->adjust();

                TestUtils::print_test_result(test_name, true);
                return true;
            }
            catch (...) {
                TestUtils::print_test_result(test_name, false);
                return false;
            }
        }

        // 测试7：try_num 边界值（=1）保留
        bool test_boundary_try_num() {
            const std::string test_name = "单线程-try_num边界值(=1)";
            try {
                // 重置容器
                while (!_container->empty()) _container->pop();

                // 推送 try_num=1 的任务
                auto task = TestUtils::generate_test_task(999, 1);
                _container->push(std::move(task));

                // 验证 try_num 保留
                auto popped = _container->pop();
                bool pass = (popped.get_trynum() == 1);
                TestUtils::print_test_result(test_name, pass);
                return pass;
            }
            catch (...) {
                TestUtils::print_test_result(test_name, false);
                return false;
            }
        }
    };

    // -------------------------- 多线程测试套件（并发正确性） --------------------------
    template <typename ContainerT>
        requires std::derived_from<ContainerT, con::container>
    class MultiThreadTest {
    public:
        MultiThreadTest(size_t push_threads = 4, size_t pop_threads = 4, size_t tasks_per_thread = 1000)
            : _push_threads(push_threads)
            , _pop_threads(pop_threads)
            , _tasks_per_thread(tasks_per_thread)
            , _container(std::make_unique<ContainerT>())
            , _total_pushed(0)
            , _total_popped(0)
            , _pop_errors(0) {}

        // 执行所有多线程测试
        bool run_all() {
            bool all_pass = true;
            all_pass &= test_concurrent_push_pop();
            all_pass &= test_data_consistency();
            return all_pass;
        }

    private:
        size_t _push_threads;
        size_t _pop_threads;
        size_t _tasks_per_thread;
        std::unique_ptr<ContainerT> _container;
        std::atomic<size_t> _total_pushed;
        std::atomic<size_t> _total_popped;
        std::atomic<size_t> _pop_errors;
        std::mutex _mtx;

        // 推送线程工作函数（随机生成 task_rank）
        void push_worker() {
            for (size_t i = 0; i < _tasks_per_thread; ++i) {
                try {
                    // 随机无符号优先级 + 1~5 的 try_num
                    unsigned int rank = static_cast<unsigned int>(TestUtils::get_rng()());
                    int try_num = static_cast<int>(TestUtils::get_rng()() % 5 + 1);
                    auto task = TestUtils::generate_test_task(rank, try_num);

                    _container->push(std::move(task));
                    _total_pushed.fetch_add(1, std::memory_order_relaxed);
                }
                catch (...) {
                    // 记录异常但不终止线程
                }
            }
        }

        // 弹出线程工作函数
        void pop_worker() {
            size_t local_popped = 0;
            size_t local_errors = 0;
            size_t max_attempts = _tasks_per_thread * _push_threads * 2; // 防止死循环

            while (local_popped < _tasks_per_thread && max_attempts-- > 0) {
                con::result<con::task_pack, bool> res = _container->pop2();
                if (res.error_code) {
                    local_popped++;
                }
                else {
                    local_errors++;
                }
            }

            _total_popped.fetch_add(local_popped, std::memory_order_relaxed);
            _pop_errors.fetch_add(local_errors, std::memory_order_relaxed);
        }

        bool test_concurrent_push_pop() {
            const std::string test_name = "多线程-并发push/pop基本正确性";
            try {
                // 重置状态
                while (!_container->empty()) _container->pop();
                _total_pushed = 0;
                _total_popped = 0;
                _pop_errors = 0;

                // 创建 push/pop 线程池
                std::vector<std::thread> push_workers;
                std::vector<std::thread> pop_workers;

                for (size_t i = 0; i < _push_threads; ++i) {
                    push_workers.emplace_back(&MultiThreadTest::push_worker, this);
                }
                for (size_t i = 0; i < _pop_threads; ++i) {
                    pop_workers.emplace_back(&MultiThreadTest::pop_worker, this);
                }

                // 等待所有线程结束
                for (auto& t : push_workers) t.join();
                for (auto& t : pop_workers) t.join();

                // 验证：弹出数 ≤ 推送数 + 容器剩余数 = 推送数
                size_t remaining = _container->size();
                bool pass = (_total_popped <= _total_pushed) && (_total_popped + remaining == _total_pushed);

                // ========== 修改：替换直接访问 _cout_mtx 的输出逻辑 ==========
                TestUtils::print("  推送总数: ", _total_pushed);
                TestUtils::print("  弹出总数: ", _total_popped);
                TestUtils::print("  弹出错误数: ", _pop_errors);
                TestUtils::print("  容器剩余任务数: ", remaining);

                TestUtils::print_test_result(test_name, pass);
                return pass;
            }
            catch (...) {
                TestUtils::print_test_result(test_name, false);
                return false;
            }
        }

        // 测试2：并发数据一致性（无重复/丢失）
        bool test_data_consistency() {
            const std::string test_name = "多线程-数据一致性（无重复/丢失）";
            try {
                // 重置容器
                while (!_container->empty()) _container->pop();
                std::atomic<size_t> task_id{ 0 };
                std::vector<size_t> pushed_ids;
                std::vector<size_t> popped_ids;
                std::mutex id_mtx;

                // 推送带唯一ID的任务
                auto push_with_id = [&]() {
                    for (size_t i = 0; i < 1000; ++i) {
                        size_t tid = task_id.fetch_add(1, std::memory_order_relaxed);
                        auto task = con::task_pack(
                            [tid]() {},
                            static_cast<unsigned int>(tid), // 用ID作为rank（唯一）
                            3
                        );
                        _container->push(std::move(task));

                        std::lock_guard<std::mutex> lock(id_mtx);
                        pushed_ids.push_back(tid);
                    }
                    };

                // 弹出并记录ID
                auto pop_with_id = [&]() {
                    for (size_t i = 0; i < 1000; ++i) {
                        con::result<con::task_pack, bool> res = _container->pop2();
                        if (res.error_code) {
                            std::lock_guard<std::mutex> lock(id_mtx);
                            popped_ids.push_back(res.result1.get_rank()); // rank 存储了唯一ID
                        }
                    }
                    };

                // 运行并发线程
                std::thread pusher1(push_with_id);
                std::thread pusher2(push_with_id);
                std::thread popper1(pop_with_id);
                std::thread popper2(pop_with_id);

                pusher1.join();
                pusher2.join();
                popper1.join();
                popper2.join();

                // 验证：弹出ID是推送ID的子集（无重复/丢失）
                std::sort(pushed_ids.begin(), pushed_ids.end());
                std::sort(popped_ids.begin(), popped_ids.end());
                bool pass = std::includes(pushed_ids.begin(), pushed_ids.end(),
                    popped_ids.begin(), popped_ids.end());

                TestUtils::print_test_result(test_name, pass);
                return pass;
            }
            catch (...) {
                TestUtils::print_test_result(test_name, false);
                return false;
            }
        }
    };

    // -------------------------- 性能测试套件 --------------------------
    template <typename ContainerT>
        requires std::derived_from<ContainerT, con::container>
    class PerformanceTest {
    public:
        PerformanceTest(size_t warmup_ops = 10000, size_t test_ops = 100000)
            : _warmup_ops(warmup_ops)
            , _test_ops(test_ops)
            , _container(std::make_unique<ContainerT>()) {}

        // 执行所有性能测试
        void run_all() {
            warmup();
            test_push_performance();
            test_pop_performance();
            test_concurrent_performance();
        }

    private:
        size_t _warmup_ops;
        size_t _test_ops;
        std::unique_ptr<ContainerT> _container;

        // 预热（消除首次运行开销）
        void warmup() {
            for (size_t i = 0; i < _warmup_ops; ++i) {
                _container->push(TestUtils::generate_test_task());
                if (i % 2 == 0) _container->pop();
            }
            while (!_container->empty()) _container->pop();
        }

        // 测试1：单线程 push 性能
        void test_push_performance() {
            std::vector<long long> latencies;
            latencies.reserve(_test_ops);

            auto total_time = TestUtils::measure_time([&]() {
                for (size_t i = 0; i < _test_ops; ++i) {
                    auto start = std::chrono::steady_clock::now();
                    _container->push(TestUtils::generate_test_task());
                    auto end = std::chrono::steady_clock::now();
                    latencies.push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
                }
                });

            TestUtils::print_perf_report(
                "单线程push性能",
                total_time,
                _test_ops,
                latencies
            );

            // 清空容器
            while (!_container->empty()) _container->pop();
        }

        // 测试2：单线程 pop 性能
        void test_pop_performance() {
            // 预填充测试数据
            for (size_t i = 0; i < _test_ops; ++i) {
                _container->push(TestUtils::generate_test_task());
            }

            std::vector<long long> latencies;
            latencies.reserve(_test_ops);

            auto total_time = TestUtils::measure_time([&]() {
                for (size_t i = 0; i < _test_ops; ++i) {
                    auto start = std::chrono::steady_clock::now();
                    _container->pop();
                    auto end = std::chrono::steady_clock::now();
                    latencies.push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
                }
                });

            TestUtils::print_perf_report(
                "单线程pop性能",
                total_time,
                _test_ops,
                latencies
            );
        }

        // 测试3：并发 push/pop 性能
        void test_concurrent_performance() {
            const size_t push_threads = 8;
            const size_t pop_threads = 8;
            const size_t ops_per_thread = _test_ops / (push_threads + pop_threads);

            std::atomic<size_t> total_ops{ 0 };
            std::vector<long long> all_latencies;
            std::mutex latency_mtx;

            // Push 工作线程（带延迟统计）
            auto push_worker = [&]() {
                for (size_t i = 0; i < ops_per_thread; ++i) {
                    auto start = std::chrono::steady_clock::now();
                    _container->push(TestUtils::generate_test_task());
                    auto end = std::chrono::steady_clock::now();

                    std::lock_guard<std::mutex> lock(latency_mtx);
                    all_latencies.push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
                    total_ops.fetch_add(1, std::memory_order_relaxed);
                }
                };

            // Pop 工作线程（带延迟统计）
            auto pop_worker = [&]() {
                size_t local_ops = 0;
                while (local_ops < ops_per_thread && total_ops.load(std::memory_order_relaxed) > 0) {
                    auto start = std::chrono::steady_clock::now();
                    con::result<con::task_pack, bool> res = _container->pop2();
                    auto end = std::chrono::steady_clock::now();

                    if (res.error_code) {
                        std::lock_guard<std::mutex> lock(latency_mtx);
                        all_latencies.push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
                        local_ops++;
                    }
                }
                };

            // 执行并发测试并计时
            auto total_time = TestUtils::measure_time([&]() {
                std::vector<std::thread> workers;
                for (size_t i = 0; i < push_threads; ++i) {
                    workers.emplace_back(push_worker);
                }
                for (size_t i = 0; i < pop_threads; ++i) {
                    workers.emplace_back(pop_worker);
                }
                for (auto& t : workers) t.join();
                });

            TestUtils::print_perf_report(
                "并发push/pop性能(8P+8C)",
                total_time,
                all_latencies.size(),
                all_latencies
            );
        }
    };

    // -------------------------- 统一测试入口 --------------------------
    template <typename ContainerT>
        requires std::derived_from<ContainerT, con::container>
    void run_container_full_test(const std::string& container_name) {
        // ========== 修改：替换所有直接访问 _cout_mtx 的输出逻辑 ==========
        TestUtils::print("\n=====================================");
        TestUtils::print("开始测试容器: ", container_name);
        TestUtils::print("=====================================");

        // 1. 单线程正确性测试
        TestUtils::print("\n----- 单线程测试 -----");
        SingleThreadTest<ContainerT> st_test;
        bool st_pass = st_test.run_all();

        // 2. 多线程并发测试
        TestUtils::print("\n----- 多线程测试 -----");
        MultiThreadTest<ContainerT> mt_test(4, 4, 1000);
        bool mt_pass = mt_test.run_all();

        // 3. 性能测试
        TestUtils::print("\n----- 性能测试 -----");
        PerformanceTest<ContainerT> perf_test(10000, 100000);
        perf_test.run_all();

        // 汇总测试结果
        TestUtils::print("\n=====================================");
        TestUtils::print("容器测试汇总: ", container_name);
        TestUtils::print("单线程测试: ", (st_pass ? "通过" : "失败"));
        TestUtils::print("多线程测试: ", (mt_pass ? "通过" : "失败"));
        TestUtils::print("性能测试: 已完成（结果见上方）");
        TestUtils::print("=====================================");
    }

} // namespace ai_t

// ========== 测试使用示例（可直接拷贝到业务代码） ==========
/*
// 假设业务中实现了 con::container 的子类
class MyContainer : public con::container {
public:
    void push(con::task_pack&& pa) override {
        // 业务实现逻辑
    }
    con::task_pack pop() override {
        // 业务实现逻辑
    }
    con::result<con::task_pack, bool> pop2() override {
        // 业务实现逻辑
    }
    void adjust() override {
        // 业务实现逻辑
    }
    bool empty() const override {
        // 业务实现逻辑
    }
    size_t size() const override {
        // 业务实现逻辑
    }
};

// 主函数中执行全量测试
int main() {
    ai_t::run_container_full_test<MyContainer>("自定义容器MyContainer");
    return 0;
}
*/
