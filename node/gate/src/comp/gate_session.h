#pragma once

#include "muduo/net/TcpConnection.h"

#include "type_define/type_define.h"

struct Session
{
	bool HasLoginNodeId() const { return login_node_id_ != kInvalidNodeId; }
	NodeId game_node_id_{kInvalidNodeId};
	NodeId login_node_id_{kInvalidNodeId};
	NodeId centre_node_id_{kInvalidNodeId};
	Guid player_guild_{ kInvalidGuid };
	muduo::net::TcpConnectionPtr conn_;
};


