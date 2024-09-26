#include "all_config.h"

#include <thread>
#include "muduo/base/CountDownLatch.h"

#include "skill_config.h"
#include "mission_config.h"
#include "condition_config.h"
#include "testmultikey_config.h"
#include "buff_config.h"
#include "test_config.h"
#include "item_config.h"
#include "globalvariable_config.h"
#include "mainscene_config.h"
#include "scene_config.h"
#include "class_config.h"
#include "monsterbase_config.h"
#include "cooldown_config.h"
void LoadAllConfig()
{
    SkillConfigurationTable::GetSingleton().Load();
    MissionConfigurationTable::GetSingleton().Load();
    ConditionConfigurationTable::GetSingleton().Load();
    TestMultiKeyConfigurationTable::GetSingleton().Load();
    BuffConfigurationTable::GetSingleton().Load();
    TestConfigurationTable::GetSingleton().Load();
    ItemConfigurationTable::GetSingleton().Load();
    GlobalVariableConfigurationTable::GetSingleton().Load();
    MainSceneConfigurationTable::GetSingleton().Load();
    SceneConfigurationTable::GetSingleton().Load();
    ClassConfigurationTable::GetSingleton().Load();
    MonsterBaseConfigurationTable::GetSingleton().Load();
    CooldownConfigurationTable::GetSingleton().Load();
}

void LoadAllConfigAsyncWhenServerLaunch()
{
    static muduo::CountDownLatch latch_(13);

    /// Begin
    {
        std::thread t([&]() {

    SkillConfigurationTable::GetSingleton().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    MissionConfigurationTable::GetSingleton().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    ConditionConfigurationTable::GetSingleton().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    TestMultiKeyConfigurationTable::GetSingleton().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    BuffConfigurationTable::GetSingleton().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    TestConfigurationTable::GetSingleton().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    ItemConfigurationTable::GetSingleton().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    GlobalVariableConfigurationTable::GetSingleton().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    MainSceneConfigurationTable::GetSingleton().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    SceneConfigurationTable::GetSingleton().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    ClassConfigurationTable::GetSingleton().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    MonsterBaseConfigurationTable::GetSingleton().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End

    /// Begin
    {
        std::thread t([&]() {

    CooldownConfigurationTable::GetSingleton().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End
    latch_.wait();
}
