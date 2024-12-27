#pragma once

#include <array>
#include <bitset>

#include "proto/logic/component/mission_comp.pb.h"
#include "type_define/type_define.h"
#include "mission/comp/missions_config_comp.h"
#include "util/game_registry.h"
#include "cpp_table_id_bit_index/mission_table_id_bit_index.h"

class AcceptMissionEvent;
class MissionConditionEvent;

using CompleteMissions = std::bitset<kMaxBitIndex>;

class MissionsComponent : public EventOwner
{
public:
    using EventMissionClassifyMap = std::unordered_map<uint32_t, UInt32Set>;

    MissionsComponent();

    [[nodiscard]] const auto& GetEventMissionsClassifyForUnitTest() const { return eventMissionsClassify; }
    [[nodiscard]] const MissionListPBComponent& getMissionsComp() const { return missionsComp; }
    [[nodiscard]] std::size_t MissionSize() const { return missionsComp.missions().size(); }
    [[nodiscard]] std::size_t CompleteSize() const { return static_cast<std::size_t>(missionsComp.complete_missions_size()); }
    [[nodiscard]] std::size_t TypeSetSize() const { return typeFilter.size(); }
    [[nodiscard]] std::size_t CanGetRewardSize() const;

    [[nodiscard]] const IMissionConfig* GetMissionConfig() const
    {
        return missionConfig;
    }

    void SetMissionConfig(const IMissionConfig* const mission_config)
    {
        if (mission_config != nullptr) {
            missionConfig = mission_config;
        }
    }

    [[nodiscard]] MissionListPBComponent& GetMissionsComp()
    {
        return missionsComp;
    }

    [[nodiscard]] EventMissionClassifyMap& GetEventMissionsClassify()
    {
        return eventMissionsClassify;
    }

    [[nodiscard]] UInt32PairSet& GetTypeFilter()
    {
        return typeFilter;
    }

    [[nodiscard]] bool IsMissionTypeNotRepeated() const
    {
        return missionTypeNotRepeated;
    }

    void SetMissionTypeNotRepeated(const bool mission_type_not_repeated)
    {
        missionTypeNotRepeated = mission_type_not_repeated;
    }

    [[nodiscard]] bool IsAccepted(const uint32_t mission_id) const
    {
        return missionsComp.missions().count(mission_id) > 0;
    }

    [[nodiscard]] bool IsComplete(const uint32_t mission_id) const
    {
        return missionsComp.complete_missions().count(mission_id) > 0;
    }

    [[nodiscard]] uint32_t IsMissionUnaccepted(uint32_t mission_id) const;
    [[nodiscard]] uint32_t IsMissionUncompleted(uint32_t mission_id) const;

private:
    const IMissionConfig* missionConfig{ nullptr };
    MissionListPBComponent missionsComp;
    EventMissionClassifyMap eventMissionsClassify; // key: classify mission by event type, value: mission list
    UInt32PairSet typeFilter;
    bool missionTypeNotRepeated{ true }; //任务类型不能重复
    CompleteMissions completedMissions;
};

using PlayerMissionList = std::array<MissionsComponent, MissionListPBComponent::kPlayerMissionSize>;
