#include "player_change_scene.h"

#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/tips_id.h"
#include "src/network/player_session.h"
#include "src/system/player_scene_system.h"

#include "event_proto/scene_event.pb.h"

//todo 各种服务器崩溃
void PlayerChangeSceneSystem::InitChangeSceneQueue(entt::entity player)
{
	tls.registry.emplace<PlayerControllerChangeSceneQueue>(player);
}

uint32_t PlayerChangeSceneSystem::PushChangeSceneInfo(entt::entity player, const ControllerChangeSceneInfo& change_info)
{
	GetPlayerComponentMemberReturnError(change_scene_queue, PlayerControllerChangeSceneQueue, kRetChangeScenePlayerQueueCompnentNull);
	CheckCondition(change_scene_queue.full(), kRetEnterSceneChangingGs);
	change_scene_queue.push_back(change_info);
    change_scene_queue.front().set_change_time(muduo::Timestamp::now().secondsSinceEpoch());//todo
	return kRetOK;
}

bool PlayerChangeSceneSystem::IsChangeQueueEmpty(entt::entity player)
{
    GetPlayerComponentMemberNullReturnFalse(change_scene_queue, PlayerControllerChangeSceneQueue);
	return change_scene_queue.empty();
}

void PlayerChangeSceneSystem::TryProcessChangeSceneQueue(entt::entity player)
{
	GetPlayerComponentMemberReturnVoid(change_scene_queue, PlayerControllerChangeSceneQueue);
    if (change_scene_queue.empty())
    {
        return;
    }
	auto& change_info = change_scene_queue.front();
    TryProcessZoneServerChangeScene(player, change_info);
    TryProcessViaCrossServerChangeScene(player, change_info);
}

void PlayerChangeSceneSystem::PopFrontChangeSceneQueue(entt::entity player)
{
    GetPlayerComponentMemberReturnVoid(change_scene_queue, PlayerControllerChangeSceneQueue);
    if (change_scene_queue.empty())
    {
        return;
    }
    change_scene_queue.pop_front();
}

void PlayerChangeSceneSystem::SetChangeGsStatus(entt::entity player, ControllerChangeSceneInfo::eChangeGsStatus s)
{
	GetPlayerComponentMemberReturnVoid(change_scene_queue, PlayerControllerChangeSceneQueue);
	if (change_scene_queue.empty())
	{
		return;
	}
    change_scene_queue.front().set_change_gs_status(s);
}

void PlayerChangeSceneSystem::TryProcessZoneServerChangeScene(entt::entity player, ControllerChangeSceneInfo& change_info)
{
    //不走跨服，只在区服务器
    if (change_info.change_cross_server_type() != ControllerChangeSceneInfo::eDotnotCrossServer)
    {
        return;
    }
    if (change_info.change_gs_type() == ControllerChangeSceneInfo::eSameGs)//同一个gs切换
    {
        TryChangeSameGsScene(player);//就算同gs,队列有消息也不能直接切换，
        return;
    }
    else if (change_info.change_gs_type() == ControllerChangeSceneInfo::eDifferentGs)
    {
        //正在切换
        //切换gs  存储完毕之后才能进入下一个场景
        //放到存储完毕切换场景的队列里面，如果等够足够时间没有存储完毕，可能就是服务器崩溃了,注意，是可能 
        ChangeDiffGsScene(player);
        return;
    }
}

void PlayerChangeSceneSystem::TryProcessViaCrossServerChangeScene(entt::entity player, ControllerChangeSceneInfo& change_info)
{
    //不跨服不走这里
    if (change_info.change_cross_server_type() != ControllerChangeSceneInfo::eCrossServer)
    {
        return;
    }
    //cross server 处理完了
    if (change_info.change_cross_server_status() != ControllerChangeSceneInfo::eEnterCrossServerSceneSucceed)
    {
        return;
    }
    if (change_info.change_gs_type() == ControllerChangeSceneInfo::eSameGs)//跨服同一个gs
    {
        TryChangeSameGsScene(player);//就算同gs,队列有消息也不能直接切换，
        return;
    }   
    else if (change_info.change_gs_type() == ControllerChangeSceneInfo::eDifferentGs)
    {
        ChangeDiffGsScene(player);
        return;
    }    
}

//当前服务器去计算当前的消息是不是同一个gs

uint32_t PlayerChangeSceneSystem::TryChangeSameGsScene(entt::entity player)
{
    GetPlayerComponentReturnError(try_change_scene_queue, PlayerControllerChangeSceneQueue, kRetChangeScenePlayerQueueCompnentNull);
    auto& change_scene_queue = try_change_scene_queue->change_scene_queue_;
    if (change_scene_queue.empty())
    {
        return kRetChangeScenePlayerQueueCompnentEmpty;
    }
    auto& change_info = change_scene_queue.front();
    auto to_scene = ScenesSystem::get_scene(change_info.scene_info().scene_id());
    if (entt::null == to_scene)//场景不存在了把消息删除,这个文件一定要注意这个队列各种异常情况
    {
        change_scene_queue.pop_front();//todo
        return kRetEnterSceneSceneNotFound;
    }
    LeaveSceneParam lp;
    lp.leaver_ = player;
    ScenesSystem::LeaveScene(lp);

    EnterSceneParam ep;
    ep.enterer_ = player;
    ep.scene_ = to_scene;
    ScenesSystem::EnterScene(ep);
    change_scene_queue.pop_front();//切换成功消息删除

    OnEnterSceneOk(player);
    return kRetOK;
}

uint32_t PlayerChangeSceneSystem::ChangeDiffGsScene(entt::entity player)
{
    GetPlayerComponentReturnError(try_change_scene_queue, PlayerControllerChangeSceneQueue, kRetChangeScenePlayerQueueCompnentNull);
    auto& change_scene_queue = try_change_scene_queue->change_scene_queue_;
    if (change_scene_queue.empty())
    {
        return kRetChangeScenePlayerQueueCompnentEmpty;
    }
    auto& change_info = change_scene_queue.front();
    if (change_info.change_gs_status() == ControllerChangeSceneInfo::eLeaveGsScene)
    {
        //正在切换
        //切换gs  存储完毕之后才能进入下一个场景
        //放到存储完毕切换场景的队列里面，如果等够足够时间没有存储完毕，可能就是服务器崩溃了,注意，是可能 
        LeaveSceneParam lp;
        lp.leaver_ = player;
        ScenesSystem::LeaveScene(lp);
    }
    else if (change_info.change_gs_status() == ControllerChangeSceneInfo::eEnterGsSceneSucceed)
    {
        auto to_scene = ScenesSystem::get_scene(change_info.scene_info().scene_id());
        if (entt::null == to_scene)//场景不存在了把消息删除,这个文件一定要注意这个队列各种异常情况
        {
            change_scene_queue.pop_front();//todo
            return kRetEnterSceneSceneNotFound;
        }
        EnterSceneParam ep;
        ep.enterer_ = player;
        ep.scene_ = to_scene;
        ScenesSystem::EnterScene(ep);
    }
    else if (change_info.change_gs_status() == ControllerChangeSceneInfo::eGateEnterGsSceneSucceed)
    {
        change_scene_queue.pop_front();
        OnEnterSceneOk(player);
    }
    return kRetOK;
}

void PlayerChangeSceneSystem::OnEnterSceneOk(entt::entity player)
{
    S2CEnterScene s2c_enter_scene_event;
    s2c_enter_scene_event.set_entity(entt::to_integral(player));
    tls.dispatcher.trigger(s2c_enter_scene_event);
}
