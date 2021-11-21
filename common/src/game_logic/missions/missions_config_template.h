#ifndef COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_CONFIG_TEMPLATE_H_
#define COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_CONFIG_TEMPLATE_H_

#include "src/game_config/mission_config.h"

namespace common
{
    struct IMissionConfig 
    {
        virtual uint32_t mission_type(uint32_t id){ return 0; }
        virtual uint32_t mission_sub_type(uint32_t id) { return 0; }
        virtual uint32_t reward_id(uint32_t id) { return 0; }
        virtual bool auto_reward(uint32_t mission_id) { return 0; }
        virtual const ::google::protobuf::RepeatedField<uint32_t>& condition_id(uint32_t mission_id)
        {
                static ::google::protobuf::RepeatedField<uint32_t> s;
                s.Clear();
                return s;
        }
        virtual const ::google::protobuf::RepeatedField<uint32_t>& next_mission_id(uint32_t mission_id)
        {
            static ::google::protobuf::RepeatedField<uint32_t> s;
            s.Clear();
            return s;
        }
        virtual bool HasMainSubTypeCheck() { return false; }
        virtual bool HasKey(uint32_t id) { return false; }
    };

    struct MissionConfig : public IMissionConfig
    {
        static MissionConfig& GetSingleton() { static MissionConfig singleton; return singleton; }

        virtual uint32_t mission_type(uint32_t id)override
        {
            auto mrow = mission_config::GetSingleton().key_id(id);
            if (nullptr == mrow)
            {
                return 0;
            }
            return mrow->mission_type();
        }

        virtual uint32_t mission_sub_type(uint32_t id)override
        {
            auto mrow = mission_config::GetSingleton().key_id(id);
            if (nullptr == mrow)
            {
                return 0;
            }
            return mrow->mission_sub_type();
        }

        virtual uint32_t reward_id(uint32_t id)override
        {
            auto mrow = mission_config::GetSingleton().key_id(id);
            if (nullptr == mrow)
            {
                return 0;
            }
            return mrow->reward_id();
        }

        virtual bool auto_reward(uint32_t mission_id)override
        {
            auto p = mission_config::GetSingleton().key_id(mission_id);
            if (nullptr == p)
            {
                return false;
            }
            return nullptr != p && p->auto_reward() > 0;
        }

        virtual const ::google::protobuf::RepeatedField<uint32_t>& condition_id(uint32_t mission_id) override
        {
            auto p = mission_config::GetSingleton().key_id(mission_id);
            if (nullptr == p)
            {
                static ::google::protobuf::RepeatedField<uint32_t> s;
                s.Clear();
                return s;
            }
            return p->condition_id();
        }

        virtual const ::google::protobuf::RepeatedField<uint32_t>& next_mission_id(uint32_t mission_id)override
        {
            auto p = mission_config::GetSingleton().key_id(mission_id);
            if (nullptr == p)
            {
                static ::google::protobuf::RepeatedField<uint32_t> s;
                s.Clear();
                return s;
            }
            return p->next_mission_id();
        }

        virtual bool HasMainSubTypeCheck() override { return true;   }
        virtual bool HasKey(uint32_t id)override { return nullptr !=  mission_config::GetSingleton().key_id(id); }
    };
}//namespace common

#endif // !COMON_SRC_GAME_LOGIC_MISSIONS_MISSIONS_CONFIG_TEMPLATE_H_