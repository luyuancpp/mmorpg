#include "movement.h"

#include "muduo/base/Logging.h"

#include "player/comp/afk_comp.h"
#include "proto/common/component/actor_comp.pb.h"
#include <thread_context/registry_manager.h>

void MovementSystem::Update(const double delta)
{
	auto view = tlsEcs.actorRegistry.view<Transform, Velocity>(entt::exclude<Acceleration, AfkComp>);
	for (auto&& [entity, transform, velocity] : view.each())
	{
		auto& location = *transform.mutable_location();
		location.set_x(location.x() + velocity.x() * delta);
		location.set_y(location.y() + velocity.y() * delta);
		location.set_z(location.z() + velocity.z() * delta);
	}
}

