#ifndef GATEWAY_SERVER_SRC_msg_receiver
#define GATEWAY_SERVER_SRC_msg_receiver

#include "c2gw.pb.h"

#include "codec/codec.h"

#include "muduo/base/noncopyable.h"

namespace gateway
{
typedef std::shared_ptr<LoginRequest> LoginRequestPtr;

class MsgReceiver : muduo::noncopyable
{
public:
    MsgReceiver(ProtobufCodec& codec)
        :codec_(codec)
    {}

    void onAnswer(const muduo::net::TcpConnectionPtr& conn,
        const LoginRequestPtr& message,
        muduo::Timestamp);
private:
    ProtobufCodec& codec_;
};
}

#endif // GATEWAY_SERVER_SRC_msg_receiver