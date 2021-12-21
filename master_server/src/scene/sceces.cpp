#include "src/scene/sceces.h"

#include "src/game_logic/game_registry.h"

using namespace common;

master::ScenesManager* g_scene_manager = nullptr;

namespace master
{
    ScenesManager::ScenesManager()
    {
    }

    const EntitySet& ScenesManager::scenes_config_id(uint32_t scene_config_id) const
    {
        auto it = confid_scenelist_.find(scene_config_id);
        if (it == confid_scenelist_.end())
        {
             static EntitySet s;
            return s;
        }
        return it->second;
    }

    entt::entity ScenesManager::scenelist(uint32_t scene_config_id) const
    {
        auto it = confid_scenelist_.find(scene_config_id);
        if (it == confid_scenelist_.end())
        {
            return entt::null;
        }
        if (it->second.empty())
        {
            return entt::null;
        }
        return *it->second.begin();
    }

    std::size_t ScenesManager::confid_scenelist_size(uint32_t scene_config_id)const
    {
        auto it = confid_scenelist_.find(scene_config_id);
        if (it == confid_scenelist_.end())
        {
            return 0;
        }
        return it->second.size();
    }

    bool ScenesManager::IsConfigSceneEmpty(uint32_t scene_config_id)
    {
        auto it = confid_scenelist_.find(scene_config_id);
        if (it == confid_scenelist_.end())
        {
            return true;
        }
        return it->second.empty();
    }

    entt::entity ScenesManager::MakeMainScene(const MakeSceneParam& param)
    {
        auto e = reg().create();
        reg().emplace<SceneConfigComp>(e, param.scene_config_id_);
        reg().emplace<MainSceneComp>(e);
        reg().emplace<PlayersComp>(e);
        auto& scene_config = reg().get<SceneConfigComp>(e);
        auto scene_guid = snow_flake_.Generate();
        reg().emplace<Guid>(e, scene_guid);
        scenes_map_.emplace(scene_guid, e);
        AddScene(scene_config, e);
        return e;
    }

    entt::entity ScenesManager::MakeScene2GameServer(const MakeScene2GameServerParam& param)
    {
        MakeSceneParam main_scene_param;
        main_scene_param.op_ = param.op_;
        main_scene_param.scene_config_id_ = param.scene_config_id_;
        auto e = MakeMainScene(main_scene_param);
        PutScene2GameServerParam put_param;
        put_param.scene_entity_ = e;
        put_param.server_entity_ = param.server_entity_;
        PutScene2GameServer(put_param);
        return e;
    }

    void ScenesManager::PutScene2GameServer(const PutScene2GameServerParam& param)
    {
        auto scene_entity = param.scene_entity_;
        auto& scene_config = reg().get<SceneConfigComp>(scene_entity);
        auto server_entity = param.server_entity_;
        auto& server_scenes = reg().get<SceneComp>(server_entity);
        server_scenes.AddScene(scene_config, scene_entity);
        auto& p_server_data = reg().get<GSDataPtrComp>(server_entity);
        reg().emplace<GSDataPtrComp>(scene_entity, p_server_data);
    }


    void ScenesManager::DestroyScene(const DestroySceneParam& param)
    {
        OnDestroyScene(param.scene_entity_);
    }

    void ScenesManager::DestroyServer(const DestroyServerParam& param)
    {
        auto server_entity = param.server_entity_;
        auto server_scenes = reg().get<SceneComp>(server_entity).scenesids_clone();
        DestroySceneParam destroy_param;
        for (auto& it : server_scenes)
        {
            OnDestroyScene(it);
        }
        reg().destroy(server_entity);
    }

    void ScenesManager::MoveServerScene2ServerScene(const MoveServerScene2ServerSceneP& param)
    {
        auto to_server_entity = param.to_server_entity_;
        auto& from_scenes_id = reg().get<SceneComp>(param.from_server_entity_).confid_sceneslist();
        auto& to_scenes_id = reg().get<SceneComp>(to_server_entity);
        auto& p_to_server_data = reg().get<GSDataPtrComp>(to_server_entity);
        for (auto& it : from_scenes_id)
        {
            for (auto& ji : it.second)
            {
                reg().emplace_or_replace<GSDataPtrComp>(ji, p_to_server_data);
                to_scenes_id.AddScene(it.first, ji);
            }
        }
        reg().emplace_or_replace<SceneComp>(param.from_server_entity_);
    }

