#pragma once

#include "proto/service/rpc/scene/player_state_attribute_sync.pb.h"

#include "rpc/player_service_interface.h"

#include "macros/return_define.h"

class ScenePlayerSyncHandler : public ::PlayerService
{
public:
    using PlayerService::PlayerService;


    static void SyncBaseAttribute(entt::entity player,
        const ::BaseAttributeSyncDataS2C* request,
        ::Empty* response);
    static void SyncAttribute2Frames(entt::entity player,
        const ::AttributeDelta2FramesS2C* request,
        ::Empty* response);
    static void SyncAttribute5Frames(entt::entity player,
        const ::AttributeDelta5FramesS2C* request,
        ::Empty* response);
    static void SyncAttribute10Frames(entt::entity player,
        const ::AttributeDelta10FramesS2C* request,
        ::Empty* response);
    static void SyncAttribute30Frames(entt::entity player,
        const ::AttributeDelta30FramesS2C* request,
        ::Empty* response);
    static void SyncAttribute60Frames(entt::entity player,
        const ::AttributeDelta60FramesS2C* request,
        ::Empty* response);

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        entt::entity player,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response) override
    {
        switch (method->index())
        {
        case 0:
            SyncBaseAttribute(player,
                static_cast<const ::BaseAttributeSyncDataS2C*>(request),
                static_cast<::Empty*>(response));
            break;
        case 1:
            SyncAttribute2Frames(player,
                static_cast<const ::AttributeDelta2FramesS2C*>(request),
                static_cast<::Empty*>(response));
            break;
        case 2:
            SyncAttribute5Frames(player,
                static_cast<const ::AttributeDelta5FramesS2C*>(request),
                static_cast<::Empty*>(response));
            break;
        case 3:
            SyncAttribute10Frames(player,
                static_cast<const ::AttributeDelta10FramesS2C*>(request),
                static_cast<::Empty*>(response));
            break;
        case 4:
            SyncAttribute30Frames(player,
                static_cast<const ::AttributeDelta30FramesS2C*>(request),
                static_cast<::Empty*>(response));
            break;
        case 5:
            SyncAttribute60Frames(player,
                static_cast<const ::AttributeDelta60FramesS2C*>(request),
                static_cast<::Empty*>(response));
            break;
        default:
            break;
        }
    }

};
