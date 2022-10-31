#pragma once

#include "src/game_logic/comp/player_scene_comp.h"

//todo ���Ի�������ʱ������������������׸�������ɾ���Ժ�������������
class PlayerChangeSceneSystem
{
public:
	static void InitChangeSceneQueue(entt::entity player);
	static uint32_t PushChangeSceneInfo(entt::entity player, const MsChangeSceneInfo& change_info);
	static bool IsChangeQueueFull(entt::entity player);
	static bool IsChangeQueueEmpty(entt::entity player);
	static void TryProcessChangeSceneQueue(entt::entity player);
	static void PopFrontChangeSceneQueue(entt::entity player);
	static void SetChangeGsStatus(entt::entity player, MsChangeSceneInfo::eChangeGsStatus s);
private:
	static void TryProcessZoneServerChangeScene(entt::entity player, MsChangeSceneInfo& change_info);//�������������л�,�����
	static void TryProcessViaCrossServerChangeScene(entt::entity player, MsChangeSceneInfo& change_info);//����ͨ�������Ϣ�Ļ�����
	static uint32_t TryChangeSameGsScene(entt::entity player);
	static uint32_t ChangeDiffGsScene(entt::entity player);//�Լ��ķ������ı�gs gs1�ĳ�����gs2�ĳ���
};