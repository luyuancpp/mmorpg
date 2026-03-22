#pragma once

#include "engine/core/type_define/type_define.h"

#include "proto/common/component/item_base_comp.pb.h"

using EntityVector = std::vector<entt::entity>;

struct InitItemParam
{
	InitItemParam() { itemPBComp.set_size(1); itemPBComp.set_item_id(kInvalidGuid); }
	ItemPBComponent itemPBComp;
};

//todo equipment list with unique guids per piece
void InitItem(InitItemParam& param);