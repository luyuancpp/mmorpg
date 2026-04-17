#include "afk.h"

#include "muduo/base/Logging.h"

#include "core/constants/fps.h"
#include "frame/manager/frame_time.h"
#include "player/comp/afk_comp.h"
#include "proto/common/component/player_comp.pb.h"
#include <thread_context/registry_manager.h>

// A player with no client messages for this many frames is marked AFK.
// 30 seconds * 20 fps = 600 frames.
constexpr uint64_t kAfkInactivityFrames = 30 * kTargetFPS;

void AfkSystem::Update(double delta)
{
    auto& registry = tlsEcs.actorRegistry;
    const uint64_t currentFrame = tlsFrameTimeManager.frameTime.current_frame();

    // 1. Check active players: if idle too long, mark AFK.
    //    Movement, AOI, and attribute sync systems use exclude<AfkComp>,
    //    so AFK players skip expensive per-tick processing automatically.
    auto activeView = registry.view<Player, LastActiveFrameComp>(entt::exclude<AfkComp>);
    for (auto&& [entity, lastActive] : activeView.each())
    {
        if (currentFrame - lastActive.frame >= kAfkInactivityFrames)
        {
            registry.emplace<AfkComp>(entity, AfkComp{currentFrame, 0});
            LOG_INFO << "Player entity " << entt::to_integral(entity)
                     << " marked AFK after " << kAfkInactivityFrames << " frames of inactivity";
        }
    }

    // 2. Check AFK players: if they received new input, remove AFK.
    auto afkView = registry.view<Player, AfkComp, LastActiveFrameComp>();
    for (auto&& [entity, afkComp, lastActive] : afkView.each())
    {
        if (lastActive.frame > afkComp.afkStartFrame)
        {
            LOG_INFO << "Player entity " << entt::to_integral(entity)
                     << " resumed activity, removing AFK state";
            registry.erase<AfkComp>(entity);
        }
    }
}
