#pragma once
#include<cassert>
#include<iostream>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<vector>
#include<queue>
#include<memory>
#include<functional>
#include<future>
#include<shared_mutex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include<spdlog/async.h>
#define MEANINGLESS SIZE_MAX
#define MAX_IMPRO_SINGLE_LEVEL_ROWS 200
#define INVALID 0
#define MAXQUEUE 100
#define POP_TRY_NUM 50
#define SEP_FIFO_CON_MAXSIZE 500 
#define MULTILEVE_CON_LEVEL_NUM 50
#define RING_QUE_DEFUALT_SIZE 2000000
#define RING_QUE_GROWTH_FACTOR 2
#define RING_QUE_MAX_SIZE 10000000
#define LOCK_FREE_CIRCULAR_CON_MAX_TRYNUM 1000
using task_rank = size_t;

//核心类
namespace con {
	class task_pack {
	private:

		std::function<void()> task;
		task_rank rank;//可以为0,0等级最低
		int try_num;//注意不要设为小于1的数字。
		std::chrono::time_point<std::chrono::steady_clock> enqueue_time;//进入多级队列的时间
	public:
		bool operator<(const task_pack& other) const noexcept {
			return this->rank < other.rank; // 按优先级排序（示例）
		}
		task_rank get_rank()const noexcept {
			return rank;
		}
		void set_time()noexcept {
			enqueue_time = std::chrono::steady_clock::now();
		}
		int get_trynum()const noexcept {
			return try_num;
		}
		//会抛出异常
		void operator()() {
			task();
		}
		void set_trynum(int num)noexcept {
			try_num = num;
		}
	public:
		task_pack() :rank(INVALID), try_num(INVALID) {}
		//task_pack()noexcept = default;
		explicit task_pack(std::function<void()>&& func, task_rank ra, int tr = 3)noexcept
			:rank(ra), task(std::move(func)), try_num(tr),
			enqueue_time(std::chrono::time_point<std::chrono::steady_clock>::min()) {}
		~task_pack() = default;
		task_pack(task_pack&&) noexcept = default;
		task_pack& operator=(task_pack&&) noexcept = default;
		task_pack(const task_pack&) = default;// 删除拷贝操作
		task_pack& operator=(const task_pack&) = default;

	};
}

/////////////////////////////////////////deb已经废弃//////////////////////////////////////////////
namespace deb {
	class con_debug {
	private:
		std::shared_ptr<spdlog::async_logger> con_logger;
		std::shared_ptr<spdlog::details::thread_pool> pool;
	public:
		inline void start(const std::string& con_name) {
			con_logger->debug("开始容器" + con_name + "的debug_test");
		}
		inline std::string name()const noexcept {
			return con_logger->name();
		}
		inline void check_task_pack(const con::task_pack& pa) {
			con_logger->debug("任务等级: {} ,最大重试次数: {}", pa.get_rank(), pa.get_trynum());
		}
		inline void debug(const std::string& msg) {
			con_logger->debug(msg);
		}
		inline std::weak_ptr<spdlog::async_logger> get_weak()noexcept {
			return con_logger;
		}
		inline std::shared_ptr<spdlog::async_logger> get_shared() {
			return con_logger;
		}
	public:
		con_debug(const std::string& name, const std::vector<std::string>& paths, bool console) {
			if (paths.empty())throw std::invalid_argument("con_debug 构造函数 : 路径不能为空");
			if (name.empty())throw std::invalid_argument("con_debug 构造函数 : 名字不能为空");
			std::vector<spdlog::sink_ptr> vec;
			for (auto& i : paths)vec.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(i));
			pool = std::make_shared<spdlog::details::thread_pool>(8192, 2);
			if (console)vec.push_back(std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>());

			con_logger = std::make_shared<spdlog::async_logger>(name,
				vec.begin(), vec.end(),
				pool,
				spdlog::async_overflow_policy::block
			);
			con_logger->set_level(spdlog::level::debug);


		}
		con_debug(const std::string& name, const std::string& path,bool cons=true,
			spdlog::level::level_enum my_level=spdlog::level::debug) {
			if (path.empty())throw std::invalid_argument("con_debug 构造函数 : 路径不能为空");
			if (name.empty())throw std::invalid_argument("con_debug 构造函数 : 名字不能为空");
			auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path);
			spdlog::sink_ptr conso_sink;
			spdlog::sinks_init_list sinks;
			if (cons) {
				conso_sink = std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>();
				sinks = { conso_sink,file_sink };
			}
			else sinks = { file_sink };
			pool = std::make_shared<spdlog::details::thread_pool>(8192, 2);
			con_logger = std::make_shared<spdlog::async_logger>(name,
				sinks,
				pool,
				spdlog::async_overflow_policy::block
			);
			con_logger->set_level(my_level);
		}
		~con_debug() = default;
		// 禁用拷贝和移动
		con_debug(const con_debug&) = delete;
		con_debug& operator=(const con_debug&) = delete;
		con_debug(con_debug&&) = delete;
		con_debug& operator=(con_debug&&) = delete;
	};
	class con_debug_call_once {
	public:
		static con_debug& get_con_debug(const std::string& name,
			const std::vector<std::string>& paths, bool console) {
			static con_debug logger(name, paths, console);
			return logger;
		}
	};
}
/// <summary>
/// ////////////////////////////////////////////////////////////////////
/// </summary>

