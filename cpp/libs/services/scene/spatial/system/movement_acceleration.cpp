#include "movement_acceleration.h"

#include "player/comp/afk_comp.h"
#include "proto/common/component/actor_comp.pb.h"
#include "thread_context/ecs_context.h"

void MovementAccelerationSystem::Update(const double delta)
{
	auto view = tlsEcs.actorRegistry.view<Transform, Velocity, Acceleration>(entt::exclude<AfkComp>);
	for (auto &&[entity, transform, velocity, acceleration] : view.each())
	{
		auto &location = *transform.mutable_location();
		location.set_x(location.x() + (velocity.x() + acceleration.x()) * delta);
		location.set_y(location.y() + (velocity.y() + acceleration.y()) * delta);
		location.set_z(location.z() + (velocity.z() + acceleration.z()) * delta);
	}
}
