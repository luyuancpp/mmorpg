#pragma once

#include "table/code/mission_table.h"

// IMissionConfig: interface for table-backed mission configuration lookups.
// All methods are pure virtual — concrete implementations must be provided.
struct IMissionConfig 
{
    virtual ~IMissionConfig() = default;

    virtual uint32_t GetMissionType(uint32_t id) const = 0;
    virtual uint32_t GetMissionSubType(uint32_t id) const = 0;
    virtual uint32_t GetRewardId(uint32_t id) const = 0;
    virtual bool AutoReward(uint32_t missionTableId) const = 0;
    virtual const ::google::protobuf::RepeatedField<uint32_t>& GetConditionIds(uint32_t missionTableId) const = 0;
    virtual const ::google::protobuf::RepeatedField<uint32_t>& GetTargetCounts(uint32_t missionTableId) const = 0;
    virtual const ::google::protobuf::RepeatedField<uint32_t>& GetNextMissionTableIds(uint32_t missionTableId) const = 0;
    virtual bool CheckTypeRepeated() const = 0;
    virtual bool HasKey(uint32_t id) const = 0;
};

struct MissionConfig : public IMissionConfig
{
    static MissionConfig& GetSingleton() { static MissionConfig singleton; return singleton; }

    uint32_t GetMissionType(uint32_t missionTableId) const override
    {
        LookupMissionOrReturn(missionTableId, 0);
        return missionRow->mission_type();
    }

    uint32_t GetMissionSubType(uint32_t missionTableId) const override
    {
        LookupMissionOrReturn(missionTableId, 0);
        return missionRow->mission_sub_type();
    }

    uint32_t GetRewardId(uint32_t missionTableId) const override
    {
        LookupMissionOrReturn(missionTableId, 0);
        return missionRow->reward_id();
    }

    bool AutoReward(uint32_t missionTableId) const override
    {
        LookupMissionOrFalse(missionTableId);
        return missionRow->auto_reward() > 0;
    }

    const ::google::protobuf::RepeatedField<uint32_t>& GetConditionIds(uint32_t missionTableId) const override
    {
        LookupMissionOrReturn(missionTableId, EmptyRepeatedField());
        return missionRow->condition_id();
    }

    const ::google::protobuf::RepeatedField<uint32_t>& GetTargetCounts(uint32_t missionTableId) const override
    {
        LookupMissionOrReturn(missionTableId, EmptyRepeatedField());
        return missionRow->target_count();
    }

    const ::google::protobuf::RepeatedField<uint32_t>& GetNextMissionTableIds(uint32_t missionTableId) const override
    {
        LookupMissionOrReturn(missionTableId, EmptyRepeatedField());
        return missionRow->next_mission_id();
    }

    bool CheckTypeRepeated() const override { return true; }
    bool HasKey(uint32_t missionTableId) const override { LookupMissionOrFalse(missionTableId); return true; }

private:
    static const ::google::protobuf::RepeatedField<uint32_t>& EmptyRepeatedField()
    {
        static const ::google::protobuf::RepeatedField<uint32_t> kEmpty;
        return kEmpty;
    }
};
