#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

static const std::size_t kMaxServerPlayerSize = 2000;
static const std::size_t kMaxScenePlayerSize = 1000;

struct EnterSceneParam
{
    entt::entity scene_{ entt::null };
    entt::entity enterer_{ entt::null };
};

struct CheckEnterSceneParam
{
	Guid scene_id_{kInvalidGuid };
	entt::entity player_{ entt::null };
};

struct LeaveSceneParam
{
    entt::entity leave_player_{ entt::null };
};

struct GetSceneParam
{
    uint32_t scene_confid_{0};
};

struct ServerPressureParam
{
    entt::entity server_{ entt::null };
};

struct ServerCrashParam
{
    entt::entity crash_entity_{ entt::null };
};

struct ReplaceCrashServerParam
{
    entt::entity cransh_server_{ entt::null };
    entt::entity replace_server_{ entt::null };
};

struct MaintainServerParam
{
    entt::entity maintain_entity_{ entt::null };
};

struct CompelChangeSceneParam
{
    entt::entity compel_change_entity_{ entt::null };
    entt::entity new_server_{ entt::null };
    uint32_t scene_confid_{0};
};

class ServerNodeSystem
{
public:

    static ServerNodeSystem& GetSingleton() { thread_local ServerNodeSystem singleton; return singleton; }

	static entt::entity GetWeightRoundRobinMainScene(const GetSceneParam& param);

    static  entt::entity GetWeightRoundRobinRoomScene(const GetSceneParam& param);

    static  entt::entity GetMainSceneNotFull(const GetSceneParam& param);

    static  void ServerEnterPressure(entt::registry& reg, const ServerPressureParam& param);

    static  void ServerEnterNoPressure(entt::registry& reg, const ServerPressureParam& param);

    static  void ServerCrashed(entt::registry& reg, const ServerCrashParam& param);

    static  void ServerMaintain(entt::registry& reg, const MaintainServerParam& param);
};

