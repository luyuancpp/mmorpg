#ifndef COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_
#define COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_

#include <memory>

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"

namespace common
{
    using SceneMapComp = std::unordered_map<Guid, entt::entity>;
    using Uint32KeyEntitySetValue = std::unordered_map<uint32_t, EntitySet>;
    using ScenePlayers = EntitySet;//弱引用，要解除玩家和场景的耦合

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
        entt::entity first_scene_id() 
        { 
            for (auto& it : confid_scenelist_)
            {
                if (it.second.empty())
                {
                    continue;
                }
                return *it.second.begin();
            }
            return entt::null;
        }
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
        EntitySet scenesids_clone() 
        { 
            EntitySet s;
			for (auto& it : confid_scenelist_)
			{
				for (auto& ji : it.second)
				{
                    s.emplace(ji);
				}
			}
			return s;
        }
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

        inline std::size_t scenes_size() const { 
            std::size_t s = 0;
			for (auto& it : confid_scenelist_)
			{
                s += it.second.size();
			}
            return s;
        }

        inline bool scenes_empty() const 
        { 
			for (auto& it : confid_scenelist_)
			{
				if (it.second.empty())
				{
					continue;
				}
				return false;
			}
			return true;
        }

        inline bool HasConfig(uint32_t scene_config_id)const{ return confid_scenelist_.find(scene_config_id) != confid_scenelist_.end(); }

        void AddScene(uint32_t scene_config_id, entt::entity scene_entity)
        {
            confid_scenelist_[scene_config_id].emplace(scene_entity);
        }

        void RemoveScene(uint32_t scene_config_id, entt::entity scene_entity)
        {
            confid_scenelist_[scene_config_id].erase(scene_entity);
        }
    private:
        Uint32KeyEntitySetValue confid_scenelist_;
    };

    struct MainScene {};

    struct MainSceneServer {};
    struct RoomSceneServer {};

    struct GSNormal{};//game server 正常状态
    struct GSMainTain{};//game server 维护状态
    struct GSCrash{};//崩溃状态

    struct NoPressure {};//
    struct Pressure {};//

    class GsData
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
        uint32_t node_id_{common::kInvalidU32Id};
        entt::entity server_entity_{};
        uint32_t player_size_{ 0 };
    };

    using GsDataPtr = std::shared_ptr<GsData>;
    using GsDataWeakPtr = std::weak_ptr<GsData>;
}//namespace common

#endif//COMMON_SRC_GAME_LOGIC_COMP_SERVER_LIST_HPP_