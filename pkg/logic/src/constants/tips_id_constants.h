#ifndef COMMON_SRC_tip_code_ERROR_CODE
#define COMMON_SRC_tip_code_ERROR_CODE

#include <cstdint>


enum  EnumCode : uint32_t
{
    kOK = 0,
    kTableIdInvalid = 2,
    kTableDataInvalid = 3,
    kEnttNull = 4,
    //改功能暂时关闭gate 用,gate写死了
    kServiceNotOpen = 5,
    //服务器关闭请重新登录 gate 用,gate写死了
    kServerCrush = 6,
    //未知参数
    kInvalidParameter = 7,

    //login server
    //找不到账号
    kLoginCantFindAccount = 1000,
    kLoginAccountPlayerFull = 1001,
    kLoginCreatePlayerUnLoadAccount = 1002,
    kLoginCreatePlayerConnectionHasNotAccount = 1003,
    kLoginUnLogin = 1004,
    kLoginIng = 1005,
    kLoginPlayerGuidError = 1006,
    kLoginEnteringGame = 1007,
    kLoginPlaying = 1008,
    kLoginCreatingPlayer = 1009,
    //已经登录了，等待进入游戏
    kLoginWaitingEnterGame = 1010,
    kLoginEnterGuid = 1011,
    kLoginAccountNameEmpty = 1012,
    kLoginCreateConnectionAccountEmpty = 1013,
    kLoginEnterGameConnectionAccountEmpty = 1014,
    kLoginUnknownError = 1015,
    kLoginSessionDisconnect = 1016,
    //在别的地方登录
    kLoginBeKickByAnOtherAccount = 1017,

    //Scene
    kEnterSceneNotFound = 4000,
    kEnterSceneNotFull = 4001,
    //所有场景已经满了
    kEnterSceneWeightRoundRobinMainScene = 4002,
    kNoUse = 4003,
    //当前服务器不可进入，请重新进入
    kEnterGameGsCrash = 4004,
    //当前服务器不可切换场景
    kEnterSceneServerType = 4005,
    //换场景参数错误
    kEnterSceneParamError = 4006,
    //场景已经满了不能切换
    kEnterSceneSceneFull = 4007,
    //您所要切换的场景不存在
    kEnterSceneSceneNotFound = 4008,
    //您当前就在这个场景，无需切换
    kEnterSceneYouInCurrentScene = 4009,
    kEnterSceneEnterCrossRoomScene = 4010,//不能进入跨服副本
    kEnterSceneGsInfoNull = 4011,//场景的数据不完整
    kEnterSceneGsFull = 4012,//gs 已经满了不能切换
    kEnterSceneYourSceneIsNull = 4013,//你的场景为空
    kEnterSceneChangeSceneOffLine = 4014,//你已经下线
    kEnterSceneChangingScene = 4015,//正在切换场景中
    kEnterSceneChangingGs = 4016,//正在切换gs中
    kChangeScenePlayerQueueComponentNull = 4017,//玩家切换场景队列组件未找到
    kChangeScenePlayerQueueComponentFull = 4018,//玩家切换场景队列组件已满
    kChangeScenePlayerQueueComponentGsNull = 4019,//找不到玩家的gs
    kChangeScenePlayerQueueComponentEmpty = 4020,//玩家切换场景队列为空
    kChangeSceneEnQueueNotSameGs = 4021,//不是同一个场景切换
    //检测进入副本场景未找到
    kCheckEnterSceneSceneParam = 4022,
    //该副本您没有归属权,不能进入
    kCheckEnterSceneCreator = 4023,
    
    //Team
    kTeamNotInApplicants = 5000,
    kTeamPlayerId = 5001,
    kTeamMembersFull = 5002,
    kTeamMemberInTeam = 5003,
    kTeamMemberNotInTeam = 5004,
    kTeamKickSelf = 5005,
    kTeamKickNotLeader = 5006,
    kTeamAppointSelf = 5007,
    kTeamAppointLeaderNotLeader = 5008,
    kTeamFull = 5009,
    kTeamInApplicantList = 5010,
    kTeamNotInApplicantList = 5011,
    kTeamListMaxSize = 5012,
    kTeamHasNotTeamId = 5013,
    kTeamDismissNotLeader = 5014,
    kTeamJoinTeamMemberListToMax = 5013,
    kTeamCreateTeamMaxMemberSize = 5014,
    kTeamPlayerNotFound = 5015,

    //mission
    kMissionTypeRepeated = 6000,
    kMissionAlreadyCompleted = 6001,
    kMissionIdNotInRewardList = 6002,
    kPlayerMissionComponentNotFound = 6003,
    kMissionIdRepeated = 6004,
    kConditionIdOutOfRange = 6005,
    kMissionNotInProgress = 6006,

    //bag 
    kBagDeleteItemFindGuid = 7000,
    kBagDeleteItemAlreadyHasGuid = 7001,
    kBagAddItemHasNotBaseComponent = 7002,
    kBagAddItemInvalidGuid = 7003,
    kBagAddItemInvalidParam = 7004,
    kBagAddItemBagFull = 7005,
    kBagAdequateAddItemSize = 7006,//不可叠加
    kBagAdequateItem = 7007,//不够物品
    kBagDelItemPos = 7008,
    kBagDelItemConfig = 7009,
    kBagDelItemGuid = 7010,
    kBagDelItemFindItem = 7011,
    kBagDelItemSize = 7012,
    kBagDelItemNotAdequateSize = 7013,
};


#define RET_CHECK_RETURN(f)\
{\
uint32_t ret(f);\
if (ret != kOK)\
{\
    return  ret;\
}\
}

#define  CHECK_RETURN_CLOSURE_ERROR(tip_code)\
if ((tip_code) != kOK)\
{\
    response->mutable_error()->set_id(tip_code);\
    return;\
}\

#define CHECK_CONDITION(condition, tip_code)\
if (condition)\
{\
     return  tip_code; \
}\

#define  RETURN_CLOSURE_ERROR(f)\
response->mutable_error()->set_id(f);\
return

#define RETURN_CLOSURE_OK ReturnClosureError(kOK);

#define  RETURN_AUTO_CLOSURE_ERROR(tip_code)\
if ((tip_code) != kOK)\
{\
    response->mutable_error()->set_id(tip_code);\
    return;\
}\

#endif // !COMMON_SRC_tip_code_ERROR_CODE