namespace con {
	
	/// <summary>
	/// ////////////////////////////以下都是基于task_pack类特化的容器/////////////////////////////
	/// </summary>
	class spin_mutex_lock_guard {
	private:
		std::atomic<bool>& signal;
	public:
		spin_mutex_lock_guard(std::atomic<bool>& sig)noexcept:signal(sig) {
			bool expected = false;
			while (!signal.compare_exchange_weak(expected,true,std::memory_order_acq_rel)) {
				expected = false;
				std::this_thread::yield();
			}

		}
		~spin_mutex_lock_guard() {
			signal.store(false, std::memory_order_release);
		}
		spin_mutex_lock_guard(const spin_mutex_lock_guard&) = delete;
		spin_mutex_lock_guard& operator=(const spin_mutex_lock_guard&) = delete;
		spin_mutex_lock_guard(spin_mutex_lock_guard&&) = delete;
		spin_mutex_lock_guard& operator=(spin_mutex_lock_guard&&) = delete;
	};
	template<typename T, typename T2>
	class result {
		static_assert(std::is_nothrow_move_constructible_v<T>, "T must be nothrow move constructible");
		static_assert(std::is_nothrow_move_constructible_v<T2>, "T2 must be nothrow move constructible");
		static_assert(std::is_nothrow_move_assignable_v<T>, "T must be nothrow move assignable");
		static_assert(std::is_nothrow_move_assignable_v<T2>, "T2 must be nothrow move assignable");
	public:
		T result1;
		T2 error_code;

	};
	class container {
	public:
		virtual void push(task_pack&& pa) = 0;
		virtual task_pack pop() = 0;
		virtual result<task_pack, bool> pop2() = 0;
		virtual void adjust() = 0;
		virtual ~container() = default;
		virtual bool empty()const = 0;
		virtual size_t size()const = 0;
	};
	class FIFO_con :public container {
	public:
		void push(task_pack&& pa)override {
			std::lock_guard<std::mutex> locker(mtx);
			que.emplace(std::move(pa));
			siz.fetch_add(1, std::memory_order_acq_rel);
		}
		task_pack pop()override {
			std::lock_guard<std::mutex> locker(mtx);
			if (siz == 0)throw std::out_of_range("multilevel_con: 多级队列为空");
			task_pack temp = std::move(que.front());
			que.pop();
			siz.fetch_sub(1, std::memory_order_acq_rel);
			return temp;
		}
		//fifo不需要调整，所以函数为空
		result<task_pack, bool> pop2()override {
			result<task_pack, bool> res;
			res.error_code = false;
			std::lock_guard<std::mutex> locker(mtx);
			if (siz == 0) return res;

			res.result1 = std::move(que.front());
			que.pop();
			siz.fetch_sub(1, std::memory_order_acq_rel);
			res.error_code = true;
			return res;
		}
		inline void adjust()noexcept override {}
		inline bool empty()const noexcept override {
			return siz.load(std::memory_order_acquire) == 0;
		}
		inline size_t size()const noexcept override {

			return siz.load(std::memory_order_acquire);
		}
	private:
		std::atomic<size_t> siz = 0;
		std::mutex mtx;
		std::queue<task_pack> que;
	public:
		FIFO_con() :siz(0) {}
		~FIFO_con()override = default;
		FIFO_con(const FIFO_con&) = delete;
		FIFO_con& operator=(const FIFO_con&) = delete;
		FIFO_con(FIFO_con&&) = delete;
		FIFO_con& operator=(FIFO_con&&) = delete;
	};
	class multilevel_con :public container {
	public:
		//rank不越界由上层调用者管理
		void push(task_pack&& pa)override {
			if (pa.get_rank() >= multi_que.size())
				throw std::invalid_argument("multilevel_con容器:多级队列无法容纳task_pack等级过高的任务");
			std::lock_guard<std::mutex> locker(mtx);
			multi_que[pa.get_rank()].emplace(std::move(pa));
			siz.fetch_add(1, std::memory_order_acq_rel);
		}
		task_pack pop()override {
			task_pack temp;
			std::lock_guard<std::mutex> locker(mtx);
			for (size_t i = multi_que.size() - 1; i != SIZE_MAX; i--) {
				if (!multi_que[i].empty()) {
					temp = std::move(multi_que[i].front());
					multi_que[i].pop();
					siz.fetch_sub(1, std::memory_order_acq_rel);
					return temp;
				}

			}
			throw std::out_of_range("multilevel_con: 多级队列为空或者无多级队列");
		}
		result<task_pack, bool> pop2()override {
			result<task_pack, bool> res;
			res.error_code = false;
			std::lock_guard<std::mutex> locker(mtx);
			for (size_t i = multi_que.size() - 1; i != SIZE_MAX; i--) {
				if (!multi_que[i].empty()) {
					res.result1 = std::move(multi_que[i].front());
					multi_que[i].pop();
					siz.fetch_sub(1, std::memory_order_acq_rel);
					res.error_code = true;
					return res;
				}
			}
			return res;
		}
		void adjust()override {

		}
		inline bool empty()const noexcept override {
			return siz.load(std::memory_order_acquire) == 0;
		}
		inline size_t size()const noexcept override {

			return siz.load(std::memory_order_acquire);
		}
	private:
		std::atomic<size_t> siz;
		std::mutex mtx;
		std::vector<std::queue<task_pack>> multi_que;
	public:
		//可以保证该容器一定有大于等于1的队列数量
		multilevel_con() :siz(0) {
			if (MULTILEVE_CON_LEVEL_NUM == 0)throw std::invalid_argument("multilevel_con: 容器队列数量不能为0");
			if (MULTILEVE_CON_LEVEL_NUM > MAXQUEUE)throw std::invalid_argument("multilevel_con: 容器队列数量超过限制");
			multi_que.resize(MULTILEVE_CON_LEVEL_NUM);
		}
		~multilevel_con()override = default;
		multilevel_con(const multilevel_con&) = delete;
		multilevel_con& operator=(const multilevel_con&) = delete;
		multilevel_con(multilevel_con&&) = delete;
		multilevel_con& operator=(multilevel_con&&) = delete;


	};
	class longest_first_con :public container {
	public:
		void push(task_pack&& pa)override {
			std::lock_guard<std::mutex> locker(mtx);
			pri_que.emplace(std::move(pa));
			siz.fetch_add(1, std::memory_order_acq_rel);
		}
		task_pack pop()override {
			std::lock_guard<std::mutex> locker(mtx);
			if (siz == 0)throw std::out_of_range("longest_first_con: 多级队列为空");
			task_pack temp = std::move(const_cast<task_pack&>(pri_que.top()));
			pri_que.pop();
			siz.fetch_sub(1, std::memory_order_acq_rel);
			return temp;
		}

