
#include "game_player_scene_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "core/network/message_system.h"
#include "player/system/player_node_system.h"
#include "player/system/player_scene_system.h"
#include "proto/scene/player_scene.pb.h"
#include "scene/comp/scene_comp.h"
#include "scene/system/scene_system.h"
#include "service_info/player_scene_service_info.h"
///<<< END WRITING YOUR CODE



void SceneScenePlayerHandler::EnterScene(entt::entity player,const ::GsEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


}



void SceneScenePlayerHandler::LeaveScene(entt::entity player,const ::GsLeaveSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


}



void SceneScenePlayerHandler::EnterSceneS2C(entt::entity player,const ::EnterSceneS2CRequest* request,
	::EnterScenerS2CResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


}


