#pragma once

#include <cstdint>

enum eCondtionType : uint32_t
{
	kConditionKillMonster = 1,//ɱ��
	kConditionTalkWithNpc,//�Ի�
	kConditionCompleteCondition,//�����Ӧ����
	kConditionUseItem,//ʹ����Ʒ
	kConditionInteration,//����
	kConditionLevelUp,//����
	kConditionCustom,//�Զ�������
	kConditionCompleteMission,//�������
	kConditionTypeMax,
};
