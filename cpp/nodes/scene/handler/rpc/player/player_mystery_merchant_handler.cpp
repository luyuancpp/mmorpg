
#include "player_mystery_merchant_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "engine/core/time/system/time.h"
#include "services/scene/player/system/player_mystery_merchant.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "thread_context/ecs_context.h"

namespace
{
	// 失败码统一写全局 tip 实体,由生成层 TRANSFER_ERROR_MESSAGE 搬进响应并清空
	void SetTip(uint32_t err)
	{
		tlsEcs.globalRegistry.get_or_emplace<TipInfoMessage>(tlsEcs.GlobalEntity()).set_id(err);
	}
} // namespace
///<<< END WRITING YOUR CODE

void SceneMysteryMerchantClientPlayerHandler::GetMysteryMerchantInfo(entt::entity player,const ::GetMysteryMerchantInfoRequest* request,
	::GetMysteryMerchantInfoResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	// 纯读取:在不在、在几线、(同线时)货架与已购数。规则全在 PlayerMysteryMerchantSystem,这里只取墙钟。
	PlayerMysteryMerchantSystem::FillInfo(player, TimeSystem::NowMillisecondsUTC(), *response->mutable_info());
///<<< END WRITING YOUR CODE

}

void SceneMysteryMerchantClientPlayerHandler::BuyMysteryMerchantGoods(entt::entity player,const ::BuyMysteryMerchantGoodsRequest* request,
	::BuyMysteryMerchantGoodsResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	// 全部校验(同线 / 距离 / 时间窗 / 限购 / 余额 / 背包)与结算都在 PlayerMysteryMerchantSystem::Buy;
	// 成功时它已经把最新货架与背包全量写进 response。
	if (const auto err = PlayerMysteryMerchantSystem::Buy(player, *request, TimeSystem::NowMillisecondsUTC(), *response);
		err != kSuccess)
	{
		SetTip(err);
	}
///<<< END WRITING YOUR CODE

}

void SceneMysteryMerchantClientPlayerHandler::NotifyMysteryMerchant(entt::entity player,const ::MysteryMerchantNotify* request,
	::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}
