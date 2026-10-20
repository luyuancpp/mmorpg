#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "node/system/node/open_file_limit.h"
#include "proto/common/base/config.pb.h" // BaseDeployConfig(无 package,全局命名空间)
#include "proto/common/base/node.pb.h"

// open_file_limit 的单元测试(容量模型 A0-2、gate 准入 G4 / P0-1)。
//
// 被测的是 Node::Initialize 里 fd 上限门槛的纯判定:需求怎么算、软限抬到多少、什么时候拒启、豁免开关怎么解析。
// getrlimit / setrlimit 不在这里跑:Evaluate 的回调注入假实现(记录调用次数与参数、返回预设 errno),
// 不读真实进程限值、不读真实 env、不依赖墙钟。Linux 上真实系统调用的路径由容器级红绿对照验证(见交付说明)。
// 和 client_endpoint 同在一个工程:两者都是「判错即 Node 拒启」的启动门槛纯函数,依赖同一组 core / proto 库。

namespace
{

    using open_file_limit::Action;
    using open_file_limit::ClientCap;
    using open_file_limit::Limits;
    using open_file_limit::Mode;
    using open_file_limit::Outcome;
    using open_file_limit::Verdict;

    constexpr uint64_t kReserve = open_file_limit::kNonClientReserve;
    constexpr uint64_t kCeiling = open_file_limit::kSoftLimitCeiling;
    constexpr uint64_t kUnlimited = open_file_limit::kUnlimited;

    // 假的 setrlimit:记下每次被调用时的目标软限,返回预设 errno(0 = 成功)。
    struct FakeSetSoftLimit
    {
        int result = 0;
        std::vector<uint64_t> calls;

        int operator()(uint64_t target)
        {
            calls.push_back(target);
            return result;
        }
    };

    BaseDeployConfig MakeDeploy(uint32_t gateMax, uint32_t battleMax)
    {
        BaseDeployConfig deploy;
        deploy.set_gate_max_connections(gateMax);
        deploy.set_battle_max_connections(battleMax);
        return deploy;
    }

    Outcome EvaluateForTest(Limits before, uint64_t required, FakeSetSoftLimit &fake)
    {
        return open_file_limit::Evaluate(before, required, [&fake](uint64_t target)
                                         { return fake(target); });
    }

} // namespace

// ---- 需求:客户端面准入上限 + 余量 ----

TEST(OpenFileLimit, RequiredForGateIsMaxConnectionsPlusReserve)
{
    const BaseDeployConfig deploy = MakeDeploy(20000, 4096);
    const ClientCap cap = open_file_limit::ClientConnectionCapFor(common::base::GateNodeService, deploy);
    EXPECT_EQ(cap.connections, 20000u);
    EXPECT_EQ(cap.configKey, "GateMaxConnections");
    EXPECT_EQ(open_file_limit::RequiredOpenFiles(cap.connections), 24096u);
}

TEST(OpenFileLimit, RequiredForBattleUsesBattleMaxConnections)
{
    // 同一份配置里带着 GateMaxConnections=20000,不得被误用到 battle 上。
    const BaseDeployConfig deploy = MakeDeploy(20000, 4096);
    const ClientCap cap = open_file_limit::ClientConnectionCapFor(common::base::BattleNodeService, deploy);
    EXPECT_EQ(cap.connections, 4096u);
    EXPECT_EQ(cap.configKey, "BattleMaxConnections");
    EXPECT_EQ(open_file_limit::RequiredOpenFiles(cap.connections), 8192u);
}

TEST(OpenFileLimit, RequiredForSceneIsReserveOnly)
{
    const BaseDeployConfig deploy = MakeDeploy(20000, 4096);
    const ClientCap cap = open_file_limit::ClientConnectionCapFor(common::base::SceneNodeService, deploy);
    EXPECT_EQ(cap.connections, 0u);
    EXPECT_TRUE(cap.configKey.empty());
    EXPECT_EQ(open_file_limit::RequiredOpenFiles(cap.connections), kReserve);
}

