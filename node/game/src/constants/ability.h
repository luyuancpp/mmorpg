#pragma once
#include <cstdint>

//�����ͷ�ʱ����ҪĿ�꼴���ͷţ���Ⱥ�ƣ��ȵذ弼�ܣ� -> 1 << 1
//�����ͷ�ʱ��Ҫѡ��Ŀ�꣨����ָ���Լ��ܣ� -> 1 << 2
//�����ͷ�ʱ��Ҫ��ָ���ص�ΪĿ�꣨������AOE���ܣ� -> 1 << 3
constexpr uint32_t kAbilityNoTarget = 1 << 1;       // Skills that can be cast without targeting (e.g., group healing, ground-targeted skills)
constexpr uint32_t kAbilitySingleTarget = 1 << 2;   // Skills that require selecting a single target (e.g., single target healing, single target attack)
constexpr uint32_t kAbilityLocationTarget = 1 << 3;  // Skills that require selecting a location as the target (e.g., AOE skills)