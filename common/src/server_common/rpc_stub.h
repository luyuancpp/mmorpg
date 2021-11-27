#ifndef SRC_SERVER_RPCCLIENT_RPC_STUB_H_
#define SRC_SERVER_RPCCLIENT_RPC_STUB_H_

#include <google/protobuf/empty.pb.h>

#include "rpc_connection_event.h"
#include "rpc_client.h"
#include "src/event/event.h"
#include "src/server_common/rpc_channel.h"

#include "muduo/base/Logging.h"

#include "common.pb.h"

namespace common
{
template<typename StubClass>
class RpcStub : noncopyable,  public Receiver<RpcStub<StubClass>>
{
public:
    using StubPtr = std::unique_ptr<StubClass>;
    using MyType = std::unique_ptr<RpcStub>;

    template<typename Request, typename Response, typename StubMethod>
    void CallMethod(const Request& request,
        void (method)(Response*),
        StubMethod stub_method)
    {
        if (nullptr == stub_)
        {
            LOG_ERROR << "Server Disconnected";
            return;
        }
        Response* presponse = new Response;
        ((*stub_).*stub_method)(nullptr, 
            &request, 
            presponse, 
            NewCallback(method, presponse));
    }

    template<typename MethodParam, typename Class, typename StubMethod>
    void CallMethod(void (Class::* method)(MethodParam),
        MethodParam& method_param,
        Class* object,
        StubMethod stub_method)
    {
        if (nullptr == stub_)
        {
            LOG_ERROR << "Server Disconnected";
            return;
        }
        ((*stub_).*stub_method)(nullptr,
            &method_param->s_reqst_,
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
            LOG_ERROR << "Server Disconnected";
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
            LOG_ERROR << "Server Disconnected";
            return; 
        }
        ((*stub_).*stub_method)(nullptr,
            &method_param->s_reqst_,
            method_param->s_resp_,
            NewCallback(method, method_param));
    }

    // no responese
    template<typename Request, typename StubMethod>
    void CallMethod(
        const Request& request,
        StubMethod stub_method)
    {
        if (nullptr == stub_)
        {
            LOG_ERROR << "Server Disconnected";
            return;
        }
        google::protobuf::Empty* presponse = new google::protobuf::Empty;
        ((*stub_).*stub_method)(nullptr,
            &request,
            presponse,
            google::protobuf::NewCallback(&DoNothing));
    }

    static void DoNothing() {}

    void receive(const RegisterStubES& es)
    {
        if (es.conn_->connected())
        {
            stub_ = std::make_unique<StubClass>(get_pointer(es.channel_));
        }
        else
        {
            LOG_ERROR << "Server Disconnected";
            stub_.reset();
        }
    }

private:  
    StubPtr stub_;
    EventManagerPtr emp_;
};

}//namespace common

#endif//SRC_SERVER_RPCCLIENT_RPC_STUB_H_
