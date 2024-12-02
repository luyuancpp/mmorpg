#include "db_service_replied_handler.h"
#include "network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
extern ProtobufDispatcher gResponseDispatcher;


void InitAccountDBServiceLoad2RedisRepliedHandler()
{
	gResponseDispatcher.registerMessageCallback<LoadAccountResponse>(std::bind(&OnAccountDBServiceLoad2RedisRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	gResponseDispatcher.registerMessageCallback<SaveAccountResponse>(std::bind(&OnAccountDBServiceSave2RedisRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnAccountDBServiceLoad2RedisRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LoadAccountResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnAccountDBServiceSave2RedisRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<SaveAccountResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

