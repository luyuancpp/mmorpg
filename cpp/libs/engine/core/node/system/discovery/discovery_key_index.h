#pragma once

// 服务发现键索引(docs/design/grpc-muduo-transport.md §13.5):etcd 发现键 → 最近一次已知的值与 mod_revision,
// 外加删除墓碑。用途:
//   * DELETE 事件不依赖 prev_kv(开 MVCC 压缩后可能拿不到):按 key 取回删除前的 NodeInfo JSON;
//   * 对账:前缀的完整 Range 快照里没有、且本地记录不比快照新的 key,视为流断期间被删;
//   * 丢掉过时的条目:Range 与 watch 走不同的流,快照可能比已经处理过的 watch 事件旧,反之亦然。
//
// 只收发现键(`<Service>.rpc/zone/...`),不收 `/allocated/` 占位键(值是裸 uuid,不是发现数据)。
// 纯数据结构:不碰全局状态、不打日志;只在节点 loop 线程使用。

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

class DiscoveryKeyIndex
{
public:
    struct Removed
    {
        std::string key;
        std::string value;
    };

    enum class DeleteResult : uint8_t
    {
        kRemoved,     // 有记录,已移除;value 是删除前的值
        kUnknownKey,  // 本地没有记录(只记墓碑)
        kStale,       // 本地记录比这次删除还新:删除事件过时,索引不动
    };

    struct DeleteOutcome
    {
        DeleteResult result{DeleteResult::kUnknownKey};
        std::string value;
    };

    // key 在 modRevision 处的值。比本地已知状态旧(本地有更新的 PUT,或不早于它的删除墓碑)返回 false,
    // 调用方应忽略这一条;同一版本重复到达(每 2s 的全量 Range)返回 true。
    bool ApplyPut(const std::string &key, const std::string &value, int64_t modRevision)
    {
        if (const auto tomb = tombstones_.find(key); tomb != tombstones_.end())
        {
            if (modRevision <= tomb->second)
            {
                return false;
            }
            tombstones_.erase(tomb);
        }
        const auto [it, inserted] = entries_.try_emplace(key);
        if (!inserted && modRevision < it->second.modRevision)
        {
            return false;
        }
        it->second.value = value;
        it->second.modRevision = modRevision;
        return true;
    }

    // key 在 revision(删除事件的 kv.mod_revision)处被删除。
    DeleteOutcome ApplyDelete(const std::string &key, int64_t revision)
    {
        DeleteOutcome outcome;
        const auto it = entries_.find(key);
        if (it != entries_.end() && it->second.modRevision > revision)
        {
            outcome.result = DeleteResult::kStale;
            return outcome;
        }
        auto &tomb = tombstones_[key];
        tomb = std::max(tomb, revision);
        if (it == entries_.end())
        {
            outcome.result = DeleteResult::kUnknownKey;
            return outcome;
        }
        outcome.result = DeleteResult::kRemoved;
        outcome.value = std::move(it->second.value);
        entries_.erase(it);
        return outcome;
    }

    // 前缀的 Range 快照:revision = snapshotRevision(Range 应答的 header.revision),snapshotKeys = 快照里的全部发现键。
    //   * 总是清掉该前缀下 ≤ snapshotRevision 的墓碑(快照已经反映了那些删除);
    //   * complete 时再移除本地有、快照里没有、且 modRevision ≤ snapshotRevision 的记录并返回(按 key 有序),
    //     同时在 snapshotRevision 处给它们记墓碑,挡住之后才到的、更旧的 PUT。
    //     modRevision > snapshotRevision 的是快照之后经 watch 才到的新键,不动。
    // complete = false(more=true / 值解析失败 / 有 key 不属于前缀)时一律不删 —— 宁可多留,不错删。
    std::vector<Removed> ApplySnapshot(std::string_view prefix, const std::unordered_set<std::string> &snapshotKeys,
                                       int64_t snapshotRevision, bool complete)
    {
        for (auto it = tombstones_.lower_bound(prefix); it != tombstones_.end() && it->first.starts_with(prefix);)
        {
            it = it->second <= snapshotRevision ? tombstones_.erase(it) : std::next(it);
        }
        std::vector<Removed> removed;
        if (!complete)
        {
            return removed;
        }
        for (auto it = entries_.lower_bound(prefix); it != entries_.end() && it->first.starts_with(prefix);)
        {
            if (it->second.modRevision <= snapshotRevision && !snapshotKeys.contains(it->first))
            {
                removed.push_back({it->first, std::move(it->second.value)});
                tombstones_[it->first] = snapshotRevision;
                it = entries_.erase(it);
                continue;
            }
            ++it;
        }
        return removed;
    }

    // 没有记录返回 nullptr;指针在下一次修改索引前有效。
    const std::string *FindValue(const std::string &key) const
    {
        const auto it = entries_.find(key);
        return it == entries_.end() ? nullptr : &it->second.value;
    }

    std::size_t size() const { return entries_.size(); }
    std::size_t tombstoneCount() const { return tombstones_.size(); }

private:
    struct Entry
    {
        std::string value;
        int64_t modRevision{0};
    };

    // 有序容器:前缀对账用 lower_bound 扫一段。
    std::map<std::string, Entry, std::less<>> entries_;
    std::map<std::string, int64_t, std::less<>> tombstones_;
};
