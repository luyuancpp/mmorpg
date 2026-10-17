#pragma once

// zone Redis 应答 → battle_settlement::ReplyShape 的唯一适配(battle 结算 / 活动局发件箱与 scene 登录钩子共用)。
//
// 单独成头是为了让 settlement_outbox.h 保持零依赖:判定函数只认 ReplyShape,单测不用拼 redisReply 也能盯住;
// 这里是唯一碰 hiredis 类型的地方,空应答 → kNoReply 这一条映射也由单测直接盯住
// (cpp/tests/routing_identity_test,SettlementReplyShape.*)。

#include <string_view>

#include <hiredis/hiredis.h>

#include "settlement_outbox.h"

namespace battle_settlement
{
	inline ReplyShape ReplyShapeOf(const redisReply *reply)
	{
		if (reply == nullptr)
		{
			return ReplyShape::kNoReply;
		}
		switch (reply->type)
		{
		case REDIS_REPLY_NIL:
			return ReplyShape::kNil;
		case REDIS_REPLY_STRING:
			return ReplyShape::kString;
		case REDIS_REPLY_STATUS:
			return ReplyShape::kStatus;
		case REDIS_REPLY_INTEGER:
			return ReplyShape::kInteger;
		case REDIS_REPLY_ARRAY:
			return ReplyShape::kArray;
		case REDIS_REPLY_ERROR:
			return ReplyShape::kError;
		default:
			return ReplyShape::kOther;
		}
	}

	// 应答携带的文本(字符串值、状态文本、错误文本);其余形状与空应答返回空串。只读视图,寿命同 reply。
	// muduo LogStream 没有 string_view 重载,进日志时包一层 std::string。
	inline std::string_view ReplyText(const redisReply *reply)
	{
		if (reply == nullptr || reply->str == nullptr)
		{
			return {};
		}
		switch (reply->type)
		{
		case REDIS_REPLY_STRING:
		case REDIS_REPLY_STATUS:
		case REDIS_REPLY_ERROR:
			return std::string_view(reply->str, reply->len);
		default:
			return {};
		}
	}
} // namespace battle_settlement
