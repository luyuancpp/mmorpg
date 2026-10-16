#pragma once

#include <grpcpp/grpcpp.h>
#include <muduo/net/EventLoop.h>
#include "proto/battle/battle_node.grpc.pb.h"

///<<< BEGIN WRITING YOUR CODE: grpc.header.includes
#include "battle_admission_gate.h"
///<<< END WRITING YOUR CODE

// 由 proto 工具生成 BattleNode 的 gRPC 骨架；自定义代码只写在守护段内。
// gRPC 线程通过 runInLoop + promise/future 等待事件循环完成请求。
// Handle* 在事件循环执行，不做阻塞 I/O；业务错误经响应字段返回。
// 投递前段在 gRPC 线程运行，可取得准入许可并提前返回非 OK。
// loop 内段在 Handle* 前复核状态：将 dispatchStatus 设为非 OK 即跳过业务委托。
// 完成后段在 gRPC 线程运行；许可与捕获的栈引用存活到 future.get() 完成。
class BattleNodeImpl final : public BattleNode::Service
{
public:
    explicit BattleNodeImpl(muduo::net::EventLoop& loop);
    ///<<< BEGIN WRITING YOUR CODE: grpc.header.public
    // ---- 建房准入闸(battle_admission_gate.h;只管 CreateBattle,其余 RPC 只作用于已有房间) ----
    // 两者都只在 loop 线程调用:loop 内复核与 Close 同在 loop 上,才能保证排在关闸之后的建房必被拒。

    // SetAfterStart 里 Agones lifecycle 启动之后调用。返回 false = 没有打开:停机已先开始
    // (闸已关,不会再开),或重复调用(已经是打开的)。
    bool OpenAdmission();

    // SetBeforeShutdown 第一句调用(先于 AbortAllRooms)。终态,幂等。
    void CloseAdmission();
    ///<<< END WRITING YOUR CODE

    grpc::Status CreateBattle(grpc::ServerContext* context,
        const ::CreateBattleRequest* request,
        ::CreateBattleResponse* response) override;

    grpc::Status DestroyBattle(grpc::ServerContext* context,
        const ::DestroyBattleRequest* request,
        ::Empty* response) override;

    grpc::Status AddObserver(grpc::ServerContext* context,
        const ::AddObserverRequest* request,
        ::AddObserverResponse* response) override;

    grpc::Status RemoveObserver(grpc::ServerContext* context,
        const ::RemoveObserverRequest* request,
        ::Empty* response) override;

    grpc::Status IssueBattleTicket(grpc::ServerContext* context,
        const ::IssueBattleTicketRequest* request,
        ::IssueBattleTicketResponse* response) override;

private:
    // 业务委托在事件循环线程执行，gRPC 线程同步等待完成。
    static void HandleCreateBattle(const ::CreateBattleRequest* request, ::CreateBattleResponse* response);
    static void HandleDestroyBattle(const ::DestroyBattleRequest* request);
    static void HandleAddObserver(const ::AddObserverRequest* request, ::AddObserverResponse* response);
    static void HandleRemoveObserver(const ::RemoveObserverRequest* request);
    static void HandleIssueBattleTicket(const ::IssueBattleTicketRequest* request, ::IssueBattleTicketResponse* response);

    muduo::net::EventLoop& loop_;
    ///<<< BEGIN WRITING YOUR CODE: grpc.header.private
    battle_admission::AdmissionGate admission_;
    ///<<< END WRITING YOUR CODE
};
