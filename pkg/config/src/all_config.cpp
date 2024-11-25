#include "all_config.h"

#include <thread>
#include "muduo/base/CountDownLatch.h"

#include "buff_config.h"
#include "skill_config.h"
#include "mission_config.h"
#include "condition_config.h"
#include "testmultikey_config.h"
#include "test_config.h"
#include "item_config.h"
#include "actoractionstate_config.h"
#include "globalvariable_config.h"
#include "mainscene_config.h"
#include "skillpermission_config.h"
#include "scene_config.h"
#include "class_config.h"
#include "monsterbase_config.h"
#include "cooldown_config.h"
void LoadAllConfig()
{
    BuffConfigurationTable::Instance().Load();
    SkillConfigurationTable::Instance().Load();
    MissionConfigurationTable::Instance().Load();
    ConditionConfigurationTable::Instance().Load();
    TestMultiKeyConfigurationTable::Instance().Load();
    TestConfigurationTable::Instance().Load();
    ItemConfigurationTable::Instance().Load();
    ActorActionStateConfigurationTable::Instance().Load();
    GlobalVariableConfigurationTable::Instance().Load();
    MainSceneConfigurationTable::Instance().Load();
    SkillPermissionConfigurationTable::Instance().Load();
    SceneConfigurationTable::Instance().Load();
    ClassConfigurationTable::Instance().Load();
    MonsterBaseConfigurationTable::Instance().Load();
    CooldownConfigurationTable::Instance().Load();
}

void LoadAllConfigAsyncWhenServerLaunch()
{
    static muduo::CountDownLatch latch_(15);

    /// Begin
    {
        std::thread t([&]() {

    BuffConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    SkillConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    MissionConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    ConditionConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    TestMultiKeyConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    TestConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    ItemConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    ActorActionStateConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    GlobalVariableConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    MainSceneConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    SkillPermissionConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    SceneConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    ClassConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    MonsterBaseConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    CooldownConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End
    latch_.wait();
}
