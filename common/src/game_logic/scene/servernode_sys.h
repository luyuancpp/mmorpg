#ifndef COMMON_SRC_SCENE_SERVER_NODE_H_
#define COMMON_SRC_SCENE_SERVER_NODE_H_

#include "entt/src/entt/entity/registry.hpp"

static const std::size_t kMaxServerPlayerSize = 2000;
static const std::size_t kMaxScenePlayerSize = 1000;

struct EnterSceneParam
{
    entt::entity scene_{ entt::null };
    entt::entity enterer_{ entt::null };
    uint32_t op_{ 0 };
};

struct LeaveSceneParam
{
    entt::entity leave_entity_{ entt::null };
    uint32_t op_{ 1 };
};

struct GetSceneParam
{
    uint32_t scene_confid_{0};
    uint32_t op_{ 0 };
};

struct ServerPressureParam
{
    entt::entity server_entity_{ entt::null };
};

struct ServerCrashParam
{
    entt::entity crash_entity_{ entt::null };
};

struct ReplaceCrashServerParam
{
    entt::entity cransh_server_entity_{ entt::null };
    entt::entity replace_server_entity_{ entt::null };
};

struct MaintainServerParam
{
    entt::entity maintain_entity_{ entt::null };
};

struct CompelChangeSceneParam
{
    entt::entity compel_change_entity_{ entt::null };
    entt::entity new_server_entity_{ entt::null };
    uint32_t scene_confid_{0};
};

class ServerNodeSystem
{
public:

	static entt::entity GetWeightRoundRobinMainScene(const GetSceneParam& param);

    static  entt::entity GetWeightRoundRobinRoomScene(const GetSceneParam& param);

    static  entt::entity GetMainSceneNotFull(const GetSceneParam& param);

    static  void ServerEnterPressure(entt::registry& reg, const ServerPressureParam& param);

    static  void ServerEnterNoPressure(entt::registry& reg, const ServerPressureParam& param);

    static  void ServerCrashed(entt::registry& reg, const ServerCrashParam& param);

    static  void ServerMaintain(entt::registry& reg, const MaintainServerParam& param);
};

#endif//COMMON_SRC_SCENE_SERVER_NODE_H_
