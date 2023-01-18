#include "gate_service_replied.h"

#include "muduo/base/Logging.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/session.h"
#include "src/system/player_common_system.h"
#include "src/system/player_change_scene.h"

extern entt::entity GetPlayerByConnId(uint64_t session_id);

void OnGateUpdatePlayerGsReplied(const TcpConnectionPtr& conn, const PlayerEnterGsResponesePtr& replied, Timestamp timestamp)
{
    //gate 更新完gs，相应的gs可以往那个gate上发送消息了
    //todo 中间返回是断开了
    entt::entity player = GetPlayerByConnId(tcp_session_id(conn));
    if (entt::null == player)
    {
        LOG_ERROR << "player not found " << tls.registry.get<Guid>(player);
        return;
    }

    PlayerCommonSystem::OnEnterGateSucceed(player);

    PlayerChangeSceneSystem::SetChangeGsStatus(player, ControllerChangeSceneInfo::eGateEnterGsSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);

}

