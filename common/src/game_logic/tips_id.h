#ifndef COMMON_SRC_tip_code_ERROR_CODE
#define COMMON_SRC_tip_code_ERROR_CODE

#include <cstdint>
#include <string>
#include <vector>


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
    kRetLoginUnkonwError = 1015,

    //Scene
    kRetEnterSceneNotFound = 4000,
    kRetEnterSceneNotFull = 4001,
    kRetEnterScenetWeightRoundRobinMainScene = 4002,//所有场景已经满了
    kRetEnterSceneCreatePlayer = 4003,//玩家进入不了跨服
    kRetEnterGameGsCrash = 4004,//当前服务器不可进入，请重新进入
    kRetEnterSceneServerType = 4005,//当前服务器不可切换场景
    kRetEnterSceneParamError = 4006,//换场景参数错误
    kRetEnterSceneSceneFull = 4007,//场景已经满了不能切换
    kRetEnterSceneSceneNotFound = 4008,//您所要切换的场景不存在
    kRetEnterSceneYouInCurrentScene = 4009,//您当前就在这个场景，无需切换
    kRetEnterSceneEnterCrossRoomScene = 4010,//不能进入跨服副本
    kRetEnterSceneGsInfoNull = 4011,//场景的数据不完整
    kRetEnterSceneGsFull = 4012,//gs 已经满了不能切换
    kRetEnterSceneYourSceneIsNull = 4013,//你的场景为空
    kRetEnterSceneChangeSceneOffLine = 4014,//你已经下线
    kRetEnterSceneChangingScene = 4015,//正在切换场景中
    kRetEnterSceneChangingGs = 4016,//正在切换gs中

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
    kRetMissionIdRepeated = 6000,
    kRetMisionTypeRepeated = 6001,
    kRetMissionComplete = 6002,
    kRetMissionGetRewardNoMissionId = 6003,
    kRetMissionPlayerMissionCompNotFound = 6004,

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


#define RET_CHECK_RET(f)\
{\
uint32_t ret(f);\
if (ret != kRetOK)\
{\
    return  ret;\
}\
}

#define  CheckReturnCloseureError(tip_code)\
if (tip_code != kRetOK)\
{\
    response->mutable_error()->set_id(tip_code);\
    done->Run();\
    return;\
}\

#define CheckCondtion(condition, tip_code)\
if (condition)\
{\
     return  tip_code; \
}\

#define  ReturnCloseureError(f)\
response->mutable_error()->set_id(f);\
done->Run();\
return;

#define ReturnCloseureOK ReturnCloseureError(kRetOK);

#define  ReturnAutoCloseureError(tip_code)\
if (tip_code != kRetOK)\
{\
    response->mutable_error()->set_id(tip_code);\
    return;\
}\

#endif // !COMMON_SRC_tip_code_ERROR_CODE
