#pragma once

#include "mission_config.h"


struct IMissionConfig 
{
    virtual uint32_t GetMissionType(uint32_t id) const  { return 0; }
    virtual uint32_t GetMissionSubType(uint32_t id) const { return 0; }
    virtual uint32_t GetRewardId(uint32_t id)const  { return 0; }
    virtual bool AutoReward(uint32_t mission_id) const  { return false; }
    virtual const ::google::protobuf::RepeatedField<uint32_t>& GetConditionIds(uint32_t mission_id) const
    {
        static ::google::protobuf::RepeatedField<uint32_t> s;
            s.Clear();
            return s;
    }
    virtual const ::google::protobuf::RepeatedField<uint32_t>& getNextMissionIds(uint32_t mission_id) const
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
        auto [missionTable, result] = GetMissionTable(id);
        if (nullptr == missionTable)
        {
            return 0;
        }
        return missionTable->mission_type();
    }

    virtual uint32_t GetMissionSubType(uint32_t id) const override
    {
        auto [missionTable, result] = GetMissionTable(id);
        if (nullptr == missionTable)
        {
            return 0;
        }
        return missionTable->mission_sub_type();
    }

    virtual uint32_t GetRewardId(uint32_t id)const override
    {
        auto [missionTable, result] = GetMissionTable(id);
        if (nullptr == missionTable)
        {
            return 0;
        }
        return missionTable->reward_id();
    }

    virtual bool AutoReward(uint32_t mission_id)const override
    {
        auto [missionTable, result] = GetMissionTable(mission_id);
        if (nullptr == missionTable)
        {
            return false;
        }
        return  missionTable->auto_reward() > 0;
    }

    virtual const ::google::protobuf::RepeatedField<uint32_t>& GetConditionIds(uint32_t mission_id) const override
    {
        auto [missionTable, result] = GetMissionTable(mission_id);
        if (nullptr == missionTable)
        {
            static ::google::protobuf::RepeatedField<uint32_t> s;
            s.Clear();
            return s;
        }
        return missionTable->condition_id();
    }

    virtual const ::google::protobuf::RepeatedField<uint32_t>& getNextMissionIds(uint32_t mission_id)const override
    {
        auto [missionTable, result] = GetMissionTable(mission_id);
        if (nullptr == missionTable)
        {
            static ::google::protobuf::RepeatedField<uint32_t> s;
            s.Clear();
            return s;
        }
        return missionTable->next_mission_id();
    }

    virtual bool CheckTypeRepeated() const override { return true;   }
    virtual bool HasKey(uint32_t id) const override { return nullptr !=  GetMissionTable(id).first; }
};
