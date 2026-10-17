#pragma once

#include "engine/core/type_define/player_guid_comp.h"
#include "thread_context/ecs_context.h"
#include "muduo/base/LogStream.h"

// 日志标识 actor 的唯一写法，同时记录业务玩家号与完整句柄；两者不能冒充或相互截断。
struct ActorLogTag
{
    Guid playerId{0};
    entt::entity entity{entt::null};
};

[[nodiscard]] inline ActorLogTag ActorLog(entt::entity actor)
{
    return {tlsEcs.GetPlayerId(actor), actor};
}

inline muduo::LogStream& operator<<(muduo::LogStream& stream, const ActorLogTag& actor)
{
    return stream << "player_id=" << actor.playerId << " entity="
                  << static_cast<uint64_t>(entt::to_integral(actor.entity));
}
