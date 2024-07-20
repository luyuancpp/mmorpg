#include "world.h"

#include <ctime>

#include "constants/frame.h"
#include "Recast/Recast.h"
#undef  TEXT
#include "system/player/player_session.h"
#include "system/scene/aoi.h"
#include "system/scene/movement.h"
#include "system/scene/movement_acceleration.h"
#include "thread_local/storage_game.h"

#include "proto/logic/component/frame_comp.pb.h"

using namespace std::chrono;

uint64_t GetTime(void)
{
    return duration_cast<std::chrono::milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void World::InitSystemBeforeConnect()
{
    tls_game.frame_time_.set_previous_time(GetTime());
    tls_game.frame_time_.set_target_fps(kTargetFPS);
    tls_game.frame_time_.set_delta_time(1.0 / tls_game.frame_time_.target_fps());
    
    PlayerSessionSystem::Init();
}

void World::InitSystemAfterConnect()
{
}

void World::Update()
{
    //https://github.com/recastnavigation/recastnavigation.git
    const auto time = GetTime();
    const double dt = (time - tls_game.frame_time_.previous_time()) / 1000.0;
    tls_game.frame_time_.set_previous_time(time);

    auto time_acc = rcClamp(tls_game.frame_time_.time_acc() + dt, -1.0f, 1.0f);
    int sim_iter = 0;
    const auto delta_time = tls_game.frame_time_.delta_time();
    while (time_acc > delta_time)
    {
        time_acc -= delta_time;
        if (sim_iter < 5)
        {
            AoiSystem::Update(delta_time);
            MovementSystem::Update(delta_time);
            MovementAccelerationSystem::Update(delta_time);
        }
        sim_iter++;
    }
    tls_game.frame_time_.set_time_acc(time_acc);
}

