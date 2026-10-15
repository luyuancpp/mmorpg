#pragma once

#include "engine/core/type_define/player_guid_comp.h"
#include "thread_context/ecs_context.h"
#include "muduo/base/LogStream.h"

// 日志中统一同时标识业务玩家号与完整 actor 句柄;两者不能冒充或相互截断。
struct ActorLogTag
{
    Guid playerId{0};
    entt::entity entity{entt::null};
};

[[nodiscard]] inline ActorLogTag ActorLog(entt::entity actor)
{
    Guid playerId = 0;
    if (tlsEcs.actorRegistry.valid(actor))
    {
        if (const auto* guid = tlsEcs.actorRegistry.try_get<PlayerGuidComp>(actor))
        {
            playerId = guid->value;
        }
    }
    return {playerId, actor};
}

inline muduo::LogStream& operator<<(muduo::LogStream& stream, const ActorLogTag& actor)
{
    return stream << "player_id=" << actor.playerId << " entity="
                  << static_cast<uint64_t>(entt::to_integral(actor.entity));
}
