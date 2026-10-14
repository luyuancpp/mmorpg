#pragma once

#include "proto/scene/player_bag.pb.h"

#include "rpc/player_service_interface.h"

#include "macros/return_define.h"

class SceneBagClientPlayerHandler : public ::PlayerService
{
public:
    using PlayerService::PlayerService;

    static void GetBag(entt::entity player,
        const ::GetBagRequest* request,
        ::GetBagResponse* response);
    static void SortBag(entt::entity player,
        const ::SortBagRequest* request,
        ::SortBagResponse* response);
    static void EquipItem(entt::entity player,
        const ::EquipItemRequest* request,
        ::EquipItemResponse* response);
    static void UnequipItem(entt::entity player,
        const ::UnequipItemRequest* request,
        ::UnequipItemResponse* response);
    static void GmGrantItem(entt::entity player,
        const ::GmGrantItemRequest* request,
        ::GmGrantItemResponse* response);
    static void EnhanceEquip(entt::entity player,
        const ::EnhanceEquipRequest* request,
        ::EnhanceEquipResponse* response);
    static void InlayGem(entt::entity player,
        const ::InlayGemRequest* request,
        ::InlayGemResponse* response);
    static void RemoveGem(entt::entity player,
        const ::RemoveGemRequest* request,
        ::RemoveGemResponse* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
			{
            GetBag(player,
                static_cast<const ::GetBagRequest*>(request),
                static_cast<::GetBagResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GetBagResponse*>(response));
			}
            break;
        case 1:
			{
            SortBag(player,
                static_cast<const ::SortBagRequest*>(request),
                static_cast<::SortBagResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::SortBagResponse*>(response));
			}
            break;
        case 2:
			{
            EquipItem(player,
                static_cast<const ::EquipItemRequest*>(request),
                static_cast<::EquipItemResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::EquipItemResponse*>(response));
			}
            break;
        case 3:
			{
            UnequipItem(player,
                static_cast<const ::UnequipItemRequest*>(request),
                static_cast<::UnequipItemResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::UnequipItemResponse*>(response));
			}
            break;
        case 4:
			{
            GmGrantItem(player,
                static_cast<const ::GmGrantItemRequest*>(request),
                static_cast<::GmGrantItemResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::GmGrantItemResponse*>(response));
			}
            break;
        case 5:
			{
            EnhanceEquip(player,
                static_cast<const ::EnhanceEquipRequest*>(request),
                static_cast<::EnhanceEquipResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::EnhanceEquipResponse*>(response));
			}
            break;
        case 6:
			{
            InlayGem(player,
                static_cast<const ::InlayGemRequest*>(request),
                static_cast<::InlayGemResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::InlayGemResponse*>(response));
			}
            break;
        case 7:
			{
            RemoveGem(player,
                static_cast<const ::RemoveGemRequest*>(request),
                static_cast<::RemoveGemResponse*>(response));
            TRANSFER_ERROR_MESSAGE(static_cast<::RemoveGemResponse*>(response));
			}
            break;
        default:
            break;
        }
    }

};
