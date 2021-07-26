#ifndef COMMON_SRC_GAME_deploy_json_H_
#define COMMON_SRC_GAME_deploy_json_H_

#include "common.pb.h"

namespace common
{
    class GameConfig
    {
    public:
        const GameConfigInfo& config_info() const { return config_info_; }

        static GameConfig& GetSingleton() {
            static GameConfig singleton;
            return singleton;
        }

        void Load(const std::string& filename);
    private:
        GameConfigInfo config_info_;
    };

    class DeployConfig
    {
    public:
        enum ConnectionDeployEnum
        {
            SERVER_DATABASE = 0,
            SERVER_DEPLOY = 1,
        };
        const ConnetionParam& connetion_param() { return connetion_param_.data(SERVER_DATABASE); }
        const ConnetionParam& deploy_param() { return connetion_param_.data(SERVER_DEPLOY); }

        static DeployConfig& GetSingleton() {
            static DeployConfig singleton;
            return singleton;
        }

        void Load(const std::string& filename);

    private:
        ConnetionParamJsonFormat connetion_param_;
    };
}//namespace common

#endif//DATABASE_SERVER_SRC_CONFIG_DATABASE_CONFIG_H_
