#ifndef COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_
#define COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_

#include <memory>

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

namespace common
{
    using SceneIds = std::unordered_set<entt::entity>;
    using SceneMap = std::unordered_map<GameGuid, entt::entity>;
    using ConfigScenes = std::unordered_map<uint32_t, SceneIds>;
    using PlayerEntities = std::unordered_set<entt::entity>;

    class SceneConfig
    {
    public:
        SceneConfig(uint32_t scene_config_id) : scene_config_id_(scene_config_id) {}
        inline void set_scene_config_id(uint32_t scene_config_id) { scene_config_id_ = scene_config_id; }
        inline  uint32_t scene_config_id()const { return scene_config_id_; }
    private:
        uint32_t scene_config_id_{ 0 };
    };

    class SceneEntityId
    {
    public:
        SceneEntityId(entt::entity scene_entity) : scene_entity_(scene_entity) {}
        inline void set_scene_entity(entt::entity scene_entity) { scene_entity_ = scene_entity; }
        inline  entt::entity scene_entity()const { return scene_entity_; }
    private:
        entt::entity scene_entity_{};
    };

    class Scenes
    {
    public:
        entt::entity first_scene() { if (scenes_.empty()) { return entt::null; } return *scenes_.begin(); }
        const SceneIds& scenes_id() const { return scenes_; }
        const ConfigScenes& scenes_config_id() const { return config_scene_; }
        const SceneIds& scenes_config_id(uint32_t scene_config_id) const 
        {
            auto it = config_scene_.find(scene_config_id);
            if (it == config_scene_.end())
            {
                static SceneIds s;
                return s;
            }
            return it->second;
        }
        SceneIds copy_scenes_id() { return scenes_; }
        entt::entity scene_id(uint32_t scene_config_id)const
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

        std::size_t scene_config_size(uint32_t scene_config_id)
        {
            auto it = config_scene_.find(scene_config_id);
            if (it == config_scene_.end())
            {
                return 0;
            }
            return it->second.size();
        }

        bool scene_config_empty(uint32_t scene_config_id)
        {
            auto it = config_scene_.find(scene_config_id);
            if (it == config_scene_.end())
            {
                return true;
            }
            return it->second.empty();
        }

        std::size_t scenes_size() const { return scenes_.size(); }

        bool scenes_empty() const { return scenes_.empty(); }

        bool HasSceneConfig(uint32_t scene_config_id) { return config_scene_.find(scene_config_id) != config_scene_.end(); }

        void AddScene(uint32_t scene_config_id, entt::entity scene_entity)
        {
            config_scene_[scene_config_id].emplace(scene_entity);
            scenes_.emplace(scene_entity);
        }

        void RemoveScene(uint32_t scene_config_id, entt::entity scene_entity)
        {
            config_scene_[scene_config_id].erase(scene_entity);
            scenes_.erase(scene_entity);
        }
    private:
        ConfigScenes config_scene_;
        SceneIds scenes_;
    };

    struct MainScene {};
    struct RoomScene {};

    struct MainSceneServer {};
    struct RoomSceneServer {};

    struct GameServerStatusNormal{};//game server Õý³£×´Ì¬
    struct GameServerMainTain{};//game server Î¬»¤×´Ì¬
    struct GameServerCrash{};//±ÀÀ£×´Ì¬

    struct ServerUpdateNotice {};
    struct ServerMaintainNotice {};

    struct GameNoPressure {};//
    struct GamePressure {};//

    class GameServerData
    {
    public:
        inline void set_server_id(uint32_t server_id) { server_id_ = server_id; }
        inline uint32_t server_id()const { return server_id_; }
        inline void set_server_entity(entt::entity server_entity) { server_entity_ = server_entity; }
        inline entt::entity server_entity()const { return server_entity_; }
        uint32_t player_size()const { return player_size_; }

        inline void OnPlayerEnter() { ++player_size_; }
        inline void OnPlayerLeave() { --player_size_; }
    private:
        uint32_t server_id_{0};
        entt::entity server_entity_{};
        uint32_t player_size_{ 0 };
    };

    using GameServerDataPtr = std::shared_ptr<GameServerData>;

    struct MainSceneServerSize
    {
        uint32_t server_size_{ 0 };
    };

}//namespace common

#endif//COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_