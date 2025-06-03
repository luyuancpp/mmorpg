#pragma once

#include "message_limiter/message_limiter.h"
#include "muduo/net/TcpConnection.h"

#include "type_define/type_define.h"

struct Session
{
	[[nodiscard]] bool HasLoginNodeId() const { return loginNodeId != kInvalidNodeId; }
	NodeId sceneNodeId{kInvalidNodeId};
	NodeId loginNodeId{kInvalidNodeId};
	NodeId centreNodeId{kInvalidNodeId};
	Guid playerGuild{ kInvalidGuid };
	muduo::net::TcpConnectionPtr conn;
	MessageLimiter messageLimiter;
};


