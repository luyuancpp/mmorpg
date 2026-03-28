
package table

import (
    "fmt"
    "log"
    "sync"
)

var loadSuccessCallback func()

// LoadTables loads all config tables synchronously.
func LoadTables(configDir string) {
    if err := ActorActionCombatStateTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load ActorActionCombatState table: %v", err)
    }
    if err := ActorActionStateTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load ActorActionState table: %v", err)
    }
    if err := BuffTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Buff table: %v", err)
    }
    if err := ClassTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Class table: %v", err)
    }
    if err := ConditionTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Condition table: %v", err)
    }
    if err := CooldownTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Cooldown table: %v", err)
    }
    if err := GlobalVariableTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load GlobalVariable table: %v", err)
    }
    if err := ItemTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Item table: %v", err)
    }
    if err := MainSceneTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load MainScene table: %v", err)
    }
    if err := MessageLimiterTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load MessageLimiter table: %v", err)
    }
    if err := MissionTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Mission table: %v", err)
    }
    if err := MonsterBaseTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load MonsterBase table: %v", err)
    }
    if err := RewardTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Reward table: %v", err)
    }
    if err := SceneTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Scene table: %v", err)
    }
    if err := SkillTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Skill table: %v", err)
    }
    if err := SkillPermissionTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load SkillPermission table: %v", err)
    }
    if err := TestTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load Test table: %v", err)
    }
    if err := TestMultiKeyTableManagerInstance.Load(configDir); err != nil {
        log.Fatalf("failed to load TestMultiKey table: %v", err)
    }

    if loadSuccessCallback != nil {
        loadSuccessCallback()
    }
}

// LoadTablesAsync loads all config tables using goroutines.
func LoadTablesAsync(configDir string) {
    var wg sync.WaitGroup
    wg.Add(18)
    go func() {
        defer wg.Done()
        if err := ActorActionCombatStateTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load ActorActionCombatState table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := ActorActionStateTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load ActorActionState table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := BuffTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Buff table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := ClassTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Class table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := ConditionTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Condition table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := CooldownTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Cooldown table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := GlobalVariableTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load GlobalVariable table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := ItemTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Item table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := MainSceneTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load MainScene table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := MessageLimiterTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load MessageLimiter table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := MissionTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Mission table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := MonsterBaseTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load MonsterBase table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := RewardTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Reward table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := SceneTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Scene table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := SkillTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Skill table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := SkillPermissionTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load SkillPermission table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := TestTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load Test table: %v", err)
        }
    }()
    go func() {
        defer wg.Done()
        if err := TestMultiKeyTableManagerInstance.Load(configDir); err != nil {
            log.Fatalf("failed to load TestMultiKey table: %v", err)
        }
    }()

    wg.Wait()

    if loadSuccessCallback != nil {
        loadSuccessCallback()
    }
}

// OnTablesLoadSuccess registers a callback invoked after all tables load.
func OnTablesLoadSuccess(cb func()) {
    loadSuccessCallback = cb
}

// ReloadTables re-creates all table managers and loads fresh data.
// Safe for hot-reload: replaces the global instances atomically.
func ReloadTables(configDir string) error {
    newActorActionCombatState := NewActorActionCombatStateTableManager()
    if err := newActorActionCombatState.Load(configDir); err != nil {
        return fmt.Errorf("reload ActorActionCombatState failed: %w", err)
    }
    newActorActionState := NewActorActionStateTableManager()
    if err := newActorActionState.Load(configDir); err != nil {
        return fmt.Errorf("reload ActorActionState failed: %w", err)
    }
    newBuff := NewBuffTableManager()
    if err := newBuff.Load(configDir); err != nil {
        return fmt.Errorf("reload Buff failed: %w", err)
    }
    newClass := NewClassTableManager()
    if err := newClass.Load(configDir); err != nil {
        return fmt.Errorf("reload Class failed: %w", err)
    }
    newCondition := NewConditionTableManager()
    if err := newCondition.Load(configDir); err != nil {
        return fmt.Errorf("reload Condition failed: %w", err)
    }
    newCooldown := NewCooldownTableManager()
    if err := newCooldown.Load(configDir); err != nil {
        return fmt.Errorf("reload Cooldown failed: %w", err)
    }
    newGlobalVariable := NewGlobalVariableTableManager()
    if err := newGlobalVariable.Load(configDir); err != nil {
        return fmt.Errorf("reload GlobalVariable failed: %w", err)
    }
    newItem := NewItemTableManager()
    if err := newItem.Load(configDir); err != nil {
        return fmt.Errorf("reload Item failed: %w", err)
    }
    newMainScene := NewMainSceneTableManager()
    if err := newMainScene.Load(configDir); err != nil {
        return fmt.Errorf("reload MainScene failed: %w", err)
    }
    newMessageLimiter := NewMessageLimiterTableManager()
    if err := newMessageLimiter.Load(configDir); err != nil {
        return fmt.Errorf("reload MessageLimiter failed: %w", err)
    }
    newMission := NewMissionTableManager()
    if err := newMission.Load(configDir); err != nil {
        return fmt.Errorf("reload Mission failed: %w", err)
    }
    newMonsterBase := NewMonsterBaseTableManager()
    if err := newMonsterBase.Load(configDir); err != nil {
        return fmt.Errorf("reload MonsterBase failed: %w", err)
    }
    newReward := NewRewardTableManager()
    if err := newReward.Load(configDir); err != nil {
        return fmt.Errorf("reload Reward failed: %w", err)
    }
    newScene := NewSceneTableManager()
    if err := newScene.Load(configDir); err != nil {
        return fmt.Errorf("reload Scene failed: %w", err)
    }
    newSkill := NewSkillTableManager()
    if err := newSkill.Load(configDir); err != nil {
        return fmt.Errorf("reload Skill failed: %w", err)
    }
    newSkillPermission := NewSkillPermissionTableManager()
    if err := newSkillPermission.Load(configDir); err != nil {
        return fmt.Errorf("reload SkillPermission failed: %w", err)
    }
    newTest := NewTestTableManager()
    if err := newTest.Load(configDir); err != nil {
        return fmt.Errorf("reload Test failed: %w", err)
    }
    newTestMultiKey := NewTestMultiKeyTableManager()
    if err := newTestMultiKey.Load(configDir); err != nil {
        return fmt.Errorf("reload TestMultiKey failed: %w", err)
    }

    // Swap all instances at once after all loads succeed.
    ActorActionCombatStateTableManagerInstance = newActorActionCombatState
    ActorActionStateTableManagerInstance = newActorActionState
    BuffTableManagerInstance = newBuff
    ClassTableManagerInstance = newClass
    ConditionTableManagerInstance = newCondition
    CooldownTableManagerInstance = newCooldown
    GlobalVariableTableManagerInstance = newGlobalVariable
    ItemTableManagerInstance = newItem
    MainSceneTableManagerInstance = newMainScene
    MessageLimiterTableManagerInstance = newMessageLimiter
    MissionTableManagerInstance = newMission
    MonsterBaseTableManagerInstance = newMonsterBase
    RewardTableManagerInstance = newReward
    SceneTableManagerInstance = newScene
    SkillTableManagerInstance = newSkill
    SkillPermissionTableManagerInstance = newSkillPermission
    TestTableManagerInstance = newTest
    TestMultiKeyTableManagerInstance = newTestMultiKey

    if loadSuccessCallback != nil {
        loadSuccessCallback()
    }
    return nil
}