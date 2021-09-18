#include "src/client.h"

#include "google/protobuf/util/json_util.h"

#include "src/client_entityid/client_entityid.h"
#include "src/file2string/file2string.h"
#include "src/luacpp/lua_client.h"

struct PlayerId {
public:
    static uint64_t player_id;
};

uint64_t PlayerId::player_id = 100;

int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid();
    if (argc > 0)
    {
        sol::state& lua = LuaClient::GetSingleton().lua();
        lua.new_usertype<LoginRequest>("LoginRequest", "account",
            sol::property(&LoginRequest::account, &LoginRequest::set_account<const std::string&>));
        lua.new_usertype<PlayerId>("PlayerId",
            "player_id",
            sol::var(PlayerId::player_id));
        int32_t nClients = 1;

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
        LOG_INFO << "all connected";
        loop.loop();
        Timestamp end(Timestamp::now());
        double seconds = timeDifference(end, start);
        printf("%f seconds\n", seconds);
        printf("%.1f calls per second\n", nClients * seconds);
        muduo::Logger::setLogLevel(muduo::Logger::WARN);
    }
    else
    {
        printf("Usage: %s host_ip numClients [numThreads]\n", argv[0]);
    }
    return 0;
}

