#pragma once

class AfkSystem
{
public:
    // Called every frame from World::Update (before movement/AOI).
    // 1. Scans players with LastActiveFrameComp — if idle > 30s, adds AfkComp.
    // 2. If a player received new input (LastActiveFrameComp updated), removes AfkComp.
    //
    // MovementSystem 通过 entt::exclude<AfkComp> 跳过挂机玩家。
    // AoiSystem, BuffSystem, ActorAttributeCalculatorSystem, and
    // ActorStateAttributeSyncSystem still run for AFK players
    // (AFK players remain visible; buffs must expire; attributes stay consistent).
    static void Update(double delta);
};