		result<task_pack, bool> pop2()override {
			result<task_pack, bool> res;
			res.error_code = false;
			std::lock_guard<std::mutex> locker(mtx);
			if (siz == 0) return res;

			res.result1 = std::move(const_cast<task_pack&>(pri_que.top()));
			pri_que.pop();
			siz.fetch_sub(1, std::memory_order_acq_rel);
			res.error_code = true;
			return res;
		}
		inline void adjust()noexcept override {}
		inline bool empty()const noexcept override {
			return siz.load(std::memory_order_acquire) == 0;
		}
		inline size_t size()const noexcept override {

			return siz.load(std::memory_order_acquire);
		}
	private:
		std::atomic<size_t> siz = 0;
		std::mutex mtx;
		std::priority_queue<task_pack> pri_que;
	public:
		longest_first_con() :siz(0) {}
		~longest_first_con()override = default;
		longest_first_con(const longest_first_con&) = delete;
		longest_first_con& operator=(const longest_first_con&) = delete;
		longest_first_con(longest_first_con&&) = delete;
		longest_first_con& operator=(longest_first_con&&) = delete;
	};

	
	class lock_free_circular_con:public container {
		enum class memory_order{relax,common,strict};
		enum class version{undone,doing,done,undoing};
		deb::con_debug logger{ "无锁可扩容容器","C:\\Users\\86172\\Desktop\\logger1.txt",
			true,spdlog::level::err };
		//
	private:
		std::unique_ptr<task_pack[]> que;
		std::unique_ptr<std::atomic<version>[]> flags;
		std::atomic<size_t> front;
		std::atomic<size_t> rear;
		std::atomic<bool> spin;
		std::atomic<bool> extending;
		std::atomic<size_t> siz;
		
