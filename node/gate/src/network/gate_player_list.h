#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/type_define/type_define.h"

struct Session
{
	bool HasLoginNodeId() const { return login_node_id_ != kInvalidNodeId; }
	NodeId game_node_id_{kInvalidNodeId};
	NodeId login_node_id_{kInvalidNodeId};
	NodeId controller_id_{kInvalidNodeId};
	muduo::net::TcpConnectionPtr conn_;
};


