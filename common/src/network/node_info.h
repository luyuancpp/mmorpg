#pragma once
#include "src/common_type/common_type.h"

enum eServerType : uint32_t
{
	kMainSceneServer,//主世界场景
	kRoomServer,//副本场景
	kMainSceneCrossServer,//跨服主世界场景
	kRoomSceneCrossServer,//跨服副本场景
};

struct GsServerType
{
	uint32_t server_type_{ kMainSceneServer };
};

enum EnumNodeType : uint32_t
{
	kLoginNode,//登录服务器
	kControllerNode,//场景切换中心服务器
	kGameNode,//逻辑服务器
	kGateNode,//网关服务器
	kDatabaseNode,//数据库服务器
};



