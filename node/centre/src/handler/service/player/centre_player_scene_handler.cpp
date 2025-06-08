
#include "centre_player_scene_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "scene/system/scene_system.h"
#include "pbc/scene_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"
#include "centre_node.h"
#include "scene/system/player_scene_system.h"
#include "player/system/player_tip_system.h"
#include "scene/system/player_change_scene_system.h"
#include "network/message_system.h"
#include "service_info/player_scene_service_info.h"

#include "proto/logic/component/player_network_comp.pb.h"
///<<< END WRITING YOUR CODE



void CentrePlayerSceneHandler::EnterScene(entt::entity player,const ::CentreEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


}



void CentrePlayerSceneHandler::LeaveScene(entt::entity player,const ::CentreLeaveSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


}



void CentrePlayerSceneHandler::LeaveSceneAsyncSavePlayerComplete(entt::entity player,const ::CentreLeaveSceneAsyncSavePlayerCompleteRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


}



void CentrePlayerSceneHandler::SceneInfoC2S(entt::entity player,const ::CentreSceneInfoRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


}


