#include "battle_node.h"
#include <future>
///<<< BEGIN WRITING YOUR CODE
#include "infra/agones/agones_gameserver_lifecycle.h"
#include "logic/battle_room_manager.h"
#include "muduo/base/Logging.h"

namespace
{

// CreateBattle 节点级准入拒绝(建房准入闸未开 / 已关,或拿不到 Agones 分配许可)时的 gRPC status message。
// 跨语言字符串契约(集群外入口 D82):match 按 codes.Unavailable + 此消息精确匹配,
// 跳过 DestroyBattle、换一个没试过的节点重试一次(go/match/internal/logic/gather.go
// 的 battleNotAllocatableMessage)。两边必须同改。
constexpr char kBattleNotAllocatableMessage[] = "battle_not_allocatable";

grpc::Status NotAllocatable()
{
    return grpc::Status(grpc::StatusCode::UNAVAILABLE, kBattleNotAllocatableMessage);
}

} // namespace

bool BattleNodeImpl::OpenAdmission()
{
    const bool opened = admission_.Open();
    LOG_INFO << "battle room admission gate: " << (opened ? "opened" : "not opened(shutdown started or repeated call)")
             << " phase=" << battle_admission::ToString(admission_.Phase());
    return opened;
}

void BattleNodeImpl::CloseAdmission()
{
    admission_.Close();
    LOG_INFO << "battle room admission gate closed: subsequent CreateBattle calls return UNAVAILABLE " << kBattleNotAllocatableMessage;
}
///<<< END WRITING YOUR CODE

BattleNodeImpl::BattleNodeImpl(muduo::net::EventLoop& loop)
    : loop_(loop)
{
}

void BattleNodeImpl::HandleCreateBattle(const ::CreateBattleRequest* request,
    ::CreateBattleResponse* response)
{
    ///<<< BEGIN WRITING YOUR CODE
    // 业务逻辑全部在手写类 BattleRoomManager(cpp/nodes/battle/logic/),此处只做委托
    BattleRoomManager::Instance().HandleCreateBattle(*request, *response);
    ///<<< END WRITING YOUR CODE
}

void BattleNodeImpl::HandleDestroyBattle(const ::DestroyBattleRequest* request)
{
    ///<<< BEGIN WRITING YOUR CODE
    // 幂等销毁,不结算(补偿/回滚路径),委托手写房间管理器
    BattleRoomManager::Instance().HandleDestroyBattle(*request);
    ///<<< END WRITING YOUR CODE
}

void BattleNodeImpl::HandleAddObserver(const ::AddObserverRequest* request,
    ::AddObserverResponse* response)
{
    ///<<< BEGIN WRITING YOUR CODE
    // 观战接入(设计文档 §10.5),错误经 response tip 返回,status 恒 OK
    BattleRoomManager::Instance().HandleAddObserver(*request, *response);
    ///<<< END WRITING YOUR CODE
}

void BattleNodeImpl::HandleRemoveObserver(const ::RemoveObserverRequest* request)
{
    ///<<< BEGIN WRITING YOUR CODE
    // 幂等清退(match 互斥路径),委托手写房间管理器
    BattleRoomManager::Instance().HandleRemoveObserver(*request);
    ///<<< END WRITING YOUR CODE
}

void BattleNodeImpl::HandleIssueBattleTicket(const ::IssueBattleTicketRequest* request,
    ::IssueBattleTicketResponse* response)
{
    ///<<< BEGIN WRITING YOUR CODE
    // 丢票补签(设计文档 §18 D25):名单核对 + 自签在手写房间管理器,错误经 response tip 返回,status 恒 OK
    BattleRoomManager::Instance().HandleIssueBattleTicket(*request, *response);
    ///<<< END WRITING YOUR CODE
}

