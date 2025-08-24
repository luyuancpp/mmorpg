#pragma once

#include "proto/logic/component/scene_comp.pb.h"
#include "util/fast_queue.h"


using ChangeSceneQueuePBComponent = FastQueue<ChangeSceneInfoPBComponent>;