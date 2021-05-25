#ifndef SRC_SERVER_RPCCLIENT_LOGIN_CLIENT_H_
#define SRC_SERVER_RPCCLIENT_LOGIN_CLIENT_H_

#include <memory>

#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/protorpc/RpcChannel.h"

using namespace muduo;
using namespace muduo::net;

namespace gateway
{

template<typename StubClass>
class RpcClient : noncopyable
{
public:

    using StubPtr = std::shared_ptr<StubClass>;

    RpcClient(EventLoop* loop,
        const InetAddress& serverAddr)
        : client_(loop, serverAddr, "RpcClient"),
          channel_(new RpcChannel)
    {
        client_.setConnectionCallback(
            std::bind(&RpcClient::onConnection, this, _1));
        client_.setMessageCallback(
            std::bind(&RpcChannel::onMessage, get_pointer(channel_), _1, _2, _3));
        client_.enableRetry();
    }

    void connect()
    {
        client_.connect();
    }

    template<typename Request, typename Response, typename Class>
    void SendRequest(Request& request, 
                     Class* object, 
                     void (Class::* method)(Response*), 
                     void (StubClass::* stub_method)(::google::protobuf::RpcController*, const Request*, Response*, ::google::protobuf::Closure*))
    {
        if (nullptr == stub_)
        {
            return;
        }
        Response * presponse = new Response;
        ((*stub_).*stub_method)(nullptr, &request, presponse, NewCallback(object, method, presponse));
    }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            conn->setTcpNoDelay(true);
            channel_->setConnection(conn);
            stub_ = std::make_shared<StubClass>(get_pointer(channel_));
        }
        else
        {
            stub_.reset();
        }
    }

    TcpClient client_;
    RpcChannelPtr channel_;
    StubPtr stub_;
};
} // namespace gateway

#endif // SRC_SERVER_RPCCLIENT_LOGIN_CLIENT_H_
