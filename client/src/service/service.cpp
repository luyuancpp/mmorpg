#include "service.h"

#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Logging.h"

#include "src/luacpp/lua_module.h"
#include "src/pb/pbc/msgmap.h"

using namespace common;
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
	dispatcher_.registerMessageCallback<ClientResponse>(
		std::bind(&ClientService::OnGsReplied, this, _1, _2, _3));
	dispatcher_.registerMessageCallback<ClientResponse>(
		std::bind(&ClientService::OnGsReplied, this, _1, _2, _3));
	dispatcher_.registerMessageCallback<MessageBody>(
		std::bind(&ClientService::OnMessageBodyReplied, this, _1, _2, _3));
	dispatcher_.registerMessageCallback<EnterSeceneS2C>(
		std::bind(&ClientService::OnMessageEnterSeceneS2CPtr, this, _1, _2, _3));

}

void ClientService::Send(const google::protobuf::Message& message)
{
    ClientRequest wrapper_message;
    wrapper_message.set_id(++id_);
    wrapper_message.set_request(message.SerializeAsString());
    auto message_id = g_msgid[message.GetDescriptor()->full_name()];
    wrapper_message.set_msg_id(message_id);
    wrapper_message.set_service(g_serviceinfo[message_id].service);
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
    EnterGame(message->players(0).guid());   
}

void ClientService::OnCreatePlayerReplied(const muduo::net::TcpConnectionPtr& conn, 
    const CreatePlayerResponsePtr& message,
    muduo::Timestamp)
{
    EnterGame(message->players(0).guid());
}

void ClientService::OnEnterGameReplied(const muduo::net::TcpConnectionPtr& conn, 
    const EnterGameResponsePtr& message,
    muduo::Timestamp)
{
	SeceneTestRequest request;
	Send(request);
}

void ClientService::OnLeaveGameReplied(const muduo::net::TcpConnectionPtr& conn, 
    const LeaveGameResponsePtr& message, 
    muduo::Timestamp)
{
    timer_task_.RunAfter(1, std::bind(&ClientService::DisConnect, this));
}

void ClientService::OnGsReplied(const muduo::net::TcpConnectionPtr& conn, 
    const ClientResponsePtr& message, 
    muduo::Timestamp t)
{
    auto msg_id = message->msg_id();
    MessagePtr response(codec_.createMessage(g_serviceinfo[msg_id].response));
    response->ParseFromString(message->response());
    dispatcher_.onProtobufMessage(conn, response, t);
}

void ClientService::OnMessageBodyReplied(const muduo::net::TcpConnectionPtr& conn,
    const MessageBodyPtr& message,
    muduo::Timestamp t)
{
	auto msg_id = message->msg_id();
	MessagePtr response(codec_.createMessage(g_serviceinfo[msg_id].response));
	response->ParseFromString(message->body());
	dispatcher_.onProtobufMessage(conn, response, t);
}

void ClientService::OnMessageEnterSeceneS2CPtr(const muduo::net::TcpConnectionPtr& conn,
    const EnterSeceneS2CPtr& message,
    muduo::Timestamp)
{
    AutoLuaPlayerPtr p(&g_lua.set("player", this));
	g_lua["LeaveGame"]();
}

void ClientService::EnterGame(Guid guid)
{
    AutoLuaPlayerPtr p(&g_lua.set("player", this));
    g_lua["EnterGame"](guid);
}

void ClientService::DisConnect()
{
    client_.disconnect();
}

