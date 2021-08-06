#include "src/client.h"

#include "google/protobuf/util/json_util.h"

#include "src/client_entityid/client_entityid.h"
#include "src/file2string/file2string.h"


int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid();
    if (argc > 0)
    {
        int nClients = 1;

        if (argc > 1)
        {
            nClients = atoi(argv[1]);
        }

        int nThreads = 1;

        if (argc > 2)
        {
            nThreads = atoi(argv[2]);
        }

        CountDownLatch allConnected(nClients);
        CountDownLatch allLeaveGame(nClients);
        CountDownLatch allFinish(nClients);
        client::ClientEntityId::gAllConnected = common::reg().create();
        common::reg().emplace<CountDownLatch*>(client::ClientEntityId::gAllConnected, &allConnected);
        client::ClientEntityId::gAllLeaveGame = common::reg().create();
        common::reg().emplace<CountDownLatch*>(client::ClientEntityId::gAllLeaveGame, &allLeaveGame);
        client::ClientEntityId::gAllFinish = common::reg().create();
        common::reg().emplace<CountDownLatch*>(client::ClientEntityId::gAllFinish, &allFinish);

        EventLoop loop;
        EventLoopThreadPool pool(&loop, "playerbench-client");
        pool.setThreadNum(nThreads);
        pool.start();

        auto contents = common::File2String("client.json");
        google::protobuf::StringPiece sp(contents.data(), contents.size());
        ConnetionParamJsonFormat connetion_param_;
        google::protobuf::util::JsonStringToMessage(sp, &connetion_param_);
        InetAddress serverAddr(connetion_param_.data(0).ip(), connetion_param_.data(0).port());
  
        std::vector<std::unique_ptr<PlayerClient>> clients;
        for (int i = 0; i < nClients; ++i)
        {
            clients.emplace_back(new PlayerClient(pool.getNextLoop(), 
                serverAddr));
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
        for (auto& client : clients)
        {
            client->DisConnect();
        }
        Timestamp end(Timestamp::now());
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

