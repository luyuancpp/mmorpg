#include "src/client.h"

#include "google/protobuf/util/json_util.h"

#include "src/file2string/file2string.h"
#include "src/game_logic/game_registry.h"
#include "src/luacpp/lua_module.h"

using namespace common;

int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid();

    if (argc > 0)
    {
        LuaModule::GetSingleton().Init();
       
        int32_t nClients = 1;

        if (argc > 1)
        {
            nClients = atoi(argv[1]);
        }
        EventLoop loop;
        
        gAllFinish = reg.create();
        reg.emplace<uint32_t>(gAllFinish, nClients);

        auto contents = File2String("client.json");
        google::protobuf::StringPiece sp(contents.data(), contents.size());
        ConnetionParamJsonFormat connetion_param_;
        google::protobuf::util::JsonStringToMessage(sp, &connetion_param_);
        InetAddress serverAddr(connetion_param_.data(0).ip(), connetion_param_.data(0).port());
  
        muduo::Logger::setLogLevel(muduo::Logger::WARN);
        std::vector<std::unique_ptr<PlayerClient>> clients;
        for (int i = 0; i < nClients; ++i)
        {
            clients.emplace_back(new PlayerClient(&loop,
                serverAddr));
            clients.back()->connect();
        }
        Timestamp start(Timestamp::now());
        muduo::Logger::setLogLevel(muduo::Logger::INFO);
        loop.loop();
        Timestamp end(Timestamp::now());
        double seconds = timeDifference(end, start);
        LOG_INFO << seconds << "seconds\n";
        muduo::Logger::setLogLevel(muduo::Logger::WARN);
    }
    else
    {
        printf("Usage: %s host_ip numClients [numThreads]\n", argv[0]);
    }
    return 0;
}

