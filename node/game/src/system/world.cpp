#include "world.h"

#include <ctime>

#include "constants/frame.h"
#include "thread_local/storage_game.h"
#include "Recast/Recast.h"
#include "system/scene/aoi.h"

#include "component_proto/frame_comp.pb.h"

using namespace std::chrono;

uint64_t GetTime(void)
{
    return duration_cast<std::chrono::milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void World::Init()
{
    tls_game.frame_time_.set_previous_time(GetTime());
    tls_game.frame_time_.set_target_fps(kTargetFPS);
    tls_game.frame_time_.set_delta_time(1.0 / tls_game.frame_time_.target_fps());
}

void World::Update()
{
    //https://github.com/recastnavigation/recastnavigation.git
    auto time = GetTime();
    double dt = (time - tls_game.frame_time_.previous_time()) / 1000.0;
    auto time_acc = rcClamp(tls_game.frame_time_.time_acc() + dt, -1.0f, 1.0f);
    int sim_iter = 0;
    while (time_acc > tls_game.frame_time_.delta_time())
    {
        time_acc -= tls_game.frame_time_.delta_time();
        if (sim_iter < 5)
        {
            AoiSystem::Update(tls_game.frame_time_.delta_time());
        }
        sim_iter++;
    }
    tls_game.frame_time_.set_time_acc(time_acc);
    tls_game.frame_time_.set_previous_time(time);
}

