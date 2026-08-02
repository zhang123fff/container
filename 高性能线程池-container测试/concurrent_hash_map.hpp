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
#include<optional>

namespace con {
	/// <summary>
	/// /////////////// /////////concurrent_hash_map     非task_pack特化////////////////////////////////////////
	/// </summary>
	template<typename Key, typename Value, typename Hash = std::hash<Key>>
	class concurrent_hash_map {
	private:
		using unique_shared_mtx_lock = std::unique_lock<std::shared_mutex>;
		using shared_shared_mtx_lock = std::shared_lock<std::shared_mutex>;
	private:
		class bucket {
		public:
			std::unordered_map<Key, Value, Hash> map;
			mutable std::shared_mutex mtx;
		};
	public:
		
		class read_handle {
		private:
			const Value& val;
			shared_shared_mtx_lock locker;
		public:
			const Value& get() const { 
				return val; 
			}
		public:
			read_handle(std::shared_mutex& mtx, const Value& val) 
				: locker(mtx), val(val) {}
			//注意不要传入一个没有获取锁资源的locker
			read_handle(shared_shared_mtx_lock&& loc, const Value& val)
				: locker(std::move(loc)), val(val) {}
			};

		class write_handle {
		private:
			Value& val;
			unique_shared_mtx_lock locker;
		public:
			Value& get() { 
				return val; 
			}
		public:
			write_handle(std::shared_mutex& mtx, Value& val)
				: locker(mtx), val(val) {}
			write_handle(unique_shared_mtx_lock&& loc, Value& val)
				: locker(std::move(loc)), val(val) {}
		};
	private:
		Hash hash;
		const size_t num_buckets;
		std::vector<bucket> buckets;
	private:
		bucket& get_bucket(const Key& key) {
			size_t idx = hash(key) % num_buckets;
			return buckets[idx];
		}

		const bucket& get_bucket(const Key& key) const {
			size_t idx = hash(key) % num_buckets;
			return buckets[idx];
		}
	public:
		bool insert(Key&& key, Value&& val) {
			auto& buc = get_bucket(key);
			std::unique_lock<std::shared_mutex> locker(buc.mtx);
			auto res = buc.map.emplace(std::move(key), std::move(val));
			return res.second;
		}

		bool insert(const Key& key, const Value& val) {
			auto& buc = get_bucket(key);
			std::unique_lock<std::shared_mutex> locker(buc.mtx);
			auto res = buc.map.emplace(key, val);
			return res.second;
		}

		//read_find无修改版本，调用read_find之后必须
		// 释放read_handle之后才能调用write_find，否则会死锁
		std::optional<read_handle> read_find(const Key& key)const {
			auto& buc = get_bucket(key);
			shared_shared_mtx_lock locker(buc.mtx);
			const auto it = std::as_const(buc.map).find(key);
			if(it == buc.map.cend())return std::nullopt;

			std::optional<read_handle> opt(std::in_place, std::move(locker), (*it).second);
			return opt;
		}

		std::optional<write_handle> write_find(const Key& key) {
			auto& buc = get_bucket(key);
			unique_shared_mtx_lock locker(buc.mtx);
			auto it = buc.map.find(key);
			if(it == buc.map.end())return std::nullopt;

			std::optional<write_handle> opt(std::in_place, std::move(locker), (*it).second);
			return opt;
		}

		bool erase(const Key& key) {
			auto& buc = get_bucket(key);
			std::unique_lock<std::shared_mutex> locker(buc.mtx);
			return buc.map.erase(key);
		}
	public:
		concurrent_hash_map(size_t num_buckets = 16) 
			: num_buckets(num_buckets), buckets(num_buckets) {
			if(num_buckets == 0)throw std::invalid_argument("num_buckets must be greater than 0");
		}
		concurrent_hash_map(std::vector<std::pair<Key, Value>>&& vec, size_t num_buckets = 16)
			: concurrent_hash_map(num_buckets){
			for (auto&& i : vec) {
				insert(std::move(i.first), std::move(i.second));
			}
		}
		concurrent_hash_map(std::initializer_list<std::pair<Key, Value>> list, size_t num_buckets = 16)
			: concurrent_hash_map(num_buckets) {
			for (const auto& i : list) {
				insert(i.first, i.second);
			}
		}
		~concurrent_hash_map() = default;

		concurrent_hash_map(const concurrent_hash_map&) = delete;
		concurrent_hash_map& operator=(const concurrent_hash_map&) = delete;
		//禁止移动构造和移动赋值
		concurrent_hash_map(concurrent_hash_map&&) = delete;
		concurrent_hash_map& operator=(concurrent_hash_map&&) = delete;


	};


}