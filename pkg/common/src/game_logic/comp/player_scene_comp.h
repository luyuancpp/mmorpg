#pragma once

#include "entt/src/entt/entity/entity.hpp"

#include <boost/circular_buffer.hpp>

#include "component_proto/scene_comp.pb.h"

struct PlayerControllerChangeSceneQueue
{
	static const uint8_t  kMaxChangeSceneQueue = 5;
	boost::circular_buffer<ControllerChangeSceneInfo> change_scene_queue_{ kMaxChangeSceneQueue };
};