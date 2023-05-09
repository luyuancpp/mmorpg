#pragma once

#include "entt/src/entt/entity/entity.hpp"

#include <boost/circular_buffer.hpp>

#include "src/pb/pbc/component_proto/scene_comp.pb.h"

struct PlayerControllerChangeSceneQueue
{
	static const uint8_t  kMaxChangeSceneQueue = 10;
	boost::circular_buffer<ControllerChangeSceneInfo> change_scene_queue_{ kMaxChangeSceneQueue };
};