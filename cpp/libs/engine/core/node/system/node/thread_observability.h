#pragma once

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "node/system/node/grpc_runtime_probe.h"

#ifdef _WIN32
#include <Windows.h>
#include <TlHelp32.h>
#else
#include <dirent.h>
#endif

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>

namespace node {
namespace observability {

inline int GetEnvIntWithDefaultAndMin(const char* name, int defaultValue, int minValue)
{
    const char* env = std::getenv(name);
    if (env == nullptr || env[0] == '\0') {
        return defaultValue;
    }

    const int parsed = std::atoi(env);
    return parsed >= minValue ? parsed : defaultValue;
}

inline bool IsDisabledEnvValue(const char* env)
{
    if (env == nullptr || env[0] == '\0') {
        return false;
    }

    if ((env[0] == '0' || env[0] == 'f' || env[0] == 'F' || env[0] == 'n' || env[0] == 'N' || env[0] == 'o' || env[0] == 'O')
        && env[1] == '\0') {
        return true;
    }

    return std::strcmp(env, "false") == 0
        || std::strcmp(env, "FALSE") == 0
        || std::strcmp(env, "off") == 0
        || std::strcmp(env, "OFF") == 0
        || std::strcmp(env, "no") == 0
        || std::strcmp(env, "NO") == 0;
}

inline bool ConfiguredThreadMonitorEnabled()
{
    const char* env = std::getenv("NODE_THREAD_MONITOR_ENABLED");
    return !IsDisabledEnvValue(env);
}

// gRPC 运行时守卫是否 fail-closed:NODE_GRPC_RUNTIME_GUARD_FATAL 设为 1 / true / on 时,发现官方运行时被初始化即
// LOG_FATAL 退出。生产默认不设(fail-open,理由见 GrpcRuntimeGuard);CI 与冒烟环境设上,把「又有代码拉起了官方
// gRPC 线程」变成硬失败,而不是一条可能没人看的 ERROR。
inline bool ConfiguredGrpcRuntimeGuardFatal()
{
    const char* env = std::getenv("NODE_GRPC_RUNTIME_GUARD_FATAL");
    if (env == nullptr) {
        return false;
    }
    return std::strcmp(env, "1") == 0
        || std::strcmp(env, "true") == 0
        || std::strcmp(env, "TRUE") == 0
        || std::strcmp(env, "on") == 0
        || std::strcmp(env, "ON") == 0;
}

inline std::string NormalizeNodeDisplayName(const std::string& rawName)
{
    if (rawName.empty()) {
        return "Node";
    }

    size_t start = rawName.find_last_of("/\\");
    std::string name = (start == std::string::npos) ? rawName : rawName.substr(start + 1);
    if (name.empty()) {
        name = "Node";
    }

    name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    return name;
}

struct ThreadMonitorConfig
{
    int stableWindowSeconds = 60;
    int growthWarnConsecutiveSamples = 6;
    int growthWarnAbsoluteIncrease = 16;
};

inline int ConfiguredThreadMonitorStableWindowSeconds()
{
    return GetEnvIntWithDefaultAndMin("NODE_THREAD_MONITOR_STABLE_WINDOW_SECONDS", 60, 1);
}

inline int ConfiguredThreadMonitorGrowthWarnConsecutiveSamples()
{
    return GetEnvIntWithDefaultAndMin("NODE_THREAD_MONITOR_GROWTH_WARN_CONSECUTIVE_SAMPLES", 6, 1);
}

inline int ConfiguredThreadMonitorGrowthWarnAbsoluteIncrease()
{
    return GetEnvIntWithDefaultAndMin("NODE_THREAD_MONITOR_GROWTH_WARN_ABSOLUTE_INCREASE", 16, 1);
}

inline int ConfiguredThreadMonitorSampleIntervalSeconds()
{
    return GetEnvIntWithDefaultAndMin("NODE_THREAD_MONITOR_SAMPLE_INTERVAL_SECONDS", 10, 1);
}

inline ThreadMonitorConfig GetThreadMonitorConfigFromEnv()
{
    ThreadMonitorConfig config;
    config.stableWindowSeconds = ConfiguredThreadMonitorStableWindowSeconds();
    config.growthWarnConsecutiveSamples = ConfiguredThreadMonitorGrowthWarnConsecutiveSamples();
    config.growthWarnAbsoluteIncrease = ConfiguredThreadMonitorGrowthWarnAbsoluteIncrease();
    return config;
}

struct ThreadMonitorSnapshot
{
    int totalThreads = -1;
    int deltaThreads = 0;
    int peakThreads = -1;
    int baselineThreads = -1;
    int increaseFromBaseline = 0;
    std::int64_t uptimeSeconds = 0;
    int consecutiveGrowthSamples = 0;
    bool baselineReady = false;
    bool baselineJustReady = false;
    bool shouldWarn = false;
};

inline int GetCurrentProcessThreadCount()
{
#ifdef _WIN32
    const DWORD pid = GetCurrentProcessId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return -1;
    }