		std::atomic<size_t> capacity;
		std::shared_mutex mtx;
	public:
		template<memory_order M>
		inline size_t next_index(size_t idx)const noexcept {
			if constexpr (M == memory_order::relax) 
				return (idx + 1) % capacity.load(std::memory_order_relaxed);
			else if constexpr (M == memory_order::common)
				return (idx + 1) % capacity.load(std::memory_order_acquire);
			else 
				return (idx + 1) % capacity.load(std::memory_order_seq_cst);
		}
		void push(task_pack&& pa)override {
			
			logger.debug("push开始");
			logger.debug("第一阶段（获取idx，可能会扩容）");
			bool ex = false;
			size_t try_num = 0;
			size_t idx = SIZE_MAX;
			bool success = false;
			std::shared_lock<std::shared_mutex> locker;
			while (!success) {
				try_num++;
				if (try_num > LOCK_FREE_CIRCULAR_CON_MAX_TRYNUM)
					throw std::runtime_error("lock_free_circular_con push函数 : try_num不能超过最大限制");
				if (ex) {
					
					extend();
					ex = false;
				}
				else {
					locker = std::shared_lock<std::shared_mutex>(mtx);
					idx = front.load(std::memory_order_acquire);
					do {
						ex = false;
						if (next_index<memory_order::common>(idx)
							== rear.load(std::memory_order_acquire)) {
							locker.unlock();
							logger.debug("发现队列已满，修改标志ex值，准备扩容");
							ex = true;

							break;
						}

					} while (!(success=front.compare_exchange_weak(idx,
						next_index<memory_order::common>(idx), std::memory_order_acq_rel)));
				}
				
				logger.get_shared()->debug("第一阶段循环 try_num {}", try_num);
			}
			
			logger.get_shared()->debug(
				"第一阶段（获取idx）已经完成。已经获得对应的位置 {},循环次数 {}。",idx,try_num);
			try_num = 0;
			version flag = version::undone;
			//自旋锁防止pop没有完成，就push
			while (!flags[idx].compare_exchange_weak(flag,
				version::doing, std::memory_order_acq_rel)) {
				try_num++;
				flag = version::undone;
				std::this_thread::yield();
			}
			logger.get_shared()->debug(
				"第二阶段（获取自旋锁，正式进行push）已经完成。位置 {}，自旋次数 {}。", idx, try_num);

			que[idx] = std::move(pa);
			flags[idx].store(version::done, std::memory_order_release);
			logger.get_shared()->debug(
				"释放自旋锁。该位置为 {}，自旋次数 {}。", idx, try_num);
			siz.fetch_add(1, std::memory_order_acq_rel);
			logger.debug("push完成");//
			return;

		}
		result<task_pack, bool> pop2()override {
			logger.debug("进入pop2()");//
			logger.debug("第一阶段（获取idx）");
			result<task_pack, bool> res;
			res.error_code = false;
			size_t try_num = 0;
			if (siz.load(std::memory_order_acquire) == 0) return res;
			std::shared_lock<std::shared_mutex> locker(mtx);
			size_t idx = rear.load(std::memory_order_acquire);
			logger.debug("开始获取idx");//
			do {
				if (atomic_empty()) return res;
				try_num++;
				logger.get_shared()->debug(
					"第一阶段循环,try_num {}。", try_num);//
			} while (!rear.compare_exchange_weak(idx,
				next_index<memory_order::common>(idx), std::memory_order_acq_rel));
			logger.get_shared()->debug(
				"第一阶段（获取idx）已经完成。已经获得对应的位置 {},循环次数 {}。", idx, try_num);//
			version flag = version::done;
			//自旋锁防止push没有完成，就pop
			logger.debug("开始第二阶段，获取自旋锁flag[idx]");//
			try_num = 0;
			while (!flags[idx].compare_exchange_weak(flag,
				version::undoing, std::memory_order_acq_rel)) {
				flag = version::done;
				try_num++;
				std::this_thread::yield();
			}
			logger.get_shared()->debug(
				"第二阶段（获取自旋锁）已经完成。自旋次数 {}。", try_num);//
			res.result1 = std::move(que[idx]);
			flags[idx].store(version::undone, std::memory_order_release);
			siz.fetch_sub(1, std::memory_order_acq_rel);
			res.error_code = true;
			logger.debug("pop2完成");//
			return res;
		}
		task_pack pop()override {
			//暂未完成
			return task_pack();
		}
		void adjust()override{
			//暂未完成
		}

