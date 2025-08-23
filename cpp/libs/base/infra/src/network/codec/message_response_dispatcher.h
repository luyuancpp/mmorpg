#pragma once

#include "dispatcher.h"

class MessageResponseDispatcher
{
public:
	typedef std::function<void(const muduo::net::TcpConnectionPtr&,
		const MessagePtr& message,
		muduo::Timestamp)> ProtobufMessageCallback;

	explicit MessageResponseDispatcher(const ProtobufMessageCallback& defaultCb)
		: defaultCallback_(defaultCb)
	{
	}

	void onProtobufMessage(uint32_t messageId, const muduo::net::TcpConnectionPtr& conn,
		const MessagePtr& message,
		muduo::Timestamp receiveTime) const
	{
		CallbackMap::const_iterator it = callbacks_.find(messageId);
		if (it != callbacks_.end())
		{
			it->second->onMessage(conn, message, receiveTime);
		}
		else
		{
			defaultCallback_(conn, message, receiveTime);
		}
	}

	template<typename T>
	void registerMessageCallback(uint32_t messageId, const typename CallbackT<T>::ProtobufMessageTCallback& callback)
	{
		std::shared_ptr<CallbackT<T> > pd(new CallbackT<T>(callback));
		callbacks_[messageId] = pd;
	}

private:
	typedef std::map<uint32_t, std::shared_ptr<Callback> > CallbackMap;

	CallbackMap callbacks_;
	ProtobufMessageCallback defaultCallback_;
};