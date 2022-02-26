#ifndef COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_
#define COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_

#include <memory>

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

namespace common
{
    using SceneMapComp = std::unordered_map<Guid, entt::entity>;
    using Uint32KeyEntitySetValue = std::unordered_map<uint32_t, EntitySet>;
    using PlayersComp = EntitySet;//弱引用，要解除玩家和场景的耦合
    using ConfigIdComp = uint32_t;

    class SceneEntity
    {
    public:
        SceneEntity(entt::entity scene_entity) : entity_(scene_entity) {}
        inline void set_entity(entt::entity scene_entity) { entity_ = scene_entity; }
        inline  entt::entity scene_entity()const { return entity_; }
    private:
        entt::entity entity_{};
    };

    class SceneComp
    {
    public:
        entt::entity first_scene_id() { if (sceneids_.empty()) { return entt::null; } return *sceneids_.begin(); }
        const EntitySet& scenesids() const { return sceneids_; }
        const Uint32KeyEntitySetValue& confid_sceneslist() const { return confid_scenelist_; }
        const EntitySet& confid_sceneslist(uint32_t scene_config_id) const 
        {
            auto it = confid_scenelist_.find(scene_config_id);
            if (it == confid_scenelist_.end())
            {
                static EntitySet s;
                return s;
            }
            return it->second;
        }
        EntitySet scenesids_clone() { return sceneids_; }
        entt::entity scenelist(uint32_t scene_config_id)const
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

        std::size_t confid_scenelist_size(uint32_t scene_config_id)
        {
            auto it = confid_scenelist_.find(scene_config_id);
            if (it == confid_scenelist_.end())
            {
                return 0;
            }
            return it->second.size();
        }

        bool scene_config_empty(uint32_t scene_config_id)
        {
            auto it = confid_scenelist_.find(scene_config_id);
            if (it == confid_scenelist_.end())
            {
                return true;
            }
            return it->second.empty();
        }

        std::size_t scenes_size() const { return sceneids_.size(); }

        inline bool scenes_empty() const { return sceneids_.empty(); }

        inline bool HasConfig(uint32_t scene_config_id)const{ return confid_scenelist_.find(scene_config_id) != confid_scenelist_.end(); }

        void AddScene(uint32_t scene_config_id, entt::entity scene_entity)
        {
            confid_scenelist_[scene_config_id].emplace(scene_entity);
            sceneids_.emplace(scene_entity);
        }

        void RemoveScene(uint32_t scene_config_id, entt::entity scene_entity)
        {
            confid_scenelist_[scene_config_id].erase(scene_entity);
            sceneids_.erase(scene_entity);
        }
    private:
        Uint32KeyEntitySetValue confid_scenelist_;
        EntitySet sceneids_;
    };

    struct MainSceneComp {};
    struct RoomSceneComp {};

    struct MainSceneServerComp {};
    struct RoomSceneServerComp {};

    struct GSNormalComp{};//game server 正常状态
    struct GSMainTainComp{};//game server 维护状态
    struct GSCrashComp{};//崩溃状态

    struct NoPressureComp {};//
    struct PressureComp {};//

    class GSData
    {
    public:
        inline void set_node_id(uint32_t node_id) { node_id_ = node_id; }
        inline uint32_t node_id()const { return node_id_; }
        inline void set_node_entity(entt::entity server_entity) { server_entity_ = server_entity; }
        inline entt::entity server_entity()const { return server_entity_; }
        uint32_t player_size()const { return player_size_; }

        inline void OnPlayerEnter() { ++player_size_; }
        inline void OnPlayerLeave() { --player_size_; }
    private:
        uint32_t node_id_{0};
        entt::entity server_entity_{};
        uint32_t player_size_{ 0 };
    };

    using GSDataPtrComp = std::shared_ptr<GSData>;

    class GSEntity
    {
    public:
        GSEntity(entt::entity e) : server_entity_(e) {}
        void set_server_entity(entt::entity server_entity) { server_entity_ = server_entity; }
        entt::entity server_entity() const { return server_entity_; }
    private:
        entt::entity server_entity_{};
    };
}//namespace common

#endif//COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_