#ifndef COMON_SRC_GAME_LOGIC_MISSIONS_PAYER_MISSIONS_H_
#define COMON_SRC_GAME_LOGIC_MISSIONS_PAYER_MISSIONS_H_

#include "src/common_type/common_type.h"
#include "missions_base.h"

#include "protobuf/src/google/protobuf/repeated_field.h"

namespace common
{
    struct AcceptMissionP : public AcceptMissionBaseP
    {
        using PBUint32V = ::google::protobuf::RepeatedField<::google::protobuf::uint32 >;
        AcceptMissionP(uint32_t mid);
    };

    struct AcceptPlayerRandomMissionP
    {
        AcceptPlayerRandomMissionP(
            uint32_t mision_id)
            :
            mission_id_(mision_id)
        {}
        uint32_t mission_id_{ 0 };
    };

    uint32_t RandomMision(const AcceptPlayerRandomMissionP& param, MissionsComp& ms);
};//namespace common

#endif // !COMON_SRC_GAME_LOGIC_MISSIONS_PAYER_MISSIONS_H_
