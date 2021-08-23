#ifndef MASTER_SRC_FACTORIES_SCENE_FACTORIES_HPP_
#define MASTER_SRC_FACTORIES_SCENE_FACTORIES_HPP_

#include "entt/src/entt/entity/registry.hpp"

#include "src/game_logic/comp/server_list.hpp"

namespace master
{
    entt::entity MakeMainSceneMap(entt::registry& reg);

    struct MakeMainSceneParam
    {
        entt::entity scene_map_entity_{};
        uint32_t scene_config_id_{ 0 };
        entt::entity server_id_{};
        uint32_t op_{ 0 };
    };

    entt::entity MakeMainScene(entt::registry& reg,
        const MakeMainSceneParam& param);

    struct MakeServerSceneParam
    {
        entt::entity scene_map_entity_{};
        entt::entity server_id_{};
        common::SceneIds scenes_id_;
        uint32_t scene_config_id_{ 0 };
        uint32_t op_{ 0 };
    };

    entt::entity MakeServerScene(entt::registry& reg,
        const MakeServerSceneParam& param);
}//namespace master

#endif//MASTER_SRC_FACTORIES_SCENE_FACTORIES_HPP_
