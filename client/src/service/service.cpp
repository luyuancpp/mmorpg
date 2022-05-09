#include "service.h"

#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Logging.h"

#include "src/luacpp/lua_module.h"
#include "src/pb/pbc/msgmap.h"
#include "src/service/logic/player_service.h"


using namespace c2gw;

ClientService::ClientService(ProtobufDispatcher& dispatcher,
                             ProtobufCodec& codec, 
                             TcpClient& client) : codec_(codec), 
                                                  client_(client),
                                                  dispatcher_(dispatcher)
{
    dispatcher_.registerMessageCallback<LoginResponse>(
        std::bind(&ClientService::OnLoginReplied, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<CreatePlayerResponse>(
        std::bind(&ClientService::OnCreatePlayerReplied, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<EnterGameResponse>(
        std::bind(&ClientService::OnEnterGameReplied, this, _1, _2, _3));
    dispatcher_.registerMessageCallback<LeaveGameResponse>(
        std::bind(&ClientService::OnLeaveGameReplied, this, _1, _2, _3));
	dispatcher_.registerMessageCallback<MessageBody>(
		std::bind(&ClientService::OnMessageBodyReplied, this, _1, _2, _3));
}

void ClientService::Send(const google::protobuf::Message& message)
{
    ClientRequest wrapper_message;
    wrapper_message.set_id(++id_);
    wrapper_message.set_request(message.SerializeAsString());
    auto message_id = g_msgid[message.GetDescriptor()->full_name()];
    wrapper_message.set_msg_id(message_id);
    codec_.send(conn_, wrapper_message);
}

void ClientService::SendOhter(const google::protobuf::Message& message)
{
    codec_.send(conn_, message);
}

void ClientService::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    conn_ = conn;
}

void ClientService::ReadyGo()
{
    AutoLuaPlayerPtr p(&g_lua.set("player", this));
    g_lua["ReadyGo"]();
}

void ClientService::OnLoginReplied(const muduo::net::TcpConnectionPtr& conn, 
                                   const LoginResponsePtr& message, 
                                   muduo::Timestamp)
{
    if (message->players().empty())
    {        
        AutoLuaPlayerPtr p(&g_lua.set("player", this));
        g_lua["CreatePlayer"]();
        return;
    }
    EnterGs(message->players(0).guid());   
}

void ClientService::OnCreatePlayerReplied(const muduo::net::TcpConnectionPtr& conn, 
    const CreatePlayerResponsePtr& message,
    muduo::Timestamp)
{
    EnterGs(message->players(0).guid());
}

void ClientService::OnEnterGameReplied(const muduo::net::TcpConnectionPtr& conn, 
    const EnterGameResponsePtr& message,
    muduo::Timestamp)
{
}

void ClientService::OnLeaveGameReplied(const muduo::net::TcpConnectionPtr& conn, 
    const LeaveGameResponsePtr& message, 
    muduo::Timestamp)
{
    DisConnect();
}

void ClientService::OnMessageBodyReplied(const muduo::net::TcpConnectionPtr& conn,
    const MessageBodyPtr& message,
    muduo::Timestamp t)
{
    auto msg_id = message->msg_id();
    auto msg_servcie = g_serviceinfo[msg_id];
    auto sit = g_player_services.find(msg_servcie.service);
    if (sit == g_player_services.end())
    {
        LOG_ERROR << "service not found " << msg_servcie.service;
        return;
    }
    const google::protobuf::ServiceDescriptor* desc = sit->second->service()->GetDescriptor();
    const google::protobuf::MethodDescriptor* method
        = desc->FindMethodByName(msg_servcie.method);
    MessagePtr response(codec_.createMessage(msg_servcie.response));
    response->ParseFromString(message->body());
    AutoLuaPlayerPtr p(&g_lua.set("player", this));
    g_player_services[msg_servcie.service]->CallMethod(method, nullptr, response.get());
}

void ClientService::EnterGs(Guid guid)
{
    AutoLuaPlayerPtr p(&g_lua.set("player", this));
    g_lua["EnterGame"](guid);
}

void ClientService::DisConnect()
{
    client_.disconnect();
}

