#include "movement.h"

#include "muduo/base/Logging.h"

#include "proto/logic/component/actor_comp.pb.h"
#include <threading/registry_manager.h>

void MovementSystem::Update(const double delta)
{
	auto view = tlsRegistryManager.actorRegistry.view<Transform, Velocity>(entt::exclude<Acceleration>);
	for (auto&& [entity, transform, velocity] : view.each())
	{
		auto& location = *transform.mutable_location();
		location.set_x(location.x() + velocity.x() * delta);
		location.set_y(location.y() + velocity.y() * delta);
		location.set_z(location.z() + velocity.z() * delta);

		// Optional: Log the updated position for debugging or logging purposes
		//LOG_TRACE << "Entity " << entt::to_integral(entity) << " moved to (" << location.x() << ", " << location.y() << ", " << location.z() << ")";
	}
}
