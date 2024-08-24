#pragma once

#include "type_define/type_define.h"

#include "proto/logic/component/item_base_comp.pb.h"

using EntityVector = std::vector<entt::entity>;

struct InitItemParam
{
	InitItemParam() { itemPBComp.set_size(1); }
	ItemPBComp itemPBComp;
};

//todo ��װ���б�װ����һ���Լ���guild
void InitItem(InitItemParam& param);