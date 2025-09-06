#include "world.h"

#include "actor/attribute/system/actor_attribute_calculator.h"
#include "actor/attribute/system/actor_state_attribute_sync.h"
#include "scene/combat/buff/system/buff.h"
#include "core/constants/fps_constants.h"
#include "scene/scene/system/aoi_system.h"
#include "scene/scene/system/movement_acceleration_system.h"
#include "scene/scene/system/movement_system.h"
#include "scene/scene/system/view_system.h"
#include "Recast/Recast.h"
#include "frame/manager/frame_time.h"
#include "type_alias/player_session_type_alias.h"
#include "proto/logic/component/frame_comp.pb.h"
#include "core/system/id_generator_manager.h"
#include "node/system/node/node.h"
#include "network/node_utils.h"
#include "frame/manager/frame_time.h"

using namespace std::chrono;

uint64_t GetTimeInMilliseconds()
{
    return duration_cast<std::chrono::milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void World::InitializeSystemBeforeConnect()
{
    tlsFrameTimeManager.frameTime.set_previous_time(GetTimeInMilliseconds());
    tlsFrameTimeManager.frameTime.set_target_fps(kTargetFPS);
    tlsFrameTimeManager.frameTime.set_delta_time(1.0 / tlsFrameTimeManager.frameTime.target_fps());
    tlsIdGeneratorManager.SetNodeId(GetNodeInfo().node_id());
    ViewSystem::Initialize();
}

void World::ReadyForGame()
{
}

void World::Update()
{
    //https://github.com/recastnavigation/recastnavigation.git
    const auto currentTime = GetTimeInMilliseconds();
    const double deltaTime = static_cast<double>((currentTime - tlsFrameTimeManager.frameTime.previous_time())) / 1000.0;
    tlsFrameTimeManager.frameTime.set_previous_time(currentTime);

    double accumulatedTime = rcClamp(tlsFrameTimeManager.frameTime.time_accumulator() + deltaTime, -1.0, 1.0);
    int simulationIterations = 0;
    const double fixedDeltaTime = tlsFrameTimeManager.frameTime.delta_time();

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

            tlsFrameTimeManager.frameTime.set_current_frame(tlsFrameTimeManager.frameTime.current_frame() + 1);
        }
        simulationIterations++;
    }

    tlsFrameTimeManager.frameTime.set_time_accumulator(accumulatedTime);
}