    THREADENTRY32 entry;
    entry.dwSize = sizeof(THREADENTRY32);
    int threadCount = 0;
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID == pid) {
                ++threadCount;
            }
            entry.dwSize = sizeof(THREADENTRY32);
        } while (Thread32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return threadCount;
#else
    DIR* dir = opendir("/proc/self/task");
    if (dir == nullptr) {
        return -1;
    }

    int threadCount = 0;
    while (dirent* entry = readdir(dir)) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        bool isNumeric = true;
        for (const char* p = entry->d_name; *p != '\0'; ++p) {
            if (!std::isdigit(static_cast<unsigned char>(*p))) {
                isNumeric = false;
                break;
            }
        }

        if (isNumeric) {
            ++threadCount;
        }
    }

    closedir(dir);
    return threadCount;
#endif
}

class ThreadMonitor
{
public:
    explicit ThreadMonitor(ThreadMonitorConfig config = {})
        : config_(config)
        , bootTime_(std::chrono::steady_clock::now())
    {
    }

    ThreadMonitorSnapshot Sample()
    {
        ThreadMonitorSnapshot snapshot;
        snapshot.totalThreads = GetCurrentProcessThreadCount();
        snapshot.uptimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - bootTime_).count();

        if (lastThreadCount_ >= 0 && snapshot.totalThreads >= 0) {
            snapshot.deltaThreads = snapshot.totalThreads - lastThreadCount_;
        }

        if (snapshot.totalThreads >= 0 && snapshot.totalThreads > peakThreadCount_) {
            peakThreadCount_ = snapshot.totalThreads;
        }
        snapshot.peakThreads = peakThreadCount_;

        if (snapshot.deltaThreads > 0) {
            ++consecutiveGrowthSamples_;
        } else {
            consecutiveGrowthSamples_ = 0;
        }
        snapshot.consecutiveGrowthSamples = consecutiveGrowthSamples_;

        if (!baselineReady_ && snapshot.uptimeSeconds >= config_.stableWindowSeconds) {
            baselineReady_ = true;
            baselineThreadCount_ = snapshot.totalThreads;
            snapshot.baselineJustReady = true;
        }

        snapshot.baselineReady = baselineReady_;
        snapshot.baselineThreads = baselineThreadCount_;

        if (baselineThreadCount_ >= 0 && snapshot.totalThreads >= 0) {
            snapshot.increaseFromBaseline = snapshot.totalThreads - baselineThreadCount_;
        }

        snapshot.shouldWarn = baselineReady_
            && snapshot.totalThreads >= 0
            && consecutiveGrowthSamples_ >= config_.growthWarnConsecutiveSamples
            && snapshot.increaseFromBaseline >= config_.growthWarnAbsoluteIncrease;

        lastThreadCount_ = snapshot.totalThreads;
        return snapshot;
    }

private:
    ThreadMonitorConfig config_;
    std::chrono::steady_clock::time_point bootTime_;
    int lastThreadCount_ = -1;
    int peakThreadCount_ = -1;
    int baselineThreadCount_ = -1;
    int consecutiveGrowthSamples_ = 0;
    bool baselineReady_ = false;
};

// gRPC 运行时守卫(gRPC 迁到 muduo 驱动第 5 批;docs/design/thread-count-monitoring_zh.md「gRPC 运行时守卫」)。
// 第 3、4 批之后节点内的 gRPC 全部走 muduo_grpc,官方 gRPC 运行时应始终未初始化;它一旦被初始化,
// EventEngine 线程池、定时器等官方线程不受节点控制,进程线程数就不再可控。守卫在注册时与每次采样时核对,
// 第一次发现已初始化即打一条 ERROR,此后只由每小时指标行的 grpc_runtime_initialized 反映。
//
// 这是可用性优先的 fail-open(AGENTS.md §11.3):只记日志与指标,不终止进程、不拒绝流量。依据:运行时被
// 初始化影响的是「线程数是否可控」这一运维属性,不涉及玩家资产、数据一致性或安全;为它让在线的
// scene / battle / gate 退出会踢人、中断战斗,代价远大于多出一批线程。风险由三处兜住:首次 ERROR 日志、
// 指标行的 grpc_runtime_initialized=0/1、回归用例 cpp/tests/rpc_controller_test/grpc_runtime_not_initialized_test.cpp。
// CI 与冒烟环境设 NODE_GRPC_RUNTIME_GUARD_FATAL=1 改为 fail-closed(发现即 LOG_FATAL),生产不设。
//
// 注册(Node 构造)与采样(loop 定时器)在同一线程上串行执行,不需要同步。
class GrpcRuntimeGuard
{
public:
    // 返回官方 gRPC 运行时当前是否已初始化;第一次为真时打一条 ERROR。detectedAt 只用于日志。
    bool Check(const std::string& displayName, const char* detectedAt, std::int64_t uptimeSeconds)
    {
        const bool initialized = IsGrpcRuntimeInitialized();
        if (initialized && !reported_) {
            reported_ = true;
            LOG_ERROR << displayName << " official gRPC runtime is initialized (grpc_is_initialized()=1, detected_at="
                << detectedAt << ", uptime_sec=" << uptimeSeconds
                << "): its threads (EventEngine workers, lifeguard, timer manager) are not controlled by the node,"
                << " so the process thread count is no longer bounded. Node gRPC traffic must go through muduo_grpc only."
                << " Troubleshoot: find code that calls grpc_init() or constructs official grpc objects"
                << " (grpc::CreateChannel / grpc::Channel, grpc::ServerBuilder::BuildAndStart / grpc::Server,"
                << " grpc::CompletionQueue, channel / server credentials, grpc::ResourceQuota, grpc::Alarm);"
                << " on Linux list thread names via /proc/<pid>/task/*/comm"
                << " (event_engine, lifeguard, grpc_global_tim, grpcpp_sync_ser)."
                << " See docs/design/thread-count-monitoring.md. Reported once; the hourly thread metrics line carries"
                << " grpc_runtime_initialized.";
            if (fatal_) {
                LOG_FATAL << displayName << " exiting: NODE_GRPC_RUNTIME_GUARD_FATAL is set and the official gRPC"
                    << " runtime is initialized (see the ERROR above).";
            }
        }
        return initialized;
    }

private:
    bool reported_ = false;
    // 构造时读一次:进程运行中改环境变量不生效,与其它 NODE_THREAD_MONITOR_* 开关一致。
    bool fatal_ = ConfiguredGrpcRuntimeGuardFatal();
};

