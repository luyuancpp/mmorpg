#pragma once

#include "type_define/type_define.h"

#include "item_util.h"
#include "thread_local/storage.h"

using ItemsMap = std::unordered_map<Guid, entt::entity>;
using PosMap = std::unordered_map<uint32_t, Guid>;

struct BagCapacity
{
	static constexpr std::size_t kDefaultCapacity{10};
	static constexpr std::size_t kEquipmentCapacity{10};
	static constexpr std::size_t kBagMaxCapacity{100};
	static constexpr std::size_t kTempBagMaxCapacity{200};
	static constexpr std::size_t kWarehouseMaxCapacity{200};
	std::size_t size_{kDefaultCapacity};
};

enum EnumBagType : uint32_t
{
	kBag = 0,
	kWarehouse = 1,
	kEquipment = 2,
	kTemporary = 3,
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
	Bag();
	~Bag();
	[[nodiscard]] entt::entity Entity() const { return entity; }
	[[nodiscard]] Guid PlayerGuid() const { return playerGuid; }
	std::size_t size() const { return capacity_.size_; }
	std::size_t ItemGridSize() const { return items_.size(); }
	std::size_t PosSize() const { return pos_.size(); }
	const PosMap& pos() const { return pos_; }

	void set_player(Guid guid) const { tls.itemRegistry.emplace<Guid>(Entity(), guid); }

	[[nodiscard]] std::size_t GetItemStackSize(uint32_t config_id) const;
	ItemPBComp* GetItemBaseByGuid(Guid guid);
	ItemPBComp* GetItemBaseByBos(uint32_t pos);
	entt::entity GetItemByGuid(Guid guid);
	entt::entity GetItemByBos(uint32_t pos);
	uint32_t GetItemPos(Guid guid);

	bool HasItem(const Guid guid) const { return items_.find(guid) != items_.end(); }
	bool IsFull() const { return items_.size() >= size(); }
	bool AdequateSize(std::size_t s) const { sizeassert(); return size() - items_.size() >= s; }
	bool NotAdequateSize(std::size_t s) const { sizeassert(); return size() - items_.size() < s; }
	uint32_t HasEnoughSpace(const U32U32UnorderedMap& tryItems);
	uint32_t HasSufficientItems(const U32U32UnorderedMap& adequate_items);

	uint32_t RemoveItem(const U32U32UnorderedMap& try_del_items);
	uint32_t DelItemByPos(const DelItemByPosParam& p);
	void Neaten();
	uint32_t AddItem(const InitItemParam& itemPBComp);
	uint32_t RemoveItem(Guid del_guid);	
	void Unlock(std::size_t sz);

	entt::registry& ItemRegistry() { return itemRegistry; }

	static Guid GeneratorItemGuid();
	static Guid LastGeneratorItemGuid();

	static bool IsInvalidItemGuid(const ItemPBComp& litem);

	static std::size_t CalcItemStackNeedGridSize(std::size_t item_size, std::size_t stack_size);

	void ToString();
private:
	void DestroyItem(Guid del_guid);
	std::size_t empty_grid_size() const { sizeassert(); return size() - items_.size(); }
	
	void sizeassert() const { assert(size() >= items_.size()); }
	uint32_t OnNewGrid(Guid guid);
	bool CanStack(const ItemPBComp& litem, const ItemPBComp& ritem);
	
	entt::entity entity;
	ItemsMap items_{};
	PosMap pos_{};
	uint32_t type_{};
	BagCapacity capacity_;
	entt::registry itemRegistry;
	Guid playerGuid{ kInvalidGuid };
	
};