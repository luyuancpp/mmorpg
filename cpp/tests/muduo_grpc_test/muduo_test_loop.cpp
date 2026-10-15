#include "muduo_test_loop.h"

// 标准库与系统头必须先于 muduo 头(见 muduo_test_loop.h 的说明)。
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>
#else
#include <arpa/inet.h>
#include <filesystem>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "muduo/net/EventLoop.h"
#include "muduo/net/TimerId.h"

namespace muduo_grpc_test
{

void EnsureSocketsInitialized()
{
#ifdef _WIN32
    static const bool initialized = []
    {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    (void)initialized;
#endif
}

uint16_t FindFreeLoopbackPort()
{
    EnsureSocketsInitialized();
#ifdef _WIN32
    const SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET)
    {
        return 0;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    uint16_t port = 0;
    int len = sizeof(addr);
    if (::bind(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0 &&
        ::getsockname(s, reinterpret_cast<sockaddr *>(&addr), &len) == 0)
    {
        port = ntohs(addr.sin_port);
    }
    ::closesocket(s);
    return port;
#else
    const int s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0)
    {
        return 0;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    uint16_t port = 0;
    socklen_t len = sizeof(addr);
    if (::bind(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0 &&
        ::getsockname(s, reinterpret_cast<sockaddr *>(&addr), &len) == 0)
    {
        port = ntohs(addr.sin_port);
    }
    ::close(s);
    return port;
#endif
}

bool RunLoopUntil(muduo::net::EventLoop &loop, const std::function<bool()> &done,
                  std::chrono::milliseconds timeout)
{
    if (done())
    {
        return true;
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    bool satisfied = false;
    const muduo::net::TimerId check = loop.runEvery(0.005, [&loop, &done, &satisfied, deadline]
                                                    {
        if (done())
        {
            satisfied = true;
            loop.quit();
            return;
        }
        if (std::chrono::steady_clock::now() >= deadline)
        {
            loop.quit();
        } });
    loop.loop();
    loop.cancel(check);
    return satisfied || done();
}

void RunLoopFor(muduo::net::EventLoop &loop, std::chrono::milliseconds duration)
{
    RunLoopUntil(loop, [] { return false; }, duration);
}

std::set<uint64_t> SnapshotThreadIds()
{
    std::set<uint64_t> ids;
#ifdef _WIN32
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return ids;
    }
    const DWORD self = GetCurrentProcessId();
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry))
    {
        do
        {
            if (entry.th32OwnerProcessID == self)
            {
                ids.insert(entry.th32ThreadID);
            }
            entry.dwSize = sizeof(entry);
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
#else
    std::error_code ec;
    for (const auto &task : std::filesystem::directory_iterator("/proc/self/task", ec))
    {
        const std::string name = task.path().filename().string();
        if (!name.empty() && name.find_first_not_of("0123456789") == std::string::npos)
        {
            ids.insert(std::stoull(name));
        }
    }
#endif
    return ids;
}

} // namespace muduo_grpc_test
