// 真实Manager接口回归：仅basic Node本机身份和EventLoop，不启动服务/注册/Redis/Kafka。
// 通过既有MutableNodeInfo临时撤下可达地址，确定性复现幂等补签失败；不加生产故障hook。
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "logic/battle_room_manager.h"
#include "muduo/net/EventLoop.h"
#include "node/system/node/node.h"
#include "node_config_manager.h"
#include "proto/common/base/node.pb.h"
#include "proto/match/match_service.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "time/system/time.h"

namespace {
constexpr uint64_t kFirstPlayer = 59001;
constexpr uint64_t kSecondPlayer = 59002;
constexpr uint64_t kObserver = 59003;
constexpr char kInstance[] = "a1094888-4208-482e-b187-a28a98ca2b54";
constexpr char kFirstOperation[] = "aa094888-4208-482e-b187-a28a98ca2b54";
constexpr char kSecondOperation[] = "bb094888-4208-482e-b187-a28a98ca2b54";
constexpr char kPlacement[] = "cc094888-4208-482e-b187-a28a98ca2b54";
std::atomic<uint64_t> nextBattleId{590000};

class ObserverFixtureEnvironment {
public:
    ObserverFixtureEnvironment(const char* name, const char* value) : name_(name) {
        if (const auto* previous = std::getenv(name)) previous_ = previous;
        Put(value);
    }
    ~ObserverFixtureEnvironment() { Put(previous_ ? previous_->c_str() : nullptr); }
private:
    void Put(const char* value) const {
#ifdef _WIN32
        (void)_putenv_s(name_.c_str(), value ? value : "");
#else
        if (value) (void)setenv(name_.c_str(), value, 1);
        else (void)unsetenv(name_.c_str());
#endif
    }
    std::string name_;
    std::optional<std::string> previous_;
};

class BattleObserverContract : public testing::Test {
protected:
    void SetUp() override {
        savedGlobal_ = gNodeConfigManager;
        savedThread_ = tlsNodeConfigManager;
        ASSERT_EQ(nullptr, gNode);
        loop_ = std::make_unique<muduo::net::EventLoop>();
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        logDirectory_ = std::filesystem::temp_directory_path() / ("battle-observer-interface-" + std::to_string(nonce));
        ASSERT_TRUE(std::filesystem::create_directory(logDirectory_));
        RecordProperty("log_directory", logDirectory_.string());
        // 此构造器不执行Initialize，不加载网络客户端或生成的业务注册表。
        node_ = std::make_unique<Node>(loop_.get(), (logDirectory_ / "node").string());
        auto& self = node_->MutableNodeInfo();
        self.set_node_id(87);
        self.set_node_type(common::base::BattleNodeService);
        self.set_node_uuid(kInstance);
        self.mutable_endpoint()->set_ip("127.0.0.1");
        self.mutable_endpoint()->set_port(18087);
        originalNode_ = self;
        gNodeConfigManager.GetBaseDeployConfig().set_battle_token_secret("unit-observer-only-ticket-key-0123456789abcdef");
        gNodeConfigManager.GetBaseDeployConfig().clear_gate_token_secret();
        gNodeConfigManager.GetBaseDeployConfig().mutable_kafka()->clear_brokers();
        tlsNodeConfigManager = gNodeConfigManager;
        // 房间身份只依赖测试序列；日志目录nonce不参与任何业务断言或授权。
        battleId_ = nextBattleId.fetch_add(1, std::memory_order_relaxed);
        // TimeSystem没有可注入时钟。本接口测试不推进/等待时间，只在同一原件中
        // 固定一次真实准入时刻，验证失败补签、幂等与操作隔离；不声称覆盖时钟边界。
        originalNowMs_ = TimeSystem::NowMillisecondsUTC();
        manager().SetRoomLifecycleHooks({});
        manager().SetClientEdge(nullptr);
    }

    void TearDown() override {
        if (node_) {
            DestroyBattleRequest request;
            request.set_battle_id(battleId_);
            request.set_placement_operation_id(kPlacement);
            request.set_target_instance_id(kInstance);
            manager().HandleDestroyBattle(request);
            manager().SetClientEdge(nullptr);
            node_.reset(); // 既有Node两跳排空完成后才销毁EventLoop。
        }
        loop_.reset();
        gNodeConfigManager = savedGlobal_;
        tlsNodeConfigManager = savedThread_;
    }

