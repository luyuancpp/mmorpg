#pragma once

#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "engine/core/type_define/type_define.h"
#include "proto/common/base/role_admission.pb.h"
#include "proto/data_service/data_service.pb.h"

struct PlayerEnterContext;
class SealAccountRoleTradeRequest;
struct RoleAdmissionState;

// scene逻辑线程唯一所有者。SQL登记成功才进入原加载链；封存永远按原实例/修订/世代。
// 普通销毁只关闭本地持有并保留SQL下限，交易ACK还必须等严格双表ACK。
class RoleAdmissionSystem {
public:
    using Clock = std::chrono::steady_clock;
    struct LocalNode { std::string instanceId, nodeId; uint32_t zoneId{0}; };
    struct SqlAck { std::string ownerEpoch, pending, playerDatabase, playerDatabase1; };
    using RegisterReply = std::function<void(bool, const data_service::RegisterRoleAdmissionResponse*)>;
    using FenceReply = std::function<void(bool, std::optional<std::string>)>;
    using CloseReply = std::function<void(bool, const data_service::CloseRoleAdmissionsResponse*)>;
    // 依赖只在当前逻辑线程使用；true表示已接管回调，false不得再回调。可同步完成。
    // 未替换的依赖沿正式HMAC/RPC/Redis适配器执行，不存在默认允许的授权分支。
    struct Dependencies {
        std::function<Clock::time_point()> now;
        std::function<LocalNode()> localNode;
        std::function<bool(data_service::RegisterRoleAdmissionRequest, RegisterReply)> registerAdmission;
        std::function<bool(data_service::CloseRoleAdmissionsRequest, CloseReply)> closeAdmissions;
        std::function<bool(data_service::AckRoleTradeDrainRequest, std::function<void(bool)>)> ackDrain;
        std::function<bool(Guid, std::function<void(std::optional<SqlAck>)>)> readSqlAck;
        // expectedPrior 首次读取后固定；重试保留同一 candidate 与 prior，不从当前身份补资格。
        std::function<bool(Guid, std::string, std::optional<std::string>, FenceReply)> registerHandoffFence;
        std::function<void(Guid, PlayerEnterContext)> activate;
        std::function<void(Guid)> freeze;
        std::function<void(Guid, uint32_t)> reject;
        std::function<bool(Guid)> hasPendingLoad;
        // 直接测生命周期的宿主显式提供 SQL 已确认原件；默认空，不能覆盖授权判定。
        // 状态机回归不填此项，必须经过 RequestEnter 与登记回包。
        std::vector<common::base::RoleAdmissionIdentity> confirmedAdmissions;
    };
    class ScopedDependencies {
    public:
        explicit ScopedDependencies(Dependencies dependencies);
        ~ScopedDependencies();
        ScopedDependencies(const ScopedDependencies&) = delete;
        ScopedDependencies& operator=(const ScopedDependencies&) = delete;
    private:
        std::shared_ptr<RoleAdmissionState> previous_;
    };
    static void RequestEnter(Guid playerId, PlayerEnterContext context);
    static bool Allows(Guid playerId, uint64_t generation, uint64_t ownerRevision);
    static bool IsSealed(Guid playerId);
    static std::optional<common::base::RoleAdmissionIdentity> ConfirmedIdentity(Guid playerId);
    // 只确认调用方原入场上下文；不能用当前 identity 给旧异步调用补新世代或新会话。
    static std::optional<common::base::RoleAdmissionIdentity> ConfirmedIdentityForContext(
        Guid playerId, const PlayerEnterContext& context);
    static bool Seal(const common::base::RoleTradeSeal& seal);
    static bool AcceptSeal(const SealAccountRoleTradeRequest& request);
    static void ObserveSave(Guid playerId, uint64_t ownerEpoch, uint64_t saveSeq);
    // 只在严格 SaveWithCommitBarrier 已接收这份完整新快照后调用；旧加载/旧SQL不能补此证据。
    static void ObserveTradeSaveQueued(Guid playerId, uint64_t ownerEpoch, uint64_t saveSeq);
    static void OnDestroyed(Guid playerId);
    static void BeginCallback(Guid playerId);
    static void EndCallback(Guid playerId);
    static void Tick();
};

// 非持久运行时授权，只由成功入场写入；scene自发换图沿用原修订，不能反查买家修订。
struct PlayerRoleAdmissionComp {
    uint64_t generation{0};
    uint64_t ownerRevision{0};
};
