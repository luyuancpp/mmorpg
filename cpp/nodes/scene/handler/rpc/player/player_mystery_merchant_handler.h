#pragma once

#include <type_traits>
#include <utility>

#include "proto/scene/player_mystery_merchant.pb.h"

#include "rpc/player_service_interface.h"

#include "macros/return_define.h"

class SceneMysteryMerchantClientPlayerHandler : public ::PlayerService
{
public:
    using PlayerService::PlayerService;

    static void GetMysteryMerchantInfo(entt::entity player,
        const ::GetMysteryMerchantInfoRequest* request,
        ::GetMysteryMerchantInfoResponse* response);
    static void BuyMysteryMerchantGoods(entt::entity player,
        const ::BuyMysteryMerchantGoodsRequest* request,
        ::BuyMysteryMerchantGoodsResponse* response);
    static void NotifyMysteryMerchant(entt::entity player,
        const ::MysteryMerchantNotify* request,
        ::Empty* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
			{
            static_assert(std::is_same_v<decltype(std::declval<::GetMysteryMerchantInfoResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            GetMysteryMerchantInfo(player,
                static_cast<const ::GetMysteryMerchantInfoRequest*>(request),
                static_cast<::GetMysteryMerchantInfoResponse*>(response));
			}
            break;
        case 1:
			{
            static_assert(std::is_same_v<decltype(std::declval<::BuyMysteryMerchantGoodsResponse&>().mutable_error_message()), ::TipInfoMessage*>,
                "玩家 RPC 非 Empty 应答必须提供 TipInfoMessage error_message");
            BuyMysteryMerchantGoods(player,
                static_cast<const ::BuyMysteryMerchantGoodsRequest*>(request),
                static_cast<::BuyMysteryMerchantGoodsResponse*>(response));
			}
            break;
        case 2:
			{
            NotifyMysteryMerchant(player,
                static_cast<const ::MysteryMerchantNotify*>(request),
                static_cast<::Empty*>(response));
			}
            break;
        default:
            break;
        }
    }

};
