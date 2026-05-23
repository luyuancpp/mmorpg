#include "bag_marshal.h"

#include <muduo/base/Logging.h>

#include "modules/bag/comp/player_bags_comp.h"
#include "modules/bag/bag_system.h"
#include "thread_context/ecs_context.h"

// bag_marshal — bridge between BagAllData proto and PlayerBagsComp on a
// player entity. Read the header for the full design rationale.
//
// Real implementation as of 2026-05-17:
//   • Marshal walks all four Bag instances in PlayerBagsComp, emitting
//     one ItemEntry per (guid, configId, stackSize, pos, bagType) and
//     one capacity entry per bag.
//   • Unmarshal reverses: for each bag clear the contents, set capacity
//     from the snapshot, then insert items via Bag::InsertItemForRestore
//     so the same guid+pos layout the player saw in the source zone is
//     preserved on the destination.
//
// Schema is the conservative 5-field subset (item_uuid / config_id /
// stack_size / pos / bag_type). Game-design extensions (enchant level,
// affixes, gem inlay, bound state) are documented as TODO at known
// field numbers in proto/common/database/bag_quest_mail_data.proto and
// will land here as the schema grows; the migration path stays
// forward-compatible because proto field numbers are reserved.

namespace bag_marshal
{

void Marshal(entt::entity player, BagAllData& out)
{
    out.Clear();

    const auto* bags = tlsEcs.actorRegistry.try_get<PlayerBagsComp>(player);
    if (bags == nullptr)
    {
        // No bags attached — caller is e.g. a freshly-created player
        // entity that hasn't been through PlayerLifecycleSystem yet.
        // Empty BagAllData is the correct payload.
        return;
    }

    // Capacity slot per EnumBagType, in order kBag/kWarehouse/kEquipment/
    // kTemporary. We always emit kBagMax entries so the consumer can
    // index bag_type → capacity directly without a sparse map.
    out.mutable_capacities()->Reserve(static_cast<int>(kBagMax));
    for (uint32_t bagType = 0; bagType < static_cast<uint32_t>(kBagMax); ++bagType)
    {
        out.add_capacities(static_cast<uint32_t>(bags->bags[bagType].GetCapacity()));
    }

    // Items across all four bags. ItemEntry.bag_type tags which bag
    // each entry belongs to, so the wire shape is one flat list and
    // the consumer can group on read.
    for (uint32_t bagType = 0; bagType < static_cast<uint32_t>(kBagMax); ++bagType)
    {
        const auto& bag = bags->bags[bagType];
        bag.ForEachItem([&out, bagType, &bag](Guid guid, const ItemComp& item) {
            ItemEntry* entry = out.add_items();
            entry->set_item_uuid(static_cast<uint64_t>(guid));
            entry->set_config_id(item.config_id());
            entry->set_stack_size(item.size());
            entry->set_pos(bag.GetItemPosByGuid(guid));
            entry->set_bag_type(bagType);
        });
    }
}

void Unmarshal(entt::entity player, const BagAllData& in)
{
    auto& bags = tlsEcs.actorRegistry.get_or_emplace<PlayerBagsComp>(player);

    // Capacity replay first — InsertItemForRestore validates against
    // capacity_ via the bag's own bookkeeping, so getting capacities
    // straight before items go in matters.
    //
    // The capacities array on the wire SHOULD have exactly kBagMax
    // entries (Marshal always emits that many). If we receive a
    // shorter array (older client / corrupt payload), the missing
    // bags fall through to default capacity_ from the Bag default ctor.
    const int caps = in.capacities_size();
    for (uint32_t bagType = 0; bagType < static_cast<uint32_t>(kBagMax); ++bagType)
    {
        bags.bags[bagType].ResetFromSnapshot();
        if (static_cast<int>(bagType) < caps)
        {
            bags.bags[bagType].SetCapacityForRestore(
                static_cast<std::size_t>(in.capacities(static_cast<int>(bagType))));
        }
    }

    // Items replay. Out-of-range bag_type values (corruption / future
    // bag types we don't support) are dropped with a WARN — better
    // than crashing, and the player just loses those items.
    for (const auto& entry : in.items())
    {
        const uint32_t bagType = entry.bag_type();
        if (bagType >= static_cast<uint32_t>(kBagMax))
        {
            LOG_WARN << "[bag_marshal] dropping ItemEntry with bag_type="
                     << bagType << " (out of range, kBagMax="
                     << static_cast<uint32_t>(kBagMax) << ") "
                     << "item_uuid=" << entry.item_uuid()
                     << " config_id=" << entry.config_id()
                     << " player=" << entt::to_integral(player);
            continue;
        }
        bags.bags[bagType].InsertItemForRestore(
            static_cast<Guid>(entry.item_uuid()),
            entry.config_id(),
            entry.stack_size(),
            entry.pos());
    }
}

}  // namespace bag_marshal
