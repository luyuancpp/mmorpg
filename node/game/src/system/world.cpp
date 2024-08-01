#include "world.h"

#include <ctime>

#include "constants/fps_constants.h"
#include "Recast/Recast.h"
#undef TEXT
#include "system/player/player_session.h"
#include "system/scene/aoi.h"
#include "system/scene/movement.h"
#include "system/scene/movement_acceleration.h"
#include "util/scene/view_util.h"
#include "thread_local/storage_game.h"

#include "proto/logic/component/frame_comp.pb.h"

using namespace std::chrono;

uint64_t GetTimeInMilliseconds()
{
	return duration_cast<std::chrono::milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void World::InitializeSystemBeforeConnect()
{
    tlsGame.frameTime.set_previous_time(GetTimeInMilliseconds());
    tlsGame.frameTime.set_target_fps(kTargetFPS);
    tlsGame.frameTime.set_delta_time(1.0 / tlsGame.frameTime.target_fps());

	PlayerSessionSystem::Initialize();
	ViewUtil::Initialize();
}

void World::InitSystemAfterConnect()
{
	// Initialization logic after connecting
	// Currently empty
}

void World::Update()
{
	//https://github.com/recastnavigation/recastnavigation.git
	const auto currentTime = GetTimeInMilliseconds();
	const double deltaTime = (currentTime - tlsGame.frameTime.previous_time()) / 1000.0;
	tlsGame.frameTime.set_previous_time(currentTime);

	double accumulatedTime = rcClamp(tlsGame.frameTime.time_accumulator() + deltaTime, -1.0, 1.0);
	int simulationIterations = 0;
	const double fixedDeltaTime = tlsGame.frameTime.delta_time();

	while (accumulatedTime > fixedDeltaTime)
	{
		accumulatedTime -= fixedDeltaTime;
		if (simulationIterations < 5)
		{
			AoiSystem::Update(fixedDeltaTime);
			MovementSystem::Update(fixedDeltaTime);
			MovementAccelerationSystem::Update(fixedDeltaTime);
		}
		simulationIterations++;
	}

	tlsGame.frameTime.set_time_accumulator(accumulatedTime);
}