    void ScenesManager::EnterScene(const EnterSceneParam& param)
    {
        auto scene_entity = param.scene_entity_;
        auto& player_entities = reg().get<PlayersComp>(scene_entity);
        player_entities.emplace(param.enter_entity_);
        reg().emplace<common::SceneEntity>(param.enter_entity_, scene_entity);
        auto p_server_data = reg().try_get<GSDataPtrComp>(scene_entity);
        if (nullptr == p_server_data)
        {
            return;
        }
        (*p_server_data)->OnPlayerEnter();
    }

    void ScenesManager::LeaveScene(const LeaveSceneParam& param)
    {
        auto leave_entity = param.leave_entity_;
        auto& player_scene_entity = reg().get<common::SceneEntity>(leave_entity);
        auto scene_entity = player_scene_entity.scene_entity();
        auto& player_entities = reg().get<PlayersComp>(scene_entity);
        player_entities.erase(leave_entity);
        reg().remove<common::SceneEntity>(leave_entity);
        auto p_server_data = reg().try_get<GSDataPtrComp>(scene_entity);
        if (nullptr == p_server_data)
        {
            return;
        }
        (*p_server_data)->OnPlayerLeave();
    }

    void ScenesManager::CompelChangeScene(const CompelChangeSceneParam& param)
    {
        auto new_server_entity = param.new_server_entity_;
        auto compel_entity = param.compel_change_entity_;
        auto& new_server_scene = reg().get<SceneComp>(new_server_entity);
        auto scene_config_id = param.scene_config_id_;

        entt::entity server_scene_enitity = entt::null;

        if (!new_server_scene.HasSceneConfig(param.scene_config_id_))
        {
            MakeScene2GameServerParam make_server_scene_param;
            make_server_scene_param.scene_config_id_ = scene_config_id;
            make_server_scene_param.server_entity_ = new_server_entity;
            server_scene_enitity = MakeScene2GameServer(make_server_scene_param);
        }
        else
        {
            server_scene_enitity = new_server_scene.scenelist(param.scene_config_id_);
        }

        if (entt::null == server_scene_enitity)
        {
            return;
        }

        LeaveSceneParam leave_param;
        leave_param.leave_entity_ = compel_entity;
        LeaveScene(leave_param);

        EnterSceneParam enter_param;
        enter_param.enter_entity_ = compel_entity;
        enter_param.scene_entity_ = server_scene_enitity;
        EnterScene(enter_param);
    }

    void ScenesManager::ReplaceCrashServer(const ReplaceCrashServerParam& param)
    {
        MoveServerScene2ServerSceneP move_param;
        move_param.from_server_entity_ = param.cransh_server_entity_;
        move_param.to_server_entity_ = param.replace_server_entity_;
        MoveServerScene2ServerScene(move_param);
        reg().destroy(move_param.from_server_entity_);
    }

    void ScenesManager::AddScene(uint32_t scene_config_id, entt::entity scene_entity)
    {
        confid_scenelist_[scene_config_id].emplace(scene_entity);
        scenes_.emplace(scene_entity);
    }

    void ScenesManager::RemoveScene(uint32_t scene_config_id, entt::entity scene_entity)
    {
        confid_scenelist_[scene_config_id].erase(scene_entity);
        scenes_.erase(scene_entity);
    }

    void ScenesManager::OnDestroyScene(entt::entity scene_entity)
    {
        auto scene_config_id = reg().get<SceneConfigComp>(scene_entity);
        RemoveScene(scene_config_id, scene_entity);
        auto scene_guid = reg().get<Guid>(scene_entity);
        scenes_map_.erase(scene_guid);
        auto p_server_data = reg().get<GSDataPtrComp>(scene_entity);
        reg().destroy(scene_entity);
        if (nullptr == p_server_data)
        {
            return;
        }
        auto& server_scene = reg().get<SceneComp>(p_server_data->server_entity());
        server_scene.RemoveScene(scene_config_id, scene_entity);
    }

}//namespace master