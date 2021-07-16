#ifndef CLIENT_SRC_SERVICE_SERVICE_H_
#define CLIENT_SRC_SERVICE_SERVICE_H_

#include "muduo/net/TcpConnection.h"
#include "muduo/net/TcpClient.h"

#include "src/server_common/codec/codec.h"
#include "src/server_common/codec/dispatcher.h"
#include "src/module/login/login.h"

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
    
private:
    ProtobufDispatcher& dispatcher_;
    ProtobufCodec& codec_;
    TcpConnectionPtr conn_;
    TcpClient& client_;

    LoginModule login_;

    uint64_t player_id_{ 0 };
};

#endif//CLIENT_SRC_SERVICE_SERVICE_H_
