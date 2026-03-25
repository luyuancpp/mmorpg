
#pragma once
#include "proto/scene/player_currency.pb.h"

#include "rpc/player_rpc_response_handler.h"

class SceneCurrencyClientPlayerReply : public ::PlayerServiceReplied
{
public:
    using PlayerServiceReplied::PlayerServiceReplied;

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
            GmAddCurrency(player,
                nullptr,
                static_cast<::GmAddCurrencyResponse*>(response));
            break;
        case 1:
            GmDeductCurrency(player,
                nullptr,
                static_cast<::GmDeductCurrencyResponse*>(response));
            break;
        case 2:
            GetCurrencyList(player,
                nullptr,
                static_cast<::GetCurrencyListResponse*>(response));
            break;
        case 3:
            GmBlockCurrency(player,
                nullptr,
                static_cast<::GmBlockCurrencyResponse*>(response));
            break;
        case 4:
            GmUnblockCurrency(player,
                nullptr,
                static_cast<::GmUnblockCurrencyResponse*>(response));
            break;
        default:
            break;
        }
    }

};
