﻿#pragma once

#include <map>
#include <set>
#include <functional> 
#include <algorithm>
#include "xxhash.h"
//https://github.com/ioriiod0/consistent_hash/blob/master/test.cpp
//https://zhuanlan.zhihu.com/p/482549860


inline uint64_t StableHash64(const std::string& data) {
	return XXH64(data.data(), data.size(), 0); // seed 可以设为固定值
}

inline uint64_t StableHash64(uint32_t val) {
	return XXH64(&val, sizeof(val), 0);
}

inline uint64_t StableHash64(uint64_t val) {
	return XXH64(&val, sizeof(val), 0);
}

template <typename Key,
    typename Value>
class ConsistentHashNode
{
public:

    using NodeListType = std::map<Key, Value>;
    
public:
    std::size_t size() const { return nodes_.size(); }

    bool empty() const { return nodes_.empty(); }

    decltype(auto) add(Key id, const Value& node)
    {
        auto hash_id = std::hash<Key>{}(id);
        return nodes_.emplace(hash_id, node); 
    }

    void remove(Key id)
    {
        auto hash_id = std::hash<Key>{}(id);
        nodes_.erase(hash_id);
    }

    decltype(auto) GetNodeValue(Key id)
    {
        auto hash_id = std::hash<Key>{}(id);
        return nodes_.find(hash_id);
    }

    decltype(auto) GetByHash(Key id)
    {
        auto hash_id = std::hash<Key>{}(id);
        if (nodes_.empty()) {
            return nodes_.end();
        }

        // todo 这里在该节点前面加的话节点就变了
        auto it = nodes_.lower_bound(hash_id);
        if (it == nodes_.end()) {
            it = nodes_.begin();
        }

        return it;
    }

    decltype(auto) begin() { return nodes_.begin(); }
    decltype(auto) end() { return nodes_.end(); }

private:
    NodeListType nodes_;
};

