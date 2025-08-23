#pragma once

#include "type_define/type_define.h"

#include "proto/logic/component/item_base_comp.pb.h"

using EntityVector = std::vector<entt::entity>;

struct InitItemParam
{
	InitItemParam() { itemPBComp.set_size(1); itemPBComp.set_item_id(kInvalidGuid); }
	ItemPBComponent itemPBComp;
};

//todo 放装备列表，装备有一堆自己的guild
void InitItem(InitItemParam& param);