    static BattleRoomManager& manager() { return BattleRoomManager::Instance(); }

    void Create() {
        CreateBattleRequest request;
        request.set_battle_id(battleId_);
        request.set_match_mode(::match::MATCH_MODE_1V1);
        request.set_seed(1);
        request.set_created_at_ms(originalNowMs_);
        request.set_deadline_ms(originalNowMs_ + 180000);
        request.set_placement_operation_id(kPlacement);
        request.set_target_instance_id(kInstance);
        request.set_required_physical_region(1);
        request.set_admit_before_ms(originalNowMs_ + 30000);
        for (uint32_t side = 0; side < 2; ++side) {
            auto* player = request.add_players();
            player->set_player_id(side == 0 ? kFirstPlayer : kSecondPlayer);
            player->set_player_name("observer-interface-participant");
            player->set_team_index(side);
            player->set_level(1);
            player->set_max_health(1000);
            player->set_max_mana(100);
            player->mutable_base_attributes()->set_health(1000);
            player->mutable_base_attributes()->set_mana(100);
            player->mutable_base_attributes()->set_speed(12);
            auto* route = player->mutable_routing();
            route->set_session_id(side + 1);
            route->set_gate_node_id(1);
            route->set_gate_instance_id("interface-no-live-gate");
            route->set_scene_node_id(1);
            route->set_scene_instance_id("interface-no-live-scene");
            route->set_zone_id(1);
        }
        CreateBattleResponse reply;
        manager().HandleCreateBattle(request, reply);
        ASSERT_EQ(0u, reply.error_message().id());
        QueryBattleCreationRequest query;
        query.set_battle_id(battleId_);
        query.set_placement_operation_id(kPlacement);
        query.set_target_instance_id(kInstance);
        QueryBattleCreationResponse state;
        manager().HandleQueryBattleCreation(query, state);
        ASSERT_EQ(BATTLE_CREATION_PRESENT, state.state());
    }

    AddObserverRequest Original(const std::string& operation) const {
        AddObserverRequest request;
        request.set_battle_id(battleId_);
        request.set_observer_player_id(kObserver);
        request.set_observer_name("公开观众名");
        request.set_spectator_operation_id(operation);
        request.set_target_instance_id(kInstance);
        request.set_admit_before_ms(originalNowMs_ + 30000);
        request.mutable_routing()->set_session_id(3);
        request.mutable_routing()->set_gate_node_id(1);
        request.mutable_routing()->set_gate_instance_id("interface-no-live-gate");
        request.mutable_routing()->set_zone_id(1);
        return request;
    }

    ObserverAdmissionState Query(const AddObserverRequest& original) {
        QueryObserverRequest query;
        query.set_battle_id(original.battle_id());
        query.set_observer_player_id(original.observer_player_id());
        query.set_spectator_operation_id(original.spectator_operation_id());
        query.set_target_instance_id(original.target_instance_id());
        QueryObserverResponse reply;
        manager().HandleQueryObserver(query, reply);
        return reply.state();
    }

    RemoveObserverResponse Remove(const AddObserverRequest& original) {
        RemoveObserverRequest request;
        request.set_battle_id(original.battle_id());
        request.set_observer_player_id(original.observer_player_id());
        request.set_spectator_operation_id(original.spectator_operation_id());
        request.set_target_instance_id(original.target_instance_id());
        request.set_admit_before_ms(original.admit_before_ms());
        RemoveObserverResponse reply;
        manager().HandleRemoveObserver(request, reply);
        return reply;
    }

    void AssertTicket(const AddObserverRequest& original) {
        IssueBattleTicketRequest request;
        request.set_battle_id(battleId_);
        request.set_player_id(kObserver);
        request.set_target_instance_id(kInstance);
        request.set_spectator_operation_id(original.spectator_operation_id());
        IssueBattleTicketResponse reply;
        manager().HandleIssueBattleTicket(request, reply);
        ASSERT_EQ(0u, reply.error_message().id());
        BattleTicketPayload payload;
        ASSERT_TRUE(payload.ParseFromString(reply.assignment().token_payload()));
        EXPECT_EQ(original.spectator_operation_id(), payload.spectator_operation_id());
        EXPECT_EQ(kInstance, payload.battle_instance_id());
        EXPECT_FALSE(reply.assignment().token_signature().empty());
    }

