#include "src/scene/sceces.h"

#include "src/game_logic/game_registry.h"

using namespace common;

ScenesSystem* g_scene_sys = nullptr;

    std::size_t ScenesSystem::scenes_size(uint32_t scene_config_id)const
    {
        auto it = confid_scenes_.find(scene_config_id);
        if (it == confid_scenes_.end())
        {
            return 0;
        }
        return it->second.size();
    }

    bool ScenesSystem::HasScene(uint32_t scene_config_id)
    {
        auto it = confid_scenes_.find(scene_config_id);
        if (it == confid_scenes_.end())
        {
            return true;
        }
        return it->second.empty();
    }

    entt::entity ScenesSystem::MakeScene(const MakeSceneP& param)
    {
        auto e = reg.create();
        auto& confid = reg.emplace<SceneConfigId>(e, param.scene_confid_);
        reg.emplace<MainScene>(e);
        reg.emplace<PlayersComp>(e);
        auto guid = snow_flake_.Generate();
        reg.emplace<Guid>(e, guid);
        scenes_map_.emplace(guid, e);
        confid_scenes_[confid].emplace(e);
        return e;
    }

    entt::entity ScenesSystem::MakeSceneGSScene(const MakeGSSceneP& param)
    {
        MakeSceneP make_p;
        make_p.op_ = param.op_;
        make_p.scene_confid_ = param.scene_confid_;
        auto e = MakeScene(make_p);
        PutScene2GSParam put_param;
        put_param.scene_entity_ = e;
        put_param.server_entity_ = param.server_entity_;
        PutScene2GS(put_param);
        return e;
    }

    void ScenesSystem::PutScene2GS(const PutScene2GSParam& param)
    {
        auto scene_entity = param.scene_entity_;
        auto& scene_config = reg.get<SceneConfigId>(scene_entity);
        auto server_entity = param.server_entity_;
        auto& server_scenes = reg.get<SceneComp>(server_entity);
        server_scenes.AddScene(scene_config, scene_entity);
        auto& p_server_data = reg.get<GSDataPtrComp>(server_entity);
        reg.emplace<GSDataPtrComp>(scene_entity, p_server_data);
    }


    void ScenesSystem::DestroyScene(const DestroySceneParam& param)
    {
        OnDestroyScene(param.scene_entity_);
    }

    void ScenesSystem::DestroyServer(const DestroyServerParam& param)
    {
        auto server_entity = param.server_entity_;
        auto server_scenes = reg.get<SceneComp>(server_entity).scenesids_clone();
        DestroySceneParam destroy_param;
        for (auto& it : server_scenes)
        {
            OnDestroyScene(it);
        }
        reg.destroy(server_entity);
    }

    void ScenesSystem::MoveServerScene2ServerScene(const MoveServerScene2ServerSceneP& param)
    {
        auto to_server_entity = param.to_server_entity_;
        auto& from_scenes_id = reg.get<SceneComp>(param.from_server_entity_).confid_sceneslist();
        auto& to_scenes_id = reg.get<SceneComp>(to_server_entity);
        auto& p_to_server_data = reg.get<GSDataPtrComp>(to_server_entity);
        for (auto& it : from_scenes_id)
        {
            for (auto& ji : it.second)
            {
                reg.emplace_or_replace<GSDataPtrComp>(ji, p_to_server_data);
                to_scenes_id.AddScene(it.first, ji);
            }
        }
        reg.emplace_or_replace<SceneComp>(param.from_server_entity_);
    }

    void ScenesSystem::EnterScene(const EnterSceneParam& param)
    {
        auto scene_entity = param.scene_entity_;
        auto& player_entities = reg.get<PlayersComp>(scene_entity);
        player_entities.emplace(param.enter_entity_);
        reg.emplace<common::SceneEntity>(param.enter_entity_, scene_entity);
        auto p_server_data = reg.try_get<GSDataPtrComp>(scene_entity);
        if (nullptr == p_server_data)
        {
            return;
        }
        (*p_server_data)->OnPlayerEnter();
    }

    void ScenesSystem::LeaveScene(const LeaveSceneParam& param)
    {
        auto leave_entity = param.leave_entity_;
        auto& player_scene_entity = reg.get<common::SceneEntity>(leave_entity);
        auto scene_entity = player_scene_entity.scene_entity();
        auto& player_entities = reg.get<PlayersComp>(scene_entity);
        player_entities.erase(leave_entity);
        reg.remove<common::SceneEntity>(leave_entity);
        auto p_server_data = reg.try_get<GSDataPtrComp>(scene_entity);
        if (nullptr == p_server_data)
        {
            return;
        }
        (*p_server_data)->OnPlayerLeave();
    }

    void ScenesSystem::CompelChangeScene(const CompelChangeSceneParam& param)
    {
        auto new_server_entity = param.new_server_entity_;
        auto compel_entity = param.compel_change_entity_;
        auto& new_server_scene = reg.get<SceneComp>(new_server_entity);
        auto scene_config_id = param.scene_confid_;

        entt::entity server_scene_enitity = entt::null;

        if (!new_server_scene.HasConfig(param.scene_confid_))
        {
            MakeGSSceneP make_server_scene_param;
            make_server_scene_param.scene_confid_ = scene_config_id;
            make_server_scene_param.server_entity_ = new_server_entity;
            server_scene_enitity = MakeSceneGSScene(make_server_scene_param);
        }
        else
        {
            server_scene_enitity = new_server_scene.scenelist(param.scene_confid_);
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

    void ScenesSystem::ReplaceCrashServer(const ReplaceCrashServerParam& param)
    {
        MoveServerScene2ServerSceneP move_param;
        move_param.from_server_entity_ = param.cransh_server_entity_;
        move_param.to_server_entity_ = param.replace_server_entity_;
        MoveServerScene2ServerScene(move_param);
        reg.destroy(move_param.from_server_entity_);
    }

    void ScenesSystem::OnDestroyScene(entt::entity scene_entity)
    {
        auto scene_config_id = reg.get<SceneConfigId>(scene_entity);
        confid_scenes_[scene_config_id].erase(scene_entity);
        auto scene_guid = reg.get<Guid>(scene_entity);
        scenes_map_.erase(scene_guid);
        auto p_server_data = reg.get<GSDataPtrComp>(scene_entity);
        reg.destroy(scene_entity);
        if (nullptr == p_server_data)
        {
            return;
        }
        auto& server_scene = reg.get<SceneComp>(p_server_data->server_entity());
        server_scene.RemoveScene(scene_config_id, scene_entity);
    }
