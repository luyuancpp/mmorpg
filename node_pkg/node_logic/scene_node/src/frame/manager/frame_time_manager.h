#pragma once

#include "proto/logic/component/frame_comp.pb.h"

class FrameTimeManager 
{
public:
	FrameTimeManager () = default;

	FrameTimeManager (const FrameTimeManager &) = delete;
	FrameTimeManager & operator=(const FrameTimeManager &) = delete;

	FrameTime frameTime;
};

extern thread_local FrameTimeManager  tlsFrameTimeManager;
