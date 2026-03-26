#include "gain_block_service.h"

#include <muduo/base/Logging.h>

// ---------------------------------------------------------------------------
// Thread-local storage — one set per GainType, per scene thread.
// ---------------------------------------------------------------------------

namespace
{

struct GlobalBlockSets
{
    std::unordered_set<uint32_t> currency;
    std::unordered_set<uint32_t> item;
};

thread_local GlobalBlockSets tls_globalBlocks;

std::unordered_set<uint32_t> &ResolveSet(GainBlockService::GainType type)
{
    switch (type)
    {
    case GainBlockService::GainType::kCurrency:
        return tls_globalBlocks.currency;
    case GainBlockService::GainType::kItem:
        return tls_globalBlocks.item;
    }
    // Fallback (should never reach here).
    return tls_globalBlocks.item;
}

// empty sentinel for safety
const std::unordered_set<uint32_t> kEmptySet;

} // namespace

// ---------------------------------------------------------------------------
// Global block / unblock
// ---------------------------------------------------------------------------

void GainBlockService::BlockGlobal(GainType type, uint32_t id)
{
    auto &s = ResolveSet(type);
    if (s.insert(id).second)
    {
        LOG_INFO << "GainBlockService: GLOBAL block type="
                 << static_cast<uint32_t>(type) << " id=" << id;
    }
}

void GainBlockService::UnblockGlobal(GainType type, uint32_t id)
{
    auto &s = ResolveSet(type);
    if (s.erase(id) > 0)
    {
        LOG_INFO << "GainBlockService: GLOBAL unblock type="
                 << static_cast<uint32_t>(type) << " id=" << id;
    }
}

bool GainBlockService::IsGloballyBlocked(GainType type, uint32_t id)
{
    return ResolveSet(type).contains(id);
}

const std::unordered_set<uint32_t> &GainBlockService::GlobalBlockedIds(GainType type)
{
    return ResolveSet(type);
}

void GainBlockService::ClearAllGlobalBlocks()
{
    tls_globalBlocks.currency.clear();
    tls_globalBlocks.item.clear();

    LOG_INFO << "GainBlockService: all global blocks cleared";
}

// ---------------------------------------------------------------------------
// Combined check (global only — per-player handled by caller)
// ---------------------------------------------------------------------------

bool GainBlockService::IsGainBlocked(GainType type, uint32_t id)
{
    return IsGloballyBlocked(type, id);
}
