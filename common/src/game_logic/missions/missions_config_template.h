#pragma once

#include "src/game_config/mission_config.h"


struct IMissionConfig 
{
    virtual uint32_t GetMissionType(uint32_t id) const  { return 0; }
    virtual uint32_t GetMissionSubType(uint32_t id) const { return 0; }
    virtual uint32_t reward_id(uint32_t id)const  { return 0; }
    virtual bool auto_reward(uint32_t mission_id) const  { return false; }
    virtual const ::google::protobuf::RepeatedField<uint32_t>& condition_id(uint32_t mission_id) const
    {
        static ::google::protobuf::RepeatedField<uint32_t> s;
            s.Clear();
            return s;
    }
    virtual const ::google::protobuf::RepeatedField<uint32_t>& next_mission_id(uint32_t mission_id) const
    {
        static ::google::protobuf::RepeatedField<uint32_t> s;
        s.Clear();
        return s;
    }
    virtual bool CheckTypeRepeated() const { return false; }
    virtual bool HasKey(uint32_t id) const { return false; }
};

struct MissionConfig : public IMissionConfig
{
    static MissionConfig& GetSingleton() { static MissionConfig singleton; return singleton; }

    virtual uint32_t GetMissionType(uint32_t id) const override
    {
        auto mrow = mission_config::GetSingleton().get(id);
        if (nullptr == mrow)
        {
            return 0;
        }
        return mrow->mission_type();
    }

    virtual uint32_t GetMissionSubType(uint32_t id) const override
    {
        auto mrow = mission_config::GetSingleton().get(id);
        if (nullptr == mrow)
        {
            return 0;
        }
        return mrow->mission_sub_type();
    }

    virtual uint32_t reward_id(uint32_t id)const override
    {
        auto mrow = mission_config::GetSingleton().get(id);
        if (nullptr == mrow)
        {
            return 0;
        }
        return mrow->reward_id();
    }

    virtual bool auto_reward(uint32_t mission_id)const override
    {
        auto p = mission_config::GetSingleton().get(mission_id);
        if (nullptr == p)
        {
            return false;
        }
        return nullptr != p && p->auto_reward() > 0;
    }

    virtual const ::google::protobuf::RepeatedField<uint32_t>& condition_id(uint32_t mission_id) const override
    {
        auto p = mission_config::GetSingleton().get(mission_id);
        if (nullptr == p)
        {
            static ::google::protobuf::RepeatedField<uint32_t> s;
            s.Clear();
            return s;
        }
        return p->condition_id();
    }

    virtual const ::google::protobuf::RepeatedField<uint32_t>& next_mission_id(uint32_t mission_id)const override
    {
        auto p = mission_config::GetSingleton().get(mission_id);
        if (nullptr == p)
        {
            static ::google::protobuf::RepeatedField<uint32_t> s;
            s.Clear();
            return s;
        }
        return p->next_mission_id();
    }

    virtual bool CheckTypeRepeated() const override { return true;   }
    virtual bool HasKey(uint32_t id) const override { return nullptr !=  mission_config::GetSingleton().get(id); }
};
