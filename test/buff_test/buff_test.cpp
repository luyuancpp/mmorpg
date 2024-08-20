#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "game_logic/combat/ability/util/ability_util.h"
#include "thread_local/storage.h"
#include "ability_config.h"
#include "buff_config.h"
#include "game_logic/combat/ability/comp/ability_comp.h"
#include "game_logic/combat/ability/constants/ability_constants.h"
#include "time/comp/timer_task_comp.h"
#include "time/util/cooldown_time_util.h"
#include "pbc/ability_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"

using ::testing::_;
using ::testing::Return;



// Main function
int main(int argc, char** argv) {
    EventLoop loop;

    ::testing::InitGoogleTest(&argc, argv);
    CooldownConfigurationTable::GetSingleton().Load();
    AbilityConfigurationTable::GetSingleton().Load();
    BuffConfigurationTable::GetSingleton().Load();

    int ret = RUN_ALL_TESTS();
    tls.registry.clear(); // Clean up thread-local storage after all tests
    return ret;
}
