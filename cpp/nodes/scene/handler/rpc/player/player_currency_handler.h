#pragma once

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
            GmAddCurrency(player,
                static_cast<const ::GmAddCurrencyRequest*>(request),
                static_cast<::GmAddCurrencyResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmAddCurrencyResponse*>(response));
			}
            break;
        case 1:
			{
            GmDeductCurrency(player,
                static_cast<const ::GmDeductCurrencyRequest*>(request),
                static_cast<::GmDeductCurrencyResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmDeductCurrencyResponse*>(response));
			}
            break;
        case 2:
			{
            GetCurrencyList(player,
                static_cast<const ::GetCurrencyListRequest*>(request),
                static_cast<::GetCurrencyListResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GetCurrencyListResponse*>(response));
			}
            break;
        case 3:
			{
            GmBlockCurrency(player,
                static_cast<const ::GmBlockCurrencyRequest*>(request),
                static_cast<::GmBlockCurrencyResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmBlockCurrencyResponse*>(response));
			}
            break;
        case 4:
			{
            GmUnblockCurrency(player,
                static_cast<const ::GmUnblockCurrencyRequest*>(request),
                static_cast<::GmUnblockCurrencyResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmUnblockCurrencyResponse*>(response));
			}
            break;
        default:
            break;
        }
    }

};
