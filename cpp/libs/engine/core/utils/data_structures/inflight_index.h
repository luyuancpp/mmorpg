#pragma once

// 在途调用索引:按调用号认领,保证每个调用恰好被处理一次;另带一个按截止时刻排序的过期堆。
//
// 「恰好一次」的判据只有一个:Extract / ExtractExpired / ExtractIf / ExtractAll 能否把条目从索引里取出来。
// 取到的那条路径负责完成,其余路径(迟到的回包、迟到的超时、已经清空的断连)一律取不到、什么都不做。
// 不依赖定时器取消,也不依赖「回包与超时谁先到」(docs/design/muduo-timer-cancellation-hazards.md)。
//
// 过期堆是惰性的:完成时不从堆里删,ExtractExpired 弹到已不在索引里的号就跳过。代价是堆里残留
// 「速率 × 超时」条已完成的记录(16 字节一条),换来每次完成少一次分配与一次树操作。
//
// 只依赖 STL、不读时钟(时刻由调用方传入),所以单测可以脱离引擎直接编译。不加锁:调用方保证只在一个线程上读写。
// 使用方:RpcChannel::outstandings_(docs/design/muduo-rpc-channel.md §4.4)、
//         gate 路由 lane 的在途索引(docs/design/client-request-ordering.md §4.5,原先就定义在 router_lane.h)。

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

using InflightSteadyTime = std::chrono::steady_clock::time_point;

// 调用号。0 保留,表示「不受索引管理」,Insert 一律拒绝。
using InflightCallId = uint64_t;
inline constexpr InflightCallId kNoInflightCallId = 0;

// Call 由调用方定义,必须有 `InflightSteadyTime expiresAt` 成员。
template <typename Call>
class InflightIndex
{
public:
	// 登记在途调用。id 必须非 0 且不重复,否则返回 false、不登记。
	bool Insert(InflightCallId id, Call call)
	{
		if (id == kNoInflightCallId)
		{
			return false;
		}
		const InflightSteadyTime expiresAt = call.expiresAt;
		if (!calls_.emplace(id, std::move(call)).second)
		{
			return false;
		}
		expiry_.emplace(expiresAt, id);
		return true;
	}

	// 认领并注销。找不到(已完成、已过期、已被批量取走,或根本不是本索引登记的)返回 nullopt。
	std::optional<Call> Extract(InflightCallId id)
	{
		auto it = calls_.find(id);
		if (it == calls_.end())
		{
			return std::nullopt;
		}
		Call call = std::move(it->second);
		calls_.erase(it);
		return call;
	}

	// 只注销、不需要返回值的场景。返回是否找到。
	bool Erase(InflightCallId id) { return calls_.erase(id) > 0; }

	// 取出全部 expiresAt <= now 的调用(按截止时刻先后)。堆里已注销的号惰性跳过。
	std::vector<std::pair<InflightCallId, Call>> ExtractExpired(InflightSteadyTime now)
	{
		std::vector<std::pair<InflightCallId, Call>> expired;
		while (!expiry_.empty() && expiry_.top().first <= now)
		{
			const InflightCallId id = expiry_.top().second;
			expiry_.pop();
			auto call = Extract(id);
			if (call)
			{
				expired.emplace_back(id, std::move(*call));
			}
		}
		return expired;
	}

	// 取出全部满足 pred(call) 的调用。O(在途数),只用于很少发生的批量场景(如节点摘除)。
	// 堆里对应的条目留着,弹出时因已不在索引里而被跳过。
	template <typename Pred>
	std::vector<std::pair<InflightCallId, Call>> ExtractIf(Pred pred)
	{
		std::vector<std::pair<InflightCallId, Call>> matched;
		for (auto it = calls_.begin(); it != calls_.end();)
		{
			if (pred(it->second))
			{
				matched.emplace_back(it->first, std::move(it->second));
				it = calls_.erase(it);
			}
			else
			{
				++it;
			}
		}
		return matched;
	}

	// 取出全部调用并清空过期堆(断连、通道关闭时用)。返回顺序不保证。
	std::vector<std::pair<InflightCallId, Call>> ExtractAll()
	{
		std::vector<std::pair<InflightCallId, Call>> all;
		all.reserve(calls_.size());
		for (auto &entry : calls_)
		{
			all.emplace_back(entry.first, std::move(entry.second));
		}
		calls_.clear();
		ExpiryHeap{}.swap(expiry_);
		return all;
	}

	bool Empty() const { return calls_.empty(); }
	std::size_t Size() const { return calls_.size(); }
	// 堆里可能残留已注销的号,这个数只用于观测(汇总日志),不参与判定。
	std::size_t ExpiryHeapSize() const { return expiry_.size(); }

private:
	using ExpiryEntry = std::pair<InflightSteadyTime, InflightCallId>;
	using ExpiryHeap = std::priority_queue<ExpiryEntry, std::vector<ExpiryEntry>, std::greater<ExpiryEntry>>;
	std::unordered_map<InflightCallId, Call> calls_;
	ExpiryHeap expiry_;
};
