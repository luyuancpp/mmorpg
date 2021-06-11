#ifndef SRC_SERVER_RPCCLIENT_STUB_RPC_CLIENT_H_
#define SRC_SERVER_RPCCLIENT_STUB_RPC_CLIENT_H_

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

namespace common
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

    template<typename Request, typename Response, typename StubMethod>
    void Send(const Request& request,
        void (method)(Response*),
        StubMethod stub_method)
    {
        if (nullptr == stub_)
        {
            return;
        }
        Response* presponse = new Response;
        ((*stub_).*stub_method)(nullptr, &request, presponse, NewCallback( method, presponse));
    }

    template<typename MethodParam, typename Class, typename StubMethod>
    void Send(void (Class::* method)(MethodParam),
        MethodParam method_param,
        Class* object,
        StubMethod stub_method)
    {
        if (nullptr == stub_)
        {
            return;
        }
        ((*stub_).*stub_method)(nullptr, 
            &method_param->server_request_, 
            method_param->s_resp_, 
            NewCallback(object, method, method_param));
    }

    template<typename Class, typename ClosureArg,  typename StubMethod>
    void SendRpcString(Class* object,
        void (method)(ClosureArg),
        ClosureArg closurearg,
        StubMethod stub_method)
    {
        if (nullptr == stub_){ return; }
        ((*stub_).*stub_method)(nullptr, 
                                &closurearg->server_request_, 
                                closurearg->server_respone_, 
                                NewCallback(object, method, closurearg));
    }

    template<typename ClosureArg, typename StubMethod>
    void SendRpcString(
        void (method)(ClosureArg),
        ClosureArg closurearg,
        StubMethod stub_method)
    {
        if (nullptr == stub_) { return; }
        ((*stub_).*stub_method)(nullptr,
            &closurearg->server_request_,
            closurearg->server_respone_,
            NewCallback(method, closurearg));
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
} // namespace common

#endif // SRC_SERVER_RPCCLIENT_STUB_RPC_CLIENT_H_
