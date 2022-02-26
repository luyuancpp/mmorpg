#ifndef MASTER_SRC_SYS_SCENE_SYS_HPP_
#define MASTER_SRC_SYS_SCENE_SYS_HPP_

#include "entt/src/entt/entity/registry.hpp"

namespace master
{

static std::size_t kMaxServerPlayerSize = 2000;
static std::size_t kMaxScenePlayerSize = 1000;

struct EnterSceneParam
{
    entt::entity scene_entity_{ entt::null };
    entt::entity enter_entity_{ entt::null };
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

	entt::entity GetWeightRoundRobinMainScene(const GetSceneParam& param);

	entt::entity GetWeightRoundRobinRoomScene(const GetSceneParam& param);

    entt::entity GetMainSceneNotFull(const GetSceneParam& param);

	void ServerEnterPressure(entt::registry& reg, const ServerPressureParam& param);

	void ServerEnterNoPressure(entt::registry& reg, const ServerPressureParam& param);

	void ServerCrashed(entt::registry& reg, const ServerCrashParam& param);

	void ServerMaintain(entt::registry& reg, const MaintainServerParam& param);
};


}//namespace master

#endif//MASTER_SRC_SCENE_SYS_SCENE_SYS_HPP_
