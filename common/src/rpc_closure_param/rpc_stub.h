#ifndef SRC_SERVER_RPCCLIENT_RPC_STUB_H_
#define SRC_SERVER_RPCCLIENT_RPC_STUB_H_

#include "src/game_rpc/game_rpc_channel.h"

namespace common
{
template<typename StubClass>
class RpcStub : noncopyable
{
public:
    using StubPtr = std::unique_ptr<StubClass>;

    RpcClient(TcpClient& client,
              RpcChannelPtr& channel)
        :  channel_(channel)
    {

    }

    template<typename Request, typename Response, typename StubMethod>
    void CallMethod(const Request& request,
        void (method)(Response*),
        StubMethod stub_method)
    {
        if (nullptr == stub_)
        {
            return;
        }
        Response* presponse = new Response;
        ((*stub_).*stub_method)(nullptr, &request, presponse, NewCallback(method, presponse));
    }

    template<typename MethodParam, typename Class, typename StubMethod>
    void CallMethod(void (Class::* method)(MethodParam),
        MethodParam& method_param,
        Class* object,
        StubMethod stub_method)
    {
        if (nullptr == stub_)
        {
            return;
        }
        ((*stub_).*stub_method)(nullptr,
            &method_param->s_rqst_,
            method_param->s_resp_,
            NewCallback(object, method, method_param));
    }

    template<typename Class, typename MethodParam, typename StubMethod>
    void CallMethodString(Class* object,
        void (Class::* method)(MethodParam),
        MethodParam& method_param,
        StubMethod stub_method)
    {
        if (nullptr == stub_) 
        { 
            return; 
        }
        ((*stub_).*stub_method)(nullptr,
            &method_param->s_reqst_,
            method_param->s_resp_,
            NewCallback(object, method, method_param));
    }

    template<typename MethodParam, typename StubMethod>
    void CallMethodString(
        void (method)(MethodParam),
        MethodParam& method_param,
        StubMethod stub_method)
    {
        if (nullptr == stub_) 
        { 
            return; 
        }
        ((*stub_).*stub_method)(nullptr,
            &method_param->s_reqst_,
            method_param->s_resp_,
            NewCallback(method, method_param));
    }
private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            stub_ = std::make_unique<StubClass>(get_pointer(channel_));
        }
        else
        {
            stub_.reset();
        }
    }

    RpcChannelPtr& channel_;
    StubPtr stub_;
};

}//namespace common

#endif//SRC_SERVER_RPCCLIENT_RPC_STUB_H_