		inline bool empty()const noexcept override{
			return siz.load(std::memory_order_acquire) == 0;
		}
		inline size_t size()const noexcept override{
			return siz.load(std::memory_order_acquire);
		}
	private:
		template<memory_order M>
		static void fill_flags(std::unique_ptr<std::atomic<version>[]>& ptr,
			size_t start,size_t end,version flag)noexcept {
			assert(ptr && "lock_free_circular_con fill_flag函数 : ptr不能为空");
			//没有判断ptr是否大于end,需要依靠编译器debug下数组越界调试信息
			
			assert(start < end && "lock_free_circular_con fill_flag函数 : start不能大于等于end");
			
			if constexpr (M == memory_order::relax) {
				for (start; start < end; start++) ptr[start].store(flag, std::memory_order_relaxed);
			}
			else if constexpr (M == memory_order::common) {
				for (start; start < end; start++) ptr[start].store(flag, std::memory_order_release);
			}
			else {
				for (start; start < end; start++) ptr[start].store(flag, std::memory_order_seq_cst);
			}
			

		}
		void extend() {
			//保证只有一个线程去扩容，阻塞其他写线程
			bool ex = false;
			logger.debug("进入扩容extend()");
			
			do {
				if (ex) {
					logger.debug("扩容被阻止");
					logger.debug("退出扩容");
					return;
				}
				std::this_thread::yield();
			} while (!extending.compare_exchange_weak(ex,true,std::memory_order_acq_rel));
			logger.debug("扩容未被阻止，准备扩容");
			//获取目标扩容大小，并检查是否超过最大限制
			std::unique_ptr<task_pack[]> tmp;
			
			std::unique_ptr<std::atomic<version>[]> tmp_flags;
			size_t new_cap = capacity.load(std::memory_order_acquire) * RING_QUE_GROWTH_FACTOR;
			if (new_cap > RING_QUE_MAX_SIZE)
				throw std::out_of_range("lock_free_circular_con extend函数 : 容器无法扩容");
			
			try {
				tmp = std::make_unique<task_pack[]>(new_cap);
				tmp_flags = std::make_unique<std::atomic<version>[]>(new_cap);
			}
			catch (...) {
				extending.store(0, std::memory_order_release);
				throw;
			}
			//上锁扩容，阻塞其他读线程
			{
				logger.debug("即将获取写锁");
				std::unique_lock<std::shared_mutex> locker;
				logger.debug("上写锁,开始扩容");
				try {
					locker = std::unique_lock<std::shared_mutex>(mtx);
				}
				catch (...) {
					extending.store(0, std::memory_order_release);
					throw;
				}
				size_t fro = front.load(std::memory_order_relaxed);
				size_t re = rear.load(std::memory_order_relaxed);
				size_t si = siz.load(std::memory_order_relaxed);
				//转移赋值到tmp中，从0开始
				for (size_t i = 0; i < si; i++) {
					tmp[i] = std::move(que[re]);
					re = next_index<memory_order::relax>(re);

				}
				if(si!=0)fill_flags<memory_order::relax>(tmp_flags, 0,si, version::done);
				fill_flags<memory_order::relax>(tmp_flags, si, new_cap, version::undone);
				//重置变量信息，转移tmp空间
				front.store(si, std::memory_order_relaxed);
				rear.store(0, std::memory_order_relaxed);
				que = std::move(tmp);
				flags = std::move(tmp_flags);
				logger.get_shared()->info("扩容成功 ,原capacity {},现capacity {}",
					capacity.load(std::memory_order_relaxed), new_cap);
				capacity.store(new_cap, std::memory_order_relaxed);
				
			}
			//恢复extending
			extending.store(0, std::memory_order_release);
			

		}		
		bool atomic_empty()noexcept {
			bool expected = false;
			bool empty = false;
			while (!spin.compare_exchange_weak(expected, true,std::memory_order_acq_rel)) {
				expected = false;
				std::this_thread::yield();
			}
			empty = (rear.load(std::memory_order_acquire) == front.load(std::memory_order_acquire));
			spin.store(false, std::memory_order_release);
			return empty;

		}
	public:
		lock_free_circular_con() :front(0), rear(0), siz(0),extending(false),spin(false) {
			que = std::make_unique<task_pack[]>(RING_QUE_DEFUALT_SIZE);
			flags = std::make_unique<std::atomic<version>[]>(RING_QUE_DEFUALT_SIZE);
			fill_flags<memory_order::relax>(flags, 0, RING_QUE_DEFUALT_SIZE, version::undone);
			capacity.store(RING_QUE_DEFUALT_SIZE, std::memory_order_relaxed);
		}
		lock_free_circular_con(size_t cap) : front(0), rear(0), siz(0), extending(false), spin(false) {
			if (cap == 0)throw std::invalid_argument("lock_free_circular_con 构造函数 : 形参cap不能为0");
			que = std::make_unique<task_pack[]>(cap);
			flags = std::make_unique<std::atomic<version>[]>(cap);
			fill_flags<memory_order::relax>(flags, 0, cap, version::undone);
			capacity.store(cap,std::memory_order_relaxed);
		}
		~lock_free_circular_con()override = default;
		
	};
	

