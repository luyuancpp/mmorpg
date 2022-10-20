#pragma once

#include <google/protobuf/empty.pb.h>

#include "rpc_connection_event.h"
#include "rpc_client.h"
#include "src/event/event.h"
#include "src/network/rpc_channel.h"

#include "muduo/base/Logging.h"

#include "common.pb.h"


template<typename StubClass>
class RpcStub :  public Receiver<RpcStub<StubClass>>
{
public:
    using StubPtr = std::unique_ptr<StubClass>;

    RpcStub() {}
    RpcStub(muduo::net::RpcChannelPtr& channel)
        :stub_(std::make_unique<StubClass>(get_pointer(channel))){}

	StubClass* stub() {return stub_.get();}
    
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
            &method_param->s_rq_,
            method_param->s_rp_,
            NewCallback(object, method, method_param));
    }

    template<typename MethodParam, typename Class, typename StubMethod>
    void CallMethodByObj(void (Class::* method)(MethodParam),
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
            &method_param.s_rq_,
            method_param.s_rp_,
            NewCallback(object, method, method_param));
    }

    template<typename MethodParam, typename StubMethod>
    void CallMethod(void (method)(MethodParam),
        MethodParam& method_param,
        StubMethod stub_method)
    {
        if (nullptr == stub_)
        {
            LOG_ERROR << "Server Disconnected";
            return;
        }
        ((*stub_).*stub_method)(nullptr,
            &method_param->s_rq_,
            method_param->s_rp_,
            NewCallback(method, method_param));
    }

	template<typename MethodParam, typename Class, typename StubMethod>
	void CallMethodByRowStub(void (Class::* method)(MethodParam),
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
			&method_param.s_rq_,
			method_param.s_rp_,
			NewCallback(object, method, method_param));
	}

    template<typename Class, typename MethodParam, typename StubMethod>
    void CallMethodString(
        void (Class::* method)(MethodParam),
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
            &method_param->s_rq_,
            method_param->s_rp_,
            NewCallback(object, method, method_param));
    }

    // no responese
    template<typename Request, typename StubMethod>
    void CallMethod( const Request& request, StubMethod stub_method)
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

    void receive(const RegisterStubEvent& es)
    {
        if (es.conn_->connected())
        {
            stub_ = std::make_unique<StubClass>(get_pointer(es.channel_));
        }
    }

private:  
    StubPtr stub_;
};

