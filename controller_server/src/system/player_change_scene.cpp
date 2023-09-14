#include "player_change_scene.h"

#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/scene/scene_system.h"
#include "src/game_logic/tips_id.h"
#include "src/system/player_scene_system.h"

#include "event_proto/scene_event.pb.h"

//todo 各种服务器崩溃
void PlayerChangeSceneSystem::InitChangeSceneQueue(entt::entity player)
{
    tls.registry.emplace<PlayerControllerChangeSceneQueue>(player);
}

uint32_t PlayerChangeSceneSystem::PushChangeSceneInfo(entt::entity player, const ControllerChangeSceneInfo& change_info)
{
    auto* const change_scene_queue = tls.registry.try_get<PlayerControllerChangeSceneQueue>(player);
    if (nullptr == change_scene_queue)
    {
        return kRetChangeScenePlayerQueueComponentNull;
    }
    CheckCondition(change_scene_queue->change_scene_queue_.full(), kRetEnterSceneChangingGs)
    change_scene_queue->change_scene_queue_.push_back(change_info);
    //todo
    change_scene_queue->change_scene_queue_.front().set_change_time(muduo::Timestamp::now().secondsSinceEpoch());
    return kRetOK;
}

void PlayerChangeSceneSystem::TryProcessChangeSceneQueue(entt::entity player)
{
    TryProcessZoneServerChangeScene(player);
    TryProcessViaCrossServerChangeScene(player);
}

void PlayerChangeSceneSystem::PopFrontChangeSceneQueue(entt::entity player)
{
    auto* const change_scene_queue = tls.registry.try_get<PlayerControllerChangeSceneQueue>(player);
    if (nullptr == change_scene_queue)
    {
        return;
    }
    if (change_scene_queue->change_scene_queue_.empty())
    {
        return;
    }
    change_scene_queue->change_scene_queue_.pop_front();
}

void PlayerChangeSceneSystem::SetChangeGsStatus(entt::entity player, ControllerChangeSceneInfo::eChangeGsStatus s)
{
    auto* const change_scene_queue = tls.registry.try_get<PlayerControllerChangeSceneQueue>(player);
    if (nullptr == change_scene_queue)
    {
        return;
    }
    if (change_scene_queue->change_scene_queue_.empty())
    {
        return;
    }
    change_scene_queue->change_scene_queue_.front().set_change_gs_status(s);
}

void PlayerChangeSceneSystem::SetChangeCrossServerSatus(entt::entity player, ControllerChangeSceneInfo::eChangeCrossServerStatus s)
{
    auto* const change_scene_queue = tls.registry.try_get<PlayerControllerChangeSceneQueue>(player);
    if (nullptr == change_scene_queue)
    {
        return;
    }
    if (change_scene_queue->change_scene_queue_.empty())
    {
        return;
    }
    change_scene_queue->change_scene_queue_.front().set_change_cross_server_status(s);
}

void PlayerChangeSceneSystem::TryProcessZoneServerChangeScene(entt::entity player)
{
    auto* const try_change_scene_queue = tls.registry.try_get<PlayerControllerChangeSceneQueue>(player);
    if (nullptr == try_change_scene_queue)
    {
        return;
    }
    auto& change_scene_queue = try_change_scene_queue->change_scene_queue_;
    if (change_scene_queue.empty())
    {
        return;
    }
    //不走跨服，只在区服务器
    if (change_scene_queue.front().change_cross_server_type() != ControllerChangeSceneInfo::eDotnotCrossServer)
    {
        return;
    }
    //同一个gs切换
    if (change_scene_queue.front().change_gs_type() == ControllerChangeSceneInfo::eSameGs)
    {
        //就算同gs,队列有消息也不能直接切换，
        TryChangeSameGsScene(player);
        return;
    }
    if (change_scene_queue.front().change_gs_type() == ControllerChangeSceneInfo::eDifferentGs)
    {
        //正在切换
        //切换gs  存储完毕之后才能进入下一个场景
        //放到存储完毕切换场景的队列里面，如果等够足够时间没有存储完毕，可能就是服务器崩溃了,注意，是可能
        ChangeDiffGsScene(player);
    }
}

