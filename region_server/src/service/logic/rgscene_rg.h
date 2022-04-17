#ifndef REGION_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_RG_H_
#define REGION_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_RG_H_
#include "player_service.h"
#include "logic_proto/scene_rg.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace regionservcie{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class RgServiceImpl : public PlayerService {
public:
    using PlayerService::PlayerService;
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void StartCrossMainGS(common::EntityPtr& entity,
        const regionservcie::StartMainRoomGSRequest* request,
        ::google::protobuf::Empty* response);
    void StartCrossRoomGS(common::EntityPtr& entity,
        const regionservcie::StartCrossRoomGSRequest* request,
        ::google::protobuf::Empty* response);
        void CallMethod(const ::google::protobuf::MethodDescriptor* method,
        common::EntityPtr& entity,
        const ::google::protobuf::Message* request,
        ::google::protobuf::Message* response)override
        {
        switch(method->index()) {
        case 0:
            StartCrossMainGS(entity,
            ::google::protobuf::internal::DownCast<const regionservcie::StartMainRoomGSRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        case 1:
            StartCrossRoomGS(entity,
            ::google::protobuf::internal::DownCast<const regionservcie::StartCrossRoomGSRequest*>( request),
            ::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
        break;
        default:
            GOOGLE_LOG(FATAL) << "Bad method index; this should never happen.";
        break;
        }
        }
};
}// namespace regionservcie
#endif//REGION_SERVER_SRC_SERVICE_LOGIC_LOGIC_PROTO_SCENE_RG_H_
