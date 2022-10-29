#include "player_change_scene.h"

#include "src/game_logic/game_registry.h"
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/tips_id.h"
#include "src/network/player_session.h"
#include "src/system/player_scene_system.h"


void PlayerChangeSceneSystem::InitChangeSceneQueue(entt::entity player)
{
	registry.emplace<PlayerMsChangeSceneQueue>(player);
}

uint32_t PlayerChangeSceneSystem::ChangeScene(entt::entity player, MsChangeSceneInfo&& change_info)
{
	GetPlayerCompnentMemberReturnError(change_scene_queue, PlayerMsChangeSceneQueue, kRetChangeScenePlayerQueueCompnentNull);
	CheckCondtion(change_scene_queue.full(), kRetChangeScenePlayerQueueCompnentFull);
	change_info.set_change_time(muduo::Timestamp::now().secondsSinceEpoch());//todo
	change_scene_queue.push_back(change_info);
	return kRetOK;
}

//��ǰ������ȥ���㵱ǰ����Ϣ�ǲ���ͬһ��gs

uint32_t PlayerChangeSceneSystem::TryChangeSameGsScene(entt::entity player)
{
	GetPlayerCompnentReturnError(try_change_scene_queue, PlayerMsChangeSceneQueue, kRetChangeScenePlayerQueueCompnentNull);
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

uint32_t PlayerChangeSceneSystem::Gs1SceneToGs2SceneInMyServer(entt::entity player)
{
    GetPlayerCompnentReturnError(try_change_scene_queue, PlayerMsChangeSceneQueue, kRetChangeScenePlayerQueueCompnentNull);
    auto& change_scene_queue = try_change_scene_queue->change_scene_queue_;
    if (change_scene_queue.empty())
    {
        return kRetChangeScenePlayerQueueCompnentEmpty;
    }
	auto& change_info = change_scene_queue.front();
	if (change_info.change_gs_status() == MsChangeSceneInfo::eLeaveGsScene)
	{
        LeaveSceneParam lp;
        lp.leaver_ = player;
        ScenesSystem::LeaveScene(lp);
	}
	else if (change_info.change_gs_status() == MsChangeSceneInfo::eLeaveGsSceneSucceed)
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
		change_info.set_change_gs_status(MsChangeSceneInfo::eEnterGsScene);
	}
	else if (change_info.change_gs_status() == MsChangeSceneInfo::eEnterGsSceneSucceed)
	{
		change_scene_queue.pop_front();//todo
	}
	return kRetOK;
}

uint32_t PlayerChangeSceneSystem::TryPopFrontMsg(entt::entity player)
{
	GetPlayerCompnentMemberReturnError(change_scene_queue, PlayerMsChangeSceneQueue, kRetChangeScenePlayerQueueCompnentNull);
	if (change_scene_queue.empty())
	{
		return kRetOK;
	}
	return kRetOK;
}

void PlayerChangeSceneSystem::TryProcessChangeSceneQueue(entt::entity player)
{
	GetPlayerCompnentMemberReturnVoid(change_scene_queue, PlayerMsChangeSceneQueue);
    if (change_scene_queue.empty())
    {
        return;
    }
	auto& front_msg = change_scene_queue.front();
	if (front_msg.change_gs_type() == MsChangeSceneInfo::eSameGs)//ͬһ��gs�л�
	{
		TryChangeSameGsScene(player);//����ͬgs,��������ϢҲ����ֱ���л���
		return;
	}
	//ͬһ����������ͬgs�л�
	if (front_msg.cross_server_type() == MsChangeSceneInfo::eDotnotCrossServer)
	{
        //�����л�
        //�л�gs  �洢���֮����ܽ�����һ������
        //�ŵ��洢����л������Ķ������棬����ȹ��㹻ʱ��û�д洢��ϣ����ܾ��Ƿ�����������,ע�⣬�ǿ��� 
		Gs1SceneToGs2SceneInMyServer(player);
		return;
	}
}
