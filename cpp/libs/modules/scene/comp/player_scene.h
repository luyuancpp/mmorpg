#pragma once

#include "proto/logic/component/scene_comp.pb.h"
#include "core/utils/data_structures/fast_queue.h"


using ChangeSceneQueuePBComponent = FastQueue<ChangeRoomInfoPBComponent>;