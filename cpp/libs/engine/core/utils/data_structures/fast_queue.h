#pragma once
#include <boost/pool/object_pool.hpp>
#include <boost/container/deque.hpp>

template<typename T>
class FastQueue {
public:
	using Ptr = T*;

	// 入队：构造一个元素并加入队列
	Ptr enqueue(const T& value) {
		Ptr ptr = pool.construct(value);  // 从池中分配并构造
		queue.push_back(ptr);
		return ptr;
	}

	// 出队：移除并返回第一个元素指针
	Ptr dequeue() {
		if (queue.empty()) return nullptr;
		Ptr ptr = queue.front();
		queue.pop_front();
		return ptr;
	}

	// 查看队头但不移除
	Ptr front() const {
		return queue.empty() ? nullptr : queue.front();
	}

	void clear() {
		queue.clear();
		// 如果确实想回收所有内存（慎用）：
		// pool.purge_memory();
	}

	size_t size() const { return queue.size(); }
	bool empty() const { return queue.empty(); }

private:
	boost::object_pool<T> pool;
	boost::container::deque<Ptr> queue;
};
