#pragma once

#include <cstdint>

enum  EnumCode : uint32_t
{
	kOK = 0,
	kTableIdInvalidError = 2,
	kInvalidTableData = 3,
	kEntityNull = 4,
	kServiceUnavailable = 5,
	kEntityIsNull = 6,
	kInvalidParameter = 7,
	kServerCrashed = 8,

	// login server
	kLoginAccountNotFound = 1000,
	kLoginAccountPlayerFull = 1001,
	kLoginCreatePlayerUnLoadAccount = 1002,
	kLoginCreatePlayerConnectionHasNotAccount = 1003,
	kLoginUnLogin = 1004,
	kLoginInProgress = 1005,
	kLoginPlayerGuidError = 1006,
	kLoginEnteringGame = 1007,
	kLoginPlaying = 1008,
	kLoginCreatingPlayer = 1009,
	kLoginWaitingEnterGame = 1010,
	kLoginEnterGameGuid = 1011,
	kLoginAccountNameEmpty = 1012,
	kLoginCreateConnectionAccountEmpty = 1013,
	kLoginEnterGameConnectionAccountEmpty = 1014,
	kLoginUnknownError = 1015,
	kLoginSessionDisconnect = 1016,
	kLoginBeKickByAnOtherAccount = 1017,

	// Scene
	kEnterSceneNotFound = 4000,
	kEnterSceneNotFull = 4001,
	kEnterSceneMainFull = 4002,
	kNoUse = 4003,
	kEnterGameNodeCrash = 4004,
	kEnterSceneServerType = 4005,
	kEnterSceneParamError = 4006,
	kEnterSceneSceneFull = 4007,
	kEnterSceneSceneNotFound = 4008,
	kEnterSceneYouInCurrentScene = 4009,
	kEnterSceneEnterCrossRoomScene = 4010,
	kEnterSceneGsInfoNull = 4011,
	kEnterSceneGsFull = 4012,
	kEnterSceneYourSceneIsNull = 4013,
	kEnterSceneChangeSceneOffLine = 4014,
	kEnterSceneChangingScene = 4015,
	kEnterSceneChangingGs = 4016,
	kChangeScenePlayerQueueNotFound = 4017,
	kChangeScenePlayerQueueFull = 4018,
	kChangeScenePlayerQueueComponentGsNull = 4019,
	kChangeScenePlayerQueueComponentEmpty = 4020,
	kChangeSceneEnQueueNotSameGs = 4021,
	kInvalidEnterSceneParameters = 4022,
	kCheckEnterSceneCreator = 4023,

	// Team
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
	kTeamMemberListFull = 5013,
	kTeamCreateTeamMaxMemberSize = 5014,
	kTeamPlayerNotFound = 5015,

	// Mission
	kMissionTypeAlreadyExists = 6000,
	kMissionAlreadyCompleted = 6001,
	kMissionIdNotInRewardList = 6002,
	kPlayerMissionComponentNotFound = 6003,
	kMissionIdRepeated = 6004,
	kConditionIdOutOfRange = 6005,
	kMissionNotInProgress = 6006,

	// Bag 
	kBagDeleteItemFindGuid = 7000,
	kBagDeleteItemAlreadyHasGuid = 7001,
	kBagAddItemHasNotBaseComponent = 7002,
	kBagAddItemInvalidGuid = 7003,
	kBagAddItemInvalidParam = 7004,
	kBagAddItemBagFull = 7005,
	kBagItemNotStacked = 7006,
	kBagInsufficientItems = 7007,
	kBagDelItemPos = 7008,
	kBagDelItemConfig = 7009,
	kBagDelItemGuid = 7010,
	kBagDelItemFindItem = 7011,
	kBagDelItemSize = 7012,
	kItemDeletionSizeMismatch = 7013,
};

#define CHECK_RETURN_IF_NOT_OK(f)\
{\
    uint32_t ret(f);\
    if (ret != kOK)\
    {\
        return ret;\
    }\
}

#define SET_ERROR_AND_RETURN_IF_NOT_OK(tip_code)\
if ((tip_code) != kOK)\
{\
    response->mutable_error()->set_id(tip_code);\
    return;\
}

#define CHECK_CONDITION(condition, tip_code)\
if (condition)\
{\
    return tip_code; \
}


