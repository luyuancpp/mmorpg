#pragma once

#include "scene/comp/nav_comp.h"
#include "proto/logic/component/frame_comp.pb.h"
#include "proto/logic/component/player_async_comp.pb.h"
#include "util/node_id_generator.h"


using NodeIdGenerator32BitId = NodeIdGenerator<uint64_t, 32>;

class ThreadLocalStorageGame
{
public:
	SceneNavMapComp sceneNav;
	FrameTime frameTime;
	NodeIdGenerator32BitId buffIdGenerator;
	NodeIdGenerator32BitId skillIdGenerator;
};

extern thread_local ThreadLocalStorageGame tlsGame;

