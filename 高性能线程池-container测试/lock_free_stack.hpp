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
	template<typename T>
	class lock_free_stack {

        //静态断言限制T
        static_assert(std::is_nothrow_move_constructible_v<T>,
            "T must have noexcept move constructor");
        static_assert(std::is_nothrow_move_assignable_v<T>,
            "T must have noexcept move assignment");
	private:
        //next被覆盖导致的内存泄露交给用户处理
        struct node {
            T data;
            node* next;

            node() : next(nullptr) {}

            node(const T& da, node* ptr) :data(da), next(ptr) {}

            node(T&& da, node* ptr) :data(std::move(da)), next(ptr) {}

            /*
                        node(const node& other)
                            : data(other.data), next(other.next) {}

                        node(node&& other) noexcept
                            : data(std::move(other.data)), next(std::move(other.next)) {
                            other.next = nullptr;
                        }

                        node& operator=(const node& other) {
                            if (this != &other) {
                                data = other.data;
                                delete next;
                                next = other.next;
                            }
                            return *this;
                        }

                        node& operator=(node&& other) noexcept {
                            if (this != &other) {
                                data = std::move(other.data);
                                delete next;
                                next = std::move(other.next);
                                other.next = nullptr;
                            }
                            return *this;
                        }

                        ~node() {
                            delete next;
                        }
            */
        };

        class deferred_reclamation {
        private:
            //结尾节点next必须是nullptr
            std::atomic<node*> to_be_deleted{nullptr};
            std::atomic<size_t> num_in_pop{0};
        private:
            //暂定
            void add_to_be_deleted() {

            }

            //为了效率，不检查该链元素大于1或者等于0，
            // 请确保ptr指向合法的空间
            void single_push(node* ptr) {
                node* cur = nullptr;
                do {
                    cur = to_be_deleted.load(std::memory_order_acquire);
                    ptr->next = cur;

                } while (!to_be_deleted.compare_exchange_weak(cur, ptr,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed));

            }
            
            //为了效率，不检查该链元素等于0，
            // 请确保head,tail参数正确，且指向合法的空间
            void batch_push(node* head, node* tail) {
                node* cur = nullptr;
                do {
                    cur = to_be_deleted.load(std::memory_order_acquire);
                    tail->next = cur;

                } while (!to_be_deleted.compare_exchange_weak(cur, head,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed));
            }

            bool check_num_in_pop()const {
                return num_in_pop.load(std::memory_order_acquire) == 1;
            }
        public:
            static void delete_nodes(node* ptr) {
                node* tmp = nullptr;
                while (ptr != nullptr) {
                    tmp = ptr;
                    ptr = ptr->next;
                    delete tmp;
                }
            }

            static void delete_single_node(node* ptr) {
                delete ptr;
            }

            //head不能是野指针
            static node* get_tail(node* head) {
                node* tmp = nullptr;
                while (head != nullptr) {
                    tmp = head;
                    head = head->next;
                }
                return tmp;
            }

            //ptr不能为空,且链长为1,结尾节点next必须是nullptr
            bool try_reclaim(node* ptr) {
                assert(ptr != nullptr && "ptr cannot be null");
                
                if (check_num_in_pop()) {
                    node* tmp = nullptr;
                    
                    tmp = to_be_deleted.exchange(tmp, std::memory_order_acq_rel);
                    if (check_num_in_pop()) {
                        delete_nodes(tmp); 
                    }
                    else {
                        if (tmp != nullptr)batch_push(tmp, get_tail(tmp));
                    }
                    delete_single_node(ptr);
                    return true;
                }
                else {
                    single_push(ptr);
                    return false;
                }
            }

            void num_in_pop_increase(size_t num) {
                num_in_pop.fetch_add(num, std::memory_order_acq_rel);
            }

            void num_in_pop_decrease(size_t num) {
                num_in_pop.fetch_sub(num, std::memory_order_acq_rel);
            }
        public:
            deferred_reclamation() : to_be_deleted(nullptr), num_in_pop(0){}

            //移动拷贝赋值默认delete

            ~deferred_reclamation() {
                delete_nodes(to_be_deleted.load(std::memory_order_acquire));
            }
        };
    private:
        deferred_reclamation def;
        std::atomic<node*> head;
        std::atomic<size_t> count;
    public:
        bool batch_push() {
            //暂未实现
        }

        bool try_push(T&& others) {
            //暂未实现
        }

        bool try_pop(T& others) {
            //暂未实现
        }

        bool push(T&& others) {
            node* cur = nullptr;
            node* tmp = new node(std::move(others), nullptr);
            do {
                cur = head.load(std::memory_order_acquire);
                tmp->next = cur;
            
            } while (!head.compare_exchange_weak(cur, tmp,
                std::memory_order_acq_rel,
                std::memory_order_relaxed));
            count.fetch_add(1, std::memory_order_release);
            return true;
        }

        bool pop(T& others) {
            node* cur = nullptr;
            def.num_in_pop_increase(1);

            do {
                cur = head.load(std::memory_order_acquire);
                if (cur == nullptr) {
                    def.num_in_pop_decrease(1);
                    return false;
                }

            } while (!head.compare_exchange_weak(cur, cur->next,
                std::memory_order_acq_rel,
                std::memory_order_relaxed));

            others = std::move(cur->data);
            cur->next = nullptr;
            def.try_reclaim(cur);
            def.num_in_pop_decrease(1);
            count.fetch_sub(1, std::memory_order_release);
            return true;
        }

        //近似值,不准确
        size_t size()const {
            return count.load(std::memory_order_acquire);
        }

        bool empty()const {
            return count.load(std::memory_order_acquire) == 0;
        }
    public:
        lock_free_stack() :head(nullptr), count(0) {}

        lock_free_stack(const lock_free_stack&) = delete;
        lock_free_stack& operator=(const lock_free_stack&) = delete;
        lock_free_stack(lock_free_stack&&) = delete;
        lock_free_stack& operator=(lock_free_stack&&) = delete;

        ~lock_free_stack() {
            deferred_reclamation::delete_nodes(head.load(std::memory_order_acquire));
        }

	};







}
