#pragma once

#include "scene/comp/nav_comp.h"
#include "proto/logic/component/frame_comp.pb.h"
#include "proto/logic/component/player_async_comp.pb.h"

class ThreadLocalStorageGame
{
public:
	SceneNavMapComp sceneNav;
	FrameTime frameTime;
};

extern thread_local ThreadLocalStorageGame tlsGame;

