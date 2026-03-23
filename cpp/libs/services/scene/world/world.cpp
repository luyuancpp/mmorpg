#include "world.h"

#include "actor/attribute/system/actor_attribute_calculator.h"
#include "actor/attribute/system/actor_state_attribute_sync.h"
#include "combat/buff/system/buff.h"
#include "core/constants/fps.h"
#include "spatial/system/aoi.h"
#include "spatial/system/movement_acceleration.h"
#include "spatial/system/movement.h"
#include "Recast/Recast.h"
#include "frame/manager/frame_time.h"
#include "proto/common/component/frame_comp.pb.h"
#include "core/system/id_generator.h"
#include "node/system/node/node.h"
#include "network/node_utils.h"

using namespace std::chrono;

constexpr int kMaxSimulationIterationsPerFrame = 5;
constexpr double kMillisecondsToSeconds = 1000.0;

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
}

void World::Update()
{
    const auto currentTime = GetTimeInMilliseconds();
    const double deltaTime = static_cast<double>((currentTime - tlsFrameTimeManager.frameTime.previous_time())) / kMillisecondsToSeconds;
    tlsFrameTimeManager.frameTime.set_previous_time(currentTime);

    double accumulatedTime = rcClamp(tlsFrameTimeManager.frameTime.time_accumulator() + deltaTime, -1.0, 1.0);
    int simulationIterations = 0;
    const double fixedDeltaTime = tlsFrameTimeManager.frameTime.delta_time();

    while (accumulatedTime > fixedDeltaTime)
    {
        accumulatedTime -= fixedDeltaTime;
        if (simulationIterations < kMaxSimulationIterationsPerFrame)
        {
            AoiSystem::Update(fixedDeltaTime);
            MovementSystem::Update(fixedDeltaTime);
            MovementAccelerationSystem::Update(fixedDeltaTime);
            BuffSystem::Update(fixedDeltaTime);

            // Attribute calculation must run last (depends on state changes above)
            ActorAttributeCalculatorSystem::Update(fixedDeltaTime);
            ActorStateAttributeSyncSystem::Update(fixedDeltaTime);

            tlsFrameTimeManager.frameTime.set_current_frame(tlsFrameTimeManager.frameTime.current_frame() + 1);
        }
        simulationIterations++;
    }

    tlsFrameTimeManager.frameTime.set_time_accumulator(accumulatedTime);
}
