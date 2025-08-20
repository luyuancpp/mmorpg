#pragma once

#include "proto/logic/component/frame_comp.pb.h"

class ThreadLocalStorageGame
{
public:
	FrameTime frameTime;
};

extern thread_local ThreadLocalStorageGame tlsGame;

