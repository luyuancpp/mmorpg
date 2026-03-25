#include <iostream>

#include <gtest/gtest.h>

#include "c2gw.pb.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "contrib/hiredis/Hiredis.h"
#include "src/redis_client/redis_client.h"

using namespace muduo;
using namespace muduo::net;

// ---------------------------------------------------------------------------
// 同步 Redis 消息存取
// ---------------------------------------------------------------------------

TEST(RedisTest, SyncMessageLoad)
{
    LoginRequest request;
    request.set_account("luhailon g");
    request.set_password("12 3");

    MessageSyncRedisClient c;
    c.Connect("127.0.0.1", 6379, 1, 1);
    c.Save(request);   // 序列化写入 Redis

    LoginRequest request_load;
    c.Load(request_load); // 从 Redis 读取并反序列化
}

void OnAsyncLoadMessage(Guid player_id, LoginRequest& message)
{
    std::cout << player_id << std::endl;
    std::cout << message.DebugString() << std::endl;
}

// ---------------------------------------------------------------------------
// 异步 Redis 消息存取（事件循环驱动）
// ---------------------------------------------------------------------------

TEST(RedisTest, AsyncMessageLoad)
{
    Logger::setLogLevel(Logger::DEBUG);

    EventLoop loop;

    InetAddress serverAddr("127.0.0.1", 6379);
    hiredis::Hiredis hiredis(&loop, serverAddr);

    hiredis.connect();
    Guid player = 1;
    using LoginRequestAsyncClient = MessageAsyncClient<Guid, LoginRequest>;
    LoginRequestAsyncClient c(hiredis);
    c.SetLoadCallback(OnAsyncLoadMessage);
    loop.runAfter(1, [&c, player]() ->void
        {
            LoginRequestAsyncClient::MessageValuePtr save_message = c.CreateMessage();
            save_message->set_account("Async luhailon g");
            save_message->set_password("12 3");
            c.Save(save_message, player);
        });
    loop.runAfter(2, [&c, player]() ->void
        {
            LoginRequestAsyncClient::MessageValuePtr load_message = c.CreateMessage();
            c.AsyncLoad(player);
        });
    loop.loop();
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}