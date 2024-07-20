#include "movement.h"

#include "thread_local/storage.h"
#include "proto/logic/component/actor_comp.pb.h"

void MovementSystem::Update(double delta)
{
    auto view = tls.registry.view <Transform, Velocity>(entt::exclude<Acceleration>);
    for (auto&& [e, transform, velocity] : view.each())
    {
        auto& location = *transform.mutable_location();
        location.set_x(location.x() + velocity.x() * delta);
        location.set_y(location.y() + velocity.y() * delta);
        location.set_z(location.z() + velocity.z() * delta);
    }
}
