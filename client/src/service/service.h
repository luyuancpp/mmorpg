#pragma once

#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpClient.h"

#include "src/common_type/common_type.h"
#include "src/game_logic/timer_task/timer_task.h"
#include "src/network/codec/codec.h"
#include "src/network/codec/dispatcher.h"
#include "src/pb/pb2sol2/pb2sol2.h"

#include "c2gw.pb.h"
#include "logic_proto/scene_client_player.pb.h"

using namespace muduo;
using namespace muduo::net;
using namespace c2gw;
using namespace clientplayer;


using LoginResponsePtr = std::shared_ptr<LoginResponse>;
using CreatePlayerResponsePtr = std::shared_ptr<CreatePlayerResponse>;
using EnterGameResponsePtr = std::shared_ptr<EnterGameResponse>;
using LeaveGameResponsePtr = std::shared_ptr<LeaveGameResponse>;
using ClientResponsePtr = std::shared_ptr<ClientResponse>;
using MessageBodyPtr = std::shared_ptr<MessageBody>;
using EnterSeceneS2CPtr = std::shared_ptr<EnterSeceneS2C>;

struct AutoLuaPlayer
{
	void operator()(sol::state_view* v) { common::g_lua.set("player", sol::lua_nil); }
};

using AutoLuaPlayerPtr = std::unique_ptr<sol::state_view, AutoLuaPlayer>;

class ClientService
{
public:
    ClientService(ProtobufDispatcher& dispatcher,
        ProtobufCodec& codec,
        TcpClient& client);

    void Send(const google::protobuf::Message& message);
    void SendOhter(const google::protobuf::Message& message);
    void OnConnection(const muduo::net::TcpConnectionPtr& conn);
    void ReadyGo();
    
    void OnLoginReplied(const muduo::net::TcpConnectionPtr& conn,
        const LoginResponsePtr& message,
        muduo::Timestamp);
    void OnCreatePlayerReplied(const muduo::net::TcpConnectionPtr& conn,
        const CreatePlayerResponsePtr& message,
        muduo::Timestamp);
    void OnEnterGameReplied(const muduo::net::TcpConnectionPtr& conn,
        const EnterGameResponsePtr& message,
        muduo::Timestamp);

    void OnLeaveGameReplied(const muduo::net::TcpConnectionPtr& conn,
        const LeaveGameResponsePtr& message,
        muduo::Timestamp);

	void OnGsReplied(const muduo::net::TcpConnectionPtr& conn,
		const ClientResponsePtr& message,
		muduo::Timestamp);

	void OnMessageBodyReplied(const muduo::net::TcpConnectionPtr& conn,
		const MessageBodyPtr& message,
		muduo::Timestamp);

    void OnMessageEnterSeceneS2CPtr(const muduo::net::TcpConnectionPtr& conn,
        const EnterSeceneS2CPtr& message,
        muduo::Timestamp);
    
    void EnterGame(Guid guid);

private:
    void DisConnect();

    ProtobufCodec& codec_;
    TcpConnectionPtr conn_;
    TcpClient& client_;
    ProtobufDispatcher& dispatcher_;

    uint64_t guid_{ 0 };
    uint64_t id_{ 0 };
    TimerTask timer_task_;
};

