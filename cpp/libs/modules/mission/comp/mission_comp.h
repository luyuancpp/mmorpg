#pragma once

#include <array>
#include <bitset>

#include "proto/logic/component/mission_comp.pb.h"
#include "core/type_define/type_define.h"
#include "modules/mission/comp/missions_config_comp.h"
#include "core/utils/registry/game_registry.h"
#include "table/code/bit_index/mission_table_id_bit_index.h"

class AcceptMissionEvent;
class MissionConditionEvent;

using MissionsBits = std::bitset<kMissionMaxBitIndex>;

class MissionsComponent : public EventOwner
{
public:
    using EventMissionClassifyMap = std::unordered_map<uint32_t, UInt32Set>;

    MissionsComponent();

    [[nodiscard]] const auto& GetEventMissionsClassifyForUnitTest() const { return eventMissionsClassify; }
    [[nodiscard]] const MissionListPBComponent& getMissionsComp() const { return missionsComp; }
    [[nodiscard]] std::size_t MissionSize() const { return missionsComp.missions().size(); }
    [[nodiscard]] std::size_t CompleteSize() const { return completedMissions.count(); }
    [[nodiscard]] std::size_t TypeSetSize() const { return typeFilter.size(); }
    [[nodiscard]] std::size_t CanGetRewardSize() const;
    MissionsBits& GetCompleteMissions() { return completedMissions; }

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

    [[nodiscard]] bool IsAccepted(const uint32_t missionId) const
    {
        return missionsComp.missions().count(missionId) > 0;
    }

    [[nodiscard]] bool IsComplete(const uint32_t missionId) const
    {
        if (!MissionBitMap.contains(missionId))
        {
            return false;
        }
        return completedMissions.test(MissionBitMap.at(missionId));
    }

    void AbandonMission(const uint32_t missionId);

    [[nodiscard]] uint32_t IsMissionUnaccepted(uint32_t missionId) const;
    [[nodiscard]] uint32_t IsMissionUncompleted(uint32_t missionId) const;


private:
    MissionListPBComponent missionsComp;
    EventMissionClassifyMap eventMissionsClassify; // key: classify mission by event type, value: mission list
    UInt32PairSet typeFilter;
    bool missionTypeNotRepeated{ true }; //任务类型不能重复
    MissionsBits completedMissions;
};

using PlayerMissionList = std::array<MissionsComponent, MissionListPBComponent::kPlayerMissionSize>;
