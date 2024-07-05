#include "world.h"

#include "thread_local/storage_game.h"

#include "component_proto/frame_comp.pb.h"

void World::Init()
{
    tls_game.frame_time_.set_targetfps(60);
}

void World::Update()
{

}

