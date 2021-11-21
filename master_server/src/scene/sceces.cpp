#include "src/scene/sceces.h"

#include "src/game_logic/game_registry.h"

using namespace common;

master::ScenesManager* g_scene_manager = nullptr;

namespace master
{
    ScenesManager::ScenesManager()
    {
    }

    const common::SceneIds& ScenesManager::scenes_config_id(uint32_t scene_config_id) const
    {
        auto it = config_scene_.find(scene_config_id);
        if (it == config_scene_.end())
        {
            static common::SceneIds s;
            return s;
        }
        return it->second;
    }

    entt::entity ScenesManager::scene_id(uint32_t scene_config_id) const
    {
        auto it = config_scene_.find(scene_config_id);
        if (it == config_scene_.end())
        {
            return entt::null;
        }
        if (it->second.empty())
        {
            return entt::null;
        }
        return *it->second.begin();
    }

    std::size_t ScenesManager::scene_config_size(uint32_t scene_config_id)const
    {
        auto it = config_scene_.find(scene_config_id);
        if (it == config_scene_.end())
        {
            return 0;
        }
        return it->second.size();
    }

    bool ScenesManager::scene_config_empty(uint32_t scene_config_id)
    {
        auto it = config_scene_.find(scene_config_id);
        if (it == config_scene_.end())
        {
            return true;
        }
        return it->second.empty();
    }

    entt::entity ScenesManager::MakeMainScene(const MakeSceneParam& param)
    {
        auto e = reg().create();
        reg().emplace<common::SceneConfig>(e, param.scene_config_id_);
        reg().emplace<common::MainScene>(e);
        reg().emplace<common::PlayerEntities>(e);
        auto& scene_config = reg().get<common::SceneConfig>(e);
        auto scene_guid = snow_flake_.Generate();
        reg().emplace<common::GameGuid>(e, scene_guid);
        scenes_map_.emplace(scene_guid, e);
        AddScene(scene_config.scene_config_id(), e);
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
        auto& scene_config = reg().get<common::SceneConfig>(scene_entity);
        auto server_entity = param.server_entity_;
        auto& server_scenes = reg().get<common::Scenes>(server_entity);
        server_scenes.AddScene(scene_config.scene_config_id(), scene_entity);
        auto& p_server_data = reg().get<common::GameServerDataPtr>(server_entity);
        reg().emplace<common::GameServerDataPtr>(scene_entity, p_server_data);
    }


    void ScenesManager::DestroyScene(const DestroySceneParam& param)
    {
        OnDestroyScene(param.scene_entity_);
    }

    void ScenesManager::DestroyServer(const DestroyServerParam& param)
    {
        auto server_entity = param.server_entity_;
        auto server_scenes = reg().get<common::Scenes>(server_entity).copy_scenes_id();
        DestroySceneParam destroy_param;
        for (auto& it : server_scenes)
        {
            OnDestroyScene(it);
        }
        reg().destroy(server_entity);
    }

    void ScenesManager::MoveServerScene2Server(const MoveServerScene2ServerParam& param)
    {
        auto to_server_entity = param.to_server_entity_;
        auto& from_scenes_id = reg().get<common::Scenes>(param.from_server_entity_).scenes_config_id();
        auto& to_scenes_id = reg().get<common::Scenes>(to_server_entity);
        auto& p_to_server_data = reg().get<common::GameServerDataPtr>(to_server_entity);
        for (auto& it : from_scenes_id)
        {
            for (auto& ji : it.second)
            {
                reg().emplace_or_replace<common::GameServerDataPtr>(ji, p_to_server_data);
                to_scenes_id.AddScene(it.first, ji);
            }
        }
        reg().emplace_or_replace<common::Scenes>(param.from_server_entity_);
    }

    void ScenesManager::EnterScene(const EnterSceneParam& param)
    {
        auto scene_entity = param.scene_entity_;
        auto& player_entities = reg().get<common::PlayerEntities>(scene_entity);
        player_entities.emplace(param.enter_entity_);
        reg().emplace<common::SceneEntityId>(param.enter_entity_, scene_entity);
        auto p_server_data = reg().try_get<common::GameServerDataPtr>(scene_entity);
        if (nullptr == p_server_data)
        {
            return;
        }
        (*p_server_data)->OnPlayerEnter();
    }

    void ScenesManager::LeaveScene(const LeaveSceneParam& param)
    {
        auto leave_entity = param.leave_entity_;
        auto& player_scene_entity = reg().get<common::SceneEntityId>(leave_entity);
        auto scene_entity = player_scene_entity.scene_entity();
        auto& player_entities = reg().get<common::PlayerEntities>(scene_entity);
        player_entities.erase(leave_entity);
        reg().remove<common::SceneEntityId>(leave_entity);
        auto p_server_data = reg().try_get<common::GameServerDataPtr>(scene_entity);
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
        auto& new_server_scene = reg().get<common::Scenes>(new_server_entity);
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
            server_scene_enitity = new_server_scene.scene_id(param.scene_config_id_);
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
        MoveServerScene2ServerParam move_param;
        move_param.from_server_entity_ = param.cransh_server_entity_;
        move_param.to_server_entity_ = param.replace_server_entity_;
        MoveServerScene2Server(move_param);
        reg().destroy(move_param.from_server_entity_);
    }

    void ScenesManager::AddScene(uint32_t scene_config_id, entt::entity scene_entity)
    {
        config_scene_[scene_config_id].emplace(scene_entity);
        scenes_.emplace(scene_entity);
    }

    void ScenesManager::RemoveScene(uint32_t scene_config_id, entt::entity scene_entity)
    {
        config_scene_[scene_config_id].erase(scene_entity);
        scenes_.erase(scene_entity);
    }

    void ScenesManager::OnDestroyScene(entt::entity scene_entity)
    {
        auto scene_config_id = reg().get<common::SceneConfig>(scene_entity).scene_config_id();
        RemoveScene(scene_config_id, scene_entity);
        auto scene_guid = reg().get<common::GameGuid>(scene_entity);
        scenes_map_.erase(scene_guid);
        auto p_server_data = reg().get<common::GameServerDataPtr>(scene_entity);
        reg().destroy(scene_entity);
        if (nullptr == p_server_data)
        {
            return;
        }
        auto& server_scene = reg().get<common::Scenes>(p_server_data->server_entity());
        server_scene.RemoveScene(scene_config_id, scene_entity);
    }

}//namespace master