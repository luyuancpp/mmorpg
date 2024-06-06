#pragma once

#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpClient.h"

#include "src/type_define/type_define.h"
#include "src/timer_task/timer_task.h"
#include "src/thread_local/thread_local_storage_lua.h"
#include "src/network/codec/codec.h"
#include "src/network/codec/dispatcher.h"

#include "common_proto/c2gate.pb.h"
#include "client_player_proto/scene_client_player.pb.h"

using namespace muduo;
using namespace muduo::net;

using LoginResponsePtr = std::shared_ptr<LoginResponse>;
using CreatePlayerResponsePtr = std::shared_ptr<CreatePlayerResponse>;
using EnterGameResponsePtr = std::shared_ptr<EnterGameResponse>;
using MessageBodyPtr = std::shared_ptr<MessageBody>;
using EnterSeceneS2CPtr = std::shared_ptr<EnterSceneS2C>;

struct AutoLuaPlayer
{
	void operator()(sol::state_view* v) { tls_lua_state.set("player", sol::lua_nil); }
};

using AutoLuaPlayerPtr = std::unique_ptr<sol::state_view, AutoLuaPlayer>;

class ClientService
{
public:
    ClientService(ProtobufDispatcher& dispatcher,
        ProtobufCodec& codec,
        TcpClient& client);

    void Send(uint32_t message_id, const google::protobuf::Message& message);
    void OnConnection(const muduo::net::TcpConnectionPtr& conn);
    void ReadyGo();
    
	void OnMessageBodyReplied(const muduo::net::TcpConnectionPtr& conn,
		const MessageBodyPtr& message,
		muduo::Timestamp);


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

