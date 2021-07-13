#include "src/client.h"

#include "src/client_entityid/client_entityid.h"

int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid();
    if (argc > 1)
    {
        int nClients = 1;

        if (argc > 2)
        {
            nClients = atoi(argv[2]);
        }

        int nThreads = 1;

        if (argc > 3)
        {
            nThreads = atoi(argv[3]);
        }

        CountDownLatch allConnected(nClients);
        CountDownLatch allLeaveGame(nClients);
        CountDownLatch allFinish(nClients);
        client::gAllLeaveGame = common::reg().create();
        common::reg().emplace<CountDownLatch*>(client::gAllLeaveGame, &allLeaveGame);
        client::gAllFinish = common::reg().create();
        common::reg().emplace<CountDownLatch*>(client::gAllFinish, &allFinish);

        EventLoop loop;
        EventLoopThreadPool pool(&loop, "playerbench-client");
        pool.setThreadNum(nThreads);
        pool.start();
        InetAddress serverAddr("127.0.0.1", 2000);

        std::vector<std::unique_ptr<PlayerClient>> clients;
        for (int i = 0; i < nClients; ++i)
        {
            clients.emplace_back(new PlayerClient(pool.getNextLoop(), 
                serverAddr, 
                &allConnected));
            clients.back()->connect();
        }
        allConnected.wait();
        Timestamp start(Timestamp::now());
        LOG_INFO << "all connected";
        for (int i = 0; i < nClients; ++i)
        {
            clients[i]->ReadyGo();
        }
        allLeaveGame.wait();
        allFinish.wait();
        Timestamp end(Timestamp::now());
        LOG_INFO << "all finished";
        double seconds = timeDifference(end, start);
        printf("%f seconds\n", seconds);
        printf("%.1f calls per second\n", nClients * seconds);

        return 0;
    }
    else
    {
        printf("Usage: %s host_ip numClients [numThreads]\n", argv[0]);
    }
    return 0;
}

