#include "actorbaseattributess2c_attribute_sync.h"

#include "engine/threading/registry_manager.h"
#include "scene/scene/comp/scene_node_scene.h"

// ============================================================================
// ActorBaseAttributesS2C Attribute Sync
// ============================================================================
void ActorBaseAttributesS2CSyncAttributes(entt::entity entity, uint32_t message_id)
{
    auto& registry = tlsRegistryManager.actorRegistry;

    // Fetch AOI and dirty mask
    const auto& aoi_list      = registry.get_or_emplace<AoiListComp>(entity);
    auto&       dirty_mask    = registry.get_or_emplace<ActorBaseAttributesS2CDirtyMaskComp>(entity);

    ActorBaseAttributesS2C sync_msg;
    const auto* msg_desc = sync_msg.GetDescriptor();

    // Iterate protobuf fields
    for (int idx = 0; idx < msg_desc->field_count(); ++idx)
    {
        const auto* field     = msg_desc->field(idx);
        int         field_num = field->number();

        // Skip clean fields
        if (!dirty_mask.dirtyMask.test(field_num))
            continue;

        // Switch each attribute field
        switch (field_num)
        {
        case ActorBaseAttributesS2C::kEntityIdFieldNumber:
        {
            // Sync entity_id
            auto& comp = registry.get_or_emplace<EntityId>(entity);
            sync_msg.mutable_entity_id()->CopyFrom(comp);
            break;
        }
        case ActorBaseAttributesS2C::kTransformFieldNumber:
        {
            // Sync transform
            auto& comp = registry.get_or_emplace<Transform>(entity);
            sync_msg.mutable_transform()->CopyFrom(comp);
            break;
        }
        case ActorBaseAttributesS2C::kVelocityFieldNumber:
        {
            // Sync velocity
            auto& comp = registry.get_or_emplace<Velocity>(entity);
            sync_msg.mutable_velocity()->CopyFrom(comp);
            break;
        }
        case ActorBaseAttributesS2C::kCombatStateFlagsFieldNumber:
        {
            // Sync combat_state_flags
            auto& comp = registry.get_or_emplace<CombatStateFlags>(entity);
            sync_msg.mutable_combat_state_flags()->CopyFrom(comp);
            break;
        }
        default:
            break;
        }
    }

    // No changes → no broadcast
    if (sync_msg.ByteSizeLong() == 0)
        return;

    // Broadcast to AOI players
    BroadcastMessageToPlayers(
        message_id,
        sync_msg,
        aoi_list.aoiList
    );

    // Cleanup
    sync_msg.Clear();
}
