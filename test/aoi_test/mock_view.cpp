#include "game_logic/scene/system/view_system.h"
#include "logic/component/actor_comp.pb.h"
#include "thread_local/storage.h"
#include "type_define/type_define.h"

#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <cstdlib>   // std::getenv
#include <algorithm> // std::max

extern EntityUnorderedMap entitiesToNotifyEntry;
extern EntityUnorderedMap entitiesToNotifyExit;

// -------------------------------------------------------------------------------------------------
// Internal helpers & state (thread-safe)
// -------------------------------------------------------------------------------------------------
namespace {

// Hash for entt::entity so it works as an unordered_map/set key
struct EntityHash {
    std::size_t operator()(entt::entity e) const noexcept {
        return static_cast<std::size_t>(static_cast<std::uint32_t>(e));
    }
};

struct EntityEq {
    bool operator()(entt::entity a, entt::entity b) const noexcept { return a == b; }
};

// Providers allow project-specific plug-ins (no hard dependency on component layout).
// If not set, sensible defaults are used.
struct Providers {
    // Position fetcher
    std::function<Vector3(entt::entity)> getPosition =
        [](entt::entity) -> Vector3 { return Vector3{0.0, 0.0, 0.0}; };

    // Custom max view radius per observer
    std::function<double(entt::entity)> getViewRadius =
        [](entt::entity) -> double {
            // Env override (meters/units)
            static const double env =
                []() -> double {
                    if (const char* v = std::getenv("VIEW_SYSTEM_DEFAULT_RADIUS")) {
                        try { return std::max(0.0, std::stod(v)); } catch (...) { /* ignore */ }
                    }
                    return 60.0; // default
                }();
            return env;
        };

    // NPC discriminator
    std::function<bool(entt::entity)> isNpc =
        [](entt::entity) -> bool { return true; };

    // "Are allies?" discriminator (can suppress enter/leave for same team)
    std::function<bool(entt::entity, entt::entity)> areAllies =
        [](entt::entity, entt::entity) -> bool { return false; };

    // Optional look-at applier
    std::function<void(entt::entity, const Vector3&)> setLookAt =
        [](entt::entity, const Vector3&) {};
};

Providers& providers() {
    static Providers p;
    return p;
}

// Last known visible sets per observer (for de-duplication and exit detection)
using VisibleSet = std::unordered_set<entt::entity, EntityHash, EntityEq>;
static std::unordered_map<entt::entity, VisibleSet, EntityHash, EntityEq> g_visible;
static std::mutex g_visMutex;

// Simple stats (optional)
struct Stats {
    std::atomic<std::uint64_t> enterChecks{0};
    std::atomic<std::uint64_t> entersQueued{0};
    std::atomic<std::uint64_t> exitsQueued{0};
} g_stats;

inline double distSq(const Vector3& a, const Vector3& b) noexcept {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

inline double clampRadius(double r) noexcept {
    // Reasonable clamp to avoid pathological values
    constexpr double kMax = 5000.0;
    return std::max(0.0, std::min(r, kMax));
}

} // namespace

// -------------------------------------------------------------------------------------------------
// Optional: provider registration (call from your composition root / bootstrap).
// If your project wants to use them, declare these in the header; otherwise they remain internal.
// -------------------------------------------------------------------------------------------------
/* Example usage (uncomment declarations in header if you want them public):

void ViewSystem::RegisterPositionProvider(std::function<Vector3(entt::entity)> fn) {
    providers().getPosition = std::move(fn);
}
void ViewSystem::RegisterViewRadiusProvider(std::function<double(entt::entity)> fn) {
    providers().getViewRadius = std::move(fn);
}
void ViewSystem::RegisterNpcPredicate(std::function<bool(entt::entity)> fn) {
    providers().isNpc = std::move(fn);
}
void ViewSystem::RegisterAllyPredicate(std::function<bool(entt::entity, entt::entity)> fn) {
    providers().areAllies = std::move(fn);
}
void ViewSystem::RegisterLookAtApplier(std::function<void(entt::entity, const Vector3&)> fn) {
    providers().setLookAt = std::move(fn);
}
*/

// -------------------------------------------------------------------------------------------------
// ViewSystem implementation
// -------------------------------------------------------------------------------------------------

bool ViewSystem::ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entity) {
    ++g_stats.enterChecks;

    // Ignore self
    if (observer == entity) return false;

    // Skip allies (optional policy)
    if (providers().areAllies(observer, entity)) return false;

    // Only for NPCs
    if (!providers().isNpc(entity)) return false;

    // Range test using default radius
    if (!IsWithinViewRadius(observer, entity)) return false;

    // De-duplicate: if already visible, skip
    {
        std::scoped_lock lk(g_visMutex);
        auto& seen = g_visible[observer];
        if (seen.find(entity) != seen.end())
            return false;
    }

    return true;
}

void ViewSystem::FillActorCreateMessageInfo(entt::entity observer,
                                            entt::entity entity,
                                            ActorCreateS2C& actorCreateMessage) {
    // Queue network/replication entry for external processing
    entitiesToNotifyEntry.emplace(observer, entity);

    // Mark as visible to prevent duplicate enters
    {
        std::scoped_lock lk(g_visMutex);
        g_visible[observer].insert(entity);
    }

    ++g_stats.entersQueued;

    // If your ActorCreateS2C has fields like positions/ids, populate them here.
    // We keep it minimal to avoid project-specific assumptions.
    (void)actorCreateMessage;
}

void ViewSystem::HandlePlayerLeaveMessage(entt::entity observer, entt::entity leaver) {
    entitiesToNotifyExit.emplace(observer, leaver);

    // Remove from visible set; if it wasn't present, that's fine (idempotent)
    {
        std::scoped_lock lk(g_visMutex);
        auto it = g_visible.find(observer);
        if (it != g_visible.end()) {
            it->second.erase(leaver);
            if (it->second.empty()) {
                g_visible.erase(it);
            }
        }
    }

    ++g_stats.exitsQueued;
}

bool ViewSystem::IsWithinViewRadius(entt::entity observer,
                                    entt::entity entrant,
                                    double view_radius) {
    const double r = clampRadius(view_radius);
    if (r <= 0.0) return false;

    const Vector3 po = providers().getPosition(observer);
    const Vector3 pe = providers().getPosition(entrant);
    return distSq(po, pe) <= (r * r);
}

bool ViewSystem::IsWithinViewRadius(entt::entity observer,
                                    entt::entity entrant) {
    // Hysteresis to reduce rapid enter/exit thrashing near the edge.
    // If already visible, allow a 5% larger radius before considering it "out".
    double base = GetMaxViewRadius(observer);
    double radius = base;

    {
        std::scoped_lock lk(g_visMutex);
        auto it = g_visible.find(observer);
        if (it != g_visible.end() && it->second.find(entrant) != it->second.end()) {
            radius *= 1.05; // 5% grace
        }
    }

    return IsWithinViewRadius(observer, entrant, radius);
}

double ViewSystem::GetMaxViewRadius(entt::entity observer) {
    return clampRadius(providers().getViewRadius(observer));
}

void ViewSystem::LookAtPosition(entt::entity entity, const Vector3& pos) {
    // Delegate to project-provided applier (e.g., set yaw/pitch on your transform/actor component)
    providers().setLookAt(entity, pos);
}

