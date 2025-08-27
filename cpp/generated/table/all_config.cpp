#include "all_config.h"

#include <thread>
#include "muduo/base/CountDownLatch.h"

#include "actoractioncombatstate_config.h"
#include "actoractionstate_config.h"
#include "buff_config.h"
#include "class_config.h"
#include "condition_config.h"
#include "cooldown_config.h"
#include "globalvariable_config.h"
#include "item_config.h"
#include "mainscene_config.h"
#include "messagelimiter_config.h"
#include "mission_config.h"
#include "monsterbase_config.h"
#include "reward_config.h"
#include "scene_config.h"
#include "skill_config.h"
#include "skillpermission_config.h"
#include "test_config.h"
#include "testmultikey_config.h"

LoadSuccessCallback loadSuccessCallback;

void LoadTables() {

    ActorActionCombatStateConfigurationTable::Instance().Load();
    ActorActionCombatStateConfigurationTable::Instance().LoadSuccess();

    ActorActionStateConfigurationTable::Instance().Load();
    ActorActionStateConfigurationTable::Instance().LoadSuccess();

    BuffConfigurationTable::Instance().Load();
    BuffConfigurationTable::Instance().LoadSuccess();

    ClassConfigurationTable::Instance().Load();
    ClassConfigurationTable::Instance().LoadSuccess();

    ConditionConfigurationTable::Instance().Load();
    ConditionConfigurationTable::Instance().LoadSuccess();

    CooldownConfigurationTable::Instance().Load();
    CooldownConfigurationTable::Instance().LoadSuccess();

    GlobalVariableConfigurationTable::Instance().Load();
    GlobalVariableConfigurationTable::Instance().LoadSuccess();

    ItemConfigurationTable::Instance().Load();
    ItemConfigurationTable::Instance().LoadSuccess();

    MainSceneConfigurationTable::Instance().Load();
    MainSceneConfigurationTable::Instance().LoadSuccess();

    MessageLimiterConfigurationTable::Instance().Load();
    MessageLimiterConfigurationTable::Instance().LoadSuccess();

    MissionConfigurationTable::Instance().Load();
    MissionConfigurationTable::Instance().LoadSuccess();

    MonsterBaseConfigurationTable::Instance().Load();
    MonsterBaseConfigurationTable::Instance().LoadSuccess();

    RewardConfigurationTable::Instance().Load();
    RewardConfigurationTable::Instance().LoadSuccess();

    SceneConfigurationTable::Instance().Load();
    SceneConfigurationTable::Instance().LoadSuccess();

    SkillConfigurationTable::Instance().Load();
    SkillConfigurationTable::Instance().LoadSuccess();

    SkillPermissionConfigurationTable::Instance().Load();
    SkillPermissionConfigurationTable::Instance().LoadSuccess();

    TestConfigurationTable::Instance().Load();
    TestConfigurationTable::Instance().LoadSuccess();

    TestMultiKeyConfigurationTable::Instance().Load();
    TestMultiKeyConfigurationTable::Instance().LoadSuccess();


    if (loadSuccessCallback){
        loadSuccessCallback();
    }
}

void LoadTablesAsync() {
    static muduo::CountDownLatch latch(18);

    std::thread ActorActionCombatStateLoadThread([](){
        ActorActionCombatStateConfigurationTable::Instance().Load();
            latch.countDown();});
            ActorActionCombatStateLoadThread.detach();

    std::thread ActorActionStateLoadThread([](){
        ActorActionStateConfigurationTable::Instance().Load();
            latch.countDown();});
            ActorActionStateLoadThread.detach();

    std::thread BuffLoadThread([](){
        BuffConfigurationTable::Instance().Load();
            latch.countDown();});
            BuffLoadThread.detach();

    std::thread ClassLoadThread([](){
        ClassConfigurationTable::Instance().Load();
            latch.countDown();});
            ClassLoadThread.detach();

    std::thread ConditionLoadThread([](){
        ConditionConfigurationTable::Instance().Load();
            latch.countDown();});
            ConditionLoadThread.detach();

    std::thread CooldownLoadThread([](){
        CooldownConfigurationTable::Instance().Load();
            latch.countDown();});
            CooldownLoadThread.detach();

    std::thread GlobalVariableLoadThread([](){
        GlobalVariableConfigurationTable::Instance().Load();
            latch.countDown();});
            GlobalVariableLoadThread.detach();

    std::thread ItemLoadThread([](){
        ItemConfigurationTable::Instance().Load();
            latch.countDown();});
            ItemLoadThread.detach();

    std::thread MainSceneLoadThread([](){
        MainSceneConfigurationTable::Instance().Load();
            latch.countDown();});
            MainSceneLoadThread.detach();

    std::thread MessageLimiterLoadThread([](){
        MessageLimiterConfigurationTable::Instance().Load();
            latch.countDown();});
            MessageLimiterLoadThread.detach();

    std::thread MissionLoadThread([](){
        MissionConfigurationTable::Instance().Load();
            latch.countDown();});
            MissionLoadThread.detach();

    std::thread MonsterBaseLoadThread([](){
        MonsterBaseConfigurationTable::Instance().Load();
            latch.countDown();});
            MonsterBaseLoadThread.detach();

    std::thread RewardLoadThread([](){
        RewardConfigurationTable::Instance().Load();
            latch.countDown();});
            RewardLoadThread.detach();

    std::thread SceneLoadThread([](){
        SceneConfigurationTable::Instance().Load();
            latch.countDown();});
            SceneLoadThread.detach();

    std::thread SkillLoadThread([](){
        SkillConfigurationTable::Instance().Load();
            latch.countDown();});
            SkillLoadThread.detach();

    std::thread SkillPermissionLoadThread([](){
        SkillPermissionConfigurationTable::Instance().Load();
            latch.countDown();});
            SkillPermissionLoadThread.detach();

    std::thread TestLoadThread([](){
        TestConfigurationTable::Instance().Load();
            latch.countDown();});
            TestLoadThread.detach();

    std::thread TestMultiKeyLoadThread([](){
        TestMultiKeyConfigurationTable::Instance().Load();
            latch.countDown();});
            TestMultiKeyLoadThread.detach();

    latch.wait();


    ActorActionCombatStateConfigurationTable::Instance().LoadSuccess();
    ActorActionStateConfigurationTable::Instance().LoadSuccess();
    BuffConfigurationTable::Instance().LoadSuccess();
    ClassConfigurationTable::Instance().LoadSuccess();
    ConditionConfigurationTable::Instance().LoadSuccess();
    CooldownConfigurationTable::Instance().LoadSuccess();
    GlobalVariableConfigurationTable::Instance().LoadSuccess();
    ItemConfigurationTable::Instance().LoadSuccess();
    MainSceneConfigurationTable::Instance().LoadSuccess();
    MessageLimiterConfigurationTable::Instance().LoadSuccess();
    MissionConfigurationTable::Instance().LoadSuccess();
    MonsterBaseConfigurationTable::Instance().LoadSuccess();
    RewardConfigurationTable::Instance().LoadSuccess();
    SceneConfigurationTable::Instance().LoadSuccess();
    SkillConfigurationTable::Instance().LoadSuccess();
    SkillPermissionConfigurationTable::Instance().LoadSuccess();
    TestConfigurationTable::Instance().LoadSuccess();
    TestMultiKeyConfigurationTable::Instance().LoadSuccess();
     if (loadSuccessCallback){
        loadSuccessCallback();
    }
}

void OnTablesLoadSuccess(const LoadSuccessCallback& callback){
loadSuccessCallback = callback;
}