grpc::Status BattleNodeImpl::CreateBattle(grpc::ServerContext* /*context*/,
    const ::CreateBattleRequest* request,
    ::CreateBattleResponse* response)
{
    ///<<< BEGIN WRITING YOUR CODE
    // 节点级准入分三道,全部在任何副作用之前;任一道不过即回 UNAVAILABLE battle_not_allocatable,
    // match 据此跳过 DestroyBattle、换节点重试一次。
    // 顺序:准入闸(本线程)→ 许可(本线程)→ 准入闸复核(loop 内)→ 预签票据(loop 内,BuildAssignment)
    // → 插表(EmplaceRoom)。
    // 注意:gRPC 线程上不能碰 NodeInfo / ClientFacing —— GetNodeInfo() 读 thread_local,这里是空的。

    // 1) 建房准入闸(battle_admission_gate.h):启动完成之前(lifecycle 还没启动,Disabled 默认值会
    //    不经 allocate 放行许可)与停机开始之后都不接新房间。闸没开就连许可都不取。
    if (const auto phase = admission_.Phase(); phase != battle_admission::AdmissionPhase::kOpen)
    {
        LOG_WARN << "CreateBattle rejected: this node is not accepting new rooms, battle_id=" << request->battle_id()
                 << " admission=" << battle_admission::ToString(phase)
                 << ",returning UNAVAILABLE " << kBattleNotAllocatableMessage;
        return NotAllocatable();
    }

    // 2) 分配许可(集群外入口 D82):必须在 gRPC 线程上、runInLoop 之前取 ——
    //    AcquireAllocationPermitBlocking 会有界阻塞调用线程等 Agones allocate(上限
    //    LifecycleOptions.allocateWaitTimeout),绝不能进 EventLoop。
    //    拿不到(排空标签在、allocate 未确认、lifecycle 已停 / 仍在 Starting)即拒绝:
    //    此刻还没投递 loop,房间、定时器、推送、Kafka 事件一概没有发生。
    //    非 Agones 环境 lifecycle 为 Disabled,许可恒放行(启动 / 停机窗口由第 1、3 道兜住)。
    auto &lifecycle = agones::GameServerLifecycle::Instance();
    auto permit = lifecycle.AcquireAllocationPermitBlocking();
    if (!permit)
    {
        LOG_WARN << "CreateBattle rejected: this instance is not allocatable, battle_id=" << request->battle_id()
                 << " agones_state=" << agones::ToString(lifecycle.State())
                 << " draining=" << (lifecycle.IsDraining() ? 1 : 0)
                 << ",returning UNAVAILABLE " << kBattleNotAllocatableMessage;
        return NotAllocatable();
    }
    // permit 活到本函数返回(覆盖 future.get()):在途创建计入 lifecycle,
    // 防止"最后一个旧房间刚移除"时 worker 抢先回 Ready;析构时若零房间(拒绝 / 幂等命中)自动收口回 Ready。

    bool closedInLoop = false;

    // 捕获的全是本栈帧与本对象的引用:本线程阻塞在 future.get() 直到任务跑完,寿命覆盖任务
    // (与其余 RPC 的 &promise 同理);任务若因 loop 退出而永不执行,也就不会解引用。
    auto &admission = admission_;
    ///<<< END WRITING YOUR CODE
    std::promise<grpc::Status> promise;
    auto future = promise.get_future();

    // future.get() 覆盖整个任务寿命；自定义段需要的成员引用应在投递前绑定，避免捕获 this。
    loop_.runInLoop([&]
                    {
        grpc::Status dispatchStatus = grpc::Status::OK;
        ///<<< BEGIN WRITING YOUR CODE: grpc.CreateBattle.in_loop
        // 3) loop 内复核准入闸:第 1 道之后、任务排队期间,SetBeforeShutdown 可能已经关闸并作废全部
        //    房间(关闸与 AbortAllRooms 在同一个 loop 任务里,排在它后面的任务必然看到 kClosed)。
        //    此刻还没进 HandleCreateBattle,没有任何副作用;放行的话房间会在进程退出时无声丢失。
        if (admission.Phase() != battle_admission::AdmissionPhase::kOpen)
        {
            closedInLoop = true;
            dispatchStatus = NotAllocatable();
        }
        ///<<< END WRITING YOUR CODE
        if (dispatchStatus.ok())
        {
            HandleCreateBattle(request, response);
        }
        promise.set_value(dispatchStatus); });

    auto status = future.get();
    ///<<< BEGIN WRITING YOUR CODE: grpc.CreateBattle.post_dispatch
    if (closedInLoop)
    {
        LOG_WARN << "CreateBattle rejected: shutdown started during dispatch(rechecked in loop), battle_id=" << request->battle_id()
                 << ",returning UNAVAILABLE " << kBattleNotAllocatableMessage;
    }
    ///<<< END WRITING YOUR CODE
    return status;
}

