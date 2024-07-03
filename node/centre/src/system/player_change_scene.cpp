#include "player_change_scene.h"

#include "constants/tips_id.h"
#include "system/scene/scene_system.h"
#include "thread_local/storage.h"

#include "event_proto/scene_event.pb.h"

//todo 各种服务器崩溃
void PlayerChangeSceneSystem::InitChangeSceneQueue(entt::entity player)
{
    tls.registry.emplace<PlayerCentreChangeSceneQueue>(player);
}

uint32_t PlayerChangeSceneSystem::PushChangeSceneInfo(entt::entity player, const CentreChangeSceneInfo& change_info)
{
    auto* const change_scene_queue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
    if (nullptr == change_scene_queue)
    {
        return kRetChangeScenePlayerQueueComponentNull;
    }
    CHECK_CONDITION(change_scene_queue->change_scene_queue_.full(), kRetEnterSceneChangingGs)
    change_scene_queue->change_scene_queue_.push_back(change_info);
    //todo
    change_scene_queue->change_scene_queue_.front().set_change_time(muduo::Timestamp::now().secondsSinceEpoch());
    return kRetOK;
}

void PlayerChangeSceneSystem::PopFrontChangeSceneQueue(entt::entity player)
{
    auto* const change_scene_queue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
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

void PlayerChangeSceneSystem::SetChangeGsStatus(entt::entity player, CentreChangeSceneInfo::eChangeGsStatus s)
{
    auto* const change_scene_queue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
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

void PlayerChangeSceneSystem::CopyTo(CentreChangeSceneInfo& change_info, const SceneInfo& scene_info)
{
    change_info.set_scene_confid(scene_info.scene_confid());
    change_info.set_dungen_confid(scene_info.dungen_confid());
    change_info.set_guid(scene_info.guid());
    change_info.set_mirror_confid(scene_info.mirror_confid());
}

void PlayerChangeSceneSystem::TryProcessChangeSceneQueue(entt::entity player)
{
    auto* const try_change_scene_queue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
    if (nullptr == try_change_scene_queue)
    {
        return;
    }
    auto& change_scene_queue = try_change_scene_queue->change_scene_queue_;
    if (change_scene_queue.empty())
    {
        return;
    }
  
    //同一个gs切换
    if (change_scene_queue.front().change_gs_type() == CentreChangeSceneInfo::eSameGs)
    {
        //就算同gs,队列有消息也不能直接切换，
        const auto& change_info = change_scene_queue.front();
        entt::entity dest_scene = entt::entity{ change_info.guid() };
        //场景不存在了把消息删除,这个文件一定要注意这个队列各种异常情况
        if (entt::null == dest_scene)
        {
            //场景崩溃了，不用换了，玩家不会变，直接从队列里面删除
            PopFrontChangeSceneQueue(player);
            return ;
        }
        ScenesSystem::LeaveScene({ player });
        ScenesSystem::EnterScene({ dest_scene , player });
        PopFrontChangeSceneQueue(player);
        OnEnterSceneOk(player);
        return;
    }
    else if (change_scene_queue.front().change_gs_type() == CentreChangeSceneInfo::eDifferentGs)
    {
        //正在切换
        //切换gs  存储完毕之后才能进入下一个场景
        //放到存储完毕切换场景的队列里面，如果等够足够时间没有存储完毕，可能就是服务器崩溃了,注意，是可能
        if (const auto& change_info = change_scene_queue.front();
            change_info.change_gs_status() == CentreChangeSceneInfo::eLeaveGsScene)
        {
            //正在切换
            //切换gs  存储完毕之后才能进入下一个场景
            //放到存储完毕切换场景的队列里面，如果等够足够时间没有存储完毕，可能就是服务器崩溃了,注意，是可能 
            ScenesSystem::LeaveScene({ player });
        }
        else if (change_info.change_gs_status() ==
            CentreChangeSceneInfo::eEnterGsSceneSucceed)
        {
            //场景不存在了把消息删除,这个文件一定要注意这个队列各种异常情况
            if (const auto dest_scene = entt::entity{ change_info.guid() };
                entt::null == dest_scene)
            {
                //todo 考虑直接删除了会不会有异常
                //这时候gate已经更新了新的game node id 又进不去新场景,那我应该让他回到老场景
                ScenesSystem::EnterDefaultScene({ player });
            }
            else
            {
                ScenesSystem::EnterScene({ dest_scene, player });
            }
        }
        else if (change_info.change_gs_status() == CentreChangeSceneInfo::eGateEnterGsSceneSucceed)
        {
            PopFrontChangeSceneQueue(player);
            OnEnterSceneOk(player);
        }
    }
}

void PlayerChangeSceneSystem::OnEnterSceneOk(entt::entity player)
{
    S2CEnterScene ev;
    ev.set_entity(entt::to_integral(player));
    tls.dispatcher.trigger(ev);
}
