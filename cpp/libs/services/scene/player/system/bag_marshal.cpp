#include "bag_marshal.h"

#include <muduo/base/Logging.h>

// See bag_marshal.h header for status — Bag isn't currently attached to
// player entity in production code, so both functions are stubs that
// handle the proto side cleanly but read/write nothing on the ECS side.
//
// When bag attaches to player entity, fill in the bodies below using
// the iteration shape that mirrors player_database_loader.cpp:
//
//   void Marshal(entt::entity player, BagAllData& out) {
//       const auto& bag = tlsEcs.actorRegistry.get<PlayerBagsComp>(player);
//       out.mutable_capacities()->Clear();
//       for (uint32_t bagType = 0; bagType < kBagMax; ++bagType) {
//           out.mutable_capacities()->Add(bag.bags[bagType].capacity());
//           for (const auto& [guid, itemEntity] : bag.bags[bagType].items()) {
//               auto* entry = out.add_items();
//               const auto& itemComp = bag.bags[bagType].itemRegistry().get<ItemComp>(itemEntity);
//               entry->set_item_uuid(guid);
//               entry->set_config_id(itemComp.config_id());
//               entry->set_stack_size(itemComp.size());
//               entry->set_pos(bag.bags[bagType].GetItemPos(guid));
//               entry->set_bag_type(bagType);
//           }
//       }
//   }
//
//   void Unmarshal(entt::entity player, const BagAllData& in) {
//       auto& bag = tlsEcs.actorRegistry.get_or_emplace<PlayerBagsComp>(player);
//       // ... reset all four bags, replay capacities, replay items
//   }
//
// The PlayerBagsComp shape doesn't exist yet — that's the bag-to-player-
// entity attach work in task #21. This file ships now as the proto
// bridge so PlayerAllData has a stable wire surface.

namespace bag_marshal
{
    void Marshal(entt::entity player, BagAllData& out)
    {
        // No-op: out stays empty. PlayerAllData consumers must tolerate
        // empty BagAllData — they always have, since this is the FIRST
        // commit that even mentions bag in PlayerAllData.
        (void)player;
        (void)out;
    }

    void Unmarshal(entt::entity player, const BagAllData& in)
    {
        // No-op for the same reason. If the incoming BagAllData has any
        // items, log it once at WARN — it means an upstream node is
        // already shipping bag data we can't yet apply, and operators
        // need to know we're silently dropping it.
        if (in.items_size() > 0 || in.capacities_size() > 0)
        {
            LOG_WARN << "[bag_marshal] dropping " << in.items_size()
                     << " items + " << in.capacities_size()
                     << " capacities from incoming BagAllData (player="
                     << entt::to_integral(player)
                     << "): bag-to-entity attach not implemented yet "
                     << "(see task #21 + bag_marshal.h header notes)";
        }
    }
}