TEST(OpenFileLimit, DisabledCapNeedsReserveOnly)
{
    // 0 只在 dev/test 合法(prod 由节点自己的门禁拒绝),这里只要求余量,不按 131071 / 65535 的回落值算。
    const BaseDeployConfig deploy = MakeDeploy(0, 0);
    EXPECT_EQ(open_file_limit::RequiredOpenFiles(
                  open_file_limit::ClientConnectionCapFor(common::base::GateNodeService, deploy).connections),
              kReserve);
    EXPECT_EQ(open_file_limit::RequiredOpenFiles(
                  open_file_limit::ClientConnectionCapFor(common::base::BattleNodeService, deploy).connections),
              kReserve);
}

// ---- 抬升目标:min(硬限, 1048576),只抬不降 ----

TEST(OpenFileLimit, RaiseTargetIsHardWhenBelowCeiling)
{
    EXPECT_EQ(open_file_limit::SoftLimitRaiseTarget({1024, 524288}, 24096), 524288u);
    EXPECT_EQ(open_file_limit::SoftLimitRaiseTarget({1024, 1024}, 24096), 1024u);
}

TEST(OpenFileLimit, RaiseTargetCappedAtCeiling)
{
    EXPECT_EQ(open_file_limit::SoftLimitRaiseTarget({1024, 1073741816}, 24096), kCeiling);
    EXPECT_EQ(open_file_limit::SoftLimitRaiseTarget({1024, kUnlimited}, 24096), kCeiling);
}

TEST(OpenFileLimit, RaiseTargetNeverLowersSoft)
{
    EXPECT_EQ(open_file_limit::SoftLimitRaiseTarget({2000000, 2000000}, 24096), 2000000u);
    EXPECT_EQ(open_file_limit::SoftLimitRaiseTarget({kUnlimited, kUnlimited}, 24096), kUnlimited);
}

// ---- Evaluate:系统调用次数、抬升结果与判定 ----

TEST(OpenFileLimit, EvaluateRaisesSoftToHardAndPasses)
{
    FakeSetSoftLimit fake;
    const Outcome outcome = EvaluateForTest({1024, 524288}, 24096, fake);
    ASSERT_EQ(fake.calls.size(), 1u);
    EXPECT_EQ(fake.calls[0], 524288u);
    EXPECT_EQ(outcome.softAfter, 524288u);
    EXPECT_TRUE(outcome.raiseAttempted);
    EXPECT_EQ(outcome.raiseErrno, 0);
    EXPECT_EQ(outcome.verdict, Verdict::kOk);
}

TEST(OpenFileLimit, EvaluateSkipsSyscallWhenAlreadyAtTarget)
{
    FakeSetSoftLimit fake;
    const Outcome outcome = EvaluateForTest({65536, 65536}, 24096, fake);
    EXPECT_TRUE(fake.calls.empty());
    EXPECT_FALSE(outcome.raiseAttempted);
    EXPECT_EQ(outcome.softAfter, 65536u);
    EXPECT_EQ(outcome.verdict, Verdict::kOk);
}

TEST(OpenFileLimit, EvaluateRefusesWhenHardBelowRequired)
{
    {
        FakeSetSoftLimit fake;
        const Outcome outcome = EvaluateForTest({1024, 1024}, 24096, fake);
        EXPECT_TRUE(fake.calls.empty());
        EXPECT_EQ(outcome.verdict, Verdict::kInsufficient);
    }
    {
        FakeSetSoftLimit fake;
        const Outcome outcome = EvaluateForTest({1024, 8192}, 24096, fake);
        ASSERT_EQ(fake.calls.size(), 1u);
        EXPECT_EQ(fake.calls[0], 8192u);
        EXPECT_EQ(outcome.softAfter, 8192u);
        EXPECT_EQ(outcome.verdict, Verdict::kInsufficient);
    }
}

TEST(OpenFileLimit, EvaluateRefusesWhenRaiseFails)
{
    FakeSetSoftLimit fake;
    fake.result = 1; // EPERM
    const Outcome outcome = EvaluateForTest({1024, 524288}, 24096, fake);
    ASSERT_EQ(fake.calls.size(), 1u);
    EXPECT_EQ(outcome.softAfter, 1024u);
    EXPECT_EQ(outcome.raiseErrno, 1);
    EXPECT_EQ(outcome.verdict, Verdict::kInsufficient);
}

