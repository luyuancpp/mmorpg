#pragma once
#include "gs2gw.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
namespace gs2gw{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
class Gs2GwServiceImpl : public Gs2GwService{
public:
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
public:
    void PlayerService(::google::protobuf::RpcController* controller,
        const gs2gw::PlayerMessageRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace gs2gw

