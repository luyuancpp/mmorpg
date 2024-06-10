#pragma once
#include "src/type_define/type_define.h"

enum e_server_type : uint32_t
{
	kMainSceneNode,//主世界场景
	kRoomNode,//副本场景
	kMainSceneCrossNode,//跨服主世界场景
	kRoomSceneCrossNode,//跨服副本场景
};

struct GsNodeType
{
	uint32_t server_type_{ kMainSceneNode };
};

enum enum_node_type : uint32_t
{
	kLoginNode,//登录服务器
	kCentreNode,//场景切换中心服务器
	kGameNode,//逻辑服务器
	kGateNode,//网关服务器
	kDatabaseNode,//数据库服务器
};



