#include "src/client.h"

int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid();
    if (argc > 1)
    {
        CurrentThread::sleepUsec(2000000);
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
        CountDownLatch allFinished(nClients);

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
                &allConnected, 
                &allFinished));
            clients.back()->connect();
        }
        allConnected.wait();
        Timestamp start(Timestamp::now());
        LOG_INFO << "all connected";
        for (int i = 0; i < nClients; ++i)
        {
            clients[i]->ReadyGo();
        }
        allFinished.wait();
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

