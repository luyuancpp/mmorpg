#include "service.h"

#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Logging.h"

#include "src/luacpp/lua_module.h"
#include "src/pb/pbc/service.h"

extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_player_service;

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

void ClientService::Send(uint32_t message_id, const google::protobuf::Message& request)
{
    ClientRequest message;
    message.set_id(++id_);
    message.set_message_id(message_id);
    message.set_request(request.SerializeAsString());
    codec_.send(conn_, message);
}

void ClientService::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    conn_ = conn;
}

void ClientService::ReadyGo()
{
    AutoLuaPlayerPtr p(&tls_lua_state.set("player", this));
    tls_lua_state["ReadyGo"]();
}

void ClientService::OnLoginReplied(const muduo::net::TcpConnectionPtr& conn, 
                                   const LoginResponsePtr& message, 
                                   muduo::Timestamp)
{
    if (message->players().empty())
    {        
        AutoLuaPlayerPtr p(&tls_lua_state.set("player", this));
        tls_lua_state["CreatePlayer"]();
        return;
    }
    EnterGs(message->players(0).player_id());   
}

void ClientService::OnCreatePlayerReplied(const muduo::net::TcpConnectionPtr& conn, 
    const CreatePlayerResponsePtr& message,
    muduo::Timestamp)
{
    EnterGs(message->players(0).player_id());
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
    auto message_id = message->message_id();
    auto& servcie_method_info = g_service_method_info[message_id];
    auto sit = g_player_service.find(servcie_method_info.service);
    if (sit == g_player_service.end())
    {
        LOG_ERROR << "service not found " << servcie_method_info.service;
        return;
    }
    const google::protobuf::ServiceDescriptor* desc = sit->second->GetDescriptor();
    const google::protobuf::MethodDescriptor* method
        = desc->FindMethodByName(servcie_method_info.method);
    MessagePtr response(codec_.createMessage(servcie_method_info.response));
    response->ParseFromString(message->body());
    AutoLuaPlayerPtr p(&tls_lua_state.set("player", this));
    g_player_service[servcie_method_info.service]->CallMethod(method, nullptr, nullptr, response.get(), nullptr);
}

void ClientService::EnterGs(Guid guid)
{
    AutoLuaPlayerPtr p(&tls_lua_state.set("player", this));
    tls_lua_state["EnterGame"](guid);
}

void ClientService::DisConnect()
{
    client_.disconnect();
}