TEST(OpenFileLimit, EvaluatePassesWhenRaiseFailsButSoftSufficient)
{
    // 抬升失败但原软限已够:放行,errno 留给调用方打 WARN 行。
    FakeSetSoftLimit fake;
    fake.result = 1;
    const Outcome outcome = EvaluateForTest({30000, 524288}, 24096, fake);
    EXPECT_EQ(outcome.softAfter, 30000u);
    EXPECT_EQ(outcome.raiseErrno, 1);
    EXPECT_EQ(outcome.verdict, Verdict::kOk);
}

TEST(OpenFileLimit, BoundaryExactlyRequiredPasses)
{
    FakeSetSoftLimit fake;
    EXPECT_EQ(EvaluateForTest({24096, 24096}, 24096, fake).verdict, Verdict::kOk);
    EXPECT_EQ(EvaluateForTest({24095, 24095}, 24096, fake).verdict, Verdict::kInsufficient);
    EXPECT_TRUE(fake.calls.empty());
}

// ---- 豁免开关:缺省 enforce,非法值 fail-closed ----

TEST(OpenFileLimit, ParseModeDefaultsToEnforce)
{
    EXPECT_EQ(open_file_limit::ParseMode(nullptr).mode, Mode::kEnforce);
    EXPECT_TRUE(open_file_limit::ParseMode(nullptr).recognized);
    EXPECT_EQ(open_file_limit::ParseMode("").mode, Mode::kEnforce);
    EXPECT_TRUE(open_file_limit::ParseMode("").recognized);
}

TEST(OpenFileLimit, ParseModeAcceptsEnforceAndWarn)
{
    EXPECT_EQ(open_file_limit::ParseMode("enforce").mode, Mode::kEnforce);
    EXPECT_TRUE(open_file_limit::ParseMode("enforce").recognized);
    EXPECT_EQ(open_file_limit::ParseMode("warn").mode, Mode::kWarn);
    EXPECT_TRUE(open_file_limit::ParseMode("warn").recognized);
}

TEST(OpenFileLimit, ParseModeInvalidFallsBackToEnforce)
{
    // 拼错、大小写不符、布尔风格的值一律不放行。
    for (const char *raw : {"WARN", "Warn", " warn", "warn ", "off", "0", "false", "disable"})
    {
        const auto setting = open_file_limit::ParseMode(raw);
        EXPECT_EQ(setting.mode, Mode::kEnforce) << raw;
        EXPECT_FALSE(setting.recognized) << raw;
    }
}

TEST(OpenFileLimit, ActionRefusesByDefaultAndWarnOnlyExemptsShortfall)
{
    EXPECT_EQ(open_file_limit::ActionFor(Verdict::kInsufficient, Mode::kEnforce), Action::kRefuse);
    EXPECT_EQ(open_file_limit::ActionFor(Verdict::kInsufficient, Mode::kWarn), Action::kProceedExempted);
    EXPECT_EQ(open_file_limit::ActionFor(Verdict::kOk, Mode::kEnforce), Action::kProceed);
    EXPECT_EQ(open_file_limit::ActionFor(Verdict::kOk, Mode::kWarn), Action::kProceed);
    // 非法 env 值经 ParseMode 落到 enforce,不够时照样拒启。
    EXPECT_EQ(open_file_limit::ActionFor(Verdict::kInsufficient, open_file_limit::ParseMode("WARN").mode),
              Action::kRefuse);
}

// ---- 输出格式:启动行是给运维 / 压测 / deploy-staging.sh grep 的契约 ----

