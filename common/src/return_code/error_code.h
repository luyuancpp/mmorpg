#ifndef COMMON_SRC_RETURN_CODE_ERROR_CODE
#define COMMON_SRC_RETURN_CODE_ERROR_CODE

#include <cstdint>
#include <string>
#include <vector>

namespace common
{
    enum  EnumCode : uint32_t
    {
        kRetOK = 0,
        kRetTableId = 2,
        kRetCofnigData = 3,

        //login server 
        kRetLoginCantFindAccount = 1000,//找不到
        kRetLoginAccountPlayerFull = 1001,
        kRetLoginCreatePlayerDonotLoadAccount = 1002,
        kRetLoignCreatePlayerConnectionHasNotAccount = 1003,
        kRetLoginHadnotLogin = 1004,
        kRetLoginIng = 1005,
        kRetLoginPlayerGuidError = 1006,
        kRetLoginEnteringGame = 1007,
        kRetLoginPlaying = 1008,
        kRetLoignCreatingPlayer = 1009,
        kRetLoignWatingEnterGame = 1010,//已经登录了，等待进入游戏
        kRetLoignEnterGuid = 1011,
        kRetLoignAccountNameEmpty = 1012,
        kRetLoginCreateConnectionAccountEmpty = 1013,
        kRetLoginEnterGameConnectionAccountEmpty = 1014,

        //Team
        kRetTeamNotInApplicants = 5000,
        kRetTeamPlayerId = 5001,
        kRetTeamMembersFull = 5002,
        kRetTeamMemberInTeam = 5003,
        kRetTeamMemberNotInTeam = 5004,
        kRetTeamKickSelf = 5005,
        kRetTeamKickNotLeader = 5006,
        kRetTeamAppointSelf = 5007,
        KRetTeamAppointLeaderNotLeader = 5008,
        kRetTeamFull = 5009,
        kRetTeamInApplicantList = 5010,
        kRetTeamNotInApplicantList = 5011,
        kRetTeamListMaxSize = 5012,
        kRetTeamHasNotTeamId = 5013,
        kRetTeamDismissNotLeader = 5014,

        //mission
        RET_MISSION_ID_REPTEATED = 6000,
        RET_MISSION_TYPE_REPTEATED = 6001,
        RET_MISSION_NO_CONDITION = 6002,
        RET_MISSION_COMPLETE = 6003,
        RET_MISSION_GET_REWARD_NO_MISSION_ID = 6004,

        //bag 
        kRetBagDeleteItemHasnotGuid = 7000,
        kRetBagDeleteItemAlreadyHasGuid = 7001,
        kRetBagAddItemHasNotBaseComponent = 7002,
        kRetBagAddItemInvalidGuid = 7003,
        kRetBagAddItemInvalidParam = 7004,
        kRetBagAddItemBagFull = 7005,
        kRetBagAdequateAddItemSize = 7006,//不可叠加
        kRetBagAdequatetem = 7007,//不够物品
        kRetBagDelItemPos = 7008,
        kRetBagDelItemConfig = 7009,
        kRetBagDelItemGuid = 7010,
        kRetBagDelItemFindItem = 7011,
        kRetBagDelItemSize = 7012,
        kRetBagDelItemNotAdequateSize = 7013,
    };
}//namespace common

#define RET_CHECK_RET(f)\
{\
uint32_t ret(f);\
if (ret != kRetOK)\
{\
    return  ret;\
}\
}

#define  CheckReturnCloseureError(return_code)\
if (return_code != kRetOK)\
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

#define ReturnCloseureOK ReturnCloseureError(kRetOK);

#endif // !COMMON_SRC_RETURN_CODE_ERROR_CODE
