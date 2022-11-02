#include "player_change_scene.h"

#include "src/game_logic/game_registry.h"
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/tips_id.h"
#include "src/network/player_session.h"
#include "src/system/player_scene_system.h"

//todo ���ַ���������
void PlayerChangeSceneSystem::InitChangeSceneQueue(entt::entity player)
{
	registry.emplace<PlayerControllerChangeSceneQueue>(player);
}

uint32_t PlayerChangeSceneSystem::PushChangeSceneInfo(entt::entity player, const ControllerChangeSceneInfo& change_info)
{
	GetPlayerCompnentMemberReturnError(change_scene_queue, PlayerControllerChangeSceneQueue, kRetChangeScenePlayerQueueCompnentNull);
	CheckCondtion(change_scene_queue.full(), kRetEnterSceneChangingGs);
	change_scene_queue.push_back(change_info);
    change_scene_queue.front().set_change_time(muduo::Timestamp::now().secondsSinceEpoch());//todo
	return kRetOK;
}

bool PlayerChangeSceneSystem::IsChangeQueueEmpty(entt::entity player)
{
    GetPlayerCompnentMemberNullReturnFalse(change_scene_queue, PlayerControllerChangeSceneQueue);
	return change_scene_queue.empty();
}

void PlayerChangeSceneSystem::TryProcessChangeSceneQueue(entt::entity player)
{
	GetPlayerCompnentMemberReturnVoid(change_scene_queue, PlayerControllerChangeSceneQueue);
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
    GetPlayerCompnentMemberReturnVoid(change_scene_queue, PlayerControllerChangeSceneQueue);
    if (change_scene_queue.empty())
    {
        return;
    }
    change_scene_queue.pop_front();
}

void PlayerChangeSceneSystem::SetChangeGsStatus(entt::entity player, ControllerChangeSceneInfo::eChangeGsStatus s)
{
	GetPlayerCompnentMemberReturnVoid(change_scene_queue, PlayerControllerChangeSceneQueue);
	if (change_scene_queue.empty())
	{
		return;
	}
    change_scene_queue.front().set_change_gs_status(s);
}

void PlayerChangeSceneSystem::TryProcessZoneServerChangeScene(entt::entity player, ControllerChangeSceneInfo& change_info)
{
    //���߿����ֻ����������
    if (change_info.change_cross_server_type() != ControllerChangeSceneInfo::eDotnotCrossServer)
    {
        return;
    }
    if (change_info.change_gs_type() == ControllerChangeSceneInfo::eSameGs)//ͬһ��gs�л�
    {
        TryChangeSameGsScene(player);//����ͬgs,��������ϢҲ����ֱ���л���
        return;
    }
    else if (change_info.change_gs_type() == ControllerChangeSceneInfo::eDifferentGs)
    {
        //�����л�
        //�л�gs  �洢���֮����ܽ�����һ������
        //�ŵ��洢����л������Ķ������棬����ȹ��㹻ʱ��û�д洢��ϣ����ܾ��Ƿ�����������,ע�⣬�ǿ��� 
        ChangeDiffGsScene(player);
        return;
    }
}

void PlayerChangeSceneSystem::TryProcessViaCrossServerChangeScene(entt::entity player, ControllerChangeSceneInfo& change_info)
{
    //�������������
    if (change_info.change_cross_server_type() != ControllerChangeSceneInfo::eCrossServer)
    {
        return;
    }
    //cross server ��������
    if (change_info.change_cross_server_status() != ControllerChangeSceneInfo::eEnterCrossServerSceneSucceed)
    {
        return;
    }
    if (change_info.change_gs_type() == ControllerChangeSceneInfo::eSameGs)//���ͬһ��gs
    {
        TryChangeSameGsScene(player);//����ͬgs,��������ϢҲ����ֱ���л���
        return;
    }   
    else if (change_info.change_gs_type() == ControllerChangeSceneInfo::eDifferentGs)
    {
        ChangeDiffGsScene(player);
        return;
    }    
}

//��ǰ������ȥ���㵱ǰ����Ϣ�ǲ���ͬһ��gs

uint32_t PlayerChangeSceneSystem::TryChangeSameGsScene(entt::entity player)
{
    GetPlayerCompnentReturnError(try_change_scene_queue, PlayerControllerChangeSceneQueue, kRetChangeScenePlayerQueueCompnentNull);
    auto& change_scene_queue = try_change_scene_queue->change_scene_queue_;
    if (change_scene_queue.empty())
    {
        return kRetChangeScenePlayerQueueCompnentEmpty;
    }
    auto& change_info = change_scene_queue.front();
    auto to_scene = ScenesSystem::get_scene(change_info.scene_info().scene_id());
    if (entt::null == to_scene)//�����������˰���Ϣɾ��,����ļ�һ��Ҫע��������и����쳣���
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
    change_scene_queue.pop_front();//�л��ɹ���Ϣɾ��
    return kRetOK;
}

uint32_t PlayerChangeSceneSystem::ChangeDiffGsScene(entt::entity player)
{
    GetPlayerCompnentReturnError(try_change_scene_queue, PlayerControllerChangeSceneQueue, kRetChangeScenePlayerQueueCompnentNull);
    auto& change_scene_queue = try_change_scene_queue->change_scene_queue_;
    if (change_scene_queue.empty())
    {
        return kRetChangeScenePlayerQueueCompnentEmpty;
    }
    auto& change_info = change_scene_queue.front();
    if (change_info.change_gs_status() == ControllerChangeSceneInfo::eLeaveGsScene)
    {
        //�����л�
        //�л�gs  �洢���֮����ܽ�����һ������
        //�ŵ��洢����л������Ķ������棬����ȹ��㹻ʱ��û�д洢��ϣ����ܾ��Ƿ�����������,ע�⣬�ǿ��� 
        LeaveSceneParam lp;
        lp.leaver_ = player;
        ScenesSystem::LeaveScene(lp);
    }
    else if (change_info.change_gs_status() == ControllerChangeSceneInfo::eLeaveGsSceneSucceed)
    {
        auto to_scene = ScenesSystem::get_scene(change_info.scene_info().scene_id());
        if (entt::null == to_scene)//�����������˰���Ϣɾ��,����ļ�һ��Ҫע��������и����쳣���
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
        change_scene_queue.pop_front();//todo
    }
    return kRetOK;
}
