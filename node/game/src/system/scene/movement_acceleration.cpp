#include "movement_acceleration.h"

#include "thread_local/storage.h"
#include "component_proto/actor_comp.pb.h"
#include "common_proto/comp.pb.h"

void MovementAccelerationSystem::Update(double delta)
{
    auto view = tls.registry.view <Transform, Velocity, Acceleration>();
    for (auto&& [e, transform, velocity, acceleration] : view.each())
    {
        auto& location = *transform.mutable_location();
        location.set_x(location.x() + (velocity.x() + acceleration.x()) * delta);
        location.set_y(location.y() + (velocity.y() + acceleration.y()) * delta);
        location.set_z(location.z() + (velocity.z() + acceleration.z()) * delta);
    }
}
