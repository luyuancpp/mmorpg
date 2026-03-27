
#include "player_currency_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "modules/currency/constants/currency.h"
#include "modules/currency/system/currency_system.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "core/utils/registry/game_registry.h"
///<<< END WRITING YOUR CODE

void SceneCurrencyClientPlayerHandler::GmAddCurrency(entt::entity player,const ::GmAddCurrencyRequest* request,
	::GmAddCurrencyResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	const auto type = static_cast<CurrencyType>(request->currency_type());
	const auto err = CurrencySystem::AddCurrency(player, type, request->amount());
	if (err != kSuccess)
	{
		tlsEcs.globalRegistry.get_or_emplace<TipInfoMessage>(tlsEcs.GlobalEntity()).set_id(err);
		return;
	}
	response->set_balance_after(CurrencySystem::GetBalance(player, type));
///<<< END WRITING YOUR CODE

}

void SceneCurrencyClientPlayerHandler::GmDeductCurrency(entt::entity player,const ::GmDeductCurrencyRequest* request,
	::GmDeductCurrencyResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	const auto type = static_cast<CurrencyType>(request->currency_type());
	const auto err = CurrencySystem::DeductCurrency(player, type, request->amount());
	if (err != kSuccess)
	{
		tlsEcs.globalRegistry.get_or_emplace<TipInfoMessage>(tlsEcs.GlobalEntity()).set_id(err);
		return;
	}
	response->set_balance_after(CurrencySystem::GetBalance(player, type));
///<<< END WRITING YOUR CODE

}

void SceneCurrencyClientPlayerHandler::GetCurrencyList(entt::entity player,const ::GetCurrencyListRequest* request,
	::GetCurrencyListResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	const auto* currency = tlsEcs.actorRegistry.try_get<CurrencyComp>(player);
	if (currency != nullptr)
	{
		response->mutable_currency()->CopyFrom(*currency);
	}
///<<< END WRITING YOUR CODE

}

void SceneCurrencyClientPlayerHandler::GmBlockCurrency(entt::entity player,const ::GmBlockCurrencyRequest* request,
	::GmBlockCurrencyResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	const auto type = static_cast<CurrencyType>(request->currency_type());
	const auto err = CurrencySystem::BlockCurrency(player, type);
	if (err != kSuccess)
	{
		tlsEcs.globalRegistry.get_or_emplace<TipInfoMessage>(tlsEcs.GlobalEntity()).set_id(err);
		return;
	}
///<<< END WRITING YOUR CODE

}

void SceneCurrencyClientPlayerHandler::GmUnblockCurrency(entt::entity player,const ::GmUnblockCurrencyRequest* request,
	::GmUnblockCurrencyResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	const auto type = static_cast<CurrencyType>(request->currency_type());
	const auto err = CurrencySystem::UnblockCurrency(player, type);
	if (err != kSuccess)
	{
		tlsEcs.globalRegistry.get_or_emplace<TipInfoMessage>(tlsEcs.GlobalEntity()).set_id(err);
		return;
	}
///<<< END WRITING YOUR CODE

}
