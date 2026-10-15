#include "eviction_policy.h"

#include <algorithm>
#include <utility>
#include <vector>

GuidVector EvictOldestFirst::SelectVictims(std::size_t needed, const ItemStore &store,
										   const IContainerLayout & /*layout*/) const
{
	if (needed == 0)
	{
		return {};
	}

	// 腾不够就一件都不挤 —— 见接口注释:销毁了一半仍然放不下,等于白白弄丢
	// 玩家的东西。这里先用实例总数快速判掉,省一次遍历排序。
	if (store.Size() < needed)
	{
		return {};
	}

	std::vector<std::pair<Guid, const ItemComp *>> byAge;
	byAge.reserve(store.Size());
	store.ForEach([&byAge](Guid guid, const ItemComp &item)
				  { byAge.emplace_back(guid, &item); });

	// 只需要最早的 needed 个,不必全序 —— partial_sort 就够。
	// 显式依赖实例序比较器,资产淘汰不依赖 ForEach 的隐式遍历顺序。
	const auto nth = byAge.begin() + static_cast<std::ptrdiff_t>(needed);
	std::partial_sort(byAge.begin(), nth, byAge.end(),
					  [](const auto &lhs, const auto &rhs)
					  { return ItemStore::AcquiredBefore(*lhs.second, *rhs.second); });

	GuidVector victims;
	victims.reserve(needed);
	for (auto it = byAge.begin(); it != nth; ++it)
	{
		victims.push_back(it->first);
	}
	return victims;
}