	//暂未完成
	/*
	class seperated_push_pop_FIFO_con :public container {
	public:
		void push(task_pack&& pa)override {
			std::lock_guard<std::mutex> locker(mtx);
			que.emplace(std::move(pa));
			siz.fetch_add(1, std::memory_order_acq_rel);
		}
		task_pack pop()override {
			std::lock_guard<std::mutex> locker(mtx);
			if (siz == 0)throw std::out_of_range("multilevel_con: 多级队列为空");
			task_pack temp = std::move(que.front());
			que.pop();
			siz.fetch_sub(1, std::memory_order_acq_rel);
			return temp;
		}
		//fifo不需要调整，所以函数为空
		result<task_pack, bool> pop2()override {
			result<task_pack, bool> res;
			res.error_code = false;
			std::lock_guard<std::mutex> locker(mtx);
			if (siz == 0) return res;

			res.result1 = std::move(que.front());
			que.pop();
			siz.fetch_sub(1, std::memory_order_acq_rel);
			res.error_code = true;
			return res;
		}
		inline void adjust()noexcept override {}
		inline bool empty()const noexcept override {
			return siz.load(std::memory_order_acquire) == 0;
		}
		inline size_t size()const noexcept override {

			return siz.load(std::memory_order_acquire);
		}
	private:
		std::atomic<size_t> siz = 0;
		std::mutex push_mtx;
		std::mutex pop_mtx;
		size_t front;
		size_t rear;
		std::unique_ptr<task_pack[]> que;
	public:
		seperated_push_pop_FIFO_con() :siz(0),front(0),rear(0) {
			//que = std::make_unique<task_pack[]>(SEP_FIFO_CON_MAXSIZE);

		}
		~seperated_push_pop_FIFO_con()override = default;
		seperated_push_pop_FIFO_con(const seperated_push_pop_FIFO_con&) = delete;
		seperated_push_pop_FIFO_con& operator=(const seperated_push_pop_FIFO_con&) = delete;
		seperated_push_pop_FIFO_con(seperated_push_pop_FIFO_con&&) = delete;
		seperated_push_pop_FIFO_con& operator=(seperated_push_pop_FIFO_con&&) = delete;
	};
	*/
	//release下10乘以10,200线程加10000轮，该容器任务吞吐量 0.302368单位 / 微秒
	template<size_t N, size_t M >
	class improved_multilevel_con :public container {
		static_assert(N != 0 && N <= MAXQUEUE && "improved_multilevel_con 模板参数N错误");
		static_assert(M != 0 && M <= MAX_IMPRO_SINGLE_LEVEL_ROWS && "improved_multilevel_con 模板参数M错误");
	private:
		std::atomic<size_t> siz;
		std::mutex mtx[N][M];
		std::atomic<bool> access;
		std::atomic<size_t> cur_max_ra;
		std::atomic<size_t> push_pos[N];
		std::atomic<size_t> ele_nums[N];
		std::atomic<size_t> pop_pos[N];
		std::queue<task_pack> multi_que[N][M];
	private:
		void update_cur_max_ra()noexcept {
			for (size_t i = N - 1; i != 0; i--) {
				if (ele_nums[i] != 0) {
					cur_max_ra.store(i, std::memory_order_release);
					return;
				}
			}
			cur_max_ra.store(0, std::memory_order_release);
		}
	public:
		size_t push_index(task_rank ra)noexcept {
			assert(ra + 1 <= N && "improved_multilevel_con  push_index()函数参数ra错误");
			size_t tmp = push_pos[ra].load(std::memory_order_acquire);
			do {

			} while (push_pos[ra].compare_exchange_weak(tmp, (tmp + 1) % M, std::memory_order_acq_rel));
			return tmp;
		}
		size_t pop_index(task_rank ra)noexcept {
			assert(ra + 1 <= N && "improved_multilevel_con  pop_index()函数参数ra错误");
			size_t tmp = pop_pos[ra].load(std::memory_order_acquire);
			do {

			} while (pop_pos[ra].compare_exchange_weak(tmp, (tmp + 1) % M, std::memory_order_acq_rel));
			return tmp;
		}
		void push(task_pack&& pa)override {
			task_rank ra = pa.get_rank();
			if (ra + 1 > N)throw std::invalid_argument("improved_multilevel_con :: push()task_pack的rank不能超过N");
			size_t idx = push_index(ra);
			{
				std::lock_guard<std::mutex> locker(mtx[ra][idx]);
				multi_que[ra][idx].emplace(std::move(pa));
			}
			//bool expected = false;
			//while (!access.compare_exchange_weak(expected, true, std::memory_order_acq_rel)) {
			//	expected = false;
			//}
			if (ra > cur_max_ra.load(std::memory_order_acquire))cur_max_ra.store(ra, std::memory_order_release);
			access.store(false, std::memory_order_release);
			ele_nums[ra].fetch_add(1, std::memory_order_acq_rel);
			siz.fetch_add(1, std::memory_order_acq_rel);

		}
		result<task_pack, bool> pop2()override {
			result<task_pack, bool> res;
			res.error_code = false;
			size_t tmp = cur_max_ra.load(std::memory_order_acquire);
			size_t num = ele_nums[tmp].load(std::memory_order_acquire);
			size_t try_num = 0;
			do {

				try_num++;
				if (try_num > POP_TRY_NUM)return res;
				if (num == 0) {
					update_cur_max_ra();
					tmp = cur_max_ra.load(std::memory_order_acquire);
					num = ele_nums[tmp].load(std::memory_order_acquire);
					if (tmp == 0 && num == 0)return res;
				}
			} while (!ele_nums[tmp].compare_exchange_weak(num, num - 1, std::memory_order_acq_rel));
			/*
			do {
				if (siz.load(std::memory_order_acquire) == 0)return res;
				size_t num = ele_nums[cur_max_ra.load(std::memory_order_acquire)].
					load(std::memory_order_acquire);

				do {
					if (num == 0) {
						tmp = MEANINGLESS;
						break;
					}


				} while (!ele_nums[cur_max_ra.load(std::memory_order_acquire)].
					compare_exchange_weak(num, num - 1, std::memory_order_acq_rel));

			} while (!cur_max_ra.compare_exchange_weak(
				tmp,cur_max_ra.load(std::memory_order_acquire),std::memory_order_acq_rel));
	*/
			size_t idx = 0;
			try_num = 0;
			while (true) {
				try_num++;
				if (try_num > POP_TRY_NUM) {
					ele_nums[tmp].fetch_add(1, std::memory_order_acq_rel);
					return res;
				}
				idx = pop_index(tmp);
				std::lock_guard<std::mutex> locker(mtx[tmp][idx]);
				if (!multi_que[tmp][idx].empty()) {
					res.result1 = std::move(multi_que[tmp][idx].front());
					multi_que[tmp][idx].pop();

					break;
				}

			}
			res.error_code = true;
			if (res.error_code)siz.fetch_sub(1, std::memory_order_acq_rel);
			return res;
		}
		void adjust()override {

		}
		inline bool empty()const noexcept override {
			return siz.load(std::memory_order_acquire) == 0;
		}
		inline size_t size()const noexcept override {
			return siz.load(std::memory_order_acquire);
		}
		//暂未完成
		task_pack pop()override {
			return task_pack();
		}
	public:
		improved_multilevel_con()
			:siz(0), push_pos{}, ele_nums{},
			cur_max_ra(0), access(false), pop_pos{} {}
		~improved_multilevel_con()override = default;
		improved_multilevel_con(const improved_multilevel_con&) = delete;
		improved_multilevel_con& operator=(const improved_multilevel_con&) = delete;
		improved_multilevel_con(improved_multilevel_con&&) = delete;
		improved_multilevel_con& operator=(improved_multilevel_con&&) = delete;
	};



