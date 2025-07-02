#pragma once

#include <boost/circular_buffer.hpp>

#include "proto/logic/component/scene_comp.pb.h"

static const uint8_t  kMaxChangeSceneQueue = 3;

struct ChangeSceneQueuePBComponent
{
	boost::circular_buffer<ChangeSceneInfoPBComponent> changeSceneQueue{ kMaxChangeSceneQueue };
};