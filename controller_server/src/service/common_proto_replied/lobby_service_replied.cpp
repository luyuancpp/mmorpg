#include "lobby_service_replied.h"

#include "muduo/base/Logging.h"

#include "src/comp/player_list.h"
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/tips_id.h"
#include "src/system/player_tip_system.h"
#include "src/system/player_change_scene.h"


void OnEnterLobbyMainSceneReplied(const TcpConnectionPtr& conn, const EnterCrossMainSceneResponesePtr& replied, Timestamp timestamp)
{
    // todo 跨服切换不行，return error
    //切跨到b服过程中，跨服没返回又切到c，跨服回来再到c目前就不考虑这种情况了，考虑的话写代码麻烦
    //todo 异步跨服返回来之前又去切换场景，导致已经切换到别的场景了，再切的话可能就不对了，不考虑这种情况了，正常人不会切那么快
    auto player = ControllerPlayerSystem::GetPlayer(replied->player_id());
    if (entt::null == player)
    {
        LOG_ERROR << "player not found" << replied->player_id();
        return;
    }
    GetPlayerCompnentMemberReturnVoid(change_scene_queue, PlayerControllerChangeSceneQueue);
    if (change_scene_queue.empty())
    {
        return;
    }
    auto scene = ScenesSystem::get_scene(replied->scene_id());
    if (entt::null == scene)
    {
        LOG_ERROR << "scene not found" << replied->scene_id();
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
        return;
    }
    auto& change_scene_info = change_scene_queue.front();
    change_scene_info.set_change_cross_server_status(ControllerChangeSceneInfo::eEnterCrossServerSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
}