void PlayerChangeSceneSystem::TryProcessViaCrossServerChangeScene(entt::entity player)
{
    const auto change_scene_queue = tls.registry.try_get<PlayerControllerChangeSceneQueue>(player);
    if (nullptr == change_scene_queue)
    {
        return;
    }
    //不跨服不走这里
    if (change_scene_queue->change_scene_queue_.front().change_cross_server_type() != ControllerChangeSceneInfo::eCrossServer)
    {
        return;
    }
    //cross server 处理完了
    if (change_scene_queue->change_scene_queue_.front().change_cross_server_status() != ControllerChangeSceneInfo::eEnterCrossServerSceneSucceed)
    {
        return;
    }
    if (change_scene_queue->change_scene_queue_.front().change_gs_type() == ControllerChangeSceneInfo::eSameGs)//跨服同一个gs
    {
        TryChangeSameGsScene(player);//就算同gs,队列有消息也不能直接切换，
        return;
    }
    if (change_scene_queue->change_scene_queue_.front().change_gs_type() == ControllerChangeSceneInfo::eDifferentGs)
    {
        ChangeDiffGsScene(player);
        return;
    }
}

//当前服务器去计算当前的消息是不是同一个gs

uint32_t PlayerChangeSceneSystem::TryChangeSameGsScene(entt::entity player)
{
    auto* const change_scene_queue = tls.registry.try_get<PlayerControllerChangeSceneQueue>(player);
    if (nullptr == change_scene_queue)
    {
        return kRetChangeScenePlayerQueueComponentNull;
    }
    if (change_scene_queue->change_scene_queue_.empty())
    {
        return kRetChangeScenePlayerQueueComponentEmpty;
    }
    const auto& change_info = change_scene_queue->change_scene_queue_.front();
    const auto to_scene = ScenesSystem::GetSceneByGuid(change_info.scene_info().guid());
    //场景不存在了把消息删除,这个文件一定要注意这个队列各种异常情况
    if (entt::null == to_scene)
    {
        //todo
        change_scene_queue->change_scene_queue_.pop_front();
        return kRetEnterSceneSceneNotFound;
    }
    LeaveSceneParam lp;
    lp.leaver_ = player;
    ScenesSystem::LeaveScene(lp);

    const EnterSceneParam enter_scene_param{to_scene, player};
    ScenesSystem::EnterScene(enter_scene_param);
    change_scene_queue->change_scene_queue_.pop_front();

    OnEnterSceneOk(player);
    return kRetOK;
}

uint32_t PlayerChangeSceneSystem::ChangeDiffGsScene(entt::entity player)
{
    auto* const change_scene_queue = tls.registry.try_get<PlayerControllerChangeSceneQueue>(player);
    if (nullptr == change_scene_queue)
    {
        return kRetChangeScenePlayerQueueComponentNull;
    }
    if (change_scene_queue->change_scene_queue_.empty())
    {
        return kRetChangeScenePlayerQueueComponentEmpty;
    }
    if (const auto& change_info = change_scene_queue->change_scene_queue_.front();
        change_info.change_gs_status() == ControllerChangeSceneInfo::eLeaveGsScene)
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
        const auto dest_scene = ScenesSystem::GetSceneByGuid(change_info.scene_info().guid());
        //场景不存在了把消息删除,这个文件一定要注意这个队列各种异常情况
        if (entt::null == dest_scene)
        {
            //todo 考虑直接删除了会不会有异常
            //这时候gate已经更新了新的game node id 又进不去新场景,那我应该让他回到老场景
            change_scene_queue->change_scene_queue_.pop_front();
            const EnterDefaultSceneParam param{player};
            ScenesSystem::EnterDefaultScene(param);
            return kRetEnterSceneSceneNotFound;
        }
        const EnterSceneParam param{dest_scene, player};
        ScenesSystem::EnterScene(param);
    }
    else if (change_info.change_gs_status() == ControllerChangeSceneInfo::eGateEnterGsSceneSucceed)
    {
        change_scene_queue->change_scene_queue_.pop_front();
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
