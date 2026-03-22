#pragma once
#include <boost/pool/object_pool.hpp>
#include <boost/container/deque.hpp>

template<typename T>
class FastQueue {
public:
	using Ptr = T*;

	Ptr enqueue(const T& value) {
		Ptr ptr = pool.construct(value);
		queue.push_back(ptr);
		return ptr;
	}

	Ptr dequeue() {
		if (queue.empty()) return nullptr;
		Ptr ptr = queue.front();
		queue.pop_front();
		return ptr;
	}

	Ptr front() const {
		return queue.empty() ? nullptr : queue.front();
	}

	void clear() {
		queue.clear();
		// To reclaim all pool memory (use with caution):
		// pool.purge_memory();
	}

	size_t size() const { return queue.size(); }
	bool empty() const { return queue.empty(); }

private:
	boost::object_pool<T> pool;
	boost::container::deque<Ptr> queue;
};
