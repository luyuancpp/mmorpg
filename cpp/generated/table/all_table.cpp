﻿#include "all_table.h"

#include <thread>
#include "muduo/base/CountDownLatch.h"

#include "actoractioncombatstate_table.h"
#include "actoractionstate_table.h"
#include "buff_table.h"
#include "class_table.h"
#include "condition_table.h"
#include "cooldown_table.h"
#include "globalvariable_table.h"
#include "item_table.h"
#include "mainscene_table.h"
#include "messagelimiter_table.h"
#include "mission_table.h"
#include "monsterbase_table.h"
#include "reward_table.h"
#include "scene_table.h"
#include "skill_table.h"
#include "skillpermission_table.h"
#include "test_table.h"
#include "testmultikey_table.h"

LoadSuccessCallback loadSuccessCallback;

void LoadTables() {

    ActorActionCombatStateTableManager::Instance().Load();
    ActorActionCombatStateTableManager::Instance().LoadSuccess();

    ActorActionStateTableManager::Instance().Load();
    ActorActionStateTableManager::Instance().LoadSuccess();

    BuffTableManager::Instance().Load();
    BuffTableManager::Instance().LoadSuccess();

    ClassTableManager::Instance().Load();
    ClassTableManager::Instance().LoadSuccess();

    ConditionTableManager::Instance().Load();
    ConditionTableManager::Instance().LoadSuccess();

    CooldownTableManager::Instance().Load();
    CooldownTableManager::Instance().LoadSuccess();

    GlobalVariableTableManager::Instance().Load();
    GlobalVariableTableManager::Instance().LoadSuccess();

    ItemTableManager::Instance().Load();
    ItemTableManager::Instance().LoadSuccess();

    MainSceneTableManager::Instance().Load();
    MainSceneTableManager::Instance().LoadSuccess();

    MessageLimiterTableManager::Instance().Load();
    MessageLimiterTableManager::Instance().LoadSuccess();

    MissionTableManager::Instance().Load();
    MissionTableManager::Instance().LoadSuccess();

    MonsterBaseTableManager::Instance().Load();
    MonsterBaseTableManager::Instance().LoadSuccess();

    RewardTableManager::Instance().Load();
    RewardTableManager::Instance().LoadSuccess();

    SceneTableManager::Instance().Load();
    SceneTableManager::Instance().LoadSuccess();

    SkillTableManager::Instance().Load();
    SkillTableManager::Instance().LoadSuccess();

    SkillPermissionTableManager::Instance().Load();
    SkillPermissionTableManager::Instance().LoadSuccess();

    TestTableManager::Instance().Load();
    TestTableManager::Instance().LoadSuccess();

    TestMultiKeyTableManager::Instance().Load();
    TestMultiKeyTableManager::Instance().LoadSuccess();


    if (loadSuccessCallback){
        loadSuccessCallback();
    }
}

void LoadTablesAsync() {
    static muduo::CountDownLatch latch(18);

    std::thread ActorActionCombatStateLoadThread([](){
        ActorActionCombatStateTableManager::Instance().Load();
            latch.countDown();});
            ActorActionCombatStateLoadThread.detach();

    std::thread ActorActionStateLoadThread([](){
        ActorActionStateTableManager::Instance().Load();
            latch.countDown();});
            ActorActionStateLoadThread.detach();

    std::thread BuffLoadThread([](){
        BuffTableManager::Instance().Load();
            latch.countDown();});
            BuffLoadThread.detach();

    std::thread ClassLoadThread([](){
        ClassTableManager::Instance().Load();
            latch.countDown();});
            ClassLoadThread.detach();

    std::thread ConditionLoadThread([](){
        ConditionTableManager::Instance().Load();
            latch.countDown();});
            ConditionLoadThread.detach();

    std::thread CooldownLoadThread([](){
        CooldownTableManager::Instance().Load();
            latch.countDown();});
            CooldownLoadThread.detach();

    std::thread GlobalVariableLoadThread([](){
        GlobalVariableTableManager::Instance().Load();
            latch.countDown();});
            GlobalVariableLoadThread.detach();

    std::thread ItemLoadThread([](){
        ItemTableManager::Instance().Load();
            latch.countDown();});
            ItemLoadThread.detach();

    std::thread MainSceneLoadThread([](){
        MainSceneTableManager::Instance().Load();
            latch.countDown();});
            MainSceneLoadThread.detach();

    std::thread MessageLimiterLoadThread([](){
        MessageLimiterTableManager::Instance().Load();
            latch.countDown();});
            MessageLimiterLoadThread.detach();

    std::thread MissionLoadThread([](){
        MissionTableManager::Instance().Load();
            latch.countDown();});
            MissionLoadThread.detach();

    std::thread MonsterBaseLoadThread([](){
        MonsterBaseTableManager::Instance().Load();
            latch.countDown();});
            MonsterBaseLoadThread.detach();

    std::thread RewardLoadThread([](){
        RewardTableManager::Instance().Load();
            latch.countDown();});
            RewardLoadThread.detach();

    std::thread SceneLoadThread([](){
        SceneTableManager::Instance().Load();
            latch.countDown();});
            SceneLoadThread.detach();

    std::thread SkillLoadThread([](){
        SkillTableManager::Instance().Load();
            latch.countDown();});
            SkillLoadThread.detach();

    std::thread SkillPermissionLoadThread([](){
        SkillPermissionTableManager::Instance().Load();
            latch.countDown();});
            SkillPermissionLoadThread.detach();

    std::thread TestLoadThread([](){
        TestTableManager::Instance().Load();
            latch.countDown();});
            TestLoadThread.detach();

    std::thread TestMultiKeyLoadThread([](){
        TestMultiKeyTableManager::Instance().Load();
            latch.countDown();});
            TestMultiKeyLoadThread.detach();

    latch.wait();


    ActorActionCombatStateTableManager::Instance().LoadSuccess();
    ActorActionStateTableManager::Instance().LoadSuccess();
    BuffTableManager::Instance().LoadSuccess();
    ClassTableManager::Instance().LoadSuccess();
    ConditionTableManager::Instance().LoadSuccess();
    CooldownTableManager::Instance().LoadSuccess();
    GlobalVariableTableManager::Instance().LoadSuccess();
    ItemTableManager::Instance().LoadSuccess();
    MainSceneTableManager::Instance().LoadSuccess();
    MessageLimiterTableManager::Instance().LoadSuccess();
    MissionTableManager::Instance().LoadSuccess();
    MonsterBaseTableManager::Instance().LoadSuccess();
    RewardTableManager::Instance().LoadSuccess();
    SceneTableManager::Instance().LoadSuccess();
    SkillTableManager::Instance().LoadSuccess();
    SkillPermissionTableManager::Instance().LoadSuccess();
    TestTableManager::Instance().LoadSuccess();
    TestMultiKeyTableManager::Instance().LoadSuccess();
     if (loadSuccessCallback){
        loadSuccessCallback();
    }
}

void OnTablesLoadSuccess(const LoadSuccessCallback& callback){
loadSuccessCallback = callback;
}