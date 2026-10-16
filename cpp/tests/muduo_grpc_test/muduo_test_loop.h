#pragma once

// 测试辅助:在测试线程自己的 muduo EventLoop 上「转到条件成立或超时为止」。
//
// 用法:测试线程构造 EventLoop(一个线程只能有一个),所有被测对象都在这个线程上创建与调用;
// 需要等异步结果时调用 RunLoopUntil,它用 loop.runEvery 定时检查条件,成立或超时就 loop.quit()。
// 不起线程、不死循环、不 sleep。
//
// 为什么检查定时器是 5ms 一次:Windows 版 muduo 的 EventLoop::wakeup() 是空操作,poll 超时取自
// 最近的定时器(上限 100ms);这个定时器同时保证循环至少每 5ms 转一圈,queueInLoop 投递的回调不会
// 被拖到 100ms 之后。
//
// 注意:本头文件不包含 muduo 头。调用方必须先包含全部标准库 / 系统头,再包含 muduo 头
// (muduo 的 Windows 适配头 #define 了 off_t、pid_t 等名字)。

#include <chrono>
#include <cstdint>
#include <functional>
#include <set>

namespace muduo
{
namespace net
{
class EventLoop;
} // namespace net
} // namespace muduo

namespace muduo_grpc_test
{

// Windows 上确保 WSAStartup 已调用(muduo 的静态初始化也会做,这里不依赖链接顺序)。
void EnsureSocketsInitialized();

// 绑定 127.0.0.1:0 拿一个当前空闲的端口再关掉。之后没人监听它,也可以拿来做「连不上」的目标。
// 失败返回 0(muduo Acceptor listen 失败会 LOG_FATAL,所以调用方拿到 0 必须直接让用例失败)。
uint16_t FindFreeLoopbackPort();

// 在 loop 上转到 done() 为真或超时;返回 done() 的最终结果。不能在 loop 自己的回调里调用。
bool RunLoopUntil(muduo::net::EventLoop &loop, const std::function<bool()> &done,
                  std::chrono::milliseconds timeout);

// 无条件转满 duration(让排队的回调、forceClose、connectDestroyed 等跑完)。
void RunLoopFor(muduo::net::EventLoop &loop, std::chrono::milliseconds duration);

// 当前进程所有线程的 id(Windows:CreateToolhelp32Snapshot;Linux:/proc/self/task)。
std::set<uint64_t> SnapshotThreadIds();

} // namespace muduo_grpc_test
