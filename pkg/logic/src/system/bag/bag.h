#pragma once

#include <unordered_map>

#include "item.h"

using ItemsMap = std::unordered_map<Guid, Item>;
using PosMap = std::unordered_map<uint32_t, Guid>;

struct BagCapacity
{
	static constexpr std::size_t kDefaultCapacity{10}; //背包默认大小
	static constexpr std::size_t kEquipmentCapacity{10}; //装备栏默认大小
	static constexpr std::size_t kBagMaxCapacity{100};
	static constexpr std::size_t kTempBagMaxCapacity{200};
	static constexpr std::size_t kWarehouseMaxCapacity{200};
	std::size_t size_{kDefaultCapacity}; //当前背包容量
};

enum EnumBagType : uint32_t
{
	//普通背包
	kBag = 0,
	//仓库
	kWarehouse = 1,
	//装备
	kEquipment = 2,
	//临时背包
	kTemporary = 3,
	//占位
	kBagMax = 4,
};

struct DelItemByPosParam
{
	Guid item_guid_{kInvalidGuid};
	uint32_t item_config_id_{kInvalidU32Id};
	uint32_t pos_{kInvalidU32Id};
	uint32_t size_{1};
};

class Bag
{
public:
	using BagEntity = ItemEntity;
	[[nodiscard]] entt::entity entity() const { return entity_.entity(); }
	[[nodiscard]] Guid player_guid() const { return tls.itemRegistry.get<Guid>(entity()); }
	inline std::size_t size() const { return tls.itemRegistry.get<BagCapacity>(entity()).size_; }
	inline std::size_t item_size() const { return items_.size(); }
	inline std::size_t pos_size() const { return pos_.size(); }
	inline const PosMap& pos() const { return pos_; }

	inline void set_player(Guid guid) const { tls.itemRegistry.emplace<Guid>(entity(), guid); }

	[[nodiscard]] std::size_t GetItemStackSize(uint32_t config_id) const;
	Item* GetItemByGuid(Guid guid);
	Item* GetItemByBos(uint32_t pos);
	uint32_t GetItemPos(Guid guid); //just for test

	inline bool HasItem(const Guid guid) const { return items_.find(guid) != items_.end(); }
	inline bool IsFull() const { return items_.size() >= size(); }
	inline bool AdequateSize(std::size_t s) const { sizeassert(); return size() - items_.size() >= s; }//足够空格子
	inline bool NotAdequateSize(std::size_t s) const { sizeassert(); return size() - items_.size() < s; }//足够空格子
	uint32_t AdequateSizeAddItem(const UInt32UInt32UnorderedMap& try_items); //足够空间放物品
	uint32_t AdequateItem(const UInt32UInt32UnorderedMap& adequate_items); //检测是否足够物品

	uint32_t DelItem(const UInt32UInt32UnorderedMap& try_del_items);
	uint32_t DelItemByPos(const DelItemByPosParam& p);
	void Neaten();
	uint32_t AddItem(const Item&  add_item);
	uint32_t DelItem(Guid del_guid);	
	void Unlock(std::size_t sz);
private:
	inline std::size_t empty_grid_size() const { sizeassert(); return size() - items_.size(); }
	std::size_t calc_item_need_grid_size(std::size_t item_size, std::size_t stack_size);
	void sizeassert() const { assert(size() >= items_.size()); }
	uint32_t OnNewGrid(const Item& item);
	bool CanStack(const Item& litem, const Item& ritem);
	
	BagEntity entity_;
	ItemsMap items_{};
	PosMap pos_{};
	uint32_t type_{};
	BagCapacity capacity_;
};