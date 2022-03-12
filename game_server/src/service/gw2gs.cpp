#include "gw2gs.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/game_server.h"
#include "src/module/player_list/player_list.h"
#include "src/server_common/closure_auto_done.h"

#include "c2gw.pb.h"
using namespace common;
using namespace game;
///<<< END WRITING YOUR CODE

namespace gw2gs{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void Gw2gsServiceImpl::PlayerService(::google::protobuf::RpcController* controller,
    const gw2gs::RpcClientRequest* request,
    gw2gs::RpcClientResponse* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE PlayerService
	c2gw::ClientRequest clientrequest;
	clientrequest.ParseFromString(request->request());
    auto it = services_.find(clientrequest.service());
    if (it == services_.end())
    {
        return;
    }
    google::protobuf::Service* service = it->second;
    const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
    const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(clientrequest.method());
    if (nullptr ==  method)
    {
        //todo client error;
        return;          
    }
    auto player = g_players.find(request->player_id());
    //if (request->)
    {
    }

    std::unique_ptr<google::protobuf::Message> player_request(service->GetRequestPrototype(method).New());
    player_request->ParseFromString(clientrequest.request());
    std::unique_ptr<google::protobuf::Message> player_response(service->GetResponsePrototype(method).New());

        
///<<< END WRITING YOUR CODE PlayerService
}

///<<<rpc end
}// namespace gw2gs
