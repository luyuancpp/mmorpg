#include "movement.h"

#include "player/comp/afk_comp.h"
#include "player/comp/player_frozen_comp.h"
#include "proto/common/component/actor_comp.pb.h"
#include "thread_context/ecs_context.h"

void MovementSystem::Update(const double delta)
{
	// PlayerFrozenComp exclude: players mid-cross-zone-migration must not
	// keep advancing position on the source side — the destination already
	// has the marshaled Transform and any further movement here would be
	// discarded on ACK + DestroyPlayer. See cross-zone-readiness-audit.md
	// §11.2 (passive-tick exclusion catalogue).
	auto view = tlsEcs.actorRegistry.view<Transform, Velocity>(
		entt::exclude<Acceleration, AfkComp, PlayerFrozenComp>);
	for (auto&& [entity, transform, velocity] : view.each())
	{
		auto& location = *transform.mutable_location();
		location.set_x(location.x() + velocity.x() * delta);
		location.set_y(location.y() + velocity.y() * delta);
		location.set_z(location.z() + velocity.z() * delta);
	}
}