grpc::Status BattleNodeImpl::DestroyBattle(grpc::ServerContext* /*context*/,
    const ::DestroyBattleRequest* request,
    ::Empty* /*response*/)
{
    ///<<< BEGIN WRITING YOUR CODE
    ///<<< END WRITING YOUR CODE
    std::promise<grpc::Status> promise;
    auto future = promise.get_future();

    // future.get() 覆盖整个任务寿命；自定义段需要的成员引用应在投递前绑定，避免捕获 this。
    loop_.runInLoop([&]
                    {
        grpc::Status dispatchStatus = grpc::Status::OK;
        ///<<< BEGIN WRITING YOUR CODE: grpc.DestroyBattle.in_loop
        ///<<< END WRITING YOUR CODE
        if (dispatchStatus.ok())
        {
            HandleDestroyBattle(request);
        }
        promise.set_value(dispatchStatus); });

    auto status = future.get();
    ///<<< BEGIN WRITING YOUR CODE: grpc.DestroyBattle.post_dispatch
    ///<<< END WRITING YOUR CODE
    return status;
}

grpc::Status BattleNodeImpl::AddObserver(grpc::ServerContext* /*context*/,
    const ::AddObserverRequest* request,
    ::AddObserverResponse* response)
{
    ///<<< BEGIN WRITING YOUR CODE
    ///<<< END WRITING YOUR CODE
    std::promise<grpc::Status> promise;
    auto future = promise.get_future();

    // future.get() 覆盖整个任务寿命；自定义段需要的成员引用应在投递前绑定，避免捕获 this。
    loop_.runInLoop([&]
                    {
        grpc::Status dispatchStatus = grpc::Status::OK;
        ///<<< BEGIN WRITING YOUR CODE: grpc.AddObserver.in_loop
        ///<<< END WRITING YOUR CODE
        if (dispatchStatus.ok())
        {
            HandleAddObserver(request, response);
        }
        promise.set_value(dispatchStatus); });

    auto status = future.get();
    ///<<< BEGIN WRITING YOUR CODE: grpc.AddObserver.post_dispatch
    ///<<< END WRITING YOUR CODE
    return status;
}

grpc::Status BattleNodeImpl::RemoveObserver(grpc::ServerContext* /*context*/,
    const ::RemoveObserverRequest* request,
    ::Empty* /*response*/)
{
    ///<<< BEGIN WRITING YOUR CODE
    ///<<< END WRITING YOUR CODE
    std::promise<grpc::Status> promise;
    auto future = promise.get_future();

    // future.get() 覆盖整个任务寿命；自定义段需要的成员引用应在投递前绑定，避免捕获 this。
    loop_.runInLoop([&]
                    {
        grpc::Status dispatchStatus = grpc::Status::OK;
        ///<<< BEGIN WRITING YOUR CODE: grpc.RemoveObserver.in_loop
        ///<<< END WRITING YOUR CODE
        if (dispatchStatus.ok())
        {
            HandleRemoveObserver(request);
        }
        promise.set_value(dispatchStatus); });

    auto status = future.get();
    ///<<< BEGIN WRITING YOUR CODE: grpc.RemoveObserver.post_dispatch
    ///<<< END WRITING YOUR CODE
    return status;
}

grpc::Status BattleNodeImpl::IssueBattleTicket(grpc::ServerContext* /*context*/,
    const ::IssueBattleTicketRequest* request,
    ::IssueBattleTicketResponse* response)
{
    ///<<< BEGIN WRITING YOUR CODE
    ///<<< END WRITING YOUR CODE
    std::promise<grpc::Status> promise;
    auto future = promise.get_future();

    // future.get() 覆盖整个任务寿命；自定义段需要的成员引用应在投递前绑定，避免捕获 this。
    loop_.runInLoop([&]
                    {
        grpc::Status dispatchStatus = grpc::Status::OK;
        ///<<< BEGIN WRITING YOUR CODE: grpc.IssueBattleTicket.in_loop
        ///<<< END WRITING YOUR CODE
        if (dispatchStatus.ok())
        {
            HandleIssueBattleTicket(request, response);
        }
        promise.set_value(dispatchStatus); });

    auto status = future.get();
    ///<<< BEGIN WRITING YOUR CODE: grpc.IssueBattleTicket.post_dispatch
    ///<<< END WRITING YOUR CODE
    return status;
}
