#pragma once

#include "entt/src/entt/entity/entity.hpp"

#include <boost/circular_buffer.hpp>

#include "proto/logic/component//scene_comp.pb.h"

static const uint8_t  kMaxChangeSceneQueue = 3;

struct CentrePlayerChangeSceneQueueComp
{
	boost::circular_buffer<CentreChangeSceneInfo> changeSceneQueue{ kMaxChangeSceneQueue };
};