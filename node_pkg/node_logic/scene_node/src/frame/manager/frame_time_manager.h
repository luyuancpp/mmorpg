#pragma once

#include "proto/logic/component/frame_comp.pb.h"

class FrameTimeManager 
{
public:
	FrameTimeManager () = default;

	FrameTimeManager (const FrameTimeManager &) = delete;
	FrameTimeManager & operator=(const FrameTimeManager &) = delete;

	static FrameTimeManager & Instance() {
		thread_local FrameTimeManager  instance;
		return instance;
	}


	FrameTime frameTime;

};