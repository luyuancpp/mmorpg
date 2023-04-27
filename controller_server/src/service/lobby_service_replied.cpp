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
    // todo ����л����У�return error
    //�п絽b�������У����û�������е�c����������ٵ�cĿǰ�Ͳ�������������ˣ����ǵĻ�д�����鷳
    //todo �첽���������֮ǰ��ȥ�л������������Ѿ��л�����ĳ����ˣ����еĻ����ܾͲ����ˣ���������������ˣ������˲�������ô��
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