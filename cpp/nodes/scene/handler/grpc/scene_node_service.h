#pragma once

#include <grpcpp/grpcpp.h>
#include <muduo/net/EventLoop.h>
#include "proto/scene_manager/scene_node_service.grpc.pb.h"

///<<< BEGIN WRITING YOUR CODE: grpc.header.includes
///<<< END WRITING YOUR CODE

// 由 proto 工具生成 SceneNodeGrpc 的 gRPC 骨架；自定义代码只写在守护段内。
// gRPC 线程通过 runInLoop + promise/future 等待事件循环完成请求。
// Handle* 在事件循环执行，不做阻塞 I/O；业务错误经响应字段返回。
// 投递前段在 gRPC 线程运行，可取得准入许可并提前返回非 OK。
// loop 内段在 Handle* 前复核状态：将 dispatchStatus 设为非 OK 即跳过业务委托。
// 完成后段在 gRPC 线程运行；许可与捕获的栈引用存活到 future.get() 完成。
class SceneNodeGrpcImpl final : public scene_node::SceneNodeGrpc::Service
{
public:
    explicit SceneNodeGrpcImpl(muduo::net::EventLoop& loop);
    ///<<< BEGIN WRITING YOUR CODE: grpc.header.public
    ///<<< END WRITING YOUR CODE

    grpc::Status CreateScene(grpc::ServerContext* context,
        const ::CreateSceneRequest* request,
        ::CreateSceneResponse* response) override;

    grpc::Status DestroyScene(grpc::ServerContext* context,
        const ::DestroySceneRequest* request,
        ::Empty* response) override;

    grpc::Status ReleasePlayer(grpc::ServerContext* context,
        const ::scene_node::ReleasePlayerRequest* request,
        ::Empty* response) override;

    grpc::Status PrepareBattle(grpc::ServerContext* context,
        const ::PrepareBattleRequest* request,
        ::PrepareBattleResponse* response) override;

    grpc::Status CancelBattlePrepare(grpc::ServerContext* context,
        const ::CancelBattlePrepareRequest* request,
        ::Empty* response) override;

    grpc::Status AssetDebit(grpc::ServerContext* context,
        const ::AssetOpRequest* request,
        ::AssetOpResponse* response) override;

    grpc::Status AssetAbortDebit(grpc::ServerContext* context,
        const ::AssetOpRequest* request,
        ::AssetOpResponse* response) override;

    grpc::Status AssetCredit(grpc::ServerContext* context,
        const ::AssetOpRequest* request,
        ::AssetOpResponse* response) override;

private:
    // 业务委托在事件循环线程执行，gRPC 线程同步等待完成。
    static void HandleCreateScene(const ::CreateSceneRequest* request, ::CreateSceneResponse* response);
    static void HandleDestroyScene(const ::DestroySceneRequest* request);
    static void HandleReleasePlayer(const ::scene_node::ReleasePlayerRequest* request);
    static void HandlePrepareBattle(const ::PrepareBattleRequest* request, ::PrepareBattleResponse* response);
    static void HandleCancelBattlePrepare(const ::CancelBattlePrepareRequest* request);
    static void HandleAssetDebit(const ::AssetOpRequest* request, ::AssetOpResponse* response);
    static void HandleAssetAbortDebit(const ::AssetOpRequest* request, ::AssetOpResponse* response);
    static void HandleAssetCredit(const ::AssetOpRequest* request, ::AssetOpResponse* response);

    muduo::net::EventLoop& loop_;
    ///<<< BEGIN WRITING YOUR CODE: grpc.header.private
    ///<<< END WRITING YOUR CODE
};
