#ifndef COMMON_SRC_GAME_GAME_CONFIG_H_
#define COMMON_SRC_GAME_GAME_CONFIG_H_

#include "common.pb.h"

namespace common
{
    class GameConfig
    {
    public:
        const ServerInfo& deploy_server() const { return deploy_server_; }

        static GameConfig& GetSingleton() {
            static GameConfig singleton;
            return singleton;
        }

        void Load(const std::string& filename);
    private:
        ServerInfo deploy_server_;
    };
}//namespace common

#endif//DATABASE_SERVER_SRC_CONFIG_DATABASE_CONFIG_H_
