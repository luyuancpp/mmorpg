#ifndef COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_CONFIG_TEMPLATE_H_
#define COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_CONFIG_TEMPLATE_H_

#include "src/game_config/mission_config.h"

namespace common
{
    template<typename Config, typename ConfigRow>
    struct MissionConfig
    {
        static MissionConfig<Config, ConfigRow> GetSingleton() {
            static MissionConfig<Config, ConfigRow> singleton;
            return singleton;
        }

        decltype(auto) key_id(uint32_t id) { return Config::GetSingleton().key_id(id); }

        uint32_t mission_type(uint32_t id)
        {
            auto mrow = Config::GetSingleton().key_id(id);
            if (nullptr == mrow)
            {
                return 0;
            }
            return mrow->mission_type();
        }

        uint32_t mission_sub_type(uint32_t id)
        {
            auto mrow = Config::GetSingleton().key_id(id);
            if (nullptr == mrow)
            {
                return 0;
            }
            return mrow->mission_sub_type();
        }

        uint32_t reward_id(uint32_t id)
        {
            auto mrow = Config::GetSingleton().key_id(id);
            if (nullptr == mrow)
            {
                return 0;
            }
            return mrow->reward_id();
        }

        bool auto_reward(uint32_t mission_id)
        {
            auto p = mission_config::GetSingleton().key_id(mission_id);
            if (nullptr == p)
            {
                return false;
            }
            return nullptr != p && p->auto_reward() > 0;
        }

        bool HasMainSubTypeCheck() { return std::is_same_v<mission_row, ConfigRow>;   }

    };
}//namespace common

#endif // !COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_CONFIG_TEMPLATE_H_