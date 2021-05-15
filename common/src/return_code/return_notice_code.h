#ifndef COMMON_SRC_RETURN_CODE_Return_Notice_Code
#define COMMON_SRC_RETURN_CODE_Return_Notice_Code

#include <cstdint>
#include <string>
#include <vector>

namespace common
{
    enum  EnumCode : uint32_t
    {
        RET_OK = 1,

        //TeamRet
        RET_TEAM_NOT_IN_APPLICANTS = 1000,
        RET_TEAM_PLAEYR_ID = 1001,
        RET_TEAM_MEMBERS_FULL = 1002,
        RET_TEAM_MEMBER_IN_TEAM = 1003,
        RET_TEAM_MEMBER_NOT_IN_TEAM = 1004,
        RET_TEAM_KICK_SELF = 1005,
        RET_TEAM_KICK_NOT_LEADER = 1006,
        RET_TEAM_APPOINT_SELF = 1007,
        RET_TEAM_APPOINT_LEADER_NOT_LEADER = 1008,
        RET_TEAM_TEAM_FULL = 1009,
        RET_TEAM_IN_APPLICANT_LIEST = 1010,
        RET_TEAM_NOT_IN_APPLICANT_LIEST = 1011,
        RET_TEAM_TEAM_LIST_MAX = 1012,
        RET_TEAM_HAS_NOT_TEAM_ID = 1013,
        RET_TEAM_DISMISS_NOT_LEADER = 1014,
    };
}//namespace common

#define RET_CHECK_RET(f)\
{\
ReturnValue ret = (f);\
if (ret != ReturnValue(RET_OK))\
{\
    return  ret;\
}\
}

#endif // !COMMON_SRC_RETURN_CODE_Return_Notice_Code
