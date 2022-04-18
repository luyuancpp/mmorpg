#include "bag.h"

#include "muduo/base/Logging.h"

#include "src/return_code/error_code.h"

using namespace common;

Item* Bag::GetItem(common::Guid guid)
{
	auto it = items_.find(guid);
	if (it == items_.end())
	{
		return nullptr;
	}
	return &it->second;
}

uint32_t Bag::AddItem(Item& item)
{
	auto p_item_base = item_reg.try_get<ItemBase>(item.entity());
	if (nullptr == p_item_base)
	{
		return RET_BAG_ADD_ITEM_HAS_NOT_BASE_COMPONENT;
	}
	auto& item_base = *p_item_base;
	if (item_base.guid() == kInvalidGuid)
	{
		LOG_ERROR << "bag add item " << player_guid();
		return RET_BAG_ADD_ITEM_INVALID_GUID;
	}
	if (item_base.config_id() <= 0 || item_base.count() <= 0)
	{
		LOG_ERROR << "bag add item" << player_guid();
		return RET_BAG_ADD_ITEM_INVALID_PARAM;
	}

	auto it = items_.find(item_base.guid());
	if (it != items_.end())
	{
		LOG_ERROR << "bag add item" << player_guid();
		return RET_BAG_DELETE_ITEM_HAS_GUID;
	}
	return RET_OK;
}

uint32_t Bag::DelItem(common::Guid guid)
{
	auto it = items_.find(guid);
	if (it == items_.end())
	{
		return RET_BAG_DELETE_ITEM_HASNOT_GUID;
	}
	items_.erase(guid);
	return RET_OK;
}