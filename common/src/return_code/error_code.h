#ifndef COMMON_SRC_RETURN_CODE_ERROR_CODE
#define COMMON_SRC_RETURN_CODE_ERROR_CODE

#include <cstdint>
#include <string>
#include <vector>

namespace common
{
    enum  EnumCode : uint32_t
    {
        RET_OK = 0,
        RET_TABLE_ID_ERROR = 2,
        RET_TABLE_DTATA_ERROR = 3,

        //login server 
        RET_LOGIN_CNAT_FIND_ACCOUNT = 1000,//找不到
        RET_LOGIN_MAX_PLAYER_SIZE = 1001,
        RET_LOGIN_CREATE_PLAYER_DONOT_LOAD_ACCOUNT = 1002,
        RET_LOGIN_CREATE_PLAYER_CONNECTION_HAS_NOT_ACCOUNT = 1003,
        RET_LOGIN_DONOT_LOGIN = 1004,
        RET_LOGIN_LOGIN_ING = 1005,
        RET_LOGIN_LOGIN_NO_PLAYER = 1006,
        RET_LOGIN_BEING_ENTER_GAME = 1007,
        RET_LOGIN_PLAYEING = 1008,
        RET_LOGIN_BEING_CREATE_PLAYER = 1009,
        RET_LOGIN_WAITING_ENTER_GAME = 1010,//已经登录了，等待进入游戏
        RET_LOGIN_ENTER_GUID = 1011,
        RET_LOGIN_ACCOUNT_NAME_EMPTY = 1012,
        REG_LOGIN_CREATEPLAYER_CONNECTION_ACCOUNT_EMPTY = 1013,
        REG_LOGIN_ENTERGAMEE_CONNECTION_ACCOUNT_EMPTY = 1014,

        //Team
        RET_TEAM_NOT_IN_APPLICANTS = 5000,
        RET_TEAM_PLAEYR_ID = 5001,
        RET_TEAM_MEMBERS_FULL = 5002,
        RET_TEAM_MEMBER_IN_TEAM = 5003,
        RET_TEAM_MEMBER_NOT_IN_TEAM = 5004,
        RET_TEAM_KICK_SELF = 5005,
        RET_TEAM_KICK_NOT_LEADER = 5006,
        RET_TEAM_APPOINT_SELF = 5007,
        RET_TEAM_APPOINT_LEADER_NOT_LEADER = 5008,
        RET_TEAM_TEAM_FULL = 5009,
        RET_TEAM_IN_APPLICANT_LIEST = 5010,
        RET_TEAM_NOT_IN_APPLICANT_LIEST = 5011,
        RET_TEAM_TEAM_LIST_MAX = 5012,
        RET_TEAM_HAS_NOT_TEAM_ID = 5013,
        RET_TEAM_DISMISS_NOT_LEADER = 5014,

        //mission
        RET_MISSION_ID_REPTEATED = 6000,
        RET_MISSION_TYPE_REPTEATED = 6001,
        RET_MISSION_NO_CONDITION = 6002,
        RET_MISSION_COMPLETE = 6003,
        RET_MISSION_GET_REWARD_NO_MISSION_ID = 6004,

        //bag 
        RET_BAG_DELETE_ITEM_HASNOT_GUID = 7000,
        RET_BAG_DELETE_ITEM_HAS_GUID = 7001,
        RET_BAG_ADD_ITEM_HAS_NOT_BASE_COMPONENT = 7002,
        RET_BAG_ADD_ITEM_INVALID_GUID = 7003,
        RET_BAG_ADD_ITEM_INVALID_PARAM = 7004,
        RET_BAG_ADD_ITEM_BAG_FULL = 7005,
    };
}//namespace common

#define RET_CHECK_RET(f)\
{\
uint32_t ret(f);\
if (ret != RET_OK)\
{\
    return  ret;\
}\
}

#define  CheckReturnCloseureError(return_code)\
if (return_code != RET_OK)\
{\
    response->mutable_error()->set_error_no(return_code);\
    done->Run();\
    return;\
}\

#define CheckCondtion(condition, return_code)\
if (condition)\
{\
     return  return_code; \
}\

#define  ReturnCloseureError(f)\
response->mutable_error()->set_error_no(f);\
done->Run();\
return;

#define ReturnCloseureOK ReturnCloseureError(RET_OK);

#endif // !COMMON_SRC_RETURN_CODE_ERROR_CODE
