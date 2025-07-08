#include "world.h"


#include "actor/attribute/system/actor_attribute_calculator_system.h"
#include "actor/attribute/system/actor_state_attribute_sync_system.h"
#include "combat/buff/system/buff_system.h"
#include "core/constants/fps_constants.h"
#include "scene/system/aoi_system.h"
#include "scene/system/movement_acceleration_system.h"
#include "scene/system/movement_system.h"
#include "scene/system/view_system.h"
#include "Recast/Recast.h"
#include "thread_local/storage_game.h"
#include "type_alias/player_session_type_alias.h"
#include "proto/logic/component/frame_comp.pb.h"
#include "thread_local/storage.h"

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

    ViewSystem::Initialize();
}

void World::ReadyForGame()
{
}

void World::Update()
{
    //https://github.com/recastnavigation/recastnavigation.git
    const auto currentTime = GetTimeInMilliseconds();
    const double deltaTime = static_cast<double>((currentTime - tlsGame.frameTime.previous_time())) / 1000.0;
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
            BuffSystem::Update(fixedDeltaTime);

            //属性计算必须放到最后
            ActorAttributeCalculatorSystem::Update(fixedDeltaTime);
            ActorStateAttributeSyncSystem::Update(fixedDeltaTime);

            tlsGame.frameTime.set_current_frame(tlsGame.frameTime.current_frame() + 1);
        }
        simulationIterations++;
    }

    tlsGame.frameTime.set_time_accumulator(accumulatedTime);
}
