package table

import (
    "log"
    "sync"
)

var loadSuccessCallback func()

// LoadTables 加载所有配置表（同步）
func LoadTables(configDir string) {
    if err := ActorActionCombatStateTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load ActorActionCombatState table: %%v", err)
    }
    if err := ActorActionStateTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load ActorActionState table: %%v", err)
    }
    if err := BuffTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Buff table: %%v", err)
    }
    if err := ClassTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Class table: %%v", err)
    }
    if err := ConditionTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Condition table: %%v", err)
    }
    if err := CooldownTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Cooldown table: %%v", err)
    }
    if err := GlobalVariableTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load GlobalVariable table: %%v", err)
    }
    if err := ItemTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Item table: %%v", err)
    }
    if err := MainSceneTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load MainScene table: %%v", err)
    }
    if err := MessageLimiterTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load MessageLimiter table: %%v", err)
    }
    if err := MissionTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Mission table: %%v", err)
    }
    if err := MonsterBaseTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load MonsterBase table: %%v", err)
    }
    if err := RewardTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Reward table: %%v", err)
    }
    if err := SceneTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Scene table: %%v", err)
    }
    if err := SkillTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Skill table: %%v", err)
    }
    if err := SkillPermissionTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load SkillPermission table: %%v", err)
    }
    if err := TestTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Test table: %%v", err)
    }
    if err := TestMultiKeyTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load TestMultiKey table: %%v", err)
    }

    if loadSuccessCallback != nil {
        loadSuccessCallback()
    }
}

// LoadTablesAsync 异步加载所有配置表（使用 goroutines + WaitGroup）
func LoadTablesAsync(configDir string) {
    var wg sync.WaitGroup
    wg.Add(18)
    go func() {
        defer wg.Done()
        if err := ActorActionCombatStateTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load ActorActionCombatState table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := ActorActionStateTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load ActorActionState table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := BuffTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Buff table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := ClassTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Class table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := ConditionTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Condition table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := CooldownTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Cooldown table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := GlobalVariableTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load GlobalVariable table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := ItemTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Item table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := MainSceneTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load MainScene table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := MessageLimiterTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load MessageLimiter table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := MissionTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Mission table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := MonsterBaseTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load MonsterBase table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := RewardTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Reward table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := SceneTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Scene table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := SkillTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Skill table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := SkillPermissionTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load SkillPermission table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := TestTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Test table: %%v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := TestMultiKeyTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load TestMultiKey table: %%v", err)
        }
    }()

    wg.Wait()

    if loadSuccessCallback != nil {
        loadSuccessCallback()
    }
}

// OnTablesLoadSuccess 注册加载完成后的回调
func OnTablesLoadSuccess(cb func()) {
    loadSuccessCallback = cb
}