	template<class T>
	std::unique_ptr<T> make_container() {
		return std::make_unique<T>();
	}
	using container_creator = std::function<std::unique_ptr<container>()>;
	class container_factory {
	public:
		enum class strategy { FIFO, mutilevel, longest_first, shortest_first };
		static const std::unordered_map<strategy, container_creator>& get_startegy_cast() {
			static const std::unordered_map<strategy, container_creator> cast{
			{strategy::FIFO,[]() {return make_container<FIFO_con>(); }},
			{strategy::longest_first,[]() {return make_container<longest_first_con>(); }},
			//{strategy::shortest_first,[]() {return make_container<shortest_first_con>(); }},
			{strategy::mutilevel,[]() {return make_container<multilevel_con>(); }}
			};
			return cast;

		}
		static bool  get_container(strategy stra, std::unique_ptr<container>& ptr) {
			auto& tmp = get_startegy_cast();
			auto iter = tmp.find(stra);
			if (iter == tmp.end())return false;
			ptr = std::move(iter->second());
			return true;
		}
	};
	class multi_strategies_tasks_container {
	private:
		class num_guard {
		private:
			std::atomic<int>& num;
		public:
			void add()noexcept {
				int tmp = num.load(std::memory_order_acquire);
				do {
					if (tmp < 0) {
						tmp = num.load(std::memory_order_acquire);
						continue;

					}
					if (num.compare_exchange_strong(tmp, tmp + 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
						break;
					}
				} while (true);
			}
			void sub()noexcept {
				int tmp = num.load(std::memory_order_acquire);
				do {} while (num.compare_exchange_strong(tmp, tmp - 1,
					std::memory_order_acq_rel, std::memory_order_relaxed));
			}
		public:
			num_guard(std::atomic<int>& tmp)noexcept :num(tmp) {}
			~num_guard() {}

		};
		class alignas(std::hardware_destructive_interference_size) atomic_int {
			static_assert(std::hardware_destructive_interference_size > sizeof(std::atomic<int>),
				"multi_strategies_tasks_container类中atomic_int内置类型 : padding数组长度不能为负数");
		public:
			std::atomic<int> num;
			char padding[std::hardware_destructive_interference_size - sizeof(std::atomic<int>)];
		public:
			atomic_int(int tmp)noexcept :num(tmp), padding{} {}
			atomic_int()noexcept = default;
			~atomic_int() = default;
		};
	private:
		std::unique_ptr<container> ptr;
		atomic_int num;
	public:
		result<task_pack, bool> pop2() {
			result<task_pack, bool> res;
			num_guard guard(num.num);
			guard.add();

			try {
				res = ptr->pop2();
			}
			catch (std::exception& ex) {
				guard.sub();
				throw;
			}
			guard.sub();
			return res;
		}
		void push(task_pack&& pa) {
			num_guard guard(num.num);
			guard.add();
			try {
				ptr->push(std::move(pa));
			}
			catch (std::exception& ex) {
				guard.sub();
				throw;
			}
			guard.sub();
		}
		//记得把基础容器里面加上if判断，防止pop空容器,修改抛出异常的方式
		task_pack pop() {
			num_guard guard(num.num);
			guard.add();
			task_pack pa;
			try {
				pa = std::move(ptr->pop());
			}
			catch (std::exception& ex) {
				guard.sub();
				throw;
			}
			guard.sub();
			return pa;
		}
	public:
		//multi_strategies_tasks_container() :num(0) {}
		multi_strategies_tasks_container(container_factory::strategy stra) :num(0) {
			if (!container_factory::get_container(stra, ptr)) {
				throw std::invalid_argument("multi_strategies_tasks_container :构造函数参数错误");
			}

		}
		~multi_strategies_tasks_container() = default;
	};

