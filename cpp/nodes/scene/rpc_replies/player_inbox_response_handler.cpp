#include "player_inbox_response_handler.h"

#include "grpc_client/player_inbox/player_inbox_grpc_client.h"
#include "proto/player_inbox/player_inbox.pb.h"
#include "services/scene/player/system/player_inbox_system.h"

// 本文件只做适配:应答里没有请求上下文,PlayerInboxSystem 按回显的 player_id + domain + correlation_id
// 找回实体与在途状态(对不上就丢弃);失败处理器拿到的是发出的请求副本,从中取同样三项。
// 失败 = 结果未知(GrpcCallFailure 的语义),只释放在途状态并退避,不推断对端是否已执行。
// 全部回调都在 scene 的 loop 线程上(muduo_grpc::Channel 的完成回调经 queueInLoop 投递到 loop 线程),不捕获任何对象。
void InitPlayerInboxReply()
{
    player_inbox::AsyncPlayerInboxFetchItemsHandler =
        [](const GrpcReplyContext & /*ctx*/, const ::player_inbox::FetchPlayerInboxResponse &response)
    { PlayerInboxSystem::OnFetchReply(response); };

    player_inbox::AsyncPlayerInboxFetchItemsFailedHandler =
        [](const GrpcCallFailure &failure, const ::player_inbox::FetchPlayerInboxRequest &request)
    {
        // failure.code() 已是 uint32_t,与 grpc::StatusCode 同值(grpc_call_tag.h),OnFetchFailed 的契约不变。
        PlayerInboxSystem::OnFetchFailed(request, failure.code(), failure.message());
    };

    player_inbox::AsyncPlayerInboxAckItemsHandler =
        [](const GrpcReplyContext & /*ctx*/, const ::player_inbox::AckPlayerInboxResponse &response)
    { PlayerInboxSystem::OnAckReply(response); };

    player_inbox::AsyncPlayerInboxAckItemsFailedHandler =
        [](const GrpcCallFailure &failure, const ::player_inbox::AckPlayerInboxRequest &request)
    {
        PlayerInboxSystem::OnAckFailed(request, failure.code(), failure.message());
    };
}