inline void RegisterThreadObservability(muduo::net::EventLoop& loop, const std::string& nodeName)
{
    const std::string displayName = NormalizeNodeDisplayName(nodeName);
    auto grpcRuntimeGuard = std::make_shared<GrpcRuntimeGuard>();
    // 注册时的核对不受 NODE_THREAD_MONITOR_ENABLED 影响:关掉线程监控只是不再周期采样(也就不再周期核对)。
    grpcRuntimeGuard->Check(displayName, "registration", 0);
    if (!ConfiguredThreadMonitorEnabled()) {
        LOG_INFO << displayName << " thread monitor disabled by env NODE_THREAD_MONITOR_ENABLED";
        return;
    }

    const auto config = GetThreadMonitorConfigFromEnv();
    const int sampleIntervalSeconds = ConfiguredThreadMonitorSampleIntervalSeconds();
    auto threadMonitor = std::make_shared<ThreadMonitor>(config);

    LOG_INFO << displayName << " thread monitor config: sample_interval_sec=" << sampleIntervalSeconds
        << ", stable_window_sec=" << config.stableWindowSeconds
        << ", growth_warn_consecutive_samples=" << config.growthWarnConsecutiveSamples
        << ", growth_warn_absolute_increase=" << config.growthWarnAbsoluteIncrease;

    // 空 = 还没打过指标行:第一次采样就打一行,此后每小时一次。不能用 time_point::min() 表示「从未打过」:
    // steady_clock 是 int64 纳秒,now - min() 有符号溢出,实际回绕成负数,指标行就永远不会打出。
    auto lastInfoLogTime = std::make_shared<std::optional<std::chrono::steady_clock::time_point>>();
    loop.runEvery(static_cast<double>(sampleIntervalSeconds), [threadMonitor, displayName, lastInfoLogTime, grpcRuntimeGuard] {
        const auto snapshot = threadMonitor->Sample();
        const bool grpcRuntimeInitialized = grpcRuntimeGuard->Check(displayName, "sample", snapshot.uptimeSeconds);
        const auto now = std::chrono::steady_clock::now();
        constexpr int kInfoLogIntervalSeconds = 3600;
        if (!lastInfoLogTime->has_value()
            || std::chrono::duration_cast<std::chrono::seconds>(now - **lastInfoLogTime).count() >= kInfoLogIntervalSeconds) {
            *lastInfoLogTime = now;
            LOG_INFO << displayName << " thread metrics: total_threads=" << snapshot.totalThreads
                << ", delta_threads=" << snapshot.deltaThreads
                << ", peak_threads=" << snapshot.peakThreads
                << ", uptime_sec=" << snapshot.uptimeSeconds
                << ", grpc_runtime_initialized=" << (grpcRuntimeInitialized ? 1 : 0);
        }

        if (snapshot.baselineJustReady) {
            LOG_INFO << displayName << " thread baseline (stable window reached): total_threads=" << snapshot.totalThreads
                << ", delta_threads=" << snapshot.deltaThreads
                << ", peak_threads=" << snapshot.peakThreads
                << ", baseline_threads=" << snapshot.baselineThreads
                << ", uptime_sec=" << snapshot.uptimeSeconds;
        }

        if (snapshot.shouldWarn) {
            LOG_WARN << displayName << " thread growth warning: total_threads=" << snapshot.totalThreads
                << ", baseline_threads=" << snapshot.baselineThreads
                << ", increase_from_baseline=" << snapshot.increaseFromBaseline
                << ", consecutive_growth_samples=" << snapshot.consecutiveGrowthSamples
                << ", peak_threads=" << snapshot.peakThreads
                << ", uptime_sec=" << snapshot.uptimeSeconds;
        }
    });
}

} // namespace observability
} // namespace node