	//AI写的
	// 高性能分片并发队列（模板参数：分片数，默认128，可根据CPU核心数调整）
	template<size_t SHARD_NUM = 128>
	class HighPerfCon : public container {
	private:
		// 每个分片的结构：独立队列+独立锁，彻底打散竞争
		struct Shard {
			std::queue<task_pack> que;
			std::mutex mtx;
		};

		std::vector<Shard> shards;          // 分片队列（堆分配，避免栈溢出）
		std::atomic<size_t> total_size{ 0 };  // 总元素数（仅用relaxed内存序）
		const size_t shard_count;           // 实际分片数（兼容CPU核心数）

		// 高效哈希函数：将task_pack映射到分片（用rank+trynum混合哈希，打散更均匀）
		size_t hash_task(const task_pack& pa) const noexcept {
			return (pa.get_rank() * 31 + pa.get_trynum()) % shard_count;
		}

	public:
		// 构造函数：初始化分片队列（堆分配，规避栈溢出）
		HighPerfCon() : shard_count(SHARD_NUM), shards(SHARD_NUM) {
			static_assert(SHARD_NUM >= 1, "分片数不能小于1");
		}

		// 核心push：哈希到分片+细粒度锁+移动语义+relaxed原子更新
		void push(task_pack&& pa) override {
			size_t shard_idx = hash_task(pa);
			{
				std::lock_guard<std::mutex> locker(shards[shard_idx].mtx);
				shards[shard_idx].que.emplace(std::move(pa));
			}
			// 仅需原子性，无需内存同步（relaxed极致性能）
			total_size.fetch_add(1, std::memory_order_relaxed);
		}

		// 适配基类的pop（调用pop2，返回有效元素或空对象）
		task_pack pop() override {
			result<task_pack, bool> res = pop2();
			return res.error_code ? std::move(res.result1) : task_pack();
		}

		// 核心pop2：轮询分片找非空队列（负载均衡），细粒度锁
		result<task_pack, bool> pop2() override {
			result<task_pack, bool> res;
			res.error_code = false;

			// 轮询分片（从随机起始位置，避免热点分片）
			static thread_local size_t start_idx = 0;
			size_t idx = start_idx;
			for (size_t i = 0; i < shard_count; ++i) {
				idx = (idx + 1) % shard_count;
				std::lock_guard<std::mutex> locker(shards[idx].mtx);
				if (!shards[idx].que.empty()) {
					res.result1 = std::move(shards[idx].que.front());
					shards[idx].que.pop();
					res.error_code = true;
					total_size.fetch_sub(1, std::memory_order_relaxed);
					break;
				}
			}
			start_idx = idx;  // 更新下一次起始位置，负载均衡
			return res;
		}

		// 适配基类：空实现（你的测试框架不依赖此函数）
		void adjust() override {}

		// 高效empty：仅原子读取（relaxed）
		bool empty() const override {
			return total_size.load(std::memory_order_relaxed) == 0;
		}

		// 高效size：仅原子读取（relaxed）
		size_t size() const override {
			return total_size.load(std::memory_order_relaxed);
		}

		// 禁用拷贝/移动，避免并发问题
		HighPerfCon(const HighPerfCon&) = delete;
		HighPerfCon& operator=(const HighPerfCon&) = delete;
		HighPerfCon(HighPerfCon&&) = delete;
		HighPerfCon& operator=(HighPerfCon&&) = delete;
		~HighPerfCon() override = default;
	};

	// 推荐分片数：根据CPU核心数调整（比如64核心用64/128分片）
	using HighPerfCon128 = HighPerfCon<128>;
}
