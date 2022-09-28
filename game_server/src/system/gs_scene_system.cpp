#include "gs_scene_system.h"

#include "muduo/base/Logging.h"

#include "ue5navmesh/Public/Detour/DetourNavMesh.h"

#include "src/game_config/mainscene_config.h"
#include "src/game_config/scene_config.h"

#include "src/game_logic/scene/servernode_system.h"
#include "src/game_logic/scene/scene.h"
#include "src/system/player_scene_system.h"
#include "src/system/recast_system.h"

#include "component_proto/player_comp.pb.h"

SceneNavs g_scene_nav;

void GsSceneSystem::LoadAllMainSceneNavBin()
{
    auto& config_all = mainscene_config::GetSingleton().all();
	for (int32_t i = 0; i < config_all.data_size(); ++i)
	{
        auto scene_nav_ptr = std::make_shared<SceneNavPtr::element_type>();
        scene_nav_ptr->p_nav_  = std::make_unique<SceneNav::DtNavMeshPtr::element_type>();
        scene_nav_ptr->p_nav_query_ = std::make_unique<SceneNav::DtNavMeshQueryPtr::element_type>();
        RecstSystem::LoadNavMesh(config_all.data(i).nav_bin_file().c_str(), scene_nav_ptr->p_nav_.get());
    }    
}

void GsSceneSystem::CreateSceneByGuid(CreateSceneBySceneInfoP& param)
{
    auto scene = ScenesSystem::GetSingleton().CreateSceneByGuid(param);
    //init scene 
    auto p_scene_row = get_scene_conf(param.scene_info_.scene_confid());
    if (nullptr == p_scene_row)
    {
        LOG_ERROR << "scene config null" << param.scene_info_.scene_confid();
        return;
    }
}

void GsSceneSystem::EnterScene(const EnterSceneParam& param)
{
    auto enterer = param.enterer_;
    ScenesSystem::GetSingleton().EnterScene(param);

    if (registry.any_of<Player>(enterer))
    {
        PlayerSceneSystem::OnEnterScene(enterer, param.scene_);
    }    
    else
    {

    }
}

void GsSceneSystem::LeaveScene(entt::entity ent)
{
    LeaveSceneParam leave;
    leave.leaver_ = ent;
    ScenesSystem::GetSingleton().LeaveScene(leave);
}
