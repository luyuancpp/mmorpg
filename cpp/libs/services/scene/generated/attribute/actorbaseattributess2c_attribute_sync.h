#pragma once
#include <bitset>
#include <boost/dynamic_bitset.hpp>

struct ActorBaseAttributesS2CDirtyMaskComp {
	boost::dynamic_bitset<> dirtyMask; // 全 0
};
void ActorBaseAttributesS2CSyncAttributes(entt::entity entity, uint32 message_id);