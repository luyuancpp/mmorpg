#pragma once
#include <cstdint>

//�����ͷ�ʱ����ҪĿ�꼴���ͷţ���Ⱥ�ƣ��ȵذ弼�ܣ� -> 1 << 1
//�����ͷ�ʱ��Ҫѡ��Ŀ�꣨����ָ���Լ��ܣ� -> 1 << 2
//�����ͷ�ʱ��Ҫ��ָ���ص�ΪĿ�꣨������AOE���ܣ� -> 1 << 3
// Enum for target requirements
enum eTargetRequirement {
	kNoTargetRequired = 1 << 0,   // ����ҪĿ��
	kTargetedAbility = 1 << 1,    // ѡ��Ŀ��
	kAreaOfEffect = 1 << 2        // ��ָ���ص�ΪĿ��
};


// Enum for ability types
enum eAbilityType {
	kPassiveAbility = 1 << 0,    // ��������
	kGeneralAbility = 1 << 1,    // ��ͨʩ������
	kChannelAbility = 1 << 2,    // ����ʩ������
	kToggleAbility = 1 << 3,     // �����༼��
	kActivateAbility = 1 << 4    // �����༼��
};