#ifndef CLIENT_SRC_SERVICE_SERVICE_H_
#define CLIENT_SRC_SERVICE_SERVICE_H_

#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpClient.h"

#include "src/common_type/common_type.h"
#include "src/game_logic/timer_task/timer_task.h"
#include "src/server_common/codec/codec.h"
#include "src/server_common/codec/dispatcher.h"

#include "c2gw.pb.h"

using namespace muduo;
using namespace muduo::net;

using LoginResponsePtr = std::shared_ptr<LoginResponse>;
using CreatePlayerResponsePtr = std::shared_ptr<CreatePlayerResponse>;
using EnterGameResponsePtr = std::shared_ptr<EnterGameResponse>;
using LeaveGameResponsePtr = std::shared_ptr<LeaveGameResponse>;

class ClientService
{
public:
    ClientService(ProtobufDispatcher& dispatcher,
        ProtobufCodec& codec,
        TcpClient& client);

    inline void Send(const google::protobuf::Message& message){ codec_.send(conn_, message);}
    void OnConnection(const muduo::net::TcpConnectionPtr& conn);
    void OnDisconnect();
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
    
    void EnterGame(common::GameGuid guid);
private:
    void DisConnect();

    ProtobufCodec& codec_;
    TcpConnectionPtr conn_;
    TcpClient& client_;
    ProtobufDispatcher& dispatcher_;

    uint64_t guid_{ 0 };

    common::TimerTask timer_task_;
};

#endif//CLIENT_SRC_SERVICE_SERVICE_H_
