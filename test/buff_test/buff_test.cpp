#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "game_logic/combat/buff/util/buff_util.h"
#include "buff_config.h"
#include "buff_error_tip.pb.h"
#include "thread_local/storage.h"
#include "proto/logic/event/buff_event.pb.h"
#include "game_logic/combat/buff/comp/buff_comp.h"
#include "time/comp/timer_task_comp.h"
#include "pbc/buff_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    // Assuming BuffConfigurationTable::GetSingleton().Load() is necessary
    BuffConfigurationTable::GetSingleton().Load();

    int ret = RUN_ALL_TESTS();

    // Clean up thread-local storage after all tests
    tls.registry.clear();
    return ret;
}
