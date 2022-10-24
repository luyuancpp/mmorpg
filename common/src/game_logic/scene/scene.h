#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "src/game_logic/game_registry.h"
#include "src/util/snow_flake.h"
#include "src/game_logic/scene/servernode_system.h"

#include "src/game_logic/comp/scene_comp.h"

#include "component_proto/scene_comp.pb.h"

struct EnterSceneParam
{
	entt::entity scene_{ entt::null };
	entt::entity enterer_{ entt::null };
};

struct LeaveSceneParam
{
	entt::entity leaver_{ entt::null };
};

struct CreateSceneP
{
    uint32_t scene_confid_{ 0 };
};

struct CreateSceneBySceneInfoP
{
    SceneInfo scene_info_;
};

struct CreateGsSceneP
{
    entt::entity node_{ entt::null };
    uint32_t scene_confid_{ 0 };
};

struct MoveServerScene2ServerSceneP
{
    entt::entity from_server_{ entt::null };
    entt::entity to_server_{ entt::null };
};

struct DestroySceneParam
{
    entt::entity scene_{ entt::null };
    entt::entity server_{ entt::null };
};

struct DestroyServerParam
{
    entt::entity server_{ entt::null };
};


entt::entity CreateMainSceneNode();
void AddMainSceneNodeCompnent(entt::entity e);

class ScenesSystem : public EntityPtr
{
public:

    static ScenesSystem& GetSingleton() { thread_local ScenesSystem singleton; return singleton; }

    SceneComp copy_scenes_id() { return scenes_; }
    std::size_t scenes_size(uint32_t scene_config_id)const;
    std::size_t scenes_size() const { return scenes_.size(); }
    std::size_t scenes_map_size() const { return scenes_.size(); }
    void set_server_squence_node_id(uint32_t node_id) { server_squence_.set_node_id(node_id); }

    entt::entity get_scene(Guid scene_id);

    bool HasScene(uint32_t scene_config_id);
    inline bool Empty() const { return scenes_.empty(); }

    uint32_t CheckScenePlayerSize(entt::entity scene);

    entt::entity CreateScene(const CreateSceneP& param);
    entt::entity CreateSceneByGuid(const CreateSceneBySceneInfoP& param);
    entt::entity CreateScene2Gs(const CreateGsSceneP& param);

    void DestroyScene(const DestroySceneParam& param);
    void DestroyServer(const DestroyServerParam& param);

    void MoveServerScene2ServerScene(const MoveServerScene2ServerSceneP& param);

    void EnterScene(const EnterSceneParam& param);

    void LeaveScene(const LeaveSceneParam& param);

    void CompelToChangeScene(const CompelChangeSceneParam& param);

    void ReplaceCrashServer(const ReplaceCrashServerParam& param);

    void LogPlayerEnterScene(entt::entity player);
    void LogPlayerLeaveScene(entt::entity player);
private:

    SceneComp scenes_;
    ServerSequence24 server_squence_;
};

