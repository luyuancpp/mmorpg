
#include "player_state_attribute_sync_replied_handler.h"

#include "service_info//player_state_attribute_sync_service_info.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE



void InitEntitySyncServiceSyncBaseAttributeRepliedHandler()
{
    gResponseDispatcher.registerMessageCallback<::Empty>(EntitySyncServiceSyncBaseAttributeMessageId,
        std::bind(&OnEntitySyncServiceSyncBaseAttributeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(EntitySyncServiceSyncAttribute2FramesMessageId,
        std::bind(&OnEntitySyncServiceSyncAttribute2FramesRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(EntitySyncServiceSyncAttribute5FramesMessageId,
        std::bind(&OnEntitySyncServiceSyncAttribute5FramesRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(EntitySyncServiceSyncAttribute10FramesMessageId,
        std::bind(&OnEntitySyncServiceSyncAttribute10FramesRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(EntitySyncServiceSyncAttribute30FramesMessageId,
        std::bind(&OnEntitySyncServiceSyncAttribute30FramesRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(EntitySyncServiceSyncAttribute60FramesMessageId,
        std::bind(&OnEntitySyncServiceSyncAttribute60FramesRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnEntitySyncServiceSyncBaseAttributeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnEntitySyncServiceSyncAttribute2FramesRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnEntitySyncServiceSyncAttribute5FramesRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnEntitySyncServiceSyncAttribute10FramesRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnEntitySyncServiceSyncAttribute30FramesRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnEntitySyncServiceSyncAttribute60FramesRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}
