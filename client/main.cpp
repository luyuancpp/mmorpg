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
        EventLoop loop;
        
        client::gAllFinish = common::reg().create();
        common::reg().emplace<uint32_t>(client::gAllFinish, nClients);

        auto contents = common::File2String("client.json");
        google::protobuf::StringPiece sp(contents.data(), contents.size());
        ConnetionParamJsonFormat connetion_param_;
        google::protobuf::util::JsonStringToMessage(sp, &connetion_param_);
        InetAddress serverAddr(connetion_param_.data(0).ip(), connetion_param_.data(0).port());
  
        std::vector<std::unique_ptr<PlayerClient>> clients;
        for (int i = 0; i < nClients; ++i)
        {
            clients.emplace_back(new PlayerClient(&loop,
                serverAddr));
            clients.back()->connect();
        }
        Timestamp start(Timestamp::now());
        LOG_INFO << "all connected";
        for (int i = 0; i < nClients; ++i)
        {
            clients[i]->ReadyGo();
        }

        loop.loop();
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

