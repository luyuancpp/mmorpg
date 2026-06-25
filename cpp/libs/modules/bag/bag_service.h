#pragma once

#include <unordered_set>

#include "engine/core/type_define/type_define.h"

#include "bag_system.h"

// Per-player item block list (GM feature).
// Stored separately from Bag — Bag is a pure container.
struct PlayerItemBlockList
{
	void Block(uint32_t configId);
	void Unblock(uint32_t configId);
	bool IsBlocked(uint32_t configId) const;
	const std::unordered_set<uint32_t> &All() const { return blocked_config_ids_; }

private:
	std::unordered_set<uint32_t> blocked_config_ids_;
};

// Orchestration layer for bag operations.
//
// Handles cross-cutting concerns that do NOT belong in the Bag container:
//   - Global (server-wide) item block check  (GainBlockService)
//   - Per-player GM item block check          (PlayerItemBlockList)
//   - Transaction logging                     (TransactionLogSystem)
//   - Anomaly detection                       (AnomalyDetector)
//
// Bag itself is a pure container (grid / slot / stack management only).
class BagService
{
public:
	// Orchestrated AddItem:
	//   block check → Bag::AddItem → transaction log → anomaly detection
	static uint32_t AddItem(
		entt::entity playerEntity,
		Bag &bag,
		const PlayerItemBlockList &blockList,
		const InitItemParam &param);

	// Orchestrated batch AddItems (config_id → count):
	//   transactional — all-or-nothing space check, then per-config
	//   block check → Bag::AddItem → transaction log → anomaly detection.
	//   Symmetric with Bag::RemoveItems.
	static uint32_t AddItems(
		entt::entity playerEntity,
		Bag &bag,
		const PlayerItemBlockList &blockList,
		const ItemCountMap &itemsToAdd);

	// Orchestrated batch AddItems carrying full ItemComp per piece
	//   (mail attachments mixing equipment + stackable items): preserves
	//   each piece's preassigned guid / attributes. Transactional —
	//   all-or-nothing space check, then per-piece
	//   block check → Bag::AddItem → transaction log → anomaly detection.
	static uint32_t AddItems(
		entt::entity playerEntity,
		Bag &bag,
		const PlayerItemBlockList &blockList,
		const std::vector<InitItemParam> &itemsToAdd);

	// Orchestrated RemoveItem:
	//   capture item info → Bag::RemoveItem → transaction log
	static uint32_t RemoveItem(
		entt::entity playerEntity,
		Bag &bag,
		Guid guid);
};
