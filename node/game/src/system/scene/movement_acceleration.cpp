#include "movement_acceleration.h"

#include "thread_local/storage.h"
#include "component_proto/actor_comp.pb.h"
#include "common_proto/comp.pb.h"

void MovementAccelerationSystem::Update(double delta)
{
    auto view = tls.registry.view <Transform, Velocity, Acceleration>();
    for (auto&& [e, transform, velocity, acceleration] : view.each())
    {
        auto& location = *transform.mutable_localtion();
        auto x_vel = (velocity.x() + acceleration.x()) * delta;
        auto y_vel = (velocity.y() + acceleration.y()) * delta;
        auto z_vel = (velocity.z() + acceleration.z()) * delta;
        location.set_x(location.x() + x_vel);
        location.set_y(location.y() + y_vel);
        location.set_z(location.z() + z_vel);
    }
}
