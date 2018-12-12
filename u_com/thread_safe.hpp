/*
 * thread_safe.hpp
 *
 *  Created on: 16 марта 2016 г.
 *      Author: coodi
 */

#pragma once

#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>

namespace thread_safe {
    using std::mutex;

    using std::lock_guard;
    using std::unique_lock;
    using std::shared_ptr;
    using std::condition_variable;
    using std::make_shared;
    using std::queue;

	template<class T>
	class threadsafe_queue {
	private:
		mutable mutex mut;
		queue <T> data_queue;
		condition_variable data_cond;
	public:
		threadsafe_queue() {};

		threadsafe_queue(threadsafe_queue const &other) {
			unique_lock <mutex> lock(other.mut);
			data_queue = other.data_queue;
		}

		void push(T &new_value) {
			{
				lock_guard <mutex> lock(mut);
				data_queue.push(new_value);
			}

			data_cond.notify_one();
		}
        void push(T new_value) {
            {
                lock_guard <mutex> lock(mut);
                data_queue.push(new_value);
            }

            data_cond.notify_one();
        }

		void wait_and_pop(T &value) {
			unique_lock <mutex> lock(mut);
			data_cond.wait(lock, [this] { return !data_queue.empty(); });
			value = data_queue.front();
			data_queue.pop();
		}

		shared_ptr <T> wait_and_pop() {
			unique_lock <mutex> lock(mut);
			data_cond.wait(lock, [this] { return !data_queue.empty(); });
			shared_ptr <T> res(make_shared<T>(data_queue.front()));
			data_queue.pop();
			return res;
		}

		bool try_pop(T &value) {
			lock_guard <mutex> lock(mut);
			if (data_queue.empty())
				return false;
			value = data_queue.front();
			data_queue.pop();
			return true;
		}

		shared_ptr <T> try_pop() {
			lock_guard <mutex> lock(mut);
			if (data_queue.empty())
				return shared_ptr<T>();
			shared_ptr <T> res(make_shared<T>(data_queue.front()));
			data_queue.pop();
			return res;
		}

		bool empty() const {
			lock_guard <mutex> lock(mut);
			return data_queue.empty();
		}

		int size() const {
			lock_guard <mutex> lock(mut);
			return data_queue.size();
		}
	};
}

