#include "movement_acceleration_system.h"

#include "proto/logic/component/comp.pb.h"
#include "proto/logic/component/actor_comp.pb.h"
#include "thread_local/storage.h"

void MovementAccelerationSystem::Update(const double delta)
{
	auto view = tls.actorRegistry.view<Transform, Velocity, Acceleration>();
	for (auto&& [entity, transform, velocity, acceleration] : view.each())
	{
		auto& location = *transform.mutable_location();
		location.set_x(location.x() + (velocity.x() + acceleration.x()) * delta);
		location.set_y(location.y() + (velocity.y() + acceleration.y()) * delta);
		location.set_z(location.z() + (velocity.z() + acceleration.z()) * delta);

		// Optional: Log the updated position for debugging or logging purposes
		//LOG_TRACE << "Entity " << entity << " moved to (" << location.x() << ", " << location.y() << ", " << location.z() << ")";
	}
}