TEST(OpenFileLimit, StartupLineFormatIsStable)
{
    const BaseDeployConfig deploy = MakeDeploy(20000, 4096);
    {
        FakeSetSoftLimit fake;
        const ClientCap cap = open_file_limit::ClientConnectionCapFor(common::base::GateNodeService, deploy);
        const Outcome outcome = EvaluateForTest({1024, 524288}, open_file_limit::RequiredOpenFiles(cap.connections), fake);
        EXPECT_EQ(open_file_limit::FormatStartupLine("gate", cap, Mode::kEnforce, outcome),
                  "[open_file_limit] role=gate verdict=ok mode=enforce required=24096 client_cap=20000"
                  " cap_key=GateMaxConnections reserve=4096 soft_before=1024 soft=524288 hard=524288 raise=ok");
    }
    {
        FakeSetSoftLimit fake;
        const ClientCap cap = open_file_limit::ClientConnectionCapFor(common::base::SceneNodeService, deploy);
        const Outcome outcome = EvaluateForTest({4096, 4096}, open_file_limit::RequiredOpenFiles(cap.connections), fake);
        EXPECT_EQ(open_file_limit::FormatStartupLine("scene", cap, Mode::kEnforce, outcome),
                  "[open_file_limit] role=scene verdict=ok mode=enforce required=4096 client_cap=0"
                  " cap_key=none reserve=4096 soft_before=4096 soft=4096 hard=4096 raise=none");
    }
    {
        FakeSetSoftLimit fake;
        fake.result = 1;
        const ClientCap cap = open_file_limit::ClientConnectionCapFor(common::base::BattleNodeService, deploy);
        const Outcome outcome = EvaluateForTest({1024, kUnlimited}, open_file_limit::RequiredOpenFiles(cap.connections), fake);
        EXPECT_EQ(open_file_limit::FormatStartupLine("battle", cap, Mode::kWarn, outcome),
                  "[open_file_limit] role=battle verdict=insufficient mode=warn required=8192 client_cap=4096"
                  " cap_key=BattleMaxConnections reserve=4096 soft_before=1024 soft=1024 hard=unlimited"
                  " raise=failed(errno=1)");
    }
}

TEST(OpenFileLimit, ExemptionWarningStatesShortfall)
{
    FakeSetSoftLimit fake;
    const Outcome outcome = EvaluateForTest({1024, 1024}, 24096, fake);
    EXPECT_EQ(open_file_limit::FormatExemptionWarning("gate", outcome),
              "[open_file_limit] WARN exemption active (NODE_OPEN_FILE_LIMIT_MODE=warn): role=gate soft=1024"
              " required=24096 shortfall=23072; starting anyway. Once the fd table is full, not only new client"
              " connections but also reconnects (peer nodes, Redis, Kafka, etcd) and log file rotation fail with"
              " EMFILE, so the node may be deregistered or crash; for gate / battle lower the client cap to"
              " soft - reserve instead");
}

TEST(OpenFileLimit, InvalidModeLineNamesEnvAndFallback)
{
    const std::string line = open_file_limit::FormatInvalidModeLine("WARN");
    EXPECT_NE(line.find("NODE_OPEN_FILE_LIMIT_MODE='WARN'"), std::string::npos) << line;
    EXPECT_NE(line.find("falling back to enforce"), std::string::npos) << line;
}

TEST(OpenFileLimit, RefusalReasonNamesKnobs)
{
    const BaseDeployConfig deploy = MakeDeploy(20000, 4096);
    {
        FakeSetSoftLimit fake;
        const ClientCap cap = open_file_limit::ClientConnectionCapFor(common::base::GateNodeService, deploy);
        const Outcome outcome = EvaluateForTest({1024, 1024}, open_file_limit::RequiredOpenFiles(cap.connections), fake);
        const std::string reason = open_file_limit::FormatRefusalReason("gate", cap, outcome);
        for (const char *needle : {"role=gate", "required=24096", "GateMaxConnections=20000", "shortfall=23072",
                                   "hard limit", "container runtime", "lower GateMaxConnections",
                                   "NODE_OPEN_FILE_LIMIT_MODE=warn"})
        {
            EXPECT_NE(reason.find(needle), std::string::npos) << needle << " missing in: " << reason;
        }
    }
    {
        // scene 没有客户端面准入上限:不得建议去调一个不存在的配置键。
        FakeSetSoftLimit fake;
        const ClientCap cap = open_file_limit::ClientConnectionCapFor(common::base::SceneNodeService, deploy);
        const Outcome outcome = EvaluateForTest({1024, 1024}, open_file_limit::RequiredOpenFiles(cap.connections), fake);
        const std::string reason = open_file_limit::FormatRefusalReason("scene", cap, outcome);
        EXPECT_NE(reason.find("no client-facing cap"), std::string::npos) << reason;
        EXPECT_EQ(reason.find("lower "), std::string::npos) << reason;
    }
}
