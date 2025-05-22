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


void LoadAllConfig()
{

    ActorActionCombatStateConfigurationTable::Instance().Load();

    ActorActionStateConfigurationTable::Instance().Load();

    BuffConfigurationTable::Instance().Load();

    ClassConfigurationTable::Instance().Load();

    ConditionConfigurationTable::Instance().Load();

    CooldownConfigurationTable::Instance().Load();

    GlobalVariableConfigurationTable::Instance().Load();

    ItemConfigurationTable::Instance().Load();

    MainSceneConfigurationTable::Instance().Load();

    MessageLimiterConfigurationTable::Instance().Load();

    MissionConfigurationTable::Instance().Load();

    MonsterBaseConfigurationTable::Instance().Load();

    RewardConfigurationTable::Instance().Load();

    SceneConfigurationTable::Instance().Load();

    SkillConfigurationTable::Instance().Load();

    SkillPermissionConfigurationTable::Instance().Load();

    TestConfigurationTable::Instance().Load();

    TestMultiKeyConfigurationTable::Instance().Load();

}

void LoadAllConfigAsyncWhenServerLaunch()
{
    static muduo::CountDownLatch latch_(18);

    
    {
        std::thread t([&]() {
            ActorActionCombatStateConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            ActorActionStateConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            BuffConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            ClassConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            ConditionConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            CooldownConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            GlobalVariableConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            ItemConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            MainSceneConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            MessageLimiterConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            MissionConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            MonsterBaseConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            RewardConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            SceneConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            SkillConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            SkillPermissionConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            TestConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    
    {
        std::thread t([&]() {
            TestMultiKeyConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    

    latch_.wait();
}