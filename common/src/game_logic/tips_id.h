#ifndef COMMON_SRC_tip_code_ERROR_CODE
#define COMMON_SRC_tip_code_ERROR_CODE

#include <cstdint>


enum  EnumCode : uint32_t
{
    kRetOK = 0,
    kRetTableId = 2,
    kRetConfigData = 3,
    kRetEnttNull = 4,
    kRetServiceNotOpen = 5, //改功能暂时关闭gate 用,gate写死了
    kRetServerCrush = 6,//服务器关闭请重新登录 gate 用,gate写死了

    //login server 
    kRetLoginCantFindAccount = 1000,//找不到
    kRetLoginAccountPlayerFull = 1001,
    kRetLoginCreatePlayerUnLoadAccount = 1002,
    kRetLoginCreatePlayerConnectionHasNotAccount = 1003,
    kRetLoginUnLogin = 1004,
    kRetLoginIng = 1005,
    kRetLoginPlayerGuidError = 1006,
    kRetLoginEnteringGame = 1007,
    kRetLoginPlaying = 1008,
    kRetLoginCreatingPlayer = 1009,
    kRetLoginWaitingEnterGame = 1010,//已经登录了，等待进入游戏
    kRetLoginEnterGuid = 1011,
    kRetLoginAccountNameEmpty = 1012,
    kRetLoginCreateConnectionAccountEmpty = 1013,
    kRetLoginEnterGameConnectionAccountEmpty = 1014,
    kRetLoginUnknownError = 1015,
    kRetLoginSessionDisconnect = 1016,

    //Scene
    kRetEnterSceneNotFound = 4000,
    kRetEnterSceneNotFull = 4001,
    kRetEnterSceneWeightRoundRobinMainScene = 4002,//所有场景已经满了
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
    kRetChangeScenePlayerQueueComponentNull = 4017,//玩家切换场景队列组件未找到
    kRetChangeScenePlayerQueueComponentFull = 4018,//玩家切换场景队列组件已满
    kRetChangeScenePlayerQueueComponentGsNull = 4019,//找不到玩家的gs
    kRetChangeScenePlayerQueueComponentEmpty = 4020,//玩家切换场景队列为空
    kRetChangeSceneEnQueueNotSameGs = 4021,//不是同一个场景切换

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
    kRetMissionTypeRepeated = 6001,
    kRetMissionComplete = 6002,
    kRetMissionGetRewardNoMissionId = 6003,
    kRetMissionPlayerMissionCompNotFound = 6004,

    //bag 
    kRetBagDeleteItemFindGuid = 7000,
    kRetBagDeleteItemAlreadyHasGuid = 7001,
    kRetBagAddItemHasNotBaseComponent = 7002,
    kRetBagAddItemInvalidGuid = 7003,
    kRetBagAddItemInvalidParam = 7004,
    kRetBagAddItemBagFull = 7005,
    kRetBagAdequateAddItemSize = 7006,//不可叠加
    kRetBagAdequateItem = 7007,//不够物品
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

#define  CheckReturnClosureError(tip_code)\
if (tip_code != kRetOK)\
{\
    response->mutable_error()->set_id(tip_code);\
    if (nullptr != done)\
    {\
        done->Run();\
    }\
    return;\
}\

#define CheckCondition(condition, tip_code)\
if (condition)\
{\
     return  tip_code; \
}\

#define  ReturnClosureError(f)\
response->mutable_error()->set_id(f);\
if (nullptr != done)\
{\
done->Run();\
}\
return;

#define ReturnClosureOK ReturnClosureError(kRetOK);

#define  ReturnAutoClosureError(tip_code)\
if (tip_code != kRetOK)\
{\
    response->mutable_error()->set_id(tip_code);\
    return;\
}\

#define  GetPlayerComponentReturnError(component_name, component, tip_code)\
auto component_name = tls.registry.try_get<component>(player);\
if (nullptr == component_name)\
{\
	return tip_code;\
}

#define  GetPlayerComponentMemberReturnError(member_name, component, tip_code)\
auto component_name = tls.registry.try_get<component>(player);\
if (nullptr == component_name)\
{\
	return tip_code;\
}\
auto& member_name = component_name->member_name##_;

#define  GetPlayerComponentMemberNullReturnFalse(member_name, component)\
auto component_name = tls.registry.try_get<component>(player);\
if (nullptr == component_name)\
{\
	return false;\
}\
auto& member_name = component_name->member_name##_;

#define  GetPlayerComponentMemberNullReturnTrue(member_name, component)\
auto component_name = tls.registry.try_get<component>(player);\
if (nullptr == component_name)\
{\
	return true;\
}\
auto& member_name = comp_name->menber_name##_;

#define  GetPlayerComponentMemberReturnVoid(member_name, component)\
auto component_name = tls.registry.try_get<component>(player);\
if (nullptr == component_name)\
{\
	return;\
}\
auto& member_name = component_name->member_name##_;

#endif // !COMMON_SRC_tip_code_ERROR_CODE