    ObserverFixtureEnvironment global_{"GLOBAL_MATCH_ENABLED", "true"};
    ObserverFixtureEnvironment region_{"PHYSICAL_REGION", "1"};
    NodeConfigManager savedGlobal_;
    NodeConfigManager savedThread_;
    std::unique_ptr<muduo::net::EventLoop> loop_;
    std::unique_ptr<Node> node_;
    NodeInfo originalNode_;
    std::filesystem::path logDirectory_;
    uint64_t battleId_{0};
    uint64_t originalNowMs_{0};
};

TEST_F(BattleObserverContract, AssignmentFailureRemovesOriginalOperationAndNewOperationRecovers) {
    ASSERT_NO_FATAL_FAILURE(Create());
    const auto first = Original(kFirstOperation);
    AddObserverResponse added;
    manager().HandleAddObserver(first, added);
    ASSERT_EQ(0u, added.error_message().id());
    ASSERT_EQ(OBSERVER_ADMISSION_ACTIVE, Query(first));

    // 保持真实房间/实例/原Add不变，只让既有签票地址判定失败。
    node_->MutableNodeInfo().clear_endpoint();
    node_->MutableNodeInfo().clear_client_endpoint();
    AddObserverResponse failed;
    manager().HandleAddObserver(first, failed);
    node_->MutableNodeInfo() = originalNode_;
    ASSERT_EQ(static_cast<uint32_t>(::kServiceUnavailable), failed.error_message().id());
    EXPECT_EQ(OBSERVER_ADMISSION_ABSENT, Query(first));

    // 不靠Remove补偿掩盖Add失败分支；新op必须已经可以接管空槽并补签。
    const auto next = Original(kSecondOperation);
    AddObserverResponse recovered;
    manager().HandleAddObserver(next, recovered);
    ASSERT_EQ(0u, recovered.error_message().id());
    ASSERT_EQ(OBSERVER_ADMISSION_ACTIVE, Query(next));
    ASSERT_NO_FATAL_FAILURE(AssertTicket(next));
    EXPECT_TRUE(Remove(first).cancelled());
    EXPECT_EQ(OBSERVER_ADMISSION_ACTIVE, Query(next));
}

TEST_F(BattleObserverContract, CancelBeforeRoomAddAndLateOldRemoveCannotRemoveNewOperation) {
    const auto cancelled = Original(kFirstOperation);
    const auto proof = Remove(cancelled);
    ASSERT_TRUE(proof.cancelled());
    EXPECT_EQ(kFirstOperation, proof.spectator_operation_id());
    EXPECT_EQ(kInstance, proof.target_instance_id());
    ASSERT_NO_FATAL_FAILURE(Create());
    AddObserverResponse late;
    manager().HandleAddObserver(cancelled, late);
    EXPECT_EQ(static_cast<uint32_t>(::kInvalidParameter), late.error_message().id());
    EXPECT_EQ(OBSERVER_ADMISSION_ABSENT, Query(cancelled));

    const auto next = Original(kSecondOperation);
    AddObserverResponse added;
    manager().HandleAddObserver(next, added);
    ASSERT_EQ(0u, added.error_message().id());
    AddObserverResponse replay;
    manager().HandleAddObserver(next, replay);
    ASSERT_EQ(0u, replay.error_message().id());
    auto changed = next;
    changed.set_observer_name("改过的原件必须拒绝");
    AddObserverResponse rejected;
    manager().HandleAddObserver(changed, rejected);
    EXPECT_EQ(static_cast<uint32_t>(::kInvalidParameter), rejected.error_message().id());
    EXPECT_TRUE(Remove(cancelled).cancelled());
    EXPECT_EQ(OBSERVER_ADMISSION_ACTIVE, Query(next));
    ASSERT_NO_FATAL_FAILURE(AssertTicket(next));
}
} // namespace
