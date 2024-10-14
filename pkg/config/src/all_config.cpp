#include "all_config.h"

#include <thread>
#include "muduo/base/CountDownLatch.h"

#include "skill_config.h"
void LoadAllConfig()
{
    SkillConfigurationTable::Instance().Load();
}

void LoadAllConfigAsyncWhenServerLaunch()
{
    static muduo::CountDownLatch latch_(1);

    /// Begin
    {
        std::thread t([&]() {

    SkillConfigurationTable::Instance().Load();
            latch_.countDown();
        });
        t.detach();
    }
    /// End
    latch_.wait();
}
