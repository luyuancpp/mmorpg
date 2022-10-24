#pragma once

#include "entt/src/entt/entity/entity.hpp"

#include <boost/circular_buffer.hpp>

#include "src/pb/pbc/component_proto/scene_comp.pb.h"

struct PlayerMsChangeSceneQueue
{
	static const uint8_t  kMaxChangeSceneQueue = 5;
	boost::circular_buffer<MsChangeSceneInfo> change_scene_queue_{ kMaxChangeSceneQueue };
	uint16_t change_scene_sequence_ = 0;
};