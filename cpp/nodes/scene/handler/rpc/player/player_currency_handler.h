#pragma once

#include <type_traits>
#include <utility>

#include "proto/scene/player_currency.pb.h"

#include "rpc/player_service_interface.h"

#include "macros/return_define.h"

class SceneCurrencyClientPlayerHandler : public ::PlayerService
{
public:
    using PlayerService::PlayerService;

    static void GmAddCurrency(entt::entity player,
        const ::GmAddCurrencyRequest* request,
        ::GmAddCurrencyResponse* response);
    static void GmDeductCurrency(entt::entity player,
        const ::GmDeductCurrencyRequest* request,
        ::GmDeductCurrencyResponse* response);
    static void GetCurrencyList(entt::entity player,
        const ::GetCurrencyListRequest* request,
        ::GetCurrencyListResponse* response);
    static void GmBlockCurrency(entt::entity player,
        const ::GmBlockCurrencyRequest* request,
        ::GmBlockCurrencyResponse* response);
    static void GmUnblockCurrency(entt::entity player,
        const ::GmUnblockCurrencyRequest* request,
        ::GmUnblockCurrencyResponse* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmAddCurrencyResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmAddCurrency(player,
                static_cast<const ::GmAddCurrencyRequest*>(request),
                static_cast<::GmAddCurrencyResponse*>(response));
			}
            break;
        case 1:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmDeductCurrencyResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmDeductCurrency(player,
                static_cast<const ::GmDeductCurrencyRequest*>(request),
                static_cast<::GmDeductCurrencyResponse*>(response));
			}
            break;
        case 2:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GetCurrencyListResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GetCurrencyList(player,
                static_cast<const ::GetCurrencyListRequest*>(request),
                static_cast<::GetCurrencyListResponse*>(response));
			}
            break;
        case 3:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmBlockCurrencyResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmBlockCurrency(player,
                static_cast<const ::GmBlockCurrencyRequest*>(request),
                static_cast<::GmBlockCurrencyResponse*>(response));
			}
            break;
        case 4:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GmUnblockCurrencyResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GmUnblockCurrency(player,
                static_cast<const ::GmUnblockCurrencyRequest*>(request),
                static_cast<::GmUnblockCurrencyResponse*>(response));
			}
            break;
        default:
            break;
        }
    }

};
