#pragma once

#include <string>
#include <string_view>

#include "entt/src/entt/entity/registry.hpp"

// 排障只读遍历全部组件池，代价 O(池数)，只供 GM 与低频故障路径使用。
// 只列类型，不记录组件值；陈旧句柄和空句柄不会创建池或读取同槽位的新实体。
template <typename Registry>
std::string DescribeEntity(const Registry& registry, typename Registry::entity_type entity)
{
    std::string out = "entity=" + std::to_string(entt::to_integral(entity));
    if (!registry.valid(entity))
    {
        return out + " valid=0";
    }
    out += " valid=1 components=[";
    bool first = true;
    for (auto [id, storage] : registry.storage())
    {
        if (!storage.contains(entity))
        {
            continue;
        }
        // 主干 EnTT 4 使用 info，本机尚在使用的 EnTT 3 构建使用 type。
        // 让分支依赖 Storage，未选中的 API 不参与实例化。
        std::string_view name = []<typename Storage>(const Storage& value) -> std::string_view {
            if constexpr (requires { value.info().name(); })
            {
                return value.info().name();
            }
            else
            {
                return value.type().name();
            }
        }(storage);
        for (std::string_view prefix : {"struct ", "class ", "enum "})
        {
            if (name.starts_with(prefix))
            {
                name.remove_prefix(prefix.size());
                break;
            }
        }
        out += first ? "" : ",";
        out += name;
        first = false;
    }
    return out + "]